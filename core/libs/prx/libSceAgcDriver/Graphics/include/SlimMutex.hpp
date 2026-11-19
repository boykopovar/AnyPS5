#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLIMMUTEX_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLIMMUTEX_HPP

#include <atomic>
#include <thread>
#include "prx/libc/include/PreciseWait.hpp"

namespace AgcDriver::Graphics {

class SlimMutex {
public:
    SlimMutex() = default;
    SlimMutex(const SlimMutex&) = delete;
    SlimMutex& operator=(const SlimMutex&) = delete;

    void lock() {
        for (unsigned spins = 0;;) {
            if (!locked.exchange(true, std::memory_order_acquire)) return;
            while (locked.load(std::memory_order_relaxed)) {
                if (spins < 128) {
                    ++spins;
                    PreciseSpinPause();
                } else {
                    std::this_thread::yield();
                }
            }
        }
    }
    bool try_lock() { return !locked.load(std::memory_order_relaxed) && !locked.exchange(true, std::memory_order_acquire); }
    void unlock() { locked.store(false, std::memory_order_release); }

private:
    std::atomic<bool> locked{false};
};

}

#endif
