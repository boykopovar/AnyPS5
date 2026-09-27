#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

struct GuestSignalSet { std::uint32_t bits[4]; };
static_assert(sizeof(GuestSignalSet) == 16);

extern "C" {
int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigfillset_nid_postfix(GuestSignalSet*);
int APS5_VABI sigaddset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigdelset_nid_postfix(GuestSignalSet*, int);
int APS5_VABI sigismember_nid_postfix(const GuestSignalSet*, int);
int APS5_VABI sigisemptyset_nid_postfix(const GuestSignalSet*);
int APS5_VABI sigandset_nid_postfix(GuestSignalSet*, const GuestSignalSet*, const GuestSignalSet*);
int APS5_VABI sigorset_nid_postfix(GuestSignalSet*, const GuestSignalSet*, const GuestSignalSet*);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool valid) { if (!valid) std::abort(); }

int main() {
    struct Guarded { std::uint32_t before = 0x12345678; GuestSignalSet set{}; std::uint32_t after = 0x87654321; } guarded;
    Require(sigemptyset_nid_postfix(&guarded.set) == 0);
    Require(sigisemptyset_nid_postfix(&guarded.set) == 1);
    for (const int signal : {1, 32, 33, 64, 65, 96, 97, 128}) {
        Require(sigaddset_nid_postfix(&guarded.set, signal) == 0);
        Require(sigismember_nid_postfix(&guarded.set, signal) == 1);
    }
    Require(guarded.set.bits[0] == 0x80000001u && guarded.set.bits[1] == 0x80000001u);
    Require(guarded.set.bits[2] == 0x80000001u && guarded.set.bits[3] == 0x80000001u);
    Require(sigdelset_nid_postfix(&guarded.set, 33) == 0);
    Require(sigismember_nid_postfix(&guarded.set, 33) == 0);
    Require(sigismember_nid_postfix(&guarded.set, 34) == 0);
    GuestSignalSet other{};
    Require(sigfillset_nid_postfix(&other) == 0);
    Require(sigandset_nid_postfix(&other, &other, &guarded.set) == 0);
    for (unsigned index = 0; index < 4; ++index) Require(other.bits[index] == guarded.set.bits[index]);
    Require(sigorset_nid_postfix(&other, &other, &guarded.set) == 0);
    Require(sigisemptyset_nid_postfix(&other) == 0);
    Require(guarded.before == 0x12345678 && guarded.after == 0x87654321);
    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(&guarded.set, 0) == -1 && *__error_nid_postfix() == 22);
    Require(sigismember_nid_postfix(&guarded.set, 129) == -1 && *__error_nid_postfix() == 22);
    Require(sigemptyset_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 22);
}
