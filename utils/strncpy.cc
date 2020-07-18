#include "strncpy.hpp"
#include <string.h>

namespace speedtest::utils {
void strncpy(char *dest, std::size_t dest_sz, const char *src) noexcept
{
    memccpy(dest, src, '\0', dest_sz - 1);
    dest[dest_sz - 1] = '\0';
}
} /* namespace speedtest::utils */
