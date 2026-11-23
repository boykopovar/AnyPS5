#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/LibcxxPlainMutexForwards.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::atomic<const LibcxxPlainMutexForwards_nid_no_patch*> registeredForwards{nullptr};

const LibcxxPlainMutexForwards_nid_no_patch& Kernel(const char* function) {
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (!forwards || !forwards->destroy || !forwards->lock || !forwards->trylock || !forwards->unlock) {
        throw std::runtime_error(std::string(function) + ": libkernel pthread calls are not registered");
    }
    return *forwards;
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
    registeredForwards.store(forwards, std::memory_order_release);
}

void APS5_VABI _ZNSt3__15mutex4lockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    CheckError(kernel.lock(CheckedMutex(self, __func__)), __func__);
}

void APS5_VABI _ZNSt3__15mutex6unlockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.unlock(CheckedMutex(self, __func__)));
}

bool APS5_VABI _ZNSt3__15mutex8try_lockEv_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    return kernel.trylock(CheckedMutex(self, __func__)) == 0;
}

void APS5_VABI _ZNSt3__15mutexD1Ev_nid_postfix(PthreadMutex* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.destroy(CheckedMutex(self, __func__)));
}

}
