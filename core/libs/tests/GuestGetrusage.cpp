#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
struct Timeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

static void Require(bool value) { if (!value) std::abort(); }
static std::int64_t CpuMicros(int who) {
    GuestResourceUsage usage{};
    *__error_nid_postfix() = 77;
    Require(getrusage_nid_postfix(who, &usage) == 0 && *__error_nid_postfix() == 77);
    return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
}
static void SpinUntil(int who, std::int64_t micros) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    volatile std::uint64_t sink = 0;
    while (CpuMicros(who) < micros) {
        for (int i = 0; i < 100000; ++i) sink = sink + i;
        Require(std::chrono::steady_clock::now() < deadline);
    }
}
static void RequireFailure(int who, GuestResourceUsage* usage, int error) {
    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(who, usage) == -1);
    Require(*__error_nid_postfix() == error);
}
#ifndef _WIN32
static std::int64_t Kilobytes(long maxrss) {
#ifdef __APPLE__
    return static_cast<std::int64_t>(maxrss) / 1024;
#else
    return static_cast<std::int64_t>(maxrss);
#endif
}
#endif
static std::int64_t HostPeakKilobytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    Require(GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)) != FALSE);
    return static_cast<std::int64_t>(memory.PeakWorkingSetSize / 1024);
#else
    struct rusage native{};
    Require(::getrusage(RUSAGE_SELF, &native) == 0);
    return Kilobytes(native.ru_maxrss);
#endif
}
static std::int64_t MaxResident(int who) {
    GuestResourceUsage usage{};
    Require(getrusage_nid_postfix(who, &usage) == 0);
    return usage.ru_maxrss;
}
static void RequirePeak(int who) {
    const std::int64_t before = HostPeakKilobytes();
    const std::int64_t reported = MaxResident(who);
    Require(before > 0 && before <= reported && reported <= HostPeakKilobytes());
}
static void RequireChildren() {
    GuestResourceUsage children;
    std::memset(&children, 0xff, sizeof(children));
    *__error_nid_postfix() = 77;
    Require(getrusage_nid_postfix(-1, &children) == 0 && *__error_nid_postfix() == 77);
#ifdef _WIN32
    const GuestResourceUsage empty{};
    Require(std::memcmp(&children, &empty, sizeof(children)) == 0);
#else
    struct rusage native{};
    Require(::getrusage(RUSAGE_CHILDREN, &native) == 0);
    Require(children.ru_utime.tv_sec == native.ru_utime.tv_sec && children.ru_utime.tv_usec == native.ru_utime.tv_usec);
    Require(children.ru_stime.tv_sec == native.ru_stime.tv_sec && children.ru_stime.tv_usec == native.ru_stime.tv_usec);
    Require(children.ru_maxrss == Kilobytes(native.ru_maxrss));
    Require(children.ru_minflt == native.ru_minflt && children.ru_majflt == native.ru_majflt);
    Require(children.ru_nvcsw == native.ru_nvcsw && children.ru_nivcsw == native.ru_nivcsw);
#endif
}
int main() {
    GuestResourceUsage untouched;
    std::memset(&untouched, 0x5a, sizeof(untouched));
    const GuestResourceUsage pattern = untouched;
    RequireFailure(2, &untouched, 22);
    RequireFailure(-2, &untouched, 22);
    RequireFailure(INT_MIN, &untouched, 22);
    RequireFailure(INT_MAX, &untouched, 22);
    Require(std::memcmp(&untouched, &pattern, sizeof(untouched)) == 0);
    RequireFailure(7, nullptr, 22);
    RequireFailure(0, nullptr, 14);
    RequireFailure(1, nullptr, 14);
    RequireFailure(-1, nullptr, 14);
    auto* const invalid = reinterpret_cast<GuestResourceUsage*>(static_cast<std::uintptr_t>(1));
    RequireFailure(2, invalid, 22);
    RequireFailure(0, invalid, 14);
    RequireFailure(1, invalid, 14);
    RequireFailure(-1, invalid, 14);
#ifndef _WIN32
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) ::_exit(0);
    int status = -1;
    Require(::waitpid(child, &status, 0) == child && status == 0);
#endif
    RequireChildren();
    RequirePeak(0);
#ifndef __APPLE__
    RequirePeak(1);
#endif
    const std::int64_t beforeTouch = MaxResident(0);
    std::vector<unsigned char> touched(64u << 20, 1);
    volatile unsigned char* pages = touched.data();
    for (std::size_t i = 0; i < touched.size(); i += 4096) pages[i] = 1;
    const std::int64_t afterTouch = MaxResident(0);
    Require(afterTouch >= beforeTouch && afterTouch >= 60000);
    RequirePeak(0);
    SpinUntil(0, 400000);
    const std::int64_t mainThread = CpuMicros(1);
    Require(mainThread >= 200000);
    std::thread worker([] {
        const std::int64_t fresh = CpuMicros(1);
        Require(fresh < 100000);
        SpinUntil(1, fresh + 100000);
        Require(CpuMicros(0) >= CpuMicros(1) + 300000);
    });
    worker.join();
    Require(CpuMicros(0) >= 500000);
    RequireChildren();
}
