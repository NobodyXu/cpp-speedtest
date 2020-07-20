#include "speedtest.hpp"
#include "../curl-cpp/curl_easy.hpp"
#include "../utils/split2int.hpp"
#include "../utils/strncpy.hpp"
#include "../utils/affix.hpp"

#include <cerrno>
#include <cassert>

#include <cstdint>

#include <cstring>
#include <cstdio>

#include <string_view>
#include <memory>
#include <utility>

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
auto Speedtest::set_url(curl::Easy_ref_t easy_ref, std::string &url) const noexcept -> 
    Ret_except<void, std::bad_alloc>
{
    const char *url_str = url.c_str();

    if (!secure) {
        // The following line requies CharT* data() noexcept; (Since C++17)
        std::memcpy(url.data() + 1, "http", 4);
        ++url_str;
    }

    auto result = easy_ref.set_url(url_str);
    if (result.has_exception_set())
        return {result};

    return {};
}

Speedtest::Config::Config(Speedtest &speedtest_arg) noexcept:
    speedtest{speedtest_arg}
{}

Speedtest::Config::xml_parse_error::xml_parse_error(const char *error_msg):
    Exception{""},
    error{error_msg}
{}
const char* Speedtest::Config::xml_parse_error::what() const noexcept
{
    return error;
}

std::size_t Speedtest::Config::GeoPosition::Hash::operator () (const GeoPosition &g) const noexcept
{
    using type = const std::uint32_t*;
    return (std::size_t{*reinterpret_cast<type>(&g.lat)} << 32) | std::size_t{*reinterpret_cast<type>(&g.lon)};
}

bool operator == (const Speedtest::Config::GeoPosition &x, const Speedtest::Config::GeoPosition &y) noexcept
{
    return x.lat == y.lat && x.lon == y.lon;
}

std::size_t Speedtest::Config::Candidate_servers::string_hash::operator () (const string &s) const noexcept
{
    using type = std::string_view;
    type sv = s.data();
    return std::hash<type>{}(sv);
}

auto xml2geoposition(pugi::xml_node &xml_node)
{
    Speedtest::Config::GeoPosition position;

    position.lat = xml_node.attribute("lat").as_float();
    position.lon = xml_node.attribute("lon").as_float();

    return position;
}

auto Speedtest::Config::get_config() noexcept -> Ret
{
    if (!easy) {
        easy = speedtest.create_easy();
        if (!easy)
            return {std::bad_alloc{}};
    }
    auto easy_ref = curl::Easy_ref_t{easy.get()};

    speedtest.set_url(easy_ref, "://www.speedtest.net/speedtest-config.php");

    std::string response;
    easy_ref.set_readall_writeback(response);

    /**
     * On my own network and machine, the result of
     * https://www.speedtest.net/speedtest-config.php takes 13633 bytes, so
     * reserving 13700 bytes is quite reasonable.
     */
    response.reserve(13700);

    {
        auto result = easy_ref.perform();
        if (result.has_exception_set())
            return {result};
    }

    auto response_code = easy_ref.get_response_code();
    if (response_code != 200) {
        char buffer[64];
        std::snprintf(buffer, 64, "Get response code %ld", response_code);
        return {Error_Response_code{buffer}};
    }

    pugi::xml_document doc;
    {
        // The following line requies CharT* std::string::data() noexcept; (Since C++17)
        auto result = doc.load_buffer_inplace(response.data(), response.size());
        if (!result)
            return {xml_parse_error{result.description()}};
    }

    auto settings = doc.child("settings");

    auto server_config = settings.child("server-config");
    auto download      = settings.child("download");
    auto upload        = settings.child("upload");
    auto client_xml    = settings.child("client");

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
    sizes.upload_len = sizes.up_sizes.size() - start;
    std::uninitialized_copy(sizes.up_sizes.begin() + start, sizes.up_sizes.end(), sizes.upload);

    counts.upload = upload_max / sizes.upload_len + upload_max % sizes.upload_len ? 1 : 0;
    counts.download = download.attribute("threadsperurl").as_uint();

    threads.upload = upload.attribute("threads").as_uint();
    threads.download = server_config.attribute("threadcount").as_uint() * 2;

    length.upload = upload.attribute("testlength").as_uint();
    length.download = download.attribute("testlength").as_uint();

    this->upload_max = counts.upload * sizes.upload_len;

    // Get client info
    utils::strncpy(client.ip, client_xml.attribute("ip").value());

    client.geolocation.position = xml2geoposition(client_xml);
    utils::strncpy(client.geolocation.country, client_xml.attribute("country").value());

    client.is_loggedin = client_xml.attribute("loggedin").as_bool();

    utils::strncpy(client.isp, client_xml.attribute("isp").value());

    client.isp_rating = client_xml.attribute("isprating").as_float();
    client.rating     = client_xml.attribute("rating").as_float();
    client.isp_dlavg  = client_xml.attribute("ispdlavg").as_float();
    client.isp_ulavg  = client_xml.attribute("ispulavg").as_float();

    return curl::Easy_ref_t::code::ok;
}

auto Speedtest::Config::get_servers(const std::unordered_set<Server_id> &servers_arg, 
                                    const std::unordered_set<Server_id> &exclude, 
                                    const char * const urls[]) noexcept ->
    Ret_except<Candidate_servers, std::bad_alloc>
{
    if (!easy) {
        easy = speedtest.create_easy();
        if (!easy)
            return {std::bad_alloc{}};
    }
    auto easy_ref = curl::Easy_ref_t{easy.get()};

    // Built query
    // ?threads=number
    char query[9 + 10 + 1];
    auto query_sz = std::snprintf(query, sizeof(query), "?threads=%u", threads.download);
    assert(query_sz > 0);
    assert(query_sz < sizeof(query)); // ret value excludes null byte

    std::string built_url;
    /**
     * The longest element of server_list_urls is 49-byte long, 
     * protocol takes 5 or 4 bytes, depending on whether it is http or https,
     * and the query takes at most sizeof(query).
     */
    built_url.reserve(4 + std::size_t(speedtest.secure) + 49 + sizeof(query));

    built_url.append("http");
    if (speedtest.secure)
        built_url += 's';
    built_url.append("://");

    // size of protocol prefix.
    // Would be used to reset built_url.
    const auto prefix_size = built_url.size();

    Candidate_servers candidates;

    std::string response;
    /**
     * On my machine, the maximum response I get from server_list_urls
     * with '?thread=4' is 221658, thus reserve 222000.
     */
    response.reserve(222000);

    for (std::size_t i = 0; urls[i] != nullptr; ++i) {
        built_url.resize(prefix_size);
        built_url.append(urls[i]).append(query, query_sz);

        {
            auto result = easy_ref.set_url(built_url.c_str());
            if (result.has_exception_set())
                return {result};
        }

        response.clear();
        easy_ref.set_readall_writeback(response);

        {
            auto result = easy_ref.perform();
            if (result.has_exception_set()) {
                if (result.has_exception_type<std::bad_alloc>())
                    return {result};

                result.Catch([](const auto&) noexcept {});
                continue;
            }
        }

        if (easy_ref.get_response_code() != 200)
            continue;

        pugi::xml_document doc;
        {
            // The following line requies CharT* std::string::data() noexcept; (Since C++17)
            auto result = doc.load_buffer_inplace(response.data(), response.size());
            if (!result)
                continue;
        }

        auto servers_xml = doc.child("settings").child("servers");
        for (auto &&server_xml: servers_xml.children("server")) {
            auto server_id = server_xml.attribute("id").as_llong();
            if (servers_arg.count(server_id))
                continue;
            if (exclude.count(server_id))
                continue;
            if (ignore_servers.count(server_id))
                continue;
            if (candidates.servers.count(server_id))
                continue;

            auto position = xml2geoposition(server_xml);
            if (!candidates.server_geolocations.count(position)) {
                auto it = candidates.server_geolocations.emplace().first;
                utils::strncpy(it->second, server_xml.attribute("country").value());
            }

            auto store_attr = [&](const char *attr_name, auto &c)
            {
                Candidate_servers::string attr_val;
                utils::strncpy(attr_val, server_xml.attribute(attr_name).value());

                auto it = c.find(attr_val);
                if (it == c.end())
                    it = c.emplace(attr_val).first;

                return it->data();
            };

            const char *name = store_attr("name", candidates.server_names);
            const char *sponsor = store_attr("sponsor", candidates.server_sponsors);

            static constexpr const auto &common_pattern = Candidate_servers::Server::common_pattern;
            std::string_view url = server_xml.attribute("url").value();
            bool is_common_pattern;
            if (utils::has_suffix(url, common_pattern)) {
                url = server_xml.attribute("host").value();
                if (utils::has_suffix(url, common_pattern.substr(0, 5)))
                    url.remove_suffix(5); // Remove ':8080' from hostname.
                is_common_pattern = true;
            } else {
                if (utils::has_prefix(url, "http")) {
                    url.remove_prefix(4);
                    if (url[0] == 's')
                        url.remove_prefix(1);
                    url.remove_prefix(3); // Remove '://'
                }
                is_common_pattern = true;
            }

            ;
        }

        ++candidates.url_parsed;
    }

    return candidates;
}
} /* namespace speedtest */
