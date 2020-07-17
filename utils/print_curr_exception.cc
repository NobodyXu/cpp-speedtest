#include "print_curr_exception.hpp"

#include <iostream>
#include <exception>

namespace speedtest::utils {
void print_curr_exception() noexcept
{
    try {
        std::exception_ptr eptr = std::current_exception();
        if (eptr)
            std::rethrow_exception(eptr);
    } catch(const std::exception& e) {
        std::cout << "Caught exception \"" << e.what() << std::endl;
    } catch (...) {
        std::cout << "Exception thrown is not based on std::exception" << std::endl;
    }
}
} /* namespace speedtest::utils */


