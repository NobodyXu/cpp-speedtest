#include "speedtest.hpp"
#include "../curl-cpp/curl_easy.hpp"
#include "../utils/split2int.hpp"

#include <cstring>
#include <cerrno>
#include <memory>

// Feature tuning
#define PUGIXML_HEADER_ONLY
#define PUGIXML_COMPACT
#define PUGIXML_NO_XPATH
#define PUGIXML_NO_EXCEPTIONS

// Memory tuning
#define PUGIXML_MEMORY_PAGE_SIZE 1024
#define PUGIXML_MEMORY_OUTPUT_STACK 1024
#define PUGIXML_MEMORY_XPATH_PAGE_SIZE 256

#include "../pugixml/src/pugixml.hpp"

namespace speedtest {
bool FakeShutdownEvent::has_event() const noexcept
{
    return false;
}

Speedtest::Speedtest(const ShutdownEvent &shutdown_event) noexcept:
    curl{nullptr},
    shutdown_event{shutdown_event}
{}

void Speedtest::set_secure(bool secure_arg) noexcept
{
    secure = secure_arg;
}
void Speedtest::set_useragent(const char *useragent_arg) noexcept
{
    useragent = useragent_arg;
}
void Speedtest::set_timeout(unsigned long timeout_arg) noexcept
{
    timeout = timeout_arg;
}
void Speedtest::set_source_addr(const char *source_addr) noexcept
{
    ip_addr = source_addr;
}

auto Speedtest::create_easy() noexcept -> 
    Ret_except<curl::Easy_t, std::bad_alloc>
{
    auto easy = curl.create_easy();
    if (!easy)
        return {std::bad_alloc{}};
    auto easy_ref = curl::Easy_ref_t{easy.get()};

    easy_ref.set_timeout(timeout);

    {
        auto result = easy_ref.set_interface(ip_addr);
        if (result.has_exception_set())
            return {result};
    }

    easy_ref.set_follow_location(-1);

    {
        auto result = easy_ref.set_useragent(useragent);
        if (result.has_exception_set())
            return {result};
    }

    return {std::move(easy)};
}
auto Speedtest::set_url(curl::Easy_ref_t easy_ref, const char *url) noexcept -> 
    Ret_except<void, std::bad_alloc>
{
    auto *sep = std::strchr(url, ':');

    if (!(secure && sep - url == 5) && !(!secure && sep - url == 4)) {
        buffer.clear();

        buffer.append("http");
        if (secure)
            buffer.push_back('s');
        buffer.append(sep);

        url = buffer.c_str();
    }

    auto result = easy_ref.set_url(url);
    if (result.has_exception_set())
        return {result};

    return {};
}

Speedtest::Config::Config(Speedtest &speedtest_arg) noexcept:
    speedtest{speedtest_arg}
{}

Speedtest::Config::xml_parse_error::xml_parse_error(const char *error_msg):
    std::runtime_error{""},
    error{error_msg}
{}
const char* Speedtest::Config::xml_parse_error::what() const noexcept
{
    return error;
}

auto Speedtest::Config::get_config() noexcept -> Ret
{
    auto easy = speedtest.create_easy();
    if (easy.has_exception_set())
        return {easy};
    auto easy_ref = curl::Easy_ref_t{easy.get_return_value().get()};

    speedtest.set_url(easy_ref, "://www.speedtest.net/speedtest-config.php");

    std::string response;
    easy_ref.set_readall_writeback(response);

    {
        auto result = easy_ref.perform();
        if (result.has_exception_set())
            return {result};
    }

    pugi::xml_document doc;
    {
        // The following line requies CharT* data() noexcept; (Since C++17)
        auto result = doc.load_buffer_inplace(response.data(), response.size());
        if (!result)
            return {xml_parse_error{result.description()}};
    }

    auto settings = doc.child("settings");

    auto server_config = settings.child("server-config");
    auto download      = settings.child("download");
    auto upload        = settings.child("upload");
    auto client        = settings.child("client");

    if (utils::split2long_set(ignore_servers, server_config.attribute("ignoreids").value())[0] != '\0') {
        if (errno == 0)
            return {xml_parse_error{"Invalid integer in xpath settings/server-config@ignoreids"}};
        else if (errno == ERANGE)
            return {xml_parse_error{"Integer too big in xpath settings/server-config@ignoreids"}};
        else if (errno == EINVAL)
            return {xml_parse_error{"Your libc runtime doesn't support strtol of base 10 integer"}};
    }

    unsigned upload_ratio = upload.attribute("ratio").as_uint();
    unsigned upload_max   = upload.attribute("maxchunkcount").as_uint();

    auto start = upload_ratio - 1;
    sizes.upload_len = 7 - start;
    std::uninitialized_copy(sizes.up_sizes.begin() + start, sizes.up_sizes.end(), sizes.upload);

    counts.upload = upload_max / sizes.upload_len + upload_max % sizes.upload_len ? 1 : 0;
    counts.download = download.attribute("threadsperurl").as_uint();

    return curl::Easy_ref_t::code::ok;
}
} /* namespace speedtest */
