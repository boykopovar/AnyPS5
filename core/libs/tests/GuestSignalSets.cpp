#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

struct GuestSignalSet {
    std::uint32_t bits[4];
};

extern "C" {
int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigfillset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigaddset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigdelset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigismember_nid_postfix(const GuestSignalSet*, int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

static bool Equals(const GuestSignalSet& set, std::uint32_t w0, std::uint32_t w1, std::uint32_t w2, std::uint32_t w3) {
    return set.bits[0] == w0 && set.bits[1] == w1 && set.bits[2] == w2 && set.bits[3] == w3;
}

int main() {
    static constexpr int Untouched = 5;
    GuestSignalSet set{{0x12345678u, 0x9abcdef0u, 0xffffffffu, 1u}};
    *__error_nid_postfix() = Untouched;
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(Equals(set, 0, 0, 0, 0));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigfillset_nid_postfix(&set) == 0);
    Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    for (int signal = 1; signal <= 128; ++signal) Require(sigismember_nid_postfix(&set, signal) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 1u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0x80000001u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0, 0));
    Require(sigaddset_nid_postfix(&set, 65) == 0);
    Require(Equals(set, 0x80000001u, 1u, 1u, 0));
    Require(sigaddset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0));
    Require(sigaddset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigismember_nid_postfix(&set, 2) == 0);
    Require(sigismember_nid_postfix(&set, 32) == 1);
    Require(sigismember_nid_postfix(&set, 33) == 1);
    Require(sigismember_nid_postfix(&set, 64) == 0);
    Require(sigismember_nid_postfix(&set, 65) == 1);
    Require(sigismember_nid_postfix(&set, 96) == 1);
    Require(sigismember_nid_postfix(&set, 97) == 0);
    Require(sigismember_nid_postfix(&set, 128) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000000u, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0, 0, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0, 0, 1u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigdelset_nid_postfix(&set, 2) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigismember_nid_postfix(&set, 128) == 0);
    Require(*__error_nid_postfix() == Untouched);

    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0, 0, 1u, 0));
    }

    Require(sigfillset_nid_postfix(&set) == 0);
    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    }

    const GuestSignalSet empty{{0, 0, 0, 0}};
    Require(sigismember_nid_postfix(&empty, 1) == 0);
    Require(sigismember_nid_postfix(&empty, 128) == 0);
    return 0;
}
