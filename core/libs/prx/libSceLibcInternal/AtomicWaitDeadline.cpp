#include "prx/libSceLibcInternal/include/AtomicWaitDeadline.hpp"

#include <chrono>

namespace {

constexpr std::uint64_t NoDeadline = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t NoTimeout = 0xFFFFFFFFULL;
constexpr std::uint64_t MaximumTimeout = 864000000ULL;

std::uint64_t Now() {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

} // namespace

extern "C" {

std::uint64_t APS5_VABI __std_atomic_wait_get_deadline_nid_postfix(std::uint64_t timeout) {
    if (timeout == NoDeadline) return NoDeadline;
    return Now() + timeout;
}

std::uint64_t APS5_VABI __std_atomic_wait_get_remaining_timeout_nid_postfix(std::uint64_t deadline) {
    if (deadline == NoDeadline) return NoTimeout;
    const std::uint64_t current = Now();
    if (current >= deadline) return 0;
    const std::uint64_t remaining = deadline - current;
    return remaining > MaximumTimeout ? MaximumTimeout : remaining;
}

}
