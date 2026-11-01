#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockProcessCputimeId = 15;
constexpr int ClockThreadCputimeId = 14;

constexpr std::int64_t NanosPerSecond = 1000000000LL;

struct KernelTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};

struct KernelTimeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

struct KernelTimezone {
    int tz_minuteswest;
    int tz_dsttime;
};

extern "C" {
int nanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int _nanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);

int clock_gettime(int clock_id, KernelTimespec* tp);
int sceKernelClockGettime(int clock_id, KernelTimespec* tp);

int clock_getres(int clock_id, KernelTimespec* res);
int sceKernelClockGetres(int clock_id, KernelTimespec* res);

int clock_settime(int clock_id, const KernelTimespec* tp);
int sceKernelClockSettime(int clock_id, const KernelTimespec* tp);

int gettimeofday(KernelTimeval* tv, KernelTimezone* tz);
int sceKernelGettimeofday(KernelTimeval* tv);
int sceKernelGettimezone(KernelTimezone* tz);

int sceKernelMicrouptime(KernelTimeval* tv);
int sceKernelNanouptime(KernelTimespec* ts);
int sceKernelNanotime(KernelTimespec* ts);
int sceKernelNanoboottime(KernelTimespec* ts);

int* __error();
}

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

void CheckNanosleepNullAndBounds() {
    KernelTimespec rem{-1, -1};

    if (__error() != nullptr) {
        *__error() = 0;
        Check(nanosleep(nullptr, nullptr) == -1 && *__error() == ErrorFault, "nanosleep null rqtp null rmtp fails with EFAULT");
        *__error() = 0;
        Check(nanosleep(nullptr, &rem) == -1 && *__error() == ErrorFault, "nanosleep null rqtp fails with EFAULT");
        *__error() = 0;
        Check(_nanosleep(nullptr, nullptr) == -1 && *__error() == ErrorFault, "_nanosleep null rqtp fails with EFAULT");
    }

    Check(sceKernelNanosleep(nullptr, nullptr) == SceKernelErrorEfault, "sceKernelNanosleep null rqtp null rmtp returns EFAULT");
    Check(sceKernelNanosleep(nullptr, &rem) == SceKernelErrorEfault, "sceKernelNanosleep null rqtp returns EFAULT");

    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec overflowNanos{0, NanosPerSecond};
    const KernelTimespec negativeBoth{-1, -1};

    if (__error() != nullptr) {
        *__error() = 0;
        Check(nanosleep(&negativeNanos, nullptr) == -1 && *__error() == ErrorInvalid, "nanosleep negative tv_nsec fails with EINVAL");
        *__error() = 0;
        Check(nanosleep(&overflowNanos, nullptr) == -1 && *__error() == ErrorInvalid, "nanosleep overflow tv_nsec fails with EINVAL");
        *__error() = 0;
        Check(nanosleep(&negativeBoth, nullptr) == -1 && *__error() == ErrorInvalid, "nanosleep negative both fails with EINVAL");
    }

    Check(sceKernelNanosleep(&negativeNanos, nullptr) == SceKernelErrorEinval, "sceKernelNanosleep negative tv_nsec returns EINVAL");
    Check(sceKernelNanosleep(&overflowNanos, nullptr) == SceKernelErrorEinval, "sceKernelNanosleep overflow tv_nsec returns EINVAL");
    Check(sceKernelNanosleep(&negativeBoth, nullptr) == SceKernelErrorEinval, "sceKernelNanosleep negative both returns EINVAL");
}

void CheckClockGettimeNullAndBounds() {
    const int validClocks[] = {ClockRealtime, ClockMonotonic, ClockProcessCputimeId, ClockThreadCputimeId};

    for (const int clockId : validClocks) {
        if (__error() != nullptr) {
            *__error() = 0;
            Check(clock_gettime(clockId, nullptr) == -1 && *__error() == ErrorFault, "clock_gettime null tp fails with EFAULT");
        }
        Check(sceKernelClockGettime(clockId, nullptr) == SceKernelErrorEfault, "sceKernelClockGettime null tp returns EFAULT");
    }

    KernelTimespec validTs{-1, -1};
    for (const int invalidClock : {-1, 999}) {
        if (__error() != nullptr) {
            *__error() = 0;
            Check(clock_gettime(invalidClock, &validTs) == -1 && *__error() == ErrorInvalid, "clock_gettime invalid clock fails with EINVAL");
        }
        Check(sceKernelClockGettime(invalidClock, &validTs) == SceKernelErrorEinval, "sceKernelClockGettime invalid clock returns EINVAL");
    }
}

void CheckGettimeofdayNullAndBounds() {
    Check(sceKernelGettimeofday(nullptr) == SceKernelErrorEfault, "sceKernelGettimeofday null tv returns EFAULT");

    auto* invalidTv = reinterpret_cast<KernelTimeval*>(static_cast<std::uintptr_t>(0x10));
    auto* invalidTz = reinterpret_cast<KernelTimezone*>(static_cast<std::uintptr_t>(0x10));

    Check(sceKernelGettimeofday(invalidTv) == SceKernelErrorEfault, "sceKernelGettimeofday invalid pointer returns EFAULT");

    Check(gettimeofday(nullptr, nullptr) == 0, "gettimeofday null arguments succeed");

    if (__error() != nullptr) {
        *__error() = 0;
        Check(gettimeofday(invalidTv, nullptr) == -1 && *__error() == ErrorFault, "gettimeofday invalid tv fails with EFAULT");
        *__error() = 0;
        Check(gettimeofday(nullptr, invalidTz) == -1 && *__error() == ErrorFault, "gettimeofday invalid tz fails with EFAULT");
    }

    Check(sceKernelGettimezone(nullptr) == SceKernelErrorEfault, "sceKernelGettimezone null tz returns EFAULT");
}

void CheckAuxiliaryTimeSyscallsNullBounds() {
    Check(sceKernelClockGetres(ClockRealtime, nullptr) == SceKernelErrorEfault, "sceKernelClockGetres null res returns EFAULT");
    Check(clock_getres(ClockRealtime, nullptr) == 0, "clock_getres null res succeeds");

    Check(sceKernelClockSettime(ClockRealtime, nullptr) == SceKernelErrorEfault, "sceKernelClockSettime null tp returns EFAULT");
    if (__error() != nullptr) {
        *__error() = 0;
        Check(clock_settime(ClockRealtime, nullptr) == -1 && *__error() == ErrorFault, "clock_settime null tp fails with EFAULT");
    }

    Check(sceKernelMicrouptime(nullptr) == SceKernelErrorEfault, "sceKernelMicrouptime null tv returns EFAULT");
    Check(sceKernelNanouptime(nullptr) == SceKernelErrorEfault, "sceKernelNanouptime null ts returns EFAULT");
    Check(sceKernelNanotime(nullptr) == SceKernelErrorEfault, "sceKernelNanotime null ts returns EFAULT");
    Check(sceKernelNanoboottime(nullptr) == SceKernelErrorEfault, "sceKernelNanoboottime null ts returns EFAULT");
}

}

int main() {
    CheckNanosleepNullAndBounds();
    CheckClockGettimeNullAndBounds();
    CheckGettimeofdayNullAndBounds();
    CheckAuxiliaryTimeSyscallsNullBounds();

    if (failures != 0) {
        return 1;
    }
    std::printf("all time syscall comprehensive negative bounds tests passed\n");
    return 0;
}
