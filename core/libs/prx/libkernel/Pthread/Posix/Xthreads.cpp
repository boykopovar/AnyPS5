#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include "../include/Cond.hpp"
#include "../../Time/include/Time.hpp"
#include "Common.hpp"

// Dinkumware C11 thread support as the title's own libc.prx implements it: each _Mtx_t/_Cnd_t is a
// pointer-sized slot holding the sce object, and results are _Thrd_* codes.

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
}

namespace {

constexpr int ThrdSuccess = 0;
constexpr int ThrdNomem = 1;
constexpr int ThrdTimedout = 2;
constexpr int ThrdBusy = 3;
constexpr int ThrdError = 4;
constexpr int MtxPlain = 0x1;
constexpr int MtxRecursive = 0x100;
constexpr int SceMutexRecursive = 2;
constexpr int SceEdeadlk = static_cast<int>(0x8002000Bu);
constexpr int SceEnomem = static_cast<int>(0x8002000Cu);
constexpr int SceEtimedout = static_cast<int>(0x8002003Cu);
constexpr int GuestRealtimeClock = 0;

int initResult(int result) {
    return result == 0 ? ThrdSuccess : result == SceEnomem ? ThrdNomem : ThrdError;
}

// std::_Pad as laid out by the title's libc; _Go() is the first vtable slot.
struct GuestPad {
    unsigned (APS5_VABI* const* vtable)(GuestPad*);
    PthreadCond cond;
    PthreadMutex mutex;
    bool started;
};
static_assert(offsetof(GuestPad, cond) == 0x8 && offsetof(GuestPad, mutex) == 0x10 && offsetof(GuestPad, started) == 0x18);

void* APS5_VABI padEntry(void* arg) {
    auto* pad = static_cast<GuestPad*>(arg);
    pad->vtable[0](pad);
    return nullptr;
}

void require(int result, const char* what) {
    if (result != ThrdSuccess) throw std::runtime_error(what);
}

}

extern "C" {

int APS5_VABI _Mtx_init_nid_postfix(PthreadMutex* mutex, int type) {
    *mutex = nullptr;
    PthreadMutexattr attr{};
    const int attrResult = scePthreadMutexattrInit(&attr);
    if (attrResult != 0) return initResult(attrResult);
    if ((type & MtxRecursive) != 0 && scePthreadMutexattrSettype(&attr, SceMutexRecursive) != 0) {
        scePthreadMutexattrDestroy(&attr);
        return ThrdError;
    }
    const int result = scePthreadMutexInit(mutex, &attr, nullptr);
    scePthreadMutexattrDestroy(&attr);
    return initResult(result);
}

void APS5_VABI _Mtx_destroy_nid_postfix(PthreadMutex* mutex) {
    scePthreadMutexDestroy(mutex);
}

int APS5_VABI _Mtx_lock_nid_postfix(PthreadMutex* mutex) {
    const int result = scePthreadMutexLock(mutex);
    return result == 0 ? ThrdSuccess : result == SceEdeadlk ? ThrdBusy : ThrdError;
}

int APS5_VABI _Mtx_unlock_nid_postfix(PthreadMutex* mutex) {
    scePthreadMutexUnlock(mutex);
    return ThrdSuccess;
}

int APS5_VABI _Cnd_init_nid_postfix(PthreadCond* cond) {
    return initResult(scePthreadCondInit(cond, nullptr, nullptr));
}

void APS5_VABI _Cnd_destroy_nid_postfix(PthreadCond* cond) {
    scePthreadCondDestroy(cond);
}

int APS5_VABI _Cnd_broadcast_nid_postfix(PthreadCond* cond) {
    scePthreadCondBroadcast(cond);
    return ThrdSuccess;
}

// xtime has the KernelTimespec layout and is an absolute gettimeofday deadline.
int APS5_VABI _Cnd_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* deadline) {
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GuestRealtimeClock, deadline, &usec)) throw std::runtime_error("_Cnd_timedwait: invalid xtime");
    const int result = scePthreadCondTimedwait(cond, mutex, usec);
    return result == 0 ? ThrdSuccess : result == SceEtimedout ? ThrdTimedout : ThrdError;
}

int APS5_VABI _Thrd_join_nid_postfix(Pthread thread, int* result) {
    void* value = nullptr;
    if (scePthreadJoin(thread, &value) != 0) return ThrdError;
    if (result) *result = static_cast<int>(reinterpret_cast<std::intptr_t>(value));
    return ThrdSuccess;
}

Pthread APS5_VABI _Thrd_id_nid_postfix() {
    return scePthreadSelf();
}

// Microseconds since the epoch, not the 100 ns ticks of other Dinkumware ports.
std::int64_t APS5_VABI _Xtime_get_ticks_nid_postfix() {
    KernelTimeval now{};
    gettimeofday_nid_postfix(&now, nullptr);
    return now.tv_sec * 1000000 + now.tv_usec;
}

void APS5_VABI _ZNSt4_PadC2Ev_nid_postfix(GuestPad* pad) {
    require(_Cnd_init_nid_postfix(&pad->cond), "std::_Pad: condition variable init failed");
    require(_Mtx_init_nid_postfix(&pad->mutex, MtxPlain), "std::_Pad: mutex init failed");
    pad->started = false;
    require(_Mtx_lock_nid_postfix(&pad->mutex), "std::_Pad: mutex lock failed");
}

void APS5_VABI _ZNSt4_PadD2Ev_nid_postfix(GuestPad* pad) {
    _Mtx_unlock_nid_postfix(&pad->mutex);
    _Mtx_destroy_nid_postfix(&pad->mutex);
    _Cnd_destroy_nid_postfix(&pad->cond);
}

void APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(GuestPad* pad, Pthread* thread) {
    if (scePthreadCreate(thread, nullptr, padEntry, pad, nullptr) != 0) throw std::runtime_error("std::_Pad: thread start failed");
    while (!pad->started) {
        if (scePthreadCondWait(&pad->cond, &pad->mutex) != 0) throw std::runtime_error("std::_Pad: launch wait failed");
    }
}

void APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_postfix(GuestPad* pad) {
    require(_Mtx_lock_nid_postfix(&pad->mutex), "std::_Pad: mutex lock failed");
    pad->started = true;
    scePthreadCondSignal(&pad->cond);
    _Mtx_unlock_nid_postfix(&pad->mutex);
}

}
