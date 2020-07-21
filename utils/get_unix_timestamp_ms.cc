#include "get_unix_timestamp_ms.hpp"
#include <sys/time.h>
#include <cassert>

namespace speedtest::utils {
std::uint64_t get_unix_timestamp_ms() noexcept
{
    struct timeval tv;
    assert(gettimeofday(&tv, nullptr) == 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
} /* namespace speedtest::utils */
