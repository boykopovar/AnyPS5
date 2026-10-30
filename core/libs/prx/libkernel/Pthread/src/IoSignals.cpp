#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libc/include/DescriptorAccess.hpp"
#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <memory>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ThreadCancel {

struct IoRestart::State {
    struct Descriptor {
        int number;
        GuestDescriptorAccess::Version version;
    };
    State* previous;
    std::uintptr_t stack;
    std::uint32_t context;
    bool allowed;
    bool ready = false;
    std::vector<Descriptor> expected;
    State(State* previous, std::uintptr_t stack, bool allowed)
        : previous(previous), stack(stack), context(JumpBuffer::GuestContextCurrent_nid_no_patch()), allowed(allowed) {}
    bool Abandoned(std::uintptr_t target, std::uint32_t targetContext) const { return context == targetContext && stack < target; }
};

namespace {

class SignalManager;
std::atomic<SignalManager*> published{nullptr};

class SetupBlock {
public:
    SetupBlock() {
#ifndef _WIN32
        sigset_t signals;
        sigfillset(&signals);
        if (const int error = pthread_sigmask(SIG_BLOCK, &signals, &previous); error != 0)
            throw std::system_error(error, std::generic_category(), "I/O delivery: blocking setup signals");
#endif
    }
    ~SetupBlock() {
#ifndef _WIN32
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#endif
    }
#ifndef _WIN32
    const sigset_t& Previous() const { return previous; }
#endif
private:
#ifndef _WIN32
    sigset_t previous{};
#endif
};

struct SignalState {
    SignalManager* manager;
    Pthread guest;
    std::mutex mutex;
    IoWait* wait = nullptr;
    bool active = true;
    std::atomic<std::uint64_t> signals{0};
    std::atomic<bool> delivery{false};
    std::atomic<bool> restart{true};
    void (*callback)() = nullptr;
    void (*retire)() = nullptr;
    IoRestart::State* replay = nullptr;
    std::vector<IoRestart::State::Descriptor> descriptors;
    std::shared_ptr<SignalState> previousDelivery;
#ifndef _WIN32
    sigset_t deliveryMask{};
    sigset_t suspendedMask{};
    bool suspended = false;
#endif

    bool Pending() const { return signals.load() != 0 || delivery.load(); }
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
constinit thread_local SignalState* current = nullptr;
struct SignalOwnership {
    std::shared_ptr<SignalState> owner;
    std::shared_ptr<SignalState> delivering;
};
SignalOwnership& Ownership() { return HostThreadLocal<SignalOwnership, SignalOwnership>(); }
constinit thread_local IoRestart::State* restarts = nullptr;
constinit thread_local unsigned depth = 0;

class SignalManager {
public:
    std::mutex mutex;
    std::vector<std::shared_ptr<SignalState>> entries;

    SignalManager() {
#ifdef _WIN32
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) throw std::system_error(GetLastError(), std::system_category(), "I/O delivery: creating wake event");
#elif defined(__APPLE__)
        static std::atomic<unsigned> serial{0};
        char name[32];
        do {
            std::snprintf(name, sizeof(name), "/a5io-%x-%x", static_cast<unsigned>(getpid()), serial.fetch_add(1));
            event = sem_open(name, O_CREAT | O_EXCL, 0600, 0);
        } while (event == SEM_FAILED && errno == EEXIST);
        if (event == SEM_FAILED) throw std::system_error(errno, std::generic_category(), "I/O delivery: creating wake semaphore");
        if (sem_unlink(name) != 0) {
            const int error = errno;
            sem_close(event);
            throw std::system_error(error, std::generic_category(), "I/O delivery: unlinking wake semaphore");
        }
#else
        if (sem_init(&event, 0, 0) != 0) throw std::system_error(errno, std::generic_category(), "I/O delivery: creating wake semaphore");
#endif
        try {
            std::thread([this] { Run(); }).detach();
        } catch (...) {
#ifdef _WIN32
            CloseHandle(event);
#elif defined(__APPLE__)
            sem_close(event);
#else
            sem_destroy(&event);
#endif
            throw;
        }
    }

    void Notify() noexcept {
#ifdef _WIN32
        SetEvent(event);
#else
        sem_post(Semaphore());
#endif
    }

private:
#ifdef _WIN32
    HANDLE event;
#elif defined(__APPLE__)
    sem_t* event;
#else
    sem_t event{};
#endif

#ifndef _WIN32
    sem_t* Semaphore() noexcept {
#ifdef __APPLE__
        return event;
#else
        return &event;
#endif
    }
#endif

    void Run() {
        for (;;) {
#ifdef _WIN32
            WaitForSingleObject(event, INFINITE);
#else
            while (sem_wait(Semaphore()) != 0 && errno == EINTR) {}
#endif
            bool pending;
            do {
                pending = false;
                {
                    std::lock_guard registered(mutex);
                    for (const auto& state : entries) {
                        std::lock_guard operation(state->mutex);
                        if (!state->active || !state->Pending()) continue;
                        pending = true;
                        if (state->wait) state->wait->Wake();
                    }
                }
                if (pending) std::this_thread::yield();
            } while (pending);
        }
    }
};

SignalManager& Manager() {
    static auto* manager = [] {
        auto* value = new SignalManager;
        published.store(value, std::memory_order_release);
        return value;
    }();
    return *manager;
}

}

IoRestart::IoRestart(bool allowed) {
    const SetupBlock blocked;
    JumpBuffer::GuestIoJumpRegister_nid_no_patch(RetireIoStack);
    state = new State(restarts, reinterpret_cast<std::uintptr_t>(this), allowed);
    restarts = state;
}

IoRestart::~IoRestart() {
    const int saved = errno;
    const SetupBlock blocked;
    auto** cursor = &restarts;
    while (*cursor && (*cursor != state || (*cursor)->stack != reinterpret_cast<std::uintptr_t>(this))) cursor = &(*cursor)->previous;
    if (*cursor) {
        *cursor = state->previous;
        delete state;
    }
    errno = saved;
}

void IoRestart::Clear() {
    const int saved = errno;
    const SetupBlock blocked;
    state->ready = false;
    state->expected.clear();
    errno = saved;
}

bool IoRestart::Ready() const { return state->ready && IoRestartError(); }

void BeginIoSignals(Pthread thread, IoRestart& restart, std::optional<TimedWait::DeliveryDeferral>& deferral) {
    if (current) {
        if (!deferral) deferral.emplace();
        ++depth;
        return;
    }
    const SetupBlock blocked;
    auto& ownership = Ownership();
    auto& manager = Manager();
    auto state = std::make_shared<SignalState>();
    state->manager = &manager;
    state->guest = thread;
    state->replay = restart.state;
    restart.state->ready = false;
    if (!deferral) deferral.emplace();
    {
        std::lock_guard lock(manager.mutex);
        manager.entries.push_back(state);
    }
    ownership.owner = std::move(state);
    current = ownership.owner.get();
    depth = 1;
#ifdef _WIN32
    TimedWait::DeliverDeferred();
#endif
}

void EndIoSignals(std::optional<TimedWait::DeliveryDeferral>& deferral) {
    if (--depth != 0) { deferral.reset(); return; }
    const int saved = errno;
    SignalState* state;
    bool pending;
    {
        const SetupBlock blocked;
        auto finishing = std::move(Ownership().owner);
        state = finishing.get();
#ifndef _WIN32
        state->deliveryMask = blocked.Previous();
#endif
        {
            std::lock_guard lock(state->mutex);
            state->active = false;
        }
        {
            std::lock_guard lock(state->manager->mutex);
            std::erase(state->manager->entries, finishing);
        }
        current = nullptr;
        pending = state->Pending();
        state->previousDelivery = std::move(Ownership().delivering);
        Ownership().delivering = std::move(finishing);
        deferral.reset();
    }
    for (int signal = 1; signal < 64; ++signal)
        if ((state->signals.fetch_and(~(std::uint64_t{1} << signal)) & (std::uint64_t{1} << signal)) != 0) std::raise(signal);
    if (state->callback) state->callback();
    state->callback = nullptr;
    TimedWait::DeliverDeferred();
    {
        const SetupBlock blocked;
        const bool unchanged = std::all_of(state->descriptors.begin(), state->descriptors.end(),
            [](const auto& descriptor) { return descriptor.version.Unchanged(); });
        state->replay->ready = pending && state->restart.load() && state->replay->allowed && !state->descriptors.empty() && unchanged;
        if (state->replay->ready) state->replay->expected = std::move(state->descriptors);
        else state->replay->expected.clear();
        auto* cursor = &Ownership().delivering;
        while (*cursor && cursor->get() != state) cursor = &(*cursor)->previousDelivery;
        if (*cursor) *cursor = std::move(state->previousDelivery);
    }
    errno = saved;
}

bool IoInterrupted() {
    if (GuestDescriptorAccess::Operation::CurrentInterrupted()) { IoNoRestart(); return true; }
    return (current && current->Pending()) || (IoEnabled() && Requested());
}

bool DeferIoSignal(int signal, bool restart) {
    if (!current || signal <= 0 || signal >= 64) return false;
    const int saved = errno;
    if (!restart) current->restart.store(false);
    current->signals.fetch_or(std::uint64_t{1} << signal);
    current->manager->Notify();
    errno = saved;
    return true;
}

bool DeferIoCallback(void (*callback)(), void (*retire)()) {
    if (!current) return false;
    current->callback = callback;
    current->retire = retire;
    current->delivery.store(true);
    current->manager->Notify();
    return true;
}

void IoNoRestart() { if (current) current->restart.store(false); }

bool TrackIoDescriptor(int descriptor) {
    if (!current || descriptor < 0) return true;
    GuestDescriptorAccess access(descriptor, GuestDescriptorAccess::Mode::Read, std::defer_lock);
    const auto expected = std::find_if(current->replay->expected.begin(), current->replay->expected.end(),
        [descriptor](const auto& entry) { return entry.number == descriptor; });
    if (expected != current->replay->expected.end() && !expected->version.Unchanged()) {
        current->restart.store(false);
        return false;
    }
    auto observed = std::find_if(current->descriptors.begin(), current->descriptors.end(),
        [descriptor](const auto& entry) { return entry.number == descriptor; });
    if (observed == current->descriptors.end()) current->descriptors.push_back({descriptor, access.Snapshot()});
    else observed->version = access.Snapshot();
    return true;
}

void RetireIoStack(std::uintptr_t stack, std::uint32_t context) {
    std::uint64_t signals = 0;
    void (*callback)() = nullptr;
#ifndef _WIN32
    std::optional<sigset_t> mask;
#endif
    {
        const SetupBlock blocked;
        const bool exiting = stack == std::numeric_limits<std::uintptr_t>::max();
        if (!current && !restarts) {
            if (!exiting) JumpBuffer::GuestContextSet_nid_no_patch(context);
            return;
        }
        const auto previousContext = JumpBuffer::GuestContextCurrent_nid_no_patch();
        if (!exiting) JumpBuffer::GuestContextSet_nid_no_patch(context);
#ifdef _WIN32
        auto* ownership = FindHostThreadLocal<SignalOwnership, SignalOwnership>();
#else
        auto* ownership = &Ownership();
#endif
        std::shared_ptr<SignalState> noDelivery;
        auto* deliveryCursor = ownership ? &ownership->delivering : &noDelivery;
#ifndef _WIN32
        const auto here = reinterpret_cast<std::uintptr_t>(&blocked);
        bool savedCurrent = false;
#endif
        while (*deliveryCursor) {
            auto& delivery = **deliveryCursor;
#ifndef _WIN32
            if (!exiting && !savedCurrent && delivery.replay->context == previousContext && here < delivery.replay->stack) {
                delivery.suspendedMask = blocked.Previous();
                delivery.suspended = true;
                savedCurrent = true;
                mask = delivery.deliveryMask;
            }
#endif
            if (!exiting && !delivery.replay->Abandoned(stack, context)) {
                deliveryCursor = &delivery.previousDelivery;
                continue;
            }
            signals |= delivery.signals.load();
            if (const auto next = exiting ? delivery.retire : delivery.callback) callback = next;
#ifndef _WIN32
            if (!exiting) mask = delivery.deliveryMask;
#endif
            *deliveryCursor = std::move(delivery.previousDelivery);
        }
#ifndef _WIN32
        if (!exiting) {
            for (auto delivery = ownership->delivering; delivery; delivery = delivery->previousDelivery) {
                if (delivery->suspended && delivery->replay->context == context && stack < delivery->replay->stack) {
                    mask = delivery->suspendedMask;
                    break;
                }
            }
        }
#endif
        auto** restartCursor = &restarts;
        while (*restartCursor) {
            if (!exiting && !(*restartCursor)->Abandoned(stack, context)) {
                restartCursor = &(*restartCursor)->previous;
                continue;
            }
            auto* retired = *restartCursor;
            *restartCursor = retired->previous;
            delete retired;
        }
        if (exiting) signals = 0;
    }
#ifndef _WIN32
    if (mask) pthread_sigmask(SIG_SETMASK, &*mask, nullptr);
#endif
    for (int signal = 1; signal < 64; ++signal)
        if ((signals & (std::uint64_t{1} << signal)) != 0) std::raise(signal);
    if (callback) callback();
}

void WakeIoDelivery(Pthread thread) {
    if (!thread) return;
    auto* manager = published.load(std::memory_order_acquire);
    if (!manager) return;
    std::lock_guard registered(manager->mutex);
    for (const auto& state : manager->entries) {
        if (state->guest != thread) continue;
        state->delivery.store(true);
        manager->Notify();
    }
}

void IoWait::BeginSignal() {
    if (!current) return;
    std::lock_guard lock(current->mutex);
    signalState = current;
    signalPrevious = current->wait;
    current->wait = this;
}

void IoWait::EndSignal() {
    if (!signalState) return;
    auto& state = *static_cast<SignalState*>(signalState);
    std::lock_guard lock(state.mutex);
    state.wait = signalPrevious;
}

}
