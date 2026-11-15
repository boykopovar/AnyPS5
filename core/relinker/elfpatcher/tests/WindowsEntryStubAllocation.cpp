#include <cstddef>
#include <cstdlib>
#include <new>

namespace WindowsEntryStubAllocation {
std::size_t largest = 0;
}

void* operator new(std::size_t size) {
    if (size > WindowsEntryStubAllocation::largest) WindowsEntryStubAllocation::largest = size;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
