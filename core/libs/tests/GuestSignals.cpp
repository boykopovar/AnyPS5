#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
using Handler = void (APS5_VABI *)(int);
struct GuestSignalSet { std::uint32_t bits[4]; };
struct GuestSignalAction { Handler handler; int flags; GuestSignalSet mask; };
static_assert(offsetof(GuestSignalAction, mask) == 12 && sizeof(GuestSignalAction) == 32);
extern "C" {
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI sigaction_nid_postfix(int, const GuestSignalAction*, GuestSignalAction*);
int APS5_VABI raise_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}
volatile std::sig_atomic_t received = 0;
void APS5_VABI Callback(int value) { received = value; }
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const auto invalid = reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1));
    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    Require(signal_nid_postfix(9, Callback) == invalid);
    Require(*__error_nid_postfix() == 22);
    Require(signal_nid_postfix(15, Callback) != invalid);
    Require(raise_nid_postfix(15) == 0 && received == 15);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(signal_nid_postfix(15, ignore) != invalid);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 0);
    Require(signal_nid_postfix(15, nullptr) == ignore);
    Require(raise_nid_postfix(100) == -1 && *__error_nid_postfix() == 22);

    GuestSignalAction previous{};
    GuestSignalAction action{Callback, 0, {}};
    Require(sigaction_nid_postfix(15, &action, &previous) == 0 && !previous.handler);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(sigaction_nid_postfix(15, nullptr, &previous) == 0 && previous.handler == Callback);
    action.flags = 2;
    Require(sigaction_nid_postfix(15, &action, nullptr) == -1 && *__error_nid_postfix() == 45);
    action.flags = 0;
    action.mask.bits[0] = 1;
    Require(sigaction_nid_postfix(15, &action, nullptr) == -1 && *__error_nid_postfix() == 45);
    Require(sigaction_nid_postfix(9, nullptr, &previous) == -1 && *__error_nid_postfix() == 22);
    Require(sigaction_nid_postfix(15, nullptr, &previous) == 0 && previous.handler == Callback);
    action.mask.bits[0] = 0;
    action.handler = nullptr;
    Require(sigaction_nid_postfix(15, &action, &previous) == 0 && previous.handler == Callback);
}
