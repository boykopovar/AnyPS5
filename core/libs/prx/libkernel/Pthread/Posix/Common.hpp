#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP

#include <cstdint>
#include <chrono>
#include <limits>
#include "SceTypes.hpp"

extern "C" int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);

namespace PosixThread {

constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_ETIMEDOUT = 60;

inline int ToErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

// Converts an absolute deadline on the guest clock into a relative wait, clamped to what the sce layer accepts.
inline bool RelativeMicroseconds(int clockId, const KernelTimespec* abstime, KernelUseconds* usec) {
    if (!abstime || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return false;
    KernelTimespec now{};
    clock_gettime_nid_postfix(clockId, &now);
    const auto deadline = std::chrono::seconds(abstime->tv_sec) + std::chrono::nanoseconds(abstime->tv_nsec);
    const auto current = std::chrono::seconds(now.tv_sec) + std::chrono::nanoseconds(now.tv_nsec);
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - current).count();
    *usec = remaining <= 0 ? 0 : remaining >= std::numeric_limits<KernelUseconds>::max() ? std::numeric_limits<KernelUseconds>::max() : static_cast<KernelUseconds>(remaining);
    return true;
}

}

#endif
