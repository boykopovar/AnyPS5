#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"

#include <utility>

namespace AgcDriver::Graphics {

ReferenceStorage::ReferenceStorage(std::size_t capacityBytes) noexcept : capacityBytes(capacityBytes) {}

std::size_t ReferenceStorage::releaseIdle(Clock::time_point now, std::array<std::vector<std::byte>, Slots>& released) noexcept {
    std::size_t freed = 0;
    for (std::size_t slot = 0; slot < Slots; ++slot) {
        if (idle[slot].capacity() == 0 || now - returnedAt[slot] < IdleLimit) continue;
        freed += idle[slot].capacity();
        bytes -= idle[slot].capacity();
        released[slot] = std::move(idle[slot]);
        idle[slot] = std::vector<std::byte>();
    }
    return freed;
}

std::size_t ReferenceStorage::ReleaseIdle(Clock::time_point now) noexcept {
    std::array<std::vector<std::byte>, Slots> released;
    std::lock_guard lock(mutex);
    return releaseIdle(now, released);
}

std::vector<std::byte> ReferenceStorage::Take(std::size_t requested, Clock::time_point now) noexcept {
    std::array<std::vector<std::byte>, Slots> released;
    std::lock_guard lock(mutex);
    releaseIdle(now, released);
    std::size_t best = Slots;
    for (std::size_t slot = 0; slot < Slots; ++slot) {
        const auto capacity = idle[slot].capacity();
        if (capacity == 0 || capacity < requested) continue;
        if (best == Slots || capacity < idle[best].capacity()) best = slot;
    }
    if (best == Slots) return {};
    auto storage = std::move(idle[best]);
    idle[best] = std::vector<std::byte>();
    bytes -= storage.capacity();
    return storage;
}

void ReferenceStorage::Return(std::vector<std::byte>&& storage, Clock::time_point now) noexcept {
    auto kept = std::move(storage);
    const auto capacity = kept.capacity();
    if (capacity == 0 || capacity > capacityBytes) return;
    std::array<std::vector<std::byte>, Slots> evicted;
    std::array<std::vector<std::byte>, Slots> released;
    std::lock_guard lock(mutex);
    releaseIdle(now, released);
    std::size_t free = Slots;
    for (std::size_t slot = 0; slot < Slots; ++slot) {
        if (idle[slot].capacity() == 0) free = slot;
    }
    while (free == Slots || bytes + capacity > capacityBytes) {
        std::size_t oldest = Slots;
        for (std::size_t slot = 0; slot < Slots; ++slot) {
            if (idle[slot].capacity() != 0 && (oldest == Slots || returned[slot] < returned[oldest])) oldest = slot;
        }
        bytes -= idle[oldest].capacity();
        evicted[oldest] = std::move(idle[oldest]);
        idle[oldest] = std::vector<std::byte>();
        free = oldest;
    }
    bytes += capacity;
    returned[free] = ++returns;
    returnedAt[free] = now;
    idle[free] = std::move(kept);
}

std::size_t ReferenceStorage::IdleBytes() const noexcept {
    std::lock_guard lock(mutex);
    return bytes;
}

std::size_t ReferenceStorage::IdleCount() const noexcept {
    std::lock_guard lock(mutex);
    std::size_t count = 0;
    for (const auto& storage : idle) count += storage.capacity() != 0 ? 1 : 0;
    return count;
}

ReferenceStorage& ReferenceStorage::Shared() noexcept {
    static ReferenceStorage storage(std::size_t{2} << 30u);
    return storage;
}

}
