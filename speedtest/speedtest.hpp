/**
 * The classes in this headers utilizes STL but has -fno-exceptions
 * enabled, thus if STL is out of memory, it will raise SIGABRT.
 */

#ifndef  __cpp_speedest_speedtest_speedtest_HPP__
# define __cpp_speedest_speedtest_speedtest_HPP__

# include "../curl-cpp/curl.hpp"
# include "../curl-cpp/curl_easy.hpp"
# include "../curl-cpp/return-exception/ret-exception.hpp"

# include "../utils/ShutdownEvent.hpp"

# include <stdexcept>
# include <utility>
# include <cstdio>

# include <memory>

# include <array>
# include <vector>
# include <unordered_set>
# include <forward_list>

# include <string>
# include <string_view>

namespace speedtest {
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

    enum class Verbose_level {
        none         = 0,
        error        = 1 << 0, // If speedtest is going to ignore errors, print them.
        debug        = 1 << 1, // Print speedtest logic
        verbose_curl = 1 << 2, // Enable curl's verbose mode
    };

protected:
    curl::curl_t curl;
    const utils::ShutdownEvent &shutdown_event;

    unsigned long timeout = 0;
    const char *ip_addr;
    const char *useragent = default_useragent;

    std::string built_url;

    FILE *stderr_stream = nullptr;
    Verbose_level verbose_level = Verbose_level::none;

    auto create_easy() noexcept -> curl::Easy_t;

    /**
     * @param url will be dupped thus can be freed after this call.
     *            <br>Have to be in format hostname:port/path?query. 
     *            <br>Otherwise, it is UNDEFINED BEHAVIOR.
     *
     * Just convenience function, access to speedtest.built_url is 
     * considered valid as long as they reset it to its original size.
     */
    auto set_url(curl::Easy_ref_t easy_ref, std::initializer_list<std::string_view> parts) noexcept -> 
        Ret_except<void, std::bad_alloc>;

    /**
     * Reserve additional len for built_url.
     *
     * User should not call speedtest.built_url.reserve.
     * <br>Instead, they should call this function.
     */
    void reserve_built_url(std::size_t len) noexcept;

    /**
     * perform the transfer, check return value of perform and check response code.
     * 
     * @return true if perform succeeds and response code == 200, false if otherwise.
     */
    auto perform_and_check(curl::Easy_ref_t easy_ref, const char *fname) noexcept -> 
        Ret_except<bool, std::bad_alloc>;


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
    Speedtest(const utils::ShutdownEvent &shutdown_event, 
              bool secure = false,
              const char *useragent = default_useragent,
              unsigned long timeout = 0,
              const char *source_addr = nullptr) noexcept;

    Speedtest(const Speedtest&) = delete;
    Speedtest(Speedtest&&) = delete;

    const Speedtest& operator = (const Speedtest&) = delete;
    const Speedtest& operator = (Speedtest&&) = delete;

    /**
     * @param stderr_stream if not null, will print message
     *                      about what feature isn't supported to it.
     * @return true  if speedtest can run on this libcurl
     *         false if not.
     */
    bool check_libcurl_support(FILE *stderr_stream) const noexcept;

    /**
     * Set level to Verbose_level::none or set stderr_stream to nullptr
     * to disable verbose mode.
     *
     * verbose mode is disabled by default.
     *
     * @param level or-ed value of Verbose_level
     *        <br>If Verbose_level::verbose_curl is added, only curl::Easy_t created
     *        after this call would be affected.
     *        <br>It is adviced that you set verbose before doing anything else.
     */
    void enable_verbose(Verbose_level level, FILE *stderr_stream) noexcept;

    /**
     * If speedtest is going to ignore errors, use this function to optionally print them.
     */
    void error(const char *fmt, ...) noexcept;

    /**
     * Use this to debug speedtest logic.
     */
    void debug(const char *fmt, ...) noexcept;

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

        /**
         * Call speedtest.create_easy and return easy_ref
         * @return curl::Easy_ref_t::curl_easy can be nullptr, which in turn
         *         notify std::bad_alloc event.
         */
        auto get_easy_ref() noexcept -> curl::Easy_ref_t;

    public:
        using Server_id = long;

        /*
         * Configurations retrieved
         */

        struct GeoPosition {
            float lat;
            float lon;
        };

        /**
         * Id of servers ignored
         */
        std::unordered_set<Server_id> ignore_servers;

        struct Sizes {
            static constexpr const std::array up_sizes{
                32768u, 65536u, 131072u, 262144u, 524288u, 1048576u, 7340032u
            };

            unsigned char upload_start;

            static constexpr const std::array download{
                350u, 500u, 750u, 1000u, 1500u, 2000u, 2500u,
                3000u, 3500u, 4000u,
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

        /**
         * @return If std::bad_alloc, then both speedtest and config is in an undefined
         *         state.
         *         <br>Attempt to use them will be Undefine Behavior.
         */
        auto get_config() noexcept -> Ret;

        struct Candidate_servers {
            /**
             * how many urls is parsed by get_servers
             */
            std::size_t url_parsed = 0;

            struct Server {
                static constexpr const std::string_view common_pattern = ":8080/speedtest/upload.php";

                /**
                 * @param url must be result of Server::url::get()
                 *
                 * Convenience function.
                 */
                static void append_url(const char *url, std::string &built_url) noexcept;

                /**
                 * @param url must be result of Server::url::get()
                 *
                 * Convenience function.
                 *
                 * Equivalent to 
                 *
                 *     append_url(url, built_url);
                 *     built_url = utils::dirname(built_url);
                 */
                static void append_dirname_url(const char *url, std::string &built_url) noexcept;

                Server_id server_id;

                /**
                 * If url.get()[0] == 1, then url contains hostname:port/path;
                 * If url.get()[1] == 2, then url contains hostname only,
                 * and the port is predefined to be 8080, path predefined to be 
                 * "/speedtest/upload.php"
                 */
                std::unique_ptr<char[]> url;

                std::string server_name;
                std::string sponsor_name;

                GeoPosition position;
                std::string country_name;

                // Provide this for std::pair
                Server(Server_id server_id,
                       std::unique_ptr<char[]> &&url, 
                       std::string &&server_name, 
                       std::string &&sponsor_name, 
                       GeoPosition pos, 
                       std::string &&country_name) noexcept;

                Server(Server&&) = default;
                Server& operator = (Server&&) = default;
            };

            std::forward_list<Server> servers;

            /**
             * shortest_distance is the distance between closest_servers
             * and current location.
             */
            double shortest_distance;
            /**
             * Iterators into servers.
             */
            std::vector<typename std::forward_list<Server>::const_iterator> closest_servers;
        };

        static constexpr const char *server_list_urls[] = {
            "www.speedtest.net/speedtest-servers-static.php",
            "c.speedtest.net/speedtest-servers-static.php",
            "www.speedtest.net/speedtest-servers.php",
            "c.speedtest.net/speedtest-servers.php",
            nullptr
        };

        /**
         * @param servers_include servers to be used.
         * @param servers_exclude servers to be excluded.
         * @param urls the last url must be nullptr, to signal the end of array.
         *             <br>Should formatted like server_list_urls.
         *
         * @return number of urls read in successfully.
         *         If std::bad_alloc, then both speedtest and config is in an undefined
         *         state.
         *         <br>Attempt to use them will be Undefine Behavior.
         *
         * Get list of servers from preconfigured site.
         */
        auto get_servers(const std::unordered_set<Server_id> &servers_include, 
                         const std::unordered_set<Server_id> &servers_exclude, 
                         const char * const urls[] = server_list_urls) noexcept -> 
            Ret_except<Candidate_servers, std::bad_alloc>;

        /**
         * @pre candidates.servers.size() != 0
         * @post after this function call, get_config and get_servers must not be
         *       called on this object again.
         * @return lists of server that has the lowest latency and 
         *         the lowest latency in ms.
         *         <br>If std::bad_alloc, then both speedtest and config is in an undefined
         *         state.
         *         <br>Attempt to use them will be Undefine Behavior.
         *
         * get_best_server will test every candidates.closest_servers
         * and returns the one with lowest average transfer time for fixed
         * amount of data.
         */
        auto get_best_server(Candidate_servers &candidates) noexcept ->
            Ret_except<std::pair<std::vector<Server_id>, std::size_t>, std::bad_alloc>;
    };

    /**
     * @param url must tbe the same format as Config::Candidate_servers::Server::url.
     * @return download speed, bytes per second.
     *         <br>If std::bad_alloc, then both speedtest and config is in an undefined
     *         state.
     *         <br>Attempt to use them will be Undefine Behavior.
     *         
     * @post just before this function return, if return value is larger than 100000 
     *       and config.thread.upload < 8, config.thread.upload is set to 8.
     *
     * config.threads.download will decides how many connections can be run 
     * in parallel.
     * <br>You can modify that value manully.
     */
    auto download(Config &config, const char *url) noexcept -> 
        Ret_except<std::size_t, std::bad_alloc, curl::Exception>;
};

auto operator | (Speedtest::Verbose_level x, Speedtest::Verbose_level y) noexcept -> Speedtest::Verbose_level;
} /* namespace speedtest */

#endif
