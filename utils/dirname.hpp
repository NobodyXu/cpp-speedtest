#ifndef  __cpp_speedest_utils_dirname_HPP__
# define __cpp_speedest_utils_dirname_HPP__

# include <string_view>

namespace speedtest::utils {
std::string_view dirname(const char *str) noexcept;
} /* namespace speedtest::utils */

#endif
