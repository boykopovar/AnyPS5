#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
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

constexpr int TimerAbstime = 1;

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

int clock_nanosleep(int clock_id, int flags, const KernelTimespec* rqtp, KernelTimespec* rmtp);
int _clock_nanosleep(int clock_id, int flags, const KernelTimespec* rqtp, KernelTimespec* rmtp);
int sceKernelClockNanosleep(int clock_id, int flags, const KernelTimespec* rqtp, KernelTimespec* rmtp);

int gettimeofday(KernelTimeval* tv, KernelTimezone* tz);
int sceKernelGettimeofday(KernelTimeval* tv);
int sceKernelGettimezone(KernelTimezone* tz);

int sceKernelMicrouptime(KernelTimeval* tv);
int sceKernelNanouptime(KernelTimespec* ts);
int sceKernelNanotime(KernelTimespec* ts);
int sceKernelNanoboottime(KernelTimespec* ts);

int* __error();
int* __error_nid_postfix();
}

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

bool IsInvalidResult(int result) {
    if (result == ErrorInvalid || result == SceKernelErrorEinval) {
        return true;
    }
    if (result == -1) {
        if (__error != nullptr && *__error() == ErrorInvalid) {
            return true;
        }
        if (__error_nid_postfix != nullptr && *__error_nid_postfix() == ErrorInvalid) {
            return true;
        }
    }
    return false;
}

bool IsFaultResult(int result) {
    if (result == ErrorFault || result == SceKernelErrorEfault) {
        return true;
    }
    if (result == -1) {
        if (__error != nullptr && *__error() == ErrorFault) {
            return true;
        }
        if (__error_nid_postfix != nullptr && *__error_nid_postfix() == ErrorFault) {
            return true;
        }
    }
    return false;
}

template <typename Func>
bool RejectsWithInvalid(Func&& func) {
    try {
        if (__error != nullptr) {
            *__error() = 0;
        }
        if (__error_nid_postfix != nullptr) {
            *__error_nid_postfix() = 0;
        }
        return IsInvalidResult(func());
    } catch (const std::exception&) {
        return true;
    }
}

template <typename Func>
bool RejectsWithFault(Func&& func) {
    try {
        if (__error != nullptr) {
            *__error() = 0;
        }
        if (__error_nid_postfix != nullptr) {
            *__error_nid_postfix() = 0;
        }
        return IsFaultResult(func());
    } catch (const std::exception&) {
        return true;
    }
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

void CheckClockNanosleepFlagsAndNullRemainderBounds() {
    KernelTimespec rem{-1, -1};
    const KernelTimespec validZero{0, 0};
    const KernelTimespec validPositive{0, 1000};
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec overflowNanos{0, NanosPerSecond};
    const KernelTimespec negativeBoth{-1, -1};
    const KernelTimespec negativeSec{-1, 0};

    auto* invalidRqtp = reinterpret_cast<const KernelTimespec*>(static_cast<std::uintptr_t>(0x10));
    auto* invalidRmtp = reinterpret_cast<KernelTimespec*>(static_cast<std::uintptr_t>(0x20));

    const int validClocks[] = {ClockRealtime, ClockMonotonic};
    const int invalidFlags[] = {-9999, -1, 2, 3, 4, 8, 16, 32, 64, 128, 256, 1024, ~TimerAbstime, 0x7fffffff};

    for (const int clockId : validClocks) {
        for (const int badFlag : invalidFlags) {
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &validZero, nullptr); }), "clock_nanosleep invalid flags null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &validZero, &rem); }), "clock_nanosleep invalid flags with rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &validPositive, nullptr); }), "clock_nanosleep invalid flags positive req null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &validPositive, &rem); }), "clock_nanosleep invalid flags positive req with rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &negativeNanos, nullptr); }), "clock_nanosleep invalid flags negative req null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, badFlag, &validZero, invalidRmtp); }), "clock_nanosleep invalid flags invalid rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return _clock_nanosleep(clockId, badFlag, &validZero, nullptr); }), "_clock_nanosleep invalid flags null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return _clock_nanosleep(clockId, badFlag, &validZero, &rem); }), "_clock_nanosleep invalid flags with rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return sceKernelClockNanosleep(clockId, badFlag, &validZero, nullptr); }), "sceKernelClockNanosleep invalid flags null rmtp returns EINVAL");
            Check(RejectsWithInvalid([&] { return sceKernelClockNanosleep(clockId, badFlag, &validZero, &rem); }), "sceKernelClockNanosleep invalid flags with rmtp returns EINVAL");
        }

        for (const int flag : {0, TimerAbstime}) {
            Check(RejectsWithFault([&] { return clock_nanosleep(clockId, flag, nullptr, nullptr); }), "clock_nanosleep null rqtp null rmtp fails with EFAULT");
            Check(RejectsWithFault([&] { return clock_nanosleep(clockId, flag, nullptr, &rem); }), "clock_nanosleep null rqtp with rmtp fails with EFAULT");
            Check(RejectsWithFault([&] { return clock_nanosleep(clockId, flag, invalidRqtp, nullptr); }), "clock_nanosleep invalid rqtp null rmtp fails with EFAULT");
            Check(RejectsWithFault([&] { return clock_nanosleep(clockId, flag, invalidRqtp, &rem); }), "clock_nanosleep invalid rqtp with rmtp fails with EFAULT");

            Check(RejectsWithFault([&] { return _clock_nanosleep(clockId, flag, nullptr, nullptr); }), "_clock_nanosleep null rqtp null rmtp fails with EFAULT");
            Check(RejectsWithFault([&] { return _clock_nanosleep(clockId, flag, nullptr, &rem); }), "_clock_nanosleep null rqtp with rmtp fails with EFAULT");
            Check(RejectsWithFault([&] { return sceKernelClockNanosleep(clockId, flag, nullptr, nullptr); }), "sceKernelClockNanosleep null rqtp null rmtp returns EFAULT");
            Check(RejectsWithFault([&] { return sceKernelClockNanosleep(clockId, flag, nullptr, &rem); }), "sceKernelClockNanosleep null rqtp with rmtp returns EFAULT");

            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &negativeNanos, nullptr); }), "clock_nanosleep negative nsec null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &negativeNanos, &rem); }), "clock_nanosleep negative nsec with rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &overflowNanos, nullptr); }), "clock_nanosleep overflow nsec null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &overflowNanos, &rem); }), "clock_nanosleep overflow nsec with rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &negativeBoth, nullptr); }), "clock_nanosleep negative both null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, flag, &negativeBoth, &rem); }), "clock_nanosleep negative both with rmtp fails with EINVAL");

            Check(RejectsWithInvalid([&] { return _clock_nanosleep(clockId, flag, &negativeNanos, nullptr); }), "_clock_nanosleep negative nsec null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return _clock_nanosleep(clockId, flag, &overflowNanos, nullptr); }), "_clock_nanosleep overflow nsec null rmtp fails with EINVAL");
            Check(RejectsWithInvalid([&] { return sceKernelClockNanosleep(clockId, flag, &negativeNanos, nullptr); }), "sceKernelClockNanosleep negative nsec null rmtp returns EINVAL");
            Check(RejectsWithInvalid([&] { return sceKernelClockNanosleep(clockId, flag, &overflowNanos, nullptr); }), "sceKernelClockNanosleep overflow nsec null rmtp returns EINVAL");
        }

        Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, 0, &negativeSec, nullptr); }), "clock_nanosleep negative sec null rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return clock_nanosleep(clockId, 0, &negativeSec, &rem); }), "clock_nanosleep negative sec with rmtp fails with EINVAL");
    }

    const int invalidClocks[] = {-9999, -1, 1, 2, 3, 5, 6, 7, 8, 9, 10, ClockThreadCputimeId, ClockProcessCputimeId, 32, 100, 9999};
    for (const int badClock : invalidClocks) {
        Check(RejectsWithInvalid([&] { return clock_nanosleep(badClock, 0, &validZero, nullptr); }), "clock_nanosleep invalid clock null rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return clock_nanosleep(badClock, 0, &validZero, &rem); }), "clock_nanosleep invalid clock with rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return clock_nanosleep(badClock, TimerAbstime, &validZero, nullptr); }), "clock_nanosleep invalid clock abstime null rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return clock_nanosleep(badClock, TimerAbstime, &validZero, &rem); }), "clock_nanosleep invalid clock abstime with rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return _clock_nanosleep(badClock, 0, &validZero, nullptr); }), "_clock_nanosleep invalid clock null rmtp fails with EINVAL");
        Check(RejectsWithInvalid([&] { return sceKernelClockNanosleep(badClock, 0, &validZero, nullptr); }), "sceKernelClockNanosleep invalid clock null rmtp returns EINVAL");
    }
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
    CheckClockNanosleepFlagsAndNullRemainderBounds();
    CheckClockGettimeNullAndBounds();
    CheckGettimeofdayNullAndBounds();
    CheckAuxiliaryTimeSyscallsNullBounds();

    if (failures != 0) {
        return 1;
    }
    std::printf("all time syscall comprehensive negative bounds tests passed\n");
    return 0;
}
