#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/LibcxxPlainMutexForwards.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

using DestroyFn = int (APS5_VABI*)(PthreadMutex*);
using LockFn = int (APS5_VABI*)(PthreadMutex*);
using TrylockFn = int (APS5_VABI*)(PthreadMutex*);
using UnlockFn = int (APS5_VABI*)(PthreadMutex*);

std::atomic<DestroyFn> registeredDestroy{nullptr};
std::atomic<LockFn> registeredLock{nullptr};
std::atomic<TrylockFn> registeredTrylock{nullptr};
std::atomic<UnlockFn> registeredUnlock{nullptr};

template <typename Fn>
Fn RequireForward(const std::atomic<Fn>& slot, const char* function) {
    const Fn fn = slot.load(std::memory_order_acquire);
    if (!fn) {
        throw std::runtime_error(std::string(function) + ": libkernel pthread calls are not registered");
    }
    return fn;
}

PthreadMutex* CheckedMutex(PthreadMutex* self, const char* function) {
    if (reinterpret_cast<std::uintptr_t>(self) % alignof(PthreadMutex) != 0 ||
        !GuestWritable_nid_no_patch(self, sizeof(PthreadMutex)))
        throw std::invalid_argument(std::string(function) + ": invalid mutex address");
    return self;
}

void CheckError(int error, const char* function) {
    if (error != 0) throw std::system_error(error, std::generic_category(), function);
}

}

extern "C" {

void RegisterLibcxxPlainMutexForwards_nid_no_patch(const LibcxxPlainMutexForwards_nid_no_patch* forwards) {
    const bool valid = forwards && forwards->destroy && forwards->lock && forwards->trylock && forwards->unlock;
    registeredDestroy.store(valid ? forwards->destroy : nullptr, std::memory_order_release);
    registeredLock.store(valid ? forwards->lock : nullptr, std::memory_order_release);
    registeredTrylock.store(valid ? forwards->trylock : nullptr, std::memory_order_release);
    registeredUnlock.store(valid ? forwards->unlock : nullptr, std::memory_order_release);
}

void APS5_VABI _ZNSt3__15mutex4lockEv_nid_postfix(PthreadMutex* self) {
    CheckError(RequireForward(registeredLock, __func__)(CheckedMutex(self, __func__)), __func__);
}

void APS5_VABI _ZNSt3__15mutex6unlockEv_nid_postfix(PthreadMutex* self) {
    static_cast<void>(RequireForward(registeredUnlock, __func__)(CheckedMutex(self, __func__)));
}

bool APS5_VABI _ZNSt3__15mutex8try_lockEv_nid_postfix(PthreadMutex* self) {
    return RequireForward(registeredTrylock, __func__)(CheckedMutex(self, __func__)) == 0;
}

void APS5_VABI _ZNSt3__15mutexD1Ev_nid_postfix(PthreadMutex* self) {
    static_cast<void>(RequireForward(registeredDestroy, __func__)(CheckedMutex(self, __func__)));
}

}
