#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#include "prx/libc/include/PreciseWait.hpp"
#endif
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <time.h>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif


extern FileStream* __stderrp_nid_postfix;
[[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
}

namespace {

constexpr std::int64_t nanosPerSecond = 1000000000;
constexpr std::int64_t longestSleepSeconds = 86400;

timespec XtimeDeadline(const Xtime& time, const char* function) {
    constexpr auto largest = std::numeric_limits<std::int64_t>::max();
    constexpr auto smallest = std::numeric_limits<std::int64_t>::min();
    const auto carry = time.nsec / nanosPerSecond;
    auto nanos = time.nsec % nanosPerSecond;
    auto seconds = time.sec;
    if ((carry > 0 && seconds > largest - carry) || (carry < 0 && seconds < smallest - carry))
        throw std::overflow_error(std::string(function) + ": xtime seconds overflow");
    seconds += carry;
    if (nanos < 0) {
        if (seconds == smallest)
            throw std::overflow_error(std::string(function) + ": xtime seconds overflow");
        nanos += nanosPerSecond;
        --seconds;
    }
    return {static_cast<time_t>(seconds), static_cast<long>(nanos)};
}

std::chrono::system_clock::time_point XtimePoint(const Xtime& time, const char* function) {
    const auto deadline = XtimeDeadline(time, function);
    constexpr auto maxDuration = std::chrono::system_clock::duration::max();
    constexpr auto minDuration = std::chrono::system_clock::duration::min();
    constexpr auto maxSeconds = std::chrono::duration_cast<std::chrono::seconds>(maxDuration).count();
    constexpr auto minSeconds = std::chrono::duration_cast<std::chrono::seconds>(minDuration).count();
    constexpr auto maxNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(maxDuration - std::chrono::seconds(maxSeconds)).count();
    if (deadline.tv_sec > maxSeconds || (deadline.tv_sec == maxSeconds && deadline.tv_nsec > maxNanos) ||
        deadline.tv_sec < minSeconds)
        throw std::overflow_error(std::string(function) + ": xtime seconds overflow");
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(deadline.tv_sec) +
        std::chrono::ceil<std::chrono::system_clock::duration>(std::chrono::nanoseconds(deadline.tv_nsec)));
}

constexpr int threadSuccess = 0;
constexpr int threadNomem = 1;
constexpr int threadTimedout = 2;
constexpr int threadBusy = 3;
constexpr int threadError = 4;
constexpr int threadTimedlockError = 5;

constexpr int mutexRecursive = 0x100;

constexpr std::uint64_t mutexTag = 0x5854'4D5F'4D54'5844ull;
constexpr std::uint64_t conditionTag = 0x5854'4E43'5F44'4E43ull;

#ifdef _WIN32
struct DinkumwareNativeMutex {
    CRITICAL_SECTION value;

    DinkumwareNativeMutex() {
        if (!InitializeCriticalSectionAndSpinCount(&value, 0))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "_Mtx_init: initializing critical section failed");
    }

    ~DinkumwareNativeMutex() { DeleteCriticalSection(&value); }
    void lock() { EnterCriticalSection(&value); }
    void unlock() { LeaveCriticalSection(&value); }
    bool try_lock() { return TryEnterCriticalSection(&value) != FALSE; }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline) {
        if (try_lock())
            return true;
        for (;;) {
            const auto now = Clock::now();
            if (now >= deadline)
                return false;
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
            Sleep(static_cast<DWORD>(std::min<std::int64_t>(remaining, 1)));
            if (try_lock()) {
                if (Clock::now() <= deadline)
                    return true;
                unlock();
                return false;
            }
        }
    }
};

struct DinkumwareNativeCondition {
    CONDITION_VARIABLE value = CONDITION_VARIABLE_INIT;

    void notify_one() { WakeConditionVariable(&value); }
    void notify_all() { WakeAllConditionVariable(&value); }

    void wait(std::unique_lock<DinkumwareNativeMutex>& lock) {
        if (SleepConditionVariableCS(&value, &lock.mutex()->value, INFINITE) == FALSE)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "_Cnd_wait: condition wait failed");
    }

    template<class Clock, class Duration>
    std::cv_status wait_until(std::unique_lock<DinkumwareNativeMutex>& lock,
                              const std::chrono::time_point<Clock, Duration>& deadline) {
        for (;;) {
            const auto now = Clock::now();
            if (now >= deadline)
                return std::cv_status::timeout;
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
            const auto largest = static_cast<std::int64_t>(std::numeric_limits<DWORD>::max() - 1U);
            const auto milliseconds = static_cast<DWORD>(std::min<std::int64_t>(remaining, largest));
            if (SleepConditionVariableCS(&value, &lock.mutex()->value, milliseconds) != FALSE)
                return std::cv_status::no_timeout;
            const DWORD error = GetLastError();
            if (error != ERROR_TIMEOUT)
                throw std::system_error(static_cast<int>(error), std::system_category(), "_Cnd_timedwait: condition wait failed");
            if (Clock::now() >= deadline)
                return std::cv_status::timeout;
        }
    }
};
#else
using DinkumwareNativeMutex = std::timed_mutex;
using DinkumwareNativeCondition = std::condition_variable_any;
#endif

struct DinkumwareMutex {
    std::uint64_t tag = mutexTag;
    int type = 0;
    int count = 0;
    std::atomic<std::thread::id> owner{};
    DinkumwareNativeMutex native;
};

struct DinkumwareCondition {
    std::uint64_t tag = conditionTag;
    DinkumwareNativeCondition native;
};

DinkumwareMutex* ResolveMutex(void** handle, const char* function) {
    if (!handle)
        throw std::invalid_argument(std::string(function) + ": null mutex handle pointer");
    auto* mutex = static_cast<DinkumwareMutex*>(*handle);
    if (!mutex)
        throw std::invalid_argument(std::string(function) + ": null mutex handle");
    if (mutex->tag != mutexTag)
        throw std::invalid_argument(std::string(function) + ": not a mutex created by _Mtx_init");
    return mutex;
}

DinkumwareCondition* ResolveCondition(void** handle, const char* function) {
    if (!handle)
        throw std::invalid_argument(std::string(function) + ": null condition handle pointer");
    auto* condition = static_cast<DinkumwareCondition*>(*handle);
    if (!condition)
        throw std::invalid_argument(std::string(function) + ": null condition handle");
    if (condition->tag != conditionTag)
        throw std::invalid_argument(std::string(function) + ": not a condition created by _Cnd_init");
    return condition;
}

bool IsOwnedByCaller(const DinkumwareMutex* mutex) {
    return mutex->owner.load(std::memory_order_acquire) == std::this_thread::get_id() && mutex->count > 0;
}

}

extern "C" {

int APS5_VABI _Mtx_init_nid_postfix(void** handle, int type) {
    if (!handle)
        throw std::invalid_argument("_Mtx_init: null handle pointer");
    *handle = nullptr;
    auto* mutex = new (std::nothrow) DinkumwareMutex();
    if (!mutex)
        return threadNomem;
    mutex->type = type;
    *handle = mutex;
    return threadSuccess;
}

int APS5_VABI _Mtx_init_with_name_nid_postfix(void** handle, int type, const char*) {
    return _Mtx_init_nid_postfix(handle, type);
}

int APS5_VABI _Mtx_init_with_default_name_override_nid_postfix(void** handle, int type, const char* name) {
    return _Mtx_init_with_name_nid_postfix(handle, type, name);
}

void APS5_VABI _Mtx_destroy_nid_postfix(void** handle) {
    if (!handle)
        throw std::invalid_argument("_Mtx_destroy: null mutex handle pointer");
    if (!*handle)
        return;
    auto* mutex = ResolveMutex(handle, "_Mtx_destroy");
    if (mutex->count != 0)
        throw std::logic_error("_Mtx_destroy: mutex destroyed while locked");
    mutex->tag = 0;
    delete mutex;
    *handle = nullptr;
}

int APS5_VABI _Mtx_lock_nid_postfix(void** handle) {
    auto* mutex = ResolveMutex(handle, "_Mtx_lock");
    const auto self = std::this_thread::get_id();
    if (mutex->owner.load(std::memory_order_acquire) == self) {
        if ((mutex->type & mutexRecursive) == 0)
            return threadBusy;
        if (mutex->count == std::numeric_limits<int>::max())
            throw std::overflow_error("_Mtx_lock: recursive lock count overflow");
        ++mutex->count;
        return threadSuccess;
    }
    try {
        mutex->native.lock();
    } catch (const std::system_error& error) {
        return error.code() == std::errc::resource_deadlock_would_occur ? threadBusy : threadError;
    }
    mutex->owner.store(self, std::memory_order_release);
    mutex->count = 1;
    return threadSuccess;
}

int APS5_VABI _Mtx_trylock_nid_postfix(void** handle) {
    auto* mutex = ResolveMutex(handle, "_Mtx_trylock");
    const auto self = std::this_thread::get_id();
    if (mutex->owner.load(std::memory_order_acquire) == self) {
        if ((mutex->type & mutexRecursive) == 0)
            return threadBusy;
        if (mutex->count == std::numeric_limits<int>::max())
            throw std::overflow_error("_Mtx_trylock: recursive lock count overflow");
        ++mutex->count;
        return threadSuccess;
    }
    if (!mutex->native.try_lock())
        return threadBusy;
    mutex->owner.store(self, std::memory_order_release);
    mutex->count = 1;
    return threadSuccess;
}

int APS5_VABI _Mtx_timedlock_nid_postfix(void** handle, const Xtime* target) {
    if (!target)
        APS5_INVALID_ARG_EX;
    const auto deadline = XtimePoint(*target, "_Mtx_timedlock");
    auto* mutex = ResolveMutex(handle, "_Mtx_timedlock");
    const auto self = std::this_thread::get_id();
    if (mutex->owner.load(std::memory_order_acquire) == self) {
        if ((mutex->type & mutexRecursive) == 0) {
            std::this_thread::sleep_until(deadline);
            return threadTimedout;
        }
        if (mutex->count == std::numeric_limits<int>::max())
            throw std::overflow_error("_Mtx_timedlock: recursive lock count overflow");
        ++mutex->count;
        return threadSuccess;
    }
    try {
        if (!mutex->native.try_lock_until(deadline))
            return threadTimedout;
    } catch (const std::system_error&) {
        return threadTimedlockError;
    }
    mutex->owner.store(self, std::memory_order_release);
    mutex->count = 1;
    return threadSuccess;
}

int APS5_VABI _Mtx_unlock_nid_postfix(void** handle) {
    auto* mutex = ResolveMutex(handle, "_Mtx_unlock");
    if (!IsOwnedByCaller(mutex))
        return threadSuccess;
    if (--mutex->count == 0) {
        mutex->owner.store(std::thread::id{}, std::memory_order_release);
        mutex->native.unlock();
    }
    return threadSuccess;
}

int APS5_VABI _Cnd_init_nid_postfix(void** handle) {
    if (!handle)
        throw std::invalid_argument("_Cnd_init: null handle pointer");
    *handle = nullptr;
    auto* condition = new (std::nothrow) DinkumwareCondition();
    if (!condition)
        return threadNomem;
    *handle = condition;
    return threadSuccess;
}

int APS5_VABI _Cnd_init_with_default_name_override_nid_postfix(void** handle, const char*) {
    return _Cnd_init_nid_postfix(handle);
}

int APS5_VABI _Cnd_init_with_name_nid_postfix(void** handle, const char* name) {
    constexpr std::size_t objectNameLimit = 32;
    if (name != nullptr) {
        std::string copied;
        if (const int error = CopyGuestString_nid_no_patch(name, objectNameLimit, copied)) {
            errno = error;
            return threadError;
        }
    }
    return _Cnd_init_nid_postfix(handle);
}

void APS5_VABI _Cnd_destroy_nid_postfix(void** handle) {
    if (!handle)
        throw std::invalid_argument("_Cnd_destroy: null condition handle pointer");
    if (!*handle)
        return;
    auto* condition = ResolveCondition(handle, "_Cnd_destroy");
    condition->tag = 0;
    delete condition;
    *handle = nullptr;
}

int APS5_VABI _Cnd_wait_nid_postfix(void** conditionHandle, void** mutexHandle) {
    auto* condition = ResolveCondition(conditionHandle, "_Cnd_wait");
    auto* mutex = ResolveMutex(mutexHandle, "_Cnd_wait");
    if (!IsOwnedByCaller(mutex))
        return threadSuccess;
    if (mutex->count != 1)
        return threadSuccess;
    mutex->count = 0;
    mutex->owner.store(std::thread::id{}, std::memory_order_release);
    std::unique_lock lock(mutex->native, std::adopt_lock);
    try {
        condition->native.wait(lock);
    } catch (const std::system_error&) {
        lock.release();
        mutex->owner.store(std::this_thread::get_id(), std::memory_order_release);
        mutex->count = 1;
        throw;
    }
    lock.release();
    mutex->owner.store(std::this_thread::get_id(), std::memory_order_release);
    mutex->count = 1;
    return threadSuccess;
}

int APS5_VABI _Cnd_timedwait_nid_postfix(void** conditionHandle, void** mutexHandle, const Xtime* target) {
    if (!target)
        APS5_INVALID_ARG_EX;
    const auto deadlineTime = XtimePoint(*target, "_Cnd_timedwait");
    auto* condition = ResolveCondition(conditionHandle, "_Cnd_timedwait");
    auto* mutex = ResolveMutex(mutexHandle, "_Cnd_timedwait");
    if (!IsOwnedByCaller(mutex))
        return threadSuccess;
    if (mutex->count != 1)
        return threadSuccess;
    mutex->count = 0;
    mutex->owner.store(std::thread::id{}, std::memory_order_release);
    std::unique_lock lock(mutex->native, std::adopt_lock);
    bool timeout = false;
    try {
        timeout = (condition->native.wait_until(lock, deadlineTime) == std::cv_status::timeout);
    } catch (const std::system_error&) {
        lock.release();
        mutex->owner.store(std::this_thread::get_id(), std::memory_order_release);
        mutex->count = 1;
        throw;
    }
    lock.release();
    mutex->owner.store(std::this_thread::get_id(), std::memory_order_release);
    mutex->count = 1;
    return timeout ? threadTimedout : threadSuccess;
}

int APS5_VABI _Cnd_broadcast_nid_postfix(void** handle) {
    ResolveCondition(handle, "_Cnd_broadcast")->native.notify_all();
    return threadSuccess;
}

int SignalCondition_nid_no_patch(void** handle) {
    ResolveCondition(handle, "_Cnd_signal")->native.notify_one();
    return threadSuccess;
}

static std::atomic<unsigned> registeredProcessorCount{0};

void RegisterProcessorCount_nid_no_patch(unsigned count) {
    registeredProcessorCount.store(count, std::memory_order_release);
}

unsigned APS5_VABI _ZNSt6thread20hardware_concurrencyEv_nid_postfix() {
    const unsigned count = registeredProcessorCount.load(std::memory_order_acquire);
    if (count == 0) throw std::runtime_error("std::thread::hardware_concurrency: libkernel has not registered the processor count");
    return count;
}

int APS5_VABI _Thrd_sleep_nid_postfix(const Xtime* target, Xtime*) {
    if (!target)
        APS5_INVALID_ARG_EX;
    const auto deadline = XtimeDeadline(*target, "_Thrd_sleep");
    for (;;) {
        timespec now{};
        if (clock_gettime(CLOCK_REALTIME, &now) != 0)
            throw std::runtime_error("_Thrd_sleep: reading the realtime clock failed");
        if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            return threadSuccess;
        timespec remaining{static_cast<time_t>(deadline.tv_sec - now.tv_sec), static_cast<long>(deadline.tv_nsec - now.tv_nsec)};
        if (remaining.tv_nsec < 0) {
            remaining.tv_nsec += nanosPerSecond;
            --remaining.tv_sec;
        }
        remaining.tv_sec = std::min<time_t>(remaining.tv_sec, longestSleepSeconds);
#ifdef _WIN32
        PreciseSleepHundredNanos(static_cast<unsigned long long>(remaining.tv_sec) * 10000000ULL +
            (static_cast<unsigned long long>(remaining.tv_nsec) + 99ULL) / 100ULL);
#else
        if (nanosleep(&remaining, nullptr) != 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("_Thrd_sleep: nanosleep failed");
        }
#endif
    }
}

int APS5_VABI _Thrd_yield_nid_postfix() {
    std::this_thread::yield();
    return threadSuccess;
}

int APS5_VABI _Cnd_signal_nid_postfix(void** handle) {
    return SignalCondition_nid_no_patch(handle);
}

struct AtThreadExitEntry {
    std::thread::id thread;
    DinkumwareCondition* condition;
    DinkumwareMutex* mutex;
    int* ready;
};

static std::mutex atThreadExitLock;
static std::list<AtThreadExitEntry> atThreadExitEntries;

void APS5_VABI _Cnd_do_broadcast_at_thread_exit_nid_postfix() noexcept {
    const auto thread = std::this_thread::get_id();
    std::list<AtThreadExitEntry> pending;
    {
        const std::lock_guard<std::mutex> lock(atThreadExitLock);
        for (auto entry = atThreadExitEntries.begin(); entry != atThreadExitEntries.end();) {
            if (entry->thread == thread) {
                auto moved = entry++;
                pending.splice(pending.end(), atThreadExitEntries, moved);
            } else {
                ++entry;
            }
        }
    }
    for (const auto& entry : pending) {
        if (entry.ready) *entry.ready = 1;
        void* condition = entry.condition;
        _Cnd_broadcast_nid_postfix(&condition);
        void* mutex = entry.mutex;
        _Mtx_unlock_nid_postfix(&mutex);
    }
}

void APS5_VABI _Cnd_register_at_thread_exit_nid_postfix(void* conditionHandle, void* mutexHandle, int* ready) {
    auto* condition = ResolveCondition(&conditionHandle, "_Cnd_register_at_thread_exit");
    auto* mutex = ResolveMutex(&mutexHandle, "_Cnd_register_at_thread_exit");
    const std::lock_guard<std::mutex> lock(atThreadExitLock);
    atThreadExitEntries.push_back({std::this_thread::get_id(), condition, mutex, ready});
}

void APS5_VABI _Cnd_unregister_at_thread_exit_nid_postfix(void* mutexHandle) {
    auto* mutex = ResolveMutex(&mutexHandle, "_Cnd_unregister_at_thread_exit");
    const std::lock_guard<std::mutex> lock(atThreadExitLock);
    atThreadExitEntries.erase(std::remove_if(atThreadExitEntries.begin(), atThreadExitEntries.end(),
        [mutex](const AtThreadExitEntry& entry) { return entry.mutex == mutex; }), atThreadExitEntries.end());
}

static_assert(offsetof(GuestPad, condition) == 0x8 && offsetof(GuestPad, mutex) == 0x10 && offsetof(GuestPad, started) == 0x18 && sizeof(GuestPad) == 0x20);

[[noreturn]] void ThrowCppError_nid_no_patch(int code);

static void RequireThrd(int result, const char* what) {
    if (result == threadSuccess) return;
    _ZSt14_Throw_C_errori_nid_postfix(result);
    throw std::runtime_error(std::string(what) + " failed with " + std::to_string(result));
}

static std::atomic<PthreadCreateFn_nid_no_patch> registeredPthreadCreate{nullptr};
static std::atomic<PthreadJoinFn_nid_no_patch> registeredPthreadJoin{nullptr};
static std::atomic<PthreadDetachFn_nid_no_patch> registeredPthreadDetach{nullptr};

void RegisterPthreadCreate_nid_no_patch(PthreadCreateFn_nid_no_patch fn) {
    registeredPthreadCreate.store(fn, std::memory_order_release);
}

void RegisterPthreadJoin_nid_no_patch(PthreadJoinFn_nid_no_patch fn) {
    registeredPthreadJoin.store(fn, std::memory_order_release);
}

void RegisterPthreadDetach_nid_no_patch(PthreadDetachFn_nid_no_patch fn) {
    registeredPthreadDetach.store(fn, std::memory_order_release);
}

static void* APS5_VABI PadEntry_nid_no_patch(void* argument) {
    auto* pad = static_cast<GuestPad*>(argument);
    const auto go = pad->vtable[0];
    const auto result = go(pad);
    _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(result));
}

void APS5_VABI _ZNSt4_PadC2Ev_nid_postfix(GuestPad* pad) {
    if (!pad) APS5_INVALID_ARG_EX;
    RequireThrd(_Cnd_init_nid_postfix(&pad->condition), "_Pad::_Pad: _Cnd_init");
    const int created = _Mtx_init_nid_postfix(&pad->mutex, 1);
    if (created != threadSuccess) {
        _Cnd_destroy_nid_postfix(&pad->condition);

        RequireThrd(created, "_Pad::_Pad: _Mtx_init");
    }
    pad->started = false;
    const int locked = _Mtx_lock_nid_postfix(&pad->mutex);
    if (locked != threadSuccess) {
        _Mtx_destroy_nid_postfix(&pad->mutex);
        _Cnd_destroy_nid_postfix(&pad->condition);

        RequireThrd(locked, "_Pad::_Pad: _Mtx_lock");
    }
}

void APS5_VABI PadConstructWithName_nid_no_patch(GuestPad* pad, const char* name) {
    if (!pad) APS5_INVALID_ARG_EX;
    RequireThrd(_Cnd_init_nid_postfix(&pad->condition), "_Pad::_Pad: _Cnd_init");
    const int created = _Mtx_init_with_name_nid_postfix(&pad->mutex, 1, name);
    if (created != threadSuccess) {
        _Cnd_destroy_nid_postfix(&pad->condition);

        RequireThrd(created, "_Pad::_Pad: _Mtx_init");
    }
    pad->started = false;
    const int locked = _Mtx_lock_nid_postfix(&pad->mutex);
    if (locked != threadSuccess) {
        _Mtx_destroy_nid_postfix(&pad->mutex);
        _Cnd_destroy_nid_postfix(&pad->condition);

        RequireThrd(locked, "_Pad::_Pad: _Mtx_lock");
    }
}

