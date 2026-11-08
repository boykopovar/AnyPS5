#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestEfault = 14;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool Equals(const GuestSignalSet& set, std::uint32_t w0, std::uint32_t w1, std::uint32_t w2, std::uint32_t w3) {
    return set.bits[0] == w0 && set.bits[1] == w1 && set.bits[2] == w2 && set.bits[3] == w3;
}

void TestEmptySetLifecycle() {
    GuestSignalSet set{{0x12345678u, 0x9abcdef0u, 0xa5a5a5a5u, 0x5a5a5a5au}};
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(Equals(set, 0, 0, 0, 0));
    for (int sig = 1; sig <= 128; ++sig) {
        Require(sigismember_nid_postfix(&set, sig) == 0);
    }
}

void TestFillSetLifecycle() {
    GuestSignalSet set{{0, 0, 0, 0}};
    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(&set) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    for (int sig = 1; sig <= 128; ++sig) {
        Require(sigismember_nid_postfix(&set, sig) == 1);
    }
}

void TestAddDelMemberLifecycle() {
    GuestSignalSet set{{0, 0, 0, 0}};
    Require(sigemptyset_nid_postfix(&set) == 0);

    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 1u, 0, 0, 0));
    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigismember_nid_postfix(&set, 2) == 0);

    Require(sigaddset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0x80000001u, 0, 0, 0));
    Require(sigismember_nid_postfix(&set, 32) == 1);

    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0, 0));
    Require(sigismember_nid_postfix(&set, 33) == 1);

    Require(sigaddset_nid_postfix(&set, 64) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 0, 0));
    Require(sigismember_nid_postfix(&set, 64) == 1);

    Require(sigaddset_nid_postfix(&set, 65) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 1u, 0));
    Require(sigismember_nid_postfix(&set, 65) == 1);

    Require(sigaddset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 0x80000001u, 0));
    Require(sigismember_nid_postfix(&set, 96) == 1);

    Require(sigaddset_nid_postfix(&set, 97) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 0x80000001u, 1u));
    Require(sigismember_nid_postfix(&set, 97) == 1);

    Require(sigaddset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 0x80000001u, 0x80000001u));
    Require(sigismember_nid_postfix(&set, 128) == 1);

    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000001u, 0x80000001u, 0x80000001u, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000000u, 0x80000001u, 0x80000001u, 0x80000001u));
    Require(sigismember_nid_postfix(&set, 1) == 0);

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000000u, 0x80000001u, 0x80000001u, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0, 0x80000001u, 0x80000001u, 0x80000001u));
    Require(sigismember_nid_postfix(&set, 32) == 0);

    Require(sigdelset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0, 0x80000000u, 0x80000001u, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 64) == 0);
    Require(Equals(set, 0, 0, 0x80000001u, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 65) == 0);
    Require(Equals(set, 0, 0, 0x80000000u, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0, 0, 0, 0x80000001u));

    Require(sigdelset_nid_postfix(&set, 97) == 0);
    Require(Equals(set, 0, 0, 0, 0x80000000u));

    Require(sigdelset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0, 0, 0, 0));
    Require(sigismember_nid_postfix(&set, 128) == 0);
}

void TestSequentialSignals() {
    GuestSignalSet set{{0, 0, 0, 0}};
    for (int sig = 1; sig <= 128; ++sig) {
        Require(sigaddset_nid_postfix(&set, sig) == 0);
        Require(sigismember_nid_postfix(&set, sig) == 1);
        if (sig > 1) {
            Require(sigismember_nid_postfix(&set, sig - 1) == 1);
        }
        if (sig < 128) {
            Require(sigismember_nid_postfix(&set, sig + 1) == 0);
        }
    }
    Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));

    for (int sig = 1; sig <= 128; ++sig) {
        Require(sigdelset_nid_postfix(&set, sig) == 0);
        Require(sigismember_nid_postfix(&set, sig) == 0);
        if (sig < 128) {
            Require(sigismember_nid_postfix(&set, sig + 1) == 1);
        }
    }
    Require(Equals(set, 0, 0, 0, 0));
}

void TestInvalidSignalNumbers() {
    GuestSignalSet set{{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u}};
    const GuestSignalSet baseline{{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u}};

    const int invalidSignals[] = {0, -1, -2, -100, 129, 130, 256, 1024, INT_MIN, INT_MAX};
    for (const int sig : invalidSignals) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
        Require(Equals(set, baseline.bits[0], baseline.bits[1], baseline.bits[2], baseline.bits[3]));

        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
        Require(Equals(set, baseline.bits[0], baseline.bits[1], baseline.bits[2], baseline.bits[3]));

        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }
}

void TestNullPointerValidation() {
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    const int invalidSignals[] = {0, -1, 129, 1024};
    for (const int sig : invalidSignals) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(nullptr, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(nullptr, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(nullptr, sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }
}

void TestErrnoPreservation() {
    GuestSignalSet set{{0, 0, 0, 0}};
    constexpr int SentinelErrno = 42;
    *__error_nid_postfix() = SentinelErrno;

    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(*__error_nid_postfix() == SentinelErrno);

    Require(sigaddset_nid_postfix(&set, 5) == 0);
    Require(*__error_nid_postfix() == SentinelErrno);

    Require(sigismember_nid_postfix(&set, 5) == 1);
    Require(*__error_nid_postfix() == SentinelErrno);

    Require(sigdelset_nid_postfix(&set, 5) == 0);
    Require(*__error_nid_postfix() == SentinelErrno);

    Require(sigfillset_nid_postfix(&set) == 0);
    Require(*__error_nid_postfix() == SentinelErrno);
}

} // namespace

int main() {
    TestEmptySetLifecycle();
    TestFillSetLifecycle();
    TestAddDelMemberLifecycle();
    TestSequentialSignals();
    TestInvalidSignalNumbers();
    TestNullPointerValidation();
    TestErrnoPreservation();
    return 0;
}
