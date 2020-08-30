#include "sigaction.hpp"

#include <string.h>
#include <err.h>

namespace speedtest::utils {
void sigaction(int signum, void (*handler)(int)) noexcept
{
    struct sigaction act;
    memset (&act, 0, sizeof (act));

    act.sa_handler = handler;
    if (sigaction(SIGABRT, &act, nullptr) == -1)
        err(1, "Attempt to register %d with void (*)(int) handler %p failed", signum, handler);
}
} /* namespace speedtest::utils */
