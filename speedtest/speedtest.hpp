/**
 * The classes in this headers utilizes STL but has -fno-exceptions
 * enabled, thus if STL is out of memory, it will raise SIGABRT.
 */

#ifndef  __cpp_speedest_speedtest_speedtest_HPP__
# define __cpp_speedest_speedtest_speedtest_HPP__

# include "../curl-cpp/curl.hpp"
# include "../curl-cpp/curl_easy.hpp"
# include "../curl-cpp/return-exception/ret-exception.hpp"

# include <stdexcept>
# include <utility>
# include <cstdio>

# include <memory>

# include <array>
# include <vector>
# include <unordered_set>

# include <string>
# include <string_view>

namespace speedtest {
class ShutdownEvent {
protected:
    ShutdownEvent() = default;

    ShutdownEvent(const ShutdownEvent&) = default;
    ShutdownEvent(ShutdownEvent&&) = default;

    ShutdownEvent& operator = (const ShutdownEvent&) = default;
    ShutdownEvent& operator = (ShutdownEvent&&) = default;

    ~ShutdownEvent() = default;

public:
    /**
     * Has shutdown event happens
     */
    virtual bool has_event() const noexcept = 0;
};
class FakeShutdownEvent: public ShutdownEvent {
public:
    FakeShutdownEvent() = default;

    FakeShutdownEvent(const FakeShutdownEvent&) = default;
    FakeShutdownEvent(FakeShutdownEvent&&) = default;

    FakeShutdownEvent& operator = (const FakeShutdownEvent&) = default;
    FakeShutdownEvent& operator = (FakeShutdownEvent&&) = default;

    bool has_event() const noexcept;
};

/**
 * @warning ctor and dtor of this object should be ran when
 *          only one thread is present.
 *
 * @warning all functions is this class is not thread-safe.
 *
 * This class has no cp/mv ctor/assignment.
 */
class Speedtest {
public:
    static constexpr const auto default_useragent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                                                    "(KHTML, like Gecko) Chrome/84.0.4147.89 Safari/537.36";

protected:
    curl::curl_t curl;
    const ShutdownEvent &shutdown_event;

    unsigned long timeout = 0;
    const char *ip_addr;
    const char *useragent = default_useragent;

    std::string built_url;

    auto create_easy() noexcept -> curl::Easy_t;

    /**
     * @param url will be dupped thus can be freed after this call.
     *            <br>Have to be in format hostname:port/path?query. 
     *            <br>Otherwise, it is UNDEFINED BEHAVIOR.
     */
    auto set_url(curl::Easy_ref_t easy_ref, std::initializer_list<std::string_view> parts) noexcept -> 
        Ret_except<void, std::bad_alloc>;

    /**
     * Reserve additional len for built_url.
     */
    void reserve_built_url(std::size_t len) noexcept;

    static std::size_t null_writeback(char*, std::size_t, std::size_t size, void*) noexcept;

public:
    /**
     * @param timeout in milliseconds. Set to 0 to disable (default);
     *                should be less than std::numeric_limits<long>::max().
     * @param ip_addr ipv4/ipv6 address
     *                Set to nullptr to use whatever TCP stack see fits.
     *
     * If libcurl is already initialized, but SSL is not initialized
     * and one of the speedtest url happen to be https, then
     * it will be a fatal error, so 
     * <br>**if you use libcurl at somewhere else in your program, please
     * initialize it to use ALL FEATURES**.
     *
     * **If initialization of libcurl fails due to whatever reason,
     * err is called to print msg and terminate the program.**
     *
     * @warning not thread safe
     */
    Speedtest(const ShutdownEvent &shutdown_event, 
              bool secure = false,
              const char *useragent = default_useragent,
              unsigned long timeout = 0,
              const char *source_addr = nullptr) noexcept;

    /**
     * @param stderr_stream if not null, will print message
     *                      about what feature isn't supported to it.
     * @return true  if speedtest can run on this libcurl
     *         false if not.
     */
    bool check_libcurl_support(FILE *stderr_stream) const noexcept;

    /**
     * @warning all functions is this class is not thread-safe.
     */
    class Config {
    public:
        using Servers_t = std::vector<std::string>;
        using Servers_view_t = std::vector<std::string_view>;

    protected:
        Speedtest &speedtest;
        curl::Easy_t easy;
        std::string response;

        auto get_easy_ref() noexcept -> curl::Easy_ref_t;

    public:
        using Server_id = long;

        /*
         * Configurations retrieved
         */

        struct GeoPosition {
            float lat;
            float lon;

            struct Hash {
                static_assert(sizeof(float) <= 4, "float on this platform does not follow IEEE 754 format");

                std::size_t operator () (const GeoPosition &g) const noexcept;
            };

            friend bool operator == (const GeoPosition &x, const GeoPosition &y) noexcept;
        };

        /**
         * Id of servers ignored
         */
        std::unordered_set<Server_id> ignore_servers;

        struct Sizes {
            static constexpr const std::array up_sizes{
                32768, 65536, 131072, 262144, 524288, 1048576, 7340032
            };

            unsigned char upload_len = 0;
            unsigned upload[up_sizes.size()];

            unsigned download[10] = {
                350, 500, 750, 1000, 1500, 2000, 2500,
                3000, 3500, 4000,
            };
        } sizes;

        struct Counts {
            unsigned upload;
            unsigned download;
        } counts;

        struct Threads {
            unsigned upload;
            unsigned download;
        } threads;

        struct Length {
            unsigned upload;
            unsigned download;
        } length;

        unsigned upload_max;

        struct Client {
            /**
             * Information retrieved from [here][1]
             *
             * [1]: https://stackoverflow.com/questions/166132/maximum-length-of-the-textual-representation-of-an-ipv6-address
             */
            char ip[46];

            struct {
                GeoPosition position;
                /**
                 * country name longer than 36 will be truncated.
                 * array is used here to prevent any heap allocation.
                 */
                char country[36];
            } geolocation;

            bool is_loggedin;

            char isp[21];
            float isp_rating;
            float rating;
            float isp_dlavg;
            float isp_ulavg;
        } client;

        /**
         * @param speedtest_arg must be kept around until Config is destroyed
         */
        Config(Speedtest &speedtest_arg) noexcept;

        class Exception: public std::runtime_error {
        public:
            using std::runtime_error::runtime_error;
        };
        class xml_parse_error: public Exception {
        public:
            const char *error;

            xml_parse_error(const char *error_msg);
            xml_parse_error(const xml_parse_error&) = default;

            const char* what() const noexcept;
        };
        class Error_Response_code: public Exception {
        public:
            using Exception::Exception;
        };
        using Ret = glue_ret_except_t<curl::Easy_ref_t::perform_ret_t, 
                                      Ret_except<void, xml_parse_error, Error_Response_code>>;

        auto get_config() noexcept -> Ret;

        struct Candidate_servers {
            /**
             * how many urls is parsed by get_servers
             */
            std::size_t url_parsed = 0;

            struct Server {
                static constexpr const std::string_view common_pattern = ":8080/speedtest/upload.php";

                /**
                 * If url.get()[0] == 1, then url contains hostname:port/path;
                 * If url.get()[1] == 2, then url contains hostname only,
                 * and the port is predefined to be 8080, path predefined to be 
                 * "/speedtest/upload.php"
                 */
                std::unique_ptr<char[]> url;

                const char *name;
                GeoPosition position;
                const char *sponsor;

                // Provide this for std::pair
                Server(std::unique_ptr<char[]> &&url, 
                       const char *name, 
                       GeoPosition pos, 
                       const char *sponsor) noexcept;

                Server(Server&&) = default;
                Server& operator = (Server&&) = default;
            };

            /**
             * std::array is chosen to store country name here
             * because store std::array<char, 32> or std::string
             * both takes 32 bytes, but std::array<char, 32> is able to 
             * pack 35 bytes (excluding null byte) without additional
             * allocation.
             */
            using string = std::array<char, 32>;

            struct string_hash {
                std::size_t operator () (const string &s) const noexcept;
            };

            /**
             * (lat, lon, country), name, cc, spnsor are often duplicated,
             * using unordered_set helps to deduplicate them.
             */
            std::unordered_set<string, string_hash> server_names;

            std::unordered_map<GeoPosition, string, 
                               typename GeoPosition::Hash> server_geolocations;

            std::unordered_set<string, string_hash> server_sponsors;

            std::unordered_map<Server_id, Server> servers;

            /**
             * shortest_distance is the distance between closest_servers
             * and current location.
             */
            double shortest_distance;
            /**
             * Pointers of servers.
             */
            std::vector<Server_id> closest_servers;
        };

        static constexpr const char *server_list_urls[] = {
            "www.speedtest.net/speedtest-servers-static.php",
            "c.speedtest.net/speedtest-servers-static.php",
            "www.speedtest.net/speedtest-servers.php",
            "c.speedtest.net/speedtest-servers.php",
            nullptr
        };

        /**
         * @param servers_arg servers to be used.
         * @param exclude servers to be excluded.
         * @param urls the last url must be nullptr, to signal the end of array.
         *             <br>Should formatted like server_list_urls.
         *
         * @return number of urls read in successfully.
         *
         * Get list of servers from preconfigured site.
         */
        auto get_servers(const std::unordered_set<Server_id> &servers, 
                         const std::unordered_set<Server_id> &exclude, 
                         const char * const urls[] = server_list_urls,
                         bool debug = false) noexcept -> 
            Ret_except<Candidate_servers, std::bad_alloc>;

        /**
         * @pre candidates.servers.size() != 0
         * @post after this function call, get_config and get_servers must not be
         *       called on this object again.
         * @return lists of server that has the lowest latency and 
         *         the lowest latency in ms.
         *
         * get_best_server will test every candidates.closest_servers
         * and returns the one with lowest average transfer time for fixed
         * amount of data.
         */
        auto get_best_server(Candidate_servers &candidates, bool debug = false) noexcept ->
            Ret_except<std::pair<std::vector<Server_id>, std::size_t>, std::bad_alloc>;
    };

    ;
};
} /* namespace speedtest */

#endif
