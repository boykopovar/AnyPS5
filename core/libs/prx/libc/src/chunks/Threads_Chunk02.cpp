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

void APS5_VABI _ZNSt4_PadC2EPKc_nid_postfix(GuestPad* pad, const char* name) {
    PadConstructWithName_nid_no_patch(pad, name);
}

void APS5_VABI _ZNSt4_PadD2Ev_nid_postfix(GuestPad* pad) {
    if (!pad) APS5_INVALID_ARG_EX;
    RequireThrd(_Mtx_unlock_nid_postfix(&pad->mutex), "_Pad::~_Pad: _Mtx_unlock");
    _Mtx_destroy_nid_postfix(&pad->mutex);
    _Cnd_destroy_nid_postfix(&pad->condition);

}

void APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_postfix(GuestPad* pad) {
    if (!pad) APS5_INVALID_ARG_EX;
    RequireThrd(_Mtx_lock_nid_postfix(&pad->mutex), "_Pad::_Release: _Mtx_lock");
    pad->started = true;
    RequireThrd(_Cnd_signal_nid_postfix(&pad->condition), "_Pad::_Release: _Cnd_signal");
    RequireThrd(_Mtx_unlock_nid_postfix(&pad->mutex), "_Pad::_Release: _Mtx_unlock");
}

static void LaunchPad_nid_no_patch(GuestPad* pad, const char* name, void** attrSlot, void** thread) {
    if (!pad || !thread) APS5_INVALID_ARG_EX;
    const auto create = registeredPthreadCreate.load(std::memory_order_acquire);
    if (!create) throw std::runtime_error("_Pad::_Launch: pthread_create is not registered");
    const int created = create(thread, attrSlot, PadEntry_nid_no_patch, pad, name);
    if (created != 0) ThrowCppError_nid_no_patch(6);
    while (!pad->started) RequireThrd(_Cnd_wait_nid_postfix(&pad->condition, &pad->mutex), "_Pad::_Launch: _Cnd_wait");
}

void APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(GuestPad* pad, void** thread) {
    LaunchPad_nid_no_patch(pad, nullptr, nullptr, thread);
}

void APS5_VABI PadLaunchWithName_nid_no_patch(GuestPad* pad, const char* name, void** thread) {
    LaunchPad_nid_no_patch(pad, name, nullptr, thread);
}

void APS5_VABI _ZNSt4_Pad7_LaunchEPKcPP7pthread_nid_postfix(GuestPad* pad, const char* name, void** thread) {
    PadLaunchWithName_nid_no_patch(pad, name, thread);
}

void APS5_VABI _ZNSt4_Pad7_LaunchEPP12pthread_attrPP7pthread_nid_postfix(GuestPad* pad, void** attrSlot, void** thread) {
    LaunchPad_nid_no_patch(pad, nullptr, attrSlot, thread);
}

void APS5_VABI PadLaunchWithNameAttr_nid_no_patch(GuestPad* pad, const char* name, void** attrSlot, void** thread) {
    LaunchPad_nid_no_patch(pad, name, attrSlot, thread);
}

void APS5_VABI _ZNSt4_Pad7_LaunchEPKcPP12pthread_attrPP7pthread_nid_postfix(GuestPad* pad, const char* name, void** attrSlot, void** thread) {
    PadLaunchWithNameAttr_nid_no_patch(pad, name, attrSlot, thread);
}

int APS5_VABI _Thrd_join_nid_postfix(void* thread, int* code) {
    const auto join = registeredPthreadJoin.load(std::memory_order_acquire);
    if (!join) throw std::runtime_error("_Thrd_join: pthread_join is not registered");
    void* result = nullptr;
    if (join(thread, &result) != 0) return threadError;
    if (code) *code = static_cast<int>(reinterpret_cast<std::intptr_t>(result));
    return threadSuccess;
}

int APS5_VABI _Thrd_detach_nid_postfix(void* thread) {
    const auto detach = registeredPthreadDetach.load(std::memory_order_acquire);
    if (!detach) throw std::runtime_error("_Thrd_detach: pthread_detach is not registered");
    return detach(thread) == 0 ? threadSuccess : threadError;
}

static Pthread CheckedLibcxxThread(Pthread* thread, const char* function) {
    Pthread value = nullptr;
    if (reinterpret_cast<std::uintptr_t>(thread) % alignof(Pthread) != 0 ||
        !GuestWritable_nid_no_patch(thread, sizeof(Pthread)) ||
        !GuestCopyIn_nid_no_patch(&value, thread, sizeof(value)))
        throw std::invalid_argument(std::string(function) + ": invalid thread address");
    return value;
}

void APS5_VABI _ZNSt3__16thread4joinEv_nid_postfix(Pthread* thread) {
    const auto value = CheckedLibcxxThread(thread, __func__);
    if (!value) throw std::system_error(EINVAL, std::generic_category(), "thread::join failed");
    const auto join = registeredPthreadJoin.load(std::memory_order_acquire);
    if (!join) throw std::runtime_error("thread::join: pthread_join is not registered");
    const int error = join(value, nullptr);
    if (error != 0) throw std::system_error(error, std::generic_category(), "thread::join failed");
    *thread = nullptr;
}

void APS5_VABI _ZNSt3__16thread6detachEv_nid_postfix(Pthread* thread) {
    const auto value = CheckedLibcxxThread(thread, __func__);
    if (!value) throw std::system_error(EINVAL, std::generic_category(), "thread::detach failed");
    const auto detach = registeredPthreadDetach.load(std::memory_order_acquire);
    if (!detach) throw std::runtime_error("thread::detach: pthread_detach is not registered");
    const int error = detach(value);
    if (error != 0) throw std::system_error(error, std::generic_category(), "thread::detach failed");
    *thread = nullptr;
}

void APS5_VABI _ZNSt3__16threadD1Ev_nid_postfix(Pthread* thread) {
    if (CheckedLibcxxThread(thread, __func__)) std::terminate();
}

[[noreturn]] void APS5_VABI _ZSt16_Throw_Cpp_errori_nid_postfix(int code) {
    ThrowCppError_nid_no_patch(code);
}

static std::atomic<const ThreadForwards_nid_no_patch*> registeredThreadForwards{nullptr};

void RegisterThreadForwards_nid_no_patch(const ThreadForwards_nid_no_patch* forwards) {
    registeredThreadForwards.store(forwards, std::memory_order_release);
}

static const ThreadForwards_nid_no_patch& ThreadForwards(const char* function) {
    const auto* forwards = registeredThreadForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel threads are not registered");
    return *forwards;
}

namespace {

constexpr int detachedState = 1;

int StartThread(void** thread, void* attribute, void* (APS5_VABI* entry)(void*), void* argument, const char* name) {
    const auto create = registeredPthreadCreate.load(std::memory_order_acquire);
    if (!create) throw std::runtime_error("_Thrd_start: pthread_create is not registered");
    bool detached = false;
    if (attribute) {
        int state = 0;
        detached = ThreadForwards("_Thrd_start").attrDetachState(attribute, &state) == 0 && state == detachedState;
    }
    const int created = create(thread, attribute, entry, argument, name);
    if (detached) *thread = nullptr;
    return created == 0 ? threadSuccess : threadError;
}

struct StartGate {
    void* (APS5_VABI* entry)(void*);
    void* argument;
    void** condition;
    void** mutex;
    std::atomic<int>* started;
};

void* APS5_VABI StartGateEntry_nid_no_patch(void* argument) {
    auto* gate = static_cast<StartGate*>(argument);
    void** const condition = gate->condition;
    void** const mutex = gate->mutex;
    std::atomic<int>* const started = gate->started;
    void* (APS5_VABI* const entry)(void*) = gate->entry;
    void* const value = gate->argument;
    RequireThrd(_Mtx_lock_nid_postfix(mutex), "_Thrd_create: _Mtx_lock");
    started->store(1, std::memory_order_release);
    RequireThrd(_Cnd_signal_nid_postfix(condition), "_Thrd_create: _Cnd_signal");
    RequireThrd(_Mtx_unlock_nid_postfix(mutex), "_Thrd_create: _Mtx_unlock");
    void* const result = entry(value);
    _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    return result;
}

}

int APS5_VABI _Thrd_start_with_name_attr_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, const char* name, void* attribute) {
    if (!thread || !entry) APS5_INVALID_ARG_EX;
    char first = '\0';
    if (name && !GuestCopyIn_nid_no_patch(&first, name, sizeof(first))) return threadError;
    return StartThread(thread, attribute, entry, argument, first != '\0' ? name : nullptr);
}

int APS5_VABI _Thrd_start_with_name_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, const char* name) {
    return _Thrd_start_with_name_attr_nid_postfix(thread, entry, argument, name, nullptr);
}

int APS5_VABI _Thrd_start_with_attr_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument, void* attribute) {
    return _Thrd_start_with_name_attr_nid_postfix(thread, entry, argument, nullptr, attribute);
}

int APS5_VABI _Thrd_start_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument) {
    return _Thrd_start_with_name_attr_nid_postfix(thread, entry, argument, nullptr, nullptr);
}

int APS5_VABI _Thrd_create_nid_postfix(void** thread, void* (APS5_VABI* entry)(void*), void* argument) {
    if (!thread || !entry) APS5_INVALID_ARG_EX;
    std::atomic<int> started{0};
    void* condition = nullptr;
    void* mutex = nullptr;
    if (_Cnd_init_nid_postfix(&condition) != threadSuccess) return threadError;
    if (_Mtx_init_nid_postfix(&mutex, 1) != threadSuccess) {
        _Cnd_destroy_nid_postfix(&condition);
        return threadError;
    }
    StartGate gate{entry, argument, &condition, &mutex, &started};
    int result = threadError;
    try {
        RequireThrd(_Mtx_lock_nid_postfix(&mutex), "_Thrd_create: _Mtx_lock");
        result = StartThread(thread, nullptr, StartGateEntry_nid_no_patch, &gate, nullptr);
        if (result == threadSuccess)
            while (started.load(std::memory_order_acquire) == 0)
                RequireThrd(_Cnd_wait_nid_postfix(&condition, &mutex), "_Thrd_create: _Cnd_wait");
        RequireThrd(_Mtx_unlock_nid_postfix(&mutex), "_Thrd_create: _Mtx_unlock");
    } catch (...) {
        _Cnd_destroy_nid_postfix(&condition);
        _Mtx_destroy_nid_postfix(&mutex);
        throw;
    }
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
    return result;
}

[[noreturn]] void APS5_VABI _Thrd_abort_nid_postfix(const char* message) {
    fputs_nid_postfix(message ? message : "", __stderrp_nid_postfix);
    fputc_nid_postfix('\n', __stderrp_nid_postfix);
    abort_nid_postfix(0, 0, 0, 0, 0, 0);
}

void APS5_VABI _Call_once_nid_postfix(std::int32_t* flag, void (APS5_VABI* function)(void)) {
    ThreadForwards(__func__).once(flag, function);
}

int APS5_VABI _Tss_create_nid_postfix(int* key, void (APS5_VABI* destructor)(void*)) {
    return ThreadForwards(__func__).keyCreate(key, destructor);
}

int APS5_VABI _Tss_delete_nid_postfix(int key) {
    return ThreadForwards(__func__).keyDelete(key);
}

void* APS5_VABI _Tss_get_nid_postfix(int key) {
    return ThreadForwards(__func__).getSpecific(key);
}

int APS5_VABI _Tss_set_nid_postfix(int key, void* value) {
    return ThreadForwards(__func__).setSpecific(key, value);
}

int APS5_VABI _Thrd_equal_nid_postfix(void* first, void* second) {
    return ThreadForwards(__func__).equal(first, second);
}

int APS5_VABI _Thrd_lt_nid_postfix(void* first, void* second) {
    return reinterpret_cast<std::uintptr_t>(first) < reinterpret_cast<std::uintptr_t>(second);
}

void* APS5_VABI _Thrd_current_nid_postfix(void) {
    return ThreadForwards(__func__).self();
}

void* APS5_VABI _Thrd_id_nid_postfix(void) {
    return ThreadForwards(__func__).self();
}

[[noreturn]] void APS5_VABI _Thrd_exit_nid_postfix(int code) {
    _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    ThreadForwards(__func__).exit(reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<unsigned int>(code))));
    std::abort();
}

int APS5_VABI _Mtx_current_owns_nid_postfix(void**) {
    return 1;
}

static std::atomic<const PthreadSyncForwards_nid_no_patch*> registeredPthreadSyncForwards{nullptr};

void RegisterPthreadSyncForwards_nid_no_patch(const PthreadSyncForwards_nid_no_patch* forwards) {
    registeredPthreadSyncForwards.store(forwards, std::memory_order_release);
}

static const PthreadSyncForwards_nid_no_patch& PthreadSync(const char* function) {
    const auto* forwards = registeredPthreadSyncForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel synchronization is not registered");
    return *forwards;
}

constexpr int KernelDeadlock = static_cast<int>(0x8002000b);
constexpr int KernelNoMemory = static_cast<int>(0x8002000c);
constexpr int KernelBusy = static_cast<int>(0x80020010);
constexpr int RecursiveMutexType = 0x100;
constexpr int KernelRecursiveMutex = 2;

static int CreateResult(int result) {
    if (result == 0) return threadSuccess;
    return result == KernelNoMemory ? threadNomem : threadError;
}

static const char* SyncObjectName(const char* name) {
    return name != nullptr && name[0] != '\0' ? name : nullptr;
}

int APS5_VABI _ZNSt8__sce_v219_Mtx_init_with_nameEPP13pthread_mutexiPKc_nid_postfix(void** mutex, int type, const char* name) {
    const auto& sync = PthreadSync(__func__);
    *mutex = nullptr;
    void* attr = nullptr;
    const int created = sync.mutexattrInit(&attr);
    if (created != 0) return CreateResult(created);
    if ((type & RecursiveMutexType) != 0 && sync.mutexattrSettype(&attr, KernelRecursiveMutex) != 0) {
        sync.mutexattrDestroy(&attr);
        return threadError;
    }
    const int initialized = sync.mutexInit(mutex, &attr, SyncObjectName(name));
    const int released = sync.mutexattrDestroy(&attr);
    if (initialized != 0) return CreateResult(initialized);
    if (released != 0) {
        sync.mutexDestroy(mutex);
        return threadError;
    }
    return threadSuccess;
}

int APS5_VABI _ZNSt8__sce_v29_Mtx_initEPP13pthread_mutexi_nid_postfix(void** mutex, int type) {
    return _ZNSt8__sce_v219_Mtx_init_with_nameEPP13pthread_mutexiPKc_nid_postfix(mutex, type, "SceLibcI_Mtx");
}

int APS5_VABI _ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(void** mutex) {
    return PthreadSync(__func__).mutexDestroy(mutex);
}

int APS5_VABI _ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(void** mutex) {
    const int result = PthreadSync(__func__).mutexLock(mutex);
    if (result == 0) return threadSuccess;
    return result == KernelDeadlock ? threadBusy : threadError;
}

int APS5_VABI _ZNSt8__sce_v212_Mtx_trylockEPP13pthread_mutex_nid_postfix(void** mutex) {
    const int result = PthreadSync(__func__).mutexTrylock(mutex);
    if (result == 0) return threadSuccess;
    return result == KernelBusy ? threadBusy : threadError;
}

int APS5_VABI _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(void** mutex) {
    PthreadSync(__func__).mutexUnlock(mutex);
    return threadSuccess;
}

int APS5_VABI _ZNSt8__sce_v219_Cnd_init_with_nameEPP12pthread_condPKc_nid_postfix(void** condition, const char* name) {
    return CreateResult(PthreadSync(__func__).condInit(condition, nullptr, SyncObjectName(name)));
}

int APS5_VABI _ZNSt8__sce_v29_Cnd_initEPP12pthread_cond_nid_postfix(void** condition) {
    return CreateResult(PthreadSync(__func__).condInit(condition, nullptr, "SceLibcI_Cnd"));
}

int APS5_VABI _ZNSt8__sce_v212_Cnd_destroyEPP12pthread_cond_nid_postfix(void** condition) {
    return PthreadSync(__func__).condDestroy(condition);
}

int APS5_VABI _ZNSt8__sce_v29_Cnd_waitEPP12pthread_condPP13pthread_mutex_nid_postfix(void** condition, void** mutex) {
    PthreadSync(__func__).condWait(condition, mutex);
    return threadSuccess;
}

int APS5_VABI _ZNSt8__sce_v211_Cnd_signalEPP12pthread_cond_nid_postfix(void** condition) {
    PthreadSync(__func__).condSignal(condition);
    return threadSuccess;
}

int APS5_VABI _ZNSt8__sce_v214_Cnd_broadcastEPP12pthread_cond_nid_postfix(void** condition) {
    PthreadSync(__func__).condBroadcast(condition);
    return threadSuccess;
}

struct SceV2AtThreadExitEntry {
    void* thread;
    void** mutex;
    void** condition;
    int* ready;
};

struct SceV2AtThreadExitBlock {
    SceV2AtThreadExitEntry entries[20];
    std::int32_t count;
    SceV2AtThreadExitBlock* next;
};

static_assert(sizeof(SceV2AtThreadExitEntry) == 0x20);
static_assert(offsetof(SceV2AtThreadExitBlock, count) == 0x280 && offsetof(SceV2AtThreadExitBlock, next) == 0x288 && sizeof(SceV2AtThreadExitBlock) == 0x290);

static SceV2AtThreadExitBlock sceV2AtThreadExitEntries {};
static void* sceV2AtThreadExitMutex = nullptr;
static std::once_flag sceV2AtThreadExitInit;

static void** SceV2AtThreadExitMutex() {
    std::call_once(sceV2AtThreadExitInit, [] {
        static_cast<void>(_ZNSt8__sce_v219_Mtx_init_with_nameEPP13pthread_mutexiPKc_nid_postfix(&sceV2AtThreadExitMutex, 0x102, "SceLibc_V2AtThreadExit"));
    });
    return &sceV2AtThreadExitMutex;
}

void APS5_VABI _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix() {
    if (_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(SceV2AtThreadExitMutex()) != threadSuccess) _ZNSt8__sce_v216_Throw_Cpp_errorEi_nid_postfix(5);
}

void APS5_VABI _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix() {
    _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(SceV2AtThreadExitMutex());
}

void APS5_VABI _ZNSt8__sce_v228_Cnd_register_at_thread_exitEPP12pthread_condPP13pthread_mutexPi_nid_postfix(void** condition, void** mutex, int* ready) {
    _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix();
    auto* block = &sceV2AtThreadExitEntries;
    while (block != nullptr && block->count == 20) {
        if (block->next == nullptr) block->next = static_cast<SceV2AtThreadExitBlock*>(std::calloc(1, sizeof(SceV2AtThreadExitBlock)));
        block = block->next;
    }
    if (block != nullptr) {
        for (auto& entry : block->entries) {
            if (entry.mutex != nullptr) continue;
            entry = {_ZNSt8__sce_v28_Thrd_idEv_nid_postfix(), mutex, condition, ready};
            ++block->count;
            break;
        }
    }
    _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix();
}

void APS5_VABI _ZNSt8__sce_v230_Cnd_unregister_at_thread_exitEPP13pthread_mutex_nid_postfix(void** mutex) {
    _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix();
    for (auto* block = &sceV2AtThreadExitEntries; block != nullptr; block = block->next) {
        if (block->count == 0) continue;
        for (auto& entry : block->entries) {
            if (entry.mutex == mutex) {
                entry.mutex = nullptr;
                --block->count;
            }
            if (block->count == 0) break;
        }
    }
    _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix();
}

void APS5_VABI _ZNSt8__sce_v232_Cnd_do_broadcast_at_thread_exitEv_nid_postfix() {
    void* const thread = _ZNSt8__sce_v28_Thrd_idEv_nid_postfix();
    _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix();
    for (auto* block = &sceV2AtThreadExitEntries; block != nullptr; block = block->next) {
        if (block->count == 0) continue;
        for (auto& entry : block->entries) {
            if (entry.mutex != nullptr && entry.thread == thread) {
                if (entry.ready != nullptr) {
                    static_cast<void>(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(entry.mutex));
                    *entry.ready = 1;
                }
                _ZNSt8__sce_v214_Cnd_broadcastEPP12pthread_cond_nid_postfix(entry.condition);
                _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(entry.mutex);
                entry.mutex = nullptr;
                --block->count;
            }
            if (block->count == 0) break;
        }
    }
    _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix();

