#include "prx/libkernel/DirectMemory/MemoryPool.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

#include "prx/libkernel/KernelErrors.hpp"

namespace {

constexpr uint64_t kPage = static_cast<uint64_t>(PS5_PAGE_SIZE);
constexpr uint64_t kPoolBytes = static_cast<uint64_t>(DIRECT_MEMORY_SIZE);
constexpr size_t kNumPages = static_cast<size_t>(kPoolBytes / kPage);

static_assert(kPage != 0 && (kPage & (kPage - 1)) == 0, "PS5_PAGE_SIZE must be a power of two");
static_assert(kPoolBytes % kPage == 0, "DIRECT_MEMORY_SIZE must be a multiple of PS5_PAGE_SIZE");

bool RoundUp(uint64_t v, uint64_t a, uint64_t& out) {
    const uint64_t r = v % a;
    if (r == 0) {
        out = v;
        return true;
    }
    if (v > UINT64_MAX - (a - r)) return false;
    out = v + (a - r);
    return true;
}

bool NormalizeAlignment(size_t alignment, uint64_t& out) {
    return RoundUp(std::max<uint64_t>(static_cast<uint64_t>(alignment), kPage), kPage, out);
}

bool NormalizeLength(size_t len, uint64_t& out) {
    uint64_t rounded;
    if (!RoundUp(static_cast<uint64_t>(len), kPage, rounded) || rounded > kPoolBytes) return false;
    out = rounded;
    return true;
}

bool PageSpan(uint64_t start, size_t len, uint64_t& lo, uint64_t& hi) {
    if (len == 0 || start >= kPoolBytes || len > kPoolBytes - start) return false;
    lo = start & ~(kPage - 1);
    hi = (start + len + kPage - 1) & ~(kPage - 1);
    return true;
}

struct PhysicalMemoryPool {
    static PhysicalMemoryPool& Instance() {
        static auto* inst = new PhysicalMemoryPool;
        return *inst;
    }

    int Alloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
        if (!physOut) return SCE_KERNEL_ERROR_EFAULT;
        if (len == 0) return SCE_KERNEL_ERROR_EINVAL;

        uint64_t align, alignedLen;
        if (!NormalizeAlignment(alignment, align) || !NormalizeLength(len, alignedLen)) return SCE_KERNEL_ERROR_EAGAIN;

        const uint64_t start = static_cast<uint64_t>(searchStart);
        const uint64_t end = std::min<uint64_t>(static_cast<uint64_t>(searchEnd), kPoolBytes);
        uint64_t cur;
        if (start >= end || !RoundUp(start, align, cur)) return SCE_KERNEL_ERROR_EAGAIN;

        std::lock_guard<std::mutex> lock(_mutex);
        const size_t pageCount = static_cast<size_t>(alignedLen / kPage);

        while (cur <= end && alignedLen <= end - cur) {
            size_t lastUsed;
            if (!_lastUsedPage(static_cast<size_t>(cur / kPage), pageCount, lastUsed)) {
                _commit(cur, alignedLen, memoryType);
                *physOut = static_cast<int64_t>(cur);
                return 0;
            }
            if (!RoundUp((static_cast<uint64_t>(lastUsed) + 1) * kPage, align, cur)) break;
        }
        return SCE_KERNEL_ERROR_EAGAIN;
    }

    bool Free(uint64_t start, size_t len) { return _release(start, len, false); }

    bool CheckedFree(uint64_t start, size_t len) { return _release(start, len, true); }

    bool ClaimRelease(uint64_t start, size_t len, bool requireAllocated) {
        uint64_t lo, hi;
        if (!PageSpan(start, len, lo, hi)) return false;
        std::unique_lock<std::mutex> lock(_mutex);
        if (requireAllocated) {
            if (!_allUsed(static_cast<size_t>(lo / kPage), static_cast<size_t>((hi - lo) / kPage)) || _claimed(lo, hi)) return false;
        } else {
            _claimFinished.wait(lock, [&] { return !_claimed(lo, hi); });
        }
        _claims.emplace(lo, hi);
        return true;
    }

    void FinishRelease(uint64_t start, size_t len) {
        uint64_t lo, hi;
        if (!PageSpan(start, len, lo, hi)) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _claims.erase(lo);
            (void)_releaseLocked(lo, hi, false);
        }
        _claimFinished.notify_all();
    }

    void AbandonRelease(uint64_t start, size_t len) noexcept {
        uint64_t lo, hi;
        if (!PageSpan(start, len, lo, hi)) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _claims.erase(lo);
        }
        _claimFinished.notify_all();
    }

    bool Find(uint64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
        if (!start || !end || !memoryType || offset >= kPoolBytes) return false;
        std::lock_guard<std::mutex> lock(_mutex);
        BlockMap::const_iterator it = _blocks.upper_bound(offset);
        if (it != _blocks.begin() && std::prev(it)->second.end > offset) --it;
        else if (!findNext || it == _blocks.end()) return false;

        *start = static_cast<int64_t>(it->first);
        *end = static_cast<int64_t>(it->second.end);
        *memoryType = it->second.type;
        return true;
    }

    bool Retype(uint64_t start, size_t len, int memoryType) {
        uint64_t lo, hi;
        if (!PageSpan(start, len, lo, hi)) return false;

        std::lock_guard<std::mutex> lock(_mutex);
        const bool splitLo = _splitAt(lo);
        try {
            _splitAt(hi);
        } catch (...) {
            if (splitLo) _unsplitAt(lo);
            throw;
        }
        for (BlockMap::iterator it = _blocks.lower_bound(lo); it != _blocks.end() && it->first < hi; ++it) {
            it->second.type = memoryType;
        }
        return true;
    }

    int LargestFree(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* startOut, size_t* sizeOut) {
        if (!startOut || !sizeOut) return SCE_KERNEL_ERROR_EINVAL;
        if (searchStart < 0 || searchEnd <= searchStart) return SCE_KERNEL_ERROR_EAGAIN;
        uint64_t align;
        if (!NormalizeAlignment(alignment, align)) return SCE_KERNEL_ERROR_EAGAIN;

        uint64_t cur;
        if (!RoundUp(static_cast<uint64_t>(searchStart), kPage, cur)) return SCE_KERNEL_ERROR_EAGAIN;
        const uint64_t limit = std::min<uint64_t>(static_cast<uint64_t>(searchEnd), kPoolBytes) & ~(kPage - 1);

        std::lock_guard<std::mutex> lock(_mutex);
        uint64_t bestStart = 0;
        uint64_t bestSize = 0;

        while (cur < limit) {
            if (_used[cur / kPage]) {
                cur += kPage;
                continue;
            }
            uint64_t runEnd = cur;
            while (runEnd < limit && !_used[runEnd / kPage]) runEnd += kPage;

            uint64_t alignedStart;
            if (!RoundUp(cur, align, alignedStart)) break;
            if (alignedStart < runEnd && runEnd - alignedStart > bestSize) {
                bestStart = alignedStart;
                bestSize = runEnd - alignedStart;
            }
            cur = runEnd;
        }

        if (bestSize == 0) return SCE_KERNEL_ERROR_ENOMEM;
        *startOut = static_cast<int64_t>(bestStart);
        *sizeOut = static_cast<size_t>(bestSize);
        return 0;
    }

    size_t FreeRun(uint64_t offset, uint64_t limit) {
        const uint64_t lim = std::min<uint64_t>(limit, kPoolBytes);
        if (offset >= lim) return 0;

        std::lock_guard<std::mutex> lock(_mutex);
        uint64_t cur = offset & ~(kPage - 1);
        size_t run = 0;
        while (cur <= lim && kPage <= lim - cur && !_used[cur / kPage]) {
            cur += kPage;
            run += static_cast<size_t>(kPage);
        }
        return run;
    }

private:
    struct Block {
        uint64_t end;
        int type;
    };
    typedef std::map<uint64_t, Block> BlockMap;

    bool _lastUsedPage(size_t first, size_t count, size_t& out) const {
        for (size_t i = count; i-- > 0;) {
            if (_used[first + i]) {
                out = first + i;
                return true;
            }
        }
        return false;
    }

    bool _allUsed(size_t first, size_t count) const {
        for (size_t i = 0; i < count; ++i) {
            if (!_used[first + i]) return false;
        }
        return true;
    }

    void _mark(size_t first, size_t count, bool used) {
        for (size_t i = 0; i < count; ++i) _used[first + i] = used;
    }

    void _commit(uint64_t start, uint64_t len, int type) {
        std::pair<BlockMap::iterator, bool> ins = _blocks.emplace(start, Block{start + len, type});
        if (!ins.second) throw std::logic_error("DirectMemory pool: block map and page flags disagree");

        try {
            CreateDirectMemoryBacking(static_cast<int64_t>(start), static_cast<size_t>(len), type);
        } catch (...) {
            _blocks.erase(ins.first);
            throw;
        }

        _mark(static_cast<size_t>(start / kPage), static_cast<size_t>(len / kPage), true);
    }

    bool _claimed(uint64_t lo, uint64_t hi) const {
        std::map<uint64_t, uint64_t>::const_iterator it = _claims.lower_bound(hi);
        return it != _claims.begin() && std::prev(it)->second > lo;
    }

    bool _release(uint64_t start, size_t len, bool requireAllocated) {
        uint64_t lo, hi;
        if (!PageSpan(start, len, lo, hi)) return false;
        std::lock_guard<std::mutex> lock(_mutex);
        return _releaseLocked(lo, hi, requireAllocated);
    }

    bool _releaseLocked(uint64_t lo, uint64_t hi, bool requireAllocated) {
        const size_t first = static_cast<size_t>(lo / kPage);
        const size_t count = static_cast<size_t>((hi - lo) / kPage);
        if (requireAllocated && !_allUsed(first, count)) return false;

        const bool splitLo = _splitAt(lo);
        bool splitHi = false;
        try {
            splitHi = _splitAt(hi);
            ForgetDirectMemory(static_cast<int64_t>(lo), static_cast<size_t>(hi - lo));
        } catch (...) {
            if (splitHi) _unsplitAt(hi);
            if (splitLo) _unsplitAt(lo);
            throw;
        }

        _blocks.erase(_blocks.lower_bound(lo), _blocks.lower_bound(hi));
        _mark(first, count, false);
        return true;
    }

    bool _splitAt(uint64_t at) {
        BlockMap::iterator it = _blocks.upper_bound(at);
        if (it == _blocks.begin()) return false;
        --it;
        if (it->first == at || it->second.end <= at) return false;

        const Block tail = {it->second.end, it->second.type};
        _blocks.emplace(at, tail);
        it->second.end = at;
        return true;
    }

    void _unsplitAt(uint64_t at) noexcept {
        BlockMap::iterator right = _blocks.find(at);
        BlockMap::iterator left = std::prev(right);
        left->second.end = right->second.end;
        _blocks.erase(right);
    }

    std::mutex _mutex;
    std::condition_variable _claimFinished;
    bool _used[kNumPages] = {};
    BlockMap _blocks;
    std::map<uint64_t, uint64_t> _claims;
};

}

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
    try {
        return PhysicalMemoryPool::Instance().Alloc(searchStart, searchEnd, len, alignment, memoryType, physOut);
    } catch (const std::bad_alloc&) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }
}

void DirectMemoryFree(int64_t start, size_t len) {
    (void)PhysicalMemoryPool::Instance().Free(static_cast<uint64_t>(start), len);
}

bool DirectMemoryCheckedFree(int64_t start, size_t len) {
    return PhysicalMemoryPool::Instance().CheckedFree(static_cast<uint64_t>(start), len);
}

bool DirectMemoryClaimRelease(int64_t start, size_t len, bool requireAllocated) {
    return PhysicalMemoryPool::Instance().ClaimRelease(static_cast<uint64_t>(start), len, requireAllocated);
}

void DirectMemoryFinishRelease(int64_t start, size_t len) {
    PhysicalMemoryPool::Instance().FinishRelease(static_cast<uint64_t>(start), len);
}

void DirectMemoryAbandonRelease(int64_t start, size_t len) noexcept {
    PhysicalMemoryPool::Instance().AbandonRelease(static_cast<uint64_t>(start), len);
}

bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
    return PhysicalMemoryPool::Instance().Find(static_cast<uint64_t>(offset), findNext, start, end, memoryType);
}

void DirectMemoryRetype(int64_t start, size_t len, int memoryType) {
    (void)PhysicalMemoryPool::Instance().Retype(static_cast<uint64_t>(start), len, memoryType);
}

int DirectMemoryLargestFree(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* startOut, size_t* sizeOut) {
    return PhysicalMemoryPool::Instance().LargestFree(searchStart, searchEnd, alignment, startOut, sizeOut);
}

size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit) {
    return PhysicalMemoryPool::Instance().FreeRun(offset, limit);
}
