#include "AllocationCounter.hpp"
#include <cstdint>
#include <new>

extern "C" void* __real__Znwy(std::size_t bytes);

namespace {

thread_local bool countingAllocations = false;
thread_local std::size_t countedAllocations = 0;
thread_local std::size_t allocationsBeforeFailure = SIZE_MAX;

}

extern "C" void BeginAllocationCount_nid_no_patch() noexcept {
    countedAllocations = 0;
    countingAllocations = true;
}

extern "C" std::size_t EndAllocationCount_nid_no_patch() noexcept {
    countingAllocations = false;
    return countedAllocations;
}

extern "C" void AllocateForCount_nid_no_patch(std::size_t bytes) {
    void* (*volatile allocate)(std::size_t) = &::operator new;
    void* allocation = allocate(bytes);
    ::operator delete(allocation);
}

extern "C" void FailAllocationsAfter_nid_no_patch(std::size_t allocations) noexcept {
    allocationsBeforeFailure = allocations;
}

extern "C" void StopAllocationFailures_nid_no_patch() noexcept {
    allocationsBeforeFailure = SIZE_MAX;
}

extern "C" void* __wrap__Znwy(std::size_t bytes) {
    if (allocationsBeforeFailure != SIZE_MAX) {
        if (allocationsBeforeFailure == 0) throw std::bad_alloc();
        --allocationsBeforeFailure;
    }
    if (countingAllocations) ++countedAllocations;
    return __real__Znwy(bytes);
}
