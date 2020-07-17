#include "utils/print_curr_exception.hpp"
#include <err.h>

#include <csignal>

int main(int argc, char* argv[])
{
    // Print exception thrown by STD c++ lib,
    // as the default msg when -fno-exceptions is
    // enabled is useless.
    struct sigaction act;
    act.sa_handler = [](int signum) noexcept
    {
        speedtest::utils::print_curr_exception();
    };
    if (sigaction(SIGABRT, &act, nullptr) == -1)
        err(1, "Attempt to register SIGABRT failed");

    ;

    return 0;
}
