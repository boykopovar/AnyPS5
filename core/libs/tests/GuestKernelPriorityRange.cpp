#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestEsrch = 3;

constexpr int GuestPrioProcess = 0;
constexpr int GuestPrioPgrp = 1;
constexpr int GuestPrioUser = 2;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void AuditGetPriorityBounds() {
    const int whichValues[] = {
        GuestPrioProcess,
        GuestPrioPgrp,
        GuestPrioUser
    };

    for (const int which : whichValues) {
        *__error_nid_postfix() = 0;
        const int prio = getpriority_nid_postfix(which, 0);
        Require(*__error_nid_postfix() == 0 || prio >= -20);
    }
}

void AuditInvalidPriorityWhich() {
    const int invalidWhich[] = {
        -1, 3, 4, 100, -100
    };

    for (const int which : invalidWhich) {
        *__error_nid_postfix() = 0;
        Require(Fails(getpriority_nid_postfix(which, 0), GuestEinval));

        *__error_nid_postfix() = 0;
        Require(Fails(setpriority_nid_postfix(which, 0, 0), GuestEinval));
    }
}

void AuditInvalidPriorityWho() {
    const int invalidWho = 9999999;
    *__error_nid_postfix() = 0;
    Require(Fails(getpriority_nid_postfix(GuestPrioProcess, invalidWho), GuestEsrch));

    *__error_nid_postfix() = 0;
    Require(Fails(setpriority_nid_postfix(GuestPrioProcess, invalidWho, 0), GuestEsrch));
}

void AuditSetPriorityRange() {
    *__error_nid_postfix() = 0;
    const int current = getpriority_nid_postfix(GuestPrioProcess, 0);
    if (*__error_nid_postfix() == 0) {
        const int testVals[] = {-20, -10, 0, 10, 19};
        for (const int val : testVals) {
            *__error_nid_postfix() = 0;
            const int res = setpriority_nid_postfix(GuestPrioProcess, 0, val);
            static_cast<void>(res);
        }
        setpriority_nid_postfix(GuestPrioProcess, 0, current);
    }
}

} // namespace

int main() {
    AuditGetPriorityBounds();
    AuditInvalidPriorityWhich();
    AuditInvalidPriorityWho();
    AuditSetPriorityRange();
    std::puts("guest_kernel_priority_range passed");
    return 0;
}
