#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static void TestSigtimedwaitZeroTimeout() {
    GuestSignalSet set{};
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 15) == 0);

    GuestTimespec timeout{0, 0};
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&set, nullptr, &timeout) == -1);
    Require(*__error_nid_postfix() == 35);
}

static void TestSigtimedwaitInvalidNsec() {
    GuestSignalSet set{};
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 15) == 0);

    GuestTimespec timeout{0, -1};
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&set, nullptr, &timeout) == -1);
    Require(*__error_nid_postfix() == 22);

    GuestTimespec timeoutLarge{0, 1000000000};
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&set, nullptr, &timeoutLarge) == -1);
    Require(*__error_nid_postfix() == 22);
}

static void TestSigtimedwaitNullPointerRejection() {
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(nullptr, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
}

int main() {
    TestSigtimedwaitZeroTimeout();
    TestSigtimedwaitInvalidNsec();
    TestSigtimedwaitNullPointerRejection();
    return 0;
}
