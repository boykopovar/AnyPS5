#include "prx/libc/include/LegacySignals.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "sigmask assertion failed: %s\n", message);
        std::abort();
    }
}

int g_received = 0;
int g_lastSignal = 0;

void APS5_VABI HandleSignal(int signal) {
    ++g_received;
    g_lastSignal = signal;
}

}

int main() {
    constexpr int testSignal = 30;
    const auto handler = reinterpret_cast<std::uintptr_t>(&HandleSignal);
    GuestSigaction action{};
    action.handler = handler;
    GuestSigaction originalAction{};
    Require(sigaction_nid_postfix(testSignal, &action, &originalAction) == 0, "install test signal handler");

    const int initialMask = sigblock_nid_postfix(0);
    Require(sigblock_nid_postfix(0) == initialMask, "sigblock 0 queries current mask");

    const int bit = 1 << (testSignal - 1);
    const int priorMask = sigblock_nid_postfix(bit);
    Require(priorMask == initialMask, "sigblock returns previous mask");
    Require((sigblock_nid_postfix(0) & bit) != 0, "signal bit is blocked");

    g_received = 0;
    Require(raise_nid_postfix(testSignal) == 0, "raise blocked signal");
    Require(g_received == 0, "blocked signal is not delivered immediately");

    const int unblockPrior = sigsetmask_nid_postfix(initialMask);
    Require((unblockPrior & bit) != 0, "sigsetmask returns prior mask with blocked bit");
    Require(g_received == 1 && g_lastSignal == testSignal, "unblocking delivers pending signal");

    const int newMask = (1 << 2) | (1 << 4);
    const int prevSet = sigsetmask_nid_postfix(newMask);
    Require(prevSet == initialMask, "sigsetmask returns initial mask");
    Require(sigblock_nid_postfix(0) == newMask, "current mask matches set mask");

    const int additionalBit = 1 << 6;
    const int prevBlock = sigblock_nid_postfix(additionalBit);
    Require(prevBlock == newMask, "sigblock returns exact prior mask");
    Require(sigblock_nid_postfix(0) == (newMask | additionalBit), "masks combine correctly");

    Require(sigsetmask_nid_postfix(initialMask) == (newMask | additionalBit), "restore initial mask");
    Require(sigaction_nid_postfix(testSignal, &originalAction, nullptr) == 0, "restore original signal action");

    return 0;
}
