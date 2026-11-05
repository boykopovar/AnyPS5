#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

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

std::uint64_t Deadline(std::uint64_t timeout) {
    return __std_atomic_wait_get_deadline_nid_postfix(timeout);
}

std::uint64_t Remaining(std::uint64_t deadline) {
    return __std_atomic_wait_get_remaining_timeout_nid_postfix(deadline);
}

void CheckNoDeadline() {
    Require(Deadline(NoDeadline) == NoDeadline, "an endless timeout must give no deadline");
    Require(Remaining(NoDeadline) == NoTimeout, "no deadline must give the no-timeout value");
    Require(Deadline(NoTimeout) != NoDeadline, "the 32-bit no-timeout value is an ordinary timeout here");
}

void CheckDeadlineIsNowPlusTimeout() {
    const std::uint64_t before = Deadline(0);
    const std::uint64_t later = Deadline(5000);
    const std::uint64_t after = Deadline(0);
    Require(after >= before, "the clock must not go backwards");
    Require(later >= before + 5000, "a deadline must not be earlier than now plus the timeout");
    Require(later <= after + 5000, "a deadline must not be later than now plus the timeout");
}

void CheckRemaining() {
    Require(Remaining(0) == 0, "a deadline at the epoch has passed");
    Require(Remaining(Deadline(0)) == 0, "a zero timeout has no time left");
    const std::uint64_t left = Remaining(Deadline(Slack));
    Require(left > 0, "a future deadline must have time left");
    Require(left <= Slack, "the time left must not exceed the timeout");
}

void CheckCap() {
    Require(Remaining(Deadline(TenDays + Slack)) == TenDays, "the time left is capped at ten days");
    Require(Remaining(Deadline(TenDays * 1000)) == TenDays, "a far deadline is capped at ten days");
    const std::uint64_t below = Remaining(Deadline(TenDays - Slack));
    Require(below > 0 && below <= TenDays - Slack, "the time left below the cap must be exact");
}

void CheckExpiry() {
    const std::uint64_t deadline = Deadline(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Remaining(deadline) == 0, "an expired deadline has no time left");
}

void CheckWrap() {
    const std::uint64_t before = Deadline(0);
    const std::uint64_t wrapped = Deadline(NoDeadline - 1);
    const std::uint64_t after = Deadline(0);
    Require(wrapped >= before - 2 && wrapped <= after - 2, "the deadline must wrap as unsigned arithmetic");
    Require(Remaining(wrapped) == 0, "a wrapped deadline has already passed");
}

} // namespace

int main() {
    CheckNoDeadline();
    CheckDeadlineIsNowPlusTimeout();
    CheckRemaining();
    CheckCap();
    CheckExpiry();
    CheckWrap();
    return 0;
}
