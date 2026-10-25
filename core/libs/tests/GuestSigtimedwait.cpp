#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

struct GuestSiginfo {
    std::int32_t signo;
    std::int32_t error;
    std::int32_t code;
    std::int32_t pid;
    std::uint32_t uid;
    std::int32_t status;
    std::uintptr_t address;
    std::int32_t value;
    std::int32_t valueTail;
    std::int64_t band;
    std::int32_t spare[7];
    std::int32_t tail;
};
using Handler = void (APS5_VABI *)(int);

volatile int received = 0;
void APS5_VABI Callback(int value) { received = value; }
static void Require(bool value) { if (!value) std::abort(); }
static void RequireUntouched(const GuestSiginfo& info) {
    const GuestSiginfo empty{};
    Require(std::memcmp(&info, &empty, sizeof(info)) == 0);
}
struct Sender {
    Pthread target = nullptr;
    int guest = 0;
    int result = -1;
};
static void* APS5_VABI Send(void* arg) {
    auto& sender = *static_cast<Sender*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sender.result = pthread_kill_nid_postfix(sender.target, sender.guest);
    return nullptr;
}
static void* APS5_VABI SendWithoutDelay(void* arg) {
    auto& sender = *static_cast<Sender*>(arg);
    sender.result = pthread_kill_nid_postfix(sender.target, sender.guest);
    return nullptr;
}
static Pthread StartSender(Sender& sender) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Send, &sender, nullptr) == 0);
    return thread;
}
static Pthread StartSenderWithoutDelay(Sender& sender) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SendWithoutDelay, &sender, nullptr) == 0);
    return thread;
}
static void JoinSender(Pthread thread, const Sender& sender) {
    Require(scePthreadJoin(thread, nullptr) == 0 && sender.result == 0);
}
struct TimeoutSender {
    Pthread target;
    std::atomic<bool> finished{false};
};
static void* APS5_VABI SendUnrelatedUntilTimeout(void* arg) {
    auto& sender = *static_cast<TimeoutSender*>(arg);
    for (int round = 0; round < 80 && !sender.finished.load(); ++round) {
        Require(pthread_kill_nid_postfix(sender.target, 2) == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!sender.finished.load()) Require(pthread_kill_nid_postfix(sender.target, 13) == 0);
    return nullptr;
}
static void CheckTimeoutWithUnrelatedSignals() {
    const GuestSignalSet blocked{{(1u << 1) | (1u << 12), 0, 0, 0}};
    const GuestSignalSet pipe{{1u << 12, 0, 0, 0}};
    const GuestSignalSet interrupt{{1u << 1, 0, 0, 0}};
    const KernelTimespec timeout{0, 200000000};
    const KernelTimespec poll{0, 0};
    GuestSignalSet previous{};
    Require(_sigprocmask_nid_postfix(1, &blocked, &previous) == 0);
    TimeoutSender sender{scePthreadSelf()};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SendUnrelatedUntilTimeout, &sender, nullptr) == 0);
    GuestSiginfo info{};
    const auto begin = std::chrono::steady_clock::now();
    const int result = sigtimedwait_nid_postfix(&pipe, &info, &timeout);
    const int error = *__error_nid_postfix();
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    sender.finished.store(true);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(result == -1 && error == 35);
    Require(elapsed >= std::chrono::milliseconds(200));
    RequireUntouched(info);
    Require(sigtimedwait_nid_postfix(&interrupt, nullptr, &poll) == 2);
    Require(sigtimedwait_nid_postfix(&pipe, nullptr, &poll) == -1 && *__error_nid_postfix() == 35);
    Require(_sigprocmask_nid_postfix(3, &previous, nullptr) == 0);
}
int main() {
    const GuestSignalSet empty{{0, 0, 0, 0}};
    const GuestSignalSet pipe{{1u << 12, 0, 0, 0}};
    const GuestSignalSet interrupt{{1u << 1, 0, 0, 0}};
    const KernelTimespec poll{0, 0};
    const KernelTimespec expired{0, 1000000000};
    const KernelTimespec negativeSeconds{-1, 0};
    const KernelTimespec negativeNanoseconds{0, -1};
    GuestSiginfo info{};
    int taken = -1;
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(nullptr, &info, &poll) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&empty, &info, &poll) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigwaitinfo_nid_postfix(nullptr, &info) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigwaitinfo_nid_postfix(&empty, &info) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 71;
    Require(sigwait_nid_postfix(nullptr, &taken) == 22 && *__error_nid_postfix() == 71 && taken == -1);
    *__error_nid_postfix() = 72;
    Require(sigwait_nid_postfix(&empty, &taken) == 22 && *__error_nid_postfix() == 72 && taken == -1);
    *__error_nid_postfix() = 73;
    Require(sigwait_nid_postfix(&interrupt, nullptr) == 22 && *__error_nid_postfix() == 73);
    RequireUntouched(info);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&interrupt, &info, &poll) == -1 && *__error_nid_postfix() == 35);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&interrupt, &info, &expired) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&interrupt, &info, &negativeSeconds) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&interrupt, &info, &negativeNanoseconds) == -1 && *__error_nid_postfix() == 22);
    RequireUntouched(info);

    GuestSignalSet previous{};
    Require(signal_nid_postfix(2, Callback) != reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1)));
    Require(_sigprocmask_nid_postfix(1, &interrupt, &previous) == 0);
    received = 0;
    Require(raise_nid_postfix(2) == 0 && received == 0);
    Require(sigwait_nid_postfix(&interrupt, &taken) == 0 && taken == 2 && received == 0);
    Require(raise_nid_postfix(2) == 0 && received == 0);
    Require(sigtimedwait_nid_postfix(&interrupt, &info, &poll) == 2 && received == 0);
    Require(info.signo == 2 && info.error == 0 && info.code == 0 && info.pid == getpid_nid_postfix() && info.uid == 0);
    Require(info.status == 0 && info.address == 0 && info.value == 0 && info.band == 0);
    Require(raise_nid_postfix(2) == 0 && received == 0);
    Require(sigwaitinfo_nid_postfix(&interrupt, &info) == 2 && received == 0);
    Require(info.signo == 2 && info.code == 0 && info.pid == getpid_nid_postfix());
    Require(raise_nid_postfix(2) == 0 && received == 0);
    Require(sigwaitinfo_nid_postfix(&interrupt, nullptr) == 2 && received == 0);
    Require(raise_nid_postfix(2) == 0 && received == 0);
    Require(sigtimedwait_nid_postfix(&interrupt, nullptr, nullptr) == 2 && received == 0);
    GuestSignalSet current{};
    Require(_sigprocmask_nid_postfix(1, nullptr, &current) == 0 && current.bits[0] == (previous.bits[0] | interrupt.bits[0]));
    Require(_sigprocmask_nid_postfix(3, &previous, nullptr) == 0);
    received = 0;
    Require(raise_nid_postfix(2) == 0 && received == 2);

    Require(_sigprocmask_nid_postfix(1, &pipe, &previous) == 0);
    received = 0;
    Require(raise_nid_postfix(13) == 0 && received == 0);
    Require(sigwaitinfo_nid_postfix(&pipe, &info) == 13 && info.signo == 13 && info.pid == getpid_nid_postfix());
    Require(_sigprocmask_nid_postfix(3, &previous, nullptr) == 0);

    const Pthread self = scePthreadSelf();
    *__error_nid_postfix() = 0;
    Require(pthread_kill_nid_postfix(nullptr, 13) == 3 && *__error_nid_postfix() == 0);
    Require(pthread_kill_nid_postfix(self, 129) == 22);
    Sender sender{self, 13, -1};
    Pthread senderThread = StartSender(sender);
    Require(sigwaitinfo_nid_postfix(&pipe, &info) == 13);
    JoinSender(senderThread, sender);
    Require(info.signo == 13 && info.code == 0 && info.pid == getpid_nid_postfix());
    senderThread = StartSender(sender);
    Require(sigtimedwait_nid_postfix(&pipe, nullptr, nullptr) == 13);
    JoinSender(senderThread, sender);
    senderThread = StartSender(sender);
    const KernelTimespec patience{1, 0};
    Require(sigtimedwait_nid_postfix(&pipe, &info, &patience) == 13 && info.signo == 13);
    JoinSender(senderThread, sender);
    Require(signal_nid_postfix(15, Callback) != reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1)));
    senderThread = StartSender(sender = Sender{self, 15, -1});
    received = 0;
    *__error_nid_postfix() = 0;
    Require(pause_nid_postfix() == -1 && *__error_nid_postfix() == 4 && received == 15);
    JoinSender(senderThread, sender);
    Require(signal_nid_postfix(15, nullptr) == Callback);

    // The senders above sleep 50 ms first, so the signal always arrives while the wait is
    // already blocked. A raise landing between the check of the pending set and the wait
    // used to be lost, and the wait then never returned, so raise with no delay instead,
    // many times.
    Require(_sigprocmask_nid_postfix(1, &interrupt, &previous) == 0);
    for (int round = 0; round < 500; ++round) {
        Sender tight{self, 2, -1};
        const Pthread senderThread = StartSenderWithoutDelay(tight);
        Require(sigwait_nid_postfix(&interrupt, &taken) == 0 && taken == 2);
        JoinSender(senderThread, tight);
    }
    Require(_sigprocmask_nid_postfix(3, &previous, nullptr) == 0);
    CheckTimeoutWithUnrelatedSignals();
}
