#ifndef  __cpp_speedest_utils_split2int_HPP__
# define __cpp_speedest_utils_split2int_HPP__

# include <cstddef>

namespace speedtest::utils {
auto split2long_set(void (*callback)(long integer, void *arg), void *arg,
                    const char *str, std::size_t delimiter_sz = 1, int base = 10) noexcept -> const char*;
/**
 * @param str can be delimited by any delimiter;
 *            <br> It can be nullptr or "".
 * @param set each valid integer is added to the set
 * @return '\0' when all integer are read in, 
 *         <br>or the pointer to the invalid overflow "integer" that cannot be read in (see below).
 *
 * If there is any invalid or overflow "integer", then split2long_set will silently ignore
 * the rest of string and return a pointer to that "integer".
 *
 * If errno == 0, then the integer is invalid;
 * If errno == ERANGE, then the integer is too big for long.
 * If errno == EINVAL, then base 10 is not supported.
 */
template <class Container>
auto split2long_set(Container &set, const char *str, 
                    std::size_t delimiter_sz = 1, int base = 10) noexcept -> const char*
{
    return split2long_set([](long integer, void *arg)
    {
        static_cast<Container*>(arg)->emplace(integer);
    }, &set, str, delimiter_sz, base);
}
} /* namespace speedtest::utils */

#endif
