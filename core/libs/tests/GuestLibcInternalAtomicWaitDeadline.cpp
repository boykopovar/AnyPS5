#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

constexpr std::uint64_t NoDeadline = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t NoTimeout = 0xFFFFFFFFULL;
constexpr std::uint64_t TenDays = 864000000ULL;
constexpr std::uint64_t Slack = 60000ULL;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal atomic wait deadline: %s\n", message);
    std::abort();
}

std::uint64_t Clock() {
#ifdef _WIN32
    return ::GetTickCount64();
#else
    struct timespec now{};
    Require(::clock_gettime(CLOCK_MONOTONIC, &now) == 0, "the test's CLOCK_MONOTONIC sample must succeed");
    return static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 1000000ULL;
#endif
}

std::uint64_t Deadline(std::uint64_t timeout) {
    return __std_atomic_wait_get_deadline_nid_postfix(timeout);
}

std::uint64_t Remaining(std::uint64_t deadline) {
    return __std_atomic_wait_get_remaining_timeout_nid_postfix(deadline);
}

void RequireRemainingBetween(std::uint64_t deadline, std::uint64_t upper, const char* message) {
    const std::uint64_t left = Remaining(deadline);
    const std::uint64_t after = Clock();
    const std::uint64_t lower = after < deadline ? deadline - after : 0;
    Require(left <= upper && left >= lower, message);
}

void CheckNoDeadline() {
    Require(Deadline(NoDeadline) == NoDeadline, "an endless timeout must give no deadline");
    Require(Remaining(NoDeadline) == NoTimeout, "no deadline must give the no-timeout value");
    Require(Deadline(NoTimeout) != NoDeadline, "the 32-bit no-timeout value is an ordinary timeout here");
}

void CheckDeadlineSamplesTheClock() {
    const std::uint64_t timeouts[] = {0, 1, 25, 5000, NoDeadline - 1};
    for (const std::uint64_t timeout : timeouts) {
        const std::uint64_t before = Clock();
        const std::uint64_t deadline = Deadline(timeout);
        const std::uint64_t after = Clock();
        Require(after >= before, "the monotonic clock must not go backwards");
        Require(deadline - timeout - before <= after - before, "a deadline must be a monotonic millisecond sample plus the timeout, as unsigned arithmetic");
    }
    Require(Remaining(Deadline(NoDeadline - 1)) == 0, "a wrapped deadline has already passed");
}

void CheckRemaining() {
    Require(Remaining(0) == 0, "a deadline at the epoch has passed");
    Require(Remaining(Clock()) == 0, "a deadline at the current monotonic time has passed");
    Require(Remaining(Deadline(0)) == 0, "a zero timeout has no time left");
    RequireRemainingBetween(Clock() + Slack, Slack, "the time left must lie between two monotonic samples");
    Require(Remaining(Deadline(Slack)) > 0, "a future deadline must have time left");
}

void CheckCap() {
    Require(Remaining(Clock() + TenDays + Slack) == TenDays, "the time left is capped at ten days");
    Require(Remaining(NoDeadline - 1) == TenDays, "a far deadline is capped at ten days");
    RequireRemainingBetween(Clock() + TenDays - Slack, TenDays - Slack, "the time left below the cap must be exact");
}

void CheckExpiry() {
    const std::uint64_t deadline = Deadline(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Remaining(deadline) == 0, "an expired deadline has no time left");
}

} // namespace

int main() {
    CheckNoDeadline();
    CheckDeadlineSamplesTheClock();
    CheckRemaining();
    CheckCap();
    CheckExpiry();
    return 0;
}
