#include "ShadowAddressSpace.hpp"

#if defined(__linux__)
#include <sys/mman.h>
#include <atomic>
#include <mutex>

namespace AsanRuntime {

extern "C" bool InitializeShadow_nid_no_patch() noexcept {
#if defined(MAP_FIXED_NOREPLACE)
    static std::atomic<bool> initialized{false};
    if (initialized.load(std::memory_order_acquire)) return true;
    try {
        static std::mutex initMutex;
        std::lock_guard lock(initMutex);
        if (initialized.load(std::memory_order_relaxed)) return true;
        auto* requested = reinterpret_cast<void*>(ShadowBase);
        auto* mapped = mmap(requested, ShadowSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped == MAP_FAILED) return false;
        if (mapped != requested) {
            munmap(mapped, ShadowSize);
            return false;
        }
        initialized.store(true, std::memory_order_release);
        return true;
    } catch (...) {
        return false;
    }
#else
    return false;
#endif
}


extern "C" bool ShadowRangeAvailable_nid_no_patch(std::uintptr_t begin, std::uintptr_t end) noexcept {
    return begin >= ShadowBase && end <= ShadowBase + ShadowSize && begin <= end && InitializeShadow_nid_no_patch();
}

}
#endif
