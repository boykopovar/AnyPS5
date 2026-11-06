#include <cstddef>
#include <cstdlib>
#include <new>

namespace IrRewriteAllocation {
std::ptrdiff_t failureAfter = -1;
std::size_t allocations = 0;
bool countAllocations = false;
}

void* operator new(std::size_t size) {
    if (IrRewriteAllocation::failureAfter == 0) throw std::bad_alloc();
    if (IrRewriteAllocation::failureAfter > 0) --IrRewriteAllocation::failureAfter;
    if (IrRewriteAllocation::countAllocations) ++IrRewriteAllocation::allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
