#include "split2int.hpp"

#include <cstdlib>
#include <cerrno>
#include <cstring>

namespace speedtest::utils {
auto split2long_set(void (*callback)(long integer, void *arg), void *arg,
                    const char *str, std::size_t delimiter_sz, int base) noexcept -> const char*
{
    errno = 0;
    if (str && str[0] != '\0') {
        for (char *endptr; ; str = endptr + delimiter_sz) {
            auto val = std::strtol(str, &endptr, base);

            // If not valid integer, break
            if (endptr == str)
                break;

            if (errno != 0)
                break;

            callback(val, arg);

            // If no more integers to be read in, break
            if (endptr[0] == '\0')
                break;
        }
    }

    return str;
}
} /* namespace speedtest::utils */
