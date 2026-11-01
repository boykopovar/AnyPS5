#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <thread>

static constexpr int SigKill = 9;
static constexpr int SigTerm = 15;
static constexpr int SigUsr1 = 30;
static constexpr int SigUsr2 = 31;
static constexpr int SaRestart = 0x2;
static constexpr int SaResethand = 0x4;
static constexpr int SaNodefer = 0x10;
static constexpr int SigBlock = 1;
static constexpr int SigUnblock = 2;
static constexpr int SigSetmask = 3;

static std::uint32_t Bit(int signal) { return 1u << (signal - 1); }

static void Require(bool value) { if (!value) std::abort(); }

static int calls = 0;
static int callsAfterNestedRaise = 0;
static std::uint32_t maskInHandler = 0;

static std::uint32_t CurrentMask() {
    GuestSignalSet set{};
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, &set) == 0);
    return set.bits[0];
}

static void APS5_VABI RaiseAgain(int signal) {
    if (++calls == 1) {
        maskInHandler = CurrentMask();
        Require(raise_nid_postfix(signal) == 0);
        callsAfterNestedRaise = calls;
    }
}

static void APS5_VABI Count(int) {
    ++calls;
}

static std::uint64_t handlerExit[12];

static void APS5_VABI LeaveWithLongjmp(int) {
    ++calls;
    longjmp_nid_postfix(handlerExit, 1);
}

static void APS5_VABI BlockFromOtherThread(int) {
    ++calls;
    std::thread([] {
        const GuestSignalSet set{{Bit(SigTerm), 0, 0, 0}};
        Require(sigprocmask_nid_postfix(SigBlock, &set, nullptr) == 0);
    }).join();
}

static void APS5_VABI SetMaskFromOtherThread(int) {
    ++calls;
    std::thread([] {
        const GuestSignalSet set{{Bit(SigTerm), 0, 0, 0}};
        Require(sigprocmask_nid_postfix(SigSetmask, &set, nullptr) == 0);
    }).join();
}

static void APS5_VABI SaveAndRestoreMask(int) {
    ++calls;
    GuestSignalSet old{};
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, &old) == 0);
    Require(sigprocmask_nid_postfix(SigSetmask, &old, nullptr) == 0);
}

static void APS5_VABI ReplaceAndRestoreMask(int) {
    ++calls;
    const GuestSignalSet none{};
    GuestSignalSet old{};
    Require(sigprocmask_nid_postfix(SigSetmask, &none, &old) == 0);
    Require(sigprocmask_nid_postfix(SigSetmask, &old, nullptr) == 0);
}

static GuestSignalSet setInHandler{};

static void APS5_VABI RecordMask(int) {
    ++calls;
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, &setInHandler) == 0);
}

static void APS5_VABI UnblockAndRaise(int signal) {
    if (++calls == 1) {
        const GuestSignalSet own{{Bit(signal), 0, 0, 0}};
        Require(sigprocmask_nid_postfix(SigUnblock, &own, nullptr) == 0);
        Require(raise_nid_postfix(signal) == 0);
        callsAfterNestedRaise = calls;
    }
}

static void Install(int signal, void (APS5_VABI* handler)(int), int flags, std::uint32_t mask) {
    GuestSigaction action{reinterpret_cast<std::uintptr_t>(handler), flags, {{mask, 0, 0, 0}}};
    Require(sigaction_nid_postfix(signal, &action, nullptr) == 0);
}

static void CheckDeferred(int signal) {
    calls = 0;
    callsAfterNestedRaise = 0;
    Install(signal, RaiseAgain, SaRestart, Bit(SigUsr1));
    Require(raise_nid_postfix(signal) == 0);
    Require(callsAfterNestedRaise == 1);
    Require(calls == 2);
    Require(maskInHandler == (Bit(signal) | Bit(SigUsr1)));
    Require(CurrentMask() == 0);
}

static void CheckNodefer() {
    calls = 0;
    callsAfterNestedRaise = 0;
    Install(SigUsr2, RaiseAgain, SaRestart | SaNodefer, 0);
    Require(raise_nid_postfix(SigUsr2) == 0);
    Require(callsAfterNestedRaise == 2);
    Require(maskInHandler == 0);
    Require(CurrentMask() == 0);
}

static void CheckResethand() {
    calls = 0;
    Install(SigUsr1, Count, SaResethand, 0);
    Require(raise_nid_postfix(SigUsr1) == 0);
    Require(calls == 1);
    GuestSigaction current{};
    Require(sigaction_nid_postfix(SigUsr1, nullptr, &current) == 0);
    Require(current.handler == 0);
    Require((current.flags & SaResethand) != 0);
}

static void CheckOtherThreadKept() {
    calls = 0;
    Install(SigUsr1, BlockFromOtherThread, SaRestart, 0);
    Require(raise_nid_postfix(SigUsr1) == 0);
    Require(calls == 1);
    Require(CurrentMask() == Bit(SigTerm));
    const GuestSignalSet set{{Bit(SigTerm), 0, 0, 0}};
    Require(sigprocmask_nid_postfix(SigUnblock, &set, nullptr) == 0);
    Require(CurrentMask() == 0);
}

static void CheckOtherThreadKeptSameSignal() {
    calls = 0;
    Install(SigUsr1, BlockFromOtherThread, SaRestart, Bit(SigTerm));
    Require(raise_nid_postfix(SigUsr1) == 0);
    Require(calls == 1);
    Require(CurrentMask() == Bit(SigTerm));
    const GuestSignalSet set{{Bit(SigTerm), 0, 0, 0}};
    Require(sigprocmask_nid_postfix(SigUnblock, &set, nullptr) == 0);
    Require(CurrentMask() == 0);
}

static void CheckOtherThreadSetMaskKept() {
    calls = 0;
    Install(SigUsr1, SetMaskFromOtherThread, SaRestart, 0);
    Require(raise_nid_postfix(SigUsr1) == 0);
    Require(calls == 1);
    Require(CurrentMask() == Bit(SigTerm));
    const GuestSignalSet none{};
    Require(sigprocmask_nid_postfix(SigSetmask, &none, nullptr) == 0);
    Require(CurrentMask() == 0);
}

static void CheckMaskRestoredInsideHandler(int signal, void (APS5_VABI* handler)(int)) {
    calls = 0;
    Install(signal, handler, SaRestart, 0);
    Require(raise_nid_postfix(signal) == 0);
    Require(calls == 1);
    Require(CurrentMask() == 0);
    Require(raise_nid_postfix(signal) == 0);
    Require(calls == 2);
    Require(CurrentMask() == 0);
}

static void CheckUnblockInsideHandler() {
    calls = 0;
    callsAfterNestedRaise = 0;
    Install(SigUsr2, UnblockAndRaise, SaRestart, 0);
    Require(raise_nid_postfix(SigUsr2) == 0);
    Require(callsAfterNestedRaise == 2);
    Require(CurrentMask() == 0);
}

static void CheckUpperMaskWords() {
    calls = 0;
    const GuestSigaction action{reinterpret_cast<std::uintptr_t>(RecordMask), SaRestart, {{Bit(SigKill), 1u << 7, 0, 1u << 31}}};
    Require(sigaction_nid_postfix(SigUsr1, &action, nullptr) == 0);
    Require(raise_nid_postfix(SigUsr1) == 0);
    Require(calls == 1);
    Require(setInHandler.bits[0] == Bit(SigUsr1) && setInHandler.bits[1] == 1u << 7 && setInHandler.bits[2] == 0 && setInHandler.bits[3] == 1u << 31);
    GuestSignalSet after{};
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, &after) == 0);
    Require(after.bits[0] == 0 && after.bits[1] == 0 && after.bits[2] == 0 && after.bits[3] == 0);
}

static void CheckLeftWithLongjmp() {
    calls = 0;
    Install(SigUsr1, LeaveWithLongjmp, SaRestart, Bit(SigUsr2));
    for (int round = 1; round <= 2; ++round) {
        if (setjmp_nid_postfix(handlerExit) == 0) {
            raise_nid_postfix(SigUsr1);
            std::abort();
        }
        Require(calls == round);
        Require(CurrentMask() == 0);
    }
}

int main() {
    CheckDeferred(SigUsr2);
    CheckDeferred(SigTerm);
    CheckNodefer();
    CheckResethand();
    CheckOtherThreadKept();
    CheckOtherThreadKeptSameSignal();
    CheckOtherThreadSetMaskKept();
    CheckMaskRestoredInsideHandler(SigUsr2, SaveAndRestoreMask);
    CheckMaskRestoredInsideHandler(SigTerm, SaveAndRestoreMask);
    CheckMaskRestoredInsideHandler(SigUsr2, ReplaceAndRestoreMask);
    CheckUnblockInsideHandler();
    CheckUpperMaskWords();
    CheckLeftWithLongjmp();
    return 0;
}
