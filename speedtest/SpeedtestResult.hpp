/**
 * The classes in this headers utilizes STL but has -fno-exceptions
 * enabled, thus if STL is out of memory, it will raise SIGABRT.
 */

#ifndef  __cpp_speedest_speedtest_SpeedtestResult_HPP__
# define __cpp_speedest_speedtest_SpeedtestResult_HPP__

# include "speedtest.hpp"

namespace speedtest {
struct SpeedtestResult {
    std::size_t download_speed;
    std::size_t upload_speed;

    /**
     * The server should be the one returned by config.get_best_server(candidates).get_return_value()
     */

    double distance;
    std::size_t ping;

    typename Speedtest::Config::Server_id server_id;
    std::string server_name;
    std::string sponsor_name;

    const char *url_ptr; // pointer get from Speedtest::Config::Candidate_servers::url
    typename Speedtest::Config::Client client;

    std::string share_url;

    /**
     * Return server id, server sponsor, server name, unix timestamp in iso, 
     * distance, ping, download speed, upload speed, share_url, ip
     */
    auto to_csv_str(const char *delimiter = ",") const noexcept -> std::string;
};
} /* namespace speedtest */

#endif
