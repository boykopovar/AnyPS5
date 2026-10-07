#include <cstdint>
#include <cstddef>
#include <exception>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "DirectMemory.hpp"

namespace {

constexpr size_t kPoolBlockSize = 2ULL * 1024 * 1024;

struct PoolState {
    std::mutex mutex;
    size_t expandedBytes = 0;
    size_t committedBytes = 0;
};

PoolState& Pool() {
    static PoolState* state = new PoolState();
    return *state;
}

bool PageAligned(uint64_t v) { return (v & (PS5_PAGE_SIZE - 1)) == 0; }

int PoolCommit(void* addr, uint64_t len, int prot) {
    if (!addr || len == 0 || !PageAligned(reinterpret_cast<uintptr_t>(addr)) || !PageAligned(len))
        return SCE_KERNEL_ERROR_EINVAL;
    const int ret = DoMprotect(addr, static_cast<size_t>(len), prot);
    if (ret == 0) {
        PoolState& pool = Pool();
        std::lock_guard<std::mutex> lock(pool.mutex);
        pool.committedBytes += static_cast<size_t>(len);
    }
    return ret;
}

int PoolDecommit(void* addr, uint64_t len) {
    if (!addr || len == 0 || !PageAligned(reinterpret_cast<uintptr_t>(addr)) || !PageAligned(len))
        return SCE_KERNEL_ERROR_EINVAL;
    const int ret = DoMprotect(addr, static_cast<size_t>(len), 0);
    if (ret == 0) {
        PoolState& pool = Pool();
        std::lock_guard<std::mutex> lock(pool.mutex);
        pool.committedBytes -=
            static_cast<size_t>(len) < pool.committedBytes ? static_cast<size_t>(len) : pool.committedBytes;
    }
    return ret;
}

template <typename TFunction> int Guarded(TFunction function) {
    try {
        return function();
    } catch (const std::exception&) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

} // namespace

extern "C" {

int APS5_VABI sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out,
                                       int flags) {
    (void)flags;
    if (!entries || num_entries < 0)
        return SCE_KERNEL_ERROR_EINVAL;
    int done = 0;
    int ret = 0;
    for (; done < num_entries; ++done) {
        const KernelMemoryPoolBatchEntry& e = entries[done];
        ret = Guarded([&] {
            switch (e.op) {
            case 1:
                return PoolCommit(e.commit.addr, e.commit.len, e.commit.prot);
            case 2:
                return PoolDecommit(e.decommit.addr, e.decommit.len);
            case 3:
                return DoMprotect(e.protect.addr, static_cast<size_t>(e.protect.len), e.protect.prot);
            case 4:
                return DoMprotect(e.type_protect.addr, static_cast<size_t>(e.type_protect.len), e.type_protect.prot);
            default:
                return SCE_KERNEL_ERROR_EINVAL;
            }
        });
        if (ret != 0)
            break;
    }
    if (num_entries_out)
        *num_entries_out = done;
    return ret;
}

int APS5_VABI sceKernelMemoryPoolCommit(void* addr, size_t len, int type, int prot, int flags) {
    (void)type;
    (void)flags;
    return Guarded([&] { return PoolCommit(addr, len, prot); });
}

int APS5_VABI sceKernelMemoryPoolDecommit(void* addr, size_t len, int flags) {
    (void)flags;
    return Guarded([&] { return PoolDecommit(addr, len); });
}

int APS5_VABI sceKernelMemoryPoolExpand(int64_t search_start, int64_t search_end, size_t len, size_t alignment,
                                        int64_t* phys_addr_out) {
    if (search_start < 0 || search_end <= search_start || len == 0 || !PageAligned(len) || !phys_addr_out ||
        (alignment != 0 && !PageAligned(alignment))) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    const int ret = DirectMemoryAlloc(search_start, search_end, len, alignment, -1, phys_addr_out);
    if (ret == 0) {
        PoolState& pool = Pool();
        std::lock_guard<std::mutex> lock(pool.mutex);
        pool.expandedBytes += len;
    }
    return ret;
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats(KernelMemoryPoolBlockStats* output, size_t output_size) {
    if (!output || output_size < sizeof(KernelMemoryPoolBlockStats))
        return SCE_KERNEL_ERROR_EINVAL;
    PoolState& pool = Pool();
    std::lock_guard<std::mutex> lock(pool.mutex);
    const size_t total = (pool.expandedBytes + kPoolBlockSize - 1) / kPoolBlockSize;
    const size_t used = (pool.committedBytes + kPoolBlockSize - 1) / kPoolBlockSize;
    output->available_flushed_blocks = static_cast<int32_t>(total > used ? total - used : 0);
    output->available_cached_blocks = 0;
    output->allocated_flushed_blocks = static_cast<int32_t>(used);
    output->allocated_cached_blocks = 0;
    return 0;
}

int APS5_VABI sceKernelMemoryPoolReserve(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
    if (!addr_out || len == 0 || !PageAligned(len) || (alignment != 0 && !PageAligned(alignment))) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    *addr_out = addr_in;
    return Guarded([&] { return DoReserveVirtual(addr_out, len, flags, alignment); });
}
}
