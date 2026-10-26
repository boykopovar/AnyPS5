#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

namespace {

constexpr int SYNC_ON_ADDRESS_OK = 0;

constexpr int UMTX_OP_RESERVED1 = 1;
constexpr int UMTX_OP_WAIT = 2;
constexpr int UMTX_OP_WAKE = 3;
constexpr int UMTX_OP_MUTEX_TRYLOCK = 4;
constexpr int UMTX_OP_MUTEX_LOCK = 5;
constexpr int UMTX_OP_MUTEX_UNLOCK = 6;
constexpr int UMTX_OP_SET_CEILING = 7;
constexpr int UMTX_OP_CV_WAIT = 8;
constexpr int UMTX_OP_CV_SIGNAL = 9;
constexpr int UMTX_OP_CV_BROADCAST = 10;
constexpr int UMTX_OP_WAIT_UINT = 11;
constexpr int UMTX_OP_RW_RDLOCK = 12;
constexpr int UMTX_OP_RW_WRLOCK = 13;
constexpr int UMTX_OP_RW_UNLOCK = 14;
constexpr int UMTX_OP_WAIT_UINT_PRIVATE = 15;
constexpr int UMTX_OP_WAKE_PRIVATE = 16;
constexpr int UMTX_OP_MUTEX_WAIT = 17;
constexpr int UMTX_OP_MUTEX_WAKE = 18;
constexpr int UMTX_OP_SEM_WAIT = 19;
constexpr int UMTX_OP_SEM_WAKE = 20;
constexpr int UMTX_OP_NWAKE_PRIVATE = 21;
constexpr int UMTX_OP_MAX = 27;
constexpr std::uint32_t UMTX_ABSTIME = 1;
constexpr int GUEST_CLOCK_REALTIME = 0;

constexpr std::uint32_t UMUTEX_UNOWNED = 0;
constexpr std::uint32_t UMUTEX_CONTESTED = 0x80000000u;
constexpr std::uint32_t UMUTEX_PRIO_PROTECT = 0x8;
constexpr std::uint32_t UMUTEX_ROBUST = 0x10;
constexpr std::uint32_t CVWAIT_ABSTIME = 0x2;
constexpr std::uint32_t CVWAIT_CLOCKID = 0x4;
constexpr std::uint32_t URWLOCK_PREFER_READER = 0x2;
constexpr std::uint32_t URWLOCK_WRITE_OWNER = 0x80000000u;
constexpr std::uint32_t URWLOCK_WRITE_WAITERS = 0x40000000u;
constexpr std::uint32_t URWLOCK_READ_WAITERS = 0x20000000u;
constexpr std::uint32_t URWLOCK_MAX_READERS = 0x1fffffffu;

constexpr int GUEST_EPERM = 1;
constexpr int GUEST_EFAULT = 14;
constexpr int GUEST_EBUSY = 16;
constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_EAGAIN = 35;
constexpr int GUEST_EOPNOTSUPP = 45;
constexpr int GUEST_ETIMEDOUT = 60;

constexpr std::int64_t NANOS_PER_SECOND = 1000000000;
constexpr std::uint64_t UNBOUNDED_WAIT_NANOS = 1ULL << 62;

struct UmtxTime {
    KernelTimespec timeout;
    std::uint32_t flags;
    std::uint32_t clockId;
};

struct UserSemaphore {
    std::uint32_t hasWaiters;
    std::uint32_t count;
    std::uint32_t flags;
};

struct UserMutex {
    std::uint32_t owner;
    std::uint32_t flags;
    std::uint32_t ceilings[2];
    std::uint32_t spare[4];
};

struct UserCondition {
    std::uint32_t hasWaiters;
    std::uint32_t flags;
    std::uint32_t clockId;
    std::uint32_t spare;
};

struct UserRwlock {
    std::uint32_t state;
    std::uint32_t flags;
    std::uint32_t blockedReaders;
    std::uint32_t blockedWriters;
    std::uint32_t spare[4];
};

static_assert(sizeof(UserMutex) == 32 && sizeof(UserCondition) == 16 && sizeof(UserRwlock) == 32);

struct AddressWaiter {
    TimedWait::SingleWaiterCondition condition;
    bool woken = false;
};

constexpr std::size_t kBucketCount = 64;

struct alignas(64) WaiterBucket {
    std::mutex mutex;
    std::unordered_map<std::uintptr_t, std::list<AddressWaiter*>> waiters;
};

std::array<WaiterBucket, kBucketCount> g_buckets;

WaiterBucket& BucketOf(std::uintptr_t address) {
    const auto hashed = (address >> 2) * UINT64_C(0x9e3779b97f4a7c15);
    return g_buckets[hashed >> 58];
}

template <class TValue>
bool IsAlignedAddress(std::uintptr_t address) {
    return address != 0 && address % alignof(TValue) == 0;
}

template <class TFunction>
void DeferringDelivery(TFunction function) {
    {
        TimedWait::DeliveryDeferral deferral;
        function();
    }
    TimedWait::DeliverDeferred();
}

template <class TReady>
std::optional<std::chrono::nanoseconds> WaitOnKeyLocked(std::uintptr_t key, TReady ready, std::optional<std::uint64_t> deadlineNanos, bool& woken) {
    auto& bucket = BucketOf(key);
    std::unique_lock<std::mutex> lock(bucket.mutex);
    if (ready()) {
        woken = true;
        return std::nullopt;
    }

    AddressWaiter waiter;
    auto& queue = bucket.waiters[key];
    const auto position = queue.insert(queue.end(), &waiter);
    const auto isWoken = [&] { return waiter.woken; };
    const auto waitStart = std::chrono::steady_clock::now();
    if (!deadlineNanos) {
        waiter.condition.Wait(lock, isWoken);
    } else {
        waiter.condition.WaitUntil(lock, *deadlineNanos, isWoken);
    }
    const auto waited = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart);

    woken = waiter.woken;
    if (!woken) {
        queue.erase(position);
        if (queue.empty()) {
            bucket.waiters.erase(key);
        }
    }
    return waited;
}

template <class TReady>
bool WaitOnKeyUntil(std::uintptr_t key, TReady ready, std::optional<std::uint64_t> deadlineNanos, const void* caller) {
    bool woken = false;
    std::optional<std::chrono::nanoseconds> waited;
    DeferringDelivery([&] { waited = WaitOnKeyLocked(key, ready, deadlineNanos, woken); });
    if (waited) {
        KernelTraceWait_nid_postfix("addr", caller, static_cast<std::uint64_t>(waited->count()), !woken);
    }
    return woken;
}

template <class TValue>
TValue LoadAddressValue(TValue* address) {
    if (reinterpret_cast<std::uintptr_t>(address) % alignof(TValue) == 0) return std::atomic_ref<TValue>(*address).load();
    TValue value;
    asm volatile("mov %1, %0" : "=r"(value) : "m"(*reinterpret_cast<const unsigned char (*)[sizeof(TValue)]>(address)));
    return value;
}

template <class TValue>
bool WaitOnAddressUntil(TValue* address, TValue expected, std::optional<std::uint64_t> deadlineNanos, const void* caller, bool& differed) {
    return WaitOnKeyUntil(reinterpret_cast<std::uintptr_t>(address), [&] { return differed = LoadAddressValue(address) != expected; }, deadlineNanos, caller);
}

template <class TValue>
bool WaitOnAddressUntil(TValue* address, TValue expected, std::optional<std::uint64_t> deadlineNanos, const void* caller) {
    bool differed = false;
    return WaitOnAddressUntil(address, expected, deadlineNanos, caller, differed);
}

template <class TValue>
int WaitOnAddress(TValue* address, TValue expected, KernelUseconds* timeout, const void* caller) {
    if (timeout != nullptr && *timeout == 0) {
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    if (address == nullptr) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    const auto deadline = timeout == nullptr ? std::nullopt : std::optional<std::uint64_t>(TimedWait::DeadlineNanos(*timeout));
    bool differed = false;
    const bool woken = WaitOnAddressUntil(address, expected, deadline, caller, differed);
    if (differed) {
        return SCE_KERNEL_ERROR_EAGAIN;
    }
    if (timeout != nullptr) {
        const std::uint64_t now = TimedWait::NowNanos();
        *timeout = woken && *deadline > now ? static_cast<KernelUseconds>((*deadline - now) / 1000) : 0;
    }
    return woken ? SYNC_ON_ADDRESS_OK : SCE_KERNEL_ERROR_ETIMEDOUT;
}

std::int32_t WakeAddressLocked(std::uintptr_t key, std::int32_t count, bool wakesAtLeastOne) {
    auto& bucket = BucketOf(key);
    std::lock_guard<std::mutex> lock(bucket.mutex);
    const auto entry = bucket.waiters.find(key);
    if (entry == bucket.waiters.end()) {
        return 0;
    }
    auto& queue = entry->second;
    std::int32_t woken = 0;
    for (; !queue.empty() && (woken < count || (wakesAtLeastOne && woken == 0)); ++woken) {
        AddressWaiter* waiter = queue.front();
        queue.pop_front();
        waiter->woken = true;
        waiter->condition.NotifyOne();
    }
    if (queue.empty()) {
        bucket.waiters.erase(entry);
    }
    return woken;
}

std::int32_t WakeAddress(std::uintptr_t key, std::int32_t count, bool wakesAtLeastOne) {
    std::int32_t woken = 0;
    DeferringDelivery([&] { woken = WakeAddressLocked(key, count, wakesAtLeastOne); });
    return woken;
}

std::uint64_t SaturatingNanos(std::int64_t seconds, std::int64_t nanos) {
    if (seconds < 0) {
        return 0;
    }
    if (seconds >= static_cast<std::int64_t>(UNBOUNDED_WAIT_NANOS / NANOS_PER_SECOND)) {
        return UNBOUNDED_WAIT_NANOS;
    }
    const std::int64_t total = seconds * NANOS_PER_SECOND + nanos;
    return total <= 0 ? 0 : static_cast<std::uint64_t>(total);
}

int UmtxDeadline(const void* sizeArgument, const void* timeArgument, std::optional<std::uint64_t>& deadlineNanos) {
    deadlineNanos.reset();
    if (timeArgument == nullptr) {
        return 0;
    }
    if (sizeArgument == nullptr) {
        const auto usecs = *static_cast<const std::uint32_t*>(timeArgument);
        deadlineNanos = TimedWait::DeadlineNanos(usecs);
        return 0;
    }
    UmtxTime time{{0, 0}, 0, GUEST_CLOCK_REALTIME};
    if (reinterpret_cast<std::uintptr_t>(sizeArgument) <= sizeof(KernelTimespec)) {
        time.timeout = *static_cast<const KernelTimespec*>(timeArgument);
    } else {
        time = *static_cast<const UmtxTime*>(timeArgument);
    }
    if (time.timeout.tv_sec < 0 || time.timeout.tv_nsec < 0 || time.timeout.tv_nsec >= NANOS_PER_SECOND) {
        return GUEST_EINVAL;
    }

    std::uint64_t remaining = 0;
    if ((time.flags & UMTX_ABSTIME) == 0) {
        remaining = SaturatingNanos(time.timeout.tv_sec, time.timeout.tv_nsec);
    } else {
        KernelTimespec now{};
        clock_gettime_nid_postfix(static_cast<int>(time.clockId), &now);

        remaining = SaturatingNanos(time.timeout.tv_sec - now.tv_sec, time.timeout.tv_nsec - now.tv_nsec);
    }
    if (remaining < UNBOUNDED_WAIT_NANOS) {
        deadlineNanos = TimedWait::NowNanos() + remaining;
    }
    return 0;
}

template <class TValue>
int UmtxWait(void* object, std::uint64_t expected, const void* sizeArgument, const void* timeArgument, const void* caller) {
    std::optional<std::uint64_t> deadline;
    if (const int error = UmtxDeadline(sizeArgument, timeArgument, deadline); error != 0) {
        return error;
    }
    if (object == nullptr) {
        return GUEST_EFAULT;
    }
    if (!IsAlignedAddress<TValue>(reinterpret_cast<std::uintptr_t>(object))) {
        APS5_INVALID_ARG_EX;
    }
    if (expected > std::numeric_limits<TValue>::max()) {
        return 0;
    }
    return WaitOnAddressUntil(static_cast<TValue*>(object), static_cast<TValue>(expected), deadline, caller) ? 0 : GUEST_ETIMEDOUT;
}

int UmtxWake(void* object, std::uint64_t count, bool isPrivate) {
    if (object == nullptr) {
        return isPrivate ? 0 : GUEST_EFAULT;
    }
    WakeAddress(reinterpret_cast<std::uintptr_t>(object), static_cast<std::int32_t>(count), true);
    return 0;
}

int UmtxNwakePrivate(void* object, std::uint64_t value) {
    const auto count = static_cast<std::int32_t>(value);
    if (count <= 0) {
        return 0;
    }
    if (object == nullptr) {
        return GUEST_EFAULT;
    }
    const auto* addresses = static_cast<void* const*>(object);
    for (std::int32_t index = 0; index < count; ++index) {
        WakeAddress(reinterpret_cast<std::uintptr_t>(addresses[index]), std::numeric_limits<std::int32_t>::max(), true);
    }
    return 0;
}

void WakeSemaphoreLocked(std::uintptr_t key, UserSemaphore* semaphore) {
    auto& bucket = BucketOf(key);
    std::lock_guard<std::mutex> lock(bucket.mutex);
    const auto entry = bucket.waiters.find(key);
    if (entry == bucket.waiters.end()) {
        return;
    }
    auto& queue = entry->second;
    if (queue.size() == 1) {
        std::atomic_ref<std::uint32_t>(semaphore->hasWaiters).store(0);
    }
    AddressWaiter* waiter = queue.front();
    queue.pop_front();
    waiter->woken = true;
    waiter->condition.NotifyOne();
    if (queue.empty()) {
        bucket.waiters.erase(entry);
    }
}

int SemWait(UserSemaphore* semaphore, const void* sizeArgument, const void* timeArgument, const void* caller) {
    std::optional<std::uint64_t> deadline;
    if (const int error = UmtxDeadline(sizeArgument, timeArgument, deadline); error != 0) {
        return error;
    }
    if (semaphore == nullptr) {
        return GUEST_EFAULT;
    }
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(semaphore))) {
        APS5_INVALID_ARG_EX;
    }
    const bool woken = WaitOnKeyUntil(reinterpret_cast<std::uintptr_t>(semaphore), [&] {
        std::uint32_t idle = 0;
        std::atomic_ref<std::uint32_t>(semaphore->hasWaiters).compare_exchange_strong(idle, 1);
        return std::atomic_ref<std::uint32_t>(semaphore->count).load() != 0;
    }, deadline, caller);
    return woken ? 0 : GUEST_ETIMEDOUT;
}

int SemWake(UserSemaphore* semaphore) {
    if (semaphore == nullptr) {
        return GUEST_EFAULT;
    }
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(semaphore))) {
        APS5_INVALID_ARG_EX;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(semaphore);
    DeferringDelivery([&] { WakeSemaphoreLocked(key, semaphore); });
    return 0;
}

std::uint32_t CurrentOwnerId() {
    return static_cast<std::uint32_t>(scePthreadGetthreadid()) & ~UMUTEX_CONTESTED;
}

std::size_t WaiterCount(std::uintptr_t key) {
    auto& bucket = BucketOf(key);
    std::lock_guard<std::mutex> lock(bucket.mutex);
    const auto entry = bucket.waiters.find(key);
    return entry == bucket.waiters.end() ? 0 : entry->second.size();
}

int CheckMutex(UserMutex* mutex) {
    if (mutex == nullptr) {
        return GUEST_EFAULT;
    }
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mutex))) {
        APS5_INVALID_ARG_EX;
    }
    const std::uint32_t flags = std::atomic_ref<std::uint32_t>(mutex->flags).load();
    if ((flags & UMUTEX_ROBUST) != 0) {
        NotImplemented_nid_no_patch("_umtx_op on a robust umutex");
    }
    return 0;
}

bool TryAcquireMutex(UserMutex* mutex, std::uint32_t id, int& error) {
    std::atomic_ref<std::uint32_t> owner(mutex->owner);
    std::uint32_t current = owner.load();
    for (;;) {
        if (current == UMUTEX_UNOWNED || current == UMUTEX_CONTESTED) {
            if (owner.compare_exchange_weak(current, id | current)) {
                error = 0;
                return true;
            }
            continue;
        }
        if ((current & ~UMUTEX_CONTESTED) == id) {
            error = 0;
            return true;
        }
        error = GUEST_EBUSY;
        return false;
    }
}

void MarkContested(UserMutex* mutex) {
    std::atomic_ref<std::uint32_t> owner(mutex->owner);
    std::uint32_t current = owner.load();
    while (current != UMUTEX_UNOWNED && (current & UMUTEX_CONTESTED) == 0 && !owner.compare_exchange_weak(current, current | UMUTEX_CONTESTED)) {
    }
}

int MutexTrylock(UserMutex* mutex) {
    if (const int error = CheckMutex(mutex); error != 0) {
        return error;
    }
    if ((std::atomic_ref<std::uint32_t>(mutex->flags).load() & UMUTEX_PRIO_PROTECT) != 0) {
        return GUEST_EINVAL;
    }
    int error = 0;
    TryAcquireMutex(mutex, CurrentOwnerId(), error);
    return error;
}

int MutexLock(UserMutex* mutex, bool acquire, const void* sizeArgument, const void* timeArgument, const void* caller) {
    if (const int error = CheckMutex(mutex); error != 0) {
        return error;
    }
    if ((std::atomic_ref<std::uint32_t>(mutex->flags).load() & UMUTEX_PRIO_PROTECT) != 0) {
        return GUEST_EINVAL;
    }
    std::optional<std::uint64_t> deadline;
    if (const int error = UmtxDeadline(sizeArgument, timeArgument, deadline); error != 0) {
        return error;
    }
    const std::uint32_t id = CurrentOwnerId();
    const auto key = reinterpret_cast<std::uintptr_t>(&mutex->owner);
    for (;;) {
        bool done = false;
        int error = 0;
        const bool woken = WaitOnKeyUntil(key, [&] {
            if (acquire) {
                done = TryAcquireMutex(mutex, id, error);
            } else {
                const std::uint32_t current = std::atomic_ref<std::uint32_t>(mutex->owner).load();
                done = current == UMUTEX_UNOWNED || current == UMUTEX_CONTESTED;
            }
            if (!done) {
                MarkContested(mutex);
            }
            return done;
        }, deadline, caller);
        if (done) {
            return 0;
        }
        if (!woken) {
            return GUEST_ETIMEDOUT;
        }
        if (!acquire) {
            return 0;
        }
    }
}

int MutexUnlock(UserMutex* mutex) {
    if (const int error = CheckMutex(mutex); error != 0) {
        return error;
    }
    if ((std::atomic_ref<std::uint32_t>(mutex->flags).load() & UMUTEX_PRIO_PROTECT) != 0) {
        NotImplemented_nid_no_patch("_umtx_op unlock of a priority-protected umutex");
    }
    std::atomic_ref<std::uint32_t> owner(mutex->owner);
    if ((owner.load() & ~UMUTEX_CONTESTED) != CurrentOwnerId()) {
        return GUEST_EPERM;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(&mutex->owner);
    owner.store(WaiterCount(key) > 1 ? UMUTEX_CONTESTED : UMUTEX_UNOWNED);
    WakeAddress(key, 1, false);
    return 0;
}

int MutexWake(UserMutex* mutex) {
    if (const int error = CheckMutex(mutex); error != 0) {
        return error;
    }
    std::atomic_ref<std::uint32_t> owner(mutex->owner);
    if ((owner.load() & ~UMUTEX_CONTESTED) != 0) {
        return 0;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(&mutex->owner);
    if (WaiterCount(key) <= 1) {
        std::uint32_t contested = UMUTEX_CONTESTED;
        owner.compare_exchange_strong(contested, UMUTEX_UNOWNED);
    }
    WakeAddress(key, 1, false);
    return 0;
}

int ConditionDeadline(const UserCondition* condition, std::uint64_t flags, const void* timeArgument, std::optional<std::uint64_t>& deadlineNanos) {
    deadlineNanos.reset();
    if (timeArgument == nullptr) {
        return 0;
    }
    const auto timeout = *static_cast<const KernelTimespec*>(timeArgument);
    if (timeout.tv_sec < 0 || timeout.tv_nsec < 0 || timeout.tv_nsec >= NANOS_PER_SECOND) {
        return GUEST_EINVAL;
    }
    std::uint64_t remaining = 0;
    if ((flags & CVWAIT_ABSTIME) == 0) {
        remaining = SaturatingNanos(timeout.tv_sec, timeout.tv_nsec);
    } else {
        const int clock = (flags & CVWAIT_CLOCKID) != 0 ? static_cast<int>(std::atomic_ref<const std::uint32_t>(condition->clockId).load()) : GUEST_CLOCK_REALTIME;
        KernelTimespec now{};
        clock_gettime_nid_postfix(clock, &now);
        remaining = SaturatingNanos(timeout.tv_sec - now.tv_sec, timeout.tv_nsec - now.tv_nsec);
    }
    if (remaining < UNBOUNDED_WAIT_NANOS) {
        deadlineNanos = TimedWait::NowNanos() + remaining;
    }
    return 0;
}

int ConditionWait(UserCondition* condition, UserMutex* mutex, std::uint64_t flags, const void* timeArgument, const void* caller) {
    if (condition == nullptr) {
        return GUEST_EFAULT;
    }
    if (const int error = CheckMutex(mutex); error != 0) {
        return error;
    }
    if ((std::atomic_ref<std::uint32_t>(mutex->flags).load() & UMUTEX_PRIO_PROTECT) != 0) {
        NotImplemented_nid_no_patch("_umtx_op condition wait on a priority-protected umutex");
    }
    std::optional<std::uint64_t> deadline;
    if (const int error = ConditionDeadline(condition, flags, timeArgument, deadline); error != 0) {
        return error;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(condition);
    auto& bucket = BucketOf(key);
    AddressWaiter waiter;
    int error = 0;
    bool woken = false;
    std::optional<std::chrono::nanoseconds> waited;
    DeferringDelivery([&] {
        std::unique_lock<std::mutex> lock(bucket.mutex);
        std::atomic_ref<std::uint32_t>(condition->hasWaiters).store(1);
        auto& queue = bucket.waiters[key];
        const auto position = queue.insert(queue.end(), &waiter);
        lock.unlock();
        error = MutexUnlock(mutex);
        lock.lock();
        if (error == 0) {
            const auto isWoken = [&] { return waiter.woken; };
            const auto waitStart = std::chrono::steady_clock::now();
            if (!deadline) {
                waiter.condition.Wait(lock, isWoken);
            } else {
                waiter.condition.WaitUntil(lock, *deadline, isWoken);
            }
            waited = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart);
        }
        woken = waiter.woken;
        if (!woken) {
            auto& current = bucket.waiters[key];
            current.erase(position);
            if (current.empty()) {
                bucket.waiters.erase(key);
                std::atomic_ref<std::uint32_t>(condition->hasWaiters).store(0);
            }
        }
    });
    if (waited) {
        KernelTraceWait_nid_postfix("cv", caller, static_cast<std::uint64_t>(waited->count()), !woken);
    }
    if (error != 0) {
        return error;
    }
    return woken ? 0 : GUEST_ETIMEDOUT;
}

int ConditionWake(UserCondition* condition, bool all) {
    if (condition == nullptr) {
        return GUEST_EFAULT;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(condition);
    DeferringDelivery([&] {
        auto& bucket = BucketOf(key);
        std::lock_guard<std::mutex> lock(bucket.mutex);
        const auto entry = bucket.waiters.find(key);
        if (entry == bucket.waiters.end()) {
            std::atomic_ref<std::uint32_t>(condition->hasWaiters).store(0);
            return;
        }
        auto& queue = entry->second;
        do {
            AddressWaiter* waiter = queue.front();
            queue.pop_front();
            waiter->woken = true;
            waiter->condition.NotifyOne();
        } while (all && !queue.empty());
        if (queue.empty()) {
            bucket.waiters.erase(entry);
            std::atomic_ref<std::uint32_t>(condition->hasWaiters).store(0);
        }
    });
    return 0;
}

int CheckRwlock(UserRwlock* rwlock) {
    if (rwlock == nullptr) {
        return GUEST_EFAULT;
    }
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(rwlock))) {
        APS5_INVALID_ARG_EX;
    }
    return 0;
}

void ClearRwlockBits(UserRwlock* rwlock, std::uint32_t bits) {
    std::atomic_ref<std::uint32_t>(rwlock->state).fetch_and(~bits);
}

int RwlockLock(UserRwlock* rwlock, bool writer, std::uint64_t operationFlags, const void* sizeArgument, const void* timeArgument, const void* caller) {
    if (const int error = CheckRwlock(rwlock); error != 0) {
        return error;
    }
    std::optional<std::uint64_t> deadline;
    if (const int error = UmtxDeadline(sizeArgument, timeArgument, deadline); error != 0) {
        return error;
    }
    std::atomic_ref<std::uint32_t> state(rwlock->state);
    std::atomic_ref<std::uint32_t> blocked(writer ? rwlock->blockedWriters : rwlock->blockedReaders);
    const std::uint32_t waiterBit = writer ? URWLOCK_WRITE_WAITERS : URWLOCK_READ_WAITERS;
    const bool preferReader = ((std::atomic_ref<std::uint32_t>(rwlock->flags).load() | operationFlags) & URWLOCK_PREFER_READER) != 0;
    const auto key = reinterpret_cast<std::uintptr_t>(writer ? &rwlock->blockedWriters : &rwlock->state);
    for (;;) {
        int error = 0;
        bool done = false;
        const bool woken = WaitOnKeyUntil(key, [&] {
            std::uint32_t current = state.load();
            for (;;) {
                const bool free = writer ? (current & (URWLOCK_WRITE_OWNER | URWLOCK_MAX_READERS)) == 0
                                         : (current & URWLOCK_WRITE_OWNER) == 0 && (preferReader || (current & URWLOCK_WRITE_WAITERS) == 0);
                if (!free) {
                    if ((current & waiterBit) == 0 && !state.compare_exchange_weak(current, current | waiterBit)) {
                        continue;
                    }
                    break;
                }
                if (!writer && (current & URWLOCK_MAX_READERS) == URWLOCK_MAX_READERS) {
                    error = GUEST_EAGAIN;
                    done = true;
                    return true;
                }
                if (state.compare_exchange_weak(current, writer ? current | URWLOCK_WRITE_OWNER : current + 1)) {
                    done = true;
                    return true;
                }
            }
            blocked.fetch_add(1);
            return false;
        }, deadline, caller);
        if (done) {
            return error;
        }
        std::uint32_t left;
        {
            std::lock_guard<std::mutex> lock(BucketOf(key).mutex);
            left = blocked.fetch_sub(1) - 1;
            if (left == 0 && (writer || woken)) {
                ClearRwlockBits(rwlock, waiterBit);
                if (blocked.load() != 0) {
                    std::uint32_t current = state.load();
                    while ((current & waiterBit) == 0 && !state.compare_exchange_weak(current, current | waiterBit)) {
                    }
                }
            }
        }
        if (!woken) {
            return GUEST_ETIMEDOUT;
        }
    }
}

int RwlockUnlock(UserRwlock* rwlock) {
    if (const int error = CheckRwlock(rwlock); error != 0) {
        return error;
    }
    std::atomic_ref<std::uint32_t> state(rwlock->state);
    std::uint32_t current = state.load();
    std::uint32_t next = 0;
    do {
        if ((current & URWLOCK_WRITE_OWNER) != 0) {
            next = current & ~URWLOCK_WRITE_OWNER;
        } else if ((current & URWLOCK_MAX_READERS) != 0) {
            next = current - 1;
        } else {
            return GUEST_EPERM;
        }
    } while (!state.compare_exchange_weak(current, next));
    if ((next & URWLOCK_MAX_READERS) != 0) {
        return 0;
    }
    const bool preferReader = (std::atomic_ref<std::uint32_t>(rwlock->flags).load() & URWLOCK_PREFER_READER) != 0;
    const bool writers = (next & URWLOCK_WRITE_WAITERS) != 0;
    const bool readers = (next & URWLOCK_READ_WAITERS) != 0;
    if (writers && (!preferReader || !readers)) {
        WakeAddress(reinterpret_cast<std::uintptr_t>(&rwlock->blockedWriters), 1, false);
    } else if (readers) {
        WakeAddress(reinterpret_cast<std::uintptr_t>(&rwlock->state), std::numeric_limits<std::int32_t>::max(), false);
    }
    return 0;
}

}  // namespace

extern "C" {

int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, KernelUseconds* timeout, const char* name) {
    (void)name;
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait8(std::uint8_t* address, std::uint8_t expected, KernelUseconds* timeout, const char* name) {
    (void)name;
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait16(std::uint16_t* address, std::uint16_t expected, KernelUseconds* timeout, const char* name) {
    (void)name;
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait32(std::uint32_t* address, std::uint32_t expected, KernelUseconds* timeout, const char* name) {
    (void)name;
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait64(std::uint64_t* address, std::uint64_t expected, KernelUseconds* timeout, const char* name) {
    (void)name;
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWake(void* address, std::int32_t count) {
    return WakeAddress(reinterpret_cast<std::uintptr_t>(address), count, true);
}

int APS5_VABI _umtx_op_nid_postfix(void* object, int operation, std::uint64_t value, void* sizeArgument, void* timeArgument) {
    const void* caller = __builtin_return_address(0);
    int error = 0;
    switch (operation) {
        case UMTX_OP_WAIT:
            error = UmtxWait<std::uint64_t>(object, value, sizeArgument, timeArgument, caller);
            break;
        case UMTX_OP_WAIT_UINT:
        case UMTX_OP_WAIT_UINT_PRIVATE:
            error = UmtxWait<std::uint32_t>(object, value, sizeArgument, timeArgument, caller);
            break;
        case UMTX_OP_WAKE:
        case UMTX_OP_WAKE_PRIVATE:
            error = UmtxWake(object, value, operation == UMTX_OP_WAKE_PRIVATE);
            break;
        case UMTX_OP_NWAKE_PRIVATE:
            error = UmtxNwakePrivate(object, value);
            break;
        case UMTX_OP_SEM_WAIT:
            error = SemWait(static_cast<UserSemaphore*>(object), sizeArgument, timeArgument, caller);
            break;
        case UMTX_OP_SEM_WAKE:
            error = SemWake(static_cast<UserSemaphore*>(object));
            break;
        case UMTX_OP_MUTEX_TRYLOCK:
            error = MutexTrylock(static_cast<UserMutex*>(object));
            break;
        case UMTX_OP_MUTEX_LOCK:
        case UMTX_OP_MUTEX_WAIT:
            error = MutexLock(static_cast<UserMutex*>(object), operation == UMTX_OP_MUTEX_LOCK, sizeArgument, timeArgument, caller);
            break;
        case UMTX_OP_MUTEX_UNLOCK:
            error = MutexUnlock(static_cast<UserMutex*>(object));
            break;
        case UMTX_OP_MUTEX_WAKE:
            error = MutexWake(static_cast<UserMutex*>(object));
            break;
        case UMTX_OP_SET_CEILING:
            if (object == nullptr || (std::atomic_ref<std::uint32_t>(static_cast<UserMutex*>(object)->flags).load() & UMUTEX_PRIO_PROTECT) == 0) {
                error = object == nullptr ? GUEST_EFAULT : GUEST_EINVAL;
            } else {
                NotImplemented_nid_no_patch("_umtx_op ceiling of a priority-protected umutex");
            }
            break;
        case UMTX_OP_CV_WAIT:
            error = ConditionWait(static_cast<UserCondition*>(object), static_cast<UserMutex*>(sizeArgument), value, timeArgument, caller);
            break;
        case UMTX_OP_CV_SIGNAL:
        case UMTX_OP_CV_BROADCAST:
            error = ConditionWake(static_cast<UserCondition*>(object), operation == UMTX_OP_CV_BROADCAST);
            break;
        case UMTX_OP_RW_RDLOCK:
        case UMTX_OP_RW_WRLOCK:
            error = RwlockLock(static_cast<UserRwlock*>(object), operation == UMTX_OP_RW_WRLOCK, value, sizeArgument, timeArgument, caller);
            break;
        case UMTX_OP_RW_UNLOCK:
            error = RwlockUnlock(static_cast<UserRwlock*>(object));
            break;
        default:
            if (operation < 0 || operation >= UMTX_OP_MAX) {
                error = GUEST_EINVAL;
            } else if (operation <= UMTX_OP_RESERVED1) {
                error = GUEST_EOPNOTSUPP;
            } else {
                NotImplemented_nid_no_patch(("_umtx_op operation " + std::to_string(operation)).c_str());
            }
            break;
    }
    if (error == 0) {
        return 0;
    }
    *__error_nid_postfix() = error;
    return -1;
}

}
