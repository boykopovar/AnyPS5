#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
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

extern "C" {

unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int);

}

static_assert(sizeof(GuestSigaction) == 32);
extern GuestSignalSet _sigintr_nid_postfix;
static bool Same(const GuestSigaction& left, const GuestSigaction& right) {
    return left.handler == right.handler && left.flags == right.flags && std::memcmp(&left.mask, &right.mask, sizeof(left.mask)) == 0;
}
volatile std::sig_atomic_t received = 0;
volatile std::uintptr_t handlerReturn = 0;
struct InfoRecord {
    int runs = 0;
    int signal = 0;
    std::int32_t info[20]{};
    UserUcontext context{};
    GuestSignalSet mask{};
};
InfoRecord infoSeen{};
GuestSignalSet contextMaskAddition{};
UserUcontext escape{};
volatile int escapes = 0;
void APS5_VABI InfoCallback(int value, void* info, UserUcontext* context) {
    ++infoSeen.runs;
    infoSeen.signal = value;
    std::memcpy(infoSeen.info, info, sizeof(infoSeen.info));
    std::memcpy(&infoSeen.context, context, sizeof(infoSeen.context));
    sigprocmask_nid_postfix(1, nullptr, &infoSeen.mask);
    for (int word = 0; word < 4; ++word) context->sigmask[word] |= contextMaskAddition.bits[word];
}
void APS5_VABI EscapeCallback(int value, void* info, UserUcontext* context) {
    InfoCallback(value, info, context);
    context->mcontext = escape.mcontext;
}
void APS5_VABI RejectedContextCallback(int value, void* info, UserUcontext* context) {
    EscapeCallback(value, info, context);
    context->mcontext.flags = 0x8;
}
int frameChange = 0;
void APS5_VABI DeliveryFrameCallback(int value, void* info, UserUcontext* context) {
    InfoCallback(value, info, context);
    auto& machine = context->mcontext;
    if (frameChange == 0) machine.rax ^= 1;
    else if (frameChange == 1) machine.rbx ^= 1;
    else if (frameChange == 2) machine.rip = escape.mcontext.rip;
    else machine.rsp = escape.mcontext.rsp;
}
struct NestedSuspend {
    int runs = 0;
    int runsAfterRaise = 0;
    int result = 0;
    int error = 0;
    GuestSignalSet mask{};
};
NestedSuspend nested{};
void APS5_VABI NestedSuspendCallback(int) {
    if (++nested.runs != 1) return;
    const GuestSignalSet none{{0, 0, 0, 0}};
    *__error_nid_postfix() = 0;
    nested.result = sigsuspend_nid_postfix(&none);
    nested.error = *__error_nid_postfix();
    sigprocmask_nid_postfix(1, nullptr, &nested.mask);
    raise_nid_postfix(30);
    nested.runsAfterRaise = nested.runs;
}
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
    Pthread target = nullptr;
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
struct MaskProbe {
    GuestSignalSet inherited{};
    GuestSignalSet changed{};
    GuestSignalSet afterUnblock{};
    int waited = 0;
};
void* APS5_VABI ChildChangesMask(void* arg) {
    auto& probe = *static_cast<MaskProbe*>(arg);
    const GuestSignalSet hangup{{0x2, 0, 0, 0}};
    sigprocmask_nid_postfix(1, nullptr, &probe.inherited);
    sigprocmask_nid_postfix(3, &hangup, nullptr);
    sigprocmask_nid_postfix(1, nullptr, &probe.changed);
    return nullptr;
}
void* APS5_VABI ChildTakesPending(void* arg) {
    auto& probe = *static_cast<MaskProbe*>(arg);
    const GuestSignalSet user2{{1u << 30, 0, 0, 0}};
    const KernelTimespec none{0, 0};
    probe.waited = sigtimedwait_nid_postfix(&user2, nullptr, &none);
    return nullptr;
}
void* APS5_VABI ChildUnblocks(void* arg) {
    auto& probe = *static_cast<MaskProbe*>(arg);
    const GuestSignalSet user2{{1u << 30, 0, 0, 0}};
    sigprocmask_nid_postfix(2, &user2, nullptr);
    sigprocmask_nid_postfix(1, nullptr, &probe.afterUnblock);
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
static bool SameSet(const GuestSignalSet& left, const GuestSignalSet& right) {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}
static bool ContextHasMask(const UserUcontext& context, const GuestSignalSet& set) {
    return std::memcmp(context.sigmask, set.bits, sizeof(set.bits)) == 0;
}
static void CheckInfo(int signal, std::int32_t code) {
    Require(infoSeen.runs == 1 && infoSeen.signal == signal);
    Require(infoSeen.info[0] == signal && infoSeen.info[1] == 0 && infoSeen.info[2] == code);
    Require(infoSeen.info[3] == getpid_nid_postfix() && infoSeen.info[4] == 0);
    for (int index = 5; index < 20; ++index) Require(infoSeen.info[index] == 0);
    Require(infoSeen.context.mcontext.len == 0x480 && infoSeen.context.mcontext.fpformat == 0x10002);
    Require(infoSeen.context.stackPointer == nullptr && infoSeen.context.stackSize == 0 && infoSeen.context.stackFlags == 4);
    infoSeen = {};
}
static void CheckSiginfoDelivery() {
    const auto infoCallback = reinterpret_cast<std::uintptr_t>(InfoCallback);
    const GuestSignalSet none{{0, 0, 0, 0}};
    const GuestSignalSet before{{1u << 27, 0, 1u << 5, 0}};
    GuestSignalSet current{};
    GuestSigaction action{};
    Require(sigprocmask_nid_postfix(3, &before, nullptr) == 0);

    const GuestSigaction abortInfo{infoCallback, 0x40, {{0, 1u << 3, 0, 0}}};
    Require(sigaction_nid_postfix(6, &abortInfo, nullptr) == 0);
    Require(raise_nid_postfix(6) == 0);
    Require(ContextHasMask(infoSeen.context, before));
    Require(SameSet(infoSeen.mask, GuestSignalSet{{(1u << 27) | (1u << 5), 1u << 3, 1u << 5, 0}}));
    CheckInfo(6, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, before));
    Require(pthread_kill_nid_postfix(scePthreadSelf(), 6) == 0);
    CheckInfo(6, 0x10007);
    Require(kill_nid_postfix(getpid_nid_postfix(), 6) == 0);
    CheckInfo(6, 0x10001);
    const GuestSigaction abortNodefer{infoCallback, 0x40 | 0x10, {}};
    Require(sigaction_nid_postfix(6, &abortNodefer, nullptr) == 0);
    Require(raise_nid_postfix(6) == 0 && SameSet(infoSeen.mask, before));
    CheckInfo(6, 0x10007);
    Require(sigaction_nid_postfix(6, nullptr, &action) == 0 && Same(action, abortNodefer));

    const GuestSigaction userReset{infoCallback, 0x40 | 0x4, {}};
    Require(sigaction_nid_postfix(30, &userReset, nullptr) == 0);
    Require(kill_nid_postfix(getpid_nid_postfix(), 30) == 0);
    Require(ContextHasMask(infoSeen.context, before));
    Require(SameSet(infoSeen.mask, GuestSignalSet{{(1u << 27) | (1u << 29), 0, 1u << 5, 0}}));
    CheckInfo(30, 0x10001);
    Require(sigaction_nid_postfix(30, nullptr, &action) == 0 && action.handler == 0 && (action.flags & 0x40) == 0);

    const GuestSignalSet user2{{1u << 30, 0, 0, 0}};
    const GuestSigaction userInfo{infoCallback, 0x40, {}};
    Require(sigaction_nid_postfix(31, &userInfo, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, &user2, nullptr) == 0);
    Require(kill_nid_postfix(getpid_nid_postfix(), 31) == 0 && infoSeen.runs == 0);
    Require(sigprocmask_nid_postfix(2, &user2, nullptr) == 0);
    Require(ContextHasMask(infoSeen.context, before));
    Require(SameSet(infoSeen.mask, GuestSignalSet{{(1u << 27) | (1u << 30), 0, 1u << 5, 0}}));
    CheckInfo(31, 0x10001);

    Require(sigprocmask_nid_postfix(1, &user2, nullptr) == 0);
    Require(raise_nid_postfix(31) == 0 && infoSeen.runs == 0);
    *__error_nid_postfix() = 0;
    Require(sigsuspend_nid_postfix(&none) == -1 && *__error_nid_postfix() == 4);
    Require(ContextHasMask(infoSeen.context, GuestSignalSet{{(1u << 27) | (1u << 30), 0, 1u << 5, 0}}));
    Require(SameSet(infoSeen.mask, user2));
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, GuestSignalSet{{(1u << 27) | (1u << 30), 0, 1u << 5, 0}}));
    Require(sigprocmask_nid_postfix(3, &before, nullptr) == 0);

    contextMaskAddition = GuestSignalSet{{1u << 15, 0, 0, 1u << 31}};
    Require(raise_nid_postfix(31) == 0);
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, GuestSignalSet{{(1u << 27) | (1u << 15), 0, 1u << 5, 1u << 31}}));
    Require(sigprocmask_nid_postfix(3, &before, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, &user2, nullptr) == 0);
    Require(raise_nid_postfix(31) == 0 && infoSeen.runs == 0);
    Require(sigsuspend_nid_postfix(&none) == -1 && *__error_nid_postfix() == 4);
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, GuestSignalSet{{(1u << 27) | (1u << 30) | (1u << 15), 0, 1u << 5, 1u << 31}}));
    contextMaskAddition = {};
    Require(sigprocmask_nid_postfix(3, &before, nullptr) == 0);

    const GuestSigaction escaping{reinterpret_cast<std::uintptr_t>(EscapeCallback), 0x40, {}};
    Require(sigaction_nid_postfix(31, &escaping, nullptr) == 0);
    escapes = 0;
    Require(getcontext_nid_postfix(&escape) == 0);
    const int escaped = escapes;
    escapes = escaped + 1;
    if (escaped == 0) {
        raise_nid_postfix(31);
        Require(false);
    }
    Require(escapes == 2);
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, before));

    const GuestSigaction rejecting{reinterpret_cast<std::uintptr_t>(RejectedContextCallback), 0x40, {}};
    Require(sigaction_nid_postfix(31, &rejecting, nullptr) == 0);
    bool rejected = false;
    try {
        raise_nid_postfix(31);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, before));

    const GuestSigaction deliveryFrame{reinterpret_cast<std::uintptr_t>(DeliveryFrameCallback), 0x40, {}};
    Require(sigaction_nid_postfix(31, &deliveryFrame, nullptr) == 0);
    for (frameChange = 0; frameChange < 4; ++frameChange) {
        rejected = false;
        try {
            raise_nid_postfix(31);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected);
        CheckInfo(31, 0x10007);
        Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, before));
    }

    const GuestSigaction user1Info{infoCallback, 0x40, {}};
    Require(sigaction_nid_postfix(30, &user1Info, nullptr) == 0);
    SuspendSender sender{scePthreadSelf(), -1};
    Pthread senderThread = nullptr;
    Require(scePthreadCreate(&senderThread, nullptr, SuspendSend, &sender, nullptr) == 0);
    *__error_nid_postfix() = 0;
    Require(pause_nid_postfix() == -1 && *__error_nid_postfix() == 4);
    Require(scePthreadJoin(senderThread, nullptr) == 0 && sender.result == 0);
    Require(ContextHasMask(infoSeen.context, before));
    Require(SameSet(infoSeen.mask, GuestSignalSet{{(1u << 27) | (1u << 29), 0, 1u << 5, 0}}));
    CheckInfo(30, 0x10007);

    constexpr std::int64_t timerValue = 0x123456789abcdefLL;
    const GuestSigaction timerInfo{infoCallback, 0x40, {}};
    Require(sigaction_nid_postfix(31, &timerInfo, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, &user2, nullptr) == 0);
    const GuestSigevent event{1, 31, timerValue, {}};
    int timer = -1;
    Require(ktimer_create_nid_postfix(4, &event, &timer) == 0);
    const GuestItimerspec once{{0, 0}, {0, 10000000}};
    Require(ktimer_settime_nid_postfix(timer, 0, &once, nullptr) == 0);
    GuestSignalSet pending{};
    for (int attempt = 0; attempt < 2000 && sigpending_nid_postfix(&pending) == 0 && (pending.bits[0] & user2.bits[0]) == 0; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require((pending.bits[0] & user2.bits[0]) != 0 && infoSeen.runs == 0);
    Require(sigprocmask_nid_postfix(2, &user2, nullptr) == 0);
    Require(infoSeen.runs == 1 && infoSeen.signal == 31 && ContextHasMask(infoSeen.context, before));
    Require(infoSeen.info[0] == 31 && infoSeen.info[1] == 0 && infoSeen.info[2] == 0x10003 && infoSeen.info[3] == 0 && infoSeen.info[4] == 0);
    std::int64_t value = 0;
    std::memcpy(&value, &infoSeen.info[8], sizeof(value));
    Require(value == timerValue && infoSeen.info[10] == timer && infoSeen.info[11] == 0);
    infoSeen = {};
    Require(ktimer_delete_nid_postfix(timer) == 0);

    const GuestSigaction outer{reinterpret_cast<std::uintptr_t>(NestedSuspendCallback), 0, {{0, 1u << 7, 0, 0}}};
    Require(sigaction_nid_postfix(30, &outer, nullptr) == 0);
    Require(sigaction_nid_postfix(31, &userInfo, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &user2, nullptr) == 0);
    contextMaskAddition = GuestSignalSet{{1u << 23, 0, 0, 0}};
    Require(raise_nid_postfix(31) == 0 && infoSeen.runs == 0);
    Require(raise_nid_postfix(30) == 0);
    Require(nested.runs == 2 && nested.runsAfterRaise == 1 && nested.result == -1 && nested.error == 4);
    Require(ContextHasMask(infoSeen.context, GuestSignalSet{{(1u << 29) | (1u << 30), 1u << 7, 0, 0}}));
    Require(SameSet(infoSeen.mask, GuestSignalSet{{(1u << 29) | (1u << 30), 1u << 7, 0, 0}}));
    CheckInfo(31, 0x10007);
    Require(SameSet(nested.mask, GuestSignalSet{{(1u << 29) | (1u << 30) | (1u << 23), 1u << 7, 0, 0}}));
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, GuestSignalSet{{(1u << 30) | (1u << 23), 0, 0, 0}}));
    contextMaskAddition = {};

    frameChange = 0;
    Require(sigaction_nid_postfix(31, &deliveryFrame, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &user2, nullptr) == 0);
    Require(raise_nid_postfix(31) == 0 && infoSeen.runs == 0);
    rejected = false;
    try {
        sigsuspend_nid_postfix(&none);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && ContextHasMask(infoSeen.context, user2));
    CheckInfo(31, 0x10007);
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0 && SameSet(current, user2));

    const GuestSigaction defaults{0, 0, {}};
    for (const int guest : {6, 30, 31}) Require(sigaction_nid_postfix(guest, &defaults, nullptr) == 0);
    Require(sigprocmask_nid_postfix(3, &none, nullptr) == 0);
}
static void CheckSignalPointerFaults() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page, 3, 0) == 0 && memory != nullptr);
    Require(sceKernelMprotect(memory, page, 1) == 0);
    const auto readOnly = memory;
    const auto unreadable = reinterpret_cast<void*>(std::uintptr_t{0x10});
    const auto fails = [](int result) { return result == -1 && *__error_nid_postfix() == 14; };
    constexpr int user = 30;
    GuestSigaction previous{};
    Require(fails(sigaction_nid_postfix(user, static_cast<const GuestSigaction*>(unreadable), nullptr)));
    Require(fails(sigaction_nid_postfix(0, static_cast<const GuestSigaction*>(unreadable), nullptr)));
    Require(fails(sigaction_nid_postfix(user, nullptr, static_cast<GuestSigaction*>(unreadable))));
    Require(fails(sigaction_nid_postfix(user, nullptr, static_cast<GuestSigaction*>(readOnly))));
    Require(sigaction_nid_postfix(0, nullptr, &previous) == -1 && *__error_nid_postfix() == 22);
    Require(sigaction_nid_postfix(user, nullptr, &previous) == 0);
    GuestSignalSet blocked{}, set{{1u << (user - 1), 0, 0, 0}}, none{};
    Require(sigprocmask_nid_postfix(3, nullptr, &blocked) == 0);
    Require(fails(sigprocmask_nid_postfix(1, unreadable, nullptr)));
    Require(fails(sigprocmask_nid_postfix(9, unreadable, nullptr)));
    Require(sigprocmask_nid_postfix(9, &set, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(fails(sigprocmask_nid_postfix(1, nullptr, unreadable)));
    Require(fails(sigprocmask_nid_postfix(1, nullptr, readOnly)));
    Require(sigprocmask_nid_postfix(1, nullptr, &none) == 0 && std::memcmp(&none, &blocked, sizeof(none)) == 0);
    Require(fails(sigprocmask_nid_postfix(1, &set, readOnly)));
    Require(sigprocmask_nid_postfix(1, nullptr, &none) == 0 && (none.bits[0] & set.bits[0]) != 0);
    Require(sigprocmask_nid_postfix(3, &blocked, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(1, static_cast<const GuestSignalSet*>(unreadable), nullptr) == 14);
    Require(pthread_sigmask_nid_postfix(1, nullptr, static_cast<GuestSignalSet*>(readOnly)) == 14);
    Require(pthread_sigmask_nid_postfix(9, static_cast<const GuestSignalSet*>(unreadable), nullptr) == 14);
    Require(pthread_sigmask_nid_postfix(9, &set, nullptr) == 22);
    Require(sigprocmask_nid_postfix(3, &blocked, nullptr) == 0);
    Require(fails(sigaltstack_nid_postfix(static_cast<const GuestStack*>(unreadable), nullptr)));
    Require(fails(sigaltstack_nid_postfix(nullptr, static_cast<GuestStack*>(unreadable))));
    Require(fails(sigaltstack_nid_postfix(nullptr, static_cast<GuestStack*>(readOnly))));
    Require(fails(sigpending_nid_postfix(static_cast<GuestSignalSet*>(unreadable))));
    Require(fails(sigpending_nid_postfix(static_cast<GuestSignalSet*>(readOnly))));
    Require(fails(sigpending_nid_postfix(nullptr)));
    Require(fails(sigsuspend_nid_postfix(static_cast<const GuestSignalSet*>(unreadable))));
    Require(fails(sigsuspend_nid_postfix(nullptr)));
    Require(fails(_sigsuspend_nid_postfix(static_cast<const GuestSignalSet*>(unreadable))));
    Require(fails(_sigsuspend_nid_postfix(nullptr)));
}

int main() {
    const auto invalid = reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1));
    CheckSignalPointerFaults();
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
    Require(pthread_kill_nid_postfix(nullptr, 15) == 22);
    Require(pthread_kill_nid_postfix(nullptr, 128) == 22);
    Require(pthread_kill_nid_postfix(nullptr, 0) == 22);
    Require(pthread_kill_nid_postfix(nullptr, 129) == 22);
    Require(pthread_kill_nid_postfix(reinterpret_cast<Pthread>(std::uintptr_t{0x10}), 15) == 3);
    Require(pthread_kill_nid_postfix(reinterpret_cast<Pthread>(std::uintptr_t{0x10}), 0) == 3);
    {
        EndingThread ending;
        Pthread thread = nullptr;
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
    {
        const GuestSignalSet parentMask{{1u << 13, 0, 0, 0}};
        Require(sigprocmask_nid_postfix(3, &parentMask, nullptr) == 0);
        MaskProbe probe;
        Pthread child = nullptr;
        Require(scePthreadCreate(&child, nullptr, ChildChangesMask, &probe, nullptr) == 0);
        Require(scePthreadJoin(child, nullptr) == 0);
        Require(probe.inherited.bits[0] == 1u << 13 && probe.changed.bits[0] == 0x2);
        Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 1u << 13);
        Require(sigprocmask_nid_postfix(3, &noSignals, nullptr) == 0);
    }
    {
        const GuestSignalSet user2{{1u << 30, 0, 0, 0}};
        const auto old = signal_nid_postfix(31, Callback);
        Require(old != invalid);
        Require(sigprocmask_nid_postfix(1, &user2, nullptr) == 0);
        received = 0;
        Require(raise_nid_postfix(31) == 0 && received == 0);
        MaskProbe probe;
        Pthread child = nullptr;
        Require(scePthreadCreate(&child, nullptr, ChildTakesPending, &probe, nullptr) == 0);
        Require(scePthreadJoin(child, nullptr) == 0);
        Require(probe.waited == 31 && received == 0);
        Require(sigpending_nid_postfix(&previous) == 0 && previous.bits[0] == 0);
        Require(raise_nid_postfix(31) == 0 && received == 0);
        Require(scePthreadCreate(&child, nullptr, ChildUnblocks, &probe, nullptr) == 0);
        Require(scePthreadJoin(child, nullptr) == 0);
        Require(received == 31 && probe.afterUnblock.bits[0] == 0);
        Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == 1u << 30);
        Require(sigprocmask_nid_postfix(3, &noSignals, nullptr) == 0);
        Require(signal_nid_postfix(31, old) != invalid);
        received = 0;
    }

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
    Require(sigaction_nid_postfix(6, &defaults, &current) == 0 && current.handler == infoCallback && current.flags == 0x50);
    CheckSiginfoDelivery();

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
    Require(sigsuspend_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(_sigsuspend_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
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
    Require(maskInHandler.bits[0] == ((1u << 28) | (1u << 29)));
    Require(sigprocmask_nid_postfix(1, nullptr, &previous) == 0 && previous.bits[0] == user.bits[0]);
    Require(signal_nid_postfix(30, Callback) != invalid);
    received = 0;
    SuspendWait wait{&empty, 0, 0};
    Pthread suspender = nullptr;
    Require(scePthreadCreate(&suspender, nullptr, SuspendEntry, &wait, nullptr) == 0);
    SuspendSender sender{suspender, -1};
    Pthread senderThread = nullptr;
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
    Pthread highSuspender = nullptr;
    Require(scePthreadCreate(&highSuspender, nullptr, SuspendEntry, &highWait, nullptr) == 0);
    SuspendSender highSender{highSuspender, -1};
    Pthread highSenderThread = nullptr;
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
    for (int unmapped : {1, 12, 30, 31, 32, 128}) {
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
    Require(signal_nid_postfix(30, Callback) == nullptr);
    Require(siginterrupt_nid_postfix(30, 1) == 0 && _sigintr_nid_postfix.bits[0] == (1u << 29));
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 &&
        interruptible.handler == reinterpret_cast<std::uintptr_t>(Callback) && interruptible.flags == 0);
    Require(signal_nid_postfix(30, Callback) == Callback);
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 && interruptible.flags == 0);
    Require(siginterrupt_nid_postfix(30, 0) == 0 && _sigintr_nid_postfix.bits[0] == 0);
    Require(sigaction_nid_postfix(30, nullptr, &interruptible) == 0 &&
        interruptible.handler == reinterpret_cast<std::uintptr_t>(Callback) && interruptible.flags == 0x2);
    Require(siginterrupt_nid_postfix(128, 1) == 0 && _sigintr_nid_postfix.bits[3] == (1u << 31));
    Require(sigaction_nid_postfix(128, nullptr, &interruptible) == 0 && interruptible.handler == 0 && interruptible.flags == 0);
    Require(siginterrupt_nid_postfix(128, 0) == 0 && _sigintr_nid_postfix.bits[3] == 0);
    Require(sigaction_nid_postfix(128, nullptr, &interruptible) == 0 && interruptible.flags == 0x2);
    for (const int badSignal : {0, -1, 129}) {
        *__error_nid_postfix() = 0;
        Require(siginterrupt_nid_postfix(badSignal, 1) == -1 && *__error_nid_postfix() == 22);
    }
    Require(_sigintr_nid_postfix.bits[0] == 0 && _sigintr_nid_postfix.bits[3] == 0);
    Require(signal_nid_postfix(30, nullptr) == Callback);
    Require(sigaction_nid_postfix(128, &restored, nullptr) == 0);
}
