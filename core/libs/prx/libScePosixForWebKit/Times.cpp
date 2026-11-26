#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestTimes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
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

constexpr std::uint64_t TicksPerSecond = 128;

std::int32_t Tick32(std::uint64_t ticks) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(ticks));
}

struct CpuTicks {
    std::int32_t user;
    std::int32_t system;
};

CpuTicks ProcessCpuTicks() {
#ifdef _WIN32
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::system_error(GetLastError(), std::system_category(), "times: GetProcessTimes");
    const auto convert = [](const FILETIME& value) {
        const auto units = (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
        return Tick32(units / (10000000 / TicksPerSecond));
    };
    return {convert(user), convert(kernel)};
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::system_error(errno, std::generic_category(), "times: getrusage");
    const auto convert = [](const timeval& value) {
        const auto seconds = static_cast<std::uint64_t>(value.tv_sec);
        const auto micros = static_cast<std::uint64_t>(value.tv_usec);
        return Tick32(seconds * TicksPerSecond + micros * TicksPerSecond / 1000000);
    };
    return {convert(usage.ru_utime), convert(usage.ru_stime)};
#endif
}

}

std::int32_t APS5_VABI times_nid_postfix(PosixForWebKitTms* output) {
    if (output == nullptr) throw std::invalid_argument("times: null output");
    const int savedErrno = errno;
    const auto cpu = ProcessCpuTicks();
    const auto nowNanos = _ZNSt3__16chrono12steady_clock3nowEv_nid_postfix();
    const PosixForWebKitTms result{cpu.user, cpu.system, 0, 0};
    const bool written = GuestCopyOut_nid_no_patch(output, &result, sizeof(result));
    errno = savedErrno;
    if (!written) throw std::invalid_argument("times: unwritable output");
    return Tick32(static_cast<std::uint64_t>(nowNanos / 7812500));
}
