#include "speedtest.hpp"

#include "../curl-cpp/curl.hpp"
#include "../curl-cpp/curl_multi.hpp"

#include "../utils/type_name.hpp"

#include <cstdio>
#include <cstdarg>

#include <type_traits>
#include <chrono>

namespace chrono = std::chrono;

namespace speedtest {
Speedtest::Speedtest(const utils::ShutdownEvent &shutdown_event, 
                     bool secure,
                     const char *useragent,
                     unsigned long timeout,
                     const char *source_addr) noexcept:
    curl{nullptr},
    shutdown_event{shutdown_event},

    timeout{timeout},
    ip_addr{source_addr},
    useragent{useragent},

    built_url("http")
{
    if (secure)
        built_url += 's';
    built_url.append("://");
}

bool Speedtest::check_libcurl_support(FILE *stderr_stream) const noexcept
{
    auto printer = [&](const char *msg) noexcept
    {
        if (stderr_stream)
            std::fputs(msg, stderr_stream);
    };

    if (!curl.has_protocol("http")) {
        printer("Protocol http not supported");
        return false;
    }

    if (!curl.has_private_ptr_support()) {
        printer("CURLOPT_PRIVATE not supported");
        return false;
    }

    return true;
}

bool operator & (Speedtest::Verbose_level x, Speedtest::Verbose_level y) noexcept
{
    using type = std::underlying_type_t<Speedtest::Verbose_level>;

    return static_cast<type>(x) & static_cast<type>(y);
}
auto operator | (Speedtest::Verbose_level x, Speedtest::Verbose_level y) noexcept -> Speedtest::Verbose_level
{
    using type = std::underlying_type_t<Speedtest::Verbose_level>;
    return Speedtest::Verbose_level{static_cast<type>(x) | static_cast<type>(y)};
}
void Speedtest::enable_verbose(Verbose_level level, FILE *stderr_stream) noexcept
{
    this->verbose_level = level;
    this->stderr_stream = stderr_stream;

    if (verbose_level & Verbose_level::verbose_curl && stderr_stream != nullptr)
        curl.stderr_stream = stderr_stream;
}
void Speedtest::error(const char *fmt, ...) noexcept
{
    if (verbose_level & Verbose_level::error && stderr_stream != nullptr) {
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr_stream, fmt, ap);
        va_end(ap);
    }
}
void Speedtest::debug(const char *fmt, ...) noexcept
{
    if (verbose_level & Verbose_level::debug && stderr_stream != nullptr) {
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr_stream, fmt, ap);
        va_end(ap);
    }
}

auto Speedtest::create_easy() noexcept -> curl::Easy_t
{
    auto easy = curl.create_easy();
    if (!easy)
        return {};
    auto easy_ref = curl::Easy_ref_t{easy.get()};

    easy_ref.set_timeout(timeout);

    {
        auto result = easy_ref.set_interface(ip_addr);
        result.Catch([](auto&&) noexcept {});
        if (result.has_exception_set())
            return {};
    }

    easy_ref.set_follow_location(-1);

    {
        auto result = easy_ref.set_useragent(useragent);
        result.Catch([](auto&&) noexcept {});
        if (result.has_exception_set())
            return {};
    }

    return {std::move(easy)};
}

auto Speedtest::set_url(curl::Easy_ref_t easy_ref, std::initializer_list<std::string_view> parts) noexcept -> 
    Ret_except<void, std::bad_alloc>
{
    auto original_size = built_url.size();

    for (const auto &part: parts)
        built_url.append(part);

    auto result = easy_ref.set_url(built_url.c_str());

    built_url.resize(original_size);
    if (result.has_exception_set())
        return {result};
    else
        return {};
}
void Speedtest::reserve_built_url(std::size_t len) noexcept
{
    auto original_size = built_url.size();

    built_url.reserve(original_size + len);
}

auto Speedtest::perform_and_check(curl::Easy_ref_t easy_ref, const char *fname) noexcept -> 
    Ret_except<bool, std::bad_alloc>
{
    return perform_and_check(easy_ref, easy_ref.perform(), fname);
}
auto Speedtest::perform_and_check(curl::Easy_ref_t easy_ref, curl::Easy_ref_t::perform_ret_t result, 
                                  const char *fname) noexcept -> Ret_except<bool, std::bad_alloc>
{
    if (result.has_exception_set()) {
        if (result.has_exception_type<std::bad_alloc>())
            return {result};

        result.Catch([&](const auto &e) noexcept
        {
            auto *type_name = utils::type_name<std::decay_t<decltype(e)>>();
            error("Catched exception %s when getting %s in %s: e.what() = %s\n",
                   type_name, easy_ref.getinfo_effective_url(), fname, e.what());
        });
        return false;
    }
    
    if (auto response_code = easy_ref.get_response_code(); response_code != 200) {
        error("Get request to %s returned %ld in %s\n", 
               easy_ref.getinfo_effective_url(), response_code, fname);
        return false;
    }

    return true;
}

std::size_t Speedtest::null_writeback(char*, std::size_t, std::size_t size, void*) noexcept
{
    return size;
}

static auto create_multi(curl::curl_t &curl) noexcept -> Ret_except<curl::Multi_t, curl::Exception>
{
    curl::Multi_t multi;
    if (auto result = curl.create_multi(); result.has_exception_set())
        return std::move(result);
    else
        multi = std::move(result).get_return_value();

    if (curl.has_http2_multiplex_support())
        multi.set_multiplexing(0);

    return std::move(multi);
}
auto Speedtest::download(Config &config, const char *url) noexcept -> 
    Ret_except<std::size_t, std::bad_alloc, curl::Exception, curl::libcurl_bug>
{
    using steady_clock = chrono::steady_clock;
    using Easy_ref_t = curl::Easy_ref_t;

    curl::Multi_t multi;
    if (auto result = create_multi(curl); result.has_exception_set())
        return {result};
    else
        multi = std::move(result).get_return_value();

    auto original_sz = built_url.size();

    Config::Candidate_servers::Server::append_dirname_url(url, built_url);
    built_url.append("/random");

    /**
     * Since number of concurrent requests is limited by 
     * config.threads.download, it is necessary to create 
     * a url generator to avoid memory consumption.
     */
    auto gen_url = [&, it = config.sizes.download.begin(), i = std::size_t{0},
                    prev_sz = built_url.size()]() mutable noexcept ->
        const char*
    {
        if (i == config.counts.download) {
            if (++it == config.sizes.download.end())
                return nullptr;

            built_url.resize(prev_sz);
            i = 0;
        }

        if (i == 0) {
            // unsigned can occupy at most 10-bytes
            char buffer[10 + 1 + 10 + 4 + 1];
            std::snprintf(buffer, sizeof(buffer), "%u.%u.jpg", *it, *it);
            built_url.append(buffer);
        }

        ++i;

        return built_url.c_str();
    };

    const char *buit_url_cstr;
    for (std::size_t i = 0; i != config.threads.download && (buit_url_cstr = gen_url()); ++i) {
        auto easy_ref = curl::Easy_ref_t{create_easy().release()};
        if (!easy_ref.curl_easy)
            return {std::bad_alloc{}};

        easy_ref.set_url(buit_url_cstr);
        easy_ref.set_writeback(null_writeback, nullptr);

        // Disable all compression methods.
        if (auto result = easy_ref.set_encoding(nullptr); result.has_exception_set())
            return {result};

        multi.add_easy(easy_ref);
    }

    std::size_t download_cnt;

    auto start = steady_clock::now();

    bool oom = false;
    auto perform_callback = [&](Easy_ref_t &easy_ref, Easy_ref_t::perform_ret_t ret, curl::Multi_t &multi, void*)
        noexcept
    {
        if (auto result = perform_and_check(easy_ref, ret, __PRETTY_FUNCTION__); 
            result.has_exception_set()) 
        {
            oom = true;
            result.Catch([](const auto&) noexcept {});
        } else
            download_cnt += easy_ref.getinfo_sizeof_response_header() + 
                            easy_ref.getinfo_sizeof_response_body();

        if (buit_url_cstr) {
            buit_url_cstr = gen_url();
            if (buit_url_cstr) {
                easy_ref.set_url(buit_url_cstr);
                return;
            }
        }

        multi.remove_easy(easy_ref);
        curl::Easy_t easy{easy_ref.curl_easy};
    };

    do {
        if (auto result = multi.perform(perform_callback, nullptr); result.has_exception_set())
            return {result};
        if (oom)
            return {std::bad_alloc{}};
    } while (multi.break_or_poll().get_return_value() != -1);

    auto seconds = chrono::duration_cast<chrono::seconds>(steady_clock::now() - start).count();
    auto download_speed = download_cnt / seconds;

    built_url.resize(original_sz);

    if (download_speed > 100000)
        config.threads.upload = 8;

    return download_speed;
}

static std::size_t gen_upload_data(char *buffer, std::size_t size, std::size_t nitems, void *userp)
{
    auto bytes = size * nitems;
    auto &i = *static_cast<std::size_t*>(userp);

    static constexpr const std::string_view prefix{"content1="};
    static constexpr const std::string_view chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (auto end = i + bytes; i != end; ++i) {
        if (i < prefix.size())
            buffer[i] = prefix[i];
        else
            buffer[i] = chars[(i - prefix.size()) % chars.size()];
    }

    return bytes;
}
auto Speedtest::upload(Config &config, const char *url) noexcept -> 
    Ret_except<std::size_t, std::bad_alloc, curl::Exception, curl::libcurl_bug>
{
    using steady_clock = chrono::steady_clock;
    using Easy_ref_t = curl::Easy_ref_t;

    curl::Multi_t multi;
    if (auto result = create_multi(curl); result.has_exception_set())
        return {result};
    else
        multi = std::move(result).get_return_value();

    auto original_sz = built_url.size();
    Config::Candidate_servers::Server::append_url(url, built_url);

    auto gen_upload_size = [&, i = config.sizes.upload_start, j = std::size_t{0}]() mutable noexcept ->
        std::size_t
    {
        if (j == config.counts.upload) {
            if (++i == config.sizes.up_sizes.size()) {
                --i; // so that if this function get called next time, it would still returns -1
                return -1;
            }

            j = 0;
        }

        ++j;

        return config.sizes.up_sizes[i];
    };

    std::size_t upload_size;
    for (std::size_t i = 0; i != config.threads.upload && (upload_size = gen_upload_size()) != -1; ++i) {
        auto easy_ref = curl::Easy_ref_t{create_easy().release()};
        if (!easy_ref.curl_easy)
            return {std::bad_alloc{}};

        easy_ref.set_url(built_url.c_str());
        easy_ref.set_writeback(null_writeback, nullptr);

        auto *upload_cnt = new (std::nothrow) std::size_t{0};
        if (!upload_cnt)
            return {std::bad_alloc{}};

        easy_ref.set_private(upload_cnt);
        easy_ref.request_post(gen_upload_data, upload_cnt, upload_size);

        multi.add_easy(easy_ref);
    }

    std::size_t upload_cnt;

    auto start = steady_clock::now();

    bool oom = false;
    auto perform_callback = [&](Easy_ref_t &easy_ref, Easy_ref_t::perform_ret_t ret, curl::Multi_t &multi, void*)
        noexcept
    {
        if (auto result = perform_and_check(easy_ref, ret, __PRETTY_FUNCTION__); 
            result.has_exception_set()) 
        {
            oom = true;
            result.Catch([](const auto&) noexcept {});
        } else
            upload_cnt += easy_ref.getinfo_sizeof_uploaded(); 

        auto *cnt = static_cast<std::size_t*>(easy_ref.get_private());

        if (upload_size == -1) {
            upload_size = gen_upload_size();
            if (upload_size != -1) {
                *cnt = 0;
                easy_ref.request_post(gen_upload_data, cnt, upload_size);
                return;
            }
        }

        delete cnt;
        multi.remove_easy(easy_ref);
        curl::Easy_t easy{easy_ref.curl_easy};
    };

    do {
        if (auto result = multi.perform(perform_callback, nullptr); result.has_exception_set())
            return {result};
        if (oom)
            return {std::bad_alloc{}};
    } while (multi.break_or_poll().get_return_value() != -1);

    auto seconds = chrono::duration_cast<chrono::seconds>(steady_clock::now() - start).count();
    auto upload_speed = upload_cnt / seconds;

    built_url.resize(original_sz);

    return upload_speed;
}
} /* namespace speedtest */
