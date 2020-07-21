#include "speedtest.hpp"

#include "../curl-cpp/curl.hpp"

#include <cstdio>

namespace speedtest {
bool FakeShutdownEvent::has_event() const noexcept
{
    return false;
}

Speedtest::Speedtest(const ShutdownEvent &shutdown_event, 
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

    return true;
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
std::size_t Speedtest::null_writeback(char*, std::size_t, std::size_t size, void*) noexcept
{
    return size;
}

} /* namespace speedtest */
