#include "ShutdownEvent.hpp"

#include "sigaction.hpp"

namespace speedtest::utils {
bool FakeShutdownEvent::has_event() const noexcept
{
    return false;
}

CtrlCShutdownEvent::CtrlCShutdownEvent() noexcept
{
    sigaction(SIGINT, [](int signum) noexcept
    {
        CtrlCShutdownEvent::is_triggered = true;
    });
}
bool CtrlCShutdownEvent::has_event() const noexcept
{
    return is_triggered;
}
} /* namespace speedtest::utils */
