#include "speedtest.hpp"

#include "../curl-cpp/curl_easy.hpp"

#include "../utils/type_name.hpp"
#include "../utils/split2int.hpp"
#include "../utils/strncpy.hpp"
#include "../utils/affix.hpp"
#include "../utils/dirname.hpp"
#include "../utils/geo_distance.hpp"
#include "../utils/get_unix_timestamp_ms.hpp"

#include <cerrno>

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

Speedtest::Config::Candidate_servers::Server::Server(Server_id server_id,
                                                     std::unique_ptr<char[]> &&url, 
                                                     std::string &&server_name, 
                                                     std::string &&sponsor_name, 
                                                     GeoPosition pos, 
                                                     std::string &&country_name) noexcept:
    server_id{server_id},
    url{std::move(url)},

    server_name{std::move(server_name)},
    sponsor_name{std::move(sponsor_name)},

    position{pos},
    country_name{std::move(country_name)}
{}

auto Speedtest::Config::get_easy_ref() noexcept -> curl::Easy_ref_t
{
    if (!easy)
        easy = speedtest.create_easy();
    return {easy.get()};
}

void Speedtest::Config::Candidate_servers::Server::append_url(const char *url, std::string &built_url) noexcept
{
    built_url.append(url + 1);

    if (url[0] == 2)
        built_url.append(common_pattern);
}
void Speedtest::Config::Candidate_servers::Server::append_dirname_url(const char *url, std::string &built_url) 
    noexcept
{
    if (url[0] == 1)
        built_url.append(utils::dirname(url + 1));
    else {
        built_url.append(url + 1);
        built_url.append(common_pattern.substr(0, 15));
    }
}

static auto xml2geoposition(pugi::xml_node &xml_node)
{
    Speedtest::Config::GeoPosition position;

    position.lat = xml_node.attribute("lat").as_float();
    position.lon = xml_node.attribute("lon").as_float();

    return position;
}

auto Speedtest::Config::get_config() noexcept -> Ret
{
    auto easy_ref = get_easy_ref();
    if (!easy_ref.curl_easy)
        return {std::bad_alloc{}};

    speedtest.set_url(easy_ref, {"www.speedtest.net/speedtest-config.php"});

    response.clear();
    easy_ref.set_readall_writeback(response);

    /**
     * On my own network and machine, the result of
     * https://www.speedtest.net/speedtest-config.php takes 13633 bytes, so
     * reserving 13700 bytes is quite reasonable.
     */
    response.reserve(13700);

    if (auto result = easy_ref.perform(); result.has_exception_set())
        return {result};

    if (auto response_code = easy_ref.get_response_code(); response_code != 200) {
        char buffer[64];
        std::snprintf(buffer, 64, "Get response code %ld", response_code);
        return {Error_Response_code{buffer}};
    }

    pugi::xml_document doc;
    // The following line requies CharT* std::string::data() noexcept; (Since C++17)
    if (auto result = doc.load_buffer_inplace(response.data(), response.size()); !result)
        return {xml_parse_error{result.description()}};

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

    sizes.upload_start = upload_ratio - 1;
    auto upload_len = sizes.up_sizes.size() - sizes.upload_start;

    counts.upload = upload_max / upload_len + upload_max % upload_len ? 1 : 0;
    counts.download = download.attribute("threadsperurl").as_uint();

    threads.upload = upload.attribute("threads").as_uint();
    threads.download = server_config.attribute("threadcount").as_uint() * 2;

    length.upload = upload.attribute("testlength").as_uint();
    length.download = download.attribute("testlength").as_uint();

    this->upload_max = counts.upload * upload_len;

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

auto Speedtest::Config::get_servers(const std::unordered_set<Server_id> &servers_include, 
                                    const std::unordered_set<Server_id> &servers_exclude, 
                                    const char * const urls[]) noexcept ->
    Ret_except<Candidate_servers, std::bad_alloc>
{
    auto easy_ref = get_easy_ref();
    if (!easy_ref.curl_easy)
        return {std::bad_alloc{}};

    // Built query
    // ?threads=number
    char query_buf[9 + 10 + 1];
    auto query_sz = std::snprintf(query_buf, sizeof(query_buf), "?threads=%u", threads.download);

    std::string_view query = query_buf;

    // The longest element of server_list_urls is 49-byte long.
    speedtest.reserve_built_url(49 + query.size());

    Candidate_servers candidates;

    std::unordered_set<Server_id> known_servers;

    /**
     * On my machine, the maximum response I get from server_list_urls
     * with '?thread=4' is 221658, thus reserve 222000.
     */
    response.reserve(222000);
    easy_ref.set_readall_writeback(response);

    for (std::size_t i = 0; urls[i] != nullptr; ++i) {
        if (auto result = speedtest.set_url(easy_ref, {urls[i], query}); result.has_exception_set())
            return {result};

        response.clear();

        if (auto result = speedtest.perform_and_check(easy_ref, __PRETTY_FUNCTION__); 
            result.has_exception_set())
            return {result};
        else if (!result)
            continue;

        pugi::xml_document doc;

        // The following line requies CharT* std::string::data() noexcept; (Since C++17)
        if (auto result = doc.load_buffer_inplace(response.data(), response.size()); !result) {
            speedtest.error("pugixml failed to parse xml retrieved from %s: %s\n",
                            easy_ref.getinfo_effective_url(), result.description());
            continue;
        }

        candidates.shortest_distance = std::numeric_limits<float>::max();
        auto servers_xml = doc.child("settings").child("servers");
        for (auto &&server_xml: servers_xml.children("server")) {
            auto server_id = server_xml.attribute("id").as_llong();

            if (known_servers.count(server_id))
                continue;
            if (servers_include.size() != 0 && !servers_include.count(server_id))
                continue;
            if (!servers_exclude.count(server_id))
                continue;
            if (ignore_servers.count(server_id))
                continue;

            known_servers.emplace(server_id);

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

            auto position = xml2geoposition(server_xml);

            candidates.servers.emplace_front(server_id, 
                                             std::move(url_ptr), 
                                             server_xml.attribute("name").value(),
                                             server_xml.attribute("sponsor").value(),
                                             position, 
                                             server_xml.attribute("country").value());

            auto d = utils::geo_distance(position.lat, position.lon, 
                                         client.geolocation.position.lat, client.geolocation.position.lon);

            if (d > candidates.shortest_distance)
                continue;

            if (d < candidates.shortest_distance) {
                candidates.shortest_distance = d;
                candidates.closest_servers.clear();
            }
            candidates.closest_servers.emplace_back(candidates.servers.cbegin());
        }

        ++candidates.url_parsed;
    }

    return candidates;
}

auto Speedtest::Config::get_best_server(Candidate_servers &candidates) noexcept ->
        Ret_except<std::pair<std::vector<Server_id>, std::size_t>, std::bad_alloc>
{
    auto easy_ref = get_easy_ref();
    if (!easy_ref.curl_easy)
        return {std::bad_alloc{}};

    static constexpr const std::string_view query_prefix = "/latency.txt?x=";
    // the 20-byte is for the unix timestamp in ms 
    // plus trailing '.'.
    char url_params[20 + 1 + 1];

    // The longest element of servers I observed is 69-byte long
    // the additional byte is for the trail_num
    speedtest.reserve_built_url(69 + query_prefix.size() + sizeof(url_params) + 1);

    easy_ref.set_writeback(Speedtest::null_writeback, nullptr);

    // Disable all compression methods.
    if (auto result = easy_ref.set_encoding(nullptr); result.has_exception_set())
        return {result};

    std::pair<std::vector<Server_id>, std::size_t> ret;
    auto &best_servers = ret.first;
    auto &lowest_latency = ret.second;

    lowest_latency = std::numeric_limits<std::size_t>::max();

    for (const auto &server_it: candidates.closest_servers) {
        const auto &server_id = server_it->server_id;
        const auto &url = server_it->url;

        static constexpr const auto &common_pattern = Candidate_servers::Server::common_pattern;
        if (!url) {
            speedtest.error("server with i = %ld have url == nullptr\n", server_id);
            continue;
        }
        if (url[0] == 0 || url[0] > 2) {
            speedtest.error("server with i = %ld have url[0] not in [1, 2], but have %d\n", 
                            server_id, int(url[0]));
            continue;
        }

        std::snprintf(url_params, sizeof(url_params), "%" PRIu64 ".", utils::get_unix_timestamp_ms());

        auto &built_url = speedtest.built_url;
        auto original_sz = built_url.size();

        Candidate_servers::Server::append_dirname_url(url.get(), built_url);

        built_url.append(query_prefix);
        built_url.append(url_params);

        built_url += 'p'; // placeholder

        std::size_t cummulated_time = 0;
        for (char i = 0; i != 3; ++i) {
            built_url[built_url.size() - 1] = '0' + i;

            if (auto result = easy_ref.set_url(built_url.c_str()); result.has_exception_set())
                return {result};

            if (auto result = speedtest.perform_and_check(easy_ref, __PRETTY_FUNCTION__); 
                result.has_exception_set())
                return {result};
            else if (result) {
                auto transfer_time = easy_ref.getinfo_transfer_time();
                speedtest.debug("In %s, %d loop for %s, transfer_time = %zu\n",
                                __PRETTY_FUNCTION__, int{i}, easy_ref.getinfo_effective_url(), transfer_time);
                cummulated_time += transfer_time;
            } else
                cummulated_time += 3600;
        }

        built_url.resize(original_sz);

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
