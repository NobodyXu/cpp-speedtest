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

template <std::size_t dest_sz>
auto strncpy(char (&dest)[dest_sz], const char *src) noexcept
{
    return strncpy(dest, dest_sz, src);
}

/**
 * @param c if it is std::string, then -std=c++17 has to be opt in;
 *          <br>if it is std::array/std::vector, then -std=c++11 has to be opt in.
 *          <br>Otherwise, it must support `T* c.data()` and `c.size()`.
 */
template <class Container>
auto strncpy(Container &&c, const char *src) noexcept
{
    /**
     * std::string supports:
     *     CharT* data() noexcept; (since C++17)
     * only from C++17.
     *
     * std::array and std::vector supports:
     *     T* data() noexcept; (since C++11)
     * from C++11.
     */
    return strncpy(c.data(), c.size(), src);
}
} /* namespace speedtest::utils */

#endif
