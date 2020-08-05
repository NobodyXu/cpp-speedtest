#include "utils/print_curr_exception.hpp"
#include "utils/sigaction.hpp"

#include "speedtest/speedtest.hpp"
#include "speedtest/SpeedtestResult.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[])
{
    // Print exception thrown by STD c++ lib,
    // as the default msg when -fno-exceptions is
    // enabled is useless.
    speedtest::utils::sigaction(SIGABRT, [](int signum) noexcept
    {
        speedtest::utils::print_curr_exception();
        std::exit(1);
    });

    speedtest::SpeedtestResult result;
    std::unique_ptr<char[]> url;

    {
        speedtest::utils::FakeShutdownEvent shutdown_event;
        speedtest::Speedtest speedtest{shutdown_event, true};

        if (!speedtest.check_libcurl_support(stderr))
            return 1;

        speedtest::Speedtest::Config config{speedtest};
        
        std::puts("Retrieving configurations...");
        config.get_config();
        result.client = config.client;

        {
            std::puts("Retrieving candidate servers...");
            auto candidates = config.get_servers().get_return_value();

            std::puts("Testing for best server...");
            auto [best_server_ids, minimal_ping] = config.get_best_server(candidates).get_return_value();

            result.distance = candidates.shortest_distance;
            result.ping = minimal_ping;

            auto server_it = best_server_ids.front();

            url = std::move(server_it->url);

            result.server_id = server_it->server_id;
            result.server_name = std::move(server_it->server_name);
            result.sponsor_name = std::move(server_it->sponsor_name);
        }

        result.download_speed = speedtest.download(config, url.get()).get_return_value();
        result.upload_speed = speedtest.upload(config, url.get()).get_return_value();
    }

    std::printf("Download speed = %zu\nUpload speed = %zu\n", result.download_speed, result.upload_speed);

    return 0;
}
