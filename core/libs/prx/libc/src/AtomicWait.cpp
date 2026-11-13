#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AtomicWaitForwards.hpp"

#include <atomic>
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

}
