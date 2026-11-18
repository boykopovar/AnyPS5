#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

namespace {

constexpr int GuestEperm = 1;
constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys adjtime: %s\n", message);
        std::abort();
    }
}

void RequirePermissionDenied(const KernelTimeval& delta) {
    KernelTimeval remaining{-3, -2};
    Require(adjtime_nid_postfix(&delta, &remaining) == -1 && *__error_nid_postfix() == GuestEperm, "jailed clock adjustment did not fail with EPERM");
    Require(remaining.tv_sec == -3 && remaining.tv_usec == -2, "denied adjustment modified oldDelta");
    Require(adjtime_nid_postfix(&delta, reinterpret_cast<KernelTimeval*>(1)) == -1 && *__error_nid_postfix() == GuestEperm, "adjtime checked oldDelta before denying the adjustment");
}

} // namespace

int main() {
    *__error_nid_postfix() = 123;
    Require(adjtime_nid_postfix(nullptr, nullptr) == 0, "adjtime with both pointers null did not succeed");
    Require(*__error_nid_postfix() == 123, "successful adjtime query changed guest errno");
    KernelTimeval remaining{-3, -2};
    Require(adjtime_nid_postfix(nullptr, &remaining) == 0, "adjtime with a null delta did not succeed");
    Require(remaining.tv_sec == 0 && remaining.tv_usec == 0, "adjtime did not report a zero pending adjustment");
    Require(adjtime_nid_postfix(nullptr, reinterpret_cast<KernelTimeval*>(1)) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime with an unwritable oldDelta did not fail with EFAULT");
    remaining = {-3, -2};
    Require(adjtime_nid_postfix(reinterpret_cast<const KernelTimeval*>(1), &remaining) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime with an unreadable delta did not fail with EFAULT");
    Require(remaining.tv_sec == -3 && remaining.tv_usec == -2, "adjtime wrote oldDelta before reading delta");
    Require(adjtime_nid_postfix(reinterpret_cast<const KernelTimeval*>(1), reinterpret_cast<KernelTimeval*>(1)) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime checked privileges before reading delta");
    for (const KernelTimeval delta : {KernelTimeval{2, 0}, KernelTimeval{0, -1}, KernelTimeval{0, 0},
            KernelTimeval{std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max()},
            KernelTimeval{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min()}}) RequirePermissionDenied(delta);
    Require(adjtime_nid_postfix(nullptr, &remaining) == 0 && remaining.tv_sec == 0 && remaining.tv_usec == 0, "denied adjustment changed the pending query result");
    std::puts("guest_kernel_sys_adjtime passed");
    return 0;
}
