#ifndef  __cpp_speedest_utils_sigaction_HPP__
# define __cpp_speedest_utils_sigaction_HPP__

# include <csignal>

namespace speedtest::utils {
/**
 * @param signum must not be SIGKILL or SIGSTOP or the two real-time signals
 *        used internally by the NPTL threading impl.
 *
 * If parameters isn't set correctly, sigaction would terminate the program
 * with err.
 */
void sigaction(int signum, void (*handler)(int)) noexcept;
} /* namespace speedtest::utils */

#endif
