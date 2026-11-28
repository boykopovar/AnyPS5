#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

namespace LibcGuard {
class ActiveStack {
    static constexpr std::size_t InlineCapacity = 64;
    std::uint64_t* inlineSlots[InlineCapacity] {};
    std::uint64_t** overflow {};
    std::size_t depth {};
    std::size_t capacity {InlineCapacity};

    std::uint64_t** Data() { return overflow ? overflow : inlineSlots; }
    std::uint64_t* const* Data() const { return overflow ? overflow : inlineSlots; }

public:
    ActiveStack() = default;
    ActiveStack(const ActiveStack&) = delete;
    ActiveStack& operator=(const ActiveStack&) = delete;
    ~ActiveStack() { std::free(overflow); }

    bool Contains(const std::uint64_t* guardObject) const {
        std::uint64_t* const* items = Data();
        for (std::size_t i = 0; i < depth; ++i)
            if (items[i] == guardObject) return true;
        return false;
    }

    bool Push(std::uint64_t* guardObject) {
        if (depth == capacity) {
            if (capacity > std::numeric_limits<std::size_t>::max() / 2 / sizeof(std::uint64_t*)) return false;
            const std::size_t nextCapacity = capacity * 2;
            auto* grown = static_cast<std::uint64_t**>(std::realloc(overflow, nextCapacity * sizeof(std::uint64_t*)));
            if (!grown) return false;
            if (!overflow) std::memcpy(grown, inlineSlots, InlineCapacity * sizeof(std::uint64_t*));
            overflow = grown;
            capacity = nextCapacity;
        }
        Data()[depth++] = guardObject;
        return true;
    }

    bool Remove(const std::uint64_t* guardObject) {
        std::uint64_t** items = Data();
        for (std::size_t i = depth; i > 0; --i) {
            const std::size_t idx = i - 1;
            if (items[idx] == guardObject) {
                if (idx != --depth) items[idx] = items[depth];
                return true;
            }
        }
        return false;
    }
};

thread_local ActiveStack stack {};
}

extern "C" {

int APS5_VABI __cxa_guard_acquire_nid_postfix(std::uint64_t* guardObject) {
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char> initialized(bytes[0]);
    std::atomic_ref<unsigned char> lock(bytes[1]);
    if (initialized.load(std::memory_order_acquire)) return 0;
    if (LibcGuard::stack.Contains(guardObject)) LibcException::Terminate();
    for (;;) {
        unsigned char expected = 0;
        if (lock.compare_exchange_weak(expected, 1, std::memory_order_acquire)) break;
        if (initialized.load(std::memory_order_acquire)) return 0;
        std::this_thread::yield();
    }
    if (initialized.load(std::memory_order_acquire)) { lock.store(0, std::memory_order_release); return 0; }
    if (!LibcGuard::stack.Push(guardObject)) {
        lock.store(0, std::memory_order_release);
        LibcException::Terminate();
    }
    return 1;
}

void APS5_VABI __cxa_guard_release_nid_postfix(std::uint64_t* guardObject) {
    if (!LibcGuard::stack.Remove(guardObject)) LibcException::Terminate();
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char>(bytes[0]).store(1, std::memory_order_release);
    std::atomic_ref<unsigned char>(bytes[1]).store(0, std::memory_order_release);
}
void APS5_VABI __cxa_guard_abort_nid_postfix(std::uint64_t* guardObject) {
    if (!LibcGuard::stack.Remove(guardObject)) LibcException::Terminate();
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char>(bytes[1]).store(0, std::memory_order_release);
}

}
