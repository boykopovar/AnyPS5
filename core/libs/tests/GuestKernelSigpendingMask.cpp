#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static volatile int g_receivedSignal = 0;

static void APS5_VABI SignalHandler(int value) {
    g_receivedSignal = value;
}

static void TestNullPointerRejection() {
    *__error_nid_postfix() = 0;
    Require(sigpending_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == 14);
}

static void TestEmptyPendingSet() {
    GuestSignalSet set{};
    Require(sigfillset_nid_postfix(&set) == 0);
    *__error_nid_postfix() = 42;
    Require(sigpending_nid_postfix(&set) == 0);
    Require(*__error_nid_postfix() == 42);
    Require(set.bits[0] == 0 && set.bits[1] == 0 && set.bits[2] == 0 && set.bits[3] == 0);

    const int testSignals[] = {1, 15, 32, 33, 64, 65, 96, 97, 128};
    for (int sig : testSignals) {
        Require(sigismember_nid_postfix(&set, sig) == 0);
    }
}

static void TestBlockedSignalPendingAndSigismember() {
    const auto invalidHandler = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    Require(signal_nid_postfix(15, SignalHandler) != invalidHandler);

    GuestSignalSet blockMask{};
    Require(sigemptyset_nid_postfix(&blockMask) == 0);
    Require(sigaddset_nid_postfix(&blockMask, 15) == 0);
    Require(sigismember_nid_postfix(&blockMask, 15) == 1);

    Require(sigprocmask_nid_postfix(1, &blockMask, nullptr) == 0);

    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 15) == 0);

    g_receivedSignal = 0;
    Require(raise_nid_postfix(15) == 0);
    Require(g_receivedSignal == 0);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 15) == 1);
    Require((pending.bits[0] & (1u << 14)) != 0);
    Require(pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);

    Require(sigismember_nid_postfix(&pending, 1) == 0);
    Require(sigismember_nid_postfix(&pending, 14) == 0);
    Require(sigismember_nid_postfix(&pending, 16) == 0);
    Require(sigismember_nid_postfix(&pending, 32) == 0);
    Require(sigismember_nid_postfix(&pending, 40) == 0);
    Require(sigismember_nid_postfix(&pending, 128) == 0);

    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(&pending, 0) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(&pending, -1) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(&pending, 129) == -1 && *__error_nid_postfix() == 22);

    Require(sigprocmask_nid_postfix(2, &blockMask, nullptr) == 0);
    Require(g_receivedSignal == 15);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 15) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);

    Require(signal_nid_postfix(15, nullptr) == SignalHandler);
}

static void TestMultipleBlockedSignalsAcrossWords() {
    const auto invalidHandler = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    const int signals[] = {15, 40, 73, 128};
    for (int sig : signals) {
        Require(signal_nid_postfix(sig, SignalHandler) != invalidHandler);
    }

    GuestSignalSet multiMask{};
    Require(sigemptyset_nid_postfix(&multiMask) == 0);
    for (int sig : signals) {
        Require(sigaddset_nid_postfix(&multiMask, sig) == 0);
    }

    Require(sigprocmask_nid_postfix(1, &multiMask, nullptr) == 0);

    Require(raise_nid_postfix(40) == 0);
    Require(raise_nid_postfix(128) == 0);

    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 15) == 0);
    Require(sigismember_nid_postfix(&pending, 40) == 1);
    Require(sigismember_nid_postfix(&pending, 73) == 0);
    Require(sigismember_nid_postfix(&pending, 128) == 1);
    Require(pending.bits[0] == 0);
    Require(pending.bits[1] == (1u << 7));
    Require(pending.bits[2] == 0);
    Require(pending.bits[3] == (1u << 31));

    Require(raise_nid_postfix(15) == 0);
    Require(raise_nid_postfix(73) == 0);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 15) == 1);
    Require(sigismember_nid_postfix(&pending, 40) == 1);
    Require(sigismember_nid_postfix(&pending, 73) == 1);
    Require(sigismember_nid_postfix(&pending, 128) == 1);

    Require(sigprocmask_nid_postfix(2, &multiMask, nullptr) == 0);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);
    for (int sig : signals) {
        Require(sigismember_nid_postfix(&pending, sig) == 0);
        Require(signal_nid_postfix(sig, nullptr) == SignalHandler);
    }
}

static void TestDiscardPendingWhenIgnored() {
    const GuestSignalSet userSignal{{1u << 29, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(1, &userSignal, nullptr) == 0);

    const auto invalidHandler = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    Require(signal_nid_postfix(30, SignalHandler) != invalidHandler);
    g_receivedSignal = 0;
    Require(raise_nid_postfix(30) == 0 && g_receivedSignal == 0);

    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 30) == 1);

    const GuestSigaction ignored{1, 0, {}};
    Require(sigaction_nid_postfix(30, &ignored, nullptr) == 0);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, 30) == 0);
    Require(pending.bits[0] == 0);

    const GuestSigaction restored{0, 0, {}};
    Require(sigaction_nid_postfix(30, &restored, nullptr) == 0);
    Require(sigprocmask_nid_postfix(2, &userSignal, nullptr) == 0);
}

int main() {
    TestNullPointerRejection();
    TestEmptyPendingSet();
    TestBlockedSignalPendingAndSigismember();
    TestMultipleBlockedSignalsAcrossWords();
    TestDiscardPendingWhenIgnored();
    return 0;
}
