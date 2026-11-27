#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/System/include/FaultSignals.hpp"
#include "prx/libkernel/System/include/TimerSignals.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#ifdef __APPLE__
#include <new>
#include <typeinfo>
#endif

static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);

extern "C" {
GuestSignalSet _sigintr_nid_postfix{};
}

namespace {

constexpr int MaxSignal = 128;
constexpr int GuestSigkill = 9;
constexpr int GuestSigstop = 17;
constexpr std::uint32_t UnblockableSignals = (1u << (GuestSigkill - 1)) | (1u << (GuestSigstop - 1));
constexpr int GuestSigalrm = 14;
constexpr int GuestSigbus = 10;
constexpr int GuestSigsegv = 11;
constexpr int GuestSigcont = 19;
constexpr int GuestSigvtalrm = 26;
constexpr int GuestSigprof = 27;
constexpr int SaRestart = 0x2;
constexpr int SaResethand = 0x4;
constexpr int SaNodefer = 0x10;
constexpr int SaSiginfo = 0x40;
constexpr int SaDefinedFlags = 0x7f;
std::atomic<GuestHandler> handlers[MaxSignal + 1]{};
std::atomic<std::uint32_t> handlerMasks[MaxSignal + 1][4]{};
std::atomic<int> handlerFlags[MaxSignal + 1]{};
std::atomic<bool> handlerReset[MaxSignal + 1]{};
std::atomic<std::uintptr_t> siginfoHandlers[MaxSignal + 1]{};
static_assert(std::atomic<GuestHandler>::is_always_lock_free);
struct ThreadSignals {
    std::atomic<std::uint32_t> blocked[4]{};
    std::atomic<std::uint64_t> holds[MaxSignal + 1]{};
};
ThreadSignals& CurrentSignals() {
    thread_local ThreadSignals* signals = nullptr;
    if (signals == nullptr) signals = new ThreadSignals();
    return *signals;
}
constexpr std::uint64_t HoldCount = 0xffffffffu;
thread_local std::uint32_t holdReleases[MaxSignal + 1]{};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
std::mutex registration;
GuestSigaction dispositions[MaxSignal + 1]{};
constexpr int GuestSigblock = 1;
constexpr int GuestSigunblock = 2;
constexpr int GuestSigsetmask = 3;
constexpr int GuestEintr = 4;
constexpr int GuestEio = 5;
constexpr int GuestEagain = 35;
constexpr int GuestErange = 34;
constexpr std::int32_t GuestSiUser = 0x10001;
constexpr std::int32_t GuestSiTimer = 0x10003;
constexpr std::int32_t GuestSiAsyncio = 0x10004;
constexpr std::int32_t GuestSiKernel = 0x10006;
constexpr std::int32_t GuestSiLwp = 0x10007;
struct GuestSiginfo {
    std::int32_t signo;
    std::int32_t error;
    std::int32_t code;
    std::int32_t pid;
    std::uint32_t uid;
    std::int32_t status;
    std::uintptr_t address;
    std::int64_t value;
    std::int32_t timerid;
    std::int32_t overrun;
    std::int32_t spare[7];
    std::int32_t tail;
};
static_assert(sizeof(GuestSiginfo) == 80 && offsetof(GuestSiginfo, code) == 8 && offsetof(GuestSiginfo, pid) == 12 &&
    offsetof(GuestSiginfo, uid) == 16 && offsetof(GuestSiginfo, address) == 24 && offsetof(GuestSiginfo, value) == 32 &&
    offsetof(GuestSiginfo, timerid) == 40 && offsetof(GuestSiginfo, overrun) == 44);

static_assert(sizeof(KernelTimespec) == 16 && offsetof(KernelTimespec, tv_nsec) == 8);
bool IgnoredByDefault(int guest) {
    return guest == 16 || guest == GuestSigcont || guest == 20 || guest == 23 || guest == 28 || guest == 29;
}
std::uint32_t SignalBit(int guest) { return 1u << ((guest - 1) % 32); }
std::uint32_t SignalWord(int guest) { return static_cast<std::uint32_t>((guest - 1) / 32); }
bool SignalInSet(const GuestSignalSet& set, int guest) { return (set.bits[SignalWord(guest)] & SignalBit(guest)) != 0; }
bool SetHasSignal(const GuestSignalSet& set) { return (set.bits[0] | set.bits[1] | set.bits[2] | set.bits[3]) != 0; }
bool HeldByHandler(ThreadSignals& self, int guest) { return (self.holds[guest].load() & HoldCount) != 0; }
void ReleaseHold(ThreadSignals& self, int guest, std::uint32_t epoch) {
    auto value = self.holds[guest].load();
    while ((value >> 32) == epoch && (value & HoldCount) != 0 && !self.holds[guest].compare_exchange_weak(value, value - 1)) {}
}
void ClearHold(ThreadSignals& self, int guest) {
    auto value = self.holds[guest].load();
    while ((value & HoldCount) != 0 && !self.holds[guest].compare_exchange_weak(value, ((value >> 32) + 1) << 32)) {}
}
std::uint32_t HeldWord(ThreadSignals& self, int word) {
    std::uint32_t held = 0;
    for (int bit = 0; bit < 32; ++bit)
        if (HeldByHandler(self, word * 32 + bit + 1)) held |= 1u << bit;
    return held;
}
std::uint32_t BlockedWord(ThreadSignals& self, int word) {
    return self.blocked[word].load() | HeldWord(self, word);
}
bool SignalBlocked(ThreadSignals& self, int guest) {
    if (guest < 1 || guest > MaxSignal) return false;
    return (self.blocked[SignalWord(guest)].load() & SignalBit(guest)) != 0 || HeldByHandler(self, guest);
}
bool SignalBlocked(int guest) { return SignalBlocked(CurrentSignals(), guest); }

struct SignalThreads {
    std::mutex lock;
    std::unordered_map<std::int64_t, ThreadSignals*> byThread;
};

SignalThreads& Threads() {
    static auto* threads = new SignalThreads();
    return *threads;
}

void RegisterSignalThread(ThreadSignals& self) {
    thread_local bool registered = false;
    if (registered) return;
    registered = true;
    const std::int64_t thread = scePthreadGetthreadid();
    auto& threads = Threads();
    std::lock_guard lock(threads.lock);
    threads.byThread[thread] = &self;
}

ThreadSignals* SignalThread(std::int64_t thread) {
    auto& threads = Threads();
    std::lock_guard lock(threads.lock);
    const auto found = threads.byThread.find(thread);
    return found == threads.byThread.end() ? nullptr : found->second;
}

struct QueuedSignal {
    std::int32_t code = 0;
    std::int32_t timer = -1;
    std::int64_t value = 0;
    std::int32_t overrun = 0;
    std::int32_t error = 0;
    std::uint64_t ticket = 0;
};

GuestSiginfo SiginfoOf(int guest, const QueuedSignal& entry) {
    GuestSiginfo info{};
    info.signo = guest;
    info.code = entry.code;
    info.pid = info.code == GuestSiKernel || info.code == GuestSiTimer || info.code == GuestSiAsyncio ? 0 : getpid_nid_postfix();
    if (info.code == GuestSiAsyncio) info.value = entry.value;
    if (info.code == GuestSiTimer) {
        info.error = entry.error;
        info.value = entry.value;
        info.timerid = entry.timer;
        info.overrun = entry.overrun;
    }
    return info;
}

struct TimerOverrun {
    std::int32_t overrun = 0;
    std::int32_t last = 0;
    std::int32_t error = 0;
};

struct PendingSignals {
    std::mutex lock;
    TimedWait::Condition changed;
    std::atomic<std::uint32_t> words[4]{};
    std::deque<QueuedSignal> queued[MaxSignal + 1];
    TimerOverrun timers[TimerSignalSlots];
};

constexpr std::size_t MaxQueuedPerSignal = 1024;

PendingSignals& Pending() {
    static auto* state = new PendingSignals();
    return *state;
}

void MarkPending(int guest, std::int32_t code) {
    auto& pending = Pending();
    {
        // under the same lock the waits check the set with: without it a signal raised
        // between the check and the wait is lost and the wait never returns
        std::lock_guard lock(pending.lock);
        auto& queued = pending.queued[guest];
        const bool merged = code == GuestSiKernel && std::find_if(queued.begin(), queued.end(), [](const QueuedSignal& entry) { return entry.code == GuestSiKernel; }) != queued.end();
        if (!merged && queued.size() < MaxQueuedPerSignal) {
            const auto firstOther = std::find_if(queued.begin(), queued.end(), [](const QueuedSignal& entry) { return entry.code != GuestSiLwp; });
            queued.insert(code == GuestSiLwp ? firstOther : queued.end(), QueuedSignal{code});
        }
        pending.words[SignalWord(guest)].fetch_or(SignalBit(guest), std::memory_order_relaxed);
    }
    pending.changed.NotifyAll();
}

void AcceptTimerLocked(QueuedSignal& entry) {
    auto& timer = Pending().timers[entry.timer];
    entry.overrun = timer.overrun;
    entry.error = timer.error;
    timer.last = timer.overrun;
    timer.overrun = 0;
}

bool TakePendingLocked(int guest, QueuedSignal* taken = nullptr) {
    auto& pending = Pending();
    auto& queued = pending.queued[guest];
    auto& word = pending.words[SignalWord(guest)];
    const std::uint32_t bit = SignalBit(guest);
    if ((word.load(std::memory_order_relaxed) & bit) == 0) return false;
    QueuedSignal entry{};
    if (!queued.empty()) {
        entry = queued.front();
        queued.pop_front();
    }
    if (queued.empty()) word.fetch_and(~bit, std::memory_order_relaxed);
    if (entry.code == GuestSiTimer) AcceptTimerLocked(entry);
    if (taken != nullptr) *taken = entry;
    return true;
}

bool TakePending(int guest, QueuedSignal* taken = nullptr) {
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    return TakePendingLocked(guest, taken);
}

bool TimerQueuedLocked(int guest, int timer) {
    const auto& queued = Pending().queued[guest];
    return std::any_of(queued.begin(), queued.end(), [timer](const QueuedSignal& entry) { return entry.code == GuestSiTimer && entry.timer == timer; });
}

bool RemoveTimerLocked(int guest, int timer) {
    auto& pending = Pending();
    auto& queued = pending.queued[guest];
    const auto found = std::find_if(queued.begin(), queued.end(), [timer](const QueuedSignal& entry) { return entry.code == GuestSiTimer && entry.timer == timer; });
    if (found == queued.end()) return false;
    queued.erase(found);
    if (queued.empty()) pending.words[SignalWord(guest)].fetch_and(~SignalBit(guest), std::memory_order_relaxed);
    return true;
}

void DiscardPendingIfIgnored(int guest, const GuestSigaction& action) {
    if (action.handler != 1 && !(action.handler == 0 && IgnoredByDefault(guest))) return;
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    pending.queued[guest].clear();
    pending.words[SignalWord(guest)].fetch_and(~SignalBit(guest), std::memory_order_relaxed);
}

bool PendingInSet(const GuestSignalSet& set, int* guest, QueuedSignal* taken) {
    for (int candidate = 1; candidate <= MaxSignal; ++candidate) {
        if (!SignalInSet(set, candidate) || !TakePendingLocked(candidate, taken)) continue;
        *guest = candidate;
        return true;
    }
    return false;
}

bool PendingInSetPresent(const GuestSignalSet& set) {
    for (int candidate = 1; candidate <= MaxSignal; ++candidate) {
        if (!SignalInSet(set, candidate)) continue;
        if ((Pending().words[SignalWord(candidate)].load(std::memory_order_relaxed) & SignalBit(candidate)) != 0) return true;
    }
    return false;
}

int NativeSignal(int guest) {
    switch (guest) {
        case 2: return SIGINT;
        case 4: return SIGILL;
        case 6: return SIGABRT;
        case 8: return SIGFPE;
        case 11: return SIGSEGV;
#ifndef _WIN32
        case 13: return SIGPIPE;
#endif
        case 15: return SIGTERM;
        default: return 0;
    }
}
void ResetToDefault(int guest) {
    handlerReset[guest].store(true);
    handlers[guest].store(nullptr);
    siginfoHandlers[guest].store(0);
    if (const int native = NativeSignal(guest)) {
        std::signal(native, SIG_DFL);
        if (native == SIGSEGV) GuestFaultCopyReinstall_nid_no_patch();
    }
}
constexpr int SsDisable = 4;
thread_local GuestStack alternateStack{nullptr, 0, SsDisable};
using GuestInfoHandler = void (APS5_VABI *)(int, void*, UserUcontext*);

GuestSignalSet ContextMask(const UserUcontext& context) {
    return GuestSignalSet{{context.sigmask[0], context.sigmask[1], context.sigmask[2], context.sigmask[3]}};
}

enum class InfoReturn { Unchanged, Mask, Context, DeliveryFrame, Uncaptured };

InfoReturn RunInfoHandler(int guest, GuestHandler handler, const QueuedSignal& entry, const GuestSignalSet& interrupted, UserUcontext& context) {
    GuestSiginfo info = SiginfoOf(guest, entry);
    volatile bool returned = false;
    const int captured = getcontext_nid_postfix(&context);
    if (returned) return InfoReturn::Mask;
    returned = true;
    if (captured != 0) return InfoReturn::Uncaptured;
    std::memcpy(context.sigmask, interrupted.bits, sizeof(context.sigmask));
    context.stackPointer = alternateStack.sp;
    context.stackSize = alternateStack.size;
    context.stackFlags = alternateStack.flags;
    const UserMcontext delivered = context.mcontext;
    reinterpret_cast<GuestInfoHandler>(handler)(guest, &info, &context);
    if (std::memcmp(&context.mcontext, &delivered, sizeof(delivered)) != 0)
        return context.mcontext.rip == delivered.rip || context.mcontext.rsp == delivered.rsp ? InfoReturn::DeliveryFrame : InfoReturn::Context;
    return std::memcmp(context.sigmask, interrupted.bits, sizeof(context.sigmask)) == 0 ? InfoReturn::Unchanged : InfoReturn::Mask;
}

template <typename Call>
auto HoldDuring(int guest, int flags, Call&& call) {
    auto& self = CurrentSignals();
    std::uint32_t mask[4];
    for (int word = 0; word < 4; ++word) mask[word] = handlerMasks[guest][word].load();
    if ((flags & SaNodefer) == 0) mask[SignalWord(guest)] |= SignalBit(guest);
    mask[0] &= ~UnblockableSignals;
    if ((flags & SaResethand) != 0) ResetToDefault(guest);
    std::uint32_t before[4];
    for (int word = 0; word < 4; ++word) before[word] = self.blocked[word].load() & mask[word];
    std::uint32_t epochs[MaxSignal + 1]{};
    std::uint32_t releases[MaxSignal + 1]{};
    for (int held = 1; held <= MaxSignal; ++held) {
        if ((mask[SignalWord(held)] & SignalBit(held)) == 0) continue;
        releases[held] = holdReleases[held];
        epochs[held] = static_cast<std::uint32_t>(self.holds[held].fetch_add(1) >> 32);
    }
    const auto returned = call();
    std::uint32_t replaced[4]{};
    for (int held = 1; held <= MaxSignal; ++held) {
        if ((mask[SignalWord(held)] & SignalBit(held)) == 0) continue;
        if (holdReleases[held] != releases[held]) replaced[SignalWord(held)] |= SignalBit(held);
        ReleaseHold(self, held, epochs[held]);
    }
    for (int word = 0; word < 4; ++word) {
        if (replaced[word] == 0) continue;
        auto& target = self.blocked[word];
        target.fetch_and(~(replaced[word] & ~before[word]));
        target.fetch_or(replaced[word] & before[word]);
    }
    return returned;
}

std::optional<GuestSignalSet> RunHandler(int guest, GuestHandler handler, int flags, const QueuedSignal& entry = {}, const GuestSignalSet* returnMask = nullptr) {
    auto& self = CurrentSignals();
    GuestSignalSet interrupted{};
    for (int word = 0; word < 4; ++word) interrupted.bits[word] = returnMask != nullptr ? returnMask->bits[word] : BlockedWord(self, word);
    UserUcontext context{};
    const InfoReturn returned = HoldDuring(guest, flags, [&] {
        if ((flags & SaSiginfo) != 0) return RunInfoHandler(guest, handler, entry, interrupted, context);
        handler(guest);
        return InfoReturn::Unchanged;
    });
    if (returned == InfoReturn::Uncaptured) throw std::runtime_error("signal: capturing the context of an SA_SIGINFO delivery failed");
    if (returned == InfoReturn::DeliveryFrame) NotImplemented_nid_no_patch("sigreturn: an SA_SIGINFO handler changed uc_mcontext but kept the %rip or %rsp of the delivery point, whose registers are libkernel's and not those of the system call");
    if (returned == InfoReturn::Context) {
        SigreturnContext_nid_no_patch(&context);
        NotImplemented_nid_no_patch("sigreturn: an SA_SIGINFO handler left a context that sigreturn rejects, where FreeBSD's signal trampoline faults on hlt");
    }
    if (returned == InfoReturn::Unchanged) return std::nullopt;
    return ContextMask(context);
}
void Dispatch(int native) {
    int guest = 0;
    for (int candidate : {2, 4, 6, 8, 11, 13, 15})
        if (NativeSignal(candidate) == native) { guest = candidate; break; }
    if (!guest) return;
#ifdef _WIN32
    // Preserve the guest's persistent registration across CRT delivery.
    std::signal(native, Dispatch);
#endif
    if (SignalBlocked(guest)) {
        MarkPending(guest, GuestSiLwp);
        return;
    }
    const auto callback = handlers[guest].load();
    if (reinterpret_cast<std::uintptr_t>(callback) > 1) RunHandler(guest, callback, handlerFlags[guest].load() & ~SaSiginfo);
}
bool CaughtWithInfo(const GuestSigaction& action) {
    return action.handler > 1 && (action.flags & SaSiginfo) != 0;
}
bool Delivered(const GuestSigaction& action) {
    return action.handler > 1 && !CaughtWithInfo(action);
}
bool Install(int guest, const GuestSigaction& action) {
    handlerReset[guest].store(false);
    for (int word = 0; word < 4; ++word) handlerMasks[guest][word].store(action.mask.bits[word]);
    handlerFlags[guest].store(action.flags);
    const bool siginfo = action.handler > 1 && (action.flags & SaSiginfo) != 0;
    siginfoHandlers[guest].store(siginfo ? action.handler : 0);
    if (siginfo && (guest == GuestSigbus || guest == GuestSigsegv)) {
        handlers[guest].store(nullptr);
        InstallSiginfoFaultDelivery();
        return true;
    }
    const int native = NativeSignal(guest);
    if (!native) return true;
    const auto previous = handlers[guest].exchange(Delivered(action) ? reinterpret_cast<GuestHandler>(action.handler) : nullptr);
    auto hostHandler = action.handler == 1 ? SIG_IGN : Delivered(action) ? Dispatch : SIG_DFL;
    if (std::signal(native, hostHandler) == SIG_ERR) {
        handlers[guest].store(previous);
        return false;
    }
    if (native == SIGSEGV) GuestFaultCopyReinstall_nid_no_patch();
    return true;
}
}

namespace {

void SettleReset(int guest) {
    if (!handlerReset[guest].exchange(false)) return;
    dispositions[guest].handler = 0;
    dispositions[guest].flags &= ~SaSiginfo;
}

GuestSigaction Disposition(int guest) {
    std::lock_guard lock(registration);
    SettleReset(guest);
    return dispositions[guest];
}

bool CaughtByGuest(int guest) {
    return Disposition(guest).handler > 1;
}

bool IsTimerSignal(int guest) { return guest == GuestSigalrm || guest == GuestSigvtalrm || guest == GuestSigprof; }
const char* TimerSignalName(int guest) { return guest == GuestSigalrm ? "SIGALRM" : guest == GuestSigvtalrm ? "SIGVTALRM" : "SIGPROF"; }

bool TerminatesWithoutHandler(int guest) {
    return IsTimerSignal(guest) && Disposition(guest).handler == 0;
}

[[noreturn]] void TerminateByTimerSignal(int guest, const std::string& cause) {
    std::fprintf(stderr, "%s: %s with the default disposition; the process terminates\n", TimerSignalName(guest), cause.c_str());
    std::fflush(stderr);
    std::_Exit(128 + guest);
}

[[noreturn]] void TerminateBySigalrm(const char* cause) {
    TerminateByTimerSignal(GuestSigalrm, cause);
}

bool Deliverable(int guest) {
    return NativeSignal(guest) != 0 || CaughtByGuest(guest) || TerminatesWithoutHandler(guest);
}

bool TakePendingUnblocked(int* guest, QueuedSignal* taken) {
    for (int candidate = 1; candidate <= MaxSignal; ++candidate) {
        if (SignalBlocked(candidate) || !Deliverable(candidate)) continue;
        if (!TakePendingLocked(candidate, taken)) continue;
        *guest = candidate;
        return true;
    }
    return false;
}

bool DeliverPendingSignal(int guest, const QueuedSignal& entry, const GuestSignalSet* returnMask = nullptr, std::optional<GuestSignalSet>* contextMask = nullptr) {
    const GuestSigaction action = Disposition(guest);
    if (action.handler == 1) return false;
    if (action.handler > 1 && !Delivered(action)) throw std::runtime_error("signal: SA_SIGINFO handlers are not delivered");
    if (IsTimerSignal(guest) && action.handler == 0)
        TerminateByTimerSignal(guest, "a pending " + std::string(TimerSignalName(guest)) + " was unblocked");
    const int native = NativeSignal(guest);
    if (native != 0 && !CaughtWithInfo(action)) {
        if (std::raise(native) != 0) throw std::runtime_error("signal: delivering the signal to the host failed");
        return action.handler > 1;
    }
    if (action.handler > 1) {
        const auto restored = RunHandler(guest, reinterpret_cast<GuestHandler>(action.handler), action.flags, entry, returnMask);
        if (contextMask != nullptr) *contextMask = restored;
        else if (restored) sigprocmask_nid_postfix(GuestSigsetmask, &*restored, nullptr);
        return true;
    }
    return false;
}

bool DeliverUnblockedPending() {
    bool caught = false;
    for (int guest = 1; guest <= MaxSignal; ++guest) {
        QueuedSignal entry{};
        while (!SignalBlocked(guest) && Deliverable(guest) && TakePending(guest, &entry)) {
            if (DeliverPendingSignal(guest, entry)) caught = true;
        }
    }
    return caught;
}

std::optional<std::uint64_t> SignalWaitDeadline(const KernelTimespec& timeout) {
    if (timeout.tv_sec > INT32_MAX) return std::nullopt;
    const std::uint64_t now = TimedWait::NowNanos();
    if (timeout.tv_sec < 0) return now;
    return now + static_cast<std::uint64_t>(timeout.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(timeout.tv_nsec);
}

int WaitForPendingInSet(const GuestSignalSet& set, bool waitForever, const KernelTimespec* timeout, int* guest, QueuedSignal* taken) {
    auto& pending = Pending();
    std::unique_lock lock(pending.lock);
    const std::optional<std::uint64_t> deadline = waitForever ? std::nullopt : SignalWaitDeadline(*timeout);
    for (;;) {
        if (PendingInSet(set, guest, taken)) return 0;
        if (!deadline) {
            pending.changed.Wait(lock);
            continue;
        }
        if (pending.changed.WaitUntil(lock, *deadline)) continue;
        return PendingInSet(set, guest, taken) ? 0 : GuestEagain;
    }
}

bool FillSiginfo(GuestSiginfo* info, int guest, const QueuedSignal& entry) {
    const GuestSiginfo local = SiginfoOf(guest, entry);
    return GuestCopyOut_nid_no_patch(info, &local, sizeof(local));
}

void WaitForDeliverableSignal(const GuestSignalSet* returnMask = nullptr, std::optional<GuestSignalSet>* contextMask = nullptr) {
    auto& pending = Pending();
    for (;;) {
        int guest = 0;
        QueuedSignal entry{};
        {
            std::unique_lock lock(pending.lock);
            if (!TakePendingUnblocked(&guest, &entry)) {
                pending.changed.Wait(lock);
                continue;
            }
        }
        if (DeliverPendingSignal(guest, entry, returnMask, contextMask)) return;
    }
}

}

namespace {
constexpr std::int64_t MaxItimerSeconds = 0x7fffffff / 2;
constexpr int ItimerReal = 0;
constexpr int ItimerVirtual = 1;
constexpr int ItimerProf = 2;
constexpr std::int64_t MinExactIntervalMicros = 1000;
constexpr std::uint64_t CpuTimerPollNanos = 1000000ULL;

bool ValidItimer(const KernelTimeval& time) {
    return time.tv_sec >= 0 && time.tv_usec >= 0 && time.tv_usec < 1000000 && time.tv_sec <= MaxItimerSeconds;
}

bool ItimerSet(const KernelTimeval& time) {
    return time.tv_sec != 0 || time.tv_usec != 0;
}

std::chrono::microseconds ItimerDuration(const KernelTimeval& time) {
    return std::chrono::seconds(time.tv_sec) + std::chrono::microseconds(time.tv_usec);
}

KernelTimeval ItimerTimeval(std::chrono::microseconds duration) {
    return {duration.count() / 1000000, duration.count() % 1000000};
}

std::chrono::microseconds CpuTimerDuration(const KernelTimeval& time) {
    const auto duration = ItimerDuration(time);
    if (duration.count() != 0 && duration.count() < MinExactIntervalMicros)
        return std::chrono::microseconds(MinExactIntervalMicros);
    return duration;
}

struct AlarmState {
    std::mutex lock;
    TimedWait::Condition changed;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::chrono::microseconds interval{0};
    bool threadStarted = false;
    ThreadSignals* armer = nullptr;
};

AlarmState& Alarms() {
    static auto* state = new AlarmState();
    return *state;
}

std::string DescribeInterval(std::chrono::microseconds interval) {
    char text[64];
    std::snprintf(text, sizeof(text), "%lld.%06lld s", static_cast<long long>(interval.count() / 1000000),
        static_cast<long long>(interval.count() % 1000000));
    return text;
}

[[noreturn]] void ThrowTimerExpiry(const std::string& message) {
#ifdef __APPLE__
    void* object = __cxa_allocate_exception_nid_postfix(sizeof(std::runtime_error));
    new (object) std::runtime_error(message);
    __cxa_throw_nid_postfix(object, const_cast<std::type_info*>(&typeid(std::runtime_error)), [](void* thrown) { static_cast<std::runtime_error*>(thrown)->~runtime_error(); });
#else
    throw std::runtime_error(message);
#endif
}

void ExpireAlarm(std::chrono::microseconds interval, ThreadSignals& armer) {
    std::uintptr_t handler = 0;
    {
        std::lock_guard lock(registration);
        handler = dispositions[GuestSigalrm].handler;
    }
    KqueueSignalSent(GuestSigalrm);
    if (handler == 1) return;
    if (SignalBlocked(armer, GuestSigalrm)) {
        MarkPending(GuestSigalrm, GuestSiKernel);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (SignalBlocked(armer, GuestSigalrm) || !TakePending(GuestSigalrm)) return;
        handler = Disposition(GuestSigalrm).handler;
        if (handler == 1) return;
    }
    if (handler == 0) TerminateBySigalrm("the alarm expired");
    ThrowTimerExpiry("ITIMER_REAL (it_interval " + DescribeInterval(interval) + ") expired: delivering SIGALRM to a guest handler is not implemented");
}

void RunAlarms() {
    auto& alarms = Alarms();
    std::unique_lock lock(alarms.lock);
    for (;;) {
        if (!alarms.deadline) {
            alarms.changed.Wait(lock);
            continue;
        }
        const auto deadline = *alarms.deadline;
        const auto now = std::chrono::steady_clock::now();
        if (now < deadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
            alarms.changed.WaitUntil(lock, TimedWait::NowNanos() + static_cast<std::uint64_t>(remaining));
            continue;
        }
        const auto interval = alarms.interval;
        ThreadSignals& armer = *alarms.armer;
        if (interval.count() == 0) alarms.deadline.reset();
        else alarms.deadline = deadline + ((now - deadline) / interval + 1) * interval;
        lock.unlock();
        ExpireAlarm(interval, armer);
        lock.lock();
    }
}

std::chrono::microseconds RemainingAlarm(const AlarmState& alarms, std::chrono::steady_clock::time_point now) {
    if (!alarms.deadline || *alarms.deadline <= now) return std::chrono::microseconds(0);
    return std::chrono::duration_cast<std::chrono::microseconds>(*alarms.deadline - now);
}

void NotifyAlarms(AlarmState& alarms) {
    if (!alarms.threadStarted) {
        Libc::StartHostThread(RunAlarms).detach();
        alarms.threadStarted = true;
    }
    alarms.changed.NotifyAll();
}

struct CpuTimerState {
    std::mutex lock;
    TimedWait::Condition changed;
    std::optional<std::uint64_t> deadlineMicros;
    std::chrono::microseconds interval{0};
    bool threadStarted = false;
    ThreadSignals* armer = nullptr;
};

CpuTimerState& CpuTimer(int which) {
    static auto* virtualTimer = new CpuTimerState();
    static auto* profTimer = new CpuTimerState();
    return which == ItimerVirtual ? *virtualTimer : *profTimer;
}

bool ReadCpuTimerMicros(int which, std::uint64_t* micros) {
    std::uint64_t userNanos = 0;
    std::uint64_t systemNanos = 0;
    const int savedErrno = *__error_nid_postfix();
    if (!GuestProcessCpuTimes_nid_no_patch(&userNanos, &systemNanos)) {
        *__error_nid_postfix() = GuestEio;
        return false;
    }
    *__error_nid_postfix() = savedErrno;
    const auto nanos = userNanos + (which == ItimerProf ? systemNanos : 0);
    *micros = nanos / 1000ULL;
    return true;
}

std::chrono::microseconds RemainingCpuTimer(const CpuTimerState& timer, std::uint64_t nowMicros) {
    if (!timer.deadlineMicros || *timer.deadlineMicros <= nowMicros) return std::chrono::microseconds(0);
    return std::chrono::microseconds(*timer.deadlineMicros - nowMicros);
}

void ExpireCpuTimer(int which, std::chrono::microseconds interval, ThreadSignals& armer) {
    const int guest = which == ItimerVirtual ? GuestSigvtalrm : GuestSigprof;
    std::uintptr_t handler = 0;
    {
        std::lock_guard lock(registration);
        handler = dispositions[guest].handler;
    }
    KqueueSignalSent(guest);
    if (handler == 1) return;
    if (SignalBlocked(armer, guest)) {
        MarkPending(guest, GuestSiKernel);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (SignalBlocked(armer, guest) || !TakePending(guest)) return;
        handler = Disposition(guest).handler;
        if (handler == 1) return;
    }
    if (handler == 0) TerminateByTimerSignal(guest, "the CPU interval timer expired");
    const char* timerName = which == ItimerVirtual ? "ITIMER_VIRTUAL" : "ITIMER_PROF";
    ThrowTimerExpiry(std::string(timerName) + " (it_interval " + DescribeInterval(interval)
        + ") expired: delivering " + TimerSignalName(guest) + " to a guest handler is not implemented");
}

void RunCpuTimer(int which) {
    auto& timer = CpuTimer(which);
    std::unique_lock lock(timer.lock);
    for (;;) {
        if (!timer.deadlineMicros) {
            timer.changed.Wait(lock);
            continue;
        }
        const std::uint64_t deadline = *timer.deadlineMicros;
        lock.unlock();
        std::uint64_t nowMicros = 0;
        const bool read = ReadCpuTimerMicros(which, &nowMicros);
        lock.lock();
        if (!timer.deadlineMicros || *timer.deadlineMicros != deadline) continue;
        if (!read || nowMicros < deadline) {
            timer.changed.WaitUntil(lock, TimedWait::NowNanos() + CpuTimerPollNanos);
            continue;
        }
        const auto interval = timer.interval;
        ThreadSignals& armer = *timer.armer;
        if (interval.count() == 0) timer.deadlineMicros.reset();
        else {
            const std::uint64_t step = static_cast<std::uint64_t>(interval.count());
            timer.deadlineMicros = deadline + ((nowMicros - deadline) / step + 1) * step;
        }
        lock.unlock();
        ExpireCpuTimer(which, interval, armer);
        lock.lock();
    }
}
void RunVirtualTimer() { RunCpuTimer(ItimerVirtual); }
void RunProfTimer() { RunCpuTimer(ItimerProf); }

void NotifyCpuTimer(CpuTimerState& timer, int which) {
    if (!timer.threadStarted) {
        Libc::StartHostThread(which == ItimerVirtual ? RunVirtualTimer : RunProfTimer).detach();
        timer.threadStarted = true;
    }
    timer.changed.NotifyAll();
}
}

extern "C" unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int seconds) {
    if (seconds > MaxItimerSeconds) {
        *__error_nid_postfix() = 22;
        return static_cast<unsigned int>(-1);
    }
    auto& alarms = Alarms();
    std::lock_guard lock(alarms.lock);
    const auto now = std::chrono::steady_clock::now();
    const auto micros = RemainingAlarm(alarms, now).count();
    const auto remaining = static_cast<unsigned int>((micros + 999999) / 1000000);
    alarms.interval = std::chrono::microseconds(0);
    if (seconds == 0) alarms.deadline.reset();
    else {
        alarms.deadline = now + std::chrono::seconds(seconds);
        alarms.armer = &CurrentSignals();
    }
    NotifyAlarms(alarms);
    return remaining;
}

static_assert(sizeof(GuestItimerval) == 32 && offsetof(GuestItimerval, value) == 16);

static int GetitimerInto(int which, GuestItimerval* output) {
    if (which < 0 || which > 2) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (output == nullptr) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    GuestItimerval result{};
    if (which != ItimerReal) {
        std::uint64_t nowMicros = 0;
        if (!ReadCpuTimerMicros(which, &nowMicros)) return -1;
        auto& timer = CpuTimer(which);
        std::lock_guard lock(timer.lock);
        result.interval = ItimerTimeval(timer.interval);
        result.value = ItimerTimeval(RemainingCpuTimer(timer, nowMicros));
        *output = result;
        return 0;
    }
    auto& alarms = Alarms();
    {
        std::lock_guard lock(alarms.lock);
        const auto now = std::chrono::steady_clock::now();
        if (alarms.deadline && *alarms.deadline > now) {
            const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(*alarms.deadline - now).count();
            result.value.tv_sec = micros / 1000000;
            result.value.tv_usec = micros % 1000000;
        }
        const auto interval = alarms.interval.count();
        result.interval.tv_sec = interval / 1000000;
        result.interval.tv_usec = interval % 1000000;
    }
    *output = result;
    return 0;
}

extern "C" int APS5_VABI getitimer_nid_postfix(int which, GuestItimerval* output) {
    GuestItimerval result{};
    const int status = GetitimerInto(which, output == nullptr ? nullptr : &result);
    if (status == 0 && !GuestCopyOut_nid_no_patch(output, &result, sizeof(result))) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    return status;
}

extern "C" int APS5_VABI setitimer_nid_postfix(int which, const GuestItimerval* value, GuestItimerval* previous) {
    GuestItimerval requested{};
    if (value != nullptr && !GuestCopyIn_nid_no_patch(&requested, value, sizeof(requested))) { *__error_nid_postfix() = 14; return -1; }
    if (which < ItimerReal || which > ItimerProf) { *__error_nid_postfix() = 22; return -1; }
    if (value == nullptr && previous == nullptr) { *__error_nid_postfix() = 14; return -1; }
    if (value != nullptr) {
        if (!ValidItimer(requested.value)) { *__error_nid_postfix() = 22; return -1; }
        if (!ItimerSet(requested.value)) requested.interval = {};
        else if (!ValidItimer(requested.interval)) { *__error_nid_postfix() = 22; return -1; }
    }
    if (which != ItimerReal) {
        scePthreadSelf();
        std::uint64_t nowMicros = 0;
        if (!ReadCpuTimerMicros(which, &nowMicros)) return -1;
        GuestItimerval old{};
        {
            auto& timer = CpuTimer(which);
            std::lock_guard lock(timer.lock);
            old.interval = ItimerTimeval(timer.interval);
            old.value = ItimerTimeval(RemainingCpuTimer(timer, nowMicros));
            if (value != nullptr) {
                const auto initial = CpuTimerDuration(requested.value);
                timer.interval = CpuTimerDuration(requested.interval);
                if (initial.count() == 0) {
                    timer.deadlineMicros.reset();
                    timer.armer = nullptr;
                } else {
                    timer.deadlineMicros = nowMicros + static_cast<std::uint64_t>(initial.count());
                    timer.armer = &CurrentSignals();
                }
                NotifyCpuTimer(timer, which);
            }
        }
        if (previous != nullptr && !GuestCopyOut_nid_no_patch(previous, &old, sizeof(old))) { *__error_nid_postfix() = 14; return -1; }
        return 0;
    }
    if (requested.interval.tv_sec == 0 && requested.interval.tv_usec != 0 && requested.interval.tv_usec < MinExactIntervalMicros)
        throw std::runtime_error("setitimer: it_interval " + DescribeInterval(ItimerDuration(requested.interval)) + " is below 1 ms, where the kernel rounds it up to a clock tick that is not known");
    GuestItimerval old{};
    {
        auto& alarms = Alarms();
        std::lock_guard lock(alarms.lock);
        const auto now = std::chrono::steady_clock::now();
        old.interval = ItimerTimeval(alarms.interval);
        old.value = ItimerTimeval(RemainingAlarm(alarms, now));
        if (value != nullptr) {
            alarms.interval = ItimerDuration(requested.interval);
            if (ItimerSet(requested.value)) {
                alarms.deadline = now + ItimerDuration(requested.value);
                alarms.armer = &CurrentSignals();
            } else alarms.deadline.reset();
            NotifyAlarms(alarms);
        }
    }
    if (previous != nullptr && !GuestCopyOut_nid_no_patch(previous, &old, sizeof(old))) { *__error_nid_postfix() = 14; return -1; }
    return 0;
}

namespace {

void AddTimerOverrun(TimerOverrun& timer, std::int64_t count) {
    const std::int64_t total = timer.overrun + count;
    timer.overrun = static_cast<std::int32_t>(std::min<std::int64_t>(total, INT_MAX));
    if (total > INT_MAX) timer.error = GuestErange;
}

void QueueTimerSignal(const TimerSignal& signal) {
    auto& pending = Pending();
    {
        std::lock_guard lock(pending.lock);
        pending.queued[signal.guest].push_back({GuestSiTimer, signal.timer, signal.value});
        pending.words[SignalWord(signal.guest)].fetch_or(SignalBit(signal.guest), std::memory_order_relaxed);
    }
    pending.changed.NotifyAll();
}

bool IgnoredOnSend(int guest, const GuestSigaction& action) {
    return action.handler == 1 || (action.handler == 0 && guest != GuestSigcont && IgnoredByDefault(guest));
}

}

bool SendTimerSignal(const TimerSignal& signal, std::int64_t missed) {
    if (signal.directed && !LiveGuestThreadExists(signal.thread)) return false;
    auto& pending = Pending();
    {
        std::lock_guard lock(pending.lock);
        auto& timer = pending.timers[signal.timer];
        AddTimerOverrun(timer, missed);
        if (TimerQueuedLocked(signal.guest, signal.timer)) {
            AddTimerOverrun(timer, 1);
            return true;
        }
        timer.error = 0;
    }
    GuestSigaction action = Disposition(signal.guest);
    KqueueSignalSent(signal.guest);
    if (IgnoredOnSend(signal.guest, action)) return true;
    ThreadSignals* target = SignalThread(signal.thread);
    if (target != nullptr && SignalBlocked(*target, signal.guest)) {
        QueueTimerSignal(signal);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (SignalBlocked(*target, signal.guest)) return true;
        {
            std::lock_guard lock(pending.lock);
            if (!RemoveTimerLocked(signal.guest, signal.timer)) return true;
        }
        action = Disposition(signal.guest);
        if (IgnoredOnSend(signal.guest, action)) return true;
    }
    if (action.handler == 0 && signal.guest == GuestSigalrm) TerminateBySigalrm("a timer expired");
    ThrowTimerExpiry("ktimer " + std::to_string(signal.timer) + " expired: delivering signal " + std::to_string(signal.guest) + " to a thread that does not block it is not implemented");
}

void ForgetTimerSignal(int timer) {
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    for (int guest = 1; guest <= MaxSignal; ++guest)
        while (RemoveTimerLocked(guest, timer)) {}
    pending.timers[timer] = {};
}

int TimerSignalOverrun(int timer) {
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    return pending.timers[timer].last;
}

namespace {

std::uint64_t QueueValueSignal(int guest, std::int32_t code, std::int64_t value) {
    static std::uint64_t nextTicket = 0;
    auto& pending = Pending();
    std::uint64_t ticket = 0;
    {
        std::lock_guard lock(pending.lock);
        auto& queued = pending.queued[guest];
        const bool merged = code == GuestSiKernel && std::any_of(queued.begin(), queued.end(), [](const QueuedSignal& entry) { return entry.code == GuestSiKernel; });
        if (merged) return 0;
        ticket = ++nextTicket;
        queued.push_back({code, -1, value, 0, 0, ticket});
        pending.words[SignalWord(guest)].fetch_or(SignalBit(guest), std::memory_order_relaxed);
    }
    pending.changed.NotifyAll();
    return ticket;
}

bool RemoveValueSignalLocked(int guest, std::uint64_t ticket) {
    auto& pending = Pending();
    auto& queued = pending.queued[guest];
    const auto found = std::find_if(queued.begin(), queued.end(), [ticket](const QueuedSignal& entry) { return entry.ticket == ticket; });
    if (found == queued.end()) return false;
    queued.erase(found);
    if (queued.empty()) pending.words[SignalWord(guest)].fetch_and(~SignalBit(guest), std::memory_order_relaxed);
    return true;
}

}

extern "C" int SendValueSignal_nid_no_patch(int guest, std::int32_t code, std::int64_t value, std::int64_t thread, bool directed, std::uint64_t* ticket) {
    *ticket = 0;
    if (directed && !LiveGuestThreadExists(thread)) return SignalSendNoThread;
    GuestSigaction action = Disposition(guest);
    KqueueSignalSent(guest);
    if (IgnoredOnSend(guest, action)) return SignalSendDone;
    ThreadSignals* target = SignalThread(thread);
    if (target == nullptr || !SignalBlocked(*target, guest)) return SignalSendUnblocked;
    const std::uint64_t queued = QueueValueSignal(guest, code, value);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (queued == 0) return SignalSendDone;
    if (SignalBlocked(*target, guest)) {
        *ticket = queued;
        return SignalSendDone;
    }
    auto& pending = Pending();
    {
        std::lock_guard lock(pending.lock);
        if (!RemoveValueSignalLocked(guest, queued)) return SignalSendDone;
    }
    return IgnoredOnSend(guest, Disposition(guest)) ? SignalSendDone : SignalSendUnblocked;
}

extern "C" void TakeValueSignal_nid_no_patch(int guest, std::uint64_t ticket) {
    if (ticket == 0) return;
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    RemoveValueSignalLocked(guest, ticket);
}

static_assert(sizeof(GuestStack) == 24 && offsetof(GuestStack, flags) == 16);
namespace {
constexpr std::size_t MinSignalStackSize = 2048;
}

namespace {

int ChangeBlockedMask(int how, const GuestSignalSet* set, GuestSignalSet* previousSet, bool releaseHolds, bool* caught = nullptr, GuestSignalSet* guestPrevious = nullptr) {
    if (set != nullptr && (how < 1 || how > 3)) { *__error_nid_postfix() = 22; return -1; }
    auto& self = CurrentSignals();
    if (set != nullptr) RegisterSignalThread(self);
    {
        std::lock_guard lock(registration);
        if (previousSet != nullptr)
            for (int word = 0; word < 4; ++word) previousSet->bits[word] = BlockedWord(self, word);
        if (set != nullptr) {
            switch (how) {
                case 1:
                    self.blocked[0].fetch_or(set->bits[0] & ~UnblockableSignals);
                    for (int word = 1; word < 4; ++word) self.blocked[word].fetch_or(set->bits[word]);
                    break;
                case 2:
                    for (int word = 0; word < 4; ++word) self.blocked[word].fetch_and(~set->bits[word]);
                    if (releaseHolds)
                        for (int guest = 1; guest <= MaxSignal; ++guest) {
                            if (!SignalInSet(*set, guest)) continue;
                            ClearHold(self, guest);
                            ++holdReleases[guest];
                        }
                    break;
                case 3:
                    self.blocked[0].store(set->bits[0] & ~UnblockableSignals);
                    for (int word = 1; word < 4; ++word) self.blocked[word].store(set->bits[word]);
                    if (releaseHolds)
                        for (int guest = 1; guest <= MaxSignal; ++guest) {
                            ClearHold(self, guest);
                            ++holdReleases[guest];
                        }
                    break;
            }
        }
    }
    const bool faulted = guestPrevious != nullptr && !GuestCopyOut_nid_no_patch(guestPrevious, previousSet, sizeof(*previousSet));
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (DeliverUnblockedPending() && caught != nullptr) *caught = true;
    if (faulted) { *__error_nid_postfix() = 14; return -1; }
    return 0;
}

}

bool SiginfoFaultHandlerInstalled(int guest) {
    return guest >= 1 && guest <= MaxSignal && siginfoHandlers[guest].load() > 1 && !SignalBlocked(guest);
}

void RunSiginfoFaultHandler(int guest, std::int32_t code, std::uintptr_t address, std::uint32_t trap, void* ucontext) {
    using SiginfoHandler = void (APS5_VABI *)(int, GuestSiginfo*, void*);
    const auto handler = reinterpret_cast<SiginfoHandler>(siginfoHandlers[guest].load());
    if (reinterpret_cast<std::uintptr_t>(handler) <= 1) return;
    GuestSiginfo info{};
    info.signo = guest;
    info.code = code;
    info.address = address;
    info.timerid = static_cast<std::int32_t>(trap);
    auto& context = *static_cast<UserUcontext*>(ucontext);
    auto& self = CurrentSignals();
    GuestSignalSet interrupted{};
    for (int word = 0; word < 4; ++word) interrupted.bits[word] = BlockedWord(self, word);
    std::memcpy(context.sigmask, interrupted.bits, sizeof(context.sigmask));
    HoldDuring(guest, handlerFlags[guest].load(), [&] {
        handler(guest, &info, ucontext);
        return 0;
    });
    if (std::memcmp(context.sigmask, interrupted.bits, sizeof(context.sigmask)) == 0) return;
    const GuestSignalSet restored = ContextMask(context);
    sigprocmask_nid_postfix(GuestSigsetmask, &restored, nullptr);
}

extern "C" int APS5_VABI SetSignalMaskCatching_nid_no_patch(const GuestSignalSet* set, GuestSignalSet* previousSet) {
    bool caught = false;
    ChangeBlockedMask(GuestSigsetmask, set, previousSet, true, &caught);
    return caught ? 1 : 0;
}

extern "C" {
GuestHandler APS5_VABI signal_nid_postfix(int guest, GuestHandler handler) {
    const auto invalid = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    if (guest < 1 || guest > MaxSignal || ((guest == GuestSigkill || guest == GuestSigstop) && handler != nullptr)) { *__error_nid_postfix() = 22; return invalid; }
    std::lock_guard lock(registration);
    SettleReset(guest);
    const GuestSigaction action{reinterpret_cast<std::uintptr_t>(handler), SignalInSet(_sigintr_nid_postfix, guest) ? 0 : SaRestart, {}};
    if (!Install(guest, action)) { *__error_nid_postfix() = 22; return invalid; }
    const auto previous = reinterpret_cast<GuestHandler>(dispositions[guest].handler);
    dispositions[guest] = action;
    DiscardPendingIfIgnored(guest, action);
    return previous;
}
int APS5_VABI sigaction_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    GuestSigaction requested{};
    if (action != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&requested, action, sizeof(requested))) { *__error_nid_postfix() = 14; return -1; }
        action = &requested;
    }
    if (guest < 1 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    if (action && action->handler > 1 && (action->flags & ~SaDefinedFlags) != 0) { *__error_nid_postfix() = 22; return -1; }
    if (action && (guest == GuestSigkill || guest == GuestSigstop) && action->handler != 0) { *__error_nid_postfix() = 22; return -1; }
    std::lock_guard lock(registration);
    SettleReset(guest);
    const GuestSigaction old = dispositions[guest];
    if (action) {
        if (!Install(guest, *action)) { *__error_nid_postfix() = 22; return -1; }
        dispositions[guest] = *action;
        DiscardPendingIfIgnored(guest, *action);
    }
    if (previous && !GuestCopyOut_nid_no_patch(previous, &old, sizeof(old))) { *__error_nid_postfix() = 14; return -1; }
    return 0;
}
int APS5_VABI _sigaction_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return sigaction_nid_postfix(guest, action, previous);
}
static int RaiseWithCode(int guest, std::int32_t code) {
    if (guest == 0) return 0;
    if (guest < 1 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    const GuestSigaction action = Disposition(guest);
    KqueueSignalSent(guest);
    if (action.handler == 1 || (action.handler == 0 && guest != GuestSigcont && IgnoredByDefault(guest))) return 0;
    if (SignalBlocked(guest)) {
        MarkPending(guest, code);
        return 0;
    }
    if (action.handler == 0 && IgnoredByDefault(guest)) return 0;
    const int native = NativeSignal(guest);
    if (native != 0 && !CaughtWithInfo(action)) {
        const int result = std::raise(native);
        if (result) { *__error_nid_postfix() = 22; return -1; }
        DeliverUnblockedPending();
        return 0;
    }
    if (!CaughtByGuest(guest)) { *__error_nid_postfix() = 22; return -1; }
    DeliverPendingSignal(guest, QueuedSignal{code});
    DeliverUnblockedPending();
    return 0;
}
int APS5_VABI raise_nid_postfix(int guest) {
    return RaiseWithCode(guest, GuestSiLwp);
}
int APS5_VABI kill_nid_postfix(int pid, int guest) {
    if (guest < 0 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    const int self = getpid_nid_postfix();
    if (pid != self && pid != 0 && pid != -self) { *__error_nid_postfix() = 3; return -1; }
    if (guest == 0) return 0;
    return RaiseWithCode(guest, GuestSiUser);
}
int APS5_VABI killpg_nid_postfix(int pgrp, int guest) {
    if (pgrp == 1) { *__error_nid_postfix() = 3; return -1; }
    return kill_nid_postfix(static_cast<int>(0u - static_cast<unsigned>(pgrp)), guest);
}
int APS5_VABI pthread_kill_nid_postfix(Pthread thread, int guest) {
    if (guest < 0 || guest > MaxSignal) return PosixThread::GUEST_EINVAL;
    if (thread == nullptr) return PosixThread::GUEST_EINVAL;
    if (thread == scePthreadSelf()) return guest == 0 || raise_nid_postfix(guest) == 0 ? 0 : *__error_nid_postfix();
    if (!GuestThreadIsLive(thread)) return PosixThread::GUEST_ESRCH;
    if (guest == 0) return 0;
    if (Disposition(guest).handler != 1) MarkPending(guest, GuestSiLwp);
    KqueueSignalSent(guest);
    return 0;
}
int APS5_VABI sigaltstack_nid_postfix(const GuestStack* stack, GuestStack* previous) {
    GuestStack replacement = alternateStack;
    GuestStack requested{};
    if (stack != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&requested, stack, sizeof(requested))) { *__error_nid_postfix() = 14; return -1; }
        stack = &requested;
        if ((stack->flags & ~SsDisable) != 0) { *__error_nid_postfix() = 22; return -1; }
        if (stack->flags & SsDisable) replacement.flags = SsDisable;
        else if (stack->size < MinSignalStackSize) { *__error_nid_postfix() = 12; return -1; }
        else replacement = {stack->sp, stack->size, 0};
    }
    const GuestStack old = alternateStack;
    alternateStack = replacement;
    if (previous != nullptr && !GuestCopyOut_nid_no_patch(previous, &old, sizeof(old))) { *__error_nid_postfix() = 14; return -1; }
    return 0;
}
int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    GuestSignalSet requested{}, old{};
    if (set != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&requested, set, sizeof(requested))) { *__error_nid_postfix() = 14; return -1; }
        set = &requested;
    }
    return ChangeBlockedMask(how, set, previousSet != nullptr ? &old : nullptr, true, nullptr, previousSet);
}

int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet) {
    return _sigprocmask_nid_postfix(how, static_cast<const GuestSignalSet*>(set),
                                    static_cast<GuestSignalSet*>(previousSet));
}

int APS5_VABI pthread_sigmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    const int saved = *__error_nid_postfix();
    const int error = _sigprocmask_nid_postfix(how, set, previousSet) == 0 ? 0 : *__error_nid_postfix();
    *__error_nid_postfix() = saved;
    return error;
}

int APS5_VABI sigtimedwait_nid_postfix(const GuestSignalSet* set, void* info, const KernelTimespec* timeout) {
    if (set == nullptr) { *__error_nid_postfix() = 22; return -1; }
    GuestSignalSet requestedSet{};
    if (!GuestCopyIn_nid_no_patch(&requestedSet, set, sizeof(requestedSet))) { *__error_nid_postfix() = 14; return -1; }
    if (timeout == nullptr && !SetHasSignal(requestedSet)) { *__error_nid_postfix() = 22; return -1; }
    KernelTimespec requestedTimeout{};
    if (timeout != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&requestedTimeout, timeout, sizeof(requestedTimeout))) { *__error_nid_postfix() = 14; return -1; }
        if ((requestedTimeout.tv_nsec < 0 || requestedTimeout.tv_nsec >= 1000000000) && !PendingInSetPresent(requestedSet)) {
            *__error_nid_postfix() = 22;
            return -1;
        }
    }
    int guest = 0;
    QueuedSignal taken{};
    const int result = WaitForPendingInSet(requestedSet, timeout == nullptr, timeout != nullptr ? &requestedTimeout : nullptr, &guest, &taken);
    if (result != 0) { *__error_nid_postfix() = result; return -1; }
    if (info != nullptr && !FillSiginfo(static_cast<GuestSiginfo*>(info), guest, taken)) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    return guest;
}

int APS5_VABI sigwaitinfo_nid_postfix(const GuestSignalSet* set, void* info) {
    if (set == nullptr) { *__error_nid_postfix() = 22; return -1; }
    GuestSignalSet requestedSet{};
    if (!GuestCopyIn_nid_no_patch(&requestedSet, set, sizeof(requestedSet))) { *__error_nid_postfix() = 14; return -1; }
    if (!SetHasSignal(requestedSet)) { *__error_nid_postfix() = 22; return -1; }
    int guest = 0;
    QueuedSignal taken{};
    if (WaitForPendingInSet(requestedSet, true, nullptr, &guest, &taken) != 0) { *__error_nid_postfix() = GuestEagain; return -1; }
    if (info != nullptr && !FillSiginfo(static_cast<GuestSiginfo*>(info), guest, taken)) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    return guest;
}

int APS5_VABI sigwait_nid_postfix(const GuestSignalSet* set, int* received) {
    if (set == nullptr || received == nullptr) return 22;
    GuestSignalSet requestedSet{};
    const int saved = *__error_nid_postfix();
    if (!GuestCopyIn_nid_no_patch(&requestedSet, set, sizeof(requestedSet))) {
        *__error_nid_postfix() = saved;
        return 14;
    }
    if (!SetHasSignal(requestedSet)) {
        *__error_nid_postfix() = saved;
        return 22;
    }
    int guest = 0;
    QueuedSignal taken{};
    const int result = WaitForPendingInSet(requestedSet, true, nullptr, &guest, &taken);
    if (result != 0) {
        *__error_nid_postfix() = saved;
        return result;
    }
    if (!GuestCopyOut_nid_no_patch(received, &guest, sizeof(guest))) {
        *__error_nid_postfix() = saved;
        return 14;
    }
    *__error_nid_postfix() = saved;
    return 0;
}

int APS5_VABI pause_nid_postfix(void) {
    WaitForDeliverableSignal();
    *__error_nid_postfix() = GuestEintr;
    return -1;
}

int APS5_VABI sigsuspend_nid_postfix(const GuestSignalSet* set) {
    GuestSignalSet requested{};
    if (set == nullptr || !GuestCopyIn_nid_no_patch(&requested, set, sizeof(requested))) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    GuestSignalSet previous{{0, 0, 0, 0}};
    GuestSignalSet interrupted{};
    auto& self = CurrentSignals();
    {
        RegisterSignalThread(self);
        std::lock_guard lock(registration);
        previous.bits[0] = self.blocked[0].exchange(requested.bits[0] & ~UnblockableSignals);
        for (int word = 1; word < 4; ++word) previous.bits[word] = self.blocked[word].exchange(requested.bits[word]);
        for (int word = 0; word < 4; ++word) interrupted.bits[word] = previous.bits[word] | HeldWord(self, word);
    }
    std::optional<GuestSignalSet> contextMask;
    try {
        WaitForDeliverableSignal(&interrupted, &contextMask);
    } catch (...) {
        std::lock_guard lock(registration);
        for (int word = 0; word < 4; ++word) self.blocked[word].store(previous.bits[word]);
        throw;
    }
    if (contextMask) sigprocmask_nid_postfix(GuestSigsetmask, &*contextMask, nullptr);
    else ChangeBlockedMask(GuestSigsetmask, &previous, nullptr, false);

    *__error_nid_postfix() = GuestEintr;
    return -1;
}

int APS5_VABI _sigsuspend_nid_postfix(const GuestSignalSet* set) {
    return sigsuspend_nid_postfix(set);
}

constexpr std::uintptr_t GuestSigHold = 3;

int ChangeBlockedSignal(int how, int guest) {
    GuestSignalSet set{};
    set.bits[static_cast<std::size_t>(guest - 1) >> 5] = 1u << ((guest - 1) & 31);
    return ChangeBlockedMask(how, &set, nullptr, true);
}

GuestHandler APS5_VABI sigset_nid_postfix(int guest, GuestHandler disposition) {
    const auto hold = reinterpret_cast<GuestHandler>(GuestSigHold);
    const auto invalid = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    if (guest < 1 || guest > MaxSignal) { *__error_nid_postfix() = 22; return invalid; }
    const bool blocked = SignalBlocked(guest);
    if (disposition == hold) {
        if (blocked) return hold;
        ChangeBlockedSignal(GuestSigblock, guest);
        std::lock_guard lock(registration);
        return reinterpret_cast<GuestHandler>(dispositions[guest].handler);
    }
    GuestSigaction action{reinterpret_cast<std::uintptr_t>(disposition), 0, {}};
    GuestSigaction previous{};
    if (sigaction_nid_postfix(guest, &action, &previous) != 0) return invalid;
    const auto previousHandler = reinterpret_cast<GuestHandler>(previous.handler);
    if (!blocked) return previousHandler;
    ChangeBlockedSignal(GuestSigunblock, guest);
    return hold;
}

int APS5_VABI sigpause_nid_postfix(int mask) {
    GuestSignalSet set{};
    set.bits[0] = static_cast<std::uint32_t>(mask);
    return sigsuspend_nid_postfix(&set);
}

int APS5_VABI sigpending_nid_postfix(GuestSignalSet* set) {
    if (set == nullptr) { *__error_nid_postfix() = 14; return -1; }
    GuestSignalSet result{};
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.lock);
        for (int word = 0; word < 4; ++word)
            result.bits[word] = pending.words[word].load(std::memory_order_relaxed) & BlockedWord(CurrentSignals(), word);
    }
    if (!GuestCopyOut_nid_no_patch(set, &result, sizeof(result))) { *__error_nid_postfix() = 14; return -1; }
    return 0;
}
}

extern "C" {

int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t programCounter) {
    (void)programCounter;
    return 0;
}

}

namespace {

[[maybe_unused]] const bool RegisteredForJumps = [] {
    JumpBuffer::GuestSigprocmaskRegister_nid_no_patch(sigprocmask_nid_postfix);
    return true;
}();
}
