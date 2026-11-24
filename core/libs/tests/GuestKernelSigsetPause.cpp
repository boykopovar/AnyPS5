#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static constexpr int GuestSigusr = 30;
static constexpr int GuestSigkill = 9;
static constexpr int GuestEinval = 22;
static constexpr int GuestEintr = 4;

static volatile int g_hits = 0;

static void APS5_VABI Handler(int) {
    g_hits = g_hits + 1;
}

static GuestHandler Hold() {
    return reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(3));
}

static GuestHandler Invalid() {
    return reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
}

static bool Blocked(int guest) {
    GuestSignalSet mask{};
    Require(sigprocmask_nid_postfix(1, nullptr, &mask) == 0);
    return ((mask.bits[(guest - 1) >> 5] >> ((guest - 1) & 31)) & 1u) != 0;
}

static std::uintptr_t Disposition(int guest) {
    GuestSigaction current{};
    Require(sigaction_nid_postfix(guest, nullptr, &current) == 0);
    return current.handler;
}

static void TestHoldBlocksAndReturnsPrevious() {
    Require(Disposition(GuestSigusr) == 0);
    Require(!Blocked(GuestSigusr));
    Require(sigset_nid_postfix(GuestSigusr, Hold()) == nullptr);
    Require(Blocked(GuestSigusr));
    Require(Disposition(GuestSigusr) == 0);

    Require(sigset_nid_postfix(GuestSigusr, Hold()) == Hold());
    Require(Blocked(GuestSigusr));
}

static void TestHandlerOnBlockedSignalUnblocksAndReturnsHold() {
    Require(Blocked(GuestSigusr));
    Require(sigset_nid_postfix(GuestSigusr, Handler) == Hold());
    Require(!Blocked(GuestSigusr));
    Require(Disposition(GuestSigusr) == reinterpret_cast<std::uintptr_t>(&Handler));
}

static void TestIgnoreReturnsPreviousHandler() {
    Require(!Blocked(GuestSigusr));
    const GuestHandler previous = sigset_nid_postfix(GuestSigusr, reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(1)));
    Require(previous == Handler);
    Require(Disposition(GuestSigusr) == 1);
    GuestSigaction restore{};
    Require(sigaction_nid_postfix(GuestSigusr, &restore, nullptr) == 0);
}

static void TestRejectsInvalidSignals() {
    *__error_nid_postfix() = 0;
    Require(sigset_nid_postfix(0, Hold()) == Invalid());
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(sigset_nid_postfix(GuestSigkill, Handler) == Invalid());
    Require(*__error_nid_postfix() == GuestEinval);
}

static void TestSigpauseReturnsOnPendingSignal() {
    Require(!Blocked(GuestSigusr));
    Require(sigset_nid_postfix(GuestSigusr, Hold()) == nullptr);
    GuestSigaction action{};
    action.handler = reinterpret_cast<std::uintptr_t>(&Handler);
    Require(sigaction_nid_postfix(GuestSigusr, &action, nullptr) == 0);
    Require(Blocked(GuestSigusr));

    g_hits = 0;
    Require(raise_nid_postfix(GuestSigusr) == 0);
    Require(g_hits == 0);
    *__error_nid_postfix() = 0;
    Require(sigpause_nid_postfix(0) == -1);
    Require(*__error_nid_postfix() == GuestEintr);
    Require(g_hits == 1);
    Require(Blocked(GuestSigusr));

    Require(sigset_nid_postfix(GuestSigusr, Handler) == Hold());
    Require(!Blocked(GuestSigusr));
    GuestSigaction restore{};
    Require(sigaction_nid_postfix(GuestSigusr, &restore, nullptr) == 0);
}

int main() {
    TestHoldBlocksAndReturnsPrevious();
    TestHandlerOnBlockedSignalUnblocksAndReturnsHold();
    TestIgnoreReturnsPreviousHandler();
    TestRejectsInvalidSignals();
    TestSigpauseReturnsOnPendingSignal();
    return 0;
}
