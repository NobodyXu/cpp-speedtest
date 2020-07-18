#include "split2int.hpp"

#include <cstdlib>
#include <cstring>

namespace speedtest::utils {
auto split2long_set(std::unordered_set<long> &set, const char *str, std::size_t delimiter_sz) noexcept ->
    const char*
{
    if (str && str[0] != '\0') {
        for (char *endptr; ; str = endptr + delimiter_sz) {
            auto val = strtol(str, &endptr, 10);

            // If not valid integer, break
            if (endptr == str)
                break;

            set.emplace(val);

            // If no more integers to be read in, break
            if (endptr[0] == '\0')
                break;
        }
    }

    return str;
}
} /* namespace speedtest::utils */
