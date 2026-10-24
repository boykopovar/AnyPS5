#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

using Handler = void (APS5_VABI *)(int);

struct GuestSignalSet {
    std::uint32_t bits[4];
};

struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};

static_assert(sizeof(GuestSignalSet) == 16 && alignof(GuestSignalSet) == 4);
static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);

extern "C" {
extern GuestSignalSet _sigintr_nid_postfix;
int* APS5_VABI __error_nid_postfix();
int APS5_VABI siginterrupt_nid_postfix(int, int);
int APS5_VABI sigaction_nid_postfix(int, const GuestSigaction*, GuestSigaction*);
Handler APS5_VABI signal_nid_postfix(int, Handler);
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

bool Same(const GuestSigaction& first, const GuestSigaction& second) {
    return first.handler == second.handler && first.flags == second.flags &&
        std::memcmp(&first.mask, &second.mask, sizeof(first.mask)) == 0;
}

void APS5_VABI HandlerFunction(int) {}

}

int main() {
    const GuestSignalSet initial = _sigintr_nid_postfix;
    Require(initial.bits[0] == 0 && initial.bits[1] == 0 && initial.bits[2] == 0 && initial.bits[3] == 0, "initial interrupt set");
    for (int guest : {1, 32, 33, 64, 65, 96, 97, 128}) {
        GuestSigaction original{};
        Require(sigaction_nid_postfix(guest, nullptr, &original) == 0, "read original disposition");
        GuestSigaction custom{reinterpret_cast<std::uintptr_t>(HandlerFunction), 0x42,
            {{0x11111111, 0x22222222, 0x33333333, 0x44444444}}};
        Require(sigaction_nid_postfix(guest, &custom, nullptr) == 0, "install custom disposition");
        Require(sigaction_nid_postfix(guest, nullptr, &custom) == 0, "read installed disposition");
        const auto word = static_cast<unsigned>(guest - 1) / 32;
        const auto bit = std::uint32_t{1} << (static_cast<unsigned>(guest - 1) % 32);
        GuestSignalSet expected{{0x55555555, 0xaaaaaaaa, 0x55555555, 0xaaaaaaaa}};
        expected.bits[word] &= ~bit;
        _sigintr_nid_postfix = expected;
        for (int flag : {1, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            *__error_nid_postfix() = 12345;
            Require(siginterrupt_nid_postfix(guest, flag) == 0, "nonzero flag");
            Require(*__error_nid_postfix() == 12345, "successful errno preservation");
            expected.bits[word] |= bit;
            Require(std::memcmp(&_sigintr_nid_postfix, &expected, sizeof(expected)) == 0, "set bit without changing neighboring bits");
            GuestSigaction actual{};
            Require(sigaction_nid_postfix(guest, nullptr, &actual) == 0, "read interrupted disposition");
            auto interrupted = custom;
            interrupted.flags &= ~0x2;
            Require(Same(actual, interrupted), "preserve handler, mask and other flags");
        }
        for (int repeat = 0; repeat < 2; ++repeat) {
            *__error_nid_postfix() = 23456;
            Require(siginterrupt_nid_postfix(guest, 0) == 0, "clear interrupt bit");
            Require(*__error_nid_postfix() == 23456, "zero flag errno preservation");
            expected.bits[word] &= ~bit;
            Require(std::memcmp(&_sigintr_nid_postfix, &expected, sizeof(expected)) == 0, "clear bit without changing neighboring bits");
            GuestSigaction actual{};
            Require(sigaction_nid_postfix(guest, nullptr, &actual) == 0 && Same(actual, custom), "restore restart without changing disposition");
        }
        Require(siginterrupt_nid_postfix(guest, -1) == 0, "make later signal interruptible");
        Require(signal_nid_postfix(guest, HandlerFunction) == HandlerFunction, "subsequent signal preserves previous handler");
        GuestSigaction actual{};
        Require(sigaction_nid_postfix(guest, nullptr, &actual) == 0 && actual.flags == 0, "signal consults shared interrupt set");
        Require(siginterrupt_nid_postfix(guest, 0) == 0, "make later signal restartable");
        Require(signal_nid_postfix(guest, HandlerFunction) == HandlerFunction, "repeat signal registration");
        Require(sigaction_nid_postfix(guest, nullptr, &actual) == 0 && actual.flags == 0x2, "signal restores restart");
        Require(sigaction_nid_postfix(guest, &original, nullptr) == 0, "restore original disposition");
        _sigintr_nid_postfix = initial;
    }
    const GuestSignalSet sentinels{{0x12345678, 0xabcdef01, 0x23456789, 0xbcdef012}};
    _sigintr_nid_postfix = sentinels;
    GuestSigaction before{};
    Require(sigaction_nid_postfix(30, nullptr, &before) == 0, "read invalid-call control");
    for (int guest : {0, -1, 129, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (int flag : {0, 1, -1}) {
            *__error_nid_postfix() = 34567;
            Require(siginterrupt_nid_postfix(guest, flag) == -1 && *__error_nid_postfix() == 22, "invalid signal reports EINVAL");
            Require(std::memcmp(&_sigintr_nid_postfix, &sentinels, sizeof(sentinels)) == 0, "invalid signal leaves all words unchanged");
        }
    }
    GuestSigaction after{};
    Require(sigaction_nid_postfix(30, nullptr, &after) == 0 && Same(before, after), "invalid calls preserve dispositions");
    _sigintr_nid_postfix = initial;
}
