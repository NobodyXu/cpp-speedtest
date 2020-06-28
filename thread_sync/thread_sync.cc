#include "thread_sync.hpp"

namespace curl {
void Thread_sync::reset(std::uintmax_t threads) noexcept
{
    count = threads;
}

void Thread_sync::thread_done()
{
    std::unique_lock<std::mutex> lk(m);
    if (--count == 0)
        cv.notify_all();
}

void Thread_sync::wait()
{
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [this]() noexcept {
        return count == 0;
    });
}
} /* namespace curl */
