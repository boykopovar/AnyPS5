#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP

#include <cstdint>
#include <cstddef>

#include "prx/libkernel/KernelErrors.hpp"

static constexpr size_t DIRECT_MEMORY_SIZE = 13824ULL * 1024 * 1024;
static constexpr size_t PS5_PAGE_SIZE = 0x4000;

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut);
void DirectMemoryFree(int64_t start, size_t len);
// Drops what the released physical range held and stops its live mappings from saving into it.
void ForgetDirectMemory(int64_t start, size_t len);
// Finds the allocation containing offset, or with findNext the first one above it; false when there is none.
bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType);
int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment);
int DoMapAnon(void** addr, size_t len, int prot, int flags);
int DoMprotect(const void* addr, size_t len, int prot);
int DoMunmap(void* addr, size_t len);
int DoReserveVirtual(void** addr, size_t len, size_t alignment);
// The SCE protection the title last mapped or protected addr with (GPU and AMPR bits included); false when unknown.
bool GuestProtection(uintptr_t addr, int* prot);

#endif