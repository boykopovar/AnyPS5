#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestTimes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ChildProcesses.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace {

struct CpuTicks {
    std::int32_t user;
    std::int32_t system;
};

CpuTicks CpuTimeTicks(bool children) {
#ifdef _WIN32
    const auto convert = [](std::uint64_t units) {
        const auto micros = static_cast<std::int64_t>(units / 10);
        return PosixForWebKitClockTicks(micros / 1000000, micros % 1000000);
    };
    if (children) {
        const auto reaped = GetReapedChildProcessTimes_nid_no_patch();
        return {convert(reaped.user), convert(reaped.kernel)};
    }
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::system_error(GetLastError(), std::system_category(), "times: GetProcessTimes");
    const auto units = [](const FILETIME& value) {
        return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    return {convert(units(user)), convert(units(kernel))};
#else
    rusage usage{};
    if (getrusage(children ? RUSAGE_CHILDREN : RUSAGE_SELF, &usage) != 0)
        throw std::system_error(errno, std::generic_category(), "times: getrusage");
    const auto convert = [](const timeval& value) {
        return PosixForWebKitClockTicks(value.tv_sec, value.tv_usec);
    };
    return {convert(usage.ru_utime), convert(usage.ru_stime)};
#endif
}

}

std::int32_t APS5_VABI times_nid_postfix(PosixForWebKitTms* output) {
    if (output == nullptr) throw std::invalid_argument("times: null output");
    const int savedErrno = errno;
    const auto self = CpuTimeTicks(false);
    const auto children = CpuTimeTicks(true);
    const auto nowNanos = _ZNSt3__16chrono12steady_clock3nowEv_nid_postfix();
    const PosixForWebKitTms result{self.user, self.system, children.user, children.system};
    const bool written = GuestCopyOut_nid_no_patch(output, &result, sizeof(result));
    errno = savedErrno;
    if (!written) throw std::invalid_argument("times: unwritable output");
    return static_cast<std::int32_t>(nowNanos / (1000000000 / PosixForWebKitClockTicksPerSecond));
}
