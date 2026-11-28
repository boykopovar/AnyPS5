#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

namespace PosixThread {

constexpr int GUEST_EPERM = 1;
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

inline std::uint64_t WaitMicrosecondsBetween(
    const KernelTimespec& now,
    const KernelTimespec& deadline,
    std::int64_t maxSeconds,
    std::uint64_t maxWaitMicros) {
    if (maxWaitMicros == 0 || maxSeconds < 0) {
        return 0;
    }
    if (deadline.tv_sec < now.tv_sec) {
        const std::uint64_t behindSeconds = static_cast<std::uint64_t>(now.tv_sec) - static_cast<std::uint64_t>(deadline.tv_sec);
        if (behindSeconds > 1 || deadline.tv_nsec <= now.tv_nsec) {
            return 0;
        }
        const std::int64_t remaining = static_cast<std::int64_t>(deadline.tv_nsec) - static_cast<std::int64_t>(now.tv_nsec) - 1000000000LL;
        if (remaining <= 0) {
            return 0;
        }
        const std::uint64_t rounded = static_cast<std::uint64_t>((remaining + 999LL) / 1000LL);
        return rounded >= maxWaitMicros ? maxWaitMicros : rounded;
    }
    const std::uint64_t aheadSeconds = static_cast<std::uint64_t>(deadline.tv_sec) - static_cast<std::uint64_t>(now.tv_sec);
    constexpr std::uint64_t maxSafeSeconds = static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max() - 1000000000LL) / 1000000000LL);
    const std::uint64_t effectiveMaxSeconds = std::min(static_cast<std::uint64_t>(maxSeconds), maxSafeSeconds);
    if (aheadSeconds > effectiveMaxSeconds) {
        return maxWaitMicros;
    }
    const std::int64_t nsecDiff = static_cast<std::int64_t>(deadline.tv_nsec) - static_cast<std::int64_t>(now.tv_nsec);
    const std::int64_t remaining = static_cast<std::int64_t>(aheadSeconds) * 1000000000LL + nsecDiff;
    if (remaining <= 0) {
        return 0;
    }
    const std::uint64_t rounded = static_cast<std::uint64_t>((remaining + 999LL) / 1000LL);
    return rounded >= maxWaitMicros ? maxWaitMicros : rounded;
}

inline bool ComputeRelativeMicroseconds(
    int clockId,
    const KernelTimespec* guestDeadline,
    std::int64_t maxSeconds,
    std::uint64_t maxWaitMicros,
    std::uint64_t* outMicros) {
    KernelTimespec deadline{};
    if (!guestDeadline || !GuestCopyIn_nid_no_patch(&deadline, guestDeadline, sizeof(deadline))) return false;
    if (deadline.tv_nsec < 0 || deadline.tv_nsec >= 1000000000) return false;
    KernelTimespec now{};
    if (clock_gettime_nid_postfix(clockId, &now) != 0) return false;

    *outMicros = WaitMicrosecondsBetween(now, deadline, maxSeconds, maxWaitMicros);
    return true;
}

inline bool RelativeMicroseconds(int clockId, const KernelTimespec* guestDeadline, KernelUseconds* usec) {
    constexpr auto maximum = std::numeric_limits<KernelUseconds>::max();
    std::uint64_t waitMicros = 0;
    if (!ComputeRelativeMicroseconds(
            clockId,
            guestDeadline,
            static_cast<std::int64_t>(maximum / 1000000) + 1,
            static_cast<std::uint64_t>(maximum),
            &waitMicros)) {
        return false;
    }
    *usec = static_cast<KernelUseconds>(waitMicros);
    return true;
}

inline bool AbsoluteDeadlineNanos(int clockId, const KernelTimespec* abstime, std::uint64_t* deadline) {
    constexpr std::int64_t horizonSeconds = 365LL * 24 * 3600;
    constexpr std::uint64_t maxWaitMicros = static_cast<std::uint64_t>(horizonSeconds) * 1000000ULL;
    std::uint64_t waitMicros = 0;
    if (!ComputeRelativeMicroseconds(clockId, abstime, horizonSeconds, maxWaitMicros, &waitMicros)) {
        return false;
    }
    *deadline = TimedWait::DeadlineNanos(waitMicros);
    return true;
}

}

#endif
