#define _GLIBCXX_HAS_GTHREADS 0
#include "MemoryPool.hpp"
#include "DirectMemory.hpp"
#include <mutex>

static constexpr size_t NUM_PAGES = DIRECT_MEMORY_SIZE / PS5_PAGE_SIZE;

struct PhysicalMemoryPool {
    static PhysicalMemoryPool& Instance() {
        static PhysicalMemoryPool inst;
        return inst;
    }

    int Alloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physOut) {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t align = (alignment == 0) ? PS5_PAGE_SIZE : alignment;
        uint64_t start = static_cast<uint64_t>(searchStart);
        uint64_t end = static_cast<uint64_t>(searchEnd);
        uint64_t cur = (start + align - 1) & ~(align - 1);
        while (cur + len <= end && cur + len <= DIRECT_MEMORY_SIZE) {
            if (_isFree(cur, len)) {
                _mark(cur, len, true);
                *physOut = static_cast<int64_t>(cur);
                return 0;
            }
            cur += align;
        }
        return SCE_KERNEL_ERROR_EAGAIN;
    }

    void Free(uint64_t start, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        _mark(start, len, false);
    }

private:
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

    std::mutex _mutex;
    bool _used[NUM_PAGES] = {};
};

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physOut) {
    return PhysicalMemoryPool::Instance().Alloc(searchStart, searchEnd, len, alignment, physOut);
}

void DirectMemoryFree(int64_t start, size_t len) {
    PhysicalMemoryPool::Instance().Free(static_cast<uint64_t>(start), len);
}
