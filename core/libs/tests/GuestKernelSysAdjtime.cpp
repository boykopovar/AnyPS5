#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int GuestEfault = 14;
constexpr char SecondsAdjustment[] = "adjtime: adjusting the clock by 2.000000 is not supported, the host clock is not changed";
constexpr char SubSecondAdjustment[] = "adjtime: adjusting the clock by 0.-00001 is not supported, the host clock is not changed";

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys adjtime: %s\n", message);
        std::abort();
    }
}

void RequireAdjustment(const KernelTimeval& delta, const char* expected) {
    try {
        adjtime_nid_postfix(&delta, nullptr);
    } catch (const std::runtime_error& value) {
        Require(std::strcmp(value.what(), expected) == 0, value.what());
        return;
    }
    Require(false, expected);
}

} // namespace

int main() {
    Require(adjtime_nid_postfix(nullptr, nullptr) == 0, "adjtime with both pointers null did not succeed");
    KernelTimeval remaining{-3, -2};
    Require(adjtime_nid_postfix(nullptr, &remaining) == 0, "adjtime with a null delta did not succeed");
    Require(remaining.tv_sec == 0 && remaining.tv_usec == 0, "adjtime did not report a zero pending adjustment");
    Require(adjtime_nid_postfix(reinterpret_cast<const KernelTimeval*>(1), nullptr) == -1 && *__error_nid_postfix() == GuestEfault, "adjtime with an unreadable delta did not fail with EFAULT");
    const KernelTimeval seconds{2, 0};
    RequireAdjustment(seconds, SecondsAdjustment);
    const KernelTimeval subSecond{0, -1};
    RequireAdjustment(subSecond, SubSecondAdjustment);
    std::puts("guest_kernel_sys_adjtime passed");
    return 0;
}
