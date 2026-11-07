#include "prx/libc/include/HostThread.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>

int main() {
    const auto initial = _mm_getcsr();
    _mm_setcsr(0x9fc0u);
    std::atomic<std::uint32_t> threadWorker{0};
    std::atomic<std::uint32_t> jthreadWorker{0};
    auto thread = Libc::StartHostThread([&] { threadWorker.store(_mm_getcsr(), std::memory_order_relaxed); });
    thread.join();
    auto jthread = Libc::StartHostJThread([&](std::stop_token) { jthreadWorker.store(_mm_getcsr(), std::memory_order_relaxed); });
    jthread.join();
    const auto caller = _mm_getcsr();
    _mm_setcsr(initial);
    if (caller != 0x9fc0u || threadWorker.load(std::memory_order_relaxed) != 0x1f80u || jthreadWorker.load(std::memory_order_relaxed) != 0x1f80u)
        throw std::runtime_error("a host worker inherited guest MXCSR");
}
