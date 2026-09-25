#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
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

struct KernelEventFlagPrivate {
    std::mutex _mutex;
    std::condition_variable _changed;
    std::string _name;
    uint64_t _pattern = 0;
    bool _multipleWaiters = false;
    bool _deleted = false;
    int _waiters = 0;
    uint64_t _cancelGeneration = 0;
};

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
    auto* flag = new (std::nothrow) KernelEventFlagPrivate();
    if (!flag) return SCE_KERNEL_ERROR_ENOMEM;
    flag->_name = name;
    flag->_pattern = init_pattern;
    flag->_multipleWaiters = (attr & EVF_ATTR_MULTI) != 0;
    *ef = flag;
    if (TraceSync()) std::fprintf(stderr, "[evf] create %p '%s' attr=0x%x init=0x%llx\n", static_cast<void*>(flag), name, attr, static_cast<unsigned long long>(init_pattern));
    return SCE_OK;
}

int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    {
        std::lock_guard lock(ef->_mutex);
        if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
        ef->_deleted = true;
        if (ef->_waiters != 0) {
            ef->_changed.notify_all();
            return SCE_OK;
        }
    }
    delete ef;
    return SCE_OK;
}

int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    ef->_pattern |= bit_pattern;
    if (TraceSync()) std::fprintf(stderr, "[evf] set %p '%s' |=0x%llx -> 0x%llx\n", static_cast<void*>(ef), ef->_name.c_str(), static_cast<unsigned long long>(bit_pattern), static_cast<unsigned long long>(ef->_pattern));
    ef->_changed.notify_all();
    return SCE_OK;
}

int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    ef->_pattern &= bit_pattern;
    return SCE_OK;
}

int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    if (num_wait_threads) *num_wait_threads = ef->_waiters;
    ef->_pattern = set_pattern;
    ++ef->_cancelGeneration;
    ef->_changed.notify_all();
    return SCE_OK;
}

int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
    if (!ef) return SCE_KERNEL_ERROR_ESRCH;
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
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
    if (bit_pattern == 0 || !IsValidWaitMode(wait_mode)) return SCE_KERNEL_ERROR_EINVAL;
    std::unique_lock lock(ef->_mutex);
    if (ef->_deleted) return SCE_KERNEL_ERROR_ESRCH;
    if (!ef->_multipleWaiters && ef->_waiters != 0) return SCE_KERNEL_ERROR_EPERM;
    const uint64_t generation = ef->_cancelGeneration;
    if (TraceSync()) std::fprintf(stderr, "[evf] wait %p '%s' want=0x%llx mode=0x%x have=0x%llx timeout=%d caller=%p outer=%p\n", static_cast<void*>(ef), ef->_name.c_str(), static_cast<unsigned long long>(bit_pattern), wait_mode, static_cast<unsigned long long>(ef->_pattern), timeout ? static_cast<int>(*timeout) : -1, __builtin_return_address(0), static_cast<void**>(static_cast<void**>(__builtin_frame_address(0))[0])[1]);
    const auto released = [&] { return ef->_deleted || ef->_cancelGeneration != generation || IsSatisfied(ef->_pattern, bit_pattern, wait_mode); };
    ++ef->_waiters;
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    if (timeout) {
        const auto deadline = waitStart + std::chrono::microseconds(*timeout);
        timedOut = !ef->_changed.wait_until(lock, deadline, released);
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
        *timeout = remaining > 0 ? static_cast<KernelUseconds>(remaining) : 0;
    } else {
        ef->_changed.wait(lock, released);
    }
    KernelTraceWait_nid_postfix("evf", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
    --ef->_waiters;
    if (ef->_deleted) {
        const bool last = ef->_waiters == 0;
        lock.unlock();
        if (last) delete ef;
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (ef->_cancelGeneration != generation) {
        if (result_pat) *result_pat = ef->_pattern;
        return SCE_KERNEL_ERROR_ECANCELED;
    }
    if (timedOut) {
        if (result_pat) *result_pat = ef->_pattern;
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    Consume(ef, bit_pattern, wait_mode, result_pat);
    return SCE_OK;
}

}
