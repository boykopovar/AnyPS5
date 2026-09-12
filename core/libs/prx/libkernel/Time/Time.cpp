#include "prx/libkernel/Time/include/Time.hpp"

#include <cerrno>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static std::uint64_t GetMonotonicNanos() {
#ifdef _WIN32
    static const std::uint64_t freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<std::uint64_t>(f.QuadPart);
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>(counter.QuadPart) * 1000000000ULL / freq;
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
#endif
}

static std::uint64_t GetStartNanos() {
    static const std::uint64_t start = GetMonotonicNanos();
    return start;
}

static void SleepNanos(std::uint64_t nanos) {
    if (nanos == 0) {
        return;
    }
#ifdef _WIN32
    const DWORD millis = static_cast<DWORD>(nanos / 1000000ULL);
    if (millis > 0) {
        Sleep(millis);
    }
#else
    struct timespec req{};
    req.tv_sec = static_cast<time_t>(nanos / 1000000000ULL);
    req.tv_nsec = static_cast<long>(nanos % 1000000000ULL);
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
#endif
}

extern "C" {

std::uint64_t APS5_VABI sceKernelGetProcessTime() {
    return (GetMonotonicNanos() - GetStartNanos()) / 1000ULL;
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter() {
    return GetMonotonicNanos() - GetStartNanos();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency() {
    return 1000000000ULL;
}

int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds) {
    SleepNanos(static_cast<std::uint64_t>(microseconds) * 1000ULL);
    return 0;
}

int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    if (rqtp == nullptr) {
        return -1;
    }
    if (rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000LL) {
        return -1;
    }
    SleepNanos(static_cast<std::uint64_t>(rqtp->tv_sec) * 1000000000ULL +
               static_cast<std::uint64_t>(rqtp->tv_nsec));
    if (rmtp != nullptr) {
        rmtp->tv_sec = 0;
        rmtp->tv_nsec = 0;
    }
    return 0;
}

int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

}
