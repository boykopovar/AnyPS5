#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEperm = 1;

constexpr int GuestRlimitData = 2;
constexpr int GuestRlimitStack = 3;
constexpr int GuestRlimitNofile = 8;
constexpr int GuestRlimitCount = 15;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void AuditSetrlimitPermissions() {
    GuestResourceLimit original{};
    Require(getrlimit_nid_postfix(GuestRlimitData, &original) == 0);

    const GuestResourceLimit invalidExceed{original.rlim_max + 1, original.rlim_max};
    bool threwEperm = false;
    try {
        setrlimit_nid_postfix(GuestRlimitData, &invalidExceed);
    } catch (const std::runtime_error&) {
        threwEperm = true;
    }
    Require(threwEperm);

    const GuestResourceLimit validLower{original.rlim_cur > 0 ? original.rlim_cur - 1 : 0, original.rlim_max};
    Require(setrlimit_nid_postfix(GuestRlimitData, &validLower) == 0);
    Require(setrlimit_nid_postfix(GuestRlimitData, &original) == 0);
}

void AuditSetrlimitInvalidResources() {
    GuestResourceLimit limit{};
    limit.rlim_cur = 1024;
    limit.rlim_max = 1024;

    const int invalidResources[] = {
        -1, GuestRlimitCount, 16, 100, 9999
    };

    for (const int resource : invalidResources) {
        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, &limit), GuestEinval));

        *__error_nid_postfix() = 0;
        Require(Fails(setrlimit_nid_postfix(resource, nullptr), GuestEfault));
    }
}

} // namespace

int main() {
    AuditSetrlimitPermissions();
    AuditSetrlimitInvalidResources();
    std::puts("guest_kernel_setrlimit_perm passed");
    return 0;
}
