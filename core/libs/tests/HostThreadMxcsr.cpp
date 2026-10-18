#include "prx/libc/include/HostThread.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>

int main() {
    const auto initial = _mm_getcsr();
    _mm_setcsr(0x9fc0u);
    std::atomic<std::uint32_t> worker{0};
    auto thread = Libc::StartHostThread([&] { worker.store(_mm_getcsr(), std::memory_order_relaxed); });
    thread.join();
    const auto caller = _mm_getcsr();
    _mm_setcsr(initial);
    if (caller != 0x9fc0u || worker.load(std::memory_order_relaxed) != 0x1f80u) throw std::runtime_error("a host worker inherited guest MXCSR");
}
