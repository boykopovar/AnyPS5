#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

static_assert(sizeof(GuestItimerval) == 32 && offsetof(GuestItimerval, value) == 16);

extern "C" {

unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int);

using Handler = void (APS5_VABI *)(int);

}

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::int64_t ReadRemaining() {
    struct {
        std::uint64_t before;
        GuestItimerval timer;
        std::uint64_t after;
    } guarded;
    std::memset(&guarded, 0xA5, sizeof(guarded));
    *__error_nid_postfix() = 12345;
    Require(getitimer_nid_postfix(0, &guarded.timer) == 0, "getitimer failed");
    Require(*__error_nid_postfix() == 12345, "getitimer changed errno on success");
    Require(guarded.before == 0xA5A5A5A5A5A5A5A5ull && guarded.after == guarded.before, "getitimer wrote outside itimerval");
    Require(guarded.timer.interval.tv_sec == 0 && guarded.timer.interval.tv_usec == 0, "alarm acquired a repeat interval");
    Require(guarded.timer.value.tv_sec >= 0 && guarded.timer.value.tv_usec >= 0
        && guarded.timer.value.tv_usec < 1000000, "invalid timeval normalization");
    return guarded.timer.value.tv_sec * 1000000 + guarded.timer.value.tv_usec;
}

int main() {
    GuestAlarm_nid_no_patch(0);
    Require(ReadRemaining() == 0, "inactive alarm is not zero");

    for (int invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        GuestItimerval output{{11, 22}, {33, 44}};
        const auto saved = output;
        Require(getitimer_nid_postfix(invalid, &output) == -1 && *__error_nid_postfix() == 22, "invalid timer did not report EINVAL");
        Require(std::memcmp(&output, &saved, sizeof(output)) == 0, "invalid timer changed output");
        Require(getitimer_nid_postfix(invalid, nullptr) == -1 && *__error_nid_postfix() == 22, "invalid timer validation order changed");
    }
    Require(getitimer_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == 14, "null output did not report EFAULT");
    for (int disarmed : {1, 2}) {
        GuestItimerval output{{11, 22}, {33, 44}};
        *__error_nid_postfix() = 12345;
        Require(getitimer_nid_postfix(disarmed, &output) == 0, "CPU timer read failed");
        Require(*__error_nid_postfix() == 12345, "CPU timer read changed errno");
        Require(output.interval.tv_sec == 0 && output.interval.tv_usec == 0 && output.value.tv_sec == 0 && output.value.tv_usec == 0,
            "a CPU timer that cannot be armed did not read as disarmed");
        Require(getitimer_nid_postfix(disarmed, nullptr) == -1 && *__error_nid_postfix() == 14, "null CPU timer output did not report EFAULT");
    }

    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    Require(signal_nid_postfix(14, ignore) == nullptr, "SIGALRM was not initially defaulted");
    Require(GuestAlarm_nid_no_patch(30) == 0, "alarm was already active");
    const auto first = ReadRemaining();
    Require(first > 0 && first <= 30000000, "armed alarm is not visible");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    Require(ReadRemaining() < first, "getitimer does not count down");
    std::array<std::thread, 4> readers;
    for (auto& reader : readers) reader = std::thread([] {
        for (int i = 0; i < 100; ++i) Require(ReadRemaining() > 0, "reading consumed the alarm");
    });
    for (auto& reader : readers) reader.join();
    Require(GuestAlarm_nid_no_patch(0) > 0, "getitimer cancelled the alarm");
    Require(ReadRemaining() == 0, "cancelled alarm is not zero");

    constexpr unsigned int longestAlarm = std::numeric_limits<int>::max() / 2;
    Require(GuestAlarm_nid_no_patch(longestAlarm) == 0, "alarm was already active");
    Require(ReadRemaining() > static_cast<std::int64_t>(longestAlarm - 1) * 1000000,
        "long alarm was truncated");
    for (unsigned int invalid : {longestAlarm + 1, static_cast<unsigned int>(std::numeric_limits<int>::max()), std::numeric_limits<unsigned int>::max()}) {
        *__error_nid_postfix() = 0;
        Require(GuestAlarm_nid_no_patch(invalid) == std::numeric_limits<unsigned int>::max() && *__error_nid_postfix() == 22,
            "an alarm beyond the itimer range did not fail with EINVAL");
        Require(ReadRemaining() > static_cast<std::int64_t>(longestAlarm - 1) * 1000000,
            "a rejected alarm replaced the pending one");
    }
    Require(GuestAlarm_nid_no_patch(0) == longestAlarm, "a rejected alarm changed the remaining time");
    GuestAlarm_nid_no_patch(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    Require(ReadRemaining() == 0, "expired alarm is not zero");
    GuestAlarm_nid_no_patch(0);
}
