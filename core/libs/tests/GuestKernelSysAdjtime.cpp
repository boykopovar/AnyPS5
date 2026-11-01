#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys adjtime: %s\n", message);
        std::abort();
    }
}

void RequireAdjustment(const KernelTimeval& delta) {
    KernelTimeval remaining{-3, -2};
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        adjtime_nid_postfix(&delta, &remaining);
    } catch (const std::runtime_error&) {
        Require(NotImplementedCalls_nid_no_patch() == before + 1, "unsupported adjustment bypassed the unimplemented-call counter");
        Require(remaining.tv_sec == -3 && remaining.tv_usec == -2, "unsupported adjustment modified oldDelta");
        return;
    }
    Require(false, "unsupported adjustment returned success");
}

} // namespace

int main() {
    Require(adjtime_nid_postfix(nullptr, nullptr) == 0, "adjtime with both pointers null did not succeed");
    KernelTimeval remaining{-3, -2};
    Require(adjtime_nid_postfix(nullptr, &remaining) == 0, "adjtime with a null delta did not succeed");
    Require(remaining.tv_sec == 0 && remaining.tv_usec == 0, "adjtime did not report a zero pending adjustment");
    Require(adjtime_nid_postfix(nullptr, reinterpret_cast<KernelTimeval*>(1)) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime with an unwritable oldDelta did not fail with EFAULT");
    remaining = {-3, -2};
    Require(adjtime_nid_postfix(reinterpret_cast<const KernelTimeval*>(1), &remaining) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime with an unreadable delta did not fail with EFAULT");
    Require(remaining.tv_sec == -3 && remaining.tv_usec == -2, "adjtime wrote oldDelta before reading delta");
    for (const KernelTimeval delta : {KernelTimeval{2, 0}, KernelTimeval{0, -1}, KernelTimeval{0, 0}}) RequireAdjustment(delta);
    std::puts("guest_kernel_sys_adjtime passed");
    return 0;
}
