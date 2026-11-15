#include "prx/libSceLibcInternal/include/AtomicWaitDeadline.hpp"

#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

constexpr std::uint64_t NoDeadline = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t NoTimeout = 0xFFFFFFFFULL;
constexpr std::uint64_t MaximumTimeout = 864000000ULL;

std::uint64_t Now() {
#ifdef _WIN32
    return ::GetTickCount64();
#else
    struct timespec now{};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) throw std::runtime_error("atomic wait deadline: reading CLOCK_MONOTONIC failed");
    return static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 1000000ULL;
#endif
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
