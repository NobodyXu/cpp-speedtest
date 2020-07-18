#ifndef  __cpp_speedest_utils_split2int_HPP__
# define __cpp_speedest_utils_split2int_HPP__

# include <unordered_set>

namespace speedtest::utils {
/**
 * @param str can be delimited by any delimiter;
 *            <br> It can be nullptr or "".
 * @param set each valid integer is added to the set
 * @return '\0' when all integer are read in, 
 *         <br>or the pointer to the invalid "integer" that cannot be read in (see below).
 *
 * If there is any invalid "integer", then split2long_set will silently ignore
 * the rest of string and return a pointer to that "integer".
 */
auto split2long_set(std::unordered_set<long> &set, const char *str, std::size_t delimiter_sz = 1) noexcept -> 
    const char*;
} /* namespace speedtest::utils */

#endif
