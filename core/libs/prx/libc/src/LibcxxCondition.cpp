#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/LibcxxConditionForwards.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::atomic<const LibcxxConditionForwards_nid_no_patch*> registeredForwards{nullptr};

const LibcxxConditionForwards_nid_no_patch& Kernel(const char* function) {
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel pthread calls are not registered");
    return *forwards;
}

PthreadCond* CheckedCondition(PthreadCond* self, const char* function) {
    if (reinterpret_cast<std::uintptr_t>(self) % alignof(PthreadCond) != 0 ||
        !GuestWritable_nid_no_patch(self, sizeof(PthreadCond)))
        throw std::invalid_argument(std::string(function) + ": invalid condition variable address");
    return self;
}

struct GuestUniqueLock {
    PthreadMutex* mutex;
    bool owns;
    std::uint8_t padding[7];
};
static_assert(sizeof(GuestUniqueLock) == 16);

PthreadMutex* CheckedLock(const void* lock, const char* function) {
    GuestUniqueLock value{};
    if (reinterpret_cast<std::uintptr_t>(lock) % alignof(GuestUniqueLock) != 0 ||
        !GuestCopyIn_nid_no_patch(&value, lock, sizeof(value)) || !value.owns ||
        reinterpret_cast<std::uintptr_t>(value.mutex) % alignof(PthreadMutex) != 0 ||
        !GuestWritable_nid_no_patch(value.mutex, sizeof(PthreadMutex)))
        throw std::invalid_argument(std::string(function) + ": unique_lock does not own a valid mutex");
    return value.mutex;
}

void CheckError(int error, const char* function) {
    if (error != 0) throw std::system_error(error, std::generic_category(), function);
}

}

extern "C" {

void RegisterLibcxxConditionForwards_nid_no_patch(const LibcxxConditionForwards_nid_no_patch* forwards) {
    registeredForwards.store(forwards, std::memory_order_release);
}

void APS5_VABI _ZNSt3__118condition_variable10notify_allEv_nid_postfix(PthreadCond* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.broadcast(CheckedCondition(self, __func__)));
}

void APS5_VABI _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(PthreadCond* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.signal(CheckedCondition(self, __func__)));
}

void APS5_VABI _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(PthreadCond* self, const void* lock) {
    auto& kernel = Kernel(__func__);
    CheckError(kernel.wait(CheckedCondition(self, __func__), CheckedLock(lock, __func__)), __func__);
}

void APS5_VABI _ZNSt3__118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE_nid_postfix(PthreadCond* self, const void* lock, std::int64_t nanoseconds) {
    auto& kernel = Kernel(__func__);
    constexpr std::int64_t billion = 1000000000;
    std::int64_t seconds = nanoseconds / billion;
    std::int64_t remainder = nanoseconds % billion;
    if (remainder < 0) {
        --seconds;
        remainder += billion;
    }
    const KernelTimespec deadline = seconds < 0 ? KernelTimespec{0, 0} : KernelTimespec{seconds, remainder};
    const int error = kernel.timedwait(CheckedCondition(self, __func__), CheckedLock(lock, __func__), &deadline);
    if (error != 60) CheckError(error, __func__);
}

void APS5_VABI _ZNSt3__118condition_variableD1Ev_nid_postfix(PthreadCond* self) {
    auto& kernel = Kernel(__func__);
    static_cast<void>(kernel.destroy(CheckedCondition(self, __func__)));
}

}
