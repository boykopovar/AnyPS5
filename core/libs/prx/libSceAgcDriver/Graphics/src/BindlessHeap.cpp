#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace AgcDriver::Graphics {

namespace {

std::atomic<std::int64_t>& walkMemoHeld() {
    static std::atomic<std::int64_t> held{0};
    return held;
}

std::atomic<std::int64_t>& walkMemoPeak() {
    static std::atomic<std::int64_t> peak{0};
    return peak;
}

}

void NoteHeapWalkMemoBytes(std::int64_t delta) {
    if (delta == 0) return;
    const auto now = walkMemoHeld().fetch_add(delta, std::memory_order_relaxed) + delta;
    auto peak = walkMemoPeak().load(std::memory_order_relaxed);
    while (now > peak && !walkMemoPeak().compare_exchange_weak(peak, now, std::memory_order_relaxed)) {}
}

namespace {

namespace Abi = ShaderRecompiler::RuntimeAbi;

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("AGC bindless heap: ") + what + " failed: " + std::to_string(static_cast<int>(result)));
}

std::atomic<std::uint64_t> spansWrittenWindow{0};
std::atomic<std::uint64_t> imageEncodesWindow{0};
std::atomic<std::uint64_t> imageCopiesWindow{0};
std::atomic<std::uint64_t> spansKeptWindow{0};
std::atomic<std::uint64_t> spansExpiredWindow{0};
std::atomic<std::uint64_t> heapComputeStages{0};
std::atomic<bool> descriptorHeapDevice{false};
std::atomic<std::uint64_t> imageTableBuilds{0};
std::atomic<std::uint64_t> dataAddressPushes{0};
std::atomic<std::uint64_t> emptyResourceSets{0};

}

bool GpuHeapEnabled() {
    static const bool enabled = std::getenv("APS5_GPU_HEAP") != nullptr;
    return enabled;
}

void NoteImageTableBuild() {
    imageTableBuilds.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t ImageTableBuilds() {
    return imageTableBuilds.load(std::memory_order_relaxed);
}

void NoteDataAddressPush() {
    dataAddressPushes.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t DataAddressPushes() {
    return dataAddressPushes.load(std::memory_order_relaxed);
}

void NoteEmptyResourceSet() {
    emptyResourceSets.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t EmptyResourceSets() {
    return emptyResourceSets.load(std::memory_order_relaxed);
}

void NoteHeapComputeStage() {
    heapComputeStages.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t HeapComputeStages() {
    return heapComputeStages.load(std::memory_order_relaxed);
}

void SetDescriptorHeapDevice(bool available) {
    descriptorHeapDevice.store(available, std::memory_order_relaxed);
}

bool DescriptorHeapEnabled() {
    return GpuHeapEnabled() && descriptorHeapDevice.load(std::memory_order_relaxed);
}

bool HeapComputeEnabled() {
    return DescriptorHeapEnabled();
}

namespace {

struct HeapSizes {
    VkDeviceSize imageStride = 0;
    VkDeviceSize samplerStride = 0;
    VkDeviceSize pixelLdsOffset = 0;
    VkDeviceSize meshWindowOffset = 0;
    VkDeviceSize resourceUsed = 0;
    VkDeviceSize samplerUsed = 0;
};

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

HeapSizes heapSizes(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties) {
    HeapSizes sizes;
    sizes.imageStride = alignUp(properties.imageDescriptorSize, properties.imageDescriptorAlignment);
    sizes.samplerStride = alignUp(properties.samplerDescriptorSize, properties.samplerDescriptorAlignment);
    const auto imageBytes = static_cast<VkDeviceSize>(Abi::GpuImageHeapSlots) * sizes.imageStride;
    sizes.pixelLdsOffset = alignUp(imageBytes, properties.bufferDescriptorAlignment);
    sizes.meshWindowOffset = alignUp(sizes.pixelLdsOffset + properties.bufferDescriptorSize, properties.bufferDescriptorAlignment);
    sizes.resourceUsed = alignUp(sizes.meshWindowOffset + properties.bufferDescriptorSize, properties.resourceHeapAlignment);
    sizes.samplerUsed = alignUp(static_cast<VkDeviceSize>(Abi::GpuSamplerHeapSlots) * sizes.samplerStride, properties.samplerHeapAlignment);
    return sizes;
}

}

namespace {

struct ResourceRing {
    VkDeviceSize offset = 0;
    std::size_t bytes = 0;
};

ResourceRing resourceRing(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, VkDeviceSize uniformAlignment) {
    static const bool disabled = std::getenv("APS5_NO_HEAP_DATA_UNIFORM") != nullptr;
    if (disabled) return {};
    const auto sizes = heapSizes(properties);
    const auto offset = alignUp(sizes.resourceUsed + properties.minResourceHeapReservedRange, std::max<VkDeviceSize>(uniformAlignment, properties.resourceHeapAlignment));
    if (offset >= properties.maxResourceHeapSize) return {};
    const auto room = static_cast<std::size_t>(properties.maxResourceHeapSize - offset) / BindlessHeap::RingChunkBytes * BindlessHeap::RingChunkBytes;
    const auto bytes = std::min(room, BindlessHeap::RingBytes);
    if (bytes < 2u * BindlessHeap::RingChunkBytes) return {};
    return {offset, bytes};
}

}

bool BindlessHeap::RingInResourceHeap(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, VkDeviceSize uniformAlignment) {
    return resourceRing(properties, uniformAlignment).bytes != 0u;
}

bool BindlessHeap::Fits(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties) {
    const auto sizes = heapSizes(properties);
    const bool resourcesFit = sizes.resourceUsed + properties.minResourceHeapReservedRange <= properties.maxResourceHeapSize;
    const bool samplersFit = sizes.samplerUsed + properties.minSamplerHeapReservedRangeWithEmbedded <= properties.maxSamplerHeapSize;
    return resourcesFit && samplersFit;
}

namespace {

std::uint32_t configuredSlotCap() {
    static const std::uint32_t cap = [] {
        std::uint32_t result = Abi::GpuImageHeapSlots;
        if (const char* value = std::getenv("APS5_HEAP_SLOT_CAP")) {
            const auto requested = static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10));
            if (requested != 0u) result = static_cast<std::uint32_t>(Abi::Binding::Samplers) + std::min(requested, Abi::GpuImageHeapSlots - static_cast<std::uint32_t>(Abi::Binding::Samplers));
        }
        return result;
    }();
    return cap;
}

}

BindlessHeap::BindlessHeap(const Context& context) : context(context) {
    Require(context.descriptorHeap, "the GPU heap needs VK_EXT_descriptor_heap");
    const auto& heapProperties = context.descriptorHeapProperties;
    const auto sizes = heapSizes(heapProperties);
    imageHeapStride = sizes.imageStride;
    samplerHeapStride = sizes.samplerStride;
    resourceHeapUsed = sizes.resourceUsed;
    pixelLdsHeapOffset = sizes.pixelLdsOffset;
    meshWindowHeapOffset = sizes.meshWindowOffset;
    samplerHeapUsed = sizes.samplerUsed;
    Require(Fits(heapProperties), "the GPU heap's slots exceed the device's descriptor heap sizes");
    static const bool hostDescriptors = std::getenv("APS5_NO_HEAP_DEVICE_DESCRIPTORS") != nullptr;
    constexpr VkMemoryPropertyFlags deviceVisible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const auto makeBuffer = [&](std::size_t bytes, VkBufferUsageFlags usage, bool deviceLocal) {
        if (deviceLocal) {
            try {
                return std::make_unique<Buffer>(context, bytes, usage, deviceVisible);
            } catch (const std::exception&) {
            }
        }
        return std::make_unique<Buffer>(context, bytes, usage);
    };
    constexpr VkBufferUsageFlags ringUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr VkBufferUsageFlags heapUsage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const auto heapRing = resourceRing(heapProperties, context.limits.minUniformBufferOffsetAlignment);
    ringInHeap = heapRing.bytes != 0u;
    const auto resourceBytes = ringInHeap ? static_cast<std::size_t>(heapRing.offset) + heapRing.bytes
                                          : static_cast<std::size_t>(resourceHeapUsed + heapProperties.minResourceHeapReservedRange);
    resourceHeap = makeBuffer(resourceBytes, ringInHeap ? heapUsage | ringUsage : heapUsage, ringInHeap || !hostDescriptors);
    samplerHeap = makeBuffer(static_cast<std::size_t>(samplerHeapUsed + heapProperties.minSamplerHeapReservedRangeWithEmbedded), heapUsage, !hostDescriptors);
    std::fill(resourceHeap->Bytes().begin(), resourceHeap->Bytes().end(), std::byte{0});
    std::fill(samplerHeap->Bytes().begin(), samplerHeap->Bytes().end(), std::byte{0});
    if (ringInHeap) {
        ringHeapOffset = heapRing.offset;
        ringBytes = heapRing.bytes;
        ringData = resourceHeap->Bytes().data() + ringHeapOffset;
        ringAlignment = static_cast<std::uint32_t>(std::max<VkDeviceSize>({context.limits.minStorageBufferOffsetAlignment, context.limits.minUniformBufferOffsetAlignment, 16}));
    } else {
        ring = makeBuffer(RingBytes, ringUsage, true);
        ringData = ring->Bytes().data();
        ringAlignment = static_cast<std::uint32_t>(std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 16));
    }
    ringChunks.resize(ringBytes / RingChunkBytes);
    nextTextureSlot = SamplerKind;
    slotCap = configuredSlotCap();
}

BindlessHeap::~BindlessHeap() = default;

namespace {

struct SweptStorageTag {};

std::vector<std::shared_ptr<void>>& sweptStorage() {
    return HostThreadLocal<std::vector<std::shared_ptr<void>>, SweptStorageTag>();
}

struct SweptStorageRelease {
    ~SweptStorageRelease() {
        auto released = std::move(sweptStorage());
        sweptStorage().clear();
    }
};

std::mutex& heapsMutex() {
    static auto& mutex = *new std::mutex();
    return mutex;
}

std::unordered_map<VkDevice, std::unique_ptr<BindlessHeap>>& heaps() {
    static auto& map = *new std::unordered_map<VkDevice, std::unique_ptr<BindlessHeap>>();
    return map;
}

std::atomic<std::uint64_t> heapsEpoch{0};

}

BindlessHeap& BindlessHeap::For(const Context& context) {
    struct Cached {
        VkDevice device = VK_NULL_HANDLE;
        BindlessHeap* heap = nullptr;
        std::uint64_t epoch = ~std::uint64_t{0};
    };
    thread_local Cached cached;
    const bool trims = CommitTrims();
    if (trims && cached.heap != nullptr && cached.device == context.device && cached.epoch == heapsEpoch.load(std::memory_order_acquire)) {
        return *cached.heap;
    }
    std::lock_guard lock(heapsMutex());
    auto& heap = heaps()[context.device];
    if (heap == nullptr) {
        heap = std::make_unique<BindlessHeap>(context);
        heapsEpoch.fetch_add(1, std::memory_order_acq_rel);
    }
    if (trims) cached = {context.device, heap.get(), heapsEpoch.load(std::memory_order_acquire)};
    return *heap;
}

void BindlessHeap::Release(VkDevice device) {
    std::unique_ptr<BindlessHeap> released;
    {
        std::lock_guard lock(heapsMutex());
        const auto found = heaps().find(device);
        if (found == heaps().end()) return;
        released = std::move(found->second);
        heaps().erase(found);
        heapsEpoch.fetch_add(1, std::memory_order_acq_rel);
    }
}

BindlessHeap* BindlessHeap::Existing(const Context& context) {
    std::lock_guard lock(heapsMutex());
    const auto found = heaps().find(context.device);
    return found == heaps().end() ? nullptr : found->second.get();
}

std::optional<std::uint32_t> BindlessHeap::TextureSlot(std::uint32_t kind, const std::shared_ptr<Texture>& texture, VkImageView view,
                                                       VkImageLayout layout) {
    const bool sampledKind = kind >= Abi::FirstImageBinding && kind < Abi::FirstStorageImageBinding;
    if (!sampledKind || texture == nullptr || view == VK_NULL_HANDLE)
        throw std::runtime_error("AGC bindless heap: invalid texture slot request");
    std::vector<std::shared_ptr<Texture>> dropped;
    SweptStorageRelease swept;
    std::lock_guard lock(mutex);
    const TextureKey key{kind, texture.get(), view, layout};
    if (const auto found = textureSlots.find(key); found != textureSlots.end()) return found->second;
    if (++slotRequests % 1024u == 0u) NoteSweepDue();
    const auto slot = takeOrSweepLocked(dropped);
    if (!slot) return std::nullopt;
    writeImageSlot(kind, *slot, view, layout);
    auto& owner = claimSlotLocked(key, *slot);
    owner.texture = texture;
    texture->AddHeapHold();
    noteTakenLocked(dropped);
    if (const auto& source = texture->SharedStorageSource(); source != nullptr && source->LeftCache()) NoteStorageEvicted();
    return slot;
}

std::optional<std::uint32_t> BindlessHeap::StorageSlot(std::uint32_t kind, std::shared_ptr<void> owner, VkImageView view) {
    return takeStorageSlot(kind, std::move(owner), view, nullptr);
}

std::optional<std::uint32_t> BindlessHeap::StorageSlot(std::uint32_t kind, const std::shared_ptr<StorageTexture>& image, VkImageView view) {
    return takeStorageSlot(kind, image, view, image.get());
}

std::optional<std::uint32_t> BindlessHeap::takeStorageSlot(std::uint32_t kind, std::shared_ptr<void> owner, VkImageView view, const StorageTexture* image) {
    if (!Abi::GpuHeapStorageKind(kind) || owner == nullptr || view == VK_NULL_HANDLE)
        throw std::runtime_error("AGC bindless heap: invalid storage slot request");
    std::vector<std::shared_ptr<Texture>> dropped;
    SweptStorageRelease swept;
    std::lock_guard lock(mutex);
    const TextureKey key{kind, owner.get(), view, VK_IMAGE_LAYOUT_GENERAL};
    if (const auto found = textureSlots.find(key); found != textureSlots.end()) return found->second;
    if (++slotRequests % 1024u == 0u) NoteSweepDue();
    const auto slot = takeOrSweepLocked(dropped);
    if (!slot) return std::nullopt;
    noteTakenLocked(dropped);
    writeImageSlot(kind, *slot, view, VK_IMAGE_LAYOUT_GENERAL);
    auto& claimed = claimSlotLocked(key, *slot);
    claimed.storage = std::move(owner);
    claimed.image = image;
    storageSlots[key.texture].push_back(*slot);
    if (image != nullptr && image->LeftCache()) NoteStorageEvicted();
    return slot;
}

std::optional<std::uint32_t> BindlessHeap::ViewSlot(std::uint32_t kind, const void* owner, VkImageView view) {
    const bool imageKind = kind >= Abi::FirstImageBinding && kind < SamplerKind;
    if (!imageKind || owner == nullptr || view == VK_NULL_HANDLE) throw std::runtime_error("AGC bindless heap: invalid view slot request");
    std::vector<std::shared_ptr<Texture>> dropped;
    SweptStorageRelease swept;
    std::lock_guard lock(mutex);
    const TextureKey key{kind, owner, view, VK_IMAGE_LAYOUT_GENERAL};
    if (const auto found = textureSlots.find(key); found != textureSlots.end()) return found->second;
    const auto slot = takeOrSweepLocked(dropped);
    if (!slot) return std::nullopt;
    noteTakenLocked(dropped);
    writeImageSlot(kind, *slot, view, VK_IMAGE_LAYOUT_GENERAL);
    claimSlotLocked(key, *slot);
    viewSlots[owner].push_back(*slot);
    return slot;
}

void BindlessHeap::ReleaseViewSlots(const void* owner) {
    std::lock_guard lock(mutex);
    const auto found = viewSlots.find(owner);
    if (found == viewSlots.end()) return;
    for (const auto slot : found->second) freeSlotLocked(slot);
    slotSerial.fetch_add(1, std::memory_order_release);
    viewSlots.erase(found);
    if (NearlyFull()) NoteSweepDue();
}

std::optional<std::uint32_t> BindlessHeap::takeOrSweepLocked(std::vector<std::shared_ptr<Texture>>& dropped) {
    auto slot = takeSlotLocked();
    if (!slot) {
        sweepLocked(dropped);
        slot = takeSlotLocked();
    }
    if (!slot) ++counts.full;
    return slot;
}

BindlessHeap::SlotOwner& BindlessHeap::claimSlotLocked(const TextureKey& key, std::uint32_t slot) {
    textureSlots.emplace(key, slot);
    if (owners.size() <= slot) owners.resize(slot + 1u);
    owners[slot].key = key;
    ++counts.textures;
    return owners[slot];
}

void BindlessHeap::freeSlotLocked(std::uint32_t slot) {
    auto& owner = owners[slot];
    if (const auto found = storageSlots.find(owner.key.texture); found != storageSlots.end()) {
        auto& slots = found->second;
        if (const auto at = std::find(slots.begin(), slots.end(), slot); at != slots.end()) slots.erase(at);
        if (slots.empty()) storageSlots.erase(found);
    }
    textureSlots.erase(owner.key);
    owner.image = nullptr;
    owner.key = {};
    ++owner.generation;
    freeSlots.push_back(slot);
    ++counts.slotsFreed;
}

const BindlessHeap::SlotOwner* BindlessHeap::ownerLocked(std::uint32_t slot) const {
    return slot < owners.size() ? &owners[slot] : nullptr;
}

std::optional<std::uint32_t> BindlessHeap::takeSlotLocked() {
    if (!freeSlots.empty()) {
        const auto slot = freeSlots.back();
        freeSlots.pop_back();
        ++counts.slotsReused;
        return slot;
    }
    if (nextTextureSlot >= slotCap) return std::nullopt;
    return nextTextureSlot++;
}

void BindlessHeap::sweepLocked(std::vector<std::shared_ptr<Texture>>& dropped) {
    ++counts.sweeps;
    const auto firstDropped = dropped.size();
    for (std::uint32_t slot = SamplerKind; slot < owners.size(); ++slot) {
        auto& owner = owners[slot];
        if (owner.storage != nullptr) {
            if (!heldOnlyHereLocked(owner.key.texture, owner.storage.use_count())) continue;
            dropStorageLocked(owner.key.texture, slot);
            continue;
        } else {
            const bool heldElsewhere =
                owner.texture == nullptr || owner.texture.use_count() > static_cast<long>(owner.texture->HeapHolds());
            if (heldElsewhere) continue;
            dropped.push_back(std::move(owner.texture));
        }
        freeSlotLocked(slot);
        slotSerial.fetch_add(1, std::memory_order_release);
    }
    for (auto index = firstDropped; index < dropped.size(); ++index) dropped[index]->DropHeapHold();
}

bool BindlessHeap::heldOnlyHereLocked(const void* owner, long uses) const {
    const auto found = storageSlots.find(owner);
    const auto slots = found != storageSlots.end() ? static_cast<long>(found->second.size()) : 1l;
    return uses <= slots;
}

bool BindlessHeap::releaseUncachedLocked(std::vector<std::shared_ptr<Texture>>& dropped, bool storagePhase) {
    bool heldElsewhere = false;
    const auto firstDropped = dropped.size();
    std::vector<std::pair<const void*, std::uint32_t>> images;
    for (std::uint32_t slot = SamplerKind; slot < owners.size(); ++slot) {
        auto& owner = owners[slot];
        if (owner.storage != nullptr) {
            if (!storagePhase) continue;
            if (owner.image == nullptr || !owner.image->LeftCache()) continue;
            const auto found = storageSlots.find(owner.key.texture);
            if (found == storageSlots.end() || found->second.front() != slot) continue;
            if (!heldOnlyHereLocked(owner.key.texture, owner.storage.use_count())) {
                heldElsewhere = true;
                continue;
            }
            images.emplace_back(owner.key.texture, slot);
            continue;
        }
        if (storagePhase || owner.texture == nullptr) continue;
        const auto& source = owner.texture->SharedStorageSource();
        if (source == nullptr || !source->LeftCache()) continue;
        if (owner.texture.use_count() > static_cast<long>(owner.texture->HeapHolds())) {
            heldElsewhere = true;
            continue;
        }
        heldElsewhere = true;
        dropped.push_back(std::move(owner.texture));
        freeSlotLocked(slot);
        slotSerial.fetch_add(1, std::memory_order_release);
    }
    for (auto index = firstDropped; index < dropped.size(); ++index) dropped[index]->DropHeapHold();
    for (const auto& [image, slot] : images) dropStorageLocked(image, slot);
    return heldElsewhere;
}

void BindlessHeap::dropStorageLocked(const void* image, std::uint32_t slot) {
    std::vector<std::uint32_t> slots{slot};
    if (const auto found = storageSlots.find(image); found != storageSlots.end()) slots = found->second;
    for (const auto held : slots) {
        sweptStorage().push_back(std::move(owners[held].storage));
        freeSlotLocked(held);
    }
    slotSerial.fetch_add(1, std::memory_order_release);
}

void BindlessHeap::sweepSamplersLocked(std::vector<std::shared_ptr<Sampler>>& dropped) {
    for (std::uint32_t slot = 0; slot < heldSamplers.size(); ++slot) {
        auto& held = heldSamplers[slot];
        if (held == nullptr || held.use_count() != 1) continue;
        samplerSlots.erase(held.get());
        dropped.push_back(std::move(held));
        ++samplerGenerations[slot];
        slotSerial.fetch_add(1, std::memory_order_release);
        freeSamplerSlots.push_back(slot);
        ++counts.slotsFreed;
    }
}

namespace {

constexpr std::uint32_t ImageSlotMargin = 1024u;
constexpr std::uint32_t SamplerSlotMargin = Abi::GpuSamplerHeapSlots / 4u;
std::atomic<bool> nearlyFull{false};

void noteNearlyFull(const char* what, std::uint32_t margin) {
    if (!nearlyFull.exchange(true, std::memory_order_relaxed))
        std::fprintf(stderr, "[gpu-heap] fewer than %u %s slots are free; draws and dispatches take the per-draw path until a sweep frees twice that many\n", margin, what);
}

}

bool BindlessHeap::NearlyFull() {
    return nearlyFull.load(std::memory_order_relaxed);
}

std::uint32_t BindlessHeap::ImageSlotCapacity() {
    return configuredSlotCap() - SamplerKind;
}

namespace {

std::atomic<bool> sweepDue{false};

std::atomic<bool> uncachedPending{false};
std::atomic<std::uint32_t> refusedSweepChecks{0};
constexpr std::uint32_t RefusedSweepInterval = 64u;

}

void BindlessHeap::NoteSweepDue() {
    sweepDue.store(true, std::memory_order_relaxed);
}

void BindlessHeap::NoteStorageEvicted() {
    uncachedPending.store(true, std::memory_order_relaxed);
}

void BindlessHeap::SweepIfDue() {
    const bool uncached = uncachedPending.load(std::memory_order_relaxed) && uncachedPending.exchange(false, std::memory_order_relaxed);
    const bool due = sweepDue.load(std::memory_order_relaxed) && sweepDue.exchange(false, std::memory_order_relaxed);
    const bool retry = !due && nearlyFull.load(std::memory_order_relaxed) &&
                       refusedSweepChecks.fetch_add(1, std::memory_order_relaxed) % RefusedSweepInterval == RefusedSweepInterval - 1u;
    if (!due && !retry && !uncached) return;
    bool again = false;
    bool room = true;
    {
        std::vector<std::shared_ptr<Texture>> dropped;
        SweptStorageRelease swept;
        std::vector<std::shared_ptr<Sampler>> droppedSamplers;
        std::lock_guard heapsLock(heapsMutex());
        for (auto& [device, heap] : heaps()) {
            if (heap == nullptr) continue;
            std::lock_guard lock(heap->mutex);
            if (due || retry) {
                heap->sweepLocked(dropped);
                heap->sweepSamplersLocked(droppedSamplers);
            }
            if (uncached && heap->releaseUncachedLocked(dropped, false)) again = true;
        }
    }
    {
        std::vector<std::shared_ptr<Texture>> none;
        SweptStorageRelease swept;
        std::lock_guard heapsLock(heapsMutex());
        for (auto& [device, heap] : heaps()) {
            if (heap == nullptr) continue;
            std::lock_guard lock(heap->mutex);
            if (uncached && heap->releaseUncachedLocked(none, true)) again = true;
            room = room && heap->roomLocked();
        }
    }
    if (again) NoteStorageEvicted();
    if (room && nearlyFull.exchange(false, std::memory_order_relaxed))
        std::fprintf(stderr, "[gpu-heap] a sweep left twice the margin of slots free; draws take the heap again\n");
}

std::uint32_t BindlessHeap::imageMargin() const {
    return std::max(1u, std::min(ImageSlotMargin, (slotCap - SamplerKind) / 4u));
}

std::size_t BindlessHeap::unusedLocked() const {
    return (nextTextureSlot < slotCap ? slotCap - nextTextureSlot : 0u) + freeSlots.size();
}

bool BindlessHeap::roomLocked() const {
    if (unusedLocked() < 2u * static_cast<std::size_t>(imageMargin())) return false;
    return Abi::GpuSamplerHeapSlots - heldSamplers.size() + freeSamplerSlots.size() >= 2u * SamplerSlotMargin;
}

void BindlessHeap::noteTakenLocked(std::vector<std::shared_ptr<Texture>>& dropped) {
    if (nearlyFull.load(std::memory_order_relaxed)) return;
    const auto margin = imageMargin();
    if (unusedLocked() >= margin) return;
    sweepLocked(dropped);
    if (unusedLocked() < margin) noteNearlyFull("image", margin);
}

void BindlessHeap::noteSamplerTakenLocked(std::vector<std::shared_ptr<Sampler>>& dropped) {
    if (nearlyFull.load(std::memory_order_relaxed)) return;
    const auto unused = [&] { return Abi::GpuSamplerHeapSlots - heldSamplers.size() + freeSamplerSlots.size(); };
    if (unused() >= SamplerSlotMargin) return;
    sweepSamplersLocked(dropped);
    if (unused() < SamplerSlotMargin) noteNearlyFull("sampler", SamplerSlotMargin);
}

std::uint32_t BindlessHeap::SamplerGeneration(std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    return slot < samplerGenerations.size() ? samplerGenerations[slot] : 0u;
}

bool BindlessHeap::GenerationsCurrent(std::span<const std::vector<std::uint32_t>> slots, std::span<const std::uint32_t> slotGenerations,
                                      std::span<const std::uint32_t> samplerSlots,
                                      std::span<const std::uint32_t> samplerGenerations) const {
    std::lock_guard lock(mutex);
    std::size_t generation = 0;
    for (std::uint32_t kind = 0; kind < slots.size(); ++kind) {
        for (const auto slot : slots[kind]) {
            const auto* owner = ownerLocked(slot);
            const auto current = owner != nullptr ? owner->generation : 0u;
            if (generation >= slotGenerations.size() || current != slotGenerations[generation++]) return false;
        }
    }
    const auto samplerGenerationOf = [&](std::uint32_t slot) {
        return slot < this->samplerGenerations.size() ? this->samplerGenerations[slot] : 0u;
    };
    for (std::size_t i = 0; i < samplerSlots.size(); ++i) {
        if (i >= samplerGenerations.size() || samplerGenerationOf(samplerSlots[i]) != samplerGenerations[i]) return false;
    }
    return true;
}

std::uint32_t BindlessHeap::SlotGeneration(std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    const auto* owner = ownerLocked(slot);
    return owner != nullptr ? owner->generation : 0u;
}

const Texture* BindlessHeap::SlotTexture(std::uint32_t kind, std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    const auto* owner = ownerLocked(slot);
    return owner != nullptr && owner->key.kind == kind ? owner->texture.get() : nullptr;
}

bool BindlessHeap::SlotHoldsStorage(std::uint32_t kind, std::uint32_t slot, const void* owner, VkImageView view) const {
    std::lock_guard lock(mutex);
    const auto* held = ownerLocked(slot);
    return held != nullptr && held->key == TextureKey{kind, owner, view, VK_IMAGE_LAYOUT_GENERAL};
}

std::uint32_t BindlessHeap::AbsentSlot(std::uint32_t kind, VkImageView view) {
    if (kind < Abi::FirstImageBinding || kind >= SamplerKind) throw std::runtime_error("AGC bindless heap: invalid absent slot request");
    std::lock_guard lock(mutex);
    if (absentSlots.contains(kind)) return kind;
    writeImageSlot(kind, kind, view, VK_IMAGE_LAYOUT_GENERAL);
    absentSlots.insert(kind);
    return kind;
}

std::optional<std::uint32_t> BindlessHeap::SamplerSlot(const std::shared_ptr<Sampler>& sampler) {
    if (sampler == nullptr) throw std::runtime_error("AGC bindless heap: invalid sampler slot request");
    std::vector<std::shared_ptr<Sampler>> dropped;
    std::lock_guard lock(mutex);
    if (const auto found = samplerSlots.find(sampler.get()); found != samplerSlots.end()) return found->second;
    if (freeSamplerSlots.empty()) sweepSamplersLocked(dropped);
    std::uint32_t slot = 0;
    if (!freeSamplerSlots.empty()) {
        slot = freeSamplerSlots.back();
        freeSamplerSlots.pop_back();
        ++counts.slotsReused;
    } else if (heldSamplers.size() < Abi::GpuSamplerHeapSlots) {
        slot = static_cast<std::uint32_t>(heldSamplers.size());
        heldSamplers.emplace_back();
        samplerGenerations.push_back(0u);
    } else {
        ++counts.full;
        return std::nullopt;
    }
    writeSamplerSlot(slot, sampler->Handle());
    samplerSlots.emplace(sampler.get(), slot);
    heldSamplers[slot] = sampler;
    ++counts.samplers;
    noteSamplerTakenLocked(dropped);
    return slot;
}

BindlessHeap::DataSpan BindlessHeap::Allocate(std::size_t words) {
    SpanMark mark;
    return Allocate(words, mark);
}

bool BindlessHeap::Held(const SpanMark& mark) {
    if (mark.entry == 0u || mark.words == 0u) return false;
    const auto first = mark.offset / RingChunkBytes;
    const auto last = (mark.offset + static_cast<std::size_t>(mark.words) * sizeof(std::uint32_t) - 1u) / RingChunkBytes;
    if (first != last) return false;
    std::lock_guard lock(mutex);
    auto& chunk = ringChunks[first];
    if (chunk.entries != mark.entry) {
        ++counts.spansExpired;
        spansExpiredWindow.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    chunk.open = true;
    ++counts.spansKept;
    spansKeptWindow.fetch_add(1, std::memory_order_relaxed);
    return true;
}

BindlessHeap::DataSpan BindlessHeap::Allocate(std::size_t words, SpanMark& mark) {
    const auto bytes = ringInHeap ? (words * sizeof(std::uint32_t) + 15u) / 16u * 16u : words * sizeof(std::uint32_t);
    if (bytes > RingChunkBytes) throw std::runtime_error("AGC bindless heap: per-draw data exceeds a ring chunk");
    std::lock_guard lock(mutex);
    auto offset = (ringCursor + ringAlignment - 1) / ringAlignment * ringAlignment;
    if (offset + bytes > ringBytes) {
        offset = 0;
        ++counts.ringWraps;
    }
    const auto lastChunk = [&] { return (offset + std::max<std::size_t>(bytes, 1u) - 1u) / RingChunkBytes; };
    const auto keptAhead = [&] {
        for (auto chunk = offset / RingChunkBytes; chunk <= lastChunk(); ++chunk) {
            if (chunk != ringChunk && ringChunks[chunk].open) return true;
        }
        return false;
    };
    for (std::size_t skipped = 0; keptAhead(); ++skipped) {
        if (skipped == ringChunks.size()) throw std::runtime_error("AGC bindless heap: one batch's shader data exceeds its ring");
        offset = (offset / RingChunkBytes + 1u) * RingChunkBytes;
        if (offset + bytes > ringBytes) {
            offset = 0;
            ++counts.ringWraps;
        }
        ++counts.keptChunksSkipped;
    }
    ringCursor = offset + bytes;
    for (auto chunk = offset / RingChunkBytes; chunk <= lastChunk(); ++chunk) enterChunk(ringChunks, chunk, ringChunk, "shader data");
    auto* base = reinterpret_cast<std::uint32_t*>(ringData + offset);
    ++counts.spansWritten;
    spansWrittenWindow.fetch_add(1, std::memory_order_relaxed);
    mark = {static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(words), ringChunks[offset / RingChunkBytes].entries};
    return {static_cast<std::uint32_t>(offset), std::span<std::uint32_t>(base, words)};
}

void BindlessHeap::enterChunk(std::vector<Chunk>& chunks, std::size_t index, std::size_t& current, const char* what) {
    if (index == current) return;
    current = index;
    auto& chunk = chunks[index];
    if (chunk.open) throw std::runtime_error(std::string("AGC bindless heap: one batch's ") + what + " exceeds its ring");
    if (chunk.serial != 0u) {
        ++counts.chunkWaits;
        if (!chunk.recorder->HasTimeline())
            throw std::runtime_error(std::string("AGC bindless heap: the ") + what +
                                     " ring needs the recorder timeline to wait for a batch");
        chunk.recorder->WaitSerial(chunk.serial);
        chunk.serial = 0u;
    }
    chunk.open = true;
    ++chunk.entries;
}

void BindlessHeap::writeSamplerSlot(std::uint32_t slot, VkSampler sampler) {
    auto record = FindSampler(sampler);
    Require(record.has_value(), "a GPU heap sampler has no recorded create info");
    const auto offset = static_cast<VkDeviceSize>(slot) * samplerHeapStride;
    Require(offset + samplerHeapStride <= samplerHeapUsed, "a GPU heap sampler slot lies outside the sampler heap");
    const VkHostAddressRangeEXT target{samplerHeap->Bytes().data() + offset, static_cast<std::size_t>(samplerHeapStride)};
    if (writeSamplerDescriptors == nullptr) writeSamplerDescriptors = context.Function<PFN_vkWriteSamplerDescriptorsEXT>("vkWriteSamplerDescriptorsEXT");
    check(writeSamplerDescriptors(context.device, 1, record->Chain(), &target), "vkWriteSamplerDescriptorsEXT");
}

void BindlessHeap::writeImageSlot(std::uint32_t kind, std::uint32_t slot, VkImageView view, VkImageLayout layout) {
    const auto offset = static_cast<VkDeviceSize>(slot) * imageHeapStride;
    Require(offset + imageHeapStride <= resourceHeapUsed, "a GPU heap image slot lies outside the resource heap");
    auto* target = resourceHeap->Bytes().data() + offset;
    if (view == VK_NULL_HANDLE) {
        std::memset(target, 0, static_cast<std::size_t>(imageHeapStride));
        return;
    }
    const bool storage = Abi::GpuHeapStorageKind(kind);
    const auto key = static_cast<std::uint32_t>(layout) | (storage ? 0x80000000u : 0u);
    const auto stride = static_cast<std::size_t>(imageHeapStride);
    if (CopyEncodedImageView(view, key, std::span<std::byte>(target, stride))) {
        ++counts.encodedCopies;
        imageCopiesWindow.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    auto record = FindImageView(view);
    Require(record.has_value(), "a GPU heap image view has no recorded create info");
    const VkImageDescriptorInfoEXT imageInfo{VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT, nullptr, record->Chain(), layout};
    VkResourceDescriptorInfoEXT resource{VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT};
    resource.type = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resource.data.pImage = &imageInfo;
    encodeScratch.resize(stride);
    const VkHostAddressRangeEXT range{encodeScratch.data(), stride};
    if (writeResourceDescriptors == nullptr) writeResourceDescriptors = context.Function<PFN_vkWriteResourceDescriptorsEXT>("vkWriteResourceDescriptorsEXT");
    check(writeResourceDescriptors(context.device, 1, &resource, &range), "vkWriteResourceDescriptorsEXT");
    std::memcpy(target, encodeScratch.data(), stride);
    KeepEncodedImageView(view, record->generation, key, encodeScratch);
    ++counts.encodes;
    imageEncodesWindow.fetch_add(1, std::memory_order_relaxed);
}

namespace {

const std::uint32_t GeneratedFaultBinding = Abi::BindingNumber(Abi::Stage::TessellationControl, Abi::Binding::FaultBuffer);

struct HeapBindingRoots {
    bool workgroupAddress = false;
    bool gdsAddress = false;
    bool faultAddress = false;
    bool pixelLds = false;
    bool meshWindow = false;
};

struct HeapBindingUse {
    std::vector<std::uint32_t> imageBindings;
    bool workgroupMemory = false;
    std::uint32_t gdsBinding = ~0u;
    bool faultMemory = false;
    bool pixelLdsMemory = false;
    bool meshWindowMemory = false;
    std::vector<std::uint32_t> dataUniforms;
    std::pair<std::uint32_t, std::uint32_t> refused{~0u, ~0u};
};

using SetAndBinding = std::pair<std::uint32_t, std::uint32_t>;

std::optional<std::unordered_map<std::uint32_t, SetAndBinding>> decoratedVariables(std::span<const std::uint32_t> spirv) {
    constexpr std::uint32_t opDecorate = 71u;
    constexpr std::uint32_t decorationBinding = 33u;
    constexpr std::uint32_t decorationSet = 34u;
    constexpr std::size_t headerWords = 5u;
    if (spirv.size() < headerWords) return std::nullopt;
    std::unordered_map<std::uint32_t, SetAndBinding> decorated;
    for (std::size_t at = headerWords; at < spirv.size();) {
        const auto count = spirv[at] >> 16u;
        if (count == 0u || at + count > spirv.size()) return std::nullopt;
        if ((spirv[at] & 0xffffu) == opDecorate && count >= 4u) {
            auto& entry = decorated.try_emplace(spirv[at + 1u], SetAndBinding{~0u, ~0u}).first->second;
            if (spirv[at + 2u] == decorationSet) entry.first = spirv[at + 3u];
            if (spirv[at + 2u] == decorationBinding) entry.second = spirv[at + 3u];
        }
        at += count;
    }
    return decorated;
}

bool heapBindings(std::span<const std::uint32_t> spirv, const HeapBindingRoots& roots, HeapBindingUse& use) {
    use = {};
    const auto decorated = decoratedVariables(spirv);
    if (!decorated) return false;
    constexpr auto bindingCount = static_cast<std::uint32_t>(Abi::Binding::Count);
    constexpr auto firstImage = bindingCount + Abi::FirstImageBinding;
    constexpr auto samplers = bindingCount + BindlessHeap::SamplerKind;
    for (const auto& [id, entry] : *decorated) {
        const auto [set, binding] = entry;
        if (set == ~0u && binding == ~0u) continue;
        if (roots.workgroupAddress && set == ShaderRecompiler::GpuWorkgroupMemoryDescriptorSet && binding == 0u) {
            use.workgroupMemory = true;
            continue;
        }
        if (roots.gdsAddress && set == Abi::GpuGdsSet && use.gdsBinding == ~0u) {
            use.gdsBinding = binding;
            continue;
        }
        if (roots.faultAddress && set == Abi::DescriptorSet && binding == GeneratedFaultBinding) {
            use.faultMemory = true;
            continue;
        }
        if (roots.pixelLds && !use.pixelLdsMemory && set == ShaderRecompiler::PixelFunctionLdsDescriptorSet && binding == 0u) {
            use.pixelLdsMemory = true;
            continue;
        }
        if (roots.meshWindow && !use.meshWindowMemory && set == ShaderRecompiler::MeshWindowDescriptorSet && binding == 0u) {
            use.meshWindowMemory = true;
            continue;
        }
        if (set == Abi::GpuDataSet && binding % bindingCount == static_cast<std::uint32_t>(Abi::Binding::ShaderData) && binding / bindingCount < Abi::StageCount) {
            use.dataUniforms.push_back(binding);
            continue;
        }
        const bool heapImageOrSampler = set == Abi::FragmentDescriptorSet && binding >= firstImage && binding <= samplers;
        if (!heapImageOrSampler) {
            use.refused = entry;
            return false;
        }
        if (std::find(use.imageBindings.begin(), use.imageBindings.end(), binding) == use.imageBindings.end())
            use.imageBindings.push_back(binding);
    }
    return (!roots.workgroupAddress || use.workgroupMemory) && (!roots.faultAddress || use.faultMemory) &&
           (!roots.pixelLds || use.pixelLdsMemory) && (!roots.meshWindow || use.meshWindowMemory);
}

VkDescriptorSetAndBindingMappingEXT heapMapping(std::uint32_t set, std::uint32_t binding, VkSpirvResourceTypeFlagsEXT resources,
                                                VkDeviceSize heapOffset, VkDeviceSize stride) {
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = set;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = resources;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mapping.sourceData.constantOffset.heapOffset = static_cast<std::uint32_t>(heapOffset);
    mapping.sourceData.constantOffset.heapArrayStride = static_cast<std::uint32_t>(stride);
    return mapping;
}

VkDescriptorSetAndBindingMappingEXT pushAddressMapping(std::uint32_t set, std::uint32_t binding, std::uint32_t pushOffset) {
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = set;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    mapping.sourceData.pushAddressOffset = pushOffset;
    return mapping;
}

constexpr std::uint32_t rootAddressOffset(std::uint32_t rootField) {
    return (Abi::RootDword + rootField) * sizeof(std::uint32_t);
}

}

bool DescriptorHeapMappable(std::span<const std::uint32_t> spirv, bool workgroupAddress, bool gdsAddress, bool pixelLds, bool meshWindow) {
    HeapBindingUse use;
    const bool mappable = heapBindings(spirv, {.workgroupAddress = workgroupAddress, .gdsAddress = gdsAddress, .pixelLds = pixelLds, .meshWindow = meshWindow}, use);
    static const bool stats = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    if (!mappable && stats) {
        static std::mutex causesMutex;
        static std::map<SetAndBinding, std::uint64_t> causes;
        std::lock_guard lock(causesMutex);
        const auto count = ++causes[use.refused];
        const bool powerOfTwo = (count & (count - 1u)) == 0u;
        if (powerOfTwo) {
            std::fprintf(
                stderr,
                "[gpu-heap] unmappable stage: descriptor set %d binding %d (workgroup address %d, gds address %d); %llu stage variants with this cause\n",
                static_cast<int>(use.refused.first), static_cast<int>(use.refused.second), workgroupAddress ? 1 : 0, gdsAddress ? 1 : 0,
                static_cast<unsigned long long>(count));
        }
    }
    return mappable;
}

bool BindlessHeap::DescriptorHeapMappings(std::span<const std::uint32_t> spirv, std::vector<VkDescriptorSetAndBindingMappingEXT>& mappings,
                                          std::uint32_t workgroupAddressOffset, bool rootGds, bool rootFault, bool pixelLds, bool meshWindow) const {
    mappings.clear();
    if (resourceHeap == nullptr) return false;
    HeapBindingUse use;
    const HeapBindingRoots roots{
        .workgroupAddress = workgroupAddressOffset != ~0u, .gdsAddress = rootGds, .faultAddress = rootFault, .pixelLds = pixelLds, .meshWindow = meshWindow};
    if (!heapBindings(spirv, roots, use)) return false;
    constexpr auto bindingCount = static_cast<std::uint32_t>(Abi::Binding::Count);
    constexpr VkSpirvResourceTypeFlagsEXT imageResources = VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT |
                                                           VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT |
                                                           VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
    std::sort(use.imageBindings.begin(), use.imageBindings.end());
    for (const auto binding : use.imageBindings) {
        const auto kind = binding - bindingCount;
        if (kind == SamplerKind)
            mappings.push_back(
                heapMapping(Abi::FragmentDescriptorSet, binding, VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT, 0, samplerHeapStride));
        else mappings.push_back(heapMapping(Abi::FragmentDescriptorSet, binding, imageResources, 0, imageHeapStride));
    }
    if (!use.dataUniforms.empty() && !ringInHeap) {
        mappings.clear();
        return false;
    }
    for (const auto binding : use.dataUniforms) {
        VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
        mapping.descriptorSet = Abi::GpuDataSet;
        mapping.firstBinding = binding;
        mapping.bindingCount = 1;
        mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT;
        mapping.sourceData.heapData.heapOffset = static_cast<std::uint32_t>(ringHeapOffset);
        mapping.sourceData.heapData.pushOffset = rootAddressOffset(Abi::RootStageDword(static_cast<Abi::Stage>(binding / bindingCount), Abi::RootStageField::ShaderData));
        mappings.push_back(mapping);
    }
    if (use.workgroupMemory)
        mappings.push_back(pushAddressMapping(ShaderRecompiler::GpuWorkgroupMemoryDescriptorSet, 0, workgroupAddressOffset));
    if (use.gdsBinding != ~0u)
        mappings.push_back(pushAddressMapping(Abi::GpuGdsSet, use.gdsBinding, rootAddressOffset(Abi::RootGdsAddress)));
    if (use.faultMemory)
        mappings.push_back(pushAddressMapping(Abi::DescriptorSet, GeneratedFaultBinding, rootAddressOffset(Abi::RootFaultAddress)));
    if (use.pixelLdsMemory)
        mappings.push_back(heapMapping(ShaderRecompiler::PixelFunctionLdsDescriptorSet, 0,
                                       VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT, pixelLdsHeapOffset, 0));
    if (use.meshWindowMemory)
        mappings.push_back(heapMapping(ShaderRecompiler::MeshWindowDescriptorSet, 0,
                                       VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT, meshWindowHeapOffset, 0));
    return true;
}

void BindlessHeap::writeBufferDescriptor(VkDeviceSize offset, VkDeviceAddress address, VkDeviceSize bytes, const char* what) {
    const auto& properties = context.descriptorHeapProperties;
    Require(offset + properties.bufferDescriptorSize <= resourceHeapUsed, std::string("the ") + what + " descriptor lies outside the resource heap");
    const VkDeviceAddressRangeEXT buffer{address, bytes};
    VkResourceDescriptorInfoEXT resource{VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT};
    resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resource.data.pAddressRange = &buffer;
    const VkHostAddressRangeEXT range{resourceHeap->Bytes().data() + offset, static_cast<std::size_t>(properties.bufferDescriptorSize)};
    Check(context.Function<PFN_vkWriteResourceDescriptorsEXT>("vkWriteResourceDescriptorsEXT")(context.device, 1, &resource, &range),
          (std::string("vkWriteResourceDescriptorsEXT ") + what).c_str());
}

void BindlessHeap::WritePixelLdsBuffer(VkDeviceAddress address, VkDeviceSize bytes) {
    writeBufferDescriptor(pixelLdsHeapOffset, address, bytes, "pixel function LDS");
}

void BindlessHeap::WriteMeshWindowBuffer(VkDeviceAddress address, VkDeviceSize bytes) {
    writeBufferDescriptor(meshWindowHeapOffset, address, bytes, "mesh window");
}

namespace {

std::mutex boundHeapsMutex;
std::unordered_set<VkCommandBuffer> boundHeaps;
std::atomic<std::uint64_t> heapBinds{0};
std::atomic<std::uint64_t> heapBindWindow{0};
std::atomic<std::uint64_t> commandBuffersBegun{0};
std::atomic<std::uint64_t> cpuBufferCopies{0};
std::atomic<std::uint64_t> cpuBufferCopyBytes{0};
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(SetBindSite::Count)> setBinds{};
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapFallback::Count)> heapFallbacks{};
std::array<std::atomic<std::uint64_t>, 4> heapRecords{};

std::size_t heapRecordIndex(bool compute, bool heap) {
    return (compute ? 2u : 0u) + (heap ? 1u : 0u);
}
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapStageReason::Count)> heapStageReasons{};
constexpr std::array<const char*, static_cast<std::size_t>(HeapStageReason::Count)> HeapStageReasonNames{"heap",
                                                                                                         "disabled",
                                                                                                         "pair refused",
                                                                                                         "fragment off heap",
                                                                                                         "stage kind",
                                                                                                         "no plan",
                                                                                                         "plan stage",
                                                                                                         "unresolved image source",
                                                                                                         "known ineligible"};
constexpr std::array<const char*, static_cast<std::size_t>(SetBindSite::Count)> SetBindSiteNames{"draw", "dispatch", "detiler",
                                                                                                 "color transfer", "presentation"};
constexpr std::array<const char*, static_cast<std::size_t>(HeapFallback::Count)> HeapFallbackNames{
    "draw unmapped", "draw mixed retry", "draw refused pair", "draw mixed reused", "dispatch unmapped"};

std::uint64_t takeCount(std::atomic<std::uint64_t>& counter) {
    return counter.exchange(0, std::memory_order_relaxed);
}

template <std::size_t N>
std::string takeNamedCounts(std::array<std::atomic<std::uint64_t>, N>& counters, const std::array<const char*, N>& names) {
    std::string text;
    for (std::size_t i = 0; i < N; ++i) text += " " + std::string(names[i]) + " " + std::to_string(takeCount(counters[i]));
    return text;
}

unsigned long long printed(std::uint64_t value) {
    return static_cast<unsigned long long>(value);
}

std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapCost::Count)> heapCostNs{};
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapCost::Count)> heapCostCalls{};
constexpr std::array<const char*, static_cast<std::size_t>(HeapCost::Count)> HeapCostNames{
    "stage prepare",  "heap stage call", "stage finish", "walk memo",        "walk",        "materialize", "template find",
    "template build", "slots",           "stage data",   "buffer addresses", "memo insert", "guards"};

void reportHeapStatistics() {
    static const bool enabled = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    if (!enabled) return;
    static std::atomic<std::uint64_t> calls{0};
    if ((calls.fetch_add(1, std::memory_order_relaxed) & 1023u) != 0u) return;
    static std::mutex reportMutex;
    static auto last = std::chrono::steady_clock::now();
    std::unique_lock lock(reportMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(10)) return;
    last = now;
    const auto binds = takeNamedCounts(setBinds, SetBindSiteNames);
    const auto fallbacks = takeNamedCounts(heapFallbacks, HeapFallbackNames);
    const auto stageReasons = takeNamedCounts(heapStageReasons, HeapStageReasonNames);
    std::string costs;
    for (std::size_t i = 0; i < heapCostNs.size(); ++i) {
        const auto calls = takeCount(heapCostCalls[i]);
        const auto ns = static_cast<double>(takeCount(heapCostNs[i]));
        const auto averageUs = calls != 0 ? ns / static_cast<double>(calls) / 1000.0 : 0.0;
        char cost[96];
        std::snprintf(cost, sizeof(cost), " %s %llu x %.2f us = %.0f ms", HeapCostNames[i], printed(calls), averageUs, ns / 1e6);
        costs += cost;
    }
    if (HeapCostTimed()) {
        const auto held = static_cast<double>(std::max<std::int64_t>(walkMemoHeld().load(std::memory_order_relaxed), 0)) / 1048576.0;
        const auto peak = static_cast<double>(walkMemoPeak().load(std::memory_order_relaxed)) / 1048576.0;
        std::fprintf(stderr, "[gpu-heap] cost (10 s):%s; walk memos hold %.1f MiB on all threads (peak %.1f MiB)\n", costs.c_str(), held, peak);
    }
    const auto heapDraws = takeCount(heapRecords[heapRecordIndex(false, true)]);
    const auto plainDraws = takeCount(heapRecords[heapRecordIndex(false, false)]);
    const auto heapDispatches = takeCount(heapRecords[heapRecordIndex(true, true)]);
    const auto plainDispatches = takeCount(heapRecords[heapRecordIndex(true, false)]);
    const auto heapBindsSeen = takeCount(heapBindWindow);
    const auto commandBuffers = takeCount(commandBuffersBegun);
    const auto copies = takeCount(cpuBufferCopies);
    const auto copiedMiB = takeCount(cpuBufferCopyBytes) >> 20u;
    const auto spansWritten = takeCount(spansWrittenWindow);
    const auto spansKept = takeCount(spansKeptWindow);
    const auto spansExpired = takeCount(spansExpiredWindow);
    const auto imageEncodes = takeCount(imageEncodesWindow);
    const auto imageCopies = takeCount(imageCopiesWindow);
    const auto templates = ShaderRecompiler::HeapTemplateCounts();
    std::fprintf(stderr,
                 "[gpu-heap] (10 s): draws %llu heap / %llu plain, dispatches %llu heap / %llu plain; "
                 "heap binds %llu over %llu command buffers; set binds:%s; fallbacks:%s; draw stages:%s; "
                 "CPU buffer copies %llu (%llu MiB); ring spans %llu written / %llu kept / %llu expired; image descriptors %llu encoded / %llu copied; templates %llu (%llu MiB, %llu evicted)\n",
                 printed(heapDraws), printed(plainDraws), printed(heapDispatches), printed(plainDispatches), printed(heapBindsSeen),
                 printed(commandBuffers), binds.c_str(), fallbacks.c_str(), stageReasons.c_str(), printed(copies), printed(copiedMiB),
                 printed(spansWritten), printed(spansKept), printed(spansExpired), printed(imageEncodes), printed(imageCopies), printed(templates.entries), printed(templates.bytes >> 20u), printed(templates.evicted));
}

void forgetHeapBinding(VkCommandBuffer commands) {
    if (!DescriptorHeapEnabled()) return;
    std::lock_guard lock(boundHeapsMutex);
    boundHeaps.erase(commands);
}

}

void NoteHeapRecord(bool compute, bool heap) {
    heapRecords[heapRecordIndex(compute, heap)].fetch_add(1, std::memory_order_relaxed);
    reportHeapStatistics();
}

void NoteHeapStageReason(HeapStageReason reason) {
    heapStageReasons[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

bool HeapCostTimed() {
    static const bool timed = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    return timed;
}

void NoteHeapCost(HeapCost cost, std::uint64_t nanoseconds) {
    heapCostNs[static_cast<std::size_t>(cost)].fetch_add(nanoseconds, std::memory_order_relaxed);
    heapCostCalls[static_cast<std::size_t>(cost)].fetch_add(1, std::memory_order_relaxed);
}

void NoteCpuBufferCopy(std::uint64_t bytes) {
    cpuBufferCopies.fetch_add(1, std::memory_order_relaxed);
    cpuBufferCopyBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void NoteHeapFallback(HeapFallback reason) {
    heapFallbacks[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t HeapFallbacks(HeapFallback reason) {
    return heapFallbacks[static_cast<std::size_t>(reason)].load(std::memory_order_relaxed);
}

void NoteCommandBufferBegun(VkCommandBuffer commands) {
    commandBuffersBegun.fetch_add(1, std::memory_order_relaxed);
    forgetHeapBinding(commands);
}

void CmdPushConstants(const Context& context, VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages,
                      std::uint32_t offset, std::uint32_t size, const void* values) {
    context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, layout, stages, offset, size, values);
    forgetHeapBinding(commands);
}

void CmdBindDescriptorSets(const Context& context, VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,
                           std::uint32_t firstSet, std::uint32_t setCount, const VkDescriptorSet* sets, SetBindSite site) {
    context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, bindPoint, layout, firstSet, setCount,
                                                                                         sets, 0, nullptr);
    setBinds[static_cast<std::size_t>(site)].fetch_add(1, std::memory_order_relaxed);
    forgetHeapBinding(commands);
}

std::uint64_t DescriptorHeapBinds() {
    return heapBinds.load(std::memory_order_relaxed);
}

void BindlessHeap::BindDescriptorHeaps(VkCommandBuffer commands) const {
    if (resourceHeap == nullptr) return;
    {
        std::lock_guard lock(boundHeapsMutex);
        if (!boundHeaps.insert(commands).second) return;
    }
    heapBinds.fetch_add(1, std::memory_order_relaxed);
    heapBindWindow.fetch_add(1, std::memory_order_relaxed);
    VkBindHeapInfoEXT info{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    info.heapRange = {resourceHeap->DeviceAddress(), ringInHeap ? ringHeapOffset + ringBytes : resourceHeapUsed + context.descriptorHeapProperties.minResourceHeapReservedRange};
    info.reservedRangeOffset = resourceHeapUsed;
    info.reservedRangeSize = context.descriptorHeapProperties.minResourceHeapReservedRange;
    context.Function<PFN_vkCmdBindResourceHeapEXT>("vkCmdBindResourceHeapEXT")(commands, &info);
    info.heapRange = {samplerHeap->DeviceAddress(),
                      samplerHeapUsed + context.descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded};
    info.reservedRangeOffset = samplerHeapUsed;
    info.reservedRangeSize = context.descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;
    context.Function<PFN_vkCmdBindSamplerHeapEXT>("vkCmdBindSamplerHeapEXT")(commands, &info);
}

VkDeviceAddress BindlessHeap::RingAddress() const {
    return ringInHeap ? resourceHeap->DeviceAddress() + ringHeapOffset : ring->DeviceAddress();
}

VkBuffer BindlessHeap::RingBuffer() const {
    return ringInHeap ? resourceHeap->Handle() : ring->Handle();
}

VkDeviceSize BindlessHeap::RingOffset() const {
    return ringInHeap ? ringHeapOffset : 0;
}

void BindlessHeap::Submitted(Recorder& recorder, std::uint64_t serial) {
    std::lock_guard lock(mutex);
    for (std::size_t index = 0; index < ringChunks.size(); ++index) {
        if (!ringChunks[index].open && index != ringChunk) continue;
        ringChunks[index].recorder = &recorder;
        ringChunks[index].serial = serial;
        ringChunks[index].open = index == ringChunk;
    }
}

BindlessHeap::Counts BindlessHeap::Statistics() const {
    std::lock_guard lock(mutex);
    return counts;
}

}
