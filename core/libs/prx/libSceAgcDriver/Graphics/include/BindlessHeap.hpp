#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "RuntimeAbi.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
struct StagePushPlacement;
}

namespace AgcDriver::Graphics {

class Buffer;
class Recorder;
class HeapSlotOwner;
class Sampler;
class StorageTexture;
class Texture;

class BindlessHeap {
public:
    explicit BindlessHeap(const Context& context);
    ~BindlessHeap();
    BindlessHeap(const BindlessHeap&) = delete;
    BindlessHeap& operator=(const BindlessHeap&) = delete;

    static BindlessHeap& For(const Context& context);
    static BindlessHeap* Existing(const Context& context);
    static void Release(VkDevice device);
    static bool Fits(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties);
    static bool RingInResourceHeap(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, VkDeviceSize uniformAlignment);

    bool DescriptorHeapMode() const { return resourceHeap != nullptr; }
    VkDeviceSize ImageHeapStride() const { return imageHeapStride; }
    void BindDescriptorHeaps(VkCommandBuffer commands) const;
    bool DescriptorHeapMappings(std::span<const std::uint32_t> spirv, std::vector<VkDescriptorSetAndBindingMappingEXT>& mappings,
                                std::uint32_t workgroupAddressOffset = ~0u, bool rootGds = false, bool rootFault = false,
                                bool pixelLds = false, bool meshWindow = false, bool scratchPool = false,
                                const ShaderRecompiler::StagePushPlacement* placement = nullptr) const;
    void WritePixelLdsBuffer(VkDeviceAddress address, VkDeviceSize bytes);
    void WriteMeshWindowBuffer(VkDeviceAddress address, VkDeviceSize bytes);
    std::pair<VkBuffer, VkDeviceSize> DataLocation(VkDeviceAddress address) const;

    std::optional<std::uint32_t> TextureSlot(std::uint32_t kind, Texture* texture, VkImageView view, VkImageLayout layout);
    std::optional<std::uint32_t> StorageSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view);
    std::optional<std::uint32_t> ViewSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view);
    void ReleaseSlots(const HeapSlotOwner* owner);
    std::uint32_t SlotGeneration(std::uint32_t slot) const;
    const Texture* SlotTexture(std::uint32_t kind, std::uint32_t slot) const;
    bool SlotHoldsStorage(std::uint32_t kind, std::uint32_t slot, const HeapSlotOwner* owner, VkImageView view) const;
    std::optional<std::uint32_t> SamplerSlot(const std::shared_ptr<Sampler>& sampler);
    std::uint32_t SamplerGeneration(std::uint32_t slot) const;
    std::uint64_t SlotSerial() const { return slotSerial.load(std::memory_order_acquire); }
    bool GenerationsCurrent(std::span<const std::vector<std::uint32_t>> slots, std::span<const std::uint32_t> slotGenerations,
                            std::span<const std::uint32_t> samplerSlots, std::span<const std::uint32_t> samplerGenerations) const;
    std::uint32_t AbsentSlot(std::uint32_t kind, VkImageView view);
    static bool NearlyFull();
    static std::uint32_t ImageSlotCapacity();
    std::uint32_t ImageSlots() const { return slotCap; }
    std::uint32_t TouchSlots(std::span<const std::vector<std::uint32_t>> slots, std::uint32_t touched) const;

    struct DataSpan {
        std::uint32_t offset = 0;
        std::span<std::uint32_t> words;
        VkDeviceAddress address = 0;
    };
    DataSpan Allocate(std::size_t words);
    struct SpanMark {
        std::uint32_t offset = 0;
        std::uint32_t words = 0;
        std::uint64_t entry = 0;
        std::uint32_t block = 0;
        VkDeviceAddress address = 0;
    };
    DataSpan Allocate(std::size_t words, SpanMark& mark);
    DataSpan RingAllocate(std::size_t words, SpanMark& mark);
    bool Held(const SpanMark& mark);
    std::uint32_t RingBufferDescriptors(std::span<const VkDeviceAddressRangeEXT> buffers, SpanMark& mark);
    static constexpr std::uint32_t BufferSlotBytes = 16u;

    static constexpr std::uint32_t SamplerKind = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);
    void Submitted(Recorder& recorder, std::uint64_t serial);
    bool TakeRingPressure();

    struct Counts {
        std::uint64_t ringWraps = 0, encodes = 0, encodedCopies = 0;
        std::uint64_t slotsFreed = 0, slotsReused = 0, slotsEvicted = 0, evictionWaits = 0;
        std::uint64_t ringWaits = 0, ringBatchPeakBytes = 0, ringSpanPeakBytes = 0, ringBytes = 0, imageSlotsHeld = 0;
        std::uint64_t spansWritten = 0, spansKept = 0, spansExpired = 0, keptChunksSkipped = 0, busyChunksSkipped = 0, bufferDescriptors = 0;
        std::uint64_t ringPressureSubmits = 0;
        std::uint64_t poolBytes = 0, poolBatchPeakBytes = 0, poolBlocksMade = 0, poolBlocksRecycled = 0, poolBlocksFreed = 0, poolBlocksHeld = 0, poolBlockBytes = 0;
    };
    Counts Statistics() const;
    std::size_t RingChunkCount() const { return ringChunks.size(); }

    static constexpr std::size_t RingBytes = 64u << 20u;
    static constexpr std::size_t DefaultRingBytes = 16u << 20u;
    static constexpr std::size_t PoolBlockBytes = 2u << 20u;
    static constexpr std::size_t PoolIdleBytes = 32u << 20u;
    static constexpr std::size_t RingChunkBytes = 2u << 20u;

private:
    Context context;
    std::byte* ringData = nullptr;
    std::size_t ringBytes = 0;
    VkDeviceSize ringHeapOffset = 0;
    bool ringInHeap = false;
    std::unique_ptr<Buffer> resourceHeap;
    std::unique_ptr<Buffer> samplerHeap;
    VkDeviceSize imageHeapStride = 0;
    VkDeviceSize samplerHeapStride = 0;
    VkDeviceSize resourceHeapUsed = 0;
    VkDeviceSize pixelLdsHeapOffset = 0;
    VkDeviceSize meshWindowHeapOffset = 0;
    VkDeviceSize samplerHeapUsed = 0;
    PFN_vkWriteResourceDescriptorsEXT writeResourceDescriptors = nullptr;
    PFN_vkWriteSamplerDescriptorsEXT writeSamplerDescriptors = nullptr;
    std::vector<std::byte> encodeScratch;
    void writeImageSlot(std::uint32_t kind, std::uint32_t slot, VkImageView view, VkImageLayout layout);
    void writeSamplerSlot(std::uint32_t slot, VkSampler sampler);
    void writeBufferDescriptor(VkDeviceSize offset, VkDeviceAddress address, VkDeviceSize bytes, const char* what);
    struct Chunk {
        Recorder* recorder = nullptr;
        std::uint64_t serial = 0;
        bool open = false;
        std::uint64_t entered = 0;
    };
    std::vector<Chunk> ringChunks;
    std::uint64_t ringEntries = 0;
    std::size_t ringChunk = ~std::size_t{0};
    void enterRingChunk(std::size_t index);
    std::size_t wrapRingOffset(std::size_t offset, std::size_t bytes);
    std::size_t ringCursor = 0;
    struct PoolBlock {
        std::unique_ptr<Buffer> buffer;
        std::byte* data = nullptr;
        VkDeviceAddress address = 0;
        std::size_t bytes = 0;
        std::size_t cursor = 0;
        std::uint64_t generation = 0;
        Recorder* recorder = nullptr;
        std::uint64_t serial = 0;
        bool open = false;
    };
    std::vector<PoolBlock> poolBlocks;
    std::size_t poolCurrent = ~std::size_t{0};
    std::uint32_t poolAlignment = 256;
    bool poolDeviceLocal = true;
    std::size_t poolBatchBytes = 0;
    std::size_t takePoolBlockLocked(std::size_t bytes);
    bool poolBlockDoneLocked(PoolBlock& block);
    void trimPoolLocked();
    std::uint32_t ringAlignment = 256;
    mutable std::mutex mutex;
    std::atomic<std::uint64_t> slotSerial{1};
    struct ImageSlotKey {
        std::uint32_t kind;
        const HeapSlotOwner* owner;
        VkImageView view;
        VkImageLayout layout;
        bool operator==(const ImageSlotKey&) const = default;
    };
    struct ImageSlotKeyHash {
        std::size_t operator()(const ImageSlotKey& key) const {
            constexpr std::size_t HashSalt = 0x9e3779b97f4a7c15ull;
            std::size_t seed = 0;
            const auto mix = [&seed](std::size_t value) { seed ^= value + HashSalt + (seed << 6u) + (seed >> 2u); };
            mix(key.kind);
            mix(static_cast<std::size_t>(key.layout));
            mix(std::hash<const HeapSlotOwner*>{}(key.owner));
            mix(std::hash<const void*>{}(reinterpret_cast<const void*>(key.view)));
            return seed;
        }
    };
    std::unordered_map<ImageSlotKey, std::uint32_t, ImageSlotKeyHash> imageSlots;
    std::unordered_map<const HeapSlotOwner*, std::vector<std::uint32_t>> ownerSlots;
    struct SlotPool {
        std::uint32_t next = 0;
        std::uint32_t limit = 0;
        std::vector<std::uint32_t> free;
        std::vector<std::uint32_t> generations;
        std::optional<std::uint32_t> Take(bool& reused);
        void Free(std::uint32_t slot);
        std::size_t Unused() const;
        std::uint32_t Generation(std::uint32_t slot) const;
    };
    struct SlotOwner {
        const Texture* texture = nullptr;
        ImageSlotKey key{};
    };
    std::vector<SlotOwner> owners;
    SlotPool imagePool;
    SlotPool samplerPool;
    std::uint32_t slotCap = 0;
    std::unique_ptr<std::atomic<std::uint32_t>[]> slotUses;
    std::atomic<std::uint32_t> useEpoch{1};
    struct RetiringSlot {
        std::uint32_t slot = 0;
        std::uint64_t serial = 0;
    };
    std::deque<RetiringSlot> retiring;
    std::size_t ringBatchBytes = 0;
    std::atomic<std::uint32_t> ringOpenChunks{0};
    std::size_t retiringAssigned = 0;
    std::uint32_t evictScanEpoch = 0;
    Recorder* submitter = nullptr;
    void touchLocked(std::uint32_t slot) const;
    void evictLocked();
    bool reclaimLocked(bool wait);
    std::optional<std::uint32_t> imageSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view, VkImageLayout layout, Texture* texture);
    SlotOwner& claimSlotLocked(const ImageSlotKey& key, std::uint32_t slot);
    void freeSlotLocked(std::uint32_t slot);
    const SlotOwner* ownerLocked(std::uint32_t slot) const;
    std::unordered_map<const HeapSlotOwner*, std::uint32_t> samplerSlots;
    std::unordered_set<std::uint32_t> absentSlots;
    bool refused = false;
    void noteRefusedLocked(const char* what, std::uint32_t margin);
    void noteTakenLocked();
    void noteSamplerTakenLocked();
    void noteReleasedLocked();
    std::uint32_t imageMargin() const;
    bool roomLocked() const;
    Counts counts;
};

class HeapSlotOwner {
public:
    HeapSlotOwner(const HeapSlotOwner&) = delete;
    HeapSlotOwner& operator=(const HeapSlotOwner&) = delete;

protected:
    HeapSlotOwner() = default;
    ~HeapSlotOwner() = default;
    void ReleaseHeapSlots(const Context& context) noexcept {
        if (heapSlotted.exchange(false, std::memory_order_acq_rel)) {
            if (auto* heap = BindlessHeap::Existing(context)) heap->ReleaseSlots(this);
        }
    }

private:
    friend class BindlessHeap;
    mutable std::atomic<bool> heapSlotted{false};
};

bool GpuHeapEnabled();
void SetDescriptorHeapDevice(bool available);
bool DescriptorHeapEnabled();
bool DescriptorHeapMappable(std::span<const std::uint32_t> spirv, bool workgroupAddress, bool gdsAddress, bool pixelLds = false, bool meshWindow = false, bool scratchPool = false);
void NoteHeapComputeStage();
std::uint64_t HeapComputeStages();
void NoteImageTableBuild();
std::uint64_t ImageTableBuilds();
void NoteDataAddressPush();
void NoteEmptyResourceSet();
std::uint64_t EmptyResourceSets();
std::uint64_t DataAddressPushes();
enum class SetBindSite : std::uint8_t { Draw, Dispatch, Detiler, ColorTransfer, Presentation, DepthTransfer, StagedMerge, FirstSampleCopy, Count };
enum class HeapFallback : std::uint8_t { DrawUnmapped, DrawMixedRetry, DrawRefusedPair, DrawMixedReused, DispatchUnmapped, Count };
enum class HeapStageReason : std::uint8_t {
    Heap,
    Disabled,
    PairRefused,
    FragmentOffHeap,
    StageKind,
    NoPlan,
    PlanStage,
    UnresolvedImageSource,
    KnownIneligible,
    Count
};
void NoteHeapStageReason(HeapStageReason reason);
void NoteHeapRecord(bool compute, bool heap);
void NoteHeapFallback(HeapFallback reason);
std::uint64_t HeapFallbacks(HeapFallback reason);
void NoteCpuBufferCopy(std::uint64_t bytes);
enum class HeapCost : std::uint8_t {
    StagePrepare,
    HeapStageCall,
    StageFinish,
    WalkMemo,
    Walk,
    Materialize,
    TemplateFind,
    TemplateBuild,
    Slots,
    StageData,
    BufferAddresses,
    MemoInsert,
    Guards,
    Count
};
bool HeapCostTimed();

void NoteHeapWalkMemoBytes(std::int64_t delta);
void NoteHeapCost(HeapCost cost, std::uint64_t nanoseconds);
class HeapCostScope {
public:
    explicit HeapCostScope(HeapCost cost) :
        cost(cost), start(HeapCostTimed() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}
    ~HeapCostScope() { Stop(); }
    HeapCostScope(const HeapCostScope&) = delete;
    HeapCostScope& operator=(const HeapCostScope&) = delete;
    void Stop() {
        if (start == std::chrono::steady_clock::time_point{}) return;
        NoteHeapCost(cost, static_cast<std::uint64_t>(
                               std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()));
        start = {};
    }

private:
    HeapCost cost;
    std::chrono::steady_clock::time_point start;
};
void NoteCommandBufferBegun(VkCommandBuffer commands);
void CmdPushConstants(const Context& context, VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages,
                      std::uint32_t offset, std::uint32_t size, const void* values);
void CmdBindDescriptorSets(const Context& context, VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                           std::uint32_t firstSet, std::uint32_t setCount, const VkDescriptorSet* sets, SetBindSite site);
void CmdPushDescriptorSet(PFN_vkCmdPushDescriptorSetKHR push, VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t set,
                          std::uint32_t writeCount, const VkWriteDescriptorSet* writes, SetBindSite site);
std::uint64_t DescriptorHeapBinds();
inline constexpr std::uint32_t HeapPassSampledKind = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
inline constexpr std::uint32_t HeapPassStorageKind = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding;
inline constexpr VkSpirvResourceTypeFlagsEXT HeapPassStorageImages = VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
class HeapPassSlots : public HeapSlotOwner {
public:
    explicit HeapPassSlots(const Context& context) : context(context) {}
    ~HeapPassSlots() { ReleaseHeapSlots(context); }
    void Release() noexcept { ReleaseHeapSlots(context); }

private:
    Context context;
};
bool HeapPassesActive(const Context& context);
VkDescriptorSetAndBindingMappingEXT HeapPushAddressMapping(std::uint32_t set, std::uint32_t binding, std::uint32_t pushOffset);
VkDescriptorSetAndBindingMappingEXT HeapPushIndexMapping(const BindlessHeap& heap, std::uint32_t set, std::uint32_t binding, VkSpirvResourceTypeFlagsEXT resources,
                                                         std::uint32_t pushOffset);
VkPipeline CreateHeapComputePipeline(const Context& context, VkComputePipelineCreateInfo info, std::span<const VkDescriptorSetAndBindingMappingEXT> mappings);
VkPipeline CreateHeapGraphicsPipeline(const Context& context, VkGraphicsPipelineCreateInfo info, std::span<const VkDescriptorSetAndBindingMappingEXT> mappings);
void CmdPushData(const Context& context, VkCommandBuffer commands, std::uint32_t bytes, const void* values);

}

#endif
