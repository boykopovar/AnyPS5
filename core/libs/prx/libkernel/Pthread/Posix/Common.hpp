#include "prx/libkernel/Time/include/Time.hpp"
#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP

#include <cstdint>
#include <chrono>
#include <limits>
#include "SceTypes.hpp"

namespace PosixThread {

constexpr int GUEST_ESRCH = 3;
constexpr int GUEST_EFAULT = 14;
constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_ENOTSUP = 45;
constexpr int GUEST_ETIMEDOUT = 60;

inline int ToErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

inline int ToSce(int error) {
    return error == 0 ? 0 : static_cast<int>(0x80020000u | static_cast<std::uint32_t>(error));
}

// Converts an absolute deadline on the guest clock into a relative wait, clamped to what the sce layer accepts.
inline bool RelativeMicroseconds(int clockId, const KernelTimespec* abstime, KernelUseconds* usec) {
    if (!abstime || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return false;
    KernelTimespec now{};
    clock_gettime_nid_postfix(clockId, &now);

    constexpr auto maximum = std::numeric_limits<KernelUseconds>::max();
    if (abstime->tv_sec < now.tv_sec - 1) {
        *usec = 0;
        return true;
    }
    const std::int64_t seconds = abstime->tv_sec - now.tv_sec;
    if (seconds > static_cast<std::int64_t>(maximum / 1000000) + 1) {
        *usec = maximum;
        return true;
    }
    const std::int64_t remaining = (seconds * 1000000000 + abstime->tv_nsec - now.tv_nsec) / 1000;
    *usec = remaining <= 0 ? 0 : remaining >= maximum ? maximum : static_cast<KernelUseconds>(remaining);
    return true;
}

}

#endif
