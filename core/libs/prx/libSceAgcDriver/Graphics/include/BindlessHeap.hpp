#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSHEAP_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "RuntimeAbi.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

class Buffer;
class Recorder;
class Sampler;
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

    VkDescriptorSetLayout HeapLayout() const { return heapLayout; }
    VkDescriptorSetLayout DataLayout() const { return dataLayout; }
    VkDescriptorSet HeapSet() const { return heapSet; }
    VkDescriptorSet DataSet() const { return dataSet; }

    std::optional<std::uint32_t> TextureSlot(std::uint32_t kind, const std::shared_ptr<Texture>& texture, VkImageView view, VkImageLayout layout);
    std::uint32_t SlotGeneration(std::uint32_t kind, std::uint32_t slot) const;
    const Texture* SlotTexture(std::uint32_t kind, std::uint32_t slot) const;
    std::optional<std::uint32_t> SamplerSlot(const std::shared_ptr<Sampler>& sampler);
    std::uint32_t SamplerGeneration(std::uint32_t slot) const;
    std::uint64_t SlotSerial() const { return slotSerial.load(std::memory_order_acquire); }
    bool GenerationsCurrent(std::span<const std::vector<std::uint32_t>> slots, std::span<const std::uint32_t> slotGenerations, std::span<const std::uint32_t> samplerSlots, std::span<const std::uint32_t> samplerGenerations) const;
    std::uint32_t AbsentSlot(std::uint32_t kind, VkImageView view);
    static bool NearlyFull();
    static void NoteSweepDue();
    static void SweepIfDue();

    struct DataSpan {
        std::uint32_t offset = 0;
        std::span<std::uint32_t> words;
    };
    DataSpan Allocate(std::size_t words);

    std::uint32_t LookupWord(std::uint32_t kind, const std::uint32_t* key, std::uint32_t slot);
    std::uint32_t LookupMisses() const;
    std::uint32_t LookupDisagreements() const;
    void AddLookupFixup(std::uint32_t slotDword, std::uint32_t slot, const std::uint32_t* key);
    static constexpr std::uint32_t FixupWords = 2u + ShaderRecompiler::RuntimeAbi::GpuLookupKeyDwords;
    static constexpr std::uint32_t SamplerKind = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);
    struct LookupResolve {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        std::shared_ptr<void> release;
    };
    LookupResolve TakeLookupResolve();
    LookupResolve Submitted(Recorder& recorder, std::uint64_t serial);

    struct Counts {
        std::uint64_t textures = 0, samplers = 0, full = 0, ringWraps = 0;
        std::uint64_t lookupSentinels = 0, lookupExplicit = 0, lookupInserts = 0, lookupFull = 0;
        std::uint64_t slotsFreed = 0, slotsReused = 0, sweeps = 0, lookupRevived = 0, lookupFixups = 0, chunkWaits = 0;
    };
    Counts Statistics() const;

    static constexpr std::size_t RingBytes = 64u << 20u;
    static constexpr std::uint32_t DataBindingRange = 64u << 10u;
    static constexpr std::size_t FixupRingWords = 1u << 20u;

private:
    Context context;
    VkDescriptorSetLayout heapLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout dataLayout = VK_NULL_HANDLE;
    VkDescriptorPool heapPool = VK_NULL_HANDLE;
    VkDescriptorPool dataPool = VK_NULL_HANDLE;
    VkDescriptorSet heapSet = VK_NULL_HANDLE;
    VkDescriptorSet dataSet = VK_NULL_HANDLE;
    std::unique_ptr<Buffer> ring;
    std::unique_ptr<Buffer> lookup;
    static constexpr std::size_t RingChunkBytes = 4u << 20u;
    static constexpr std::size_t FixupChunkWords = 1u << 17u;
    struct Chunk {
        Recorder* recorder = nullptr;
        std::uint64_t serial = 0;
        bool open = false;
    };
    std::vector<Chunk> ringChunks;
    std::vector<Chunk> fixupChunks;
    std::size_t ringChunk = ~std::size_t{0};
    std::size_t fixupChunk = ~std::size_t{0};
    void enterChunk(std::vector<Chunk>& chunks, std::size_t index, std::size_t& current, const char* what);
    LookupResolve takeLookupResolveLocked(Recorder* recorder, std::uint64_t serial);
    std::vector<std::uint32_t> lookupShadow;
    std::size_t ringCursor = 0;
    std::uint32_t ringAlignment = 256;
    mutable std::mutex mutex;
    std::atomic<std::uint64_t> slotSerial{1};
    struct TextureKey {
        std::uint32_t kind;
        const Texture* texture;
        VkImageView view;
        VkImageLayout layout;
        bool operator==(const TextureKey&) const = default;
    };
    struct TextureKeyHash {
        std::size_t operator()(const TextureKey& key) const { return std::hash<const void*>{}(key.texture) ^ std::hash<const void*>{}(reinterpret_cast<const void*>(key.view)) ^ (static_cast<std::size_t>(key.kind) << 48u) ^ (static_cast<std::size_t>(key.layout) << 40u); }
    };
    std::unordered_map<TextureKey, std::uint32_t, TextureKeyHash> textureSlots;
    std::vector<std::uint32_t> nextTextureSlot;
    struct SlotOwner {
        std::shared_ptr<Texture> texture;
        TextureKey key{};
        std::uint32_t generation = 0;
    };
    std::vector<std::vector<SlotOwner>> owners;
    std::vector<std::vector<std::uint32_t>> freeSlots;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> lookupBySlot;
    std::uint32_t slotCap = 0;
    std::uint64_t slotRequests = 0;
    void sweepLocked(std::vector<std::shared_ptr<Texture>>& dropped);
    std::optional<std::uint32_t> takeSlotLocked(std::uint32_t kind);
    std::unordered_map<const Sampler*, std::uint32_t> samplerSlots;
    std::unordered_map<std::uint32_t, std::uint32_t> absentSlots;
    std::vector<std::shared_ptr<Sampler>> heldSamplers;
    std::vector<std::uint32_t> samplerGenerations;
    std::vector<std::uint32_t> freeSamplerSlots;
    void sweepSamplersLocked(std::vector<std::shared_ptr<Sampler>>& dropped);
    void noteTakenLocked(std::uint32_t kind, std::vector<std::shared_ptr<Texture>>& dropped);
    void noteSamplerTakenLocked(std::vector<std::shared_ptr<Sampler>>& dropped);
    std::uint32_t imageMargin() const;
    std::size_t unusedLocked(std::uint32_t kind) const;
    bool roomLocked() const;
    Counts counts;
    void reportLookup();
    void createLookupResolve();
    std::vector<std::uint32_t> lookupFixups;
    std::unique_ptr<Buffer> fixupRing;
    std::size_t fixupCursor = 0;
    VkDescriptorSetLayout resolveSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout resolveLayout = VK_NULL_HANDLE;
    VkPipeline resolvePipeline = VK_NULL_HANDLE;
    VkDescriptorPool resolvePool = VK_NULL_HANDLE;
    VkDescriptorSet resolveSet = VK_NULL_HANDLE;
    VkCommandPool resolveCommands = VK_NULL_HANDLE;
    std::shared_ptr<std::mutex> freeResolveMutex = std::make_shared<std::mutex>();
    std::shared_ptr<std::vector<VkCommandBuffer>> freeResolve = std::make_shared<std::vector<VkCommandBuffer>>();
    std::uint64_t resolveDispatches = 0;
};

bool GpuHeapEnabled();
bool BindlessTexturesEnabled();
bool GpuConstantsEnabled();
void NoteImageTableBuild();
std::uint64_t ImageTableBuilds();

}

#endif
