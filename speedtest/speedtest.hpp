/**
 * The classes in this headers utilizes STL but has -fno-exceptions
 * enabled, thus if STL is out of memory, it will raise SIGABRT.
 */

#ifndef  __cpp_speedest_speedtest_speedtest_HPP__
# define __cpp_speedest_speedtest_speedtest_HPP__

# include "../curl-cpp/curl.hpp"
# include "../curl-cpp/curl_easy.hpp"

# include <stdexcept>
# include <utility>
# include <vector>
# include <array>
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
    const char *ip_addr = nullptr;
    bool secure = false;
    const char *useragent = default_useragent;

    std::string buffer;

    auto create_easy() noexcept -> 
        Ret_except<curl::Easy_t, std::bad_alloc>;

    /**
     * @param url will be dupped thus can be freed after this call.
     *
     *            Have to be in format protocol://... or ://...;
     *            Protocol has to be either http or https.
     *
     *            Otherwise, it is UNDEFINED BEHAVIOR.
     */
    auto set_url(curl::Easy_ref_t easy_ref, const char *url) noexcept -> 
        Ret_except<void, std::bad_alloc>;

public:
    /**
     * If libcurl is already initialized, but SSL is not initialized
     * and one of the speedtest url happen to be https, then
     * it will be a fatal error, so please initialize libcurl with ALL features.
     *
     * **If initialization of libcurl fails due to whatever reason,
     * err is called to print msg and terminate the program.**
     */
    Speedtest(const ShutdownEvent &shutdown_event) noexcept;

    /**
     * By default, secure == false
     */
    void set_secure(bool secure_arg) noexcept;
    /**
     * By default, useragent is set to default_useragent
     */
    void set_useragent(const char *useragent_arg) noexcept;
    /**
     * @param timeout in milliseconds. Set to 0 to disable (default);
     *                should be less than std::numeric_limits<long>::max().
     * @warning not thread safe
     */
    void set_timeout(unsigned long timeout_arg) noexcept;
    /**
     * @param ip_addr ipv4/ipv6 address
     *                Set to nullptr to use whatever TCP stack see fits (default).
     * @warning not thread safe
     */
    void set_source_addr(const char *source_addr) noexcept;

    /**
     * @warning all functions is this class is not thread-safe.
     */
    class Config {
    public:
        using Servers_t = std::vector<std::string>;
        using Servers_view_t = std::vector<std::string_view>;

    protected:
        Speedtest &speedtest;

    public:
        /**
         * url of servers.
         * Can be either http://..., https://... or ://...
         */
        std::vector<std::string> servers;
        /**
         * Pointers of servers.
         */
        std::vector<std::string_view> closest_servers;

        /*
         * Configurations retrieved
         */

        /**
         * Id of servers ignored
         */
        std::unordered_set<long> ignore_servers;

        struct Sizes {
            static constexpr const std::array up_sizes{
                32768, 65536, 131072, 262144, 524288, 1048576, 7340032
            };

            unsigned char upload_len = 0;
            unsigned upload[7];

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

            struct Geolocation {
                float lat;
                float lon;

                char country[11];
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

        class xml_parse_error: public std::runtime_error {
        public:
            const char *error;

            xml_parse_error(const char *error_msg);
            xml_parse_error(const xml_parse_error&) = default;

            const char* what() const noexcept;
        };
        using Ret = glue_ret_except_t<curl::Easy_ref_t::perform_ret_t, 
                                      Ret_except<void, xml_parse_error>>;

        auto get_config() noexcept -> Ret;
        /**
         * @param exclude servers to be excluded.
         *
         * Get list of servers from preconfigured site.
         * You can skip this by manually adding servers to Config::servers.
         *
         * You can also add more server by adding them to Config::severs.
         */
        auto get_servers(Servers_t &exclude) noexcept -> Ret;

        auto get_closest_servers(unsigned long limit = 5) const noexcept -> const Servers_view_t&;

        auto get_best_server() noexcept ->
            curl::Easy_ref_t::perform_ret_t;
    };

    ;
};
} /* namespace speedtest */

#endif
