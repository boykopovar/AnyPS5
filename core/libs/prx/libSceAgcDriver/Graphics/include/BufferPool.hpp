#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BUFFERPOOL_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace AgcDriver::Graphics {

struct BufferAllocation {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapping;
    VkDeviceAddress address;
    VkDeviceSize allocationBytes;
    // Size the VkBuffer was created with (see BufferPool::Capacity), not the size a user asked for.
    std::size_t bytes;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
};

// Released buffer allocations kept for reuse, since creating, binding and mapping one costs tens of
// microseconds and a build makes several. Small requests are served by size class (the next power of
// two), so the many differently sized data and copied-region buffers of consecutive builds hit;
// requests of a MiB and more keep their exact size (the multi-MiB registered-range snapshots repeat
// exactly, and rounding them would waste pinned host memory). Retained allocations are evicted least
// recently used under a byte budget and a slot count, so a burst of small buffers between two large
// builds no longer sweeps the large ones out. Reuse is safe because a Buffer is only released once
// the GPU work using it completed (kept until the batch fence, or after a CommandBatch wait).
// APS5_NO_BUFFER_CLASSES=1 matches exact sizes only, as before; APS5_BUFFER_POOL_SLOTS=64 restores
// the old slot count (the bound applies to each tier below, so twice that many slots in all).
//
// The size classes and the exact-size allocations are retained in two tiers with a budget each:
// the multi-MiB texture scratch buffers that come back from a batch (GPU-direct uploads and
// write-backs of storage images) filled the one budget, so every one returned evicted hundreds of
// the 256-byte copied-region buffers to fit, and those then missed on every build (81869 misses
// and 80850 evictions per run, each a Vulkan create or destroy under the pool mutex, 3.5 s per
// run). APS5_BUFFER_POOL_SHARED=1 keeps one tier as before.
class BufferPool {
public:
    explicit BufferPool(const Context& context);
    ~BufferPool();
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    // The size a buffer for `bytes` is created with: its size class, or `bytes` itself when large.
    static std::size_t Capacity(std::size_t bytes);
    std::optional<BufferAllocation> Take(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void Put(const BufferAllocation& allocation) noexcept;

private:
    struct Slot {
        BufferAllocation allocation;
        std::uint64_t lastUse;
    };
    // One retention tier: its slots, their bytes, the byte budget they are evicted under and its
    // counters (APS5_PROFILE_DRAW, reported every 10 s from Take).
    struct Tier {
        std::vector<Slot> free;
        VkDeviceSize retainedBytes = 0;
        VkDeviceSize budget = 0;
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::uint64_t evictions = 0;
    };
    // The tier a buffer of `capacity` is retained in (the large one for everything when shared).
    Tier& tierFor(std::size_t capacity);
    void destroy(const BufferAllocation& allocation) noexcept;
    // Moves the tier's least recently used slot to `evicted`; the caller destroys those after
    // releasing the mutex, so builds taking buffers on other threads do not wait behind the
    // Vulkan destroy calls. Nothing changes when the vector cannot grow.
    void evictOldest(Tier& tier, std::vector<BufferAllocation>& evicted);
    // The retained-slot bound of each tier (APS5_BUFFER_POOL_SLOTS, default `defaultSlots`), read once.
    static std::size_t MaxSlots();
    VkDevice device;
    PFN_vkUnmapMemory unmap;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    std::mutex mutex;
    // Not `small`/`large`: <rpcndr.h> (via <windows.h>) defines `small` as a macro.
    Tier smallTier;
    Tier largeTier;
    std::uint64_t clock = 0;
    static constexpr VkDeviceSize budget = 512ull * 1024 * 1024;
    // The small tier's own budget (512 slots of at most half a MiB each): pinned host memory the
    // large tier's budget does not count.
    static constexpr VkDeviceSize smallBudget = 64ull * 1024 * 1024;
    // Requests of this size and more keep their exact size and go to the large tier.
    static constexpr std::size_t classLimit = std::size_t{1} << 20u;
    static constexpr std::size_t defaultSlots = 512;
};

std::shared_ptr<BufferPool> GetBufferPool(const Context& context);

}

#endif
