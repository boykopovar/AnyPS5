#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSigprocmaskHowParameterBounds() {
    GuestSignalSet initial{};
    Require(sigprocmask_nid_postfix(1, nullptr, &initial) == 0);

    GuestSignalSet set{};
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 10) == 0);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(0, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(4, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(-1, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(100, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(INT_MIN, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(INT_MAX, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(_sigprocmask_nid_postfix(0, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(_sigprocmask_nid_postfix(4, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(_sigprocmask_nid_postfix(-1, &set, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 77;
    Require(pthread_sigmask_nid_postfix(0, &set, nullptr) == 22);
    Require(*__error_nid_postfix() == 77);

    *__error_nid_postfix() = 77;
    Require(pthread_sigmask_nid_postfix(4, &set, nullptr) == 22);
    Require(*__error_nid_postfix() == 77);

    *__error_nid_postfix() = 77;
    Require(pthread_sigmask_nid_postfix(-1, &set, nullptr) == 22);
    Require(*__error_nid_postfix() == 77);

    *__error_nid_postfix() = 77;
    Require(pthread_sigmask_nid_postfix(100, &set, nullptr) == 22);
    Require(*__error_nid_postfix() == 77);

    GuestSignalSet queried{};
    Require(sigprocmask_nid_postfix(0, nullptr, &queried) == 0);
    Require(sigprocmask_nid_postfix(4, nullptr, &queried) == 0);
    Require(sigprocmask_nid_postfix(-1, nullptr, &queried) == 0);
    Require(pthread_sigmask_nid_postfix(0, nullptr, &queried) == 0);
    Require(pthread_sigmask_nid_postfix(99, nullptr, &queried) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(1, nullptr, nullptr) == 0);

    Require(sigprocmask_nid_postfix(3, &initial, nullptr) == 0);
}

void TestUnblockableSignalsBounds() {
    GuestSignalSet initial{};
    Require(sigprocmask_nid_postfix(1, nullptr, &initial) == 0);

    GuestSignalSet full{};
    Require(sigfillset_nid_postfix(&full) == 0);
    Require(sigprocmask_nid_postfix(3, &full, nullptr) == 0);

    GuestSignalSet current{};
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 9) == 0);
    Require(sigismember_nid_postfix(&current, 17) == 0);
    Require(sigismember_nid_postfix(&current, 1) == 1);
    Require(sigismember_nid_postfix(&current, 2) == 1);
    Require(sigismember_nid_postfix(&current, 10) == 1);
    Require(sigismember_nid_postfix(&current, 12) == 1);

    GuestSignalSet empty{};
    Require(sigemptyset_nid_postfix(&empty) == 0);
    Require(sigprocmask_nid_postfix(3, &empty, nullptr) == 0);

    GuestSignalSet unblockableOnly{};
    Require(sigemptyset_nid_postfix(&unblockableOnly) == 0);
    Require(sigaddset_nid_postfix(&unblockableOnly, 9) == 0);
    Require(sigaddset_nid_postfix(&unblockableOnly, 17) == 0);
    Require(sigprocmask_nid_postfix(1, &unblockableOnly, nullptr) == 0);

    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 9) == 0);
    Require(sigismember_nid_postfix(&current, 17) == 0);

    Require(sigprocmask_nid_postfix(3, &initial, nullptr) == 0);
}

void TestSigprocmaskOperationsAndTransitions() {
    GuestSignalSet initial{};
    Require(sigprocmask_nid_postfix(1, nullptr, &initial) == 0);

    GuestSignalSet empty{};
    Require(sigemptyset_nid_postfix(&empty) == 0);
    Require(sigprocmask_nid_postfix(3, &empty, nullptr) == 0);

    GuestSignalSet block10{};
    Require(sigemptyset_nid_postfix(&block10) == 0);
    Require(sigaddset_nid_postfix(&block10, 10) == 0);

    GuestSignalSet previous{};
    Require(sigprocmask_nid_postfix(1, &block10, &previous) == 0);
    Require(sigismember_nid_postfix(&previous, 10) == 0);

    GuestSignalSet current{};
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 10) == 1);

    GuestSignalSet block12{};
    Require(sigemptyset_nid_postfix(&block12) == 0);
    Require(sigaddset_nid_postfix(&block12, 12) == 0);
    Require(sigprocmask_nid_postfix(1, &block12, &previous) == 0);
    Require(sigismember_nid_postfix(&previous, 10) == 1);
    Require(sigismember_nid_postfix(&previous, 12) == 0);

    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 10) == 1);
    Require(sigismember_nid_postfix(&current, 12) == 1);

    Require(sigprocmask_nid_postfix(2, &block10, &previous) == 0);
    Require(sigismember_nid_postfix(&previous, 10) == 1);
    Require(sigismember_nid_postfix(&previous, 12) == 1);

    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 10) == 0);
    Require(sigismember_nid_postfix(&current, 12) == 1);

    Require(sigprocmask_nid_postfix(3, &empty, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    Require(sigismember_nid_postfix(&current, 10) == 0);
    Require(sigismember_nid_postfix(&current, 12) == 0);

    Require(sigprocmask_nid_postfix(3, &initial, nullptr) == 0);
}

void TestSignalSetHelperBounds() {
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(nullptr, 1) == -1);
    Require(*__error_nid_postfix() == 14);

    GuestSignalSet set{};
    Require(sigemptyset_nid_postfix(&set) == 0);

    const int invalidSignals[] = {0, -1, -50, 129, 130, 256, 1000, INT_MIN, INT_MAX};
    for (int sig : invalidSignals) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, sig) == -1);
        Require(*__error_nid_postfix() == 22);
    }

    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigaddset_nid_postfix(&set, 128) == 0);
    Require(sigismember_nid_postfix(&set, 128) == 1);

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(sigismember_nid_postfix(&set, 1) == 0);
    Require(sigdelset_nid_postfix(&set, 128) == 0);
    Require(sigismember_nid_postfix(&set, 128) == 0);
}

void TestSignalDispositionBounds() {
    const auto invalidHandler = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));

    *__error_nid_postfix() = 0;
    Require(signal_nid_postfix(0, nullptr) == invalidHandler);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(signal_nid_postfix(-1, nullptr) == invalidHandler);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(signal_nid_postfix(129, nullptr) == invalidHandler);
    Require(*__error_nid_postfix() == 22);

    auto dummy = [](int) APS5_VABI {};

    *__error_nid_postfix() = 0;
    Require(signal_nid_postfix(9, dummy) == invalidHandler);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(signal_nid_postfix(17, dummy) == invalidHandler);
    Require(*__error_nid_postfix() == 22);

    GuestSigaction act{};
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(0, &act, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(-1, &act, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(129, &act, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    act.handler = reinterpret_cast<std::uintptr_t>(dummy);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(9, &act, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(17, &act, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    GuestSigaction badFlags{};
    badFlags.handler = 42;
    badFlags.flags = 0x80;
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(10, &badFlags, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    badFlags.flags = 0x100;
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(10, &badFlags, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
}

void TestSignalSendingAndPendingBounds() {
    *__error_nid_postfix() = 0;
    Require(kill_nid_postfix(getpid_nid_postfix(), -1) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(kill_nid_postfix(getpid_nid_postfix(), 129) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(kill_nid_postfix(999999, 0) == -1);
    Require(*__error_nid_postfix() == 3);

    Require(kill_nid_postfix(getpid_nid_postfix(), 0) == 0);
    Require(kill_nid_postfix(0, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(killpg_nid_postfix(1, 0) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(killpg_nid_postfix(0, -1) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(killpg_nid_postfix(0, 129) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(pthread_kill_nid_postfix(nullptr, 0) == 22);
    Require(pthread_kill_nid_postfix(scePthreadSelf(), -1) == 22);
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 129) == 22);
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 0) == 0);

    *__error_nid_postfix() = 0;
    Require(sigpending_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);

    GuestStack badStack{};
    badStack.flags = 1;
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&badStack, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    badStack.flags = 2;
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&badStack, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    char buf[1024];
    badStack.sp = buf;
    badStack.size = sizeof(buf);
    badStack.flags = 0;
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&badStack, nullptr) == -1);
    Require(*__error_nid_postfix() == 12);

    GuestStack oldStack{};
    Require(sigaltstack_nid_postfix(nullptr, &oldStack) == 0);
}

} // namespace

int main() {
    TestSigprocmaskHowParameterBounds();
    TestUnblockableSignalsBounds();
    TestSigprocmaskOperationsAndTransitions();
    TestSignalSetHelperBounds();
    TestSignalDispositionBounds();
    TestSignalSendingAndPendingBounds();
    return 0;
}
