#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "PushDataLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <type_traits>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <spirv/unified1/spirv.hpp>

#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace AgcDriver::Graphics {


namespace {

namespace Abi = ShaderRecompiler::RuntimeAbi;

constexpr std::size_t SpanAlignment = 16u;

bool pipelineStats() {
    static const bool enabled = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    return enabled;
}

bool isSampledKind(std::uint32_t kind) {
    return kind >= Abi::FirstImageBinding && kind < Abi::FirstStorageImageBinding;
}

bool isImageKind(std::uint32_t kind) {
    return kind >= Abi::FirstImageBinding && kind < BindlessHeap::SamplerKind;
}


std::atomic<std::uint64_t> heapComputeStages{0};
std::atomic<bool> descriptorHeapDevice{false};
std::atomic<std::uint64_t> imageTableBuilds{0};
std::atomic<std::uint64_t> dataAddressPushes{0};
std::atomic<std::uint64_t> pushedUserDataKeeps{0};
std::atomic<std::uint64_t> userWordsRewrites{0};
std::atomic<std::uint64_t> userWordsRewriteBlockWords{0};
std::atomic<std::uint64_t> userWordsRewriteUserWords{0};
std::atomic<std::uint64_t> embeddedSamplerMappings{0};
std::atomic<std::uint64_t> slotPressureSubmits{0};
std::atomic<std::uint64_t> heapSlotEvictions{0};
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

namespace {

template <typename TValue>
void countOnRecordingThread(std::atomic<TValue>& counter, std::type_identity_t<TValue> amount) {
    counter.store(counter.load(std::memory_order_relaxed) + amount, std::memory_order_relaxed);
}

}

void NoteDataAddressPush() {
    countOnRecordingThread(dataAddressPushes, 1);
}

std::uint64_t DataAddressPushes() {
    return dataAddressPushes.load(std::memory_order_relaxed);
}

void NotePushedUserDataKeep() {
    countOnRecordingThread(pushedUserDataKeeps, 1);
}

void NoteUserWordsRewrite(std::size_t blockWords, std::size_t userWords) {
    countOnRecordingThread(userWordsRewrites, 1);
    countOnRecordingThread(userWordsRewriteBlockWords, blockWords);
    countOnRecordingThread(userWordsRewriteUserWords, userWords);
}

std::uint64_t PushedUserDataKeeps() {
    return pushedUserDataKeeps.load(std::memory_order_relaxed);
}

std::uint64_t EmbeddedSamplerMappings() {
    return embeddedSamplerMappings.load(std::memory_order_relaxed);
}

std::uint64_t SlotPressureSubmits() {
    return slotPressureSubmits.load(std::memory_order_relaxed);
}

std::uint64_t HeapSlotEvictions() {
    return heapSlotEvictions.load(std::memory_order_relaxed);
}

void NoteEmptyResourceSet() {
    countOnRecordingThread(emptyResourceSets, 1);
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

std::uint32_t PushedBufferRecordCap() {
    static const std::uint32_t cap = [] {
        const char* value = std::getenv("APS5_PUSHED_BUFFER_RECORDS");
        return value != nullptr && *value != '\0' ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : ShaderRecompiler::RuntimeAbi::PushedBufferLimit;
    }();
    return cap;
}

namespace {

std::atomic<bool> uniformConstantBuffers{false};

}

bool ChooseUniformConstantBuffers(bool standardLayout, const VkPhysicalDeviceLimits& limits) {
    const bool chosen = standardLayout && limits.maxUniformBufferRange >= ShaderRecompiler::RuntimeAbi::UniformConstantBufferBytes &&
                        limits.minUniformBufferOffsetAlignment <= ShaderRecompiler::RuntimeAbi::UniformViewAdjustmentLimit + 1u;
    uniformConstantBuffers.store(chosen, std::memory_order_relaxed);
    return chosen;
}

bool UniformConstantBuffersEnabled() {
    return DescriptorHeapEnabled() && uniformConstantBuffers.load(std::memory_order_relaxed);
}

namespace {

struct HeapSizes {
    std::uint32_t imageSlots = 0;
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

constexpr std::uint32_t MaximumImageSlots = 1u << 19u;

HeapSizes heapLayout(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, std::uint32_t slots) {
    HeapSizes sizes;
    sizes.imageSlots = slots;
    sizes.imageStride = alignUp(properties.imageDescriptorSize, properties.imageDescriptorAlignment);
    sizes.samplerStride = alignUp(properties.samplerDescriptorSize, properties.samplerDescriptorAlignment);
    const auto imageBytes = static_cast<VkDeviceSize>(slots) * sizes.imageStride;
    sizes.pixelLdsOffset = alignUp(imageBytes, properties.bufferDescriptorAlignment);
    sizes.meshWindowOffset = alignUp(sizes.pixelLdsOffset + properties.bufferDescriptorSize, properties.bufferDescriptorAlignment);
    sizes.resourceUsed = alignUp(sizes.meshWindowOffset + properties.bufferDescriptorSize, properties.resourceHeapAlignment);
    sizes.samplerUsed = alignUp(static_cast<VkDeviceSize>(Abi::GpuSamplerHeapSlots) * sizes.samplerStride, properties.samplerHeapAlignment);
    return sizes;
}

VkDeviceSize layoutEnd(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties, const HeapSizes& sizes) {
    return sizes.resourceUsed + properties.minResourceHeapReservedRange;
}

HeapSizes heapSizes(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties) {
    const auto base = heapLayout(properties, Abi::GpuImageHeapSlots);
    const auto end = layoutEnd(properties, base);
    if (end >= properties.maxResourceHeapSize || base.imageStride == 0u) return base;
    auto slots = static_cast<std::uint32_t>(std::min<VkDeviceSize>(MaximumImageSlots, Abi::GpuImageHeapSlots + (properties.maxResourceHeapSize - end) / base.imageStride));
    while (slots > Abi::GpuImageHeapSlots) {
        const auto grown = heapLayout(properties, slots);
        const auto grownEnd = layoutEnd(properties, grown);
        if (grownEnd <= properties.maxResourceHeapSize) return grown;
        const auto over = grownEnd - properties.maxResourceHeapSize;
        const auto step = static_cast<std::uint32_t>(std::min<VkDeviceSize>(slots, over / base.imageStride + 1u));
        slots = slots - step > Abi::GpuImageHeapSlots ? slots - step : Abi::GpuImageHeapSlots;
    }
    return base;
}

}

bool BindlessHeap::Fits(const VkPhysicalDeviceDescriptorHeapPropertiesEXT& properties) {
    const auto sizes = heapLayout(properties, Abi::GpuImageHeapSlots);
    const bool resourcesFit = sizes.resourceUsed + properties.minResourceHeapReservedRange <= properties.maxResourceHeapSize;
    const bool samplersFit = sizes.samplerUsed + properties.minSamplerHeapReservedRangeWithEmbedded <= properties.maxSamplerHeapSize;
    return resourcesFit && samplersFit;
}


namespace {

std::uint32_t configuredSlotCap(std::uint32_t slots) {
    static const std::uint32_t requested = [] {
        const char* value = std::getenv("APS5_HEAP_SLOT_CAP");
        return value != nullptr ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : 0u;
    }();
    if (requested == 0u || slots <= BindlessHeap::SamplerKind) return slots;
    return BindlessHeap::SamplerKind + std::min(requested, slots - BindlessHeap::SamplerKind);
}

std::atomic<std::uint32_t> deviceImageSlots{Abi::GpuImageHeapSlots};

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
    constexpr VkMemoryPropertyFlags deviceVisible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const auto makeBuffer = [&](std::size_t bytes, VkBufferUsageFlags usage) {
        try {
            return std::make_unique<Buffer>(context, bytes, usage, deviceVisible);
        } catch (const std::exception&) {
        }
        return std::make_unique<Buffer>(context, bytes, usage);
    };
    constexpr VkBufferUsageFlags heapUsage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    resourceHeap = makeBuffer(static_cast<std::size_t>(resourceHeapUsed + heapProperties.minResourceHeapReservedRange), heapUsage);
    samplerHeap = makeBuffer(static_cast<std::size_t>(samplerHeapUsed + heapProperties.minSamplerHeapReservedRangeWithEmbedded), heapUsage);
    std::fill(resourceHeap->Bytes().begin(), resourceHeap->Bytes().end(), std::byte{0});
    std::fill(samplerHeap->Bytes().begin(), samplerHeap->Bytes().end(), std::byte{0});
    writeResourceDescriptors = context.Function<PFN_vkWriteResourceDescriptorsEXT>("vkWriteResourceDescriptorsEXT");
    poolAlignment = static_cast<std::uint32_t>(std::max<VkDeviceSize>({context.limits.minStorageBufferOffsetAlignment, context.limits.minUniformBufferOffsetAlignment, SpanAlignment}));
    slotCap = configuredSlotCap(sizes.imageSlots);
    deviceImageSlots.store(slotCap, std::memory_order_relaxed);
    slotUses = std::make_unique<std::atomic<std::uint32_t>[]>(slotCap);
    imagePool.Configure(SamplerKind, slotCap);
    samplerPool.Configure(0u, Abi::GpuSamplerHeapSlots);
    samplerUses = std::make_unique<std::atomic<std::uint32_t>[]>(Abi::GpuSamplerHeapSlots);
}

namespace {

constexpr std::uint32_t ImageSlotMargin = 1024u;
constexpr std::uint32_t EvictIdleSubmissions = 2u;
constexpr std::uint32_t SamplerSlotMargin = Abi::GpuSamplerHeapSlots / 4u;
std::atomic<std::uint32_t> refusedHeaps{0};

}

BindlessHeap::~BindlessHeap() {
    if (refused) refusedHeaps.fetch_sub(1, std::memory_order_relaxed);
}

namespace {

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

namespace {

struct CachedHeap {
    VkDevice device = VK_NULL_HANDLE;
    BindlessHeap* heap = nullptr;
    std::uint64_t epoch = ~std::uint64_t{0};
};

CachedHeap& ThreadCachedHeap() {
    thread_local CachedHeap cached;
    return cached;
}

}

BindlessHeap& BindlessHeap::For(const Context& context) {
    auto& cached = ThreadCachedHeap();
    if (cached.heap != nullptr && cached.device == context.device && cached.epoch == heapsEpoch.load(std::memory_order_acquire)) {
        return *cached.heap;
    }
    std::lock_guard lock(heapsMutex());
    auto& heap = heaps()[context.device];
    if (heap == nullptr) {
        heap = std::make_unique<BindlessHeap>(context);
        heapsEpoch.fetch_add(1, std::memory_order_acq_rel);
    }
    cached = {context.device, heap.get(), heapsEpoch.load(std::memory_order_acquire)};
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
    auto& cached = ThreadCachedHeap();
    if (cached.heap != nullptr && cached.device == context.device && cached.epoch == heapsEpoch.load(std::memory_order_acquire)) return cached.heap;
    std::lock_guard lock(heapsMutex());
    const auto found = heaps().find(context.device);
    if (found == heaps().end()) return nullptr;
    cached = {context.device, found->second.get(), heapsEpoch.load(std::memory_order_acquire)};
    return found->second.get();
}

std::optional<std::uint32_t> BindlessHeap::TextureSlot(std::uint32_t kind, Texture* texture, VkImageView view, VkImageLayout layout) {
    Require(isSampledKind(kind) && texture != nullptr && view != VK_NULL_HANDLE, "bindless heap: invalid texture slot request");
    const HeapSlotOwner* owner = texture;
    return imageSlot(kind, owner, view, layout, texture);
}

std::optional<std::uint32_t> BindlessHeap::StorageSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view) {
    Require(Abi::GpuHeapStorageKind(kind) && owner != nullptr && view != VK_NULL_HANDLE, "bindless heap: invalid storage slot request");
    return imageSlot(kind, owner, view, VK_IMAGE_LAYOUT_GENERAL, nullptr);
}

std::optional<std::uint32_t> BindlessHeap::ViewSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view) {
    Require(isImageKind(kind) && owner != nullptr && view != VK_NULL_HANDLE, "bindless heap: invalid view slot request");
    return imageSlot(kind, owner, view, VK_IMAGE_LAYOUT_GENERAL, nullptr);
}

std::optional<std::uint32_t> BindlessHeap::imageSlot(std::uint32_t kind, const HeapSlotOwner* owner, VkImageView view, VkImageLayout layout, Texture* texture) {
    std::lock_guard lock(mutex);
    const ImageSlotKey key{kind, owner, view, layout};
    if (const auto found = imageSlots.find(key); found != imageSlots.end()) {
        touchLocked(found->second);
        return found->second;
    }
    bool reused = false;
    auto slot = imagePool.Take(reused);
    if (!slot && reclaimLocked(true)) slot = imagePool.Take(reused);
    if (!slot) return std::nullopt;
    if (reused) ++counts.slotsReused;
    writeImageSlot(kind, *slot, view, layout);
    claimSlotLocked(key, *slot).texture = texture;
    touchLocked(*slot);
    owner->heapSlotted.store(true, std::memory_order_release);
    noteTakenLocked();
    return slot;
}

void BindlessHeap::ReleaseSlots(const HeapSlotOwner* owner) {
    std::lock_guard lock(mutex);
    bool released = false;
    if (const auto found = ownerSlots.find(owner); found != ownerSlots.end()) {
        const auto slots = std::move(found->second);
        ownerSlots.erase(found);
        for (const auto slot : slots) freeSlotLocked(slot);
        released = true;
    }
    if (const auto found = samplerSlots.find(owner); found != samplerSlots.end()) {
        samplerOwners[found->second] = nullptr;
        samplerPinned[found->second] = false;
        samplerPool.Free(found->second);
        samplerSlots.erase(found);
        ++counts.slotsFreed;
        released = true;
    }
    if (!released) return;
    slotSerial.fetch_add(1, std::memory_order_release);
    noteReleasedLocked();
}

BindlessHeap::SlotOwner& BindlessHeap::claimSlotLocked(const ImageSlotKey& key, std::uint32_t slot) {
    imageSlots.emplace(key, slot);
    ownerSlots[key.owner].push_back(slot);
    if (owners.size() <= slot) owners.resize(slot + 1u);
    owners[slot].key = key;
    return owners[slot];
}

void BindlessHeap::freeSlotLocked(std::uint32_t slot) {
    auto& owner = owners[slot];
    imageSlots.erase(owner.key);
    owner.texture = nullptr;
    owner.key = {};
    imagePool.Free(slot);
    ++counts.slotsFreed;
}

const BindlessHeap::SlotOwner* BindlessHeap::ownerLocked(std::uint32_t slot) const {
    return slot < owners.size() ? &owners[slot] : nullptr;
}

std::optional<std::uint32_t> BindlessHeap::SlotPool::Take(bool& reused) {
    reused = false;
    if (!free.empty()) {
        const auto slot = free.back();
        free.pop_back();
        reused = true;
        return slot;
    }
    if (next >= limit) return std::nullopt;
    return next++;
}

void BindlessHeap::SlotPool::Configure(std::uint32_t first, std::uint32_t slots) {
    next = first;
    limit = slots;
    generations = std::make_unique<std::atomic<std::uint32_t>[]>(slots);
}

void BindlessHeap::SlotPool::Retire(std::uint32_t slot) {
    generations[slot].fetch_add(1u, std::memory_order_release);
}

void BindlessHeap::SlotPool::Free(std::uint32_t slot) {
    Retire(slot);
    free.push_back(slot);
}

std::size_t BindlessHeap::SlotPool::Unused() const {
    return (next < limit ? limit - next : 0u) + free.size();
}

std::uint32_t BindlessHeap::SlotPool::Generation(std::uint32_t slot) const {
    return slot < limit ? generations[slot].load(std::memory_order_acquire) : 0u;
}

bool BindlessHeap::NearlyFull() {
    return refusedHeaps.load(std::memory_order_relaxed) != 0u;
}




std::uint32_t BindlessHeap::ImageSlotCapacity() {
    return deviceImageSlots.load(std::memory_order_relaxed) - SamplerKind;
}

std::uint32_t BindlessHeap::imageMargin() const {
    return std::max(1u, std::min(ImageSlotMargin, (slotCap - SamplerKind) / 4u));
}

bool BindlessHeap::roomLocked() const {
    if (imagePool.Unused() + retiringAssigned < 2u * static_cast<std::size_t>(imageMargin())) return false;
    return samplerPool.Unused() + samplerRetiringAssigned >= 2u * SamplerSlotMargin;
}

void BindlessHeap::noteRefusedLocked(const char* what, std::uint32_t margin) {
    if (refused) return;
    refused = true;
    refusedHeaps.fetch_add(1, std::memory_order_relaxed);
    pressured.store(true, std::memory_order_relaxed);
    std::fprintf(stderr, "[gpu-heap] fewer than %u %s slots are free; the next draw or dispatch submits its batch so idle slots can be evicted\n", margin, what);
}

void BindlessHeap::noteTakenLocked() {
    const auto margin = imageMargin();
    if (imagePool.Unused() >= 2u * static_cast<std::size_t>(margin)) return;
    if (imagePool.Unused() + imageRetiring() < 2u * static_cast<std::size_t>(margin)) evictLocked(EvictIdleSubmissions);
    if (!refused && imagePool.Unused() + retiringAssigned < margin) noteRefusedLocked("image", margin);
}

void BindlessHeap::touchLocked(std::uint32_t slot) const {
    if (slot < slotCap) slotUses[slot].store(useEpoch.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void BindlessHeap::touchSamplerLocked(std::uint32_t slot) const {
    if (slot < Abi::GpuSamplerHeapSlots) samplerUses[slot].store(useEpoch.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

std::uint32_t BindlessHeap::TouchSlots(std::span<const std::uint32_t> slots, std::span<const std::uint32_t> samplerSlots, std::uint32_t touched) const {
    const auto epoch = useEpoch.load(std::memory_order_relaxed);
    if (touched == epoch) return epoch;
    for (const auto slot : slots) {
        if (slot < slotCap) slotUses[slot].store(epoch, std::memory_order_relaxed);
    }
    for (const auto slot : samplerSlots) {
        if (slot < Abi::GpuSamplerHeapSlots) samplerUses[slot].store(epoch, std::memory_order_relaxed);
    }
    return epoch;
}

void BindlessHeap::evictLocked(std::uint32_t idleSubmissions) {
    const auto epoch = useEpoch.load(std::memory_order_relaxed);
    if (submitter == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    if (idleSubmissions != 0u && evictScanEpoch == epoch) return;
    if (idleSubmissions != 0u) evictScanEpoch = epoch;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> idle;
    for (std::uint32_t slot = SamplerKind; slot < imagePool.next && slot < owners.size(); ++slot) {
        if (owners[slot].key.owner == nullptr) continue;
        const auto use = slotUses[slot].load(std::memory_order_relaxed);
        if (use + idleSubmissions <= epoch) idle.emplace_back(use, slot);
    }
    const auto wanted = std::min<std::size_t>(idle.size(), 4u * static_cast<std::size_t>(imageMargin()));
    if (wanted == 0u) return;
    std::nth_element(idle.begin(), idle.begin() + static_cast<std::ptrdiff_t>(wanted - 1u), idle.end());
    for (std::size_t index = 0; index < wanted; ++index) {
        const auto slot = idle[index].second;
        auto& owner = owners[slot];
        imageSlots.erase(owner.key);
        if (const auto found = ownerSlots.find(owner.key.owner); found != ownerSlots.end()) {
            auto& held = found->second;
            held.erase(std::remove(held.begin(), held.end(), slot), held.end());
            if (held.empty()) ownerSlots.erase(found);
        }
        owner.texture = nullptr;
        owner.key = {};
        imagePool.Retire(slot);
        retiring.push_back({slot, 0u});
        ++counts.slotsEvicted;
        heapSlotEvictions.fetch_add(1, std::memory_order_relaxed);
    }
    slotSerial.fetch_add(1, std::memory_order_release);
}

void BindlessHeap::evictSamplersLocked(std::uint32_t idleSubmissions) {
    const auto epoch = useEpoch.load(std::memory_order_relaxed);
    if (submitter == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    if (idleSubmissions != 0u && samplerEvictScanEpoch == epoch) return;
    if (idleSubmissions != 0u) samplerEvictScanEpoch = epoch;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> idle;
    for (std::uint32_t slot = 0; slot < samplerPool.next && slot < samplerOwners.size(); ++slot) {
        if (samplerOwners[slot] == nullptr || samplerPinned[slot]) continue;
        const auto use = samplerUses[slot].load(std::memory_order_relaxed);
        if (use + idleSubmissions <= epoch) idle.emplace_back(use, slot);
    }
    const auto wanted = std::min<std::size_t>(idle.size(), 2u * static_cast<std::size_t>(SamplerSlotMargin));
    if (wanted == 0u) return;
    std::nth_element(idle.begin(), idle.begin() + static_cast<std::ptrdiff_t>(wanted - 1u), idle.end());
    for (std::size_t index = 0; index < wanted; ++index) {
        const auto slot = idle[index].second;
        samplerSlots.erase(samplerOwners[slot]);
        samplerOwners[slot] = nullptr;
        samplerPool.Retire(slot);
        retiring.push_back({slot, 0u, true});
        ++retiringSamplers;
        ++counts.samplersEvicted;
        heapSlotEvictions.fetch_add(1, std::memory_order_relaxed);
    }
    slotSerial.fetch_add(1, std::memory_order_release);
}

bool BindlessHeap::reclaimLocked(bool wait) {
    bool reclaimed = false;
    while (!retiring.empty() && retiring.front().serial != 0u && submitter != nullptr) {
        const auto serial = retiring.front().serial;
        if (!submitter->WaitSerialFor(serial, 0u)) {
            if (!wait || reclaimed || serial > submitter->Submissions()) break;
            submitter->WaitSerial(serial);
            ++counts.evictionWaits;
        }
        while (!retiring.empty() && retiring.front().serial != 0u && retiring.front().serial <= serial) {
            const auto entry = retiring.front();
            retiring.pop_front();
            if (entry.sampler) {
                samplerPool.free.push_back(entry.slot);
                --retiringSamplers;
                --samplerRetiringAssigned;
            } else {
                imagePool.free.push_back(entry.slot);
                --retiringAssigned;
            }
            reclaimed = true;
        }
    }
    if (reclaimed) noteReleasedLocked();
    return reclaimed;
}

void BindlessHeap::noteSamplerTakenLocked() {
    if (samplerPool.Unused() >= 2u * SamplerSlotMargin) return;
    if (samplerPool.Unused() + retiringSamplers < 2u * SamplerSlotMargin) evictSamplersLocked(EvictIdleSubmissions);
    if (!refused && samplerPool.Unused() + samplerRetiringAssigned < SamplerSlotMargin) noteRefusedLocked("sampler", SamplerSlotMargin);
}

std::optional<std::uint32_t> BindlessHeap::PinnedSamplerSlot(const std::shared_ptr<Sampler>& sampler) {
    return samplerSlot(sampler, true);
}

void BindlessHeap::noteReleasedLocked() {
    if (!refused || !roomLocked()) return;
    refused = false;
    refusedHeaps.fetch_sub(1, std::memory_order_relaxed);
    pressured.store(false, std::memory_order_relaxed);
}

std::uint32_t BindlessHeap::SamplerGeneration(std::uint32_t slot) const {
    if (slot == Abi::NoEmbeddedSampler) return 0u;
    return samplerPool.Generation(slot);
}

bool BindlessHeap::GenerationsCurrent(std::span<const std::uint32_t> slots, std::span<const std::uint32_t> slotGenerations,
                                      std::span<const std::uint32_t> samplerSlots,
                                      std::span<const std::uint32_t> samplerGenerations) const {
    if (slots.size() != slotGenerations.size()) return false;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (imagePool.Generation(slots[i]) != slotGenerations[i]) return false;
    }
    for (std::size_t i = 0; i < samplerSlots.size(); ++i) {
        if (samplerSlots[i] == Abi::NoEmbeddedSampler) continue;
        if (i >= samplerGenerations.size() || samplerPool.Generation(samplerSlots[i]) != samplerGenerations[i]) return false;
    }
    return true;
}

std::uint32_t BindlessHeap::SlotGeneration(std::uint32_t slot) const {
    return imagePool.Generation(slot);
}

const Texture* BindlessHeap::SlotTexture(std::uint32_t kind, std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    const auto* owner = ownerLocked(slot);
    return owner != nullptr && owner->key.kind == kind ? owner->texture : nullptr;
}

bool BindlessHeap::SlotHoldsStorage(std::uint32_t kind, std::uint32_t slot, const HeapSlotOwner* owner, VkImageView view) const {
    std::lock_guard lock(mutex);
    const auto* held = ownerLocked(slot);
    return held != nullptr && held->key == ImageSlotKey{kind, owner, view, VK_IMAGE_LAYOUT_GENERAL};
}

std::uint32_t BindlessHeap::AbsentSlot(std::uint32_t kind, VkImageView view) {
    Require(isImageKind(kind), "bindless heap: invalid absent slot request");
    std::lock_guard lock(mutex);
    if (absentSlots.contains(kind)) return kind;
    writeImageSlot(kind, kind, view, VK_IMAGE_LAYOUT_GENERAL);
    absentSlots.insert(kind);
    return kind;
}

std::optional<std::uint32_t> BindlessHeap::SamplerSlot(const std::shared_ptr<Sampler>& sampler) {
    return samplerSlot(sampler, false);
}

std::optional<std::uint32_t> BindlessHeap::samplerSlot(const std::shared_ptr<Sampler>& sampler, bool pin) {
    Require(sampler != nullptr, "bindless heap: invalid sampler slot request");
    std::lock_guard lock(mutex);
    if (const auto found = samplerSlots.find(static_cast<const HeapSlotOwner*>(sampler.get())); found != samplerSlots.end()) {
        touchSamplerLocked(found->second);
        if (pin) samplerPinned[found->second] = true;
        return found->second;
    }
    bool reused = false;
    auto slot = samplerPool.Take(reused);
    if (!slot && reclaimLocked(true)) slot = samplerPool.Take(reused);
    if (!slot) return std::nullopt;
    if (reused) ++counts.slotsReused;
    writeSamplerSlot(*slot, sampler->Handle());
    const HeapSlotOwner* owner = sampler.get();
    samplerSlots.emplace(owner, *slot);
    if (samplerOwners.size() <= *slot) {
        samplerOwners.resize(*slot + 1u, nullptr);
        samplerPinned.resize(*slot + 1u, false);
    }
    samplerOwners[*slot] = owner;
    samplerPinned[*slot] = pin;
    touchSamplerLocked(*slot);
    owner->heapSlotted.store(true, std::memory_order_release);
    noteSamplerTakenLocked();
    return slot;
}

BindlessHeap::DataSpan BindlessHeap::Allocate(std::size_t words) {
    SpanMark mark;
    return Allocate(words, mark);
}

BindlessHeap::SpanScope::SpanScope(const BindlessHeap& heap) : heap(heap), acquired(heap.spanOwner.load(std::memory_order_relaxed) != std::this_thread::get_id()) {
    if (!acquired) return;
    heap.spanMutex.lock();
    heap.spanOwner.store(std::this_thread::get_id(), std::memory_order_relaxed);
}

BindlessHeap::SpanScope::~SpanScope() {
    if (!acquired) return;
    heap.spanOwner.store(std::thread::id{}, std::memory_order_relaxed);
    heap.spanMutex.unlock();
}

bool BindlessHeap::Held(const SpanMark& mark) {
    if (mark.entry == 0u || mark.words == 0u) return false;
    SpanScope scope(*this);
    return heldLocked(mark);
}

bool BindlessHeap::heldLocked(const SpanMark& mark) {
    if (mark.entry == 0u || mark.words == 0u) return false;
    if (mark.block != 0u) {
        const auto index = mark.block - 1u;
        if (index >= poolBlocks.size() || poolBlocks[index].generation != mark.entry) {
            ++counts.spansExpired;
            return false;
        }
        poolBlocks[index].open = true;
        ++counts.spansKept;
        return true;
    }
    return false;
}

void BindlessHeap::Write(const DataSpan& span, std::span<const std::uint32_t> words) {
    Require(words.size() <= span.words.size(), "the words written to a GPU heap span exceed it");
#if defined(__SSE2__) || defined(_M_X64)
    std::size_t index = 0;
    for (; index + 4u <= words.size(); index += 4u)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(span.words.data() + index), _mm_loadu_si128(reinterpret_cast<const __m128i*>(words.data() + index)));
    auto* tail = reinterpret_cast<volatile std::uint32_t*>(span.words.data());
    for (; index < words.size(); ++index) tail[index] = words[index];
#else
    std::memcpy(span.words.data(), words.data(), words.size() * sizeof(std::uint32_t));
#endif
}

BindlessHeap::DataSpan BindlessHeap::Allocate(std::size_t words, SpanMark& mark) {
    SpanScope scope(*this);
    return allocateLocked(words, mark);
}

std::optional<BindlessHeap::DataSpan> BindlessHeap::KeepOrAllocate(const SpanScope& held, std::size_t words, SpanMark& mark, bool same) {
    Require(&held.heap == this, "a heap span was kept or allocated under another heap's lock");
    if (same && heldLocked(mark)) return std::nullopt;
    return allocateLocked(words, mark);
}

std::size_t BindlessHeap::makeTableBlockLocked(std::size_t bytes) {
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr VkMemoryPropertyFlags deviceVisible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    std::unique_ptr<Buffer> buffer;
    try {
        buffer = std::make_unique<Buffer>(context, bytes, usage, deviceVisible);
    } catch (const std::exception&) {
    }
    if (buffer == nullptr) buffer = std::make_unique<Buffer>(context, bytes, usage);
    const auto vacant = std::find_if(tableBlocks.begin(), tableBlocks.end(), [](const TableBlock& block) { return block.buffer == nullptr; });
    auto& block = vacant != tableBlocks.end() ? *vacant : tableBlocks.emplace_back();
    block.data = buffer->Bytes().data();
    block.address = buffer->DeviceAddress();
    block.buffer = std::move(buffer);
    block.bytes = bytes;
    block.cursor = 0;
    return static_cast<std::size_t>(&block - tableBlocks.data());
}

std::optional<BindlessHeap::DataSpan> BindlessHeap::AllocateTable(const SpanScope& held, std::size_t words, TableMark& mark) {
    Require(&held.heap == this, "a heap table was allocated under another heap's lock");
    const auto bytes = std::max<std::size_t>(words, 1u) * sizeof(std::uint32_t);
    std::uint32_t sizeClass = 0;
    while (sizeClass < TableClasses && (static_cast<std::size_t>(poolAlignment) << sizeClass) < bytes) ++sizeClass;
    const bool dedicated = sizeClass == TableClasses || (static_cast<std::size_t>(poolAlignment) << sizeClass) > TableBlockBytes;
    std::pair<std::uint32_t, std::uint32_t> place{};
    std::size_t placedBytes = 0;
    if (dedicated) {
        placedBytes = static_cast<std::size_t>(alignUp(bytes, poolAlignment));
        place = {static_cast<std::uint32_t>(makeTableBlockLocked(placedBytes) + 1u), 0u};
        sizeClass = TableClasses;
    } else {
        placedBytes = static_cast<std::size_t>(poolAlignment) << sizeClass;
        auto& free = tableFree[sizeClass];
        if (!free.empty()) {
            place = free.back();
            free.pop_back();
        } else {
            if (tableShared == 0u || tableBlocks[tableShared - 1u].cursor + placedBytes > tableBlocks[tableShared - 1u].bytes) tableShared = makeTableBlockLocked(TableBlockBytes) + 1u;
            auto& block = tableBlocks[tableShared - 1u];
            place = {static_cast<std::uint32_t>(tableShared), static_cast<std::uint32_t>(block.cursor)};
            block.cursor += placedBytes;
        }
    }
    const auto& block = tableBlocks[place.first - 1u];
    mark = {place.first, place.second, sizeClass, block.address + place.second};
    ++counts.spansWritten;
    counts.poolBytes += placedBytes;
    return DataSpan{place.second, std::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(block.data + place.second), words), mark.address};
}

void BindlessHeap::RetireTable(const SpanScope& held, TableMark& mark) {
    Require(&held.heap == this, "a heap table was retired under another heap's lock");
    if (mark.address == 0u) return;
    retiredTables.push_back({mark, nullptr, 0u, false});
    mark = {};
}

void BindlessHeap::NoteTableKept(const SpanScope& held) {
    Require(&held.heap == this, "a heap table was kept under another heap's lock");
    ++counts.spansKept;
}

void BindlessHeap::reclaimTablesLocked() {
    while (!retiredTables.empty()) {
        const auto& front = retiredTables.front();
        if (!front.submitted || (front.serial != 0u && !front.recorder->WaitSerialFor(front.serial, 0u))) return;
        if (front.mark.sizeClass == TableClasses) {
            auto& block = tableBlocks[front.mark.block - 1u];
            block.buffer.reset();
            block.data = nullptr;
            block.address = 0;
            block.bytes = 0;
        } else {
            tableFree[front.mark.sizeClass].emplace_back(front.mark.block, front.mark.offset);
        }
        retiredTables.pop_front();
    }
}

BindlessHeap::DataSpan BindlessHeap::allocateLocked(std::size_t words, SpanMark& mark) {
    const auto bytes = alignUp(std::max<std::size_t>(words, 1u) * sizeof(std::uint32_t), SpanAlignment);
    if (poolCurrent >= poolBlocks.size() || alignUp(poolBlocks[poolCurrent].cursor, poolAlignment) + bytes > poolBlocks[poolCurrent].bytes) poolCurrent = takePoolBlockLocked(bytes);
    auto& block = poolBlocks[poolCurrent];
    const auto offset = static_cast<std::size_t>(alignUp(block.cursor, poolAlignment));
    block.cursor = offset + bytes;
    block.open = true;
    ++counts.spansWritten;
    counts.poolBytes += bytes;
    counts.spanPeakBytes = std::max<std::uint64_t>(counts.spanPeakBytes, bytes);
    poolBatchBytes += bytes;
    mark = {static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(words), block.generation, static_cast<std::uint32_t>(poolCurrent + 1u), block.address + offset};
    return {static_cast<std::uint32_t>(offset), std::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(block.data + offset), words), mark.address};
}

bool BindlessHeap::poolBlockDoneLocked(PoolBlock& block) {
    if (block.serial == 0u) return true;
    if (block.recorder == nullptr || !block.recorder->WaitSerialFor(block.serial, 0u)) return false;
    block.serial = 0u;
    return true;
}

std::size_t BindlessHeap::takePoolBlockLocked(std::size_t bytes) {
    const auto need = std::max(bytes, PoolBlockBytes);
    auto best = poolBlocks.size();
    std::size_t vacant = poolBlocks.size();
    for (std::size_t index = 0; index < poolBlocks.size(); ++index) {
        auto& block = poolBlocks[index];
        if (block.buffer == nullptr) {
            vacant = std::min(vacant, index);
            continue;
        }
        if (index == poolCurrent || block.open || block.bytes < need) continue;
        if (best < poolBlocks.size() && poolBlocks[best].bytes <= block.bytes) continue;
        if (poolBlockDoneLocked(block)) best = index;
    }
    if (best < poolBlocks.size()) {
        auto& block = poolBlocks[best];
        block.cursor = 0;
        ++block.generation;
        ++counts.poolBlocksRecycled;
        return best;
    }
    const auto size = need == PoolBlockBytes ? PoolBlockBytes : static_cast<std::size_t>(alignUp(need, PoolBlockBytes));
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr VkMemoryPropertyFlags deviceVisible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    std::unique_ptr<Buffer> buffer;
    try {
        buffer = std::make_unique<Buffer>(context, size, usage, deviceVisible);
    } catch (const std::exception&) {
    }
    if (buffer == nullptr) buffer = std::make_unique<Buffer>(context, size, usage);
    if (vacant == poolBlocks.size()) poolBlocks.emplace_back();
    auto& block = poolBlocks[vacant];
    block.data = buffer->Bytes().data();
    block.address = buffer->DeviceAddress();
    block.buffer = std::move(buffer);
    block.bytes = size;
    block.cursor = 0;
    ++block.generation;
    block.recorder = nullptr;
    block.serial = 0u;
    block.open = false;
    ++counts.poolBlocksMade;
    return vacant;
}

void BindlessHeap::trimPoolLocked() {
    std::size_t idle = 0;
    for (std::size_t index = 0; index < poolBlocks.size(); ++index) {
        const auto& block = poolBlocks[index];
        if (block.buffer != nullptr && index != poolCurrent && !block.open) idle += block.bytes;
    }
    for (std::size_t index = poolBlocks.size(); index-- > 0u && idle > PoolIdleBytes;) {
        auto& block = poolBlocks[index];
        if (block.buffer == nullptr || index == poolCurrent || block.open || !poolBlockDoneLocked(block)) continue;
        idle -= block.bytes;
        block.buffer.reset();
        block.data = nullptr;
        block.address = 0;
        block.bytes = 0;
        ++block.generation;
        ++counts.poolBlocksFreed;
    }
}

bool BindlessHeap::TakeSlotPressure() {
    if (!pressured.load(std::memory_order_relaxed)) return false;
    std::lock_guard lock(mutex);
    if (!refused) return false;
    if (imagePool.Unused() + imageRetiring() < 2u * static_cast<std::size_t>(imageMargin())) evictLocked(0u);
    if (samplerPool.Unused() + retiringSamplers < 2u * SamplerSlotMargin) evictSamplersLocked(0u);
    ++counts.slotPressureSubmits;
    slotPressureSubmits.fetch_add(1, std::memory_order_relaxed);
    return true;
}

std::span<const std::uint32_t> BindlessHeap::DataWords(VkDeviceAddress address, std::size_t words) const {
    SpanScope scope(*this);
    const auto bytes = words * sizeof(std::uint32_t);
    for (const auto& block : poolBlocks) {
        if (block.buffer != nullptr && address >= block.address && address + bytes <= block.address + block.bytes) return {reinterpret_cast<const std::uint32_t*>(block.data + (address - block.address)), words};
    }
    for (const auto& block : tableBlocks) {
        if (block.buffer != nullptr && address >= block.address && address + bytes <= block.address + block.bytes) return {reinterpret_cast<const std::uint32_t*>(block.data + (address - block.address)), words};
    }
    throw std::runtime_error("AGC bindless heap: the words read lie outside every data block");
}

std::pair<VkBuffer, VkDeviceSize> BindlessHeap::DataLocation(VkDeviceAddress address) const {
    SpanScope scope(*this);
    for (const auto& block : poolBlocks) {
        if (block.buffer != nullptr && address >= block.address && address < block.address + block.bytes) return {block.buffer->Handle(), address - block.address};
    }
    for (const auto& block : tableBlocks) {
        if (block.buffer != nullptr && address >= block.address && address < block.address + block.bytes) return {block.buffer->Handle(), address - block.address};
    }
    throw std::runtime_error("AGC bindless heap: shader data lies outside every data pool block");
}

void BindlessHeap::writeSamplerSlot(std::uint32_t slot, VkSampler sampler) {
    auto record = FindSampler(sampler);
    Require(record.has_value(), "a GPU heap sampler has no recorded create info");
    const auto offset = static_cast<VkDeviceSize>(slot) * samplerHeapStride;
    Require(offset + samplerHeapStride <= samplerHeapUsed, "a GPU heap sampler slot lies outside the sampler heap");
    const VkHostAddressRangeEXT target{samplerHeap->Bytes().data() + offset, static_cast<std::size_t>(samplerHeapStride)};
    if (writeSamplerDescriptors == nullptr) writeSamplerDescriptors = context.Function<PFN_vkWriteSamplerDescriptorsEXT>("vkWriteSamplerDescriptorsEXT");
    Check(writeSamplerDescriptors(context.device, 1, record->Chain(), &target), "vkWriteSamplerDescriptorsEXT");
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
    Check(writeResourceDescriptors(context.device, 1, &resource, &range), "vkWriteResourceDescriptorsEXT");
    std::memcpy(target, encodeScratch.data(), stride);
    KeepEncodedImageView(view, record->generation, key, encodeScratch);
    ++counts.encodes;
}

namespace {

const std::uint32_t GeneratedFaultBinding = Abi::BindingNumber(Abi::Stage::TessellationControl, Abi::Binding::FaultBuffer);

struct HeapBindingRoots {
    bool workgroupAddress = false;
    bool gdsAddress = false;
    bool faultAddress = false;
    bool pixelLds = false;
    bool meshWindow = false;
    bool scratchPool = false;
};

using SetAndBinding = std::pair<std::uint32_t, std::uint32_t>;

struct HeapBindingUse {
    std::vector<std::uint32_t> imageBindings;
    bool workgroupMemory = false;
    std::uint32_t gdsBinding = ~0u;
    bool faultMemory = false;
    bool pixelLdsMemory = false;
    bool meshWindowMemory = false;
    bool scratchPoolMemory = false;
    std::vector<std::uint32_t> dataUniforms;
    std::vector<std::uint32_t> embeddedSamplers;
    std::vector<std::uint32_t> pushedBuffers;
    std::pair<std::uint32_t, std::uint32_t> refused{~0u, ~0u};
};

std::optional<std::unordered_map<std::uint32_t, SetAndBinding>> decoratedVariables(std::span<const std::uint32_t> spirv) {
    constexpr std::size_t HeaderWords = 5u;
    constexpr std::uint32_t DecorateWords = 4u;
    if (spirv.size() < HeaderWords) return std::nullopt;
    std::unordered_map<std::uint32_t, SetAndBinding> decorated;
    for (std::size_t at = HeaderWords; at < spirv.size();) {
        const auto count = spirv[at] >> 16u;
        if (count == 0u || at + count > spirv.size()) return std::nullopt;
        if ((spirv[at] & 0xffffu) == static_cast<std::uint32_t>(spv::OpDecorate) && count >= DecorateWords) {
            auto& entry = decorated.try_emplace(spirv[at + 1u], SetAndBinding{~0u, ~0u}).first->second;
            if (spirv[at + 2u] == static_cast<std::uint32_t>(spv::DecorationDescriptorSet)) entry.first = spirv[at + 3u];
            if (spirv[at + 2u] == static_cast<std::uint32_t>(spv::DecorationBinding)) entry.second = spirv[at + 3u];
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
        if (roots.scratchPool && set == ShaderRecompiler::GpuScratchPoolDescriptorSet && binding == 0u) {
            use.scratchPoolMemory = true;
            continue;
        }
        const auto dataKind = binding % bindingCount;
        const bool dataBlock = dataKind == static_cast<std::uint32_t>(Abi::Binding::ShaderData) || dataKind == static_cast<std::uint32_t>(Abi::Binding::UserData);
        if (set == Abi::GpuDataSet && dataBlock && binding / bindingCount < Abi::StageCount) {
            use.dataUniforms.push_back(binding);
            continue;
        }
        if (set == Abi::EmbeddedSamplerSet) {
            use.embeddedSamplers.push_back(binding);
            continue;
        }
        if (set == Abi::PushedBufferSet) {
            if (std::ranges::find(use.pushedBuffers, binding) == use.pushedBuffers.end()) use.pushedBuffers.push_back(binding);
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
           (!roots.pixelLds || use.pixelLdsMemory) && (!roots.meshWindow || use.meshWindowMemory) && (!roots.scratchPool || use.scratchPoolMemory);
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

std::uint32_t rootItemOffset(const ShaderRecompiler::StagePushPlacement* placement, Abi::RootItem item) {
    Require(placement != nullptr && placement->Item(item) != Abi::NoPushPosition, "a heap mapping reads a root item its pipeline's push layout does not hold");
    return placement->Item(item) * static_cast<std::uint32_t>(sizeof(std::uint32_t));
}

VkDescriptorSetAndBindingMappingEXT pushedBufferMapping(std::uint32_t binding, const ShaderRecompiler::StagePushPlacement* placement) {
    Require(placement != nullptr && binding < placement->pushedBuffers, "a heap mapping reads a pushed buffer its pipeline's push layout does not count");
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = Abi::PushedBufferSet;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
    const auto root = binding < Abi::PushedBufferLimit ? placement->bufferRoots[binding] : Abi::NoPushPosition;
    if (root != Abi::NoPushPosition) {
        mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
        mapping.sourceData.pushAddressOffset = root * static_cast<std::uint32_t>(sizeof(std::uint32_t));
        return mapping;
    }
    Require(placement->bufferOverflow != Abi::NoPushPosition, "a pushed buffer outside the push data has no overflow block");
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT;
    mapping.sourceData.indirectAddress.pushOffset = placement->bufferOverflow * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    mapping.sourceData.indirectAddress.addressOffset = binding * Abi::OverflowRecordWords * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    return mapping;
}

VkDescriptorSetAndBindingMappingEXT dataUniformMapping(std::uint32_t binding, const ShaderRecompiler::StagePushPlacement* placement) {
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = Abi::GpuDataSet;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    const bool user = binding % static_cast<std::uint32_t>(Abi::Binding::Count) == static_cast<std::uint32_t>(Abi::Binding::UserData);
    const bool userPlaced = user && placement != nullptr && placement->Item(Abi::RootItem::UserData) != Abi::NoPushPosition;
    mapping.sourceData.pushAddressOffset = rootItemOffset(placement, userPlaced ? Abi::RootItem::UserData : Abi::RootItem::ShaderData);
    return mapping;
}

}

bool DescriptorHeapMappable(std::span<const std::uint32_t> spirv, bool workgroupAddress, bool gdsAddress, bool pixelLds, bool meshWindow, bool scratchPool,
                            std::pair<std::uint32_t, std::uint32_t>* refused) {
    HeapBindingUse use;
    const bool mappable = heapBindings(spirv, {.workgroupAddress = workgroupAddress, .gdsAddress = gdsAddress, .pixelLds = pixelLds, .meshWindow = meshWindow, .scratchPool = scratchPool}, use);
    if (!mappable && refused != nullptr) *refused = use.refused;
    if (!mappable && pipelineStats()) {
        static std::mutex causesMutex;
        static std::map<SetAndBinding, std::uint64_t> causes;
        std::lock_guard lock(causesMutex);
        const auto count = ++causes[use.refused];
        if (std::has_single_bit(count)) {
            std::fprintf(
                stderr,
                "[gpu-heap] unmappable stage: descriptor set %d binding %d (workgroup address %d, gds address %d); %llu stage variants with this cause\n",
                static_cast<int>(use.refused.first), static_cast<int>(use.refused.second), workgroupAddress ? 1 : 0, gdsAddress ? 1 : 0,
                static_cast<unsigned long long>(count));
        }
    }
    return mappable;
}

namespace {

constexpr std::uint32_t InternalEmbeddedSamplers = 8u;

std::uint32_t embeddedSamplerLimit(const Context& context) {
    static const auto cap = [] {
        const char* value = std::getenv("APS5_EMBEDDED_SAMPLER_CAP");
        return value != nullptr ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : ~0u;
    }();
    const auto device = context.descriptorHeapProperties.maxDescriptorHeapEmbeddedSamplers;
    return std::min(cap, device > InternalEmbeddedSamplers ? device - InternalEmbeddedSamplers : 0u);
}

}

std::uint32_t BindlessHeap::EmbeddedSamplerCount() const {
    std::lock_guard lock(embeddedMutex);
    return embeddedSamplerCount;
}

std::optional<VkDescriptorSetAndBindingMappingEXT> BindlessHeap::embeddedSamplerMapping(const ShaderRecompiler::DescriptorBinding& binding, std::size_t element) const {
    if (binding.samplerEmbeddedWords.size() < (element + 1u) * 4u || element >= binding.samplerDepthCompare.size()) return std::nullopt;
    std::array<std::uint32_t, 4> words{};
    std::copy_n(binding.samplerEmbeddedWords.begin() + static_cast<std::ptrdiff_t>(element * 4u), 4u, words.begin());
    if (element < binding.samplerCoordinatesIgnored.size() && binding.samplerCoordinatesIgnored[element]) words[0] &= ~(1u << 15u);
    const bool compare = binding.samplerDepthCompare[element];
    const bool unnormalized = element < binding.samplerUnnormalized.size() && binding.samplerUnnormalized[element];
    const bool integerBorder = element < binding.samplerIntegerBorder.size() && binding.samplerIntegerBorder[element];
    const std::array<std::uint32_t, 5> key{words[0], words[1], words[2], words[3], (compare ? 1u : 0u) | (unnormalized ? 2u : 0u) | (integerBorder ? 4u : 0u)};
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = Abi::EmbeddedSamplerSet;
    mapping.firstBinding = binding.samplerEmbedded[element];
    mapping.bindingCount = 1;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    std::lock_guard lock(embeddedMutex);
    auto found = embeddedSamplers.find(key);
    if (found == embeddedSamplers.end()) {
        auto resource = DecodeSamplerResource(words, unnormalized, true, integerBorder);
        resource.compareEnable = compare;
        EmbeddedSampler made;
        made.info = Sampler::CreateInfo(context, resource, made.reduction);
        if (embeddedSamplerCount < embeddedSamplerLimit(context)) ++embeddedSamplerCount;
        else made.heapSampler = std::make_shared<Sampler>(context, resource);
        found = embeddedSamplers.emplace(key, std::move(made)).first;
        if (found->second.info.pNext != nullptr) found->second.info.pNext = &found->second.reduction;
    }
    auto& entry = found->second;
    if (entry.heapSampler == nullptr) {
        mapping.sourceData.constantOffset.pEmbeddedSampler = &entry.info;
        embeddedSamplerMappings.fetch_add(1, std::memory_order_relaxed);
        return mapping;
    }
    if (entry.slot == ~0u) {
        const auto slot = const_cast<BindlessHeap*>(this)->PinnedSamplerSlot(entry.heapSampler);
        Require(slot.has_value(), "the global sampler heap is full");
        entry.slot = *slot;
    }
    mapping.sourceData.constantOffset.heapOffset = static_cast<std::uint32_t>(entry.slot * samplerHeapStride);
    return mapping;
}

bool BindlessHeap::DescriptorHeapMappings(std::span<const std::uint32_t> spirv, std::vector<VkDescriptorSetAndBindingMappingEXT>& mappings,
                                          std::uint32_t workgroupAddressOffset, bool rootGds, bool rootFault, bool pixelLds, bool meshWindow,
                                          bool scratchPool, const ShaderRecompiler::StagePushPlacement* placement,
                                          const std::vector<ShaderRecompiler::DescriptorBinding>* bindings) const {
    mappings.clear();
    if (resourceHeap == nullptr) return false;
    HeapBindingUse use;
    const HeapBindingRoots roots{
        .workgroupAddress = workgroupAddressOffset != ~0u, .gdsAddress = rootGds, .faultAddress = rootFault, .pixelLds = pixelLds, .meshWindow = meshWindow,
        .scratchPool = scratchPool};
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
    std::sort(use.embeddedSamplers.begin(), use.embeddedSamplers.end());
    for (const auto binding : use.embeddedSamplers) {
        if (bindings == nullptr) return false;
        std::optional<VkDescriptorSetAndBindingMappingEXT> mapping;
        for (const auto& candidate : *bindings) {
            if (candidate.role != ShaderRecompiler::DescriptorRole::GuestSamplers) continue;
            const auto found = std::ranges::find(candidate.samplerEmbedded, binding);
            if (found == candidate.samplerEmbedded.end()) continue;
            mapping = embeddedSamplerMapping(candidate, static_cast<std::size_t>(found - candidate.samplerEmbedded.begin()));
            break;
        }
        if (!mapping) return false;
        mappings.push_back(*mapping);
    }
    for (const auto binding : use.dataUniforms) mappings.push_back(dataUniformMapping(binding, placement));
    std::ranges::sort(use.pushedBuffers);
    for (const auto binding : use.pushedBuffers) mappings.push_back(pushedBufferMapping(binding, placement));
    if (use.workgroupMemory)
        mappings.push_back(pushAddressMapping(ShaderRecompiler::GpuWorkgroupMemoryDescriptorSet, 0, workgroupAddressOffset));
    if (use.scratchPoolMemory)
        mappings.push_back(pushAddressMapping(ShaderRecompiler::GpuScratchPoolDescriptorSet, 0, ShaderRecompiler::GpuScratchPoolAddressPushByte));
    if (use.gdsBinding != ~0u)
        mappings.push_back(pushAddressMapping(Abi::GpuGdsSet, use.gdsBinding, rootItemOffset(placement, Abi::RootItem::GdsAddress)));
    if (use.faultMemory)
        mappings.push_back(pushAddressMapping(Abi::DescriptorSet, GeneratedFaultBinding, rootItemOffset(placement, Abi::RootItem::FaultAddress)));
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
std::atomic<std::uint64_t> boundHeapsEpoch{0};
std::atomic<std::uint64_t> heapBinds{0};
std::atomic<std::uint64_t> heapBindWindow{0};
std::atomic<std::uint64_t> commandBuffersBegun{0};
std::atomic<std::uint64_t> cpuBufferCopies{0};
std::atomic<std::uint64_t> cpuBufferCopyBytes{0};
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(SetBindSite::Count)> setBinds{};
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapExcluded::Count)> heapExcludedCounts{};
std::array<std::atomic<std::uint64_t>, 4> heapRecords{};

std::size_t heapRecordIndex(bool compute, bool heap) {
    return (compute ? 2u : 0u) + (heap ? 1u : 0u);
}
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeapStageReason::Count)> heapStageReasons{};
constexpr std::array<const char*, static_cast<std::size_t>(HeapStageReason::Count)> HeapStageReasonNames{"heap", "disabled", "excluded"};
constexpr std::array<const char*, static_cast<std::size_t>(SetBindSite::Count)> SetBindSiteNames{"draw", "dispatch", "detiler",
                                                                                                 "color transfer", "presentation", "depth transfer", "staged merge", "first-sample copy"};
constexpr std::array<const char*, static_cast<std::size_t>(HeapExcluded::Count)> HeapExcludedNames{"draw excluded", "dispatch excluded"};

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
    "stage prepare", "heap stage call", "stage finish", "walk", "materialize", "template find", "template build", "slots", "stage data", "guards"};

void reportHeapStatistics() {
    if (!pipelineStats()) return;
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
    const auto excluded = takeNamedCounts(heapExcludedCounts, HeapExcludedNames);
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
        std::fprintf(stderr, "[gpu-heap] cost (10 s):%s\n", costs.c_str());
    }
    const auto heapDraws = takeCount(heapRecords[heapRecordIndex(false, true)]);
    const auto plainDraws = takeCount(heapRecords[heapRecordIndex(false, false)]);
    const auto heapDispatches = takeCount(heapRecords[heapRecordIndex(true, true)]);
    const auto plainDispatches = takeCount(heapRecords[heapRecordIndex(true, false)]);
    const auto heapBindsSeen = takeCount(heapBindWindow);
    const auto commandBuffers = takeCount(commandBuffersBegun);
    const auto copies = takeCount(cpuBufferCopies);
    const auto copiedMiB = takeCount(cpuBufferCopyBytes) >> 20u;
    static BindlessHeap::Counts lastTotals;
    BindlessHeap::Counts totals;
    {
        std::lock_guard heapsLock(heapsMutex());
        for (const auto& entry : heaps()) {
            const auto counts = entry.second->Statistics();
            totals.spansWritten += counts.spansWritten;
            totals.spansKept += counts.spansKept;
            totals.spansExpired += counts.spansExpired;
            totals.encodes += counts.encodes;
            totals.encodedCopies += counts.encodedCopies;
            totals.slotsEvicted += counts.slotsEvicted;
            totals.evictionWaits += counts.evictionWaits;
            totals.imageSlotsHeld += counts.imageSlotsHeld;
            totals.spanPeakBytes = std::max(totals.spanPeakBytes, counts.spanPeakBytes);
            totals.poolBytes += counts.poolBytes;
            totals.slotPressureSubmits += counts.slotPressureSubmits;
            totals.samplersEvicted += counts.samplersEvicted;
            totals.poolBlocksMade += counts.poolBlocksMade;
            totals.poolBlocksRecycled += counts.poolBlocksRecycled;
            totals.poolBlocksFreed += counts.poolBlocksFreed;
            totals.poolBlocksHeld += counts.poolBlocksHeld;
            totals.poolBlockBytes += counts.poolBlockBytes;
            totals.poolBatchPeakBytes = std::max(totals.poolBatchPeakBytes, counts.poolBatchPeakBytes);
        }
    }
    const auto windowDelta = [](std::uint64_t now, std::uint64_t before) { return now >= before ? now - before : now; };
    const auto spansWritten = windowDelta(totals.spansWritten, lastTotals.spansWritten);
    const auto spansKept = windowDelta(totals.spansKept, lastTotals.spansKept);
    const auto spansExpired = windowDelta(totals.spansExpired, lastTotals.spansExpired);
    const auto imageEncodes = windowDelta(totals.encodes, lastTotals.encodes);
    const auto imageCopies = windowDelta(totals.encodedCopies, lastTotals.encodedCopies);
    const auto slotsEvicted = windowDelta(totals.slotsEvicted, lastTotals.slotsEvicted);
    const auto evictionWaits = windowDelta(totals.evictionWaits, lastTotals.evictionWaits);
    const auto poolMiB = windowDelta(totals.poolBytes, lastTotals.poolBytes) >> 20u;
    const auto slotPressureSubmits = windowDelta(totals.slotPressureSubmits, lastTotals.slotPressureSubmits);
    const auto samplersEvicted = windowDelta(totals.samplersEvicted, lastTotals.samplersEvicted);
    const auto poolMade = windowDelta(totals.poolBlocksMade, lastTotals.poolBlocksMade);
    const auto poolRecycled = windowDelta(totals.poolBlocksRecycled, lastTotals.poolBlocksRecycled);
    const auto poolFreed = windowDelta(totals.poolBlocksFreed, lastTotals.poolBlocksFreed);
    lastTotals = totals;
    static std::uint64_t lastPushedKeeps = 0;
    const auto pushedKeepsTotal = PushedUserDataKeeps();
    const auto pushedKeeps = windowDelta(pushedKeepsTotal, lastPushedKeeps);
    lastPushedKeeps = pushedKeepsTotal;
    static std::array<std::uint64_t, 3> lastUserRewrites{};
    const std::array<std::uint64_t, 3> userRewriteTotals{userWordsRewrites.load(std::memory_order_relaxed), userWordsRewriteBlockWords.load(std::memory_order_relaxed), userWordsRewriteUserWords.load(std::memory_order_relaxed)};
    std::array<std::uint64_t, 3> userRewrites{};
    for (std::size_t i = 0; i < userRewrites.size(); ++i) userRewrites[i] = windowDelta(userRewriteTotals[i], lastUserRewrites[i]);
    lastUserRewrites = userRewriteTotals;
    const auto templates = ShaderRecompiler::HeapTemplateCounts();
    std::fprintf(stderr,
                 "[gpu-heap] (10 s): draws %llu heap / %llu plain, dispatches %llu heap / %llu plain; "
                 "heap binds %llu over %llu command buffers; set binds:%s; excluded:%s; draw stages:%s; "
                 "CPU buffer copies %llu (%llu MiB); spans %llu written / %llu kept (%llu for pushed user data only) / %llu expired, %llu written for user words only (%llu block words, %llu user words); data pool %llu MiB written, %llu blocks (%llu MiB), %llu made / %llu recycled / %llu freed, since start at most %llu KiB per batch and %llu KiB per span; image descriptors %llu encoded / %llu copied; image slots %llu of %u held, %llu evicted (%llu waits); samplers evicted %llu; slot pressure submits %llu; templates %llu (%llu MiB, %llu evicted)\n",
                 printed(heapDraws), printed(plainDraws), printed(heapDispatches), printed(plainDispatches), printed(heapBindsSeen),
                 printed(commandBuffers), binds.c_str(), excluded.c_str(), stageReasons.c_str(), printed(copies), printed(copiedMiB),
                 printed(spansWritten), printed(spansKept), printed(pushedKeeps), printed(spansExpired), printed(userRewrites[0]), printed(userRewrites[1]), printed(userRewrites[2]), printed(poolMiB), printed(totals.poolBlocksHeld), printed(totals.poolBlockBytes >> 20u), printed(poolMade), printed(poolRecycled), printed(poolFreed), printed(totals.poolBatchPeakBytes >> 10u), printed(totals.spanPeakBytes >> 10u), printed(imageEncodes), printed(imageCopies), printed(totals.imageSlotsHeld), BindlessHeap::ImageSlotCapacity(), printed(slotsEvicted), printed(evictionWaits), printed(samplersEvicted), printed(slotPressureSubmits), printed(templates.entries), printed(templates.bytes >> 20u), printed(templates.evicted));
}

void forgetHeapBinding(VkCommandBuffer commands) {
    if (!DescriptorHeapEnabled()) return;
    std::lock_guard lock(boundHeapsMutex);
    if (boundHeaps.erase(commands) != 0) boundHeapsEpoch.fetch_add(1, std::memory_order_release);
}

}

void NoteHeapRecord(bool compute, bool heap) {
    if (!pipelineStats()) return;
    heapRecords[heapRecordIndex(compute, heap)].fetch_add(1, std::memory_order_relaxed);
    reportHeapStatistics();
}

void NoteHeapStageReason(HeapStageReason reason) {
    heapStageReasons[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

bool HeapCostTimed() {
    static const bool timed = [] {
        const char* stats = std::getenv("APS5_PIPELINE_STATS");
        return stats != nullptr && std::string_view(stats) == "2";
    }();
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

void NoteHeapExcluded(HeapExcluded reason) {
    heapExcludedCounts[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t HeapExcludedCount(HeapExcluded reason) {
    return heapExcludedCounts[static_cast<std::size_t>(reason)].load(std::memory_order_relaxed);
}

std::uint64_t HeapStageReasons(HeapStageReason reason) {
    return heapStageReasons[static_cast<std::size_t>(reason)].load(std::memory_order_relaxed);
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
    if (pipelineStats()) setBinds[static_cast<std::size_t>(site)].fetch_add(1, std::memory_order_relaxed);
    forgetHeapBinding(commands);
}

void CmdPushDescriptorSet(PFN_vkCmdPushDescriptorSetKHR push, VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t set,
                          std::uint32_t writeCount, const VkWriteDescriptorSet* writes, SetBindSite site) {
    push(commands, bindPoint, layout, set, writeCount, writes);
    if (pipelineStats()) setBinds[static_cast<std::size_t>(site)].fetch_add(1, std::memory_order_relaxed);
    forgetHeapBinding(commands);
}

std::uint64_t DescriptorHeapBinds() {
    return heapBinds.load(std::memory_order_relaxed);
}

bool HeapPassesActive(const Context& context) {
    return DescriptorHeapEnabled() && context.descriptorHeap && BindlessHeap::For(context).DescriptorHeapMode();
}

VkDescriptorSetAndBindingMappingEXT HeapPushAddressMapping(std::uint32_t set, std::uint32_t binding, std::uint32_t pushOffset) {
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = set;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    mapping.sourceData.pushAddressOffset = pushOffset;
    return mapping;
}

VkDescriptorSetAndBindingMappingEXT HeapPushIndexMapping(const BindlessHeap& heap, std::uint32_t set, std::uint32_t binding, VkSpirvResourceTypeFlagsEXT resources,
                                                         std::uint32_t pushOffset) {
    VkDescriptorSetAndBindingMappingEXT mapping{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT};
    mapping.descriptorSet = set;
    mapping.firstBinding = binding;
    mapping.bindingCount = 1;
    mapping.resourceMask = resources;
    mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    mapping.sourceData.pushIndex.pushOffset = pushOffset;
    mapping.sourceData.pushIndex.heapIndexStride = static_cast<std::uint32_t>(heap.ImageHeapStride());
    mapping.sourceData.pushIndex.heapArrayStride = static_cast<std::uint32_t>(heap.ImageHeapStride());
    return mapping;
}

VkPipeline CreateHeapComputePipeline(const Context& context, VkComputePipelineCreateInfo info, std::span<const VkDescriptorSetAndBindingMappingEXT> mappings) {
    if (!HeapPassesActive(context)) return VK_NULL_HANDLE;
    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo{VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
    mappingInfo.pNext = info.stage.pNext;
    mappingInfo.mappingCount = static_cast<std::uint32_t>(mappings.size());
    mappingInfo.pMappings = mappings.data();
    info.stage.pNext = &mappingInfo;
    auto heapFlags = DescriptorHeapFlags();
    heapFlags.pNext = info.pNext;
    info.pNext = &heapFlags;
    info.layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    auto pipelineCacheLock = context.LockPipelineCacheForUse();
    if (context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return pipeline;
}

VkPipeline CreateHeapGraphicsPipeline(const Context& context, VkGraphicsPipelineCreateInfo info, std::span<const VkDescriptorSetAndBindingMappingEXT> mappings) {
    if (!HeapPassesActive(context)) return VK_NULL_HANDLE;
    std::vector<VkPipelineShaderStageCreateInfo> stages(info.pStages, info.pStages + info.stageCount);
    std::vector<VkShaderDescriptorSetAndBindingMappingInfoEXT> mappingInfos(stages.size(), {VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT});
    for (std::size_t index = 0; index < stages.size(); ++index) {
        mappingInfos[index].pNext = stages[index].pNext;
        mappingInfos[index].mappingCount = static_cast<std::uint32_t>(mappings.size());
        mappingInfos[index].pMappings = mappings.data();
        stages[index].pNext = &mappingInfos[index];
    }
    info.pStages = stages.data();
    auto heapFlags = DescriptorHeapFlags();
    heapFlags.pNext = info.pNext;
    info.pNext = &heapFlags;
    info.layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    auto pipelineCacheLock = context.LockPipelineCacheForUse();
    if (context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return pipeline;
}

void CmdPushData(const Context& context, VkCommandBuffer commands, std::uint32_t bytes, const void* values) {
    const VkPushDataInfoEXT data{VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT, nullptr, 0, {values, bytes}};
    context.Function<PFN_vkCmdPushDataEXT>("vkCmdPushDataEXT")(commands, &data);
}

void BindlessHeap::BindDescriptorHeaps(VkCommandBuffer commands) const {
    if (resourceHeap == nullptr) return;
    struct LastBound {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        const BindlessHeap* heap = nullptr;
        std::uint64_t epoch = 0;
    };
    thread_local LastBound last;
    const auto epoch = boundHeapsEpoch.load(std::memory_order_acquire);
    if (last.commands == commands && last.heap == this && last.epoch == epoch) return;
    {
        std::lock_guard lock(boundHeapsMutex);
        const bool inserted = boundHeaps.insert(commands).second;
        last = {commands, this, epoch};
        if (!inserted) return;
    }
    heapBinds.fetch_add(1, std::memory_order_relaxed);
    heapBindWindow.fetch_add(1, std::memory_order_relaxed);
    VkBindHeapInfoEXT info{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    info.heapRange = {resourceHeap->DeviceAddress(), resourceHeapUsed + context.descriptorHeapProperties.minResourceHeapReservedRange};
    info.reservedRangeOffset = resourceHeapUsed;
    info.reservedRangeSize = context.descriptorHeapProperties.minResourceHeapReservedRange;
    context.Function<PFN_vkCmdBindResourceHeapEXT>("vkCmdBindResourceHeapEXT")(commands, &info);
    info.heapRange = {samplerHeap->DeviceAddress(),
                      samplerHeapUsed + context.descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded};
    info.reservedRangeOffset = samplerHeapUsed;
    info.reservedRangeSize = context.descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;
    context.Function<PFN_vkCmdBindSamplerHeapEXT>("vkCmdBindSamplerHeapEXT")(commands, &info);
}

void BindlessHeap::Submitted(Recorder& recorder, std::uint64_t serial) {
    if (!recorder.HasTimeline()) throw std::runtime_error("AGC bindless heap: a batch using the heap was submitted by a recorder without a timeline semaphore");
    {
        std::lock_guard lock(mutex);
        submitter = &recorder;
        useEpoch.fetch_add(1, std::memory_order_relaxed);
        if (refused && imagePool.Unused() + imageRetiring() < 2u * static_cast<std::size_t>(imageMargin())) evictLocked(EvictIdleSubmissions);
        if (refused && samplerPool.Unused() + retiringSamplers < 2u * SamplerSlotMargin) evictSamplersLocked(EvictIdleSubmissions);
        for (auto entry = retiring.rbegin(); entry != retiring.rend() && entry->serial == 0u; ++entry) {
            entry->serial = serial;
            if (entry->sampler) ++samplerRetiringAssigned;
            else ++retiringAssigned;
        }
        if (!retiring.empty()) reclaimLocked(false);
        noteReleasedLocked();
    }
    SpanScope scope(*this);
    counts.poolBatchPeakBytes = std::max<std::uint64_t>(counts.poolBatchPeakBytes, poolBatchBytes);
    poolBatchBytes = 0;
    for (std::size_t index = 0; index < poolBlocks.size(); ++index) {
        auto& block = poolBlocks[index];
        if (block.buffer == nullptr || (!block.open && index != poolCurrent)) continue;
        block.recorder = &recorder;
        block.serial = serial;
        block.open = index == poolCurrent;
    }
    for (auto entry = retiredTables.rbegin(); entry != retiredTables.rend() && !entry->submitted; ++entry) {
        entry->recorder = &recorder;
        entry->serial = serial;
        entry->submitted = true;
    }
    reclaimTablesLocked();
    trimPoolLocked();
}

BindlessHeap::Counts BindlessHeap::Statistics() const {
    SpanScope scope(*this);
    std::lock_guard lock(mutex);
    auto snapshot = counts;
    for (const auto& block : poolBlocks) {
        if (block.buffer == nullptr) continue;
        ++snapshot.poolBlocksHeld;
        snapshot.poolBlockBytes += block.bytes;
    }
    snapshot.imageSlotsHeld = imagePool.next - std::min<std::size_t>(imagePool.next, SamplerKind + imagePool.free.size() + imageRetiring());
    return snapshot;
}

}
