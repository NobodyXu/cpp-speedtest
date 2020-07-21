#include "speedtest.hpp"

#include "../curl-cpp/curl_easy.hpp"

#include "../utils/split2int.hpp"
#include "../utils/strncpy.hpp"
#include "../utils/affix.hpp"
#include "../utils/geo_distance.hpp"
#include "../utils/get_unix_timestamp_ms.hpp"

#include <cerrno>
#include <cassert>

#include <cstdint>

#include <cstring>
#include <cstdio>
#include <cinttypes>

#include <string_view>
#include <memory>
#include <new>
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

Speedtest::Config::Candidate_servers::Server::Server(std::unique_ptr<char[]> &&url, 
                                                     const char *name, 
                                                     GeoPosition pos, 
                                                     const char *sponsor) noexcept:
    url{std::move(url)},
    name{name},
    position{pos},
    sponsor{sponsor}
{}

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

    speedtest.set_url(easy_ref, {"www.speedtest.net/speedtest-config.php"});

    response.clear();
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
                                    const char * const urls[],
                                    bool debug) noexcept ->
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
    char query_buf[9 + 10 + 1];
    auto query_sz = std::snprintf(query_buf, sizeof(query_buf), "?threads=%u", threads.download);
    assert(query_sz > 0);
    assert(query_sz < sizeof(query_buf)); // ret value excludes null byte

    std::string_view query = query_buf;

    // The longest element of server_list_urls is 49-byte long.
    speedtest.reserve_built_url(49 + query.size());

    Candidate_servers candidates;

    /**
     * On my machine, the maximum response I get from server_list_urls
     * with '?thread=4' is 221658, thus reserve 222000.
     */
    response.reserve(222000);

    for (std::size_t i = 0; urls[i] != nullptr; ++i) {
        {
            auto result = speedtest.set_url(easy_ref, {urls[i], query});
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

                result.Catch([&](const auto &e) noexcept
                {
                    if (debug)
                        std::fprintf(stderr, "Catched exception in %s when getting %s: e.what() = %s\n",
                                     __PRETTY_FUNCTION__, easy_ref.getinfo_effective_url(), e.what());
                });
                continue;
            }
        }
        
        auto response_code = easy_ref.get_response_code();
        if (response_code != 200) {
            if (debug)
                std::fprintf(stderr, "Get request to %s returned %ld\n", 
                             easy_ref.getinfo_effective_url(), response_code);
            continue;
        }

        pugi::xml_document doc;
        {
            // The following line requies CharT* std::string::data() noexcept; (Since C++17)
            auto result = doc.load_buffer_inplace(response.data(), response.size());
            if (!result) {
                if (debug) {
                    std::fprintf(stderr, "pugixml failed to parse xml retrieved from %s: %s\n",
                                 easy_ref.getinfo_effective_url(), result.description());
                }
                continue;
            }
        }

        candidates.shortest_distance = std::numeric_limits<float>::max();
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

            bool is_common_pattern = utils::has_suffix(url, common_pattern);
            if (is_common_pattern)
                url.remove_suffix(common_pattern.size()); // Remove common_pattern off the url

            if (utils::has_prefix(url, "http")) {
                url.remove_prefix(4);
                if (url[0] == 's')
                    url.remove_prefix(1);
                url.remove_prefix(3); // Remove '://'
            }

            auto url_ptr = std::unique_ptr<char[]>{new (std::nothrow) char[1 + url.size() + 1]};
            if (!url_ptr)
                return {std::bad_alloc{}};

            url_ptr[0] = char{is_common_pattern} + 1;
            utils::strncpy(url_ptr.get() + 1, url.size() + 1, url.data());

            candidates.servers.try_emplace(server_id, std::move(url_ptr), name, position, sponsor);

            auto d = utils::geo_distance(position.lat, position.lon, 
                                         client.geolocation.position.lat, client.geolocation.position.lon);

            if (d > candidates.shortest_distance)
                continue;

            if (d < candidates.shortest_distance) {
                candidates.shortest_distance = d;
                candidates.closest_servers.clear();
            }
            candidates.closest_servers.emplace_back(server_id);
        }

        ++candidates.url_parsed;
    }

    return candidates;
}

auto Speedtest::Config::get_best_server(Candidate_servers &candidates, bool debug) noexcept ->
        Ret_except<std::pair<std::vector<Server_id>, std::size_t>, std::bad_alloc>
{
    if (!easy) {
        easy = speedtest.create_easy();
        if (!easy)
            return {std::bad_alloc{}};
    }
    auto easy_ref = curl::Easy_ref_t{easy.get()};

    static constexpr const std::string_view query_prefix = "/latency.txt?x=";
    // the 20-byte is for the unix timestamp in ms.
    char unix_timestamp_ms[21];

    // The longest element of servers I observed is 69-byte long
    speedtest.reserve_built_url(69 + query_prefix.size() + sizeof(unix_timestamp_ms));

    std::pair<std::vector<Server_id>, std::size_t> ret;
    auto &best_servers = ret.first;
    auto &lowest_latency = ret.second;

    lowest_latency = std::numeric_limits<std::size_t>::max();

    for (const auto &server_id: candidates.closest_servers) {
        const auto it = candidates.servers.find(server_id);
        if (it == candidates.servers.end()) {
            if (debug)
                std::fprintf(stderr, "Can't find server with id = %ld\n", server_id);
            continue;
        }
        const auto &url = it->second.url;

        static constexpr const auto &common_pattern = Candidate_servers::Server::common_pattern;
        if (!url) {
            if (debug)
                std::fprintf(stderr, "server with i = %ld have url == nullptr\n", server_id);
            continue;
        }
        if (url[0] == 0 || url[0] > 2) {
            if (debug)
                std::fprintf(stderr, "server with i = %ld have url[0] not in [1, 2], but have %d\n", 
                             server_id, int(url[0]));
            continue;
        }

        easy_ref.set_writeback(Speedtest::null_writeback, nullptr);

        // Disable all compression methods.
        if (auto result = easy_ref.set_encoding(nullptr); result.has_exception_set())
            return {result};

        std::size_t cummulated_time = 0;
        for (char i = 0; i != 3; ++i) {
            using utils::get_unix_timestamp_ms;
            std::snprintf(unix_timestamp_ms, sizeof(unix_timestamp_ms), "%" PRIu64 "", get_unix_timestamp_ms());

            {
                auto result = [&]() noexcept
                {
                    if (url[0] == 1)
                        return speedtest.set_url(easy_ref, {
                            std::string_view{url.get() + 1},
                            query_prefix, 
                            std::string_view(unix_timestamp_ms)
                        });
                    else
                        return speedtest.set_url(easy_ref, {
                            std::string_view{url.get() + 1}, 
                            common_pattern,
                            query_prefix, 
                            std::string_view(unix_timestamp_ms)
                        });
                }();

                if (result.has_exception_set())
                    return {result};
            }

            {
                auto result = easy_ref.perform();
                if (result.has_exception_set()) {
                    if (result.has_exception_type<std::bad_alloc>())
                        return {result};

                    result.Catch([&](const auto &e) noexcept
                    {
                        if (debug)
                            std::fprintf(stderr, "Catched exception in %s when getting %s: e.what() = %s\n",
                                         __PRETTY_FUNCTION__, easy_ref.getinfo_effective_url(), e.what());
                    });
                    cummulated_time += 3600;
                    continue;
                }
                
                auto response_code = easy_ref.get_response_code();
                if (response_code != 200) {
                    if (debug)
                        std::fprintf(stderr, "Get request to %s returned %ld\n", 
                                     easy_ref.getinfo_effective_url(), response_code);
                    cummulated_time += 3600;
                    continue;
                }
            }

            cummulated_time += easy_ref.getinfo_transfer_time();
        }

        if (cummulated_time < lowest_latency) {
            lowest_latency = cummulated_time;
            best_servers.clear();
        }

        if (cummulated_time == lowest_latency)
            best_servers.emplace_back(server_id);
    }

    return std::move(ret);
}
} /* namespace speedtest */
