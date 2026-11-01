#include "AllocationFailure.hpp"
#include <cstdlib>
#include <new>

namespace {

thread_local int allocationCountdown = -1;

}

void AgcDriver::Tests::SetAllocationFailureCountdown(int countdown) noexcept {
    allocationCountdown = countdown;
}

void* operator new(std::size_t bytes) {
    if (allocationCountdown == 0) {
        allocationCountdown = -1;
        throw std::bad_alloc();
    }
    if (allocationCountdown > 0) --allocationCountdown;
    if (auto* allocation = std::malloc(bytes == 0 ? 1 : bytes)) return allocation;
    throw std::bad_alloc();
}

void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete[](void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept { std::free(allocation); }
void operator delete[](void* allocation, std::size_t) noexcept { std::free(allocation); }
