#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "RuntimeAbi.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
struct StagePushPlacement;
struct DescriptorBinding;
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

    bool DescriptorHeapMode() const { return resourceHeap != nullptr; }
    VkDeviceSize ImageHeapStride() const { return imageHeapStride; }
    void BindDescriptorHeaps(VkCommandBuffer commands) const;
    bool DescriptorHeapMappings(std::span<const std::uint32_t> spirv, std::vector<VkDescriptorSetAndBindingMappingEXT>& mappings,
                                std::uint32_t workgroupAddressOffset = ~0u, bool rootGds = false, bool rootFault = false,
                                bool pixelLds = false, bool meshWindow = false, bool scratchPool = false,
                                const ShaderRecompiler::StagePushPlacement* placement = nullptr,
                                const std::vector<ShaderRecompiler::DescriptorBinding>* bindings = nullptr) const;
    void WritePixelLdsBuffer(VkDeviceAddress address, VkDeviceSize bytes);
    void WriteMeshWindowBuffer(VkDeviceAddress address, VkDeviceSize bytes);
    std::pair<VkBuffer, VkDeviceSize> DataLocation(VkDeviceAddress address) const;
    std::span<const std::uint32_t> DataWords(VkDeviceAddress address, std::size_t words) const;
    std::uint32_t EmbeddedSamplerCount() const;

    std::optional<std::uint32_t> TextureSlot(std::uint32_t kind, Texture* texture, VkImageView view, VkImageLayout layout);
    std::optional<std::uint32_t> StorageSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view);
    std::optional<std::uint32_t> ViewSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
    void ReleaseSlots(const HeapSlotOwner* owner);
    std::uint32_t SlotGeneration(std::uint32_t slot) const;
    const Texture* SlotTexture(std::uint32_t kind, std::uint32_t slot) const;
    bool SlotHoldsStorage(std::uint32_t kind, std::uint32_t slot, const HeapSlotOwner* owner, VkImageView view) const;
    std::optional<std::uint32_t> SamplerSlot(const std::shared_ptr<Sampler>& sampler);
    std::optional<std::uint32_t> PinnedSamplerSlot(const std::shared_ptr<Sampler>& sampler);
    std::uint32_t SamplerGeneration(std::uint32_t slot) const;
    std::uint64_t SlotSerial() const { return slotSerial.load(std::memory_order_acquire); }
    bool GenerationsCurrent(std::span<const std::uint32_t> slots, std::span<const std::uint32_t> slotGenerations,
                            std::span<const std::uint32_t> samplerSlots, std::span<const std::uint32_t> samplerGenerations) const;
    std::uint32_t AbsentSlot(std::uint32_t kind, VkImageView view);
    static bool NearlyFull();
    static std::uint32_t ImageSlotCapacity();
    std::uint32_t ImageSlots() const { return slotCap; }
    std::uint32_t TouchSlots(std::span<const std::uint32_t> slots, std::span<const std::uint32_t> samplerSlots, std::uint32_t touched) const;

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
    static void Write(const DataSpan& span, std::span<const std::uint32_t> words);
    bool Held(const SpanMark& mark);
    class SpanScope {
    public:
        explicit SpanScope(const BindlessHeap& heap);
        ~SpanScope();
        SpanScope(const SpanScope&) = delete;
        SpanScope& operator=(const SpanScope&) = delete;

    private:
        friend class BindlessHeap;
        const BindlessHeap& heap;
        bool acquired;
    };
    std::optional<DataSpan> KeepOrAllocate(const SpanScope& held, std::size_t words, SpanMark& mark, bool same);
    struct TableMark {
        std::uint32_t block = 0;
        std::uint32_t offset = 0;
        std::uint32_t sizeClass = 0;
        VkDeviceAddress address = 0;
    };
    std::optional<DataSpan> AllocateTable(const SpanScope& held, std::size_t words, TableMark& mark);
    void RetireTable(const SpanScope& held, TableMark& mark);
    void NoteTableKept(const SpanScope& held);
    static constexpr std::size_t TableBlockBytes = 1u << 20u;
    static constexpr std::uint32_t TableClasses = 16u;

    static constexpr std::uint32_t SamplerKind = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);
    void Submitted(Recorder& recorder, std::uint64_t serial);
    bool TakeSlotPressure();

    struct Counts {
        std::uint64_t encodes = 0, encodedCopies = 0;
        std::uint64_t slotsFreed = 0, slotsReused = 0, slotsEvicted = 0, evictionWaits = 0;
        std::uint64_t spanPeakBytes = 0, imageSlotsHeld = 0;
        std::uint64_t spansWritten = 0, spansKept = 0, spansExpired = 0;
        std::uint64_t slotPressureSubmits = 0, samplersEvicted = 0;
        std::uint64_t poolBytes = 0, poolBatchPeakBytes = 0, poolBlocksMade = 0, poolBlocksRecycled = 0, poolBlocksFreed = 0, poolBlocksHeld = 0, poolBlockBytes = 0;
    };
    Counts Statistics() const;

    static constexpr std::size_t PoolBlockBytes = 2u << 20u;
    static constexpr std::size_t PoolIdleBytes = 32u << 20u;

private:
    Context context;
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
    struct TableBlock {
        std::unique_ptr<Buffer> buffer;
        std::byte* data = nullptr;
        VkDeviceAddress address = 0;
        std::size_t bytes = 0;
        std::size_t cursor = 0;
    };
    std::vector<TableBlock> tableBlocks;
    std::array<std::vector<std::pair<std::uint32_t, std::uint32_t>>, TableClasses> tableFree;
    struct RetiredTable {
        TableMark mark;
        Recorder* recorder = nullptr;
        std::uint64_t serial = 0;
        bool submitted = false;
    };
    std::deque<RetiredTable> retiredTables;
    std::size_t tableShared = 0;
    std::size_t makeTableBlockLocked(std::size_t bytes);
    void reclaimTablesLocked();
    std::size_t poolCurrent = ~std::size_t{0};
    std::uint32_t poolAlignment = 256;
    std::size_t poolBatchBytes = 0;
    std::size_t takePoolBlockLocked(std::size_t bytes);
    bool heldLocked(const SpanMark& mark);
    DataSpan allocateLocked(std::size_t words, SpanMark& mark);
    bool poolBlockDoneLocked(PoolBlock& block);
    void trimPoolLocked();
    mutable std::mutex mutex;
    mutable std::mutex spanMutex;
    mutable std::atomic<std::thread::id> spanOwner{};
    struct EmbeddedSampler {
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        VkSamplerReductionModeCreateInfoEXT reduction{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT};
        std::shared_ptr<Sampler> heapSampler;
        std::uint32_t slot = ~0u;
    };
    mutable std::mutex embeddedMutex;
    mutable std::map<std::array<std::uint32_t, 5>, EmbeddedSampler> embeddedSamplers;
    mutable std::uint32_t embeddedSamplerCount = 0;
    std::optional<VkDescriptorSetAndBindingMappingEXT> embeddedSamplerMapping(const ShaderRecompiler::DescriptorBinding& binding, std::size_t element) const;
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
        std::unique_ptr<std::atomic<std::uint32_t>[]> generations;
        void Configure(std::uint32_t first, std::uint32_t slots);
        void Retire(std::uint32_t slot);
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
        bool sampler = false;
    };
    std::deque<RetiringSlot> retiring;
    std::size_t retiringAssigned = 0;
    std::size_t retiringSamplers = 0;
    std::size_t samplerRetiringAssigned = 0;
    std::uint32_t evictScanEpoch = 0;
    std::uint32_t samplerEvictScanEpoch = 0;
    std::vector<const HeapSlotOwner*> samplerOwners;
    std::vector<bool> samplerPinned;
    std::unique_ptr<std::atomic<std::uint32_t>[]> samplerUses;
    std::atomic<bool> pressured{false};
    Recorder* submitter = nullptr;
    void touchLocked(std::uint32_t slot) const;
    void touchSamplerLocked(std::uint32_t slot) const;
    void evictLocked(std::uint32_t idleSubmissions);
    void evictSamplersLocked(std::uint32_t idleSubmissions);
    std::optional<std::uint32_t> samplerSlot(const std::shared_ptr<Sampler>& sampler, bool pin);
    std::size_t imageRetiring() const { return retiring.size() - retiringSamplers; }
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
bool ChooseUniformConstantBuffers(bool standardLayout, const VkPhysicalDeviceLimits& limits);
bool UniformConstantBuffersEnabled();
bool DescriptorHeapMappable(std::span<const std::uint32_t> spirv, bool workgroupAddress, bool gdsAddress, bool pixelLds = false, bool meshWindow = false, bool scratchPool = false,
                            std::pair<std::uint32_t, std::uint32_t>* refused = nullptr);
void NoteHeapComputeStage();
std::uint64_t HeapComputeStages();
void NoteImageTableBuild();
std::uint64_t ImageTableBuilds();
void NoteDataAddressPush();
void NotePushedUserDataKeep();
void NoteUserWordsRewrite(std::size_t blockWords, std::size_t userWords);
std::uint64_t PushedUserDataKeeps();
void NoteEmptyResourceSet();
std::uint64_t EmptyResourceSets();
std::uint64_t DataAddressPushes();
std::uint64_t EmbeddedSamplerMappings();
std::uint64_t SlotPressureSubmits();
std::uint64_t HeapSlotEvictions();
enum class SetBindSite : std::uint8_t { Draw, Dispatch, Detiler, ColorTransfer, Presentation, DepthTransfer, StagedMerge, FirstSampleCopy, Count };
enum class HeapExcluded : std::uint8_t { Draw, Dispatch, Count };
enum class HeapStageReason : std::uint8_t {
    Heap,
    Disabled,
    Excluded,
    Count
};
void NoteHeapStageReason(HeapStageReason reason);
void NoteHeapRecord(bool compute, bool heap);
void NoteHeapExcluded(HeapExcluded reason);
std::uint64_t HeapExcludedCount(HeapExcluded reason);
std::uint64_t HeapStageReasons(HeapStageReason reason);
void NoteCpuBufferCopy(std::uint64_t bytes);
enum class HeapCost : std::uint8_t {
    StagePrepare,
    HeapStageCall,
    StageFinish,
    Walk,
    Materialize,
    TemplateFind,
    TemplateBuild,
    Slots,
    StageData,
    Guards,
    Count
};
bool HeapCostTimed();

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
