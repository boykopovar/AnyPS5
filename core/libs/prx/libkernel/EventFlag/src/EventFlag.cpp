#include "prx/libSceSaveData/SaveDataCore.hpp"
#include "prx/libkernel/include/Api.hpp"
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
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EACCES = 0x8002000D;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;
static constexpr int SCE_KERNEL_ERROR_EFAULT = 0x8002000E;
static constexpr int SCE_KERNEL_ERROR_ENAMETOOLONG = 0x8002003F;
static constexpr int SCE_KERNEL_ERROR_ECANCELED = 0x80020055;

static constexpr uint32_t EVF_ATTR_TH_FIFO = 0x01;
static constexpr uint32_t EVF_ATTR_TH_PRIO = 0x02;
static constexpr uint32_t EVF_ATTR_SINGLE = 0x10;
static constexpr uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr uint32_t EVF_ATTR_SHARED = 0x100;
static constexpr size_t EVF_NAME_CAPACITY = 32;
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
    bool _shared = false;
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

struct EventFlagHandle {
    std::shared_ptr<KernelEventFlagPrivate> flag;
    bool opened = false;
};

std::shared_mutex eventFlagRegistryMutex;
std::unordered_map<KernelEventFlag, std::shared_ptr<EventFlagHandle>> eventFlagRegistry;

std::shared_ptr<EventFlagHandle> ResolveEventFlag(KernelEventFlag handle) {
    if (!handle) return {};
    std::shared_lock lock(eventFlagRegistryMutex);
    const auto it = eventFlagRegistry.find(handle);
    return it == eventFlagRegistry.end() ? std::shared_ptr<EventFlagHandle>{} : it->second;
}

KernelEventFlag RegisterEventFlag(const std::shared_ptr<EventFlagHandle>& entry) {
    const auto handle = reinterpret_cast<KernelEventFlag>(entry.get());
    eventFlagRegistry.emplace(handle, entry);
    return handle;
}

bool RetireEventFlag(KernelEventFlag handle, const std::shared_ptr<EventFlagHandle>& owner) {
    std::unique_lock lock(eventFlagRegistryMutex);
    const auto it = eventFlagRegistry.find(handle);
    if (it == eventFlagRegistry.end() || it->second != owner) return false;
    eventFlagRegistry.erase(it);
    return true;
}

}

static bool TraceSync() {
    static const bool enabled = std::getenv("APS5_TRACE_SYNC") != nullptr;
    return enabled;
}

static bool IsValidWaitMode(uint32_t waitMode) {
    const uint32_t match = waitMode & (EVF_WAITMODE_AND | EVF_WAITMODE_OR);
    const uint32_t clear = waitMode & (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT);
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
    if ((attr & EVF_ATTR_TH_FIFO) && (attr & EVF_ATTR_TH_PRIO)) return SCE_KERNEL_ERROR_EINVAL;
    if ((attr & ~(EVF_ATTR_TH_FIFO | EVF_ATTR_TH_PRIO | EVF_ATTR_SINGLE | EVF_ATTR_MULTI | EVF_ATTR_SHARED)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (std::strlen(name) >= EVF_NAME_CAPACITY) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    KernelEventFlag handle = nullptr;
    std::shared_ptr<KernelEventFlagPrivate> flag;
    try {
        flag = std::make_shared<KernelEventFlagPrivate>();
        flag->_name = name;
        flag->_pattern = init_pattern;
        flag->_multipleWaiters = (attr & EVF_ATTR_MULTI) != 0;
        flag->_shared = (attr & EVF_ATTR_SHARED) != 0;
        auto entry = std::make_shared<EventFlagHandle>(EventFlagHandle{flag, false});
        std::unique_lock lock(eventFlagRegistryMutex);
        if (flag->_shared) {
            for (const auto& [other, otherEntry] : eventFlagRegistry) {
                if (otherEntry->opened || !otherEntry->flag->_shared) continue;
                std::lock_guard flagLock(otherEntry->flag->_mutex);
                if (!otherEntry->flag->_deleted && otherEntry->flag->_name == flag->_name) return SCE_KERNEL_ERROR_EPERM;
            }
        }
        handle = RegisterEventFlag(entry);
    } catch (const std::bad_alloc&) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    *ef = handle;
    if (TraceSync()) std::fprintf(stderr, "[evf] create %p '%s' attr=0x%x init=0x%llx\n", static_cast<void*>(handle), name, attr, static_cast<unsigned long long>(init_pattern));
    return SCE_OK;
}

int APS5_VABI sceKernelOpenEventFlag(KernelEventFlag* ef, const char* name) {
    if (!name) return SCE_KERNEL_ERROR_EFAULT;
    if (std::strlen(name) >= EVF_NAME_CAPACITY) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    try {
        std::unique_lock lock(eventFlagRegistryMutex);
        std::shared_ptr<KernelEventFlagPrivate> target;
        for (const auto& [handle, entry] : eventFlagRegistry) {
            if (entry->opened || !entry->flag->_shared) continue;
            std::lock_guard flagLock(entry->flag->_mutex);
            if (entry->flag->_deleted || entry->flag->_name != name) continue;
            target = entry->flag;
            break;
        }
        if (!target) return SCE_KERNEL_ERROR_ESRCH;
        if (!ef) return SCE_KERNEL_ERROR_EFAULT;
        *ef = RegisterEventFlag(std::make_shared<EventFlagHandle>(EventFlagHandle{std::move(target), true}));
    } catch (const std::bad_alloc&) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    return SCE_OK;
}

int APS5_VABI sceKernelCloseEventFlag(KernelEventFlag ef) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    if (!entry->opened) return SCE_KERNEL_ERROR_EPERM;
    return RetireEventFlag(ef, entry) ? SCE_OK : SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    if (entry->opened) return SCE_KERNEL_ERROR_EPERM;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::unique_lock lock(flag->_mutex);
    if (flag->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    flag->_deleted = true;
    flag->_changed.NotifyAll();
    flag->_changed.Wait(lock, [&] { return flag->_waiters == 0; });
    lock.unlock();
    RetireEventFlag(ef, entry);
    return SCE_OK;
}

int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::lock_guard lock(flag->_mutex);
    if (flag->_deleted) return entry->opened ? SCE_OK : SCE_KERNEL_ERROR_ESRCH;
    flag->_pattern |= bit_pattern;
    if (TraceSync()) std::fprintf(stderr, "[evf] set %p '%s' |=0x%llx -> 0x%llx\n", static_cast<void*>(ef), flag->_name.c_str(), static_cast<unsigned long long>(bit_pattern), static_cast<unsigned long long>(flag->_pattern));
    for (auto* waiter = flag->_waiterHead; waiter != nullptr; waiter = waiter->next) {
        if (!waiter->satisfied && IsSatisfied(flag->_pattern, waiter->bitPattern, waiter->waitMode)) {
            waiter->satisfied = true;
            waiter->resultPattern = flag->_pattern;
            if (waiter->waitMode & EVF_WAITMODE_CLEAR_ALL) {
                flag->_pattern = 0;
            } else if (waiter->waitMode & EVF_WAITMODE_CLEAR_PAT) {
                flag->_pattern &= ~waiter->bitPattern;
            }
        }
    }
    flag->_changed.NotifyAll();
    return SCE_OK;
}

int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::lock_guard lock(flag->_mutex);
    if (flag->_deleted) return entry->opened ? SCE_OK : SCE_KERNEL_ERROR_ESRCH;
    flag->_pattern &= bit_pattern;
    return SCE_OK;
}

int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::lock_guard lock(flag->_mutex);
    if (flag->_deleted) {
        if (!entry->opened) return SCE_KERNEL_ERROR_ESRCH;
        if (num_wait_threads) *num_wait_threads = 0;
        return SCE_OK;
    }
    if (num_wait_threads) *num_wait_threads = flag->_waiters;
    flag->_pattern = set_pattern;
    ++flag->_cancelGeneration;
    flag->_changed.NotifyAll();
    return SCE_OK;
}

int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::lock_guard lock(flag->_mutex);
    if (flag->_deleted) {
        if (!entry->opened) return SCE_KERNEL_ERROR_ESRCH;
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (!flag->_multipleWaiters && flag->_waiters != 0) {
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_EPERM;
    }
    if (!IsSatisfied(flag->_pattern, bit_pattern, wait_mode)) {
        if (result_pat) *result_pat = flag->_pattern;
        return SCE_KERNEL_ERROR_EBUSY;
    }
    Consume(flag, bit_pattern, wait_mode, result_pat);
    return SCE_OK;
}

int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
    const auto entry = ResolveEventFlag(ef);
    if (!entry) return SCE_KERNEL_ERROR_ESRCH;
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
    KernelEventFlagPrivate* flag = entry->flag.get();
    std::unique_lock lock(flag->_mutex);
    if (flag->_deleted) {
        if (!entry->opened) return SCE_KERNEL_ERROR_ESRCH;
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (!flag->_multipleWaiters && flag->_waiters != 0) {
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_EPERM;
    }
    if (IsSatisfied(flag->_pattern, bit_pattern, wait_mode)) {
        Consume(flag, bit_pattern, wait_mode, result_pat);
        return SCE_OK;
    }
    const uint64_t generation = flag->_cancelGeneration;
    if (TraceSync()) std::fprintf(stderr, "[evf] wait %p '%s' want=0x%llx mode=0x%x have=0x%llx timeout=%d caller=%p outer=%p\n", static_cast<void*>(ef), flag->_name.c_str(), static_cast<unsigned long long>(bit_pattern), wait_mode, static_cast<unsigned long long>(flag->_pattern), timeout ? static_cast<int>(*timeout) : -1, __builtin_return_address(0), static_cast<void**>(static_cast<void**>(__builtin_frame_address(0))[0])[1]);
    ++flag->_waiters;
    WaiterNode waiter;
    waiter.bitPattern = bit_pattern;
    waiter.waitMode = wait_mode;
    flag->pushWaiter(&waiter);
    struct WaiterGuard {
        KernelEventFlagPrivate* flag;
        WaiterNode* node;
        ~WaiterGuard() {
            flag->removeWaiter(node);
            --flag->_waiters;
            flag->_changed.NotifyAll();
        }
    } waiterGuard{flag, &waiter};
    const auto released = [&] { return flag->_deleted || flag->_cancelGeneration != generation || waiter.satisfied; };
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    if (timeout) {
        const std::uint64_t deadline = TimedWait::DeadlineNanos(*timeout);
        timedOut = !flag->_changed.WaitUntil(lock, deadline, released);
        *timeout = static_cast<KernelUseconds>(TimedWait::RemainingMicros(deadline));
    } else {
        flag->_changed.Wait(lock, released);
    }
    KernelTraceWait_nid_postfix("evf", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
    if (flag->_deleted) {
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (flag->_cancelGeneration != generation) {
        if (result_pat) *result_pat = 0;
        return SCE_KERNEL_ERROR_ECANCELED;
    }
    if (timedOut && !waiter.satisfied) {
        if (result_pat) *result_pat = flag->_pattern;
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    if (result_pat) *result_pat = waiter.resultPattern;
    return SCE_OK;
}

}

extern "C" int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle) {
    const auto entry = ResolveEventFlag(handle);
    if (!entry) return -1;
    std::lock_guard lock(entry->flag->_mutex);
    return entry->flag->_waiters;
}
