#include "sigaction.hpp"

#include <err.h>

namespace speedtest::utils {
void sigaction(int signum, void (*handler)(int)) noexcept
{
    struct sigaction act;
    act.sa_handler = handler;
    if (sigaction(SIGABRT, &act, nullptr) == -1)
        err(1, "Attempt to register %d with void (*)(int) handler %p failed", signum, handler);
}
} /* namespace speedtest::utils */
