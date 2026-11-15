#include "AllocationCounter.hpp"
#include <new>

extern "C" void* __real__Znwy(std::size_t bytes);

namespace {

thread_local bool countingAllocations = false;
thread_local std::size_t countedAllocations = 0;

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

extern "C" void* __wrap__Znwy(std::size_t bytes) {
    if (countingAllocations) ++countedAllocations;
    return __real__Znwy(bytes);
}
