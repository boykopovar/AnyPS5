#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef __APPLE__
#include <new>
#include <typeinfo>
#endif

static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);

extern "C" GuestSignalSet _sigintr_nid_postfix{};

namespace {

constexpr int MaxSignal = 128;
constexpr int GuestSigkill = 9;
constexpr int GuestSigstop = 17;
constexpr std::uint32_t UnblockableSignals = (1u << (GuestSigkill - 1)) | (1u << (GuestSigstop - 1));
constexpr int GuestSigalrm = 14;
constexpr int GuestSigcont = 19;
constexpr int SaRestart = 0x2;
constexpr int SaResethand = 0x4;
constexpr int SaNodefer = 0x10;
constexpr int SaSiginfo = 0x40;
constexpr int SaDefinedFlags = 0x7f;
std::atomic<GuestHandler> handlers[MaxSignal + 1]{};
std::atomic<std::uint32_t> handlerMasks[MaxSignal + 1][4]{};
std::atomic<int> handlerFlags[MaxSignal + 1]{};
std::atomic<bool> handlerReset[MaxSignal + 1]{};
static_assert(std::atomic<GuestHandler>::is_always_lock_free);
std::atomic<std::uint32_t> blockedMask{0};
std::atomic<std::uint32_t> blockedMaskExtra[3]{};
std::atomic<std::uint64_t> handlerHolds[MaxSignal + 1]{};
constexpr std::uint64_t HoldCount = 0xffffffffu;
thread_local std::uint32_t holdReleases[MaxSignal + 1]{};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
std::mutex registration;
GuestSigaction dispositions[MaxSignal + 1]{};
constexpr int GuestSigblock = 1;
constexpr int GuestSigunblock = 2;
constexpr int GuestSigsetmask = 3;
constexpr int GuestEintr = 4;
constexpr int GuestEagain = 35;
constexpr int GuestSiuser = 0;
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
static_assert(sizeof(GuestSiginfo) == 80 && offsetof(GuestSiginfo, code) == 8 && offsetof(GuestSiginfo, pid) == 12 &&
    offsetof(GuestSiginfo, uid) == 16 && offsetof(GuestSiginfo, address) == 24 && offsetof(GuestSiginfo, value) == 32 &&
    offsetof(GuestSiginfo, band) == 40);

static_assert(sizeof(KernelTimespec) == 16 && offsetof(KernelTimespec, tv_nsec) == 8);
bool IgnoredByDefault(int guest) {
    return guest == 16 || guest == GuestSigcont || guest == 20 || guest == 23 || guest == 28 || guest == 29;
}
std::uint32_t SignalBit(int guest) { return 1u << ((guest - 1) % 32); }
std::uint32_t SignalWord(int guest) { return static_cast<std::uint32_t>((guest - 1) / 32); }
bool SignalInSet(const GuestSignalSet& set, int guest) { return (set.bits[SignalWord(guest)] & SignalBit(guest)) != 0; }
bool SetHasSignal(const GuestSignalSet& set) { return (set.bits[0] | set.bits[1] | set.bits[2] | set.bits[3]) != 0; }
bool HeldByHandler(int guest) { return (handlerHolds[guest].load() & HoldCount) != 0; }
void ReleaseHold(int guest, std::uint32_t epoch) {
    auto value = handlerHolds[guest].load();
    while ((value >> 32) == epoch && (value & HoldCount) != 0 && !handlerHolds[guest].compare_exchange_weak(value, value - 1)) {}
}
void ClearHold(int guest) {
    auto value = handlerHolds[guest].load();
    while ((value & HoldCount) != 0 && !handlerHolds[guest].compare_exchange_weak(value, ((value >> 32) + 1) << 32)) {}
}
std::uint32_t HeldWord(int word) {
    std::uint32_t held = 0;
    for (int bit = 0; bit < 32; ++bit)
        if (HeldByHandler(word * 32 + bit + 1)) held |= 1u << bit;
    return held;
}
std::uint32_t BlockedWord(int word) {
    return (word == 0 ? blockedMask.load() : blockedMaskExtra[word - 1].load()) | HeldWord(word);
}
bool SignalBlocked(int guest) {
    if (guest < 1 || guest > MaxSignal) return false;
    const auto word = SignalWord(guest);
    const auto mask = word == 0 ? blockedMask.load() : blockedMaskExtra[word - 1].load();
    return (mask & SignalBit(guest)) != 0 || HeldByHandler(guest);
}

struct PendingSignals {
    std::mutex lock;
    TimedWait::Condition changed;
    std::atomic<std::uint32_t> words[4]{};
};

PendingSignals& Pending() {
    static auto* state = new PendingSignals();
    return *state;
}

void MarkPending(int guest) {
    auto& pending = Pending();
    {
        // under the same lock the waits check the set with: without it a signal raised
        // between the check and the wait is lost and the wait never returns
        std::lock_guard lock(pending.lock);
        pending.words[SignalWord(guest)].fetch_or(SignalBit(guest), std::memory_order_relaxed);
    }
    pending.changed.NotifyAll();
}

bool TakePending(int guest) {
    auto& pending = Pending();
    auto& word = pending.words[SignalWord(guest)];
    const std::uint32_t bit = SignalBit(guest);
    if ((word.load(std::memory_order_relaxed) & bit) == 0) return false;
    return (word.fetch_and(~bit, std::memory_order_relaxed) & bit) != 0;
}

void DiscardPendingIfIgnored(int guest, const GuestSigaction& action) {
    if (action.handler == 1 || (action.handler == 0 && IgnoredByDefault(guest))) TakePending(guest);
}

bool PendingInSet(const GuestSignalSet& set, int* guest) {
    for (int candidate = 1; candidate <= MaxSignal; ++candidate) {
        if (!SignalInSet(set, candidate) || !TakePending(candidate)) continue;
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
        case 15: return SIGTERM;
        default: return 0;
    }
}
void ResetToDefault(int guest) {
    handlerReset[guest].store(true);
    handlers[guest].store(nullptr);
    if (const int native = NativeSignal(guest)) std::signal(native, SIG_DFL);
}
void RunHandler(int guest, GuestHandler handler) {
    const int flags = handlerFlags[guest].load();
    std::uint32_t mask[4];
    for (int word = 0; word < 4; ++word) mask[word] = handlerMasks[guest][word].load();
    if ((flags & SaNodefer) == 0) mask[SignalWord(guest)] |= SignalBit(guest);
    mask[0] &= ~UnblockableSignals;
    if ((flags & SaResethand) != 0) ResetToDefault(guest);
    std::uint32_t before[4];
    for (int word = 0; word < 4; ++word) before[word] = (word == 0 ? blockedMask.load() : blockedMaskExtra[word - 1].load()) & mask[word];
    std::uint32_t epochs[MaxSignal + 1]{};
    std::uint32_t releases[MaxSignal + 1]{};
    for (int held = 1; held <= MaxSignal; ++held) {
        if ((mask[SignalWord(held)] & SignalBit(held)) == 0) continue;
        releases[held] = holdReleases[held];
        epochs[held] = static_cast<std::uint32_t>(handlerHolds[held].fetch_add(1) >> 32);
    }
    handler(guest);
    std::uint32_t replaced[4]{};
    for (int held = 1; held <= MaxSignal; ++held) {
        if ((mask[SignalWord(held)] & SignalBit(held)) == 0) continue;
        if (holdReleases[held] != releases[held]) replaced[SignalWord(held)] |= SignalBit(held);
        ReleaseHold(held, epochs[held]);
    }
    for (int word = 0; word < 4; ++word) {
        if (replaced[word] == 0) continue;
        auto& target = word == 0 ? blockedMask : blockedMaskExtra[word - 1];
        target.fetch_and(~(replaced[word] & ~before[word]));
        target.fetch_or(replaced[word] & before[word]);
    }
}
void Dispatch(int native) {
    int guest = 0;
    for (int candidate : {2, 4, 6, 8, 11, 15})
        if (NativeSignal(candidate) == native) { guest = candidate; break; }
    if (!guest) return;
#ifdef _WIN32
    // Preserve the guest's persistent registration across CRT delivery.
    std::signal(native, Dispatch);
#endif
    if (SignalBlocked(guest)) {
        MarkPending(guest);
        return;
    }
    const auto callback = handlers[guest].load();
    if (reinterpret_cast<std::uintptr_t>(callback) > 1) RunHandler(guest, callback);
}
bool Delivered(const GuestSigaction& action) {
    return action.handler > 1 && (action.flags & SaSiginfo) == 0;
}
bool Install(int guest, const GuestSigaction& action) {
    handlerReset[guest].store(false);
    for (int word = 0; word < 4; ++word) handlerMasks[guest][word].store(action.mask.bits[word]);
    handlerFlags[guest].store(action.flags);
    const int native = NativeSignal(guest);
    if (!native) return true;
    const auto previous = handlers[guest].exchange(Delivered(action) ? reinterpret_cast<GuestHandler>(action.handler) : nullptr);
    auto hostHandler = action.handler == 1 ? SIG_IGN : Delivered(action) ? Dispatch : SIG_DFL;
    if (std::signal(native, hostHandler) == SIG_ERR) {
        handlers[guest].store(previous);
        return false;
    }
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

bool TerminatesWithoutHandler(int guest) {
    return guest == GuestSigalrm && Disposition(guest).handler == 0;
}

[[noreturn]] void TerminateBySigalrm(const char* cause) {
    std::fprintf(stderr, "SIGALRM: %s with the default disposition; the process terminates\n", cause);
    std::fflush(stderr);
    std::_Exit(128 + GuestSigalrm);
}

bool Deliverable(int guest) {
    return NativeSignal(guest) != 0 || CaughtByGuest(guest) || TerminatesWithoutHandler(guest);
}

bool TakePendingUnblocked(int* guest) {
    for (int candidate = 1; candidate <= MaxSignal; ++candidate) {
        if (SignalBlocked(candidate) || !Deliverable(candidate)) continue;
        if (!TakePending(candidate)) continue;
        *guest = candidate;
        return true;
    }
    return false;
}

bool DeliverPendingSignal(int guest) {
    const GuestSigaction action = Disposition(guest);
    if (action.handler == 1) return false;
    if (action.handler > 1 && !Delivered(action)) throw std::runtime_error("signal: SA_SIGINFO handlers are not delivered");
    if (guest == GuestSigalrm && action.handler == 0) TerminateBySigalrm("a pending SIGALRM was unblocked");
    const int native = NativeSignal(guest);
    if (native != 0) {
        if (std::raise(native) != 0) throw std::runtime_error("signal: delivering the signal to the host failed");
        return action.handler > 1;
    }
    if (action.handler > 1) {
        RunHandler(guest, reinterpret_cast<GuestHandler>(action.handler));
        return true;
    }
    return false;
}

bool DeliverUnblockedPending() {
    bool caught = false;
    for (int guest = 1; guest <= MaxSignal; ++guest) {
        if (SignalBlocked(guest) || !Deliverable(guest) || !TakePending(guest)) continue;
        if (DeliverPendingSignal(guest)) caught = true;
    }
    return caught;
}

std::optional<std::uint64_t> SignalWaitDeadline(const KernelTimespec& timeout) {
    if (timeout.tv_sec > INT32_MAX) return std::nullopt;
    const std::uint64_t now = TimedWait::NowNanos();
    if (timeout.tv_sec < 0) return now;
    return now + static_cast<std::uint64_t>(timeout.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(timeout.tv_nsec);
}

int WaitForPendingInSet(const GuestSignalSet& set, bool waitForever, const KernelTimespec* timeout, int* guest) {
    auto& pending = Pending();
    std::unique_lock lock(pending.lock);
    const std::optional<std::uint64_t> deadline = waitForever ? std::nullopt : SignalWaitDeadline(*timeout);
    for (;;) {
        if (PendingInSet(set, guest)) return 0;
        if (!deadline) {
            pending.changed.Wait(lock);
            continue;
        }
        if (pending.changed.WaitUntil(lock, *deadline)) continue;
        return PendingInSet(set, guest) ? 0 : GuestEagain;
    }
}

void FillSiginfo(GuestSiginfo* info, int guest) {
    *info = {};
    info->signo = guest;
    info->code = GuestSiuser;
    info->pid = getpid_nid_postfix();
}

void WaitForDeliverableSignal() {
    auto& pending = Pending();
    for (;;) {
        int guest = 0;
        {
            std::unique_lock lock(pending.lock);
            if (!TakePendingUnblocked(&guest)) {
                pending.changed.Wait(lock);
                continue;
            }
        }
        if (DeliverPendingSignal(guest)) return;
    }
}

}

namespace {
constexpr std::int64_t MaxItimerSeconds = 0x7fffffff / 2;
struct AlarmState {
    std::mutex lock;
    std::condition_variable changed;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::chrono::microseconds interval{0};
    bool threadStarted = false;
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

void ExpireAlarm(std::chrono::microseconds interval) {
    std::uintptr_t handler = 0;
    {
        std::lock_guard lock(registration);
        handler = dispositions[GuestSigalrm].handler;
    }
    if (handler == 1) return;
    if (SignalBlocked(GuestSigalrm)) {
        MarkPending(GuestSigalrm);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (SignalBlocked(GuestSigalrm) || !TakePending(GuestSigalrm)) return;
        handler = Disposition(GuestSigalrm).handler;
        if (handler == 1) return;
    }
    if (handler == 0) TerminateBySigalrm("the alarm expired");
#ifdef __APPLE__
    const std::string message = "ITIMER_REAL (it_interval " + DescribeInterval(interval) + ") expired: delivering SIGALRM to a guest handler is not implemented";
    void* object = __cxa_allocate_exception_nid_postfix(sizeof(std::runtime_error));
    new (object) std::runtime_error(message);
    __cxa_throw_nid_postfix(object, const_cast<std::type_info*>(&typeid(std::runtime_error)), [](void* thrown) { static_cast<std::runtime_error*>(thrown)->~runtime_error(); });
#else
    throw std::runtime_error("ITIMER_REAL (it_interval " + DescribeInterval(interval) + ") expired: delivering SIGALRM to a guest handler is not implemented");
#endif
}

void RunAlarms() {
    auto& alarms = Alarms();
    std::unique_lock lock(alarms.lock);
    for (;;) {
        if (!alarms.deadline) {
            alarms.changed.wait(lock);
            continue;
        }
        const auto deadline = *alarms.deadline;
        if (alarms.changed.wait_until(lock, deadline) != std::cv_status::timeout || alarms.deadline != deadline) continue;
        const auto interval = alarms.interval;
        if (interval.count() == 0) alarms.deadline.reset();
        else {
            const auto now = std::chrono::steady_clock::now();
            const auto periods = now < deadline ? 1 : (now - deadline) / interval + 1;
            alarms.deadline = deadline + periods * interval;
        }
        lock.unlock();
        ExpireAlarm(interval);
        lock.lock();
    }
}

std::chrono::microseconds RemainingAlarm(const AlarmState& alarms, std::chrono::steady_clock::time_point now) {
    if (!alarms.deadline || *alarms.deadline <= now) return std::chrono::microseconds(0);
    return std::chrono::duration_cast<std::chrono::microseconds>(*alarms.deadline - now);
}

void NotifyAlarms(AlarmState& alarms) {
    if (!alarms.threadStarted) {
        std::thread(RunAlarms).detach();
        alarms.threadStarted = true;
    }
    alarms.changed.notify_all();
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
    else alarms.deadline = now + std::chrono::seconds(seconds);
    NotifyAlarms(alarms);
    return remaining;
}

static_assert(sizeof(GuestItimerval) == 32 && offsetof(GuestItimerval, value) == 16);

extern "C" int APS5_VABI getitimer_nid_postfix(int which, GuestItimerval* output) {
    if (which < 0 || which > 2) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    if (output == nullptr) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    if (which != 0) {
        *output = {};
        return 0;
    }
    GuestItimerval result{};
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

namespace {
constexpr int ItimerReal = 0;
constexpr int ItimerProf = 2;
constexpr std::int64_t MinExactIntervalMicros = 1000;

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
}

extern "C" int APS5_VABI setitimer_nid_postfix(int which, const GuestItimerval* value, GuestItimerval* previous) {
    if (which < ItimerReal || which > ItimerProf) { *__error_nid_postfix() = 22; return -1; }
    if (value == nullptr && previous == nullptr) { *__error_nid_postfix() = 14; return -1; }
    GuestItimerval requested{};
    if (value != nullptr) {
        requested = *value;
        if (!ValidItimer(requested.value)) { *__error_nid_postfix() = 22; return -1; }
        if (!ItimerSet(requested.value)) requested.interval = {};
        else if (!ValidItimer(requested.interval)) { *__error_nid_postfix() = 22; return -1; }
    }
    if (which != ItimerReal) {
        if (ItimerSet(requested.value))
            throw std::runtime_error(std::string("setitimer: ") + (which == ItimerProf ? "ITIMER_PROF" : "ITIMER_VIRTUAL") + " needs process CPU time accounting, which is not implemented");
        if (previous != nullptr) *previous = {};
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
            if (ItimerSet(requested.value)) alarms.deadline = now + ItimerDuration(requested.value);
            else alarms.deadline.reset();
            NotifyAlarms(alarms);
        }
    }
    if (previous != nullptr) *previous = old;
    return 0;
}

static_assert(sizeof(GuestStack) == 24 && offsetof(GuestStack, flags) == 16);
namespace {
constexpr int SsDisable = 4;
constexpr std::size_t MinSignalStackSize = 2048;
thread_local GuestStack alternateStack{nullptr, 0, SsDisable};
}

namespace {

int ChangeBlockedMask(int how, const GuestSignalSet* set, GuestSignalSet* previousSet, bool releaseHolds, bool* caught = nullptr) {
    if (set != nullptr && (how < 1 || how > 3)) { *__error_nid_postfix() = 22; return -1; }
    {
        std::lock_guard lock(registration);
        if (previousSet != nullptr)
            for (int word = 0; word < 4; ++word) previousSet->bits[word] = BlockedWord(word);
        if (set != nullptr) {
            switch (how) {
                case 1:
                    blockedMask.fetch_or(set->bits[0] & ~UnblockableSignals);
                    for (int word = 1; word < 4; ++word) blockedMaskExtra[word - 1].fetch_or(set->bits[word]);
                    break;
                case 2:
                    blockedMask.fetch_and(~set->bits[0]);
                    for (int word = 1; word < 4; ++word) blockedMaskExtra[word - 1].fetch_and(~set->bits[word]);
                    if (releaseHolds)
                        for (int guest = 1; guest <= MaxSignal; ++guest) {
                            if (!SignalInSet(*set, guest)) continue;
                            ClearHold(guest);
                            ++holdReleases[guest];
                        }
                    break;
                case 3:
                    blockedMask.store(set->bits[0] & ~UnblockableSignals);
                    for (int word = 1; word < 4; ++word) blockedMaskExtra[word - 1].store(set->bits[word]);
                    if (releaseHolds)
                        for (int guest = 1; guest <= MaxSignal; ++guest) {
                            ClearHold(guest);
                            ++holdReleases[guest];
                        }
                    break;
            }
        }
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (DeliverUnblockedPending() && caught != nullptr) *caught = true;
    return 0;
}

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
    if (previous) *previous = old;
    return 0;
}
int APS5_VABI _sigaction_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return sigaction_nid_postfix(guest, action, previous);
}
int APS5_VABI raise_nid_postfix(int guest) {
    if (guest == 0) return 0;
    if (guest < 1 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    const GuestSigaction action = Disposition(guest);
    if (action.handler > 1 && !Delivered(action)) throw std::runtime_error("raise: SA_SIGINFO handlers are not delivered");
    if (action.handler == 1 || (action.handler == 0 && guest != GuestSigcont && IgnoredByDefault(guest))) return 0;
    if (SignalBlocked(guest)) {
        MarkPending(guest);
        return 0;
    }
    if (action.handler == 0 && IgnoredByDefault(guest)) return 0;
    const int native = NativeSignal(guest);
    if (native != 0) {
        const int result = std::raise(native);
        if (result) { *__error_nid_postfix() = 22; return -1; }
        DeliverUnblockedPending();
        return 0;
    }
    if (!CaughtByGuest(guest)) { *__error_nid_postfix() = 22; return -1; }
    DeliverPendingSignal(guest);
    DeliverUnblockedPending();
    return 0;
}
int APS5_VABI kill_nid_postfix(int pid, int guest) {
    if (guest < 0 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    const int self = getpid_nid_postfix();
    if (pid != self && pid != 0 && pid != -self) { *__error_nid_postfix() = 3; return -1; }
    return guest == 0 ? 0 : raise_nid_postfix(guest);
}
int APS5_VABI killpg_nid_postfix(int pgrp, int guest) {
    if (pgrp < 0) { *__error_nid_postfix() = 22; return -1; }
    return kill_nid_postfix(-pgrp, guest);
}
int APS5_VABI pthread_kill_nid_postfix(Pthread thread, int guest) {
    if (guest < 0 || guest > MaxSignal) return PosixThread::GUEST_EINVAL;
    if (thread == nullptr) return PosixThread::GUEST_ESRCH;
    if (thread == scePthreadSelf()) return guest == 0 || raise_nid_postfix(guest) == 0 ? 0 : *__error_nid_postfix();
    if (!GuestThreadIsLive(thread)) return PosixThread::GUEST_ESRCH;
    if (guest == 0) return 0;
    if (Disposition(guest).handler != 1) MarkPending(guest);
    return 0;
}
int APS5_VABI sigaltstack_nid_postfix(const GuestStack* stack, GuestStack* previous) {
    GuestStack replacement = alternateStack;
    if (stack != nullptr) {
        if ((stack->flags & ~SsDisable) != 0) { *__error_nid_postfix() = 22; return -1; }
        if (stack->flags & SsDisable) replacement.flags = SsDisable;
        else if (stack->size < MinSignalStackSize) { *__error_nid_postfix() = 12; return -1; }
        else replacement = {stack->sp, stack->size, 0};
    }
    if (previous != nullptr) *previous = alternateStack;
    alternateStack = replacement;
    return 0;
}
int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return ChangeBlockedMask(how, set, previousSet, true);
}

int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet) {
    return _sigprocmask_nid_postfix(how, static_cast<const GuestSignalSet*>(set),
                                    static_cast<GuestSignalSet*>(previousSet));
}

int APS5_VABI pthread_sigmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    if (set != nullptr && (how < 1 || how > 3)) return 22;
    return _sigprocmask_nid_postfix(how, set, previousSet);
}

int APS5_VABI sigtimedwait_nid_postfix(const GuestSignalSet* set, void* info, const KernelTimespec* timeout) {
    if (set == nullptr || !SetHasSignal(*set)) { *__error_nid_postfix() = 22; return -1; }
    if (timeout != nullptr && (timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000) && !PendingInSetPresent(*set)) {
        *__error_nid_postfix() = 22;
        return -1;
    }
    int guest = 0;
    const int result = WaitForPendingInSet(*set, timeout == nullptr, timeout, &guest);
    if (result != 0) { *__error_nid_postfix() = result; return -1; }
    if (info != nullptr) FillSiginfo(static_cast<GuestSiginfo*>(info), guest);
    return guest;
}

int APS5_VABI sigwaitinfo_nid_postfix(const GuestSignalSet* set, void* info) {
    if (set == nullptr || !SetHasSignal(*set)) { *__error_nid_postfix() = 22; return -1; }
    int guest = 0;
    if (WaitForPendingInSet(*set, true, nullptr, &guest) != 0) { *__error_nid_postfix() = GuestEagain; return -1; }
    if (info != nullptr) FillSiginfo(static_cast<GuestSiginfo*>(info), guest);
    return guest;
}

int APS5_VABI sigwait_nid_postfix(const GuestSignalSet* set, int* received) {
    if (set == nullptr || received == nullptr || !SetHasSignal(*set)) return 22;
    int guest = 0;
    const int result = WaitForPendingInSet(*set, true, nullptr, &guest);
    if (result != 0) return result;
    *received = guest;
    return 0;
}

int APS5_VABI pause_nid_postfix(void) {
    WaitForDeliverableSignal();
    *__error_nid_postfix() = GuestEintr;
    return -1;
}

int APS5_VABI sigsuspend_nid_postfix(const GuestSignalSet* set) {
    if (set == nullptr) throw std::invalid_argument("sigsuspend: null mask");
    GuestSignalSet previous{{0, 0, 0, 0}};
    {
        std::lock_guard lock(registration);
        previous.bits[0] = blockedMask.exchange(set->bits[0] & ~UnblockableSignals);
        for (int word = 1; word < 4; ++word) previous.bits[word] = blockedMaskExtra[word - 1].exchange(set->bits[word]);
    }
    WaitForDeliverableSignal();
    ChangeBlockedMask(GuestSigsetmask, &previous, nullptr, false);

    *__error_nid_postfix() = GuestEintr;
    return -1;
}

int APS5_VABI _sigsuspend_nid_postfix(const GuestSignalSet* set) {
    return sigsuspend_nid_postfix(set);
}

int APS5_VABI sigpending_nid_postfix(GuestSignalSet* set) {
    if (set == nullptr) { *__error_nid_postfix() = 14; return -1; }
    auto& pending = Pending();
    std::lock_guard lock(pending.lock);
    for (int word = 0; word < 4; ++word)
        set->bits[word] = pending.words[word].load(std::memory_order_relaxed) & BlockedWord(word);
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

struct RegisterSignalsOnLoad {
    RegisterSignalsOnLoad() {
        RegisterSigaction_nid_no_patch(reinterpret_cast<SigactionFn_nid_no_patch>(sigaction_nid_postfix), _sigintr_nid_postfix.bits);
    }
};
const RegisterSignalsOnLoad gRegisterSignals;
}
