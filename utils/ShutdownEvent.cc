#include "ShutdownEvent.hpp"

namespace speedtest::utils {
bool FakeShutdownEvent::has_event() const noexcept
{
    return false;
}
} /* namespace speedtest::utils */
