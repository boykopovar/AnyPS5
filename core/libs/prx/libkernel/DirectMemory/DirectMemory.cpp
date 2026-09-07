#include "DirectMemory.hpp"
#include "MemoryPool.hpp"
#include <sys/mman.h>
#include <cstdio>

static int LinuxProtFromSce(int prot) {
    int result = PROT_NONE;
    if (prot & 1) result |= PROT_READ;
    if (prot & 2) result |= PROT_WRITE;
    if (prot & 4) result |= PROT_EXEC;
    return result;
}

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    fprintf(stderr, "DoMapDirect: addr=%p len=%zu prot=%d flags=%d physStart=%ld\n", addr ? *addr : nullptr, len, prot, flags, physStart);
    (void)alignment;
    if (!addr || len == 0 || (len & (PS5_PAGE_SIZE - 1)) || physStart < 0
        || (static_cast<uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)))
        return SCE_KERNEL_ERROR_EINVAL;
    constexpr int GUEST_MAP_FIXED = 0x10;
    bool fixed = (flags & GUEST_MAP_FIXED) != 0;
    int mmapFlags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* hint = fixed ? *addr : nullptr;
    if (fixed) mmapFlags |= MAP_FIXED;
    void* result = mmap(hint, len, LinuxProtFromSce(prot), mmapFlags, -1, 0);
    if (result == MAP_FAILED) return SCE_KERNEL_ERROR_ENOMEM;
    fprintf(stderr, "DoMapDirect: mmap result=%p linuxProt=%d\n", result, LinuxProtFromSce(prot));
    *addr = result;
    return 0;
}

int DoMapAnon(void** addr, size_t len, int prot, int flags) {
    fprintf(stderr, "DoMapAnon: addr=%p len=%zu prot=%d flags=%d\n", addr ? *addr : nullptr, len, prot, flags);
    if (!addr || len == 0 || (len & (PS5_PAGE_SIZE - 1))) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int GUEST_MAP_FIXED = 0x10;
    bool fixed = (flags & GUEST_MAP_FIXED) != 0;
    int mmapFlags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* hint = fixed ? *addr : nullptr;
    if (fixed) mmapFlags |= MAP_FIXED;
    void* result = mmap(hint, len, LinuxProtFromSce(prot), mmapFlags, -1, 0);
    if (result == MAP_FAILED) return SCE_KERNEL_ERROR_ENOMEM;
    *addr = result;
    return 0;
}

int DoMprotect(const void* addr, size_t len, int prot) {
    if (mprotect(const_cast<void*>(addr), len, LinuxProtFromSce(prot)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    return 0;
}

int DoMunmap(void* addr, size_t len) {
    munmap(addr, len);
    return 0;
}

int DoReserveVirtual(void** addr, size_t len, size_t alignment) {
    if (!addr || len == 0) return SCE_KERNEL_ERROR_EINVAL;
    size_t allocLen = alignment ? len + alignment : len;
    void* result = mmap(nullptr, allocLen, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) return SCE_KERNEL_ERROR_ENOMEM;
    if (alignment) {
        uintptr_t raw = reinterpret_cast<uintptr_t>(result);
        uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
        if (aligned != raw) munmap(result, aligned - raw);
        uintptr_t end = aligned + len;
        uintptr_t rawEnd = raw + allocLen;
        if (end < rawEnd) munmap(reinterpret_cast<void*>(end), rawEnd - end);
        result = reinterpret_cast<void*>(aligned);
    }
    *addr = result;
    return 0;
}
