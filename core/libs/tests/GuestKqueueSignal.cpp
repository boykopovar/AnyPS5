#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

using Handler = void (APS5_VABI *)(int);

static constexpr int GuestEnoent = 2;
static constexpr int GuestEsrch = 3;
static constexpr int GuestEbadf = 9;
static constexpr int GuestEinval = 22;
static constexpr int UserSignal = 30;
static constexpr int TermSignal = 15;
static constexpr int AlarmSignal = 14;
static constexpr std::int16_t EvfiltRead = -1;
static constexpr std::int16_t EvfiltSignal = -6;
static constexpr std::uint16_t EvAdd = 0x0001;
static constexpr std::uint16_t EvDelete = 0x0002;
static constexpr std::uint16_t EvEnable = 0x0004;
static constexpr std::uint16_t EvDisable = 0x0008;
static constexpr std::uint16_t EvOneshot = 0x0010;
static constexpr std::uint16_t EvClear = 0x0020;
static constexpr std::uint16_t EvReceipt = 0x0040;
static constexpr std::uint16_t EvDispatch = 0x0080;
static constexpr std::uint16_t EvForceoneshot = 0x0100;
static constexpr std::uint16_t EvError = 0x4000;

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static KernelEvent Change(std::uintptr_t signal, std::uint16_t flags, void* udata = nullptr) {
    KernelEvent change{};
    change.ident = signal;
    change.filter = EvfiltSignal;
    change.flags = flags;
    change.udata = udata;
    return change;
}

static int Apply(int queue, const KernelEvent& change) {
    return kevent_nid_postfix(queue, &change, 1, nullptr, 0, nullptr);
}

static int Poll(int queue, KernelEvent* events, int count = 1) {
    const KernelTimespec zero{0, 0};
    return kevent_nid_postfix(queue, nullptr, 0, events, count, &zero);
}

static KernelEvent Take(int queue, int signal, std::intptr_t count) {
    KernelEvent event{};
    Require(Poll(queue, &event) == 1, "a generated signal was not reported");
    Require(event.ident == static_cast<std::uintptr_t>(signal) && event.filter == EvfiltSignal, "the signal event has the wrong identity");
    Require(event.data == count, "the signal event has the wrong occurrence count");
    Require(event.fflags == 0 && (event.flags & EvClear) != 0, "the signal event was not cleared automatically");
    return event;
}

static void Empty(int queue) {
    KernelEvent event{};
    Require(Poll(queue, &event) == 0, "a signal event was reported twice or before generation");
}

static void Send(int signal, int count = 1) {
    for (int i = 0; i < count; ++i) Require(raise_nid_postfix(signal) == 0, "raising the signal failed");
}

static void RequireFailure(int result, int error) {
    Require(result == -1 && *__error_nid_postfix() == error, "the operation returned the wrong error");
}

class Queue {
public:
    Queue() : descriptor(kqueue_nid_postfix()) {
        Require(descriptor >= 0, "creating the signal queue failed");
    }
    ~Queue() {
        if (descriptor >= 0) Require(close_nid_postfix(descriptor) == 0, "closing the signal queue failed");
    }
    int Get() const { return descriptor; }
    void Close() {
        Require(descriptor >= 0 && close_nid_postfix(descriptor) == 0, "closing the signal queue failed");
        descriptor = -1;
    }
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
private:
    int descriptor;
};

class Disposition {
public:
    Disposition(int signal, Handler handler) : signal(signal) {
        const GuestSigaction action{reinterpret_cast<std::uintptr_t>(handler), 0, {}};
        Require(sigaction_nid_postfix(signal, &action, &previous) == 0, "setting the signal disposition failed");
    }
    ~Disposition() {
        Require(sigaction_nid_postfix(signal, &previous, nullptr) == 0, "restoring the signal disposition failed");
    }
    Disposition(const Disposition&) = delete;
    Disposition& operator=(const Disposition&) = delete;
private:
    int signal;
    GuestSigaction previous{};
};

static Handler Ignore() {
    return reinterpret_cast<Handler>(std::uintptr_t{1});
}

static GuestSignalSet SignalSet(int signal) {
    GuestSignalSet set{};
    Require(sigemptyset_nid_postfix(&set) == 0 && sigaddset_nid_postfix(&set, signal) == 0, "creating the signal set failed");
    return set;
}

class Blocked {
public:
    explicit Blocked(int signal) : set(SignalSet(signal)) {
        Require(sigprocmask_nid_postfix(1, &set, &previous) == 0, "blocking the signal failed");
    }
    ~Blocked() {
        Require(sigprocmask_nid_postfix(3, &previous, nullptr) == 0, "restoring the signal mask failed");
    }
    void Unblock() {
        Require(sigprocmask_nid_postfix(2, &set, nullptr) == 0, "unblocking the signal failed");
    }
    const GuestSignalSet& Set() const { return set; }
    Blocked(const Blocked&) = delete;
    Blocked& operator=(const Blocked&) = delete;
private:
    GuestSignalSet set;
    GuestSignalSet previous{};
};

static bool Pending(int signal) {
    GuestSignalSet set{};
    Require(sigpending_nid_postfix(&set) == 0, "reading pending signals failed");
    const unsigned bit = static_cast<unsigned>(signal - 1);
    return (set.bits[bit / 32] & (1u << (bit % 32))) != 0;
}

static volatile std::sig_atomic_t callbacks = 0;
static void APS5_VABI Callback(int) {
    callbacks = callbacks + 1;
}

static void IgnoredAndIndependentQueues() {
    Disposition disposition(UserSignal, Ignore());
    Send(UserSignal, 3);
    Queue first;
    Queue second;
    int marker = 0;
    auto change = Change(UserSignal, EvAdd, &marker);
    change.fflags = 0xffffffffu;
    change.data = 123;
    Require(Apply(first.Get(), change) == 0 && Apply(second.Get(), Change(UserSignal, EvAdd)) == 0, "registering EVFILT_SIGNAL failed");
    Empty(first.Get());
    Empty(second.Get());
    Send(UserSignal, 3);
    Require(Take(first.Get(), UserSignal, 3).udata == &marker, "the signal event lost udata");
    Empty(first.Get());
    Take(second.Get(), UserSignal, 3);
    Empty(second.Get());
    Send(UserSignal);
    Take(first.Get(), UserSignal, 1);
    Take(second.Get(), UserSignal, 1);
    for (int signal : {40, 73, 128}) {
        Disposition highDisposition(signal, Ignore());
        Require(Apply(first.Get(), Change(signal, EvAdd)) == 0, "registering a high signal failed");
        Send(signal, 2);
        Take(first.Get(), signal, 2);
        Require(Apply(first.Get(), Change(signal, EvDelete)) == 0, "deleting the high signal failed");
    }
}

static void BlockedSignalsKeepTheirPendingState() {
    Disposition disposition(UserSignal, Callback);
    Blocked blocked(UserSignal);
    callbacks = 0;
    Send(UserSignal, 2);
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the blocked signal failed");
    Empty(queue.Get());
    Require(Pending(UserSignal), "registration lost an older pending signal");
    int received = 0;
    Require(sigwait_nid_postfix(&blocked.Set(), &received) == 0 && received == UserSignal, "consuming a preregistration signal failed");
    Send(UserSignal, 3);
    Require(Pending(UserSignal) && callbacks == 0, "a blocked signal was delivered or lost");
    Take(queue.Get(), UserSignal, 3);
    Require(Pending(UserSignal), "reading the signal event consumed the pending signal");
    Require(sigwait_nid_postfix(&blocked.Set(), &received) == 0 && received == UserSignal, "sigwait did not consume the pending signal");
    Require(!Pending(UserSignal) && callbacks == 0, "sigwait delivered the signal handler");
    Empty(queue.Get());
    Send(UserSignal, 2);
    Take(queue.Get(), UserSignal, 2);
    blocked.Unblock();
    Require(callbacks == 1, "unblocking did not deliver the coalesced pending signal once");
    Empty(queue.Get());
}

static void EnableDispatchAndOneshot() {
    Disposition disposition(UserSignal, Ignore());
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd | EvDisable)) == 0, "adding a disabled signal failed");
    Send(UserSignal, 2);
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvEnable)) == 0, "enabling the signal failed");
    Take(queue.Get(), UserSignal, 2);
    Require(Apply(queue.Get(), Change(UserSignal, EvDisable)) == 0, "disabling the signal failed");
    Send(UserSignal);
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvEnable)) == 0, "reenabling the signal failed");
    Take(queue.Get(), UserSignal, 1);
    Require(Apply(queue.Get(), Change(UserSignal, EvDelete)) == 0, "deleting the enabled signal failed");

    int firstMarker = 0;
    int secondMarker = 0;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd | EvDispatch, &firstMarker)) == 0, "adding a dispatch signal failed");
    Send(UserSignal, 2);
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd | EvOneshot, &secondMarker)) == 0, "adding the same signal again failed");
    const auto event = Take(queue.Get(), UserSignal, 2);
    Require(event.udata == &secondMarker && (event.flags & EvDispatch) != 0 && (event.flags & EvOneshot) == 0, "repeated EV_ADD reset the count, flags or udata");
    Send(UserSignal, 3);
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "readding a dispatch signal failed");
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvEnable)) == 0, "reenabling dispatch failed");
    Take(queue.Get(), UserSignal, 3);
    Require(Apply(queue.Get(), Change(UserSignal, EvEnable)) == 0, "enabling dispatch after delivery failed");
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvDelete)) == 0, "deleting dispatch failed");

    Require(Apply(queue.Get(), Change(UserSignal, EvAdd | EvOneshot)) == 0, "adding a oneshot signal failed");
    Send(UserSignal, 2);
    Require((Take(queue.Get(), UserSignal, 2).flags & EvOneshot) != 0, "oneshot was not preserved");
    Send(UserSignal);
    Empty(queue.Get());
    RequireFailure(Apply(queue.Get(), Change(UserSignal, EvDelete)), GuestEnoent);
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "readding a consumed oneshot failed");
    Empty(queue.Get());
    Send(UserSignal);
    Take(queue.Get(), UserSignal, 1);
    Send(UserSignal);
    Require(Apply(queue.Get(), Change(UserSignal, EvDelete)) == 0, "deleting a pending signal event failed");
    Send(UserSignal, 2);
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "recreating the signal event failed");
    Empty(queue.Get());
    Send(UserSignal);
    Take(queue.Get(), UserSignal, 1);
}

static void MultipleSignalsAndPeeking() {
    Disposition firstDisposition(UserSignal, Ignore());
    Disposition secondDisposition(UserSignal + 1, Ignore());
    Queue child;
    Queue parent;
    Require(Apply(child.Get(), Change(UserSignal, EvAdd)) == 0 && Apply(child.Get(), Change(UserSignal + 1, EvAdd)) == 0, "registering two signals failed");
    Send(UserSignal, 2);
    Send(UserSignal + 1, 3);
    KernelEvent first{};
    KernelEvent second{};
    Require(Poll(child.Get(), &first) == 1 && Poll(child.Get(), &second) == 1, "a one-event output buffer lost a signal");
    Require(first.ident != second.ident && first.filter == EvfiltSignal && second.filter == EvfiltSignal, "the output buffer repeated a signal");
    Require((first.ident == UserSignal && first.data == 2 && second.ident == UserSignal + 1 && second.data == 3)
        || (second.ident == UserSignal && second.data == 2 && first.ident == UserSignal + 1 && first.data == 3), "a signal left outside the output buffer lost its count");
    Empty(child.Get());
    KernelEvent watch{};
    watch.ident = static_cast<std::uintptr_t>(child.Get());
    watch.filter = EvfiltRead;
    watch.flags = EvAdd;
    Require(Apply(parent.Get(), watch) == 0, "watching two signal registrations failed");
    Send(UserSignal, 2);
    for (int i = 0; i < 2; ++i) {
        KernelEvent event{};
        Require(Poll(parent.Get(), &event) == 1 && event.ident == watch.ident && event.filter == EvfiltRead && event.data == 1, "peeking did not preserve the child signal event");
    }
    Send(UserSignal);
    Take(child.Get(), UserSignal, 3);
    Empty(child.Get());
    Empty(parent.Get());
}

static void ForcedOneshot() {
    Disposition disposition(UserSignal, Ignore());
    Queue queue;
    for (int pending : {0, 2}) {
        Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering a forced oneshot failed");
        Send(UserSignal, pending);
        Require(Apply(queue.Get(), Change(UserSignal, EvForceoneshot)) == 0, "forcing the signal oneshot failed");
        Require((Take(queue.Get(), UserSignal, pending).flags & EvOneshot) != 0, "forcing did not preserve oneshot");
        RequireFailure(Apply(queue.Get(), Change(UserSignal, EvDelete)), GuestEnoent);
    }
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd | EvDisable)) == 0, "registering a disabled forced signal failed");
    Require(Apply(queue.Get(), Change(UserSignal, EvForceoneshot)) == 0, "forcing a disabled signal failed");
    Empty(queue.Get());
    Require(Apply(queue.Get(), Change(UserSignal, EvEnable)) == 0, "enabling a forced signal failed");
    Take(queue.Get(), UserSignal, 0);
    RequireFailure(Apply(queue.Get(), Change(UserSignal, EvDelete)), GuestEnoent);
}

static void ClosingDescriptorKeepsSignalRegistration() {
    int descriptors[2]{};
    Require(pipe_nid_postfix(descriptors) == 0, "creating a numbered descriptor failed");
    const auto usable = [](int value) { return value >= 1 && value <= 128 && value != 9 && value != 17; };
    const int signal = usable(descriptors[0]) ? descriptors[0] : descriptors[1];
    if (usable(signal)) {
        Disposition disposition(signal, Ignore());
        Queue queue;
        Require(Apply(queue.Get(), Change(signal, EvAdd)) == 0, "registering a descriptor-numbered signal failed");
        Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0, "closing the numbered descriptors failed");
        Send(signal);
        Take(queue.Get(), signal, 1);
    } else {
        Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0, "closing descriptors outside the signal range failed");
        std::puts("descriptor signal collision skipped: both pipe descriptors are outside the catchable signal range");
    }
}

static void InvalidChangesAndSignalProbes() {
    Disposition disposition(UserSignal, Ignore());
    Queue queue;
    KernelEvent receipt{};
    const KernelTimespec zero{0, 0};
    for (const auto signal : {std::uintptr_t{0}, std::uintptr_t{129}, std::uintptr_t{0x10000001e}, std::numeric_limits<std::uintptr_t>::max()}) {
        Require(Apply(queue.Get(), Change(signal, EvAdd)) == 0, "registering an inert signal identifier failed");
        const auto change = Change(signal, EvAdd | EvReceipt);
        Require(kevent_nid_postfix(queue.Get(), &change, 1, &receipt, 1, &zero) == 1, "an inert signal did not return a receipt");
        Require(receipt.ident == signal && receipt.filter == EvfiltSignal && (receipt.flags & EvError) != 0 && receipt.data == 0, "the inert signal receipt was wrong");
    }
    RequireFailure(Apply(queue.Get(), Change(UserSignal, 0)), GuestEnoent);
    RequireFailure(Apply(queue.Get(), Change(UserSignal, EvDelete)), GuestEnoent);
    RequireFailure(Apply(queue.Get(), Change(UserSignal, EvAdd | EvEnable | EvDisable)), GuestEinval);
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the probed signal failed");
    Require(raise_nid_postfix(0) == 0 && kill_nid_postfix(getpid_nid_postfix(), 0) == 0 && pthread_kill_nid_postfix(scePthreadSelf(), 0) == 0, "signal zero was not a successful probe");
    RequireFailure(raise_nid_postfix(-1), GuestEinval);
    RequireFailure(raise_nid_postfix(129), GuestEinval);
    RequireFailure(kill_nid_postfix(std::numeric_limits<int>::max(), UserSignal), GuestEsrch);
    Require(pthread_kill_nid_postfix(nullptr, UserSignal) == GuestEsrch, "a null thread did not fail with ESRCH");
    Require(pthread_kill_nid_postfix(reinterpret_cast<Pthread>(std::uintptr_t{0x10}), UserSignal) == GuestEsrch, "an unknown thread did not fail with ESRCH");
    Empty(queue.Get());
    const int pid = getpid_nid_postfix();
    Require(kill_nid_postfix(pid, UserSignal) == 0 && kill_nid_postfix(0, UserSignal) == 0 && killpg_nid_postfix(pid, UserSignal) == 0, "sending a signal to the current process or group failed");
    Require(pthread_kill_nid_postfix(scePthreadSelf(), UserSignal) == 0, "sending a signal to the current thread failed");
    Take(queue.Get(), UserSignal, 4);
    Empty(queue.Get());
}

static volatile std::sig_atomic_t nativeCallbacks = 0;
static volatile std::sig_atomic_t nestedRemaining = 0;
static volatile std::sig_atomic_t nestedResult = 0;

static void APS5_VABI NativeCallback(int) {
    nativeCallbacks = nativeCallbacks + 1;
    if (raise_nid_postfix(UserSignal) != 0) nestedResult = -1;
    if (nestedRemaining != 0) {
        nestedRemaining = nestedRemaining - 1;
        if (raise_nid_postfix(TermSignal) != 0) nestedResult = -1;
    }
}

static void NativeSignalsAndNestedRaises() {
    Disposition userDisposition(UserSignal, Ignore());
    Disposition termDisposition(TermSignal, NativeCallback);
    Queue termQueue;
    Queue userQueue;
    Require(Apply(termQueue.Get(), Change(TermSignal, EvAdd)) == 0 && Apply(userQueue.Get(), Change(UserSignal, EvAdd)) == 0, "registering native signal observers failed");
    nativeCallbacks = 0;
    nestedRemaining = 1;
    nestedResult = 0;
    Send(TermSignal);
    Require(nativeCallbacks == 2 && nestedResult == 0, "the nested native handler was not delivered twice");
    Take(termQueue.Get(), TermSignal, 2);
    Take(userQueue.Get(), UserSignal, 2);
    Empty(termQueue.Get());
    Require(std::raise(SIGTERM) == 0, "raising a native host signal failed");
    Require(nativeCallbacks == 3 && nestedResult == 0, "the host signal did not reach the guest handler");
    Empty(termQueue.Get());
    Take(userQueue.Get(), UserSignal, 1);
    Blocked blocked(TermSignal);
    Send(TermSignal, 2);
    Require(Pending(TermSignal) && nativeCallbacks == 3, "the blocked native signal was delivered");
    Take(termQueue.Get(), TermSignal, 2);
    nestedRemaining = 1;
    blocked.Unblock();
    Require(nativeCallbacks == 5 && nestedResult == 0, "pending replay lost a nested native raise");
    Take(termQueue.Get(), TermSignal, 1);
    Take(userQueue.Get(), UserSignal, 2);
    Empty(termQueue.Get());
    Empty(userQueue.Get());
}

struct LiveThread {
    std::atomic<bool> release{false};
};

static void* APS5_VABI LiveEntry(void* argument) {
    auto& state = *static_cast<LiveThread*>(argument);
    while (!state.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return nullptr;
}

static void RemoteThreadSignals() {
    Disposition disposition(UserSignal, Callback);
    Blocked blocked(UserSignal);
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the remote thread signal failed");
    LiveThread state;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, LiveEntry, &state, nullptr) == 0, "creating the target thread failed");
    Require(pthread_kill_nid_postfix(thread, 0) == 0, "probing a live thread failed");
    Empty(queue.Get());
    Require(pthread_kill_nid_postfix(thread, UserSignal) == 0, "sending a remote signal failed");
    Take(queue.Get(), UserSignal, 1);
    Require(Pending(UserSignal), "observing the remote signal consumed pending state");
    int received = 0;
    Require(sigwait_nid_postfix(&blocked.Set(), &received) == 0 && received == UserSignal, "consuming the remote pending signal failed");
    {
        Disposition ignored(UserSignal, Ignore());
        Require(pthread_kill_nid_postfix(thread, UserSignal) == 0, "sending an ignored remote signal failed");
        Take(queue.Get(), UserSignal, 1);
        Require(!Pending(UserSignal), "an ignored remote signal was made pending");
    }
    state.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == 0, "joining the target thread failed");
    Require(pthread_kill_nid_postfix(thread, UserSignal) == GuestEsrch, "a dead thread accepted a signal");
    Empty(queue.Get());
}

static void IgnoredAlarm() {
    Disposition disposition(AlarmSignal, Ignore());
    Queue queue;
    Require(Apply(queue.Get(), Change(AlarmSignal, EvAdd)) == 0, "registering SIGALRM failed");
    GuestItimerval previous{};
    const GuestItimerval once{{0, 0}, {0, 20000}};
    Require(setitimer_nid_postfix(0, &once, &previous) == 0, "arming the ignored alarm failed");
    KernelEvent event{};
    const KernelTimespec second{1, 0};
    Require(kevent_nid_postfix(queue.Get(), nullptr, 0, &event, 1, &second) == 1, "the ignored alarm did not wake the queue");
    Require(event.ident == AlarmSignal && event.filter == EvfiltSignal && event.data == 1, "the ignored alarm event was wrong");
    Require(setitimer_nid_postfix(0, &previous, nullptr) == 0, "restoring the timer failed");
    Empty(queue.Get());
}

static bool Until(const std::atomic<bool>& ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ready.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return ready.load();
}

#ifdef _WIN32
struct PausedSignalCopy {
    void* page = nullptr;
    std::size_t bytes = 0;
    std::atomic<bool> trapped{false};
    std::atomic<bool> resume{false};
    std::atomic<bool> timedOut{false};
};

static PausedSignalCopy* pausedSignalCopy = nullptr;

static LONG CALLBACK PauseSignalCopy(EXCEPTION_POINTERS* exception) {
    const auto& record = *exception->ExceptionRecord;
    auto* pause = pausedSignalCopy;
    if (pause == nullptr || record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record.NumberParameters < 2 || record.ExceptionInformation[0] != 1) return EXCEPTION_CONTINUE_SEARCH;
    const auto address = static_cast<std::uintptr_t>(record.ExceptionInformation[1]);
    const auto begin = reinterpret_cast<std::uintptr_t>(pause->page);
    if (address < begin || address - begin >= pause->bytes) return EXCEPTION_CONTINUE_SEARCH;
    pause->trapped.store(true);
    const auto deadline = GetTickCount64() + 5000;
    while (!pause->resume.load() && GetTickCount64() < deadline) Sleep(1);
    if (!pause->resume.load()) pause->timedOut.store(true);
    DWORD previous = 0;
    return VirtualProtect(pause->page, pause->bytes, PAGE_READWRITE, &previous) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

static void* APS5_VABI RemotePendingOrderEntry(void*) {
    Disposition disposition(UserSignal, Callback);
    Blocked blocked(UserSignal);
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the pending-order signal failed");
    PausedSignalCopy pause;
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    pause.bytes = system.dwPageSize;
    pause.page = VirtualAlloc(nullptr, pause.bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
    Require(pause.page != nullptr, "allocating the delayed sigaction output failed");
    pausedSignalCopy = &pause;
    void* handler = AddVectoredExceptionHandler(1, PauseSignalCopy);
    Require(handler != nullptr, "installing the sigaction page-fault handler failed");
    std::atomic<int> inspected{-2};
    std::thread inspector([&] { inspected.store(sigaction_nid_postfix(UserSignal, nullptr, static_cast<GuestSigaction*>(pause.page))); });
    const bool trapped = Until(pause.trapped);
    const Pthread target = scePthreadSelf();
    std::atomic<int> sent{-2};
    std::thread producer;
    KernelEvent early{};
    const KernelTimespec second{1, 0};
    const KernelTimespec zero{0, 0};
    int observed = -2;
    int before = -2;
    int beforeError = 0;
    if (trapped) {
        producer = std::thread([&] { sent.store(pthread_kill_nid_postfix(target, UserSignal)); });
        observed = kevent_nid_postfix(queue.Get(), nullptr, 0, &early, 1, &second);
        before = sigtimedwait_nid_postfix(&blocked.Set(), nullptr, &zero);
        beforeError = *__error_nid_postfix();
    }
    pause.resume.store(true);
    if (!trapped) {
        DWORD previous = 0;
        Require(VirtualProtect(pause.page, pause.bytes, PAGE_READWRITE, &previous) != 0, "releasing the sigaction output failed");
    }
    inspector.join();
    if (producer.joinable()) producer.join();
    KernelEvent after{};
    const int following = Poll(queue.Get(), &after);
    const int received = sigtimedwait_nid_postfix(&blocked.Set(), nullptr, &zero);
    Require(RemoveVectoredExceptionHandler(handler) != 0 && VirtualFree(pause.page, 0, MEM_RELEASE) != 0, "releasing the pending-order fixture failed");
    pausedSignalCopy = nullptr;
    Require(trapped && !pause.timedOut.load() && inspected.load() == 0, "the signal disposition copy was not paused safely");
    std::fprintf(stderr, "remote signal publication: before kevent=%d pending=%d errno=%d; after kevent=%d pending=%d\n", observed, before, beforeError, following, received);
    Require(observed == 0 && before == -1 && beforeError == 35, "the signal event was visible before its pending signal existed");
    Require(sent.load() == 0 && following == 1 && after.ident == UserSignal && after.filter == EvfiltSignal && after.data == 1 && received == UserSignal, "resuming signal generation did not publish both notification and pending state");
    Empty(queue.Get());
    return nullptr;
}

static void RemoteNotificationFollowsPendingState() {
    Pthread observer = nullptr;
    Require(scePthreadCreate(&observer, nullptr, RemotePendingOrderEntry, nullptr, nullptr) == 0, "creating the signal target observer failed");
    Require(scePthreadJoin(observer, nullptr) == 0, "joining the signal target observer failed");
}
#endif

static void IndefiniteAndNestedWaits() {
    Disposition disposition(UserSignal, Ignore());
    Queue child;
    Queue parent;
    Require(Apply(child.Get(), Change(UserSignal, EvAdd)) == 0, "registering the waiting signal failed");
    KernelEvent watch{};
    watch.ident = static_cast<std::uintptr_t>(child.Get());
    watch.filter = EvfiltRead;
    watch.flags = EvAdd;
    Require(Apply(parent.Get(), watch) == 0, "watching the signal queue failed");
    for (const bool nested : {false, true}) {
        std::atomic<bool> entered{false};
        std::atomic<bool> done{false};
        int result = -2;
        KernelEvent event{};
        const int descriptor = nested ? parent.Get() : child.Get();
        std::thread waiter([&] {
            entered.store(true);
            result = kevent_nid_postfix(descriptor, nullptr, 0, &event, 1, nullptr);
            done.store(true);
        });
        Require(Until(entered), "the queue waiter did not start");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(!done.load(), "an empty queue completed an indefinite wait");
        Send(UserSignal);
        const bool woke = Until(done);
        if (!woke) {
            if (nested) parent.Close();
            else child.Close();
        }
        waiter.join();
        Require(woke && result == 1, "the signal did not wake the indefinite waiter");
        if (nested) {
            Require(event.ident == static_cast<std::uintptr_t>(child.Get()) && event.filter == EvfiltRead && event.data == 1, "the parent queue did not see one ready signal event");
            Take(child.Get(), UserSignal, 1);
        } else {
            Require(event.ident == UserSignal && event.filter == EvfiltSignal && event.data == 1, "the direct waiter returned the wrong signal");
        }
        Empty(child.Get());
        Empty(parent.Get());
    }
}

static void ConcurrentDeletionAndClose() {
    Disposition disposition(UserSignal, Ignore());
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the concurrent signal failed");
    std::atomic<bool> start{false};
    std::atomic<bool> sent{true};
    std::thread producer([&] {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 1000; ++i) {
            if (raise_nid_postfix(UserSignal) != 0) sent.store(false);
            if (i % 8 == 0) std::this_thread::yield();
        }
    });
    start.store(true);
    for (int i = 0; i < 40; ++i) {
        Require(Apply(queue.Get(), Change(UserSignal, EvDelete)) == 0, "deleting a signal while it was generated failed");
        Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "recreating a signal while it was generated failed");
        KernelEvent event{};
        const int count = Poll(queue.Get(), &event);
        Require(count == 0 || (count == 1 && event.data > 0), "concurrent signal generation returned an invalid event");
    }
    queue.Close();
    producer.join();
    Require(sent.load(), "a concurrent signal raise failed");
    Queue recreated;
    Require(Apply(recreated.Get(), Change(UserSignal, EvAdd)) == 0, "registering after queue close failed");
    Empty(recreated.Get());
    Send(UserSignal);
    Take(recreated.Get(), UserSignal, 1);

    std::atomic<bool> entered{false};
    std::atomic<bool> done{false};
    int result = 0;
    int error = 0;
    const int descriptor = recreated.Get();
    std::thread waiter([&] {
        KernelEvent event{};
        entered.store(true);
        result = kevent_nid_postfix(descriptor, nullptr, 0, &event, 1, nullptr);
        error = *__error_nid_postfix();
        done.store(true);
    });
    Require(Until(entered), "the closing queue waiter did not start");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    recreated.Close();
    const bool woke = Until(done);
    Require(woke, "closing a signal queue did not wake its waiter");
    waiter.join();
    Require(result == -1 && error == GuestEbadf, "closing the signal queue returned the wrong waiting error");
}

static void APS5_VABI NoopCallback(int) {}

static void ConcurrentCountsAreNeitherLostNorRepeated() {
    Disposition disposition(UserSignal, Ignore());
    Queue queue;
    Require(Apply(queue.Get(), Change(UserSignal, EvAdd)) == 0, "registering the counted concurrent signal failed");
    constexpr std::intptr_t count = 2000;
    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<bool> success{true};
    std::thread producer([&] {
        while (!start.load()) std::this_thread::yield();
        for (std::intptr_t i = 0; i < count; ++i) {
            if (raise_nid_postfix(UserSignal) != 0) success.store(false);
            if (i % 8 == 0) std::this_thread::yield();
        }
        done.store(true);
    });
    std::thread toggler([&] {
        const auto invalid = reinterpret_cast<Handler>(std::numeric_limits<std::uintptr_t>::max());
        while (!start.load()) std::this_thread::yield();
        while (!done.load()) {
            if (signal_nid_postfix(UserSignal, NoopCallback) == invalid || signal_nid_postfix(UserSignal, Ignore()) == invalid) success.store(false);
            std::this_thread::yield();
        }
    });
    start.store(true);
    std::intptr_t total = 0;
    for (;;) {
        const bool finished = done.load();
        KernelEvent event{};
        const int result = Poll(queue.Get(), &event);
        Require(result == 0 || (result == 1 && event.ident == UserSignal && event.filter == EvfiltSignal && event.data > 0), "polling concurrent signals returned an invalid event");
        if (result == 1) total += event.data;
        Require(total <= count, "concurrent signal polling repeated an occurrence");
        if (finished && result == 0) break;
        std::this_thread::yield();
    }
    producer.join();
    toggler.join();
    Require(success.load() && total == count, "concurrent signal generation or disposition changes lost an occurrence");
    Empty(queue.Get());
}

int main() {
    IgnoredAndIndependentQueues();
    BlockedSignalsKeepTheirPendingState();
    EnableDispatchAndOneshot();
    MultipleSignalsAndPeeking();
    ForcedOneshot();
    ClosingDescriptorKeepsSignalRegistration();
    InvalidChangesAndSignalProbes();
    NativeSignalsAndNestedRaises();
    RemoteThreadSignals();
#ifdef _WIN32
    RemoteNotificationFollowsPendingState();
#endif
    IgnoredAlarm();
    IndefiniteAndNestedWaits();
    ConcurrentDeletionAndClose();
    ConcurrentCountsAreNeitherLostNorRepeated();
    std::puts("signal kqueue tests passed");
}
