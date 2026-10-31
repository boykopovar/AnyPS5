#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEVICEIMAGETABLE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEVICEIMAGETABLE_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeviceImageKeys.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "IntermediateRepresentation/IrMetadata/Resources.hpp"
#include "DeviceImageTableAbi.hpp"
#include "ImageTableAbi.hpp"
#include "RuntimeAbi.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class Buffer;
class Recorder;
class Sampler;
class Texture;

struct DeviceTableView {
    ShaderRecompiler::DescriptorImageShape shape = ShaderRecompiler::DescriptorImageShape::Image2D;
    bool depthCompare = false;
    bool identitySwizzle = false;
};

struct DeviceTableTexture {
    std::shared_ptr<Texture> texture;
    VkImageView view = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint64_t address = 0;
    std::uint64_t guestBytes = 0;
    std::uint64_t generation = 0;
    std::string unservable;
    std::array<std::uint32_t, 8> words{};
    bool depthCompare = false;
};

DeviceTableTexture LookupDeviceTableTexture(const Context& context, std::span<const std::uint32_t, 8> descriptor, const DeviceTableView& view, std::uint32_t samplerClass);
bool DeviceTableTextureCurrent(const Context& context, const std::array<std::uint32_t, 8>& words, bool depthCompare, const Texture& texture);

bool GpuPathEnabled();

struct DeviceImagePathFeatures {
    bool gpuHeap = false;
    bool descriptorHeap = false;
};

std::optional<std::string> DeviceImagePathRefusal(const DeviceImagePathFeatures& features);

class DeviceImageTable {
public:
    using Key = DeviceImageKeys::Key;
    using Interface = ShaderRecompiler::DeviceImageInterface;
    using InterfaceLookup = std::function<std::shared_ptr<const Interface>(std::uint64_t id)>;
    struct Slots {
        std::function<std::optional<std::uint32_t>(std::uint32_t kind, const std::shared_ptr<Texture>& texture, VkImageView view, VkImageLayout layout)> take;
        std::function<std::uint32_t(std::uint32_t kind, std::uint32_t slot)> generation;
        std::function<std::uint64_t()> serial;
        std::function<bool()> nearlyFull;
        std::function<std::optional<std::uint32_t>(const std::shared_ptr<Sampler>& sampler)> takeSampler;
        std::function<std::uint32_t(std::uint32_t slot)> samplerGeneration;
    };
    struct Budget {
        std::uint32_t entries = 16384u;
        std::uint64_t textureBytes = 0;
        std::uint64_t checkPresents = 60u;
        std::uint64_t idlePresents = 120u;
        std::uint32_t maxBuckets = DeviceImageKeys::MaxBuckets;
    };
    static constexpr std::uint32_t OverflowBatches = 3u;

    static constexpr std::uint32_t RingCapacity = 4096u;
    static constexpr std::uint32_t RingChunks = 16u;
    static constexpr std::uint32_t RingWords = ShaderRecompiler::DeviceImageTableAbi::RingRecordsWord + RingCapacity * ShaderRecompiler::DeviceImageTableAbi::RecordWords;
    static constexpr std::uint32_t TableWords = ShaderRecompiler::DeviceImageTableAbi::HeaderWords + DeviceImageKeys::MaxBuckets * ShaderRecompiler::DeviceImageTableAbi::BucketWords;

    static Slots HeapSlots(const Context& context);

    DeviceImageTable(const Context& context, InterfaceLookup lookup, Slots slots);
    DeviceImageTable(const Context& context, InterfaceLookup lookup, Slots slots, Budget budget);
    ~DeviceImageTable();
    DeviceImageTable(const DeviceImageTable&) = delete;
    DeviceImageTable& operator=(const DeviceImageTable&) = delete;

    static DeviceImageTable& Create(const Context& context, InterfaceLookup lookup);
    static DeviceImageTable* Existing(const Context& context);
    static bool ExpectsKeys();
    static bool KeepsUnexpectedPrograms();
    static void Release(VkDevice device);

    void Prepare(const Context& build, Recorder& recorder, std::span<const ShaderRecompiler::PipelineIdentity> programs = {});
    std::optional<std::string> Expect(const Context& build, Recorder& recorder, std::span<const ShaderRecompiler::DeviceImageTableAbi::Expectation> images);
    void FallBackProgram(const ShaderRecompiler::PipelineIdentity& identity, std::uint64_t codeAddress, const std::string& reason);
    void NoteUnexpectedBatch(Recorder& recorder);
    std::uint64_t UnexpectedSerial(Recorder& recorder);
    void Settle(const Context& build, Recorder& recorder);
    void SetInterfaceLookup(InterfaceLookup lookup);
    void NoteProgram(const ShaderRecompiler::PipelineIdentity& identity, std::uint64_t codeAddress);
    bool ServesProgram(std::uint64_t codeAddress) const;
    bool ServesIdentity(const ShaderRecompiler::PipelineIdentity& identity) const;
    ShaderRecompiler::ImageTableAbi::Trailer Trailer(Recorder& recorder);
    std::span<std::uint32_t> RingChunk(Recorder& recorder);
    VkBuffer TableBuffer() const;
    std::optional<DeviceImageValue> Find(const Key& key) const;
    std::vector<std::uint32_t> KeyWords() const;
    std::string LastFault() const;
    std::string LastFallback() const;
    std::uint32_t OverflowStreak(const ShaderRecompiler::PipelineIdentity& identity) const;
    std::shared_ptr<Texture> EntryTexture(const Key& key) const;

    struct Counts {
        std::uint64_t prepares = 0;
        std::uint64_t drained = 0;
        std::uint64_t distinct = 0;
        std::uint64_t dropped = 0;
        std::uint64_t inserted = 0;
        std::uint64_t faults = 0;
        std::uint64_t rejected = 0;
        std::uint64_t uploads = 0;
        std::uint64_t uploadBytes = 0;
        std::uint64_t chunkWaits = 0;
        std::uint64_t unserved = 0;
        std::uint64_t heapFull = 0;
        std::uint64_t tableFull = 0;
        std::uint64_t scans = 0;
        std::uint64_t checked = 0;
        std::uint64_t refreshed = 0;
        std::uint64_t evicted = 0;
        std::uint64_t swept = 0;
        std::uint64_t fallbacks = 0;
        std::uint64_t expected = 0;
        std::uint64_t expectedInserted = 0;
        std::uint64_t unexpectedBatches = 0;
        std::uint64_t settles = 0;
        std::uint32_t entries = 0;
        std::uint32_t buckets = 0;
        std::array<std::uint32_t, ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding> peakTextures{};
    };
    Counts Totals() const;
    static std::vector<Counts> DeviceTotals();

private:
    struct Entry {
        std::shared_ptr<Texture> texture;
        std::uint32_t kind = 0;
        std::uint32_t slot = 0;
        std::uint32_t slotGeneration = 0;
        std::uint64_t address = 0;
        std::uint64_t guestBytes = 0;
        std::uint64_t generation = 0;
        std::uint64_t stamp = 0;
        std::uint64_t used = 0;
        std::uint64_t usedPresent = 0;
        std::array<std::uint32_t, 8> words{};
        bool depthCompare = false;
        std::shared_ptr<Sampler> sampler;
        std::uint32_t samplerSlot = 0;
        std::uint32_t samplerGeneration = 0;
    };
    struct ValidationState {
        std::uint64_t epoch = 0;
        std::uint64_t unwatch = 0;
        std::uint64_t depthSurfaces = 0;
        std::uint64_t pending = 0;
        std::uint64_t ownWrites = 0;
        bool operator==(const ValidationState&) const = default;
    };
    struct Block {
        std::uint64_t generation = 0;
        std::set<Key> keys;
    };
    struct Rejection {
        ShaderRecompiler::ImageTableAbi::PoisonReason reason = ShaderRecompiler::ImageTableAbi::PoisonReason::DriverRejected;
        std::string message;
    };
    struct Chunk {
        std::uint64_t serial = 0;
        std::uint64_t generation = 0;
        bool busy = false;
        std::set<ShaderRecompiler::PipelineIdentity> programs;
        bool unexpected = false;
    };
    using Programs = std::shared_ptr<const std::set<ShaderRecompiler::PipelineIdentity>>;
    struct PendingMiss {
        DeviceImageMiss miss;
        Programs programs;
    };

    void resolve(const Context& build, Recorder& recorder);
    std::optional<DeviceImageValue> lookupKey(const Context& build, const Key& key, Entry& entry, std::string* failure = nullptr);
    DeviceImageValue reject(const Key& key, ShaderRecompiler::ImageTableAbi::PoisonReason reason, const std::string& message);
    void unserved(const Key& key, const std::string& why);
    void heapFull(const Key& key, std::uint32_t kind);
    bool insert(const Key& key, const DeviceImageValue& value);
    void fallBack(const std::set<ShaderRecompiler::PipelineIdentity>& programs, const std::string& reason);
    void hold(const Key& key, Entry entry);
    void release(const Key& key, Recorder& recorder);
    void retainSlots(Recorder& recorder);
    void refresh(const Context& build, Recorder& recorder);
    void evict(const Context& build, Recorder& recorder);
    void upload(Recorder& recorder);
    VkDeviceSize writeHeader(VkCommandBuffer commands);
    std::uint32_t chunkFor(Recorder& recorder, bool mayWait);
    void drain(std::uint32_t chunk);
    std::string faultText(const Key& key, std::uint32_t pc) const;

    Context context;
    InterfaceLookup lookup;
    Slots slots;
    mutable std::recursive_mutex mutex;
    DeviceImageKeys keys;
    std::map<Key, Entry> entries;
    std::map<Key, Rejection> rejections;
    std::vector<PendingMiss> pending;
    std::unique_ptr<Buffer> table;
    std::unique_ptr<Buffer> ring;
    VkDeviceSize chunkStride = 0;
    std::vector<Chunk> chunks;
    std::uint32_t uploadedBuckets = 0;
    std::array<std::uint32_t, ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding> liveTextures{};
    std::map<std::pair<std::uint32_t, const void*>, std::uint32_t> textureUses;
    std::set<Key> unservedKeys;
    std::set<std::uint32_t> fullKinds;
    Budget budget;
    std::map<std::uint64_t, Block> blocks;
    std::optional<ValidationState> validated;
    std::uint64_t scannedStamp = 0;
    std::uint64_t slotSerial = 0;
    std::uint64_t useSerial = 0;
    std::uint64_t evictionCheck = 0;
    std::map<ShaderRecompiler::PipelineIdentity, std::set<std::uint64_t>> identityPrograms;
    std::set<ShaderRecompiler::PipelineIdentity> failedPrograms;
    std::set<std::uint64_t> fallbackPrograms;
    std::map<ShaderRecompiler::PipelineIdentity, std::uint32_t> overflowStreaks;
    std::map<ShaderRecompiler::PipelineIdentity, std::uint64_t> overflowFrom;
    std::uint64_t resolveGeneration = 0;
    bool tableFullReported = false;
    std::string lastFault;
    std::string lastFallback;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    Counts totals;
};

}

#endif
