#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <initializer_list>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <thread>
using Handler = void (APS5_VABI *)(int);
struct GuestSignalSet {
    std::uint32_t bits[4];
};
extern "C" {
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI raise_nid_postfix(int);
int APS5_VABI kill_nid_postfix(int, int);
int APS5_VABI getpid_nid_postfix();
int APS5_VABI pthread_kill_nid_postfix(void*, int);
void* APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCreate(void** thread, const void* attr, void* (APS5_VABI *entry)(void*), void* arg, const char* name);
int APS5_VABI scePthreadJoin(void* thread, void** retval);
int APS5_VABI sigaction_nid_postfix(int, const void*, void*);
int APS5_VABI sigprocmask_nid_postfix(int, const void*, void*);
int APS5_VABI pthread_sigmask_nid_postfix(int, const void*, void*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sigsuspend_nid_postfix(const GuestSignalSet*);
int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t);
unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int);
int APS5_VABI sigpending_nid_postfix(GuestSignalSet*);
}
struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};
static_assert(sizeof(GuestSigaction) == 32);
extern GuestSignalSet _sigintr_nid_postfix;
static bool Same(const GuestSigaction& left, const GuestSigaction& right) {
    return left.handler == right.handler && left.flags == right.flags && std::memcmp(&left.mask, &right.mask, sizeof(left.mask)) == 0;
}
volatile std::sig_atomic_t received = 0;
volatile std::uintptr_t handlerReturn = 0;
void APS5_VABI InfoCallback(int, void*, void*) {}
struct SuspendWait {
    const GuestSignalSet* mask = nullptr;
    int result = 0;
    int error = 0;
};
void* APS5_VABI SuspendEntry(void* arg) {
    auto& wait = *static_cast<SuspendWait*>(arg);
    wait.result = sigsuspend_nid_postfix(wait.mask);
    wait.error = *__error_nid_postfix();
    return nullptr;
}
struct SuspendSender {
    void* target = nullptr;
    int result = -1;
};
struct EndingThread {
    std::atomic<bool> release{false};
};
void* APS5_VABI EndingEntry(void* arg) {
    auto& state = *static_cast<EndingThread*>(arg);
    while (!state.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return nullptr;
}
void* APS5_VABI SuspendSend(void* arg) {
    auto& sender = *static_cast<SuspendSender*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sender.result = pthread_kill_nid_postfix(sender.target, 30);
    return nullptr;
}
void APS5_VABI Callback(int value) {
    received = value;
    handlerReturn = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
}
GuestSignalSet maskInHandler{};
void APS5_VABI MaskCallback(int value) {
    received = value;
    sigprocmask_nid_postfix(1, nullptr, &maskInHandler);
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
    received = 0;
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 15) == 0 && received == 15);
    received = 0;
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 0) == 0 && received == 0);
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 200) == 22);
    Require(pthread_kill_nid_postfix(scePthreadSelf(), -1) == 22);
    Require(pthread_kill_nid_postfix(nullptr, 15) == 3);
    Require(pthread_kill_nid_postfix(nullptr, 128) == 3);
    Require(pthread_kill_nid_postfix(nullptr, 129) == 22);
    Require(pthread_kill_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x10}), 15) == 3);
    Require(pthread_kill_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x10}), 0) == 3);
    {
        EndingThread ending;
        void* thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, EndingEntry, &ending, nullptr) == 0);
        Require(pthread_kill_nid_postfix(thread, 0) == 0);
        ending.release.store(true);
        int answer = 0;
        for (int attempt = 0; attempt < 2000 && (answer = pthread_kill_nid_postfix(thread, 0)) != 3; ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(answer == 3);
        Require(pthread_kill_nid_postfix(thread, 15) == 3);
        Require(scePthreadJoin(thread, nullptr) == 0);
        Require(pthread_kill_nid_postfix(thread, 0) == 3);
    }
    Require(received == 0);
    Require(signal_nid_postfix(15, ignore) != invalid);
    received = 0;
    Require(raise_nid_postfix(15) == 0 && received == 0);
    Require(signal_nid_postfix(15, nullptr) == ignore);
    Require(raise_nid_postfix(100) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    received = 0;
    Require(raise_nid_postfix(0) == 0 && received == 0 && *__error_nid_postfix() == 0);
    Require(raise_nid_postfix(129) == -1 && *__error_nid_postfix() == 22);
    Require(raise_nid_postfix(-1) == -1 && *__error_nid_postfix() == 22);
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
    const GuestSignalSet withKillAndStop{{(1u << 8) | (1u << 16) | 0x20, 0, 0, 0}};
    const GuestSignalSet noSignals{{0, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(1, &withKillAndStop, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 0x20);
    Require(sigprocmask_nid_postfix(3, &noSignals, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &withKillAndStop, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 0x20);
    Require(sigprocmask_nid_postfix(3, &noSignals, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(1, &withKillAndStop, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 0x20);
    Require(pthread_sigmask_nid_postfix(3, &noSignals, nullptr) == 0);

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
    for (const int flags : {0x80, 0x100, 0x8000, 0x40 | 0x80}) {
        const GuestSigaction undefinedFlags{callback, flags, {}};
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(15, &undefinedFlags, nullptr) == -1 && *__error_nid_postfix() == 22);
        Require(sigaction_nid_postfix(15, nullptr, &current) == 0 && current.handler == 0 && current.flags == 0x2);
    }
    const GuestSigaction ignoredWithFlags{1, 0x8000, {}};
    Require(sigaction_nid_postfix(15, &ignoredWithFlags, nullptr) == 0);
    Require(sigaction_nid_postfix(15, &defaults, nullptr) == 0);

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

    *__error_nid_postfix() = 0;
    bool nullRejected = false;
    try { sigsuspend_nid_postfix(nullptr); } catch (const std::invalid_argument&) { nullRejected = true; }
    Require(nullRejected);
    Require(signal_nid_postfix(30, Callback) != invalid);
    const GuestSignalSet user{{1u << 29, 0, 0, 0}};
    const GuestSignalSet empty{{0, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &user, &previous) == 0);
    received = 0;
    Require(raise_nid_postfix(30) == 0 && received == 0);
    *__error_nid_postfix() = 0;
    Require(sigsuspend_nid_postfix(&empty) == -1 && *__error_nid_postfix() == 4 && received == 30);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == user.bits[0]);
    Require(signal_nid_postfix(30, MaskCallback) != invalid);
    const GuestSignalSet suspendWithKillAndStop{{(1u << 8) | (1u << 16) | (1u << 28), 0, 0, 0}};
    received = 0;
    Require(raise_nid_postfix(30) == 0 && received == 0);
    Require(sigsuspend_nid_postfix(&suspendWithKillAndStop) == -1 && received == 30);
    Require(maskInHandler.bits[0] == (1u << 28));
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == user.bits[0]);
    Require(signal_nid_postfix(30, Callback) != invalid);
    received = 0;
    SuspendWait wait{&empty, 0, 0};
    void* suspender = nullptr;
    Require(scePthreadCreate(&suspender, nullptr, SuspendEntry, &wait, nullptr) == 0);
    SuspendSender sender{suspender, -1};
    void* senderThread = nullptr;
    Require(scePthreadCreate(&senderThread, nullptr, SuspendSend, &sender, nullptr) == 0);
    Require(scePthreadJoin(senderThread, nullptr) == 0 && sender.result == 0);
    Require(scePthreadJoin(suspender, nullptr) == 0);
    Require(wait.result == -1);
    Require(wait.error == 4);
    Require(received == 30);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == user.bits[0]);
    Require(sigprocmask_nid_postfix(2, &user, nullptr) == 0);
    const GuestSignalSet highBlock{{1u << 29, 1u << 7, 0, 0}};
    const GuestSignalSet suspendMask{{0, 0, 1u << 8, 0}};
    Require(sigprocmask_nid_postfix(3, &highBlock, nullptr) == 0);
    Require(signal_nid_postfix(73, Callback) != invalid);
    received = 0;
    SuspendWait highWait{&suspendMask, 0, 0};
    void* highSuspender = nullptr;
    Require(scePthreadCreate(&highSuspender, nullptr, SuspendEntry, &highWait, nullptr) == 0);
    SuspendSender highSender{highSuspender, -1};
    void* highSenderThread = nullptr;
    Require(scePthreadCreate(&highSenderThread, nullptr, SuspendSend, &highSender, nullptr) == 0);
    Require(scePthreadJoin(highSenderThread, nullptr) == 0 && highSender.result == 0);
    Require(scePthreadJoin(highSuspender, nullptr) == 0);
    Require(highWait.result == -1 && highWait.error == 4 && received == 30);
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && std::memcmp(&previous, &highBlock, sizeof(highBlock)) == 0);
    Require(sigprocmask_nid_postfix(3, &empty, nullptr) == 0);
    Require(signal_nid_postfix(73, nullptr) == Callback);
    signal_nid_postfix(30, nullptr);

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
    for (int unmapped : {1, 13, 30, 31, 32, 128}) {
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(unmapped, ignore) == nullptr);
        Require(sigaction_nid_postfix(unmapped, nullptr, &current) == 0 && current.handler == 1 && current.flags == 0x2);
        Require(signal_nid_postfix(unmapped, Callback) == ignore);
        Require(sigaction_nid_postfix(unmapped, nullptr, &current) == 0 && current.handler == callback && current.flags == 0x2);
        Require(signal_nid_postfix(unmapped, nullptr) == Callback);
    }
    for (int outOfRange : {0, 129, -1}) {
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(outOfRange, nullptr) == invalid && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(outOfRange, Callback) == invalid && *__error_nid_postfix() == 22);
    }
    for (int fixed : {9, 17}) {
        const GuestSigaction flagged{0, 0x1 | 0x4, {{0x5, 0x6, 0x7, 0x80000000u}}};
        Require(sigaction_nid_postfix(fixed, &flagged, nullptr) == 0);
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(fixed, Callback) == invalid && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(fixed, ignore) == invalid && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(signal_nid_postfix(fixed, invalid) == invalid && *__error_nid_postfix() == 22);
        Require(sigaction_nid_postfix(fixed, nullptr, &current) == 0 && current.handler == 0);
        *__error_nid_postfix() = 12345;
        Require(signal_nid_postfix(fixed, nullptr) == nullptr && *__error_nid_postfix() == 12345);
        Require(sigaction_nid_postfix(fixed, nullptr, &current) == 0 && current.handler == 0 && current.flags == 0x2 && current.mask.bits[0] == 0);
        Require(sigaction_nid_postfix(fixed, &defaults, nullptr) == 0);
    }
    {
        const GuestSigaction masked{callback, 0x1 | 0x4, {{0x5, 0x6, 0x7, 0x80000000u}}};
        Require(sigaction_nid_postfix(13, &masked, nullptr) == 0);
        *__error_nid_postfix() = 12345;
        Require(signal_nid_postfix(13, ignore) == Callback && *__error_nid_postfix() == 12345);
        Require(sigaction_nid_postfix(13, nullptr, &current) == 0);
        Require(current.handler == 1 && current.flags == 0x2 && current.mask.bits[0] == 0 && current.mask.bits[1] == 0 && current.mask.bits[2] == 0 && current.mask.bits[3] == 0);
        Require(sigaction_nid_postfix(13, &masked, nullptr) == 0);
        Require(signal_nid_postfix(13, Callback) == Callback);
        Require(sigaction_nid_postfix(13, nullptr, &current) == 0);
        Require(current.handler == callback && current.flags == 0x2 && current.mask.bits[3] == 0);
    }
    Require(signal_nid_postfix(13, invalid) == Callback);
    Require(sigaction_nid_postfix(13, nullptr, &current) == 0 && current.handler == static_cast<std::uintptr_t>(-1) && current.flags == 0x2);
    Require(signal_nid_postfix(13, nullptr) == invalid);
    Require(sigaction_nid_postfix(13, nullptr, &current) == 0 && current.handler == 0);
    *__error_nid_postfix() = 0;
    Require(sigpending_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
    *__error_nid_postfix() = 42;
    GuestSignalSet pendingSignals{{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
    Require(sigpending_nid_postfix(&pendingSignals) == 0);
    Require(pendingSignals.bits[0] == 0 && pendingSignals.bits[1] == 0 && pendingSignals.bits[2] == 0 && pendingSignals.bits[3] == 0);
    Require(*__error_nid_postfix() == 42);
    Require(signal_nid_postfix(15, nullptr) == Callback);
    Require(signal_nid_postfix(15, Callback) == nullptr);
    received = 0;
    Require(sigprocmask_nid_postfix(1, &termMask, nullptr) == 0);
    Require(raise_nid_postfix(15) == 0 && received == 0);
    pendingSignals = {{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
    Require(sigpending_nid_postfix(&pendingSignals) == 0);
    Require(pendingSignals.bits[0] == (1u << 14) && pendingSignals.bits[1] == 0 && pendingSignals.bits[2] == 0 && pendingSignals.bits[3] == 0);
    Require(sigprocmask_nid_postfix(2, &termMask, nullptr) == 0 && received == 15);
    Require(sigpending_nid_postfix(&pendingSignals) == 0);
    Require(pendingSignals.bits[0] == 0 && pendingSignals.bits[1] == 0 && pendingSignals.bits[2] == 0 && pendingSignals.bits[3] == 0);
    Require(signal_nid_postfix(15, nullptr) == Callback);

    const GuestSignalSet highSignals{{0, 1u << 7, 1u << 8, 1u << 31}};
    for (int guest : {40, 73, 128}) Require(signal_nid_postfix(guest, Callback) != invalid);
    received = 0;
    GuestSignalSet previousHighSignals{};
    Require(sigprocmask_nid_postfix(1, &highSignals, &previousHighSignals) == 0);
    Require(previousHighSignals.bits[0] == 0 && previousHighSignals.bits[1] == 0 &&
        previousHighSignals.bits[2] == 0 && previousHighSignals.bits[3] == 0);
    for (int guest : {40, 73, 128}) Require(raise_nid_postfix(guest) == 0 && received == 0);
    Require(sigpending_nid_postfix(&pendingSignals) == 0 &&
        std::memcmp(&pendingSignals, &highSignals, sizeof(highSignals)) == 0);
    Require(sigprocmask_nid_postfix(2, &highSignals, &previousHighSignals) == 0);
    Require(std::memcmp(&previousHighSignals, &highSignals, sizeof(highSignals)) == 0);
    Require(received == 128);
    Require(sigpending_nid_postfix(&pendingSignals) == 0 &&
        pendingSignals.bits[0] == 0 && pendingSignals.bits[1] == 0 &&
        pendingSignals.bits[2] == 0 && pendingSignals.bits[3] == 0);
    for (int guest : {40, 73, 128}) Require(signal_nid_postfix(guest, nullptr) == Callback);
    for (const int ignoredByDefault : {16, 19, 20, 23, 28, 29}) {
        *__error_nid_postfix() = 0;
        received = 0;
        Require(raise_nid_postfix(ignoredByDefault) == 0 && received == 0 && *__error_nid_postfix() == 0);
        Require(kill_nid_postfix(getpid_nid_postfix(), ignoredByDefault) == 0 && *__error_nid_postfix() == 0);
    }
    const GuestSignalSet ignoredByDefaultSet{{(1u << 15) | (1u << 19) | (1u << 22) | (1u << 27) | (1u << 28), 0, 0, 0}};
    GuestSignalSet pendingIgnored{};
    Require(sigprocmask_nid_postfix(3, &ignoredByDefaultSet, nullptr) == 0);
    for (const int ignoredByDefault : {16, 20, 23, 28, 29}) Require(raise_nid_postfix(ignoredByDefault) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    const GuestSignalSet noneBlocked{{0, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0);
    const GuestSigaction ignored{1, 0, {}};
    const GuestSigaction restored{0, 0, {}};
    const GuestSignalSet userSignal{{1u << 29, 0, 0, 0}};
    Require(sigaction_nid_postfix(30, &ignored, nullptr) == 0);
    *__error_nid_postfix() = 0;
    Require(raise_nid_postfix(30) == 0 && kill_nid_postfix(getpid_nid_postfix(), 30) == 0 && *__error_nid_postfix() == 0);
    Require(sigprocmask_nid_postfix(3, &userSignal, nullptr) == 0);
    Require(raise_nid_postfix(30) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0);
    Require(sigaction_nid_postfix(30, &restored, nullptr) == 0);

    const GuestSigaction caught{callback, 0, {}};
    received = 0;
    Require(sigaction_nid_postfix(30, &caught, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &userSignal, nullptr) == 0);
    Require(raise_nid_postfix(30) == 0 && received == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == userSignal.bits[0]);
    Require(sigaction_nid_postfix(30, &restored, nullptr) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == userSignal.bits[0]);
    Require(sigaction_nid_postfix(30, &ignored, nullptr) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    Require(sigaction_nid_postfix(30, &caught, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0 && received == 0);
    Require(sigprocmask_nid_postfix(3, &userSignal, nullptr) == 0);
    Require(raise_nid_postfix(30) == 0 && received == 0);
    Require(signal_nid_postfix(30, ignore) == Callback);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    Require(signal_nid_postfix(30, Callback) == ignore);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0 && received == 0);
    Require(signal_nid_postfix(30, nullptr) == Callback);

    const GuestSignalSet windowSignal{{1u << 27, 0, 0, 0}};
    Require(sigaction_nid_postfix(28, &caught, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &windowSignal, nullptr) == 0);
    Require(raise_nid_postfix(28) == 0 && received == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == windowSignal.bits[0]);
    Require(sigaction_nid_postfix(28, &restored, nullptr) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    Require(sigaction_nid_postfix(28, &caught, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0 && received == 0);
    Require(sigaction_nid_postfix(28, &restored, nullptr) == 0);

    const GuestSignalSet continueSignal{{1u << 18, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &continueSignal, nullptr) == 0);
    Require(raise_nid_postfix(19) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == continueSignal.bits[0]);
    Require(signal_nid_postfix(19, nullptr) == nullptr);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0 && received == 0);

    const GuestSignalSet highSignal{{0, 1u << 7, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &highSignal, nullptr) == 0);
    Require(raise_nid_postfix(40) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[1] == highSignal.bits[1]);
    Require(sigaction_nid_postfix(40, &ignored, nullptr) == 0);
    Require(sigpending_nid_postfix(&pendingIgnored) == 0 && pendingIgnored.bits[0] == 0 && pendingIgnored.bits[1] == 0);
    Require(sigaction_nid_postfix(40, &restored, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &noneBlocked, nullptr) == 0);
    Require(_sigintr_nid_postfix.bits[0] == 0 && _sigintr_nid_postfix.bits[1] == 0 && _sigintr_nid_postfix.bits[2] == 0 &&
        _sigintr_nid_postfix.bits[3] == 0);
    GuestSigaction interruptible{};
    Require(signal_nid_postfix(30, Callback) == nullptr);
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 && interruptible.flags == 0x2);
    _sigintr_nid_postfix.bits[0] = 1u << 29;
    Require(signal_nid_postfix(30, Callback) == Callback);
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 &&
        interruptible.handler == reinterpret_cast<std::uintptr_t>(Callback) && interruptible.flags == 0);
    signal_nid_postfix(31, Callback);
    Require(sigaction_nid_postfix(31, nullptr, &interruptible) == 0 && interruptible.flags == 0x2);
    _sigintr_nid_postfix.bits[3] = 1u << 31;
    Require(signal_nid_postfix(128, Callback) == nullptr);
    Require(sigaction_nid_postfix(128, nullptr, &interruptible) == 0 && interruptible.flags == 0);
    _sigintr_nid_postfix.bits[0] = 0;
    _sigintr_nid_postfix.bits[3] = 0;
    Require(signal_nid_postfix(30, Callback) == Callback);
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 && interruptible.flags == 0x2);
    Require(signal_nid_postfix(30, nullptr) == Callback && signal_nid_postfix(31, nullptr) == Callback &&
        signal_nid_postfix(128, nullptr) == Callback);
}
