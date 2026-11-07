#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWNJOIN_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SHUTDOWNJOIN_HPP

#include "prx/libc/include/Shutdown.hpp"

#include <atomic>
#include <chrono>
#include <thread>

template <typename Mutex>
void LockOrEndIfHostAbandoned(Mutex& mutex, int code) {
    while (!mutex.try_lock()) {
        if (LibcHostShutdownAbandoned_nid_no_patch())
            LibcTerminate_nid_no_patch(code);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

template <typename Thread>
void JoinOrEndIfHostAbandoned(Thread& thread, int code) {
    if (LibcHostShutdownAbandoned_nid_no_patch())
        LibcTerminate_nid_no_patch(code);
    if (thread.joinable())
        thread.join();
}

template <typename Thread>
void JoinWhenReturnedOrEndIfAbandoned(Thread& thread, const std::atomic<bool>& returned, int code) {
    if (!thread.joinable()) return;
    while (!returned.load(std::memory_order_acquire)) {
        if (LibcHostShutdownAbandoned_nid_no_patch())
            LibcTerminate_nid_no_patch(code);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (thread.joinable())
        thread.join();
}

#endif
