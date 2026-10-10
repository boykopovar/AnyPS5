#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" int* APS5_VABI __error_nid_postfix();

struct GuestSignalSet {
    std::uint32_t bits[4];
};
static_assert(sizeof(GuestSignalSet) == 16);

namespace {
constexpr int MaximumSignal = 128;
constexpr int InvalidArgument = 22;

bool ValidSignal(int signal) { return signal > 0 && signal <= MaximumSignal; }
std::size_t WordIndex(int signal) { return static_cast<std::size_t>(signal - 1) >> 5; }
std::uint32_t Bit(int signal) { return 1u << ((signal - 1) & 31); }
int RejectSignal() {
    *__error_nid_postfix() = InvalidArgument;
    return -1;
}
void RequireSet(const GuestSignalSet* set, const char* function) {
    if (set == nullptr) throw std::invalid_argument(std::string(function) + ": set is null");
}
}

extern "C" {
int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet* set) {
    RequireSet(set, "sigemptyset");
    for (auto& word : set->bits) word = 0;
    return 0;
}

int APS5_VABI sigfillset_nid_postfix(GuestSignalSet* set) {
    RequireSet(set, "sigfillset");
    for (auto& word : set->bits) word = ~0u;
    return 0;
}

int APS5_VABI sigaddset_nid_postfix(GuestSignalSet* set, int signal) {
    RequireSet(set, "sigaddset");
    if (!ValidSignal(signal)) return RejectSignal();
    set->bits[WordIndex(signal)] |= Bit(signal);
    return 0;
}

int APS5_VABI sigdelset_nid_postfix(GuestSignalSet* set, int signal) {
    RequireSet(set, "sigdelset");
    if (!ValidSignal(signal)) return RejectSignal();
    set->bits[WordIndex(signal)] &= ~Bit(signal);
    return 0;
}

int APS5_VABI sigismember_nid_postfix(const GuestSignalSet* set, int signal) {
    RequireSet(set, "sigismember");
    if (!ValidSignal(signal)) return RejectSignal();
    return (set->bits[WordIndex(signal)] & Bit(signal)) != 0 ? 1 : 0;
}
}
