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
int APS5_VABI _ZNSt8__sce_v228_Timespec64_diff_to_usecondsEPK8timespec_nid_postfix(const Xtime* target) {
    if (!target)
        APS5_INVALID_ARG_EX;
    const auto ticks = static_cast<std::uint64_t>(_Xtime_get_ticks_nid_postfix());
    const std::uint64_t nowSeconds = ticks / 1000000;
    const std::int64_t nowNanos = static_cast<std::int64_t>(ticks % 1000000) * 1000;
    std::uint64_t seconds = static_cast<std::uint64_t>(target->sec) + static_cast<std::uint64_t>(target->nsec / nanosPerSecond);
    std::int64_t nanos = target->nsec % nanosPerSecond;
    if (nanos < 0) {
        nanos += nanosPerSecond;
        --seconds;
    }
    if (nanos < nowNanos) {
        nanos += nanosPerSecond;
        --seconds;
    }
    seconds -= nowSeconds;
    nanos -= nowNanos;
    if (static_cast<std::int64_t>(seconds) < 0 || (seconds == 0 && nanos <= 0)) {
        seconds = 0;
        nanos = 0;
    }
    return static_cast<int>(static_cast<std::uint32_t>(seconds) * 1000000u + static_cast<std::uint32_t>((nanos + 999) / 1000));
}

void APS5_VABI _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(Xtime* now) {
    if (!now)
        APS5_INVALID_ARG_EX;
    const auto ticks = static_cast<std::uint64_t>(_Xtime_get_ticks_nid_postfix());
    now->sec = static_cast<std::int64_t>(ticks / 1000000);
    now->nsec = static_cast<std::int64_t>(ticks % 1000000) * 1000;
}

Xtime APS5_VABI _ZNSt8__sce_v222_Timespec64_diff_to_tsEPK8timespec_nid_postfix(const Xtime* target) {
    if (!target)
        APS5_INVALID_ARG_EX;
    constexpr std::uint64_t billion = nanosPerSecond;
    const auto ticks = static_cast<std::uint64_t>(_Xtime_get_ticks_nid_postfix());
    const std::uint64_t nowSeconds = ticks / 1000000;
    const auto nowNanos = static_cast<std::int64_t>(ticks % 1000000) * 1000;
    std::uint64_t seconds = static_cast<std::uint64_t>(target->sec);
    std::uint64_t nanos = static_cast<std::uint64_t>(target->nsec);
    if (static_cast<std::int64_t>(nanos) < 0) {
        const std::uint64_t lowest = 0 - billion;
        const std::uint64_t whole = nanos < lowest ? (lowest - nanos - 1) / billion + 1 : 0;
        seconds -= whole + 1;
        nanos += (whole + 1) * billion;
    }
    if (static_cast<std::int64_t>(nanos) >= nanosPerSecond) {
        const std::uint64_t whole = ((nanos >= 2 * billion - 1 ? nanos - (2 * billion - 1) : 0) + billion - 1) / billion;
        seconds += whole + 1;
        nanos -= (whole + 1) * billion;
    }
    if (static_cast<std::int64_t>(nanos) < nowNanos) {
        seconds += ~nowSeconds;
        nanos += billion;
    } else {
        seconds -= nowSeconds;
    }
    nanos -= static_cast<std::uint64_t>(nowNanos);
    if (static_cast<std::int64_t>(seconds) < 0 || (seconds == 0 && static_cast<std::int64_t>(nanos) <= 0)) return {0, 0};
    return {static_cast<std::int64_t>(seconds), static_cast<std::int64_t>(nanos)};
}

static int SharedMutexResult(int result) {
    if (result == 0) return threadSuccess;
    if ((static_cast<std::uint32_t>(result) & 0xffff0000u) == 0x80020000u) return result & 0xffff;
    return 1;
}

int APS5_VABI _Smtx_init_with_name_nid_postfix(void** smtx, const char* name) {
    return SharedMutexResult(PthreadSync(__func__).rwlockInit(smtx, nullptr, SyncObjectName(name)));
}

int APS5_VABI _Smtx_init_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockInit(smtx, nullptr, "SceLibc_V2Smtx"));
}

int APS5_VABI _Smtx_destroy_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockDestroy(smtx));
}

int APS5_VABI _Smtx_lock_exclusive_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockWrlock(smtx));
}

int APS5_VABI _Smtx_lock_shared_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockRdlock(smtx));
}

int APS5_VABI _Smtx_try_lock_exclusive_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockTrywrlock(smtx));
}

int APS5_VABI _Smtx_try_lock_shared_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockTryrdlock(smtx));
}

int APS5_VABI _Smtx_timedlock_exclusive_nid_postfix(void** smtx, const Xtime* target) {
    const int timeout = _ZNSt8__sce_v228_Timespec64_diff_to_usecondsEPK8timespec_nid_postfix(target);
    return SharedMutexResult(PthreadSync(__func__).rwlockTimedwrlock(smtx, static_cast<unsigned int>(timeout)));
}

int APS5_VABI _Smtx_timedlock_shared_nid_postfix(void** smtx, const Xtime* target) {
    const int timeout = _ZNSt8__sce_v228_Timespec64_diff_to_usecondsEPK8timespec_nid_postfix(target);
    return SharedMutexResult(PthreadSync(__func__).rwlockTimedrdlock(smtx, static_cast<unsigned int>(timeout)));
}

int APS5_VABI _Smtx_unlock_exclusive_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockUnlock(smtx));
}

int APS5_VABI _Smtx_unlock_shared_nid_postfix(void** smtx) {
    return SharedMutexResult(PthreadSync(__func__).rwlockUnlock(smtx));
}

namespace {

struct SharedMutexInstance {
    void* handle;
    std::uint64_t references;
};

std::mutex g_sharedMutexInstancesLock;
std::map<void*, SharedMutexInstance*> g_sharedMutexInstances;

}

void** APS5_VABI __std_acquire_shared_mutex_for_instance_nid_postfix(void* instance) {
    std::lock_guard<std::mutex> guard(g_sharedMutexInstancesLock);
    const auto found = g_sharedMutexInstances.find(instance);
    if (found != g_sharedMutexInstances.end()) {
        ++found->second->references;
        return &found->second->handle;
    }
    SharedMutexInstance* node = new SharedMutexInstance{nullptr, 1};
    if (_Smtx_init_with_name_nid_postfix(&node->handle, "") != 0) {
        delete node;
        throw std::runtime_error("shared mutex for instance: the instance mutex did not initialise");
    }
    g_sharedMutexInstances.emplace(instance, node);
    return &node->handle;
}

void APS5_VABI __std_release_shared_mutex_for_instance_nid_postfix(void* instance) {
    std::lock_guard<std::mutex> guard(g_sharedMutexInstancesLock);
    const auto found = g_sharedMutexInstances.find(instance);
    if (found == g_sharedMutexInstances.end()) return;
    SharedMutexInstance* node = found->second;
    if (--node->references != 0) return;
    _Smtx_destroy_nid_postfix(&node->handle);
    g_sharedMutexInstances.erase(found);
    delete node;
}

int APS5_VABI _ZNSt8__sce_v212_Thrd_detachEP7pthread_nid_postfix(void* thread) {
    return PthreadSync(__func__).detach(thread) == 0 ? threadSuccess : threadError;
}

int APS5_VABI _ZNSt8__sce_v210_Thrd_joinEP7pthreadPi_nid_postfix(void* thread, int* code) {
    void* result = nullptr;
    if (PthreadSync(__func__).join(thread, &result) != 0) return threadError;
    if (code) *code = static_cast<int>(reinterpret_cast<std::intptr_t>(result));
    return threadSuccess;
}

void APS5_VABI _ZNSt8__sce_v211_Thrd_yieldEv_nid_postfix() {
    PthreadSync(__func__).yield();
}

void* APS5_VABI _ZNSt8__sce_v28_Thrd_idEv_nid_postfix() {
    return _Thrd_current_nid_postfix();
}

int APS5_VABI _ZNSt8__sce_v221_Thrd_start_with_nameEPP7pthreadPFPvS3_ES3_PKc_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, const char* name) {
    return _Thrd_start_with_name_nid_postfix(thread, entry, argument, name);
}

int APS5_VABI _ZNSt8__sce_v221_Thrd_start_with_attrEPP7pthreadPFPvS3_ES3_PP12pthread_attr_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, void* attribute) {
    return _Thrd_start_with_attr_nid_postfix(thread, entry, argument, attribute);
}

int APS5_VABI _ZNSt8__sce_v226_Thrd_start_with_name_attrEPP7pthreadPFPvS3_ES3_PKcPP12pthread_attr_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, const char* name, void* attribute) {
    return _Thrd_start_with_name_attr_nid_postfix(thread, entry, argument, name, attribute);
}

