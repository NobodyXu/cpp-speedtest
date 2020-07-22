#include "ShutdownEvent.hpp"

namespace speedtest::utils {
bool FakeShutdownEvent::has_event() const noexcept
{
    return false;
}
bool CtrlCShutdownEvent::has_event() const noexcept
{
    return is_triggered;
}
} /* namespace speedtest::utils */
