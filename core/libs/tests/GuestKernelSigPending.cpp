#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr int ErrorFault = 14;
constexpr int SignalBlock = 1;
constexpr int SignalUnblock = 2;
constexpr int SignalSetmask = 3;
constexpr int SignalTerm = 15;
constexpr int SignalUser1 = 30;
constexpr int SignalUser2 = 31;

std::atomic<int> gLastReceivedSignal{0};
std::atomic<int> gSignalCount{0};

void APS5_VABI TestSignalHandler(int sig) {
    gLastReceivedSignal.store(sig);
    gSignalCount.fetch_add(1);
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestNullAndFaultRejection() {
    *__error_nid_postfix() = 0;
    Require(sigpending_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorFault);

    *__error_nid_postfix() = 0;
    Require(sigpending_nid_postfix(reinterpret_cast<GuestSignalSet*>(std::uintptr_t{0x10})) == -1);
    Require(*__error_nid_postfix() == ErrorFault);
}

void TestEmptySetWhenNoSignalsPending() {
    GuestSignalSet set{{0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd}};
    *__error_nid_postfix() = 42;
    Require(sigpending_nid_postfix(&set) == 0);
    Require(set.bits[0] == 0 && set.bits[1] == 0 && set.bits[2] == 0 && set.bits[3] == 0);
    Require(*__error_nid_postfix() == 42);

    Require(sigismember_nid_postfix(&set, SignalUser1) == 0);
    Require(sigismember_nid_postfix(&set, SignalUser2) == 0);
    Require(sigismember_nid_postfix(&set, SignalTerm) == 0);
}

void TestSingleBlockedSignalPending() {
    gLastReceivedSignal.store(0);
    gSignalCount.store(0);
    Require(signal_nid_postfix(SignalUser1, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));

    GuestSignalSet blockMask{{1u << (SignalUser1 - 1), 0, 0, 0}};
    GuestSignalSet previousMask{};
    Require(_sigprocmask_nid_postfix(SignalBlock, &blockMask, &previousMask) == 0);

    Require(raise_nid_postfix(SignalUser1) == 0);
    Require(gLastReceivedSignal.load() == 0);
    Require(gSignalCount.load() == 0);

    GuestSignalSet pending{{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(pending.bits[0] == (1u << (SignalUser1 - 1)));
    Require(pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 1);
    Require(sigismember_nid_postfix(&pending, SignalUser2) == 0);
    Require(sigismember_nid_postfix(&pending, SignalTerm) == 0);

    Require(_sigprocmask_nid_postfix(SignalUnblock, &blockMask, nullptr) == 0);
    Require(gLastReceivedSignal.load() == SignalUser1);
    Require(gSignalCount.load() == 1);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 0);

    signal_nid_postfix(SignalUser1, nullptr);
    _sigprocmask_nid_postfix(SignalSetmask, &previousMask, nullptr);
}

void TestMultipleBlockedSignalsPending() {
    gLastReceivedSignal.store(0);
    gSignalCount.store(0);
    Require(signal_nid_postfix(SignalUser1, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));
    Require(signal_nid_postfix(SignalUser2, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));

    GuestSignalSet blockMask{{(1u << (SignalUser1 - 1)) | (1u << (SignalUser2 - 1)), 0, 0, 0}};
    GuestSignalSet previousMask{};
    Require(_sigprocmask_nid_postfix(SignalBlock, &blockMask, &previousMask) == 0);

    Require(raise_nid_postfix(SignalUser1) == 0);
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require((pending.bits[0] & (1u << (SignalUser1 - 1))) != 0);
    Require((pending.bits[0] & (1u << (SignalUser2 - 1))) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 1);
    Require(sigismember_nid_postfix(&pending, SignalUser2) == 0);

    Require(raise_nid_postfix(SignalUser2) == 0);
    Require(sigpending_nid_postfix(&pending) == 0);
    Require((pending.bits[0] & (1u << (SignalUser1 - 1))) != 0);
    Require((pending.bits[0] & (1u << (SignalUser2 - 1))) != 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 1);
    Require(sigismember_nid_postfix(&pending, SignalUser2) == 1);

    GuestSignalSet unblockUser1{{1u << (SignalUser1 - 1), 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(SignalUnblock, &unblockUser1, nullptr) == 0);
    Require(gLastReceivedSignal.load() == SignalUser1);
    Require(gSignalCount.load() == 1);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require((pending.bits[0] & (1u << (SignalUser1 - 1))) == 0);
    Require((pending.bits[0] & (1u << (SignalUser2 - 1))) != 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser2) == 1);

    GuestSignalSet unblockUser2{{1u << (SignalUser2 - 1), 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(SignalUnblock, &unblockUser2, nullptr) == 0);
    Require(gLastReceivedSignal.load() == SignalUser2);
    Require(gSignalCount.load() == 2);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser2) == 0);

    signal_nid_postfix(SignalUser1, nullptr);
    signal_nid_postfix(SignalUser2, nullptr);
    _sigprocmask_nid_postfix(SignalSetmask, &previousMask, nullptr);
}

void TestIgnoredSignalDiscarded() {
    gLastReceivedSignal.store(0);
    gSignalCount.store(0);
    Require(signal_nid_postfix(SignalUser1, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));

    GuestSignalSet blockMask{{1u << (SignalUser1 - 1), 0, 0, 0}};
    GuestSignalSet previousMask{};
    Require(_sigprocmask_nid_postfix(SignalBlock, &blockMask, &previousMask) == 0);

    Require(raise_nid_postfix(SignalUser1) == 0);
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 1);

    GuestSigaction ignoredAction{};
    ignoredAction.handler = 1;
    Require(sigaction_nid_postfix(SignalUser1, &ignoredAction, nullptr) == 0);

    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);

    Require(_sigprocmask_nid_postfix(SignalUnblock, &blockMask, nullptr) == 0);
    Require(gLastReceivedSignal.load() == 0);
    Require(gSignalCount.load() == 0);

    GuestSigaction defaultAction{};
    defaultAction.handler = 0;
    Require(sigaction_nid_postfix(SignalUser1, &defaultAction, nullptr) == 0);
    _sigprocmask_nid_postfix(SignalSetmask, &previousMask, nullptr);
}

void TestThreadSignalMaskQueryIsolation() {
    gLastReceivedSignal.store(0);
    gSignalCount.store(0);
    Require(signal_nid_postfix(SignalUser1, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));

    GuestSignalSet blockMask{{1u << (SignalUser1 - 1), 0, 0, 0}};
    GuestSignalSet previousMask{};
    Require(_sigprocmask_nid_postfix(SignalBlock, &blockMask, &previousMask) == 0);

    Require(raise_nid_postfix(SignalUser1) == 0);
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 1);

    std::thread worker([&] {
        GuestSignalSet workerPending{{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
        Require(sigpending_nid_postfix(&workerPending) == 0);
        Require(sigismember_nid_postfix(&workerPending, SignalUser1) == 0);
        Require(workerPending.bits[0] == 0 && workerPending.bits[1] == 0 && workerPending.bits[2] == 0 && workerPending.bits[3] == 0);
    });
    worker.join();

    Require(_sigprocmask_nid_postfix(SignalUnblock, &blockMask, nullptr) == 0);
    Require(gLastReceivedSignal.load() == SignalUser1);

    signal_nid_postfix(SignalUser1, nullptr);
    _sigprocmask_nid_postfix(SignalSetmask, &previousMask, nullptr);
}

void TestUnblockedSignalsNeverPending() {
    gLastReceivedSignal.store(0);
    gSignalCount.store(0);
    Require(signal_nid_postfix(SignalUser1, TestSignalHandler) != reinterpret_cast<GuestHandler>(-1));

    GuestSignalSet emptyMask{};
    GuestSignalSet previousMask{};
    Require(_sigprocmask_nid_postfix(SignalSetmask, &emptyMask, &previousMask) == 0);

    Require(raise_nid_postfix(SignalUser1) == 0);
    Require(gLastReceivedSignal.load() == SignalUser1);
    Require(gSignalCount.load() == 1);

    GuestSignalSet pending{{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
    Require(sigpending_nid_postfix(&pending) == 0);
    Require(pending.bits[0] == 0 && pending.bits[1] == 0 && pending.bits[2] == 0 && pending.bits[3] == 0);
    Require(sigismember_nid_postfix(&pending, SignalUser1) == 0);

    signal_nid_postfix(SignalUser1, nullptr);
    _sigprocmask_nid_postfix(SignalSetmask, &previousMask, nullptr);
}

} // namespace

int main() {
    TestNullAndFaultRejection();
    TestEmptySetWhenNoSignalsPending();
    TestSingleBlockedSignalPending();
    TestMultipleBlockedSignalsPending();
    TestIgnoredSignalDiscarded();
    TestThreadSignalMaskQueryIsolation();
    TestUnblockedSignalsNeverPending();
    return 0;
}
