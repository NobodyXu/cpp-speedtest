#ifndef  __cpp_speedtest_thread_sync_thread_sync_HPP__
# define __cpp_speedtest_thread_sync_thread_sync_HPP__

# include <mutex>
# include <condition_variable>
# include <cstdint>

namespace curl {
class Thread_sync {
    std::mutex m;
    std::condition_variable cv;
    std::uintmax_t count;

public:
    Thread_sync() = default;
    Thread_sync(const Thread_sync&) = delete;
    Thread_sync(Thread_sync&&) = delete;

    Thread_sync& operator = (const Thread_sync&) = delete;
    Thread_sync& operator = (Thread_sync&&) = delete;

    void reset(std::uintmax_t threads) noexcept;

    /**
     * Notify the waiting thread that the job is done.
     */
    void thread_done();
    /**
     * Wait for the job to be done.
     */
    void wait();

    ~Thread_sync() = default;
};
} /* namespace curl */

#endif
