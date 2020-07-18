#ifndef  __cpp_speedest_utils_strncpy_HPP__
# define __cpp_speedest_utils_strncpy_HPP__

# include <cstddef>

namespace speedtest::utils {
/**
 * @param dest_sz size of dest, including the null byte.
 * strncpy copies at most dest_sz - 1 bytes of src, including null byte.
 * The dest will always be null-terminated.
 */
void strncpy(char * dest, std::size_t dest_sz, const char * src) noexcept;
} /* namespace speedtest::utils */

#endif
