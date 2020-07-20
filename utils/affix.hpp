#ifndef  __cpp_speedest_utils_affix_HPP__
# define __cpp_speedest_utils_affix_HPP__

# include <string_view>

namespace speedtest::utils {
bool has_suffix(std::string_view str, std::string_view suffix) noexcept;

} /* namespace speedtest::utils */

#endif
