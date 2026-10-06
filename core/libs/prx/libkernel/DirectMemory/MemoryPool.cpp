#include "prx/libkernel/DirectMemory/MemoryPool.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <map>
#include <mutex>

static constexpr size_t NUM_PAGES = DIRECT_MEMORY_SIZE / PS5_PAGE_SIZE;

struct PhysicalMemoryPool {
    static PhysicalMemoryPool& Instance() {
        static PhysicalMemoryPool inst;
        return inst;
    }

    int Alloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t align = (alignment == 0) ? PS5_PAGE_SIZE : alignment;
        uint64_t start = static_cast<uint64_t>(searchStart);
        uint64_t end = static_cast<uint64_t>(searchEnd);
        uint64_t cur = (start + align - 1) & ~(align - 1);
        while (cur + len <= end && cur + len <= DIRECT_MEMORY_SIZE) {
            if (_isFree(cur, len)) {
                CreateDirectMemoryBacking(static_cast<int64_t>(cur), len, memoryType);
                _mark(cur, len, true);
                _blocks[cur] = {cur + len, memoryType};
                *physOut = static_cast<int64_t>(cur);
                return 0;
            }
            cur += align;
        }
        return SCE_KERNEL_ERROR_EAGAIN;
    }

    void Free(uint64_t start, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        _free(start, len);
    }

    bool CheckedFree(uint64_t start, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (start > DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - start) return false;
        const size_t first = start / PS5_PAGE_SIZE;
        for (size_t i = 0; i < len / PS5_PAGE_SIZE; ++i)
            if (!_used[first + i]) return false;
        _free(start, len);
        return true;
    }

    bool Find(uint64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _blocks.upper_bound(offset);
        if (it != _blocks.begin() && std::prev(it)->second.end > offset) --it;
        else if (!findNext || it == _blocks.end()) return false;
        *start = static_cast<int64_t>(it->first);
        *end = static_cast<int64_t>(it->second.end);
        *memoryType = it->second.type;
        return true;
    }

    size_t FreeRun(uint64_t offset, uint64_t limit) {
        std::lock_guard<std::mutex> lock(_mutex);
        uint64_t cur = offset & ~static_cast<uint64_t>(PS5_PAGE_SIZE - 1);
        size_t run = 0;
        while (cur + PS5_PAGE_SIZE <= limit && cur + PS5_PAGE_SIZE <= DIRECT_MEMORY_SIZE
               && _isFree(cur, PS5_PAGE_SIZE)) {
            cur += PS5_PAGE_SIZE;
            run += PS5_PAGE_SIZE;
        }
        return run;
    }

private:
    void _free(uint64_t start, size_t len) {
        ForgetDirectMemory(static_cast<int64_t>(start), len);
        _mark(start, len, false);
        const uint64_t end = start + len;
        auto it = _blocks.upper_bound(start);
        if (it != _blocks.begin() && std::prev(it)->second.end > start) --it;
        while (it != _blocks.end() && it->first < end) {
            const uint64_t blockStart = it->first;
            const Block block = it->second;
            it = _blocks.erase(it);
            if (blockStart < start) _blocks[blockStart] = {start, block.type};
            if (block.end > end) it = _blocks.emplace(end, Block{block.end, block.type}).first;
        }
    }

    bool _isFree(uint64_t offset, size_t len) const {
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        for (size_t i = 0; i < count; ++i)
            if (_used[first + i]) return false;
        return true;
    }

    void _mark(uint64_t offset, size_t len, bool used) {
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        for (size_t i = 0; i < count; ++i)
            _used[first + i] = used;
    }

    struct Block {
        uint64_t end;
        int type;
    };

    std::mutex _mutex;
    bool _used[NUM_PAGES] = {};
    std::map<uint64_t, Block> _blocks;
};

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
    return PhysicalMemoryPool::Instance().Alloc(searchStart, searchEnd, len, alignment, memoryType, physOut);
}

void DirectMemoryFree(int64_t start, size_t len) {
    PhysicalMemoryPool::Instance().Free(static_cast<uint64_t>(start), len);
}

bool DirectMemoryCheckedFree(int64_t start, size_t len) {
    return PhysicalMemoryPool::Instance().CheckedFree(static_cast<uint64_t>(start), len);
}

bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
    return PhysicalMemoryPool::Instance().Find(static_cast<uint64_t>(offset), findNext, start, end, memoryType);
}

size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit) {
    return PhysicalMemoryPool::Instance().FreeRun(offset, limit);
}
