#ifndef  __cpp_speedtest_utils_type_name_HPP__
# define __cpp_speedtest_utils_type_name_HPP__

# include "../curl-cpp/return-exception/ret-exception.hpp"

namespace speedtest::utils {
/**
 * Signature:
 *
 *     template <class T>
 *     constexpr auto type_name() -> const char*
 */
using ret_exception::impl::type_name;
} /* namespace speedtest::utils */

#endif
