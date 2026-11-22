#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AtomicWaitForwards.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint64_t InfiniteMilliseconds = 0xffffffff;
constexpr std::uint64_t LargestMilliseconds = 0x418938;
constexpr std::int32_t WakeAll = 0x7fffffff;
constexpr char WaitName[] = "SceLibcV2AtomicWait";

std::atomic<const AtomicWaitForwards_nid_no_patch*> registeredForwards{nullptr};

const AtomicWaitForwards_nid_no_patch& Kernel(const char* function) {
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel wait on address calls are not registered");
    return *forwards;
}

KernelUseconds Microseconds(std::uint64_t milliseconds) {
    return milliseconds < LargestMilliseconds ? static_cast<KernelUseconds>(milliseconds) * 1000 : 0xffffffff;
}

std::int64_t* CheckedLibcxxAtomic(const std::int64_t* storage, const char* function) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage);
    if (!storage || address % alignof(std::int64_t) != 0)
        throw std::invalid_argument(std::string(function) + ": invalid 64-bit atomic address");
    std::int64_t probe = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&probe, storage, sizeof(probe));
    errno = savedErrno;
    if (!readable)
        throw std::invalid_argument(std::string(function) + ": invalid 64-bit atomic address");
    return const_cast<std::int64_t*>(storage);
}

template<typename TValue, typename TWait>
int Wait(TWait wait, const void* storage, TValue expected, std::uint64_t milliseconds) {
    auto* address = static_cast<TValue*>(const_cast<void*>(storage));
    if (milliseconds == InfiniteMilliseconds) {
        wait(address, expected, nullptr, WaitName);
        return 1;
    }
    KernelUseconds timeout = Microseconds(milliseconds);
    wait(address, expected, &timeout, WaitName);
    return 1;
}

}

extern "C" {

void RegisterAtomicWaitForwards_nid_no_patch(const AtomicWaitForwards_nid_no_patch* forwards) {
    registeredForwards.store(forwards, std::memory_order_release);
}

int APS5_VABI __std_atomic_wait_direct_8_nid_postfix(const void* storage, std::uint8_t expected, std::uint64_t milliseconds) {
    return Wait(Kernel(__func__).wait8, storage, expected, milliseconds);
}

int APS5_VABI __std_atomic_wait_direct_16_nid_postfix(const void* storage, std::uint16_t expected, std::uint64_t milliseconds) {
    return Wait(Kernel(__func__).wait16, storage, expected, milliseconds);
}

int APS5_VABI __std_atomic_wait_direct_32_nid_postfix(const void* storage, std::uint32_t expected, std::uint64_t milliseconds) {
    return Wait(Kernel(__func__).wait32, storage, expected, milliseconds);
}

int APS5_VABI __std_atomic_wait_direct_64_nid_postfix(const void* storage, std::uint64_t expected, std::uint64_t milliseconds) {
    return Wait(Kernel(__func__).wait64, storage, expected, milliseconds);
}

void APS5_VABI __std_atomic_notify_one_direct_nid_postfix(const void* storage) {
    Kernel(__func__).wake(const_cast<void*>(storage), 1);
}

void APS5_VABI __std_atomic_notify_all_direct_nid_postfix(const void* storage) {
    Kernel(__func__).wake(const_cast<void*>(storage), WakeAll);
}

std::int64_t APS5_VABI _ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(const std::int64_t* storage) {
    return std::atomic_ref<std::int64_t>(*CheckedLibcxxAtomic(storage, __func__)).load(std::memory_order_acquire);
}

void APS5_VABI _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(const std::int64_t* storage, std::int64_t expected) {
    __std_atomic_wait_direct_64_nid_postfix(CheckedLibcxxAtomic(storage, __func__), static_cast<std::uint64_t>(expected), InfiniteMilliseconds);
}

void APS5_VABI _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(const std::int64_t* storage) {
    __std_atomic_notify_one_direct_nid_postfix(CheckedLibcxxAtomic(storage, __func__));
}

}
