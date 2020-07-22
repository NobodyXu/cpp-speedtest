# include "dirname.hpp"
#include <cstring>

namespace speedtest::utils {
std::string_view dirname(const char *str) noexcept
{
    auto last_slash = std::strrchr(str, '/');
    return std::string_view(0, last_slash - str);
}
} /* namespace speedtest::utils */
