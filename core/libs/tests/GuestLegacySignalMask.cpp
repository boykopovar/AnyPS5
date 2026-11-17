#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <bit>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using Handler = void (APS5_VABI *)(int);

namespace {

using SignalSet = std::array<std::uint32_t, 4>;
volatile std::sig_atomic_t received = 0;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "legacy signal mask: %s\n", message);
        std::abort();
    }
}

SignalSet CurrentMask() {
    SignalSet mask{};
    Require(sigprocmask_nid_postfix(1, nullptr, mask.data()) == 0, "cannot read the current mask");
    return mask;
}

void APS5_VABI Receive(int number) {
    received = number;
}

}

int main() {
    const SignalSet original = CurrentMask();
    SignalSet expected{0x20, 0x81234567, 0x89abcdef, 0xfedcba98};
    Require(sigprocmask_nid_postfix(3, expected.data(), nullptr) == 0, "cannot seed the mask");
    *__error_nid_postfix() = 123;
    Require(sigblock_nid_postfix(0) == 0x20 && CurrentMask() == expected, "an empty block changed the mask");
    Require(sigblock_nid_postfix(std::numeric_limits<int>::min()) == 0x20, "blocking lost the previous mask");
    expected[0] |= 0x80000000;
    Require(CurrentMask() == expected, "blocking did not preserve the existing mask");
    Require(sigblock_nid_postfix(-1) == std::bit_cast<int>(expected[0]), "the previous mask was not signed at bit 31");
    constexpr std::uint32_t unblockable = (1u << 8) | (1u << 16);
    expected[0] = ~unblockable;
    Require(CurrentMask() == expected, "blocking all signals lost upper words or blocked SIGKILL/SIGSTOP");
    constexpr std::uint32_t userSignal = 1u << 29;
    Require(sigsetmask_nid_postfix(userSignal) == std::bit_cast<int>(expected[0]), "replacement lost the previous mask");
    expected = {userSignal, 0, 0, 0};
    Require(CurrentMask() == expected, "replacement did not clear the upper mask words");
    Require(sigsetmask_nid_postfix(-1) == static_cast<int>(userSignal), "a negative replacement lost the previous mask");
    Require(CurrentMask() == SignalSet{~unblockable, 0, 0, 0}, "a negative replacement was sign-extended or kept unmaskable signals");
    Require(sigsetmask_nid_postfix(0) == std::bit_cast<int>(~unblockable), "clearing lost the previous mask");
    Require(CurrentMask() == SignalSet{}, "the signal mask was not cleared");
    Require(*__error_nid_postfix() == 123, "mask operations changed errno");

    const Handler previous = signal_nid_postfix(30, Receive);
    Require(previous != reinterpret_cast<Handler>(std::numeric_limits<std::uintptr_t>::max()), "cannot install the signal handler");
    Require(sigblock_nid_postfix(userSignal) == 0, "cannot block SIGUSR1");
    Require(raise_nid_postfix(30) == 0 && received == 0, "a blocked signal was delivered");
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0 && (pending.bits[0] & userSignal) != 0, "a blocked signal was not queued");
    Require(sigsetmask_nid_postfix(0) == static_cast<int>(userSignal) && received == 30, "clearing the mask did not deliver SIGUSR1");
    Require(sigpending_nid_postfix(&pending) == 0 && (pending.bits[0] & userSignal) == 0, "the delivered signal remained pending");
    Require(signal_nid_postfix(30, previous) == Receive, "cannot restore the signal handler");
    Require(sigprocmask_nid_postfix(3, original.data(), nullptr) == 0, "cannot restore the original mask");
}
