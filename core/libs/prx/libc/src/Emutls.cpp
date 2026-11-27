#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace {

void* AllocateAligned(std::size_t size, std::size_t alignment) noexcept {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* pointer = nullptr;
    return posix_memalign(&pointer, alignment, size) == 0 ? pointer : nullptr;
#endif
}

struct AlignedDelete {
    void operator()(void* pointer) const noexcept {
#ifdef _WIN32
        _aligned_free(pointer);
#else
        std::free(pointer);
#endif
    }
};

using Allocation = std::unique_ptr<void, AlignedDelete>;

class ThreadInstances {
    struct Entry {
        std::uintptr_t index;
        void* block;
    };

    Entry* entries = nullptr;
    std::size_t count = 0;
    std::size_t capacity = 0;

    std::size_t Position(std::uintptr_t index) const noexcept {
        if (count == 0) return 0;
        const auto* found = std::lower_bound(entries, entries + count, index,
            [](const Entry& entry, std::uintptr_t key) { return entry.index < key; });
        return static_cast<std::size_t>(found - entries);
    }

public:
    ThreadInstances() = default;
    ThreadInstances(const ThreadInstances&) = delete;
    ThreadInstances& operator=(const ThreadInstances&) = delete;

    ~ThreadInstances() {
        for (std::size_t i = 0; i < count; ++i) AlignedDelete{}(entries[i].block);
        std::free(entries);
    }

    void* Find(std::uintptr_t index) const noexcept {
        const auto position = Position(index);
        return position < count && entries[position].index == index ? entries[position].block : nullptr;
    }

    bool Insert(std::uintptr_t index, Allocation& block) noexcept {
        if (count == capacity) {
            if (capacity > std::numeric_limits<std::size_t>::max() / sizeof(Entry) / 2)
                return false;
            const auto newCapacity = capacity == 0 ? 8 : capacity * 2;
            auto* grown = static_cast<Entry*>(std::realloc(entries, newCapacity * sizeof(Entry)));
            if (!grown) return false;
            entries = grown;
            capacity = newCapacity;
        }
        const auto position = Position(index);
        if (position < count)
            std::memmove(entries + position + 1, entries + position, (count - position) * sizeof(Entry));
        entries[position] = {index, block.release()};
        ++count;
        return true;
    }
};

std::mutex controlMutex;
std::uintptr_t nextIndex = 0;
thread_local ThreadInstances instances;

}

extern "C" void* APS5_VABI __emutls_get_address_nid_postfix(GuestEmutlsControl* control) {
    if (control == nullptr) throw std::invalid_argument("__emutls_get_address: null control");
    const int savedErrno = errno;
    GuestEmutlsControl guest{};
    std::uintptr_t index;
    {
        std::lock_guard lock(controlMutex);
        if (!GuestCopyIn_nid_no_patch(&guest, control, sizeof(guest)))
            throw std::invalid_argument("__emutls_get_address: unreadable control");
        const auto alignment = std::max(guest.alignment, static_cast<std::uintptr_t>(sizeof(void*)));
        if (!std::has_single_bit(alignment))
            throw std::invalid_argument("__emutls_get_address: invalid alignment");
        index = guest.index;
        if (index == 0) {
            if (nextIndex == std::numeric_limits<std::uintptr_t>::max())
                throw std::overflow_error("__emutls_get_address: index overflow");
            index = nextIndex + 1;
            if (!GuestCopyOut_nid_no_patch(&control->index, &index, sizeof(index)))
                throw std::invalid_argument("__emutls_get_address: unwritable index");
            nextIndex = index;
        } else {
            nextIndex = std::max(nextIndex, index);
        }
    }

    const auto alignment = std::max(guest.alignment, static_cast<std::uintptr_t>(sizeof(void*)));

    if (void* found = instances.Find(index)) {
        errno = savedErrno;
        return found;
    }

    Allocation block(AllocateAligned(std::max(guest.size, std::uintptr_t{1}), alignment));
    if (!block) {
        errno = savedErrno;
        throw std::bad_alloc{};
    }
    if (guest.initialValue != nullptr) {
        if (!GuestCopyIn_nid_no_patch(block.get(), guest.initialValue, guest.size))
            throw std::invalid_argument("__emutls_get_address: unreadable template");
    } else {
        std::memset(block.get(), 0, guest.size);
    }
    void* result = block.get();
    if (!instances.Insert(index, block)) {
        errno = savedErrno;
        throw std::bad_alloc{};
    }
    errno = savedErrno;
    return result;
}
