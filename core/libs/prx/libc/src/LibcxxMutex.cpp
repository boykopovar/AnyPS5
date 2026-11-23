#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/LibcxxMutexForwards.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::atomic<const LibcxxMutexForwards_nid_no_patch*> registeredForwards{nullptr};

const LibcxxMutexForwards_nid_no_patch& Kernel(const char* function) {
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel pthread calls are not registered");
    return *forwards;
}

PthreadMutex* CheckedMutex(PthreadMutex* self, const char* function) {
    if (reinterpret_cast<std::uintptr_t>(self) % alignof(PthreadMutex) != 0 ||
        !GuestWritable_nid_no_patch(self, sizeof(PthreadMutex)))
        throw std::invalid_argument(std::string(function) + ": invalid recursive mutex address");
    return self;
}

void CheckError(int error, const char* function) {
    if (error != 0) throw std::system_error(error, std::generic_category(), function);
}

}

extern "C" {

void RegisterLibcxxMutexForwards_nid_no_patch(const LibcxxMutexForwards_nid_no_patch* forwards) {
    registeredForwards.store(forwards, std::memory_order_release);
}

void APS5_VABI _ZNSt3__115recursive_mutexC1Ev_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    CheckedMutex(self, __func__);
    PthreadMutexattr attr = nullptr;
    CheckError(kernel.mutexAttrInit(&attr), __func__);
    const int typeError = kernel.mutexAttrSettype(&attr, 2);
    if (typeError != 0) {
        kernel.mutexAttrDestroy(&attr);
        CheckError(typeError, __func__);
    }
    const int initError = kernel.mutexInit(self, &attr);
    const int destroyError = kernel.mutexAttrDestroy(&attr);
    CheckError(initError, __func__);
    if (destroyError != 0) {
        kernel.mutexDestroy(self);
        CheckError(destroyError, __func__);
    }
}

void APS5_VABI _ZNSt3__115recursive_mutexD1Ev_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.mutexDestroy(CheckedMutex(self, __func__)));
}

void APS5_VABI _ZNSt3__115recursive_mutex4lockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    CheckError(kernel.mutexLock(CheckedMutex(self, __func__)), __func__);
}

bool APS5_VABI _ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    return kernel.mutexTrylock(CheckedMutex(self, __func__)) == 0;
}

void APS5_VABI _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.mutexUnlock(CheckedMutex(self, __func__)));
}

}
