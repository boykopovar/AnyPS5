#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_IMAGETABLE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_IMAGETABLE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <bitset>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class ImageTable {
public:
    static constexpr std::uint32_t LogicalCapacity = 16386;
    static constexpr std::uint32_t InvalidSlot = UINT32_MAX;
    static constexpr std::size_t FramesInFlight = 3;

    enum class SlotState : std::uint8_t { Free, Resident, Retiring, Spilled };

    struct Slot {
        std::uint64_t contentHash = 0;
        std::uint64_t lastUsedFrame = 0;
        std::uint32_t version = 0;
        std::uint32_t logicalIndex = InvalidSlot;
        SlotState state = SlotState::Free;
        bool used = false;
    };

    struct DirtyRun {
        std::uint32_t first;
        std::uint32_t count;
        std::vector<std::uint32_t> versions;
    };

    struct StagedDescriptor {
        std::uint32_t slot;
        std::uint32_t version;
        VkDescriptorImageInfo info;
        std::shared_ptr<void> owner;
    };

    class FrameStaging {
    public:
        void Add(StagedDescriptor descriptor);
        std::vector<StagedDescriptor> Take();
        std::size_t Size() const;

    private:
        mutable std::mutex mutex;
        std::vector<StagedDescriptor> descriptors;
    };

    // Slot-only construction keeps allocator behavior testable without a Vulkan device.
    explicit ImageTable(std::uint32_t physicalCapacity);
    explicit ImageTable(const Context& context);
    ~ImageTable();
    ImageTable(const ImageTable&) = delete;
    ImageTable& operator=(const ImageTable&) = delete;

    bool Enabled() const { return set != VK_NULL_HANDLE; }
    std::uint32_t Capacity() const { return physicalCapacity; }
    VkDescriptorSetLayout Layout() const { return layout; }
    VkDescriptorSet Set() const { return set; }

    // A resident mapping is stable until its slot is retired. Nullopt means the caller must spill.
    std::optional<std::uint32_t> Acquire(std::uint32_t logicalIndex, std::uint64_t contentHash, std::uint64_t frame);
    void MarkUsed(std::uint32_t physicalIndex, std::uint64_t frame);
    void CompleteFrame(std::uint64_t frame);
    void MarkDirty(std::uint32_t physicalIndex);
    std::vector<DirtyRun> DirtyRuns() const;
    void ClearDirty(std::span<const DirtyRun> runs);
    Slot GetSlot(std::uint32_t physicalIndex) const;
    std::uint32_t PhysicalSlot(std::uint32_t logicalIndex) const;
    FrameStaging& Staging(std::size_t frameIndex);

private:
    void initializeSlots(std::uint32_t capacity);
    void retire(std::uint32_t physicalIndex);
    void reclaimCompleted();
    void releaseSlot(std::uint32_t physicalIndex);

    Context context{};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::uint32_t physicalCapacity = 0;
    std::array<Slot, LogicalCapacity> slots{};
    std::array<std::uint32_t, LogicalCapacity> logicalToPhysical{};
    std::vector<std::uint32_t> freeList;
    std::list<std::uint32_t> residentLru;
    std::array<std::list<std::uint32_t>::iterator, LogicalCapacity> lruPositions{};
    std::vector<std::uint32_t> retiring;
    std::bitset<LogicalCapacity> dirtyBits;
    std::vector<std::uint32_t> dirty;
    std::array<FrameStaging, FramesInFlight> staging;
    mutable std::mutex mutex;
    std::uint64_t completedFrame = 0;
};

}

#endif
