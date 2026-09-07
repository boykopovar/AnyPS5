#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_MEMORYPOOL_HPP
#define CORE_LIBS_PRX_LINKERNEL_DIRECTMEMORY_MEMORYPOOL_HPP

#include <cstdint>
#include <cstddef>

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physOut);
void DirectMemoryFree(int64_t start, size_t len);

#endif
