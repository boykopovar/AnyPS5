#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <stdexcept>
#include <string>
#include <system_error>
#include <x86intrin.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;

static bool IsCpuClock(int clockId) {
    const auto bits = static_cast<std::uint32_t>(clockId);
    if ((bits & CPU_CLOCK_BIT) == 0)
        return false;
    if ((bits & CPU_CLOCK_PROCESS_BIT) != 0)
        throw std::runtime_error("clock: unsupported process CPU clock_id " + std::to_string(clockId));
    return true;
}

static std::uint64_t RawMonotonicNanos() {
#ifdef _WIN32
    return TimedWait::NowNanos();
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
#endif
}

// Debug aid: APS5_TIME_SCALE=<factor> slows (or speeds) the guest's monotonic clocks and TSC, so a
// title running far below its frame rate sees plausible frame times. Sleeps still take real time.
static double TimeScale() {
    static const double scale = [] {
        const char* value = std::getenv("APS5_TIME_SCALE");
        const double parsed = value ? std::strtod(value, nullptr) : 1.0;
        return parsed > 0.0 ? parsed : 1.0;
    }();
    return scale;
}

static std::uint64_t ClockOrigin() {
    static const std::uint64_t origin = RawMonotonicNanos();
    return origin;
}

static std::uint64_t GetMonotonicNanos() {
    const auto raw = RawMonotonicNanos();
    if (TimeScale() == 1.0) return raw;
    const auto origin = ClockOrigin();
    return origin + static_cast<std::uint64_t>(static_cast<double>(raw - origin) * TimeScale());
}

static std::uint64_t GetStartNanos() {
    static const std::uint64_t start = GetMonotonicNanos();
    return start;
}

#ifdef _WIN32
// Every relative wait in the process (winpthreads nanosleep, std::condition_variable::wait_for, the
// GPU driver's poll sleeps) rounds up to the scheduler tick, 15.6 ms by default; a console game and
// its driver hand labels between queues dozens of times per frame, so the tick is raised to 0.5 ms at
// load, and Windows 11 is told not to drop it while the window is unfocused. APS5_NO_TIMER_RESOLUTION=1
// keeps the default for comparison.
static const bool g_timerResolutionRaised = [] {
    if (std::getenv("APS5_NO_TIMER_RESOLUTION") != nullptr) return false;
    using NtSetTimerResolutionFn = LONG(WINAPI*)(ULONG, BOOLEAN, PULONG);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto setResolution = ntdll ? reinterpret_cast<NtSetTimerResolutionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "NtSetTimerResolution"))) : nullptr;
    ULONG previous = 0;
    const bool raised = setResolution != nullptr && setResolution(5000, TRUE, &previous) >= 0;
    PROCESS_POWER_THROTTLING_STATE throttling{};
    throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    throttling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    throttling.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
    using NtQueryTimerResolutionFn = LONG(WINAPI*)(PULONG, PULONG, PULONG);
    const auto queryResolution = ntdll ? reinterpret_cast<NtQueryTimerResolutionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "NtQueryTimerResolution"))) : nullptr;
    ULONG minimum = 0, maximum = 0, current = 0;
    if (queryResolution) queryResolution(&minimum, &maximum, &current);
    std::fprintf(stderr, "[time] scheduler tick %s (was %.2f ms, now %.2f ms, timed waits %s)\n", raised ? "raised to 0.5 ms" : "unchanged", previous / 10000.0, current / 10000.0, TimedWait::Coarse() ? "coarse" : "fine");
    return raised;
}();

extern "C" {
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
}

static void ReportTimedWait(const char* name, const std::function<void()>& body) {
    std::vector<double> samples(200);
    for (auto& sample : samples) {
        const std::uint64_t start = TimedWait::NowNanos();
        body();
        sample = static_cast<double>(TimedWait::NowNanos() - start) / 1000.0;
    }
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (const double sample : samples) sum += sample;
    std::fprintf(stderr, "[timers] %-44s mean %8.1f  p50 %8.1f  max %8.1f us\n", name, sum / static_cast<double>(samples.size()), samples[samples.size() / 2], samples.back());
}

static const bool g_timerSelfTest = [] {
    if (std::getenv("APS5_TRACE_TIMERS") == nullptr) return false;
    std::mutex mutex;
    std::condition_variable cv;
    ReportTimedWait("std::condition_variable::wait_for(1 ms)", [&] { std::unique_lock<std::mutex> lock(mutex); cv.wait_for(lock, std::chrono::milliseconds(1)); });
    PthreadCond cond = nullptr;
    PthreadMutex guestMutex = nullptr;
    scePthreadCondInit(&cond, nullptr, nullptr);
    scePthreadMutexInit(&guestMutex, nullptr, nullptr);
    scePthreadMutexLock(&guestMutex);
    ReportTimedWait("scePthreadCondTimedwait(1000 us)", [&] { scePthreadCondTimedwait(&cond, &guestMutex, 1000); });
    ReportTimedWait("scePthreadCondTimedwait(13000 us)", [&] { scePthreadCondTimedwait(&cond, &guestMutex, 13000); });
    scePthreadMutexUnlock(&guestMutex);
    scePthreadMutexDestroy(&guestMutex);
    scePthreadCondDestroy(&cond);
    ReportTimedWait("sceKernelUsleep(1000)", [&] { sceKernelUsleep_nid_postfix(1000); });
    ReportTimedWait("Sleep(1)", [&] { Sleep(1); });
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ReportTimedWait("WaitForSingleObject(event, 1)", [&] { WaitForSingleObject(event, 1); });
    CloseHandle(event);
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    ReportTimedWait("high-resolution waitable timer 1 ms", [&] { LARGE_INTEGER due{}; due.QuadPart = -10000; SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE); WaitForSingleObject(timer, INFINITE); });
    CloseHandle(timer);
    return true;
}();
#endif

extern "C" {

std::uint64_t APS5_VABI sceKernelGetProcessTime() {
    const std::uint64_t start = GetStartNanos();
    return (GetMonotonicNanos() - start) / 1000ULL;
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter() {
    const std::uint64_t start = GetStartNanos();
    return GetMonotonicNanos() - start;
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency() {
    return 1000000000ULL;
}

// Debug aid: APS5_TRACE_USLEEP reports every 2000 calls which guest call sites sleep, so a CPU
// thread polling for GPU or I/O completion can be found (offsets are relative to the executable).
static void TraceSleep(const void* caller, std::uint64_t microseconds) {
    static const bool enabled = std::getenv("APS5_TRACE_USLEEP") != nullptr;
    if (!enabled) return;
    static std::mutex mutex;
    static std::unordered_map<std::uintptr_t, std::pair<std::uint64_t, std::uint64_t>> sites;
    static std::uint64_t calls = 0;
    std::lock_guard lock(mutex);
    auto& site = sites[reinterpret_cast<std::uintptr_t>(caller)];
    ++site.first;
    site.second += microseconds;
    if (++calls % 2000 != 0) return;
#ifdef _WIN32
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
#else
    const std::uintptr_t image = 0;
#endif
    std::vector<std::pair<std::uintptr_t, std::pair<std::uint64_t, std::uint64_t>>> hot(sites.begin(), sites.end());
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    std::fprintf(stderr, "[usleep] %llu calls:", static_cast<unsigned long long>(calls));
    for (std::size_t i = 0; i < hot.size() && i < 6; ++i) std::fprintf(stderr, " exe+0x%llx x%llu (%llu us)", static_cast<unsigned long long>(hot[i].first - image), static_cast<unsigned long long>(hot[i].second.first), static_cast<unsigned long long>(hot[i].second.second));
    std::fprintf(stderr, "\n");
}

void KernelTraceWait_nid_postfix(const char* kind, const void* caller, std::uint64_t waitedNanos, bool timedOut) {
    static const bool enabled = std::getenv("APS5_TRACE_WAITS") != nullptr;
    if (!enabled) return;
    struct Site { std::uint64_t calls = 0; std::uint64_t timeouts = 0; std::uint64_t nanos = 0; std::uint64_t longest = 0; unsigned long lastThread = 0; };
    static std::mutex mutex;
    static std::unordered_map<std::string, Site> sites;
    static std::uint64_t lastReport = 0;
    char key[96];
#ifdef _WIN32
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned long thread = GetCurrentThreadId();
#else
    const std::uintptr_t image = 0;
    const unsigned long thread = 0;
#endif
    std::snprintf(key, sizeof(key), "%s exe+0x%llx", kind, static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(caller) - image));
    std::lock_guard lock(mutex);
    auto& site = sites[key];
    ++site.calls;
    site.timeouts += timedOut ? 1u : 0u;
    site.nanos += waitedNanos;
    site.longest = std::max(site.longest, waitedNanos);
    site.lastThread = thread;
    const auto now = GetMonotonicNanos();
    if (lastReport == 0) lastReport = now;
    if (now - lastReport < 3000000000ULL) return;
    lastReport = now;
    std::vector<std::pair<std::string, Site>> hot(sites.begin(), sites.end());
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.nanos > b.second.nanos; });
    std::fprintf(stderr, "[waits]");
    for (std::size_t i = 0; i < hot.size() && i < 8; ++i) {
        const auto& s = hot[i].second;
        std::fprintf(stderr, " | %s x%llu %.1fs (avg %.1f ms, max %.0f ms, %llu timeouts, tid %lu)", hot[i].first.c_str(), static_cast<unsigned long long>(s.calls), s.nanos / 1e9, s.nanos / 1e6 / static_cast<double>(s.calls), s.longest / 1e6, static_cast<unsigned long long>(s.timeouts), s.lastThread);
    }
    std::fprintf(stderr, "\n");
    for (auto& [name, s] : sites) s = Site{};
}

int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds) {
    TraceSleep(__builtin_return_address(0), microseconds);
    TimedWait::SleepNanos(static_cast<std::uint64_t>(microseconds) * 1000ULL);
    return 0;
}

static int SleepForRequest(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    constexpr int guestFault = 14;
    constexpr int guestInvalid = 22;
    if (rqtp == nullptr) return guestFault;
    if (rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000LL) return guestInvalid;
    if (rqtp->tv_sec >= 0) {
        TimedWait::SleepNanos(static_cast<std::uint64_t>(rqtp->tv_sec) * 1000000000ULL +
                              static_cast<std::uint64_t>(rqtp->tv_nsec));
    }
    if (rmtp != nullptr) {
        rmtp->tv_sec = 0;
        rmtp->tv_nsec = 0;
    }
    return 0;
}

int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    const int error = SleepForRequest(rqtp, rmtp);
    return error == 0 ? 0 : SceKernelError(error);
}

int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    const int error = SleepForRequest(rqtp, rmtp);
    if (error == 0) return 0;
    *__error_nid_postfix() = error;
    return -1;
}

int APS5_VABI _nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return nanosleep_nid_postfix(rqtp, rmtp);
}

int APS5_VABI usleep_nid_postfix(KernelUseconds microseconds) {
    TimedWait::SleepNanos(static_cast<std::uint64_t>(microseconds) * 1000ULL);
    return 0;
}

#ifdef _WIN32
static std::uint64_t CpuTimeNanos(bool callingThread, bool countKernel) {
    FILETIME creation{};
    FILETIME exitTime{};
    FILETIME kernel{};
    FILETIME user{};
    if (callingThread) {
        if (!GetThreadTimes(GetCurrentThread(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "clock_gettime: GetThreadTimes failed");
    } else {
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "clock_gettime: GetProcessTimes failed");
    }
    const auto toTicks = [](const FILETIME& time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    return (toTicks(user) + (countKernel ? toTicks(kernel) : 0ULL)) * 100ULL;
}

static std::uint64_t CpuTimeResolutionNanos() {
    DWORD adjustment = 0;
    DWORD increment = 0;
    BOOL adjustmentDisabled = FALSE;
    if (!GetSystemTimeAdjustment(&adjustment, &increment, &adjustmentDisabled))
        throw std::system_error(GetLastError(), std::system_category(), "clock_getres: GetSystemTimeAdjustment failed");
    if (increment == 0)
        throw std::runtime_error("clock_getres: GetSystemTimeAdjustment reported no clock interrupt period");
    return static_cast<std::uint64_t>(increment) * 100ULL;
}

static std::uint64_t ProcessCpuNanos(bool countSystem) {
    return CpuTimeNanos(false, countSystem);
}

static std::uint64_t ProcessCpuResolutionNanos() {
    return CpuTimeResolutionNanos();
}
#else
static std::uint64_t ClockTicksPerSecond() {
    const long rate = sysconf(_SC_CLK_TCK);
    if (rate <= 0)
        throw std::runtime_error("clock_gettime: sysconf(_SC_CLK_TCK) reported no clock tick rate");
    return static_cast<std::uint64_t>(rate);
}

static std::uint64_t ProcessCpuNanos(bool countSystem) {
    struct tms usage{};
    errno = 0;
    if (times(&usage) == static_cast<clock_t>(-1) && errno != 0)
        throw std::system_error(errno, std::generic_category(), "clock_gettime: times failed");
    const std::uint64_t rate = ClockTicksPerSecond();
    const std::uint64_t ticks = static_cast<std::uint64_t>(usage.tms_utime) + (countSystem ? static_cast<std::uint64_t>(usage.tms_stime) : 0ULL);
    return ticks / rate * 1000000000ULL + ticks % rate * 1000000000ULL / rate;
}

static std::uint64_t ProcessCpuResolutionNanos() {
    const std::uint64_t rate = ClockTicksPerSecond();
    return (1000000000ULL + rate - 1ULL) / rate;
}
#endif

int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp) {
    if (tp == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (IsCpuClock(clockId)) {
        std::uint64_t nanos = 0;
        if (!GuestThreadCpuNanos(static_cast<int>(static_cast<std::uint32_t>(clockId) & CPU_CLOCK_ID_MASK), &nanos)) {
            *__error_nid_postfix() = GUEST_EINVAL;
            return -1;
        }
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    if (clockId == 1 || clockId == 2) {
        const std::uint64_t nanos = ProcessCpuNanos(clockId == 2);
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
#ifdef _WIN32
    if (clockId == 0 || clockId == 9) {
        FILETIME ft{};
        GetSystemTimePreciseAsFileTime(&ft);
        std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        t -= 116444736000000000ULL;
        t *= 100ULL;
        tp->tv_sec = static_cast<std::int64_t>(t / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(t % 1000000000ULL);
        return 0;
    }
    if (clockId == 10 || clockId == 13) {
        FILETIME ft{};
        GetSystemTimeAsFileTime(&ft);
        std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        t -= 116444736000000000ULL;
        t *= 100ULL;
        tp->tv_sec = static_cast<std::int64_t>(t / 1000000000ULL);
        tp->tv_nsec = clockId == 13 ? 0 : static_cast<std::int64_t>(t % 1000000000ULL);
        return 0;
    }
    if (clockId == 4 || clockId == 5 || clockId == 7 || clockId == 8 || clockId == 11 || clockId == 12) {
        std::uint64_t nanos = GetMonotonicNanos();
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    if (clockId == 14 || clockId == 15) {
        const std::uint64_t nanos = CpuTimeNanos(clockId == 14, true);
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
#else
    clockid_t nativeId;
    switch (clockId) {
        case 0:
        case 9:
            nativeId = CLOCK_REALTIME;
            break;
        case 10:
        case 13:
#ifdef CLOCK_REALTIME_COARSE
            nativeId = CLOCK_REALTIME_COARSE;
#else
            nativeId = CLOCK_REALTIME;
#endif
            break;
        case 4:
        case 7:
        case 11:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 5:
        case 8:
        case 12:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 14:
            nativeId = CLOCK_THREAD_CPUTIME_ID;
            break;
        case 15:
            nativeId = CLOCK_PROCESS_CPUTIME_ID;
            break;
        default:
            throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
    }
    struct timespec ts{};
    if (clock_gettime(nativeId, &ts) != 0) {
        return -1;
    }
    tp->tv_sec = static_cast<std::int64_t>(ts.tv_sec);
    tp->tv_nsec = clockId == 13 ? 0 : static_cast<std::int64_t>(ts.tv_nsec);
    return 0;
#endif
}

int APS5_VABI gettimeofday_nid_postfix(KernelTimeval* tv, KernelTimezone* tz) {
    if (tv == nullptr) {
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    tv->tv_sec = static_cast<std::int64_t>(t / 10000000ULL);
    tv->tv_usec = static_cast<std::int64_t>((t % 10000000ULL) / 10ULL);
#else
    struct timeval native{};
    if (gettimeofday(&native, nullptr) != 0) {
        return -1;
    }
    tv->tv_sec = static_cast<std::int64_t>(native.tv_sec);
    tv->tv_usec = static_cast<std::int64_t>(native.tv_usec);
#endif
    if (tz != nullptr) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    return 0;
}

int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res) {
    if (res == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (IsCpuClock(clockId))
        return clock_getres_nid_postfix(GUEST_CLOCK_THREAD_CPUTIME_ID, res);
    if (clockId == 1 || clockId == 2) {
        const std::uint64_t nanos = ProcessCpuResolutionNanos();
        res->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        res->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    if (clockId == 13) {
        res->tv_sec = 1;
        res->tv_nsec = 0;
        return 0;
    }
#ifdef _WIN32
    if (clockId == 0 || clockId == 9) {
        res->tv_sec = 0;
        res->tv_nsec = 100LL;
        return 0;
    }
    if (clockId == 4 || clockId == 5 || clockId == 7 || clockId == 8 || clockId == 10 || clockId == 11 || clockId == 12) {
        static const std::uint64_t freq = [] {
            LARGE_INTEGER f{};
            QueryPerformanceFrequency(&f);
            return static_cast<std::uint64_t>(f.QuadPart);
        }();
        std::uint64_t nsPerTick = (1000000000ULL + freq - 1ULL) / freq;
        res->tv_sec = 0;
        res->tv_nsec = static_cast<std::int64_t>(nsPerTick);
        return 0;
    }
    if (clockId == 14 || clockId == 15) {
        const std::uint64_t nanos = CpuTimeResolutionNanos();
        res->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        res->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
#else
    clockid_t nativeId;
    switch (clockId) {
        case 0:
        case 9:
            nativeId = CLOCK_REALTIME;
            break;
        case 10:
#ifdef CLOCK_REALTIME_COARSE
            nativeId = CLOCK_REALTIME_COARSE;
#else
            nativeId = CLOCK_REALTIME;
#endif
            break;
        case 4:
        case 7:
        case 11:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 5:
        case 8:
        case 12:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 14:
            nativeId = CLOCK_THREAD_CPUTIME_ID;
            break;
        case 15:
            nativeId = CLOCK_PROCESS_CPUTIME_ID;
            break;
        default:
            throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
    }
    struct timespec ts{};
    if (clock_getres(nativeId, &ts) != 0) {
        return -1;
    }
    res->tv_sec = static_cast<std::int64_t>(ts.tv_sec);
    res->tv_nsec = static_cast<std::int64_t>(ts.tv_nsec);
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

// The sce* clock calls share the POSIX implementations above. The console clock is kept in UTC:
// the time zone reported here and by gettimeofday has no offset and no daylight saving.
static constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000e);

int APS5_VABI sceKernelClockGetres(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    return clock_getres_nid_postfix(static_cast<int>(clock_id), tp);
}

int APS5_VABI sceKernelClockGettime(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    return clock_gettime_nid_postfix(static_cast<int>(clock_id), tp);
}

int APS5_VABI sceKernelConvertLocaltimeToUtc(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimezone* timezone, int32_t* dst_seconds) {
    (void)reserved;
    if (utc_time != nullptr) *utc_time = local_time;
    if (timezone != nullptr) *timezone = {0, 0};
    if (dst_seconds != nullptr) *dst_seconds = 0;
    return 0;
}

int APS5_VABI sceKernelConvertUtcToLocaltime(int64_t utc_time, int64_t* local_time, KernelTimesec* st, uint64_t* dst_sec) {
    if (local_time != nullptr) *local_time = utc_time;
    if (st != nullptr) *st = {utc_time, 0u, 0u};
    if (dst_sec != nullptr) *dst_sec = 0;
    return 0;
}

int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    return gettimeofday_nid_postfix(tp, nullptr);
}

int APS5_VABI sceKernelGettimezone(KernelTimezone* tz) {
    if (tz == nullptr) return SceKernelErrorEfault;
    *tz = {0, 0};
    return 0;
}

uint64_t APS5_VABI sceKernelReadTsc(void) {
    if (TimeScale() == 1.0) return __rdtsc();
    static const std::uint64_t origin = __rdtsc();
    return origin + static_cast<std::uint64_t>(static_cast<double>(__rdtsc() - origin) * TimeScale());
}

uint64_t APS5_VABI sceKernelGetTscFrequency(void) {
    static const std::uint64_t frequency = [] {
        const std::uint64_t startNanos = RawMonotonicNanos();
        const std::uint64_t startTicks = __rdtsc();
        TimedWait::SleepNanos(20000000ULL);
        const std::uint64_t elapsedNanos = RawMonotonicNanos() - startNanos;
        const std::uint64_t elapsedTicks = __rdtsc() - startTicks;
        return static_cast<std::uint64_t>(static_cast<long double>(elapsedTicks) * 1000000000.0L / static_cast<long double>(elapsedNanos));
    }();
    return frequency;
}

unsigned int APS5_VABI sceKernelSleep(unsigned int seconds) {
    TimedWait::SleepNanos(static_cast<std::uint64_t>(seconds) * 1000000000ULL);
    return 0;
}

}
