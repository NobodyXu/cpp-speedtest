#ifndef  __cpp_speedest_speedtest_speedtest_HPP__
# define __cpp_speedest_speedtest_speedtest_HPP__

# include "../curl-cpp/curl.hpp"
# include "../curl-cpp/curl_easy.hpp"

# include <utility>
# include <vector>
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
 * This class has no cp/mv ctor/assignment.
 */
class Speedtest {
protected:
    curl::curl_t curl;

    unsigned long timeout = 0;
    const char *ip_addr = nullptr;

    const ShutdownEvent &shutdown_event;
    bool secure;

    auto create_easy() noexcept -> 
        Ret_except<curl::Easy_t, std::bad_alloc>;

public:
    /**
     * If libcurl is already initialized, but SSL is not initialized
     * and one of the speedtest url happen to be https, then
     * it will be a fatal error, so please initialize libcurl with ALL features.
     *
     * **If initialization of libcurl fails due to whatever reason,
     * err is called to print msg and terminate the program.**
     */
    Speedtest(const ShutdownEvent &shutdown_event, bool secure = false) noexcept;

    /**
     * @param timeout in milliseconds. Set to 0 to disable;
     *                should be less than std::numeric_limits<long>::max().
     * @warning not thread safe
     */
    void set_timeout(unsigned long timeout_arg) noexcept;
    /**
     * @param ip_addr ipv4/ipv6 address
     *                Set to nullptr to use whatever TCP stack see fits.
     * @warning not thread safe
     */
    void set_source_addr(const char *source_addr) noexcept;

    class Config {
    public:
        using Servers_t = std::vector<std::string>;
        using Servers_view_t = std::vector<std::string_view>;

    protected:
        Speedtest &speedtest;

        std::vector<std::string> servers;
        std::vector<std::string_view> closest_servers;

    public:
        /**
         * @param speedtest_arg must be kept around until Config is destroyed
         */
        Config(Speedtest &speedtest_arg) noexcept;

        auto get_config() noexcept -> curl::Easy_ref_t::perform_ret_t;

        auto get_servers(Servers_t &servers, Servers_t &exclude) noexcept -> curl::Easy_ref_t::perform_ret_t;

        auto get_closest_servers(unsigned long limit = 5) const noexcept -> const Servers_view_t&;

        auto get_best_server(const Servers_view_t &servers_arg) noexcept;
    };

    ;
};
} /* namespace speedtest */

#endif
