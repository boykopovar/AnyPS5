#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "DirectMemory.hpp"

namespace {

constexpr size_t kPoolBlockSize = 2ULL * 1024 * 1024;
constexpr int DefinedProtection = 0x3F7;

struct PoolState {
    std::mutex mutex;
    size_t expandedBytes = 0;
    size_t committedBytes = 0;
    std::map<uintptr_t, uintptr_t> committedRanges;
};

PoolState& Pool() {
    static PoolState* state = new PoolState();
    return *state;
}

bool PageAligned(uint64_t v) { return (v & (PS5_PAGE_SIZE - 1)) == 0; }

void AddCommittedRange(PoolState& pool, uintptr_t lo, uintptr_t hi) {
    auto it = pool.committedRanges.lower_bound(lo);
    std::map<uintptr_t, uintptr_t>::iterator target;
    if (it != pool.committedRanges.begin() && std::prev(it)->second >= lo) {
        target = std::prev(it);
    } else if (it != pool.committedRanges.end() && it->first == lo) {
        target = it;
    } else {
        target = pool.committedRanges.emplace_hint(it, lo, lo);
    }

    uintptr_t mergedHi = std::max(hi, target->second);
    size_t overlap = 0;
    if (target->second > lo) {
        overlap += static_cast<size_t>(std::min(hi, target->second) - lo);
    }
    auto cur = std::next(target);
    while (cur != pool.committedRanges.end() && cur->first <= hi) {
        mergedHi = std::max(mergedHi, cur->second);
        if (cur->first < hi) {
            overlap += static_cast<size_t>(std::min(hi, cur->second) - cur->first);
        }
        cur = pool.committedRanges.erase(cur);
    }
    target->second = mergedHi;
    pool.committedBytes += static_cast<size_t>(hi - lo) - overlap;
}

void RemoveCommittedRange(PoolState& pool, uintptr_t lo, uintptr_t hi) {
    auto right = pool.committedRanges.lower_bound(hi);
    if (right != pool.committedRanges.begin()) {
        auto last = std::prev(right);
        if (last->second > hi) {
            pool.committedRanges.emplace_hint(right, hi, last->second);
            last->second = hi;
        }
    }

    auto it = pool.committedRanges.lower_bound(lo);
    if (it != pool.committedRanges.begin() && std::prev(it)->second > lo) {
        --it;
    }
    while (it != pool.committedRanges.end() && it->first < hi) {
        const uintptr_t overlapLo = std::max(lo, it->first);
        const uintptr_t overlapHi = it->second;
        pool.committedBytes -= static_cast<size_t>(overlapHi - overlapLo);
        if (it->first < lo) {
            it->second = lo;
            ++it;
        } else {
            it = pool.committedRanges.erase(it);
        }
    }
}

int PoolCommit(void* addr, uint64_t len, int type, int prot) {
    const uintptr_t lo = reinterpret_cast<uintptr_t>(addr);
    if (!addr || len == 0 || !PageAligned(lo) || !PageAligned(len) || len > UINTPTR_MAX - lo) return SCE_KERNEL_ERROR_EINVAL;
    const int ret = DoMtypeprotect(addr, static_cast<size_t>(len), type, prot);
    if (ret == 0) {
        const uintptr_t hi = lo + static_cast<uintptr_t>(len);
        PoolState& pool = Pool();
        std::lock_guard<std::mutex> lock(pool.mutex);
        AddCommittedRange(pool, lo, hi);
    }
    return ret;
}

int PoolDecommit(void* addr, uint64_t len) {
    const uintptr_t lo = reinterpret_cast<uintptr_t>(addr);
    if (!addr || len == 0 || !PageAligned(lo) || !PageAligned(len) || len > UINTPTR_MAX - lo) return SCE_KERNEL_ERROR_EINVAL;
    const int ret = DoMprotect(addr, static_cast<size_t>(len), 0);
    if (ret == 0) {
        const uintptr_t hi = lo + static_cast<uintptr_t>(len);
        PoolState& pool = Pool();
        std::lock_guard<std::mutex> lock(pool.mutex);
        RemoveCommittedRange(pool, lo, hi);
    }
    return ret;
}

template <typename TFunction>
int Guarded(TFunction function) {
    try {
        return function();
    } catch (const std::exception&) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

}  // namespace

extern "C" {

int APS5_VABI sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)flags;
 if (!entries || num_entries < 0) return SCE_KERNEL_ERROR_EINVAL;
 if (num_entries_out != nullptr) {
  const int savedErrno = errno;
  int initial = 0;
  const bool writable = GuestCopyOut_nid_no_patch(num_entries_out, &initial, sizeof(initial));
  errno = savedErrno;
  if (!writable) return SCE_KERNEL_ERROR_EFAULT;
 }
 int done = 0;
 int ret = 0;
 for (; done < num_entries; ++done) {
  KernelMemoryPoolBatchEntry e;
  {
   const int savedErrno = errno;
   const bool read = GuestCopyIn_nid_no_patch(&e, entries + done, sizeof(e));
   errno = savedErrno;
   if (!read) {
    ret = SCE_KERNEL_ERROR_EFAULT;
    break;
   }
  }
  ret = Guarded([&] {
   switch (e.op) {
    case 1: return PoolCommit(e.commit.addr, e.commit.len, e.commit.type, e.commit.prot);
    case 2: return PoolDecommit(e.decommit.addr, e.decommit.len);
    case 3: return DoMprotect(e.protect.addr, static_cast<size_t>(e.protect.len), e.protect.prot & DefinedProtection);
    case 4: return DoMtypeprotect(e.type_protect.addr, static_cast<size_t>(e.type_protect.len), e.type_protect.type, e.type_protect.prot);
    default: return SCE_KERNEL_ERROR_EINVAL;
   }
  });
  if (ret != 0) break;
 }
 if (num_entries_out != nullptr) {
  const int savedErrno = errno;
  const bool copied = GuestCopyOut_nid_no_patch(num_entries_out, &done, sizeof(done));
  errno = savedErrno;
  if (!copied) return SCE_KERNEL_ERROR_EFAULT;
 }
 return ret;
}

int APS5_VABI sceKernelMemoryPoolCommit(void* addr, size_t len, int type, int prot, int flags) {
 (void)flags;
 return Guarded([&] { return PoolCommit(addr, len, type, prot); });
}

int APS5_VABI sceKernelMemoryPoolDecommit(void* addr, size_t len, int flags) {
 (void)flags;
 return Guarded([&] { return PoolDecommit(addr, len); });
}

int APS5_VABI sceKernelMemoryPoolExpand(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int64_t* phys_addr_out) {
 if (search_start < 0 || search_end <= search_start || len == 0 || !PageAligned(len) || !phys_addr_out
  || (alignment != 0 && !PageAligned(alignment))) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 int64_t physical = 0;
 const int ret = DirectMemoryAlloc(search_start, std::min<int64_t>(search_end, static_cast<int64_t>(DirectMemorySize())), len, alignment, -1, &physical);
 if (ret != 0) return ret;
 if (!GuestCopyOut_nid_no_patch(phys_addr_out, &physical, sizeof(physical))) {
  DirectMemoryFree(physical, len);
  return SCE_KERNEL_ERROR_EFAULT;
 }
 PoolState& pool = Pool();
 std::lock_guard<std::mutex> lock(pool.mutex);
 pool.expandedBytes += len;
 return 0;
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats(KernelMemoryPoolBlockStats* output, size_t output_size) {
 if (!output || output_size < sizeof(KernelMemoryPoolBlockStats)) return SCE_KERNEL_ERROR_EINVAL;
 PoolState& pool = Pool();
 KernelMemoryPoolBlockStats stats{};
 {
  std::lock_guard<std::mutex> lock(pool.mutex);
  const size_t total = (pool.expandedBytes + kPoolBlockSize - 1) / kPoolBlockSize;
  const size_t used = (pool.committedBytes + kPoolBlockSize - 1) / kPoolBlockSize;
  stats.available_flushed_blocks = static_cast<int32_t>(total > used ? total - used : 0);
  stats.allocated_flushed_blocks = static_cast<int32_t>(used);
 }
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(output, &stats, sizeof(stats));
 errno = savedErrno;
 return copied ? 0 : SCE_KERNEL_ERROR_EFAULT;
}

int APS5_VABI sceKernelMemoryPoolReserve(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
 if (!addr_out || len == 0 || !PageAligned(len) || (alignment != 0 && !PageAligned(alignment))) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 const int savedErrno = errno;
 const bool writable = GuestCopyOut_nid_no_patch(addr_out, &addr_in, sizeof(addr_in));
 errno = savedErrno;
 if (!writable) return SCE_KERNEL_ERROR_EFAULT;
 void* reserved = addr_in;
 const int ret = Guarded([&] { return DoReserveVirtual(&reserved, len, flags, alignment); });
 if (ret != 0) return ret;
 const int savedErrnoAfter = errno;
 const bool copied = GuestCopyOut_nid_no_patch(addr_out, &reserved, sizeof(reserved));
 errno = savedErrnoAfter;
 if (!copied) {
  munmap_nid_postfix(reserved, len);
  return SCE_KERNEL_ERROR_EFAULT;
 }
 return 0;
}

}
