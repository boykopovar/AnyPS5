#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP

#include <cstdint>
#include <cstddef>

static constexpr size_t DIRECT_MEMORY_SIZE = 13824ULL * 1024 * 1024;
static constexpr size_t PS5_PAGE_SIZE = 0x4000;

static constexpr int SCE_KERNEL_ERROR_EINVAL = -2147418107;
static constexpr int SCE_KERNEL_ERROR_EAGAIN = -2147418110;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = -2147418105;
static constexpr int SCE_KERNEL_ERROR_EACCES = -2147418108;
static constexpr int SCE_KERNEL_ERROR_EFAULT = -2147418103;

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physOut);
void DirectMemoryFree(int64_t start, size_t len);
int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment);
int DoMapAnon(void** addr, size_t len, int prot, int flags);
int DoMprotect(const void* addr, size_t len, int prot);
int DoMunmap(void* addr, size_t len);
int DoReserveVirtual(void** addr, size_t len, size_t alignment);

#endif