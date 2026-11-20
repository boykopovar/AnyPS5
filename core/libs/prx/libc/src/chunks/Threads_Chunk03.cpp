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

