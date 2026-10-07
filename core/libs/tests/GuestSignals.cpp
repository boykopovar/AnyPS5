#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>
using Handler = void(APS5_VABI*)(int);
extern "C" {
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI raise_nid_postfix(int);
int APS5_VABI sigprocmask_nid_postfix(int, const void*, void*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t);
}
struct GuestSignalSet {
    std::uint32_t bits[4];
};
volatile std::sig_atomic_t received = 0;
volatile std::uintptr_t handlerReturn = 0;
void APS5_VABI Callback(int value) {
    received = value;
    handlerReturn = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
}
static void Require(bool value) {
    if (!value)
        std::abort();
}
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
}
