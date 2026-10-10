#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <thread>
using Handler = void (APS5_VABI *)(int);
extern "C" {
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI raise_nid_postfix(int);
int APS5_VABI sigaction_nid_postfix(int, const void*, void*);
int APS5_VABI sigprocmask_nid_postfix(int, const void*, void*);
int APS5_VABI pthread_sigmask_nid_postfix(int, const void*, void*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t);
unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int);
}
struct GuestSignalSet {
    std::uint32_t bits[4];
};
struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};
static_assert(sizeof(GuestSigaction) == 32);
static bool Same(const GuestSigaction& left, const GuestSigaction& right) {
    return left.handler == right.handler && left.flags == right.flags && std::memcmp(&left.mask, &right.mask, sizeof(left.mask)) == 0;
}
volatile std::sig_atomic_t received = 0;
volatile std::uintptr_t handlerReturn = 0;
void APS5_VABI InfoCallback(int, void*, void*) {}
void APS5_VABI Callback(int value) {
    received = value;
    handlerReturn = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const auto invalid = reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1));
    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    Require(signal_nid_postfix(9, Callback) == invalid);
    Require(*__error_nid_postfix() == 22);
    Require(signal_nid_postfix(15, Callback) != invalid);
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(handlerReturn != 0 && _is_signal_return_nid_postfix(handlerReturn) == 0);
    Require(_is_signal_return_nid_postfix(reinterpret_cast<std::uintptr_t>(Callback)) == 0);
    Require(_is_signal_return_nid_postfix(0) == 0);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(signal_nid_postfix(15, ignore) != invalid);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 0);
    Require(signal_nid_postfix(15, nullptr) == ignore);
    Require(raise_nid_postfix(100) == -1 && *__error_nid_postfix() == 22);
    GuestSignalSet blocked{{0x20, 0, 0, 0}};
    GuestSignalSet previous{{}};
    Require(sigprocmask_nid_postfix(3, &blocked, &previous) == 0);
    Require(previous.bits[0] == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0);
    Require(previous.bits[0] == 0x20);
    Require(sigprocmask_nid_postfix(2, &blocked, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0);
    Require(previous.bits[0] == 0);
    *__error_nid_postfix() = 0;
    Require(pthread_sigmask_nid_postfix(1, &blocked, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(0, &blocked, &previous) == 22);
    Require(pthread_sigmask_nid_postfix(4, &blocked, &previous) == 22);
    Require(*__error_nid_postfix() == 0);
    Require(pthread_sigmask_nid_postfix(0, nullptr, &previous) == 0);
    Require(previous.bits[0] == 0x20);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 0x20);
    Require(pthread_sigmask_nid_postfix(2, &blocked, &previous) == 0 && previous.bits[0] == 0x20);
    Require(pthread_sigmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 0);

    const auto callback = reinterpret_cast<std::uintptr_t>(Callback);
    const auto infoCallback = reinterpret_cast<std::uintptr_t>(InfoCallback);
    GuestSigaction current{};
    std::memset(&current, 0xa5, sizeof(current));
    Require(sigaction_nid_postfix(15, nullptr, &current) == 0);
    Require(current.handler == 0 && current.flags == 0x2 && current.mask.bits[0] == 0 && current.mask.bits[3] == 0);
    const GuestSigaction term{callback, 0, {{0x4000, 0, 0, 0x80000000u}}};
    Require(sigaction_nid_postfix(15, &term, &current) == 0 && current.handler == 0);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(sigaction_nid_postfix(15, nullptr, &current) == 0);
    Require(Same(current, term));
    const GuestSigaction defaults{0, 0, {}};
    Require(sigaction_nid_postfix(15, &defaults, nullptr) == 0);
    Require(signal_nid_postfix(15, Callback) == nullptr);
    Require(sigaction_nid_postfix(15, nullptr, &current) == 0 && current.handler == callback && current.flags == 0x2);
    Require(signal_nid_postfix(15, nullptr) == Callback);

    const GuestSigaction crash{infoCallback, 0x40 | 0x10, {}};
    for (int recorded : {10, 1, 64, 128}) {
        Require(sigaction_nid_postfix(recorded, &crash, nullptr) == 0);
        Require(sigaction_nid_postfix(recorded, nullptr, &current) == 0);
        Require(Same(current, crash));
        Require(sigaction_nid_postfix(recorded, &defaults, nullptr) == 0);
    }
    Require(sigaction_nid_postfix(6, &crash, nullptr) == 0);
    bool rejected = false;
    try {
        raise_nid_postfix(6);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
    Require(sigaction_nid_postfix(6, &defaults, &current) == 0 && current.handler == infoCallback && current.flags == 0x50);

    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(0, nullptr, &current) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(129, nullptr, &current) == -1 && *__error_nid_postfix() == 22);
    for (int fixed : {9, 17}) {
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(fixed, &term, &current) == -1 && *__error_nid_postfix() == 22);
        Require(sigaction_nid_postfix(fixed, &defaults, &current) == 0 && current.handler == 0);
    }
    Require(sigaction_nid_postfix(15, nullptr, nullptr) == 0);
    Require(signal_nid_postfix(15, Callback) != invalid);
    GuestSignalSet termMask{{1u << 14, 0, 0, 0}};
    received = 0;
    Require(sigprocmask_nid_postfix(1, &termMask, nullptr) == 0);
    Require(raise_nid_postfix(15) == 0 && received == 0);
    Require(sigprocmask_nid_postfix(2, &termMask, nullptr) == 0);
    Require(raise_nid_postfix(15) == 0 && received == 15);
    GuestSignalSet urgMask{{1u << 15, 0, 0, 0}};
    received = 0;
    Require(sigprocmask_nid_postfix(1, &urgMask, nullptr) == 0);
    Require(raise_nid_postfix(15) == 0 && received == 15);
    Require(sigprocmask_nid_postfix(2, &urgMask, nullptr) == 0);

    Require(GuestAlarm_nid_no_patch(0) == 0);
    Require(GuestAlarm_nid_no_patch(5) == 0);
    Require(GuestAlarm_nid_no_patch(3) == 5);
    Require(GuestAlarm_nid_no_patch(0) == 3);
    Require(GuestAlarm_nid_no_patch(0) == 0);
    const GuestSigaction ignoreAlarm{1, 0, {}};
    Require(sigaction_nid_postfix(14, &ignoreAlarm, nullptr) == 0);
    Require(GuestAlarm_nid_no_patch(1) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    Require(GuestAlarm_nid_no_patch(0) == 0);
}
