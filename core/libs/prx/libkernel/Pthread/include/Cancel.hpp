#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_CANCEL_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_CANCEL_HPP

#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/DescriptorAccess.hpp"
#include <cstdint>
#include <cerrno>
#include <optional>
#include <utility>

inline constexpr int GUEST_CANCEL_ENABLE = 0;
inline constexpr int GUEST_CANCEL_DISABLE = 1;
inline constexpr int GUEST_CANCEL_DEFERRED = 0;
inline constexpr int GUEST_CANCEL_ASYNCHRONOUS = 2;
inline void* const GUEST_CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});

struct CleanupHandler {
    CleanupHandler* previous;
    void (APS5_VABI* routine)(void*);
    void* argument;
    int onHeap;
};

extern "C" {
void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info);
void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute);
void APS5_VABI pthread_cleanup_push_nid_postfix(void (APS5_VABI* routine)(void*), void* argument);
void APS5_VABI pthread_cleanup_pop_nid_postfix(int execute);
}

namespace ThreadCancel {

bool Requested();
void Check();
void* CleanupMark();
void CleanupRestore(void* mark);
void BeginWait(TimedWait::Condition* condition);
void EndWait();

bool IoEnabled();
bool IoActive();
bool IoInterrupted();
bool IoRestartError();
class IoRestart {
public:
    struct State;
    explicit IoRestart(bool allowed);
    ~IoRestart();
    bool Ready() const;
    void Clear();
    IoRestart(const IoRestart&) = delete;
    IoRestart& operator=(const IoRestart&) = delete;
private:
    friend void BeginIoSignals(Pthread, IoRestart&, std::optional<TimedWait::DeliveryDeferral>&);
    State* state;
};
void BeginIoSignals(Pthread thread, IoRestart& restart, std::optional<TimedWait::DeliveryDeferral>& deferral);
void EndIoSignals(std::optional<TimedWait::DeliveryDeferral>& deferral);
bool DeferIoSignal(int signal, bool restart = true);
bool DeferIoCallback(void (*callback)(), void (*retire)());
void WakeIoDelivery(Pthread thread);
bool TrackIoDescriptor(int descriptor);
void IoNoRestart();
void RetireIoStack(std::uintptr_t stack, std::uint32_t context = 0);

class IoPoint {
public:
    explicit IoPoint(IoRestart& restart);
    ~IoPoint() noexcept(false);
    IoPoint(const IoPoint&) = delete;
    IoPoint& operator=(const IoPoint&) = delete;

private:
    std::optional<TimedWait::DeliveryDeferral> delivery;
};

class IoWait {
public:
    using Interrupt = void (*)(void*) noexcept;
    IoWait(Interrupt interrupt, void* argument);
    ~IoWait();
    IoWait(const IoWait&) = delete;
    IoWait& operator=(const IoWait&) = delete;
    void Wake() noexcept { interrupt(argument); }

private:
    void BeginSignal();
    void EndSignal();
    Interrupt interrupt;
    void* argument;
    IoWait* previous = nullptr;
    Pthread thread = nullptr;
    void* signalState = nullptr;
    IoWait* signalPrevious = nullptr;
    GuestDescriptorAccess::Operation::WakeRegistration descriptorWake;
};

class NativeIo {
public:
    NativeIo();
    ~NativeIo();
    static void InterruptError();
    NativeIo(const NativeIo&) = delete;
    NativeIo& operator=(const NativeIo&) = delete;

private:
    struct State;
    State* state = nullptr;
};

template <class TCall>
auto Native(TCall&& call) {
    NativeIo operation;
    if (IoInterrupted()) {
        NativeIo::InterruptError();
        return static_cast<decltype(call())>(-1);
    }
    return std::forward<TCall>(call)();
}

template <class TCall>
auto Io(TCall&& call, bool restartable = true) {
    Check();
    IoRestart restart(restartable);
    for (;;) {
        decltype(call()) result;
        {
            IoPoint point(restart);
            result = call();
        }
        if (result >= 0) return result;
        if (Requested()) { restart.Clear(); Check(); }
        if (!restart.Ready()) return result;
    }
}

template <class TLock>
struct WaitLock {
    TLock& inner;
    void lock() {
        EndWait();
        inner.lock();
    }
    void unlock() { inner.unlock(); }
};

template <class TLock>
bool Wait(TimedWait::Condition& condition, TLock& lock, std::optional<std::uint64_t> deadlineNanos) {
    BeginWait(&condition);
    if (Requested()) {
        EndWait();
        return true;
    }
    WaitLock<TLock> waitLock{lock};
    if (deadlineNanos) return condition.WaitUntil(waitLock, *deadlineNanos);
    condition.Wait(waitLock);
    return true;
}

template <class TLock, class TPredicate>
bool WaitUntil(TimedWait::Condition& condition, TLock& lock, std::optional<std::uint64_t> deadlineNanos, TPredicate predicate) {
    while (!predicate()) {
        const bool signaled = Wait(condition, lock, deadlineNanos);
        if (Requested()) {
            condition.NotifyOne();
            lock.unlock();
            Check();
        }
        if (!signaled) return predicate();
    }
    return true;
}

}

#endif
