#include "prx/libkernel/DirectMemory/MemoryPool.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <iterator>
#include <map>
#include <mutex>
#include <new>
#include <utility>

#ifndef SCE_KERNEL_ERROR_EINVAL
#define SCE_KERNEL_ERROR_EINVAL 0x80020016
#endif

#ifndef SCE_KERNEL_ERROR_EFAULT
#define SCE_KERNEL_ERROR_EFAULT 0x8002000E
#endif

#ifndef SCE_KERNEL_ERROR_EAGAIN
#define SCE_KERNEL_ERROR_EAGAIN 0x80020023
#endif

#ifndef SCE_KERNEL_ERROR_ENOMEM
#define SCE_KERNEL_ERROR_ENOMEM 0x8002000C
#endif

static constexpr size_t NUM_PAGES = DIRECT_MEMORY_SIZE / PS5_PAGE_SIZE;

struct PhysicalMemoryPool {
    static PhysicalMemoryPool& Instance() {
        static PhysicalMemoryPool inst;
        return inst;
    }

    int Alloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
        if (!physOut) return SCE_KERNEL_ERROR_EFAULT;
        if (len == 0 || searchStart < 0 || searchEnd < 0) return SCE_KERNEL_ERROR_EINVAL;

        uint64_t start = static_cast<uint64_t>(searchStart);
        uint64_t end = static_cast<uint64_t>(searchEnd);

        if (start >= end || start >= DIRECT_MEMORY_SIZE || end > DIRECT_MEMORY_SIZE) return SCE_KERNEL_ERROR_EINVAL;

        size_t align = (alignment == 0) ? PS5_PAGE_SIZE : alignment;
        if ((align & (align - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
        align = std::max<size_t>(align, PS5_PAGE_SIZE);

        if (len > SIZE_MAX - (PS5_PAGE_SIZE - 1)) return SCE_KERNEL_ERROR_EINVAL;
        uint64_t alignedLen = (len + PS5_PAGE_SIZE - 1) & ~static_cast<uint64_t>(PS5_PAGE_SIZE - 1);
        if (alignedLen == 0 || alignedLen > DIRECT_MEMORY_SIZE) return SCE_KERNEL_ERROR_EINVAL;

        if (start > UINT64_MAX - (align - 1)) return SCE_KERNEL_ERROR_EINVAL;

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            const uint64_t mask = ~static_cast<uint64_t>(align - 1);
            uint64_t cur = (start + align - 1) & mask;

            while (cur <= end && alignedLen <= end - cur && alignedLen <= DIRECT_MEMORY_SIZE - cur) {
                if (_isFree(cur, alignedLen)) {
                    const int rc = _commit(cur, alignedLen, memoryType);
                    if (rc == 0) *physOut = static_cast<int64_t>(cur);
                    return rc;
                }
                if (cur > UINT64_MAX - align) break;
                cur += align;
            }
            return SCE_KERNEL_ERROR_EAGAIN;
        } catch (const std::bad_alloc&) {
            return SCE_KERNEL_ERROR_ENOMEM;
        } catch (...) {
            return SCE_KERNEL_ERROR_EFAULT;
        }
    }

    bool Free(uint64_t start, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _release(start, len);
    }

    bool CheckedFree(uint64_t start, size_t len) {
        try {
            std::lock_guard<std::mutex> lock(_mutex);
            return _release(start, len);
        } catch (...) {
            return false;
        }
    }

    bool Find(uint64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
        if (!start || !end || !memoryType || offset >= DIRECT_MEMORY_SIZE) return false;
        try {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _blocks.upper_bound(offset);
            if (it != _blocks.begin() && std::prev(it)->second.end > offset) --it;
            else if (!findNext || it == _blocks.end()) return false;

            *start = static_cast<int64_t>(it->first);
            *end = static_cast<int64_t>(it->second.end);
            *memoryType = it->second.type;
            return true;
        } catch (...) {
            return false;
        }
    }

    bool Retype(uint64_t start, size_t len, int memoryType) {
        if (len == 0 || start >= DIRECT_MEMORY_SIZE) return false;
        if (start & (PS5_PAGE_SIZE - 1)) return false;
        if (len > SIZE_MAX - (PS5_PAGE_SIZE - 1)) return false;

        uint64_t alignedLen = (len + PS5_PAGE_SIZE - 1) & ~static_cast<uint64_t>(PS5_PAGE_SIZE - 1);
        if (alignedLen > DIRECT_MEMORY_SIZE - start) return false;

        std::lock_guard<std::mutex> lock(_mutex);
        const uint64_t end = start + alignedLen;

        if (!_isCovered(start, end)) return false;

        const bool splitStart = _splitAt(start);
        // ISSUE: Ensure exceptions from Retype() are handled at the public API boundary.
        try {
            _splitAt(end);
        } catch (...) {
            if (splitStart) _unsplitAt(start);
            throw;
        }

        for (BlockMap::iterator it = _blocks.find(start); it != _blocks.end() && it->first < end; ++it) {
            it->second.type = memoryType;
        }
        return true;
    }

    size_t FreeRun(uint64_t offset, uint64_t limit) {
        if (offset >= DIRECT_MEMORY_SIZE || offset >= limit || limit > DIRECT_MEMORY_SIZE) return 0;
        std::lock_guard<std::mutex> lock(_mutex);
        uint64_t cur = offset & ~static_cast<uint64_t>(PS5_PAGE_SIZE - 1);
        size_t run = 0;

        while (cur <= limit && PS5_PAGE_SIZE <= limit - cur) {
            if (_isFree(cur, PS5_PAGE_SIZE)) {
                cur += PS5_PAGE_SIZE;
                run += PS5_PAGE_SIZE;
            } else {
                break;
            }
        }
        return run;
    }

private:
    struct Block {
        uint64_t end;
        int type;
    };
    typedef std::map<uint64_t, Block> BlockMap;
    typedef std::map<uint64_t, uint64_t> AllocMap;

    int _commit(uint64_t start, uint64_t len, int type) {
        const uint64_t end = start + len;

        std::pair<AllocMap::iterator, bool> ar = _allocs.emplace(start, end);
        if (!ar.second) return SCE_KERNEL_ERROR_EFAULT;

        std::pair<BlockMap::iterator, bool> br(_blocks.end(), false);
        try {
            br = _blocks.emplace(start, Block{end, type});
        } catch (...) {
            _allocs.erase(ar.first);
            throw;
        }
        if (!br.second) {
            _allocs.erase(ar.first);
            return SCE_KERNEL_ERROR_EFAULT;
        }
        // ISSUE: Verify partial backing-memory changes are rolled back on failure.
        try {
            CreateDirectMemoryBacking(static_cast<int64_t>(start), len, type);
        } catch (...) {
            _blocks.erase(br.first);
            _allocs.erase(ar.first);
            return SCE_KERNEL_ERROR_ENOMEM;
        }

        _mark(start, len, true);
        return 0;
    }

    bool _release(uint64_t start, size_t len) {
        if (len == 0 || start >= DIRECT_MEMORY_SIZE) return false;
        if (start & (PS5_PAGE_SIZE - 1)) return false;

        if (len > SIZE_MAX - (PS5_PAGE_SIZE - 1)) return false;
        uint64_t alignedLen = (len + PS5_PAGE_SIZE - 1) & ~static_cast<uint64_t>(PS5_PAGE_SIZE - 1);
        if (alignedLen > DIRECT_MEMORY_SIZE - start) return false;

        AllocMap::iterator ait = _allocs.find(start);
        if (ait == _allocs.end() || ait->second != start + alignedLen) return false;

        // ISSUE: Verify backing-memory consistency if ForgetDirectMemory() throws.
        ForgetDirectMemory(static_cast<int64_t>(start), alignedLen);

        const uint64_t end = start + alignedLen;
        _blocks.erase(_blocks.lower_bound(start), _blocks.lower_bound(end));
        _allocs.erase(ait);
        _mark(start, alignedLen, false);
        return true;
    }

    bool _isCovered(uint64_t start, uint64_t end) const {
        BlockMap::const_iterator it = _blocks.upper_bound(start);
        if (it == _blocks.begin()) return false;
        --it;

        uint64_t cur = start;
        while (cur < end) {
            if (it == _blocks.end() || it->first > cur || it->second.end <= cur) return false;
            cur = it->second.end;
            ++it;
        }
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

    bool _isFree(uint64_t offset, size_t len) const {
        if (offset >= DIRECT_MEMORY_SIZE || len == 0) return false;
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        if (first > NUM_PAGES || count > NUM_PAGES - first) return false;

        for (size_t i = 0; i < count; ++i) {
            if (_used[first + i]) return false;
        }
        return true;
    }

    void _mark(uint64_t offset, size_t len, bool used) {
        if (offset >= DIRECT_MEMORY_SIZE || len == 0) return;
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        if (first > NUM_PAGES || count > NUM_PAGES - first) return;

        for (size_t i = 0; i < count; ++i) {
            _used[first + i] = used;
        }
    }

    std::mutex _mutex;
    bool _used[NUM_PAGES] = {};
    BlockMap _blocks;
    AllocMap _allocs;
};

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
    if (!physOut) return SCE_KERNEL_ERROR_EFAULT;
    try {
        return PhysicalMemoryPool::Instance().Alloc(searchStart, searchEnd, len, alignment, memoryType, physOut);
    } catch (...) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
}

void DirectMemoryFree(int64_t start, size_t len) {
    if (start < 0 || len == 0) return;
    (void)PhysicalMemoryPool::Instance().Free(static_cast<uint64_t>(start), len);
}

bool DirectMemoryCheckedFree(int64_t start, size_t len) {
    if (start < 0 || len == 0) return false;
    try {
        return PhysicalMemoryPool::Instance().CheckedFree(static_cast<uint64_t>(start), len);
    } catch (...) {
        return false;
    }
}

bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
    if (offset < 0 || !start || !end || !memoryType) return false;
    try {
        return PhysicalMemoryPool::Instance().Find(static_cast<uint64_t>(offset), findNext, start, end, memoryType);
    } catch (...) {
        return false;
    }
}

void DirectMemoryRetype(int64_t start, size_t len, int memoryType) {
    if (start < 0 || len == 0) return;
    (void)PhysicalMemoryPool::Instance().Retype(static_cast<uint64_t>(start), len, memoryType);
}

size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit) {
    if (offset >= DIRECT_MEMORY_SIZE) return 0;
    try {
        return PhysicalMemoryPool::Instance().FreeRun(offset, limit);
    } catch (...) {
        return 0;
    }
}
