#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

static constexpr int GUEST_RUSAGE_SELF = 0;
static constexpr int GUEST_RUSAGE_CHILDREN = -1;
static constexpr int GUEST_RUSAGE_THREAD = 1;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EFAULT = 14;

static void Require(bool value) {
    if (!value) std::abort();
}

static void RequireFailure(int who, GuestResourceUsage* usage, int expectedError) {
    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(who, usage) == -1);
    Require(*__error_nid_postfix() == expectedError);
}

static void TestRusageSelf() {
    GuestResourceUsage usage{};
    Require(getrusage_nid_postfix(GUEST_RUSAGE_SELF, &usage) == 0);
    Require(usage.ru_utime.tv_sec >= 0);
    Require(usage.ru_utime.tv_usec >= 0 && usage.ru_utime.tv_usec < 1000000);
    Require(usage.ru_stime.tv_sec >= 0);
    Require(usage.ru_stime.tv_usec >= 0 && usage.ru_stime.tv_usec < 1000000);
    Require(usage.ru_maxrss >= 0);
    Require(usage.ru_minflt >= 0);
    Require(usage.ru_majflt >= 0);
    Require(usage.ru_nvcsw >= 0);
    Require(usage.ru_nivcsw >= 0);

    const auto startTotal = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 +
                            usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;

    volatile std::uint64_t sink = 0;
    for (int i = 0; i < 200000; ++i) {
        sink += static_cast<std::uint64_t>(i);
    }
    Require(sink > 0);

    GuestResourceUsage usageAfter{};
    Require(getrusage_nid_postfix(GUEST_RUSAGE_SELF, &usageAfter) == 0);
    Require(usageAfter.ru_utime.tv_sec >= 0);
    Require(usageAfter.ru_utime.tv_usec >= 0 && usageAfter.ru_utime.tv_usec < 1000000);
    Require(usageAfter.ru_stime.tv_sec >= 0);
    Require(usageAfter.ru_stime.tv_usec >= 0 && usageAfter.ru_stime.tv_usec < 1000000);

    const auto afterTotal = (usageAfter.ru_utime.tv_sec + usageAfter.ru_stime.tv_sec) * 1000000 +
                            usageAfter.ru_utime.tv_usec + usageAfter.ru_stime.tv_usec;
    Require(afterTotal >= startTotal);
}

static void TestRusageChildren() {
    GuestResourceUsage usage;
    std::memset(&usage, 0x5a, sizeof(usage));
    Require(getrusage_nid_postfix(GUEST_RUSAGE_CHILDREN, &usage) == 0);

    const GuestResourceUsage zeroUsage{};
    Require(std::memcmp(&usage, &zeroUsage, sizeof(usage)) == 0);
    Require(usage.ru_utime.tv_sec == 0);
    Require(usage.ru_utime.tv_usec == 0);
    Require(usage.ru_stime.tv_sec == 0);
    Require(usage.ru_stime.tv_usec == 0);
    Require(usage.ru_maxrss == 0);
    Require(usage.ru_ixrss == 0);
    Require(usage.ru_idrss == 0);
    Require(usage.ru_isrss == 0);
    Require(usage.ru_minflt == 0);
    Require(usage.ru_majflt == 0);
    Require(usage.ru_nswap == 0);
    Require(usage.ru_inblock == 0);
    Require(usage.ru_oublock == 0);
    Require(usage.ru_msgsnd == 0);
    Require(usage.ru_msgrcv == 0);
    Require(usage.ru_nsignals == 0);
    Require(usage.ru_nvcsw == 0);
    Require(usage.ru_nivcsw == 0);
}

static void TestRusageThread() {
    GuestResourceUsage threadUsage{};
    Require(getrusage_nid_postfix(GUEST_RUSAGE_THREAD, &threadUsage) == 0);
    Require(threadUsage.ru_utime.tv_sec >= 0);
    Require(threadUsage.ru_utime.tv_usec >= 0 && threadUsage.ru_utime.tv_usec < 1000000);
    Require(threadUsage.ru_stime.tv_sec >= 0);
    Require(threadUsage.ru_stime.tv_usec >= 0 && threadUsage.ru_stime.tv_usec < 1000000);

    std::thread worker([] {
        GuestResourceUsage subUsage{};
        Require(getrusage_nid_postfix(GUEST_RUSAGE_THREAD, &subUsage) == 0);
        Require(subUsage.ru_utime.tv_sec >= 0);
        Require(subUsage.ru_utime.tv_usec >= 0 && subUsage.ru_utime.tv_usec < 1000000);
        Require(subUsage.ru_stime.tv_sec >= 0);
        Require(subUsage.ru_stime.tv_usec >= 0 && subUsage.ru_stime.tv_usec < 1000000);
    });
    worker.join();
}

static void TestInvalidWho() {
    GuestResourceUsage untouched;
    std::memset(&untouched, 0xa5, sizeof(untouched));
    const GuestResourceUsage pattern = untouched;

    RequireFailure(-2, &untouched, GUEST_EINVAL);
    RequireFailure(2, &untouched, GUEST_EINVAL);
    RequireFailure(999, &untouched, GUEST_EINVAL);
    RequireFailure(-999, &untouched, GUEST_EINVAL);
    RequireFailure(100, &untouched, GUEST_EINVAL);
    RequireFailure(-100, &untouched, GUEST_EINVAL);
    RequireFailure(INT_MIN, &untouched, GUEST_EINVAL);
    RequireFailure(INT_MAX, &untouched, GUEST_EINVAL);

    Require(std::memcmp(&untouched, &pattern, sizeof(untouched)) == 0);
}

static void TestNullPointer() {
    RequireFailure(GUEST_RUSAGE_SELF, nullptr, GUEST_EFAULT);
    RequireFailure(GUEST_RUSAGE_CHILDREN, nullptr, GUEST_EFAULT);
    RequireFailure(GUEST_RUSAGE_THREAD, nullptr, GUEST_EFAULT);
}

static void TestInvalidPointer() {
    auto* const invalid = reinterpret_cast<GuestResourceUsage*>(static_cast<std::uintptr_t>(1));
    RequireFailure(GUEST_RUSAGE_SELF, invalid, GUEST_EFAULT);
    RequireFailure(GUEST_RUSAGE_CHILDREN, invalid, GUEST_EFAULT);
    RequireFailure(GUEST_RUSAGE_THREAD, invalid, GUEST_EFAULT);
}

static void TestErrorPrecedence() {
    RequireFailure(2, nullptr, GUEST_EINVAL);
    RequireFailure(-2, nullptr, GUEST_EINVAL);
    RequireFailure(999, nullptr, GUEST_EINVAL);
    RequireFailure(-999, nullptr, GUEST_EINVAL);

    auto* const invalid = reinterpret_cast<GuestResourceUsage*>(static_cast<std::uintptr_t>(1));
    RequireFailure(2, invalid, GUEST_EINVAL);
    RequireFailure(-2, invalid, GUEST_EINVAL);
    RequireFailure(999, invalid, GUEST_EINVAL);
    RequireFailure(-999, invalid, GUEST_EINVAL);
}

int main() {
    TestRusageSelf();
    TestRusageChildren();
    TestRusageThread();
    TestInvalidWho();
    TestNullPointer();
    TestInvalidPointer();
    TestErrorPrecedence();
    return 0;
}
