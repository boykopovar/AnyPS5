#include <cstdint>
#include <cstddef>
#include <atomic>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EACCES = 0x8002000D;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;
static constexpr int SCE_KERNEL_ERROR_ECANCELED = 0x80020055;

static constexpr uint32_t EVF_ATTR_SINGLE = 0x10;
static constexpr uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr uint32_t EVF_WAITMODE_AND = 0x01;
static constexpr uint32_t EVF_WAITMODE_OR = 0x02;
static constexpr uint32_t EVF_WAITMODE_CLEAR_ALL = 0x10;
static constexpr uint32_t EVF_WAITMODE_CLEAR_PAT = 0x20;

struct WaiterNode {
    uint64_t bitPattern = 0;
    uint32_t waitMode = 0;
    uint64_t resultPattern = 0;
    bool satisfied = false;
    WaiterNode* prev = nullptr;
    WaiterNode* next = nullptr;
};

struct KernelEventFlagPrivate {
    std::mutex _mutex;
    TimedWait::Condition _changed;
    std::string _name;
    uint64_t _pattern = 0;
    bool _multipleWaiters = false;
    bool _deleted = false;
    int _waiters = 0;
    uint64_t _cancelGeneration = 0;
    WaiterNode* _waiterHead = nullptr;
    WaiterNode* _waiterTail = nullptr;

    void pushWaiter(WaiterNode* node) {
        node->prev = _waiterTail;
        node->next = nullptr;
        if (_waiterTail) {
            _waiterTail->next = node;
        } else {
            _waiterHead = node;
        }
        _waiterTail = node;
    }

    void removeWaiter(WaiterNode* node) {
        if (node->prev) {
            node->prev->next = node->next;
        } else if (_waiterHead == node) {
            _waiterHead = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        } else if (_waiterTail == node) {
            _waiterTail = node->prev;
        }
        node->prev = nullptr;
        node->next = nullptr;
    }
};

namespace {

std::mutex eventFlagRegistryMutex;
std::unordered_map<KernelEventFlag, std::shared_ptr<KernelEventFlagPrivate>> eventFlagRegistry;
std::atomic<int> eventFlagApiPauseTarget{0};
std::atomic<int> eventFlagApiPauseEntered{0};
std::atomic<bool> eventFlagApiPauseReleased{true};

void PauseEventFlagApi(int kind) {
    if (eventFlagApiPauseTarget.load(std::memory_order_acquire) != kind) return;
    eventFlagApiPauseEntered.fetch_add(1, std::memory_order_acq_rel);
    while (!eventFlagApiPauseReleased.load(std::memory_order_acquire)) std::this_thread::yield();
}

std::shared_ptr<KernelEventFlagPrivate> ResolveEventFlag(KernelEventFlag handle) {
    if (!handle) return {};
    std::lock_guard lock(eventFlagRegistryMutex);
    const auto it = eventFlagRegistry.find(handle);
    return it == eventFlagRegistry.end() ? std::shared_ptr<KernelEventFlagPrivate>{} : it->second;
}

void RetireEventFlag(KernelEventFlag handle, const std::shared_ptr<KernelEventFlagPrivate>& owner) {
    std::lock_guard lock(eventFlagRegistryMutex);
    const auto it = eventFlagRegistry.find(handle);
    if (it != eventFlagRegistry.end() && it->second == owner) eventFlagRegistry.erase(it);
}

}

static bool TraceSync() {
    static const bool enabled = std::getenv("APS5_TRACE_SYNC") != nullptr;
    return enabled;
}

static bool IsValidWaitMode(uint32_t waitMode) {
    const uint32_t match = waitMode & (EVF_WAITMODE_AND | EVF_WAITMODE_OR);
    const uint32_t clear = waitMode & (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT);
    if ((waitMode & ~(EVF_WAITMODE_AND | EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) != 0) return false;
    return (match == EVF_WAITMODE_AND || match == EVF_WAITMODE_OR) && clear != (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT);
}

static bool IsSatisfied(uint64_t pattern, uint64_t bitPattern, uint32_t waitMode) {
    return (waitMode & EVF_WAITMODE_AND) ? (pattern & bitPattern) == bitPattern : (pattern & bitPattern) != 0;
}

static void Consume(KernelEventFlagPrivate* flag, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPattern) {
    if (resultPattern) *resultPattern = flag->_pattern;
    if (waitMode & EVF_WAITMODE_CLEAR_ALL) flag->_pattern = 0;
    else if (waitMode & EVF_WAITMODE_CLEAR_PAT) flag->_pattern &= ~bitPattern;
}

extern "C" {

int APS5_VABI sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
    (void)param;
    if (!ef || !name) return SCE_KERNEL_ERROR_EINVAL;
    if ((attr & EVF_ATTR_SINGLE) && (attr & EVF_ATTR_MULTI)) return SCE_KERNEL_ERROR_EINVAL;
    if ((attr & ~0x33u) != 0) return SCE_KERNEL_ERROR_EINVAL;
    std::shared_ptr<KernelEventFlagPrivate> flag;
    try {
        flag = std::make_shared<KernelEventFlagPrivate>();
        flag->_name = name;
        flag->_pattern = init_pattern;
        flag->_multipleWaiters = (attr & EVF_ATTR_MULTI) != 0;
        std::lock_guard lock(eventFlagRegistryMutex);
        eventFlagRegistry.emplace(flag.get(), flag);
    } catch (const std::bad_alloc&) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    *ef = flag.get();
    if (TraceSync()) std::fprintf(stderr, "[evf] create %p '%s' attr=0x%x init=0x%llx\n", static_cast<void*>(flag.get()), name, attr, static_cast<unsigned long long>(init_pattern));
    return SCE_OK;
}

int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    std::unique_lock lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    ef->_deleted = true;
    ef->_changed.NotifyAll();
    ef->_changed.Wait(lock, [&] { return ef->_waiters == 0; });
    lock.unlock();
    RetireEventFlag(ef, keepAlive);
    return SCE_OK;
}

int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    PauseEventFlagApi(1);
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    ef->_pattern |= bit_pattern;
    if (TraceSync()) std::fprintf(stderr, "[evf] set %p '%s' |=0x%llx -> 0x%llx\n", static_cast<void*>(ef), ef->_name.c_str(), static_cast<unsigned long long>(bit_pattern), static_cast<unsigned long long>(ef->_pattern));
    bool clearAll = false;
    uint64_t clearMask = 0;
    for (auto* waiter = ef->_waiterHead; waiter != nullptr; waiter = waiter->next) {
        if (!waiter->satisfied && IsSatisfied(ef->_pattern, waiter->bitPattern, waiter->waitMode)) {
            waiter->satisfied = true;
            waiter->resultPattern = ef->_pattern;
            if (waiter->waitMode & EVF_WAITMODE_CLEAR_ALL) {
                clearAll = true;
            } else if (waiter->waitMode & EVF_WAITMODE_CLEAR_PAT) {
                clearMask |= waiter->bitPattern;
            }
        }
    }
    if (clearAll) {
        ef->_pattern = 0;
    } else if (clearMask != 0) {
        ef->_pattern &= ~clearMask;
    }
    ef->_changed.NotifyAll();
    return SCE_OK;
}

int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    ef->_pattern &= bit_pattern;
    return SCE_OK;
}

int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    if (num_wait_threads) *num_wait_threads = ef->_waiters;
    ef->_pattern = set_pattern;
    ++ef->_cancelGeneration;
    ef->_changed.NotifyAll();
    return SCE_OK;
}

int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
    PauseEventFlagApi(2);
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    if (!ef->_multipleWaiters && ef->_waiters != 0) return SCE_KERNEL_ERROR_EPERM;
    if (!IsSatisfied(ef->_pattern, bit_pattern, wait_mode)) {
        if (result_pat) *result_pat = ef->_pattern;
        return SCE_KERNEL_ERROR_EBUSY;
    }
    Consume(ef, bit_pattern, wait_mode, result_pat);
    return SCE_OK;
}

int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    const auto keepAlive = ResolveEventFlag(ef);
    if (!keepAlive) return SCE_KERNEL_ERROR_ESRCH;
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
    std::unique_lock lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    if (!ef->_multipleWaiters && ef->_waiters != 0) return SCE_KERNEL_ERROR_EPERM;
    if (IsSatisfied(ef->_pattern, bit_pattern, wait_mode)) {
        Consume(ef, bit_pattern, wait_mode, result_pat);
        return SCE_OK;
    }
    const uint64_t generation = ef->_cancelGeneration;
    if (TraceSync()) std::fprintf(stderr, "[evf] wait %p '%s' want=0x%llx mode=0x%x have=0x%llx timeout=%d caller=%p outer=%p\n", static_cast<void*>(ef), ef->_name.c_str(), static_cast<unsigned long long>(bit_pattern), wait_mode, static_cast<unsigned long long>(ef->_pattern), timeout ? static_cast<int>(*timeout) : -1, __builtin_return_address(0), static_cast<void**>(static_cast<void**>(__builtin_frame_address(0))[0])[1]);
    ++ef->_waiters;
    WaiterNode waiter;
    waiter.bitPattern = bit_pattern;
    waiter.waitMode = wait_mode;
    ef->pushWaiter(&waiter);
    struct WaiterGuard {
        KernelEventFlagPrivate* flag;
        WaiterNode* node;
        ~WaiterGuard() {
            flag->removeWaiter(node);
            --flag->_waiters;
            flag->_changed.NotifyAll();
        }
    } waiterGuard{ef, &waiter};
    const auto released = [&] { return ef->_deleted || ef->_cancelGeneration != generation || waiter.satisfied; };
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    if (timeout) {
        const std::uint64_t deadline = TimedWait::DeadlineNanos(*timeout);
        timedOut = !ef->_changed.WaitUntil(lock, deadline, released);
        *timeout = static_cast<KernelUseconds>(TimedWait::RemainingMicros(deadline));
    } else {
        ef->_changed.Wait(lock, released);
    }
    KernelTraceWait_nid_postfix("evf", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
    if (ef->_deleted) {
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (ef->_cancelGeneration != generation) {
        if (result_pat) *result_pat = ef->_pattern;
        return SCE_KERNEL_ERROR_ECANCELED;
    }
    if (timedOut && !waiter.satisfied) {
        if (result_pat) *result_pat = ef->_pattern;
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    if (result_pat) *result_pat = waiter.resultPattern;
    return SCE_OK;
}

}

extern "C" int APS5_VABI APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle) {
    const auto keepAlive = ResolveEventFlag(handle);
    if (!keepAlive) return -1;
    std::lock_guard lock(keepAlive->_mutex);
    return keepAlive->_waiters;
}

extern "C" void APS5_VABI APS5_Test_SetEventFlagApiPause(int kind) {
    eventFlagApiPauseEntered.store(0, std::memory_order_relaxed);
    eventFlagApiPauseReleased.store(false, std::memory_order_relaxed);
    eventFlagApiPauseTarget.store(kind, std::memory_order_release);
}

extern "C" int APS5_VABI APS5_Test_GetEventFlagApiPauseCount() {
    return eventFlagApiPauseEntered.load(std::memory_order_acquire);
}

extern "C" void APS5_VABI APS5_Test_ReleaseEventFlagApiPause() {
    eventFlagApiPauseReleased.store(true, std::memory_order_release);
    eventFlagApiPauseTarget.store(0, std::memory_order_release);
}
