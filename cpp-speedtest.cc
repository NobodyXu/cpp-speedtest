#include "utils/print_curr_exception.hpp"
#include "utils/sigaction.hpp"

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

    return 0;
}
