#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "RuntimeAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/HeapLookupResolve_spv.h"
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

namespace Abi = ShaderRecompiler::RuntimeAbi;

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("AGC bindless heap: ") + what + " failed: " + std::to_string(static_cast<int>(result)));
}

std::uint32_t fragmentBinding(std::uint32_t kind) {
    return Abi::BindingNumber(Abi::Stage::Fragment, static_cast<Abi::Binding>(kind));
}

}

bool GpuHeapEnabled() {
    static const bool enabled = std::getenv("APS5_GPU_HEAP") != nullptr;
    return enabled;
}

bool BindlessTexturesEnabled() {
    return GpuHeapEnabled();
}

namespace {
std::atomic<std::uint64_t> imageTableBuilds{0};
}

void NoteImageTableBuild() {
    imageTableBuilds.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t ImageTableBuilds() {
    return imageTableBuilds.load(std::memory_order_relaxed);
}

bool GpuConstantsEnabled() {
    return GpuHeapEnabled();
}

BindlessHeap::BindlessHeap(const Context& context) : context(context) {
    const auto createLayout = context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout");
    const auto createPool = context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
    const auto allocateSets = context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
    const auto updateSets = context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets");

    std::vector<VkDescriptorSetLayoutBinding> heapBindings;
    constexpr VkShaderStageFlags heapStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    for (std::uint32_t kind = Abi::FirstImageBinding; kind < Abi::FirstStorageImageBinding; ++kind) heapBindings.push_back({fragmentBinding(kind), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Abi::GpuImageHeapSlots, heapStages, nullptr});
    heapBindings.push_back({fragmentBinding(static_cast<std::uint32_t>(Abi::Binding::Samplers)), VK_DESCRIPTOR_TYPE_SAMPLER, Abi::GpuSamplerHeapSlots, heapStages, nullptr});
    const std::vector<VkDescriptorBindingFlags> heapFlags(heapBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT);
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flagInfo.bindingCount = static_cast<std::uint32_t>(heapFlags.size());
    flagInfo.pBindingFlags = heapFlags.data();
    VkDescriptorSetLayoutCreateInfo heapInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    heapInfo.pNext = &flagInfo;
    heapInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    heapInfo.bindingCount = static_cast<std::uint32_t>(heapBindings.size());
    heapInfo.pBindings = heapBindings.data();
    check(createLayout(context.device, &heapInfo, nullptr, &heapLayout), "vkCreateDescriptorSetLayout (heap)");

    const auto mainBinding = [](Abi::Binding binding) { return Abi::BindingNumber(Abi::Stage::Main, binding); };
    const std::array<VkDescriptorSetLayoutBinding, 4> dataBindings{{
        {fragmentBinding(static_cast<std::uint32_t>(Abi::Binding::ShaderData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {fragmentBinding(static_cast<std::uint32_t>(Abi::Binding::FlattenedSrt)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {mainBinding(Abi::Binding::ShaderData), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {mainBinding(Abi::Binding::FlattenedSrt), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    }};
    VkDescriptorSetLayoutCreateInfo dataInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dataInfo.bindingCount = static_cast<std::uint32_t>(dataBindings.size());
    dataInfo.pBindings = dataBindings.data();
    check(createLayout(context.device, &dataInfo, nullptr, &dataLayout), "vkCreateDescriptorSetLayout (data)");

    std::map<VkDescriptorType, std::uint32_t> heapSizes;
    for (const auto& binding : heapBindings) heapSizes[binding.descriptorType] += binding.descriptorCount;
    std::vector<VkDescriptorPoolSize> sizes;
    for (const auto& [type, count] : heapSizes) sizes.push_back({type, count});
    VkDescriptorPoolCreateInfo heapPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    heapPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    heapPoolInfo.maxSets = 1;
    heapPoolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
    heapPoolInfo.pPoolSizes = sizes.data();
    check(createPool(context.device, &heapPoolInfo, nullptr, &heapPool), "vkCreateDescriptorPool (heap)");
    const std::array<VkDescriptorPoolSize, 1> dataSizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 4}}};
    VkDescriptorPoolCreateInfo dataPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dataPoolInfo.maxSets = 1;
    dataPoolInfo.poolSizeCount = static_cast<std::uint32_t>(dataSizes.size());
    dataPoolInfo.pPoolSizes = dataSizes.data();
    check(createPool(context.device, &dataPoolInfo, nullptr, &dataPool), "vkCreateDescriptorPool (data)");

    VkDescriptorSetAllocateInfo heapAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    heapAllocate.descriptorPool = heapPool;
    heapAllocate.descriptorSetCount = 1;
    heapAllocate.pSetLayouts = &heapLayout;
    check(allocateSets(context.device, &heapAllocate, &heapSet), "vkAllocateDescriptorSets (heap)");
    VkDescriptorSetAllocateInfo dataAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dataAllocate.descriptorPool = dataPool;
    dataAllocate.descriptorSetCount = 1;
    dataAllocate.pSetLayouts = &dataLayout;
    check(allocateSets(context.device, &dataAllocate, &dataSet), "vkAllocateDescriptorSets (data)");

    try {
        ring = std::make_unique<Buffer>(context, RingBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } catch (const std::exception&) {
        ring = std::make_unique<Buffer>(context, RingBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    ringAlignment = static_cast<std::uint32_t>(std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 16));
    const VkDescriptorBufferInfo ringInfo{ring->Handle(), 0, DataBindingRange};
    constexpr VkDeviceSize lookupBytes = (Abi::GpuLookupHeaderDwords + Abi::GpuLookupEntries * Abi::GpuLookupEntryDwords) * sizeof(std::uint32_t);
    try {
        lookup = std::make_unique<Buffer>(context, lookupBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } catch (const std::exception&) {
        lookup = std::make_unique<Buffer>(context, lookupBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    {
        auto* table = reinterpret_cast<std::uint32_t*>(lookup->Bytes().data());
        std::fill_n(table, lookupBytes / sizeof(std::uint32_t), 0u);
        table[0] = Abi::GpuLookupEntries - 1u;
        lookupShadow.assign(table, table + lookupBytes / sizeof(std::uint32_t));
    }
    std::array<VkWriteDescriptorSet, 4> writes{};
    for (std::size_t index = 0; index < writes.size(); ++index) writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dataSet, dataBindings[index].binding, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, nullptr, &ringInfo, nullptr};
    updateSets(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    nextTextureSlot.assign(Abi::FirstStorageImageBinding, 0u);
    ringChunks.resize(RingBytes / RingChunkBytes);
    fixupChunks.resize(FixupRingWords / FixupChunkWords);
    owners.resize(Abi::FirstStorageImageBinding);
    freeSlots.resize(Abi::FirstStorageImageBinding);
    const char* cap = std::getenv("APS5_HEAP_SLOT_CAP");
    slotCap = cap != nullptr ? std::min<std::uint32_t>(static_cast<std::uint32_t>(std::strtoul(cap, nullptr, 10)), Abi::GpuImageHeapSlots) : Abi::GpuImageHeapSlots;
    if (slotCap == 0u) slotCap = Abi::GpuImageHeapSlots;
}

BindlessHeap::~BindlessHeap() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (profile && lookup != nullptr && resolveDispatches != 0u) std::fprintf(stderr, "[gpu-lookup] at teardown: %llu resolve dispatches, %llu slot words; GPU misses %u, disagreements with the committer %u\n", static_cast<unsigned long long>(resolveDispatches), static_cast<unsigned long long>(counts.lookupFixups), LookupMisses(), LookupDisagreements());
    const auto destroyPool = context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
    if (resolveCommands != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, resolveCommands, nullptr);
    if (resolvePool != VK_NULL_HANDLE) destroyPool(context.device, resolvePool, nullptr);
    if (resolvePipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, resolvePipeline, nullptr);
    if (resolveLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, resolveLayout, nullptr);
    if (resolveSetLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, resolveSetLayout, nullptr);
    const auto destroyLayout = context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout");
    if (heapPool != VK_NULL_HANDLE) destroyPool(context.device, heapPool, nullptr);
    if (dataPool != VK_NULL_HANDLE) destroyPool(context.device, dataPool, nullptr);
    if (heapLayout != VK_NULL_HANDLE) destroyLayout(context.device, heapLayout, nullptr);
    if (dataLayout != VK_NULL_HANDLE) destroyLayout(context.device, dataLayout, nullptr);
}

namespace {

std::mutex& heapsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<VkDevice, std::unique_ptr<BindlessHeap>>& heaps() {
    static auto& map = *new std::unordered_map<VkDevice, std::unique_ptr<BindlessHeap>>();
    return map;
}

}

namespace {
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

std::optional<std::uint32_t> BindlessHeap::TextureSlot(std::uint32_t kind, const std::shared_ptr<Texture>& texture, VkImageView view, VkImageLayout layout) {
    if (kind < Abi::FirstImageBinding || kind >= Abi::FirstStorageImageBinding || texture == nullptr || view == VK_NULL_HANDLE) throw std::runtime_error("AGC bindless heap: invalid texture slot request");
    std::vector<std::shared_ptr<Texture>> dropped;
    std::lock_guard lock(mutex);
    const TextureKey key{kind, texture.get(), view, layout};
    if (const auto found = textureSlots.find(key); found != textureSlots.end()) return found->second;
    if (++slotRequests % 1024u == 0u) NoteSweepDue();
    auto slot = takeSlotLocked(kind);
    if (!slot) {
        sweepLocked(dropped);
        slot = takeSlotLocked(kind);
    }
    if (!slot) {
        ++counts.full;
        return std::nullopt;
    }
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, view, layout};
    const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, heapSet, fragmentBinding(kind), *slot, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &image, nullptr, nullptr};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
    textureSlots.emplace(key, *slot);
    auto& kindOwners = owners[kind];
    if (kindOwners.size() <= *slot) kindOwners.resize(*slot + 1u);
    kindOwners[*slot].texture = texture;
    kindOwners[*slot].key = key;
    texture->AddHeapHold();
    ++counts.textures;
    noteTakenLocked(kind, dropped);
    return slot;
}

std::optional<std::uint32_t> BindlessHeap::takeSlotLocked(std::uint32_t kind) {
    auto& free = freeSlots[kind];
    if (!free.empty()) {
        const auto slot = free.back();
        free.pop_back();
        ++counts.slotsReused;
        return slot;
    }
    auto& next = nextTextureSlot[kind];
    if (next >= slotCap) return std::nullopt;
    return next++;
}

void BindlessHeap::sweepLocked(std::vector<std::shared_ptr<Texture>>& dropped) {
    ++counts.sweeps;
    const auto firstDropped = dropped.size();
    auto* table = reinterpret_cast<std::uint32_t*>(lookup->Bytes().data());
    for (std::uint32_t kind = 0; kind < owners.size(); ++kind) {
        auto& kindOwners = owners[kind];
        for (std::uint32_t slot = 0; slot < kindOwners.size(); ++slot) {
            auto& owner = kindOwners[slot];
            if (owner.texture == nullptr || owner.texture.use_count() > static_cast<long>(owner.texture->HeapHolds())) continue;
            textureSlots.erase(owner.key);
            if (const auto entries = lookupBySlot.find(kind << 16u | slot); entries != lookupBySlot.end()) {
                for (const auto at : entries->second) {
                    lookupShadow[at + Abi::GpuLookupSlotDword] = Abi::GpuLookupSentinel;
                    table[at + Abi::GpuLookupSlotDword] = Abi::GpuLookupSentinel;
                }
                lookupBySlot.erase(entries);
            }
            dropped.push_back(std::move(owner.texture));
            owner.key = {};
            ++owner.generation;
            slotSerial.fetch_add(1, std::memory_order_release);
            freeSlots[kind].push_back(slot);
            ++counts.slotsFreed;
        }
    }
    for (auto index = firstDropped; index < dropped.size(); ++index) dropped[index]->DropHeapHold();
}

void BindlessHeap::sweepSamplersLocked(std::vector<std::shared_ptr<Sampler>>& dropped) {
    auto* table = reinterpret_cast<std::uint32_t*>(lookup->Bytes().data());
    for (std::uint32_t slot = 0; slot < heldSamplers.size(); ++slot) {
        auto& held = heldSamplers[slot];
        if (held == nullptr || held.use_count() != 1) continue;
        samplerSlots.erase(held.get());
        if (const auto entries = lookupBySlot.find(SamplerKind << 16u | slot); entries != lookupBySlot.end()) {
            for (const auto at : entries->second) {
                lookupShadow[at + Abi::GpuLookupSlotDword] = Abi::GpuLookupSentinel;
                table[at + Abi::GpuLookupSlotDword] = Abi::GpuLookupSentinel;
            }
            lookupBySlot.erase(entries);
        }
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
    if (!nearlyFull.exchange(true, std::memory_order_relaxed)) std::fprintf(stderr, "[gpu-heap] fewer than %u %s slots are free; draws take the per-draw path from now on\n", margin, what);
}

}

bool BindlessHeap::NearlyFull() {
    return nearlyFull.load(std::memory_order_relaxed);
}

namespace {

std::atomic<bool> sweepDue{false};

}

void BindlessHeap::NoteSweepDue() {
    sweepDue.store(true, std::memory_order_relaxed);
}

void BindlessHeap::SweepIfDue() {
    if (!sweepDue.load(std::memory_order_relaxed) || !sweepDue.exchange(false, std::memory_order_relaxed)) return;
    std::vector<std::shared_ptr<Texture>> dropped;
    std::vector<std::shared_ptr<Sampler>> droppedSamplers;
    std::lock_guard heapsLock(heapsMutex());
    bool room = true;
    for (auto& [device, heap] : heaps()) {
        if (heap == nullptr) continue;
        std::lock_guard lock(heap->mutex);
        heap->sweepLocked(dropped);
        heap->sweepSamplersLocked(droppedSamplers);
        room = room && heap->roomLocked();
    }
    if (room && nearlyFull.exchange(false, std::memory_order_relaxed)) std::fprintf(stderr, "[gpu-heap] a sweep left twice the margin of slots free; draws take the heap again\n");
}

std::uint32_t BindlessHeap::imageMargin() const {
    return std::max(1u, std::min(ImageSlotMargin, slotCap / 4u));
}

std::size_t BindlessHeap::unusedLocked(std::uint32_t kind) const {
    const auto next = nextTextureSlot[kind];
    return (next < slotCap ? slotCap - next : 0u) + freeSlots[kind].size();
}

bool BindlessHeap::roomLocked() const {
    for (std::uint32_t kind = Abi::FirstImageBinding; kind < Abi::FirstStorageImageBinding; ++kind) {
        if (unusedLocked(kind) < 2u * static_cast<std::size_t>(imageMargin())) return false;
    }
    return Abi::GpuSamplerHeapSlots - heldSamplers.size() + freeSamplerSlots.size() >= 2u * SamplerSlotMargin;
}

void BindlessHeap::noteTakenLocked(std::uint32_t kind, std::vector<std::shared_ptr<Texture>>& dropped) {
    if (nearlyFull.load(std::memory_order_relaxed)) return;
    const auto margin = imageMargin();
    if (unusedLocked(kind) >= margin) return;
    sweepLocked(dropped);
    if (unusedLocked(kind) < margin) noteNearlyFull("image", margin);
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

bool BindlessHeap::GenerationsCurrent(std::span<const std::vector<std::uint32_t>> slots, std::span<const std::uint32_t> slotGenerations, std::span<const std::uint32_t> samplerSlots, std::span<const std::uint32_t> samplerGenerations) const {
    std::lock_guard lock(mutex);
    std::size_t generation = 0;
    for (std::uint32_t kind = 0; kind < slots.size(); ++kind) {
        for (const auto slot : slots[kind]) {
            const auto current = kind < owners.size() && slot < owners[kind].size() ? owners[kind][slot].generation : 0u;
            if (generation >= slotGenerations.size() || current != slotGenerations[generation++]) return false;
        }
    }
    for (std::size_t i = 0; i < samplerSlots.size(); ++i) {
        if (i >= samplerGenerations.size() || (samplerSlots[i] < this->samplerGenerations.size() ? this->samplerGenerations[samplerSlots[i]] : 0u) != samplerGenerations[i]) return false;
    }
    return true;
}

std::uint32_t BindlessHeap::SlotGeneration(std::uint32_t kind, std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    return kind < owners.size() && slot < owners[kind].size() ? owners[kind][slot].generation : 0u;
}

const Texture* BindlessHeap::SlotTexture(std::uint32_t kind, std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    return kind < owners.size() && slot < owners[kind].size() ? owners[kind][slot].texture.get() : nullptr;
}

std::uint32_t BindlessHeap::AbsentSlot(std::uint32_t kind, VkImageView view) {
    if (kind < Abi::FirstImageBinding || kind >= Abi::FirstStorageImageBinding) throw std::runtime_error("AGC bindless heap: invalid absent slot request");
    std::lock_guard lock(mutex);
    if (const auto found = absentSlots.find(kind); found != absentSlots.end()) return found->second;
    const auto taken = takeSlotLocked(kind);
    if (!taken) throw std::runtime_error("AGC bindless heap: the image heap is full");
    const auto slot = *taken;
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
    const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, heapSet, fragmentBinding(kind), slot, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &image, nullptr, nullptr};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
    absentSlots.emplace(kind, slot);
    return slot;
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
    const VkDescriptorImageInfo info{sampler->Handle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, heapSet, fragmentBinding(static_cast<std::uint32_t>(Abi::Binding::Samplers)), slot, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &info, nullptr, nullptr};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
    samplerSlots.emplace(sampler.get(), slot);
    heldSamplers[slot] = sampler;
    ++counts.samplers;
    noteSamplerTakenLocked(dropped);
    return slot;
}

BindlessHeap::DataSpan BindlessHeap::Allocate(std::size_t words) {
    const auto bytes = words * sizeof(std::uint32_t);
    if (bytes > DataBindingRange) throw std::runtime_error("AGC bindless heap: per-draw data exceeds the data binding range");
    std::lock_guard lock(mutex);
    auto offset = (ringCursor + ringAlignment - 1) / ringAlignment * ringAlignment;
    if (offset + DataBindingRange > RingBytes) {
        offset = 0;
        ++counts.ringWraps;
    }
    ringCursor = offset + bytes;
    for (auto chunk = offset / RingChunkBytes; chunk <= (offset + std::max<std::size_t>(bytes, 1u) - 1u) / RingChunkBytes; ++chunk) enterChunk(ringChunks, chunk, ringChunk, "shader data");
    auto* base = reinterpret_cast<std::uint32_t*>(ring->Bytes().data() + offset);
    return {static_cast<std::uint32_t>(offset), std::span<std::uint32_t>(base, words)};
}

void BindlessHeap::reportLookup() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile || resolveDispatches % 8192u != 0u) return;
    std::fprintf(stderr, "[gpu-lookup] %llu slot words resolved on the GPU in %llu resolve dispatches (%llu table decisions to look up), %llu named by the draw (key taken by another slot), %llu table full; %llu entries, %llu revived; GPU misses %u, disagreements with the committer %u; slots: %llu textures placed, %llu freed, %llu reused, %llu sweeps\n", static_cast<unsigned long long>(counts.lookupFixups), static_cast<unsigned long long>(resolveDispatches), static_cast<unsigned long long>(counts.lookupSentinels), static_cast<unsigned long long>(counts.lookupExplicit), static_cast<unsigned long long>(counts.lookupFull), static_cast<unsigned long long>(counts.lookupInserts), static_cast<unsigned long long>(counts.lookupRevived), LookupMisses(), LookupDisagreements(), static_cast<unsigned long long>(counts.textures), static_cast<unsigned long long>(counts.slotsFreed), static_cast<unsigned long long>(counts.slotsReused), static_cast<unsigned long long>(counts.sweeps));
}

std::uint32_t BindlessHeap::LookupDisagreements() const {
    return reinterpret_cast<const volatile std::uint32_t*>(lookup->Bytes().data())[2];
}

void BindlessHeap::AddLookupFixup(std::uint32_t slotDword, std::uint32_t slot, const std::uint32_t* key) {
    std::lock_guard lock(mutex);
    lookupFixups.insert(lookupFixups.end(), {slotDword, slot});
    lookupFixups.insert(lookupFixups.end(), key, key + Abi::GpuLookupKeyDwords);
}

void BindlessHeap::createLookupResolve() {
    const std::array<VkDescriptorSetLayoutBinding, 3> bindings{{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    }};
    VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setInfo.pBindings = bindings.data();
    check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &resolveSetLayout), "vkCreateDescriptorSetLayout (lookup resolve)");
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 8};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &resolveSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &resolveLayout), "vkCreatePipelineLayout (lookup resolve)");
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = sizeof(HEAP_LOOKUP_RESOLVE_SPV);
    moduleInfo.pCode = HEAP_LOOKUP_RESOLVE_SPV;
    VkShaderModule module = VK_NULL_HANDLE;
    check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule (lookup resolve)");
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
    pipelineInfo.layout = resolveLayout;
    check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resolvePipeline), "vkCreateComputePipelines (lookup resolve)");
    context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &size;
    check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &resolvePool), "vkCreateDescriptorPool (lookup resolve)");
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = resolvePool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &resolveSetLayout;
    check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocate, &resolveSet), "vkAllocateDescriptorSets (lookup resolve)");
    fixupRing = std::make_unique<Buffer>(context, FixupRingWords * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    const std::array<VkDescriptorBufferInfo, 3> infos{{{ring->Handle(), 0, VK_WHOLE_SIZE}, {lookup->Handle(), 0, VK_WHOLE_SIZE}, {fixupRing->Handle(), 0, VK_WHOLE_SIZE}}};
    std::array<VkWriteDescriptorSet, 3> writes{};
    for (std::uint32_t index = 0; index < writes.size(); ++index) writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, resolveSet, index, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &infos[index], nullptr};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    VkCommandPoolCreateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandInfo.queueFamilyIndex = context.queueFamily;
    check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &commandInfo, nullptr, &resolveCommands), "vkCreateCommandPool (lookup resolve)");
}

void BindlessHeap::enterChunk(std::vector<Chunk>& chunks, std::size_t index, std::size_t& current, const char* what) {
    if (index == current) return;
    current = index;
    auto& chunk = chunks[index];
    if (chunk.open) throw std::runtime_error(std::string("AGC bindless heap: one batch's ") + what + " exceeds its ring");
    if (chunk.serial != 0u) {
        ++counts.chunkWaits;
        if (!chunk.recorder->HasTimeline()) throw std::runtime_error(std::string("AGC bindless heap: the ") + what + " ring needs the recorder timeline to wait for a batch");
        chunk.recorder->WaitSerial(chunk.serial);
        chunk.serial = 0u;
    }
    chunk.open = true;
}

BindlessHeap::LookupResolve BindlessHeap::Submitted(Recorder& recorder, std::uint64_t serial) {
    std::lock_guard lock(mutex);
    const auto tag = [&](std::vector<Chunk>& chunks, std::size_t current) {
        for (std::size_t index = 0; index < chunks.size(); ++index) {
            if (chunks[index].open || index == current) chunks[index] = {&recorder, serial, index == current};
        }
    };
    tag(ringChunks, ringChunk);
    auto resolve = takeLookupResolveLocked(&recorder, serial);
    tag(fixupChunks, fixupChunk);
    return resolve;
}

BindlessHeap::LookupResolve BindlessHeap::TakeLookupResolve() {
    std::lock_guard lock(mutex);
    return takeLookupResolveLocked(nullptr, 0u);
}

BindlessHeap::LookupResolve BindlessHeap::takeLookupResolveLocked(Recorder* recorder, std::uint64_t serial) {
    if (lookupFixups.empty()) return {};
    if (resolvePipeline == VK_NULL_HANDLE) createLookupResolve();
    const auto words = lookupFixups.size();
    if (words > FixupRingWords) throw std::runtime_error("AGC bindless heap: a submission's lookup fixups exceed the fixup ring");
    if (fixupCursor + words > FixupRingWords) fixupCursor = 0;
    fixupCursor = (fixupCursor + FixupWords - 1u) / FixupWords * FixupWords;
    if (fixupCursor + words > FixupRingWords) fixupCursor = 0;
    for (auto chunk = fixupCursor / FixupChunkWords; chunk <= (fixupCursor + words - 1u) / FixupChunkWords; ++chunk) {
        enterChunk(fixupChunks, chunk, fixupChunk, "lookup fixups");
        if (recorder != nullptr) fixupChunks[chunk] = {recorder, serial, false};
    }
    std::memcpy(fixupRing->Bytes().data() + fixupCursor * sizeof(std::uint32_t), lookupFixups.data(), words * sizeof(std::uint32_t));
    const std::array<std::uint32_t, 2> push{static_cast<std::uint32_t>(fixupCursor / FixupWords), static_cast<std::uint32_t>(words / FixupWords)};
    fixupCursor += words;
    lookupFixups.clear();
    VkCommandBuffer commands = VK_NULL_HANDLE;
    {
        std::lock_guard freeLock(*freeResolveMutex);
        if (!freeResolve->empty()) {
            commands = freeResolve->back();
            freeResolve->pop_back();
        }
    }
    if (commands == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = resolveCommands;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &commands), "vkAllocateCommandBuffers (lookup resolve)");
    }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer (lookup resolve)");
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, resolveLayout, 0, 1, &resolveSet, 0, nullptr);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, resolveLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, (push[1] + 63u) / 64u, 1, 1);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer (lookup resolve)");
    ++resolveDispatches;
    counts.lookupFixups += push[1];
    reportLookup();
    auto release = std::shared_ptr<void>(new char, [commands, freeMutex = freeResolveMutex, free = freeResolve](char* marker) {
        delete marker;
        std::lock_guard freeLock(*freeMutex);
        free->push_back(commands);
    });
    return {commands, std::move(release)};
}

std::uint32_t BindlessHeap::LookupWord(std::uint32_t kind, const std::uint32_t* key, std::uint32_t slot) {
    std::lock_guard lock(mutex);
    auto* table = reinterpret_cast<std::uint32_t*>(lookup->Bytes().data());
    auto hash = Abi::GpuLookupHash(key);
    hash ^= hash >> 16u;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13u;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16u;
    for (std::uint32_t probe = 0; probe < Abi::GpuLookupProbes; ++probe) {
        const auto at = Abi::GpuLookupHeaderDwords + ((hash + probe) & (Abi::GpuLookupEntries - 1u)) * Abi::GpuLookupEntryDwords;
        auto* entry = lookupShadow.data() + at;
        if (entry[0] == 0u) {
            std::copy_n(key, Abi::GpuLookupKeyDwords, entry + 1);
            entry[Abi::GpuLookupSlotDword] = slot;
            entry[0] = 1u;
            auto* device = table + at;
            std::copy_n(entry + 1, Abi::GpuLookupSlotDword, device + 1);
            std::atomic_thread_fence(std::memory_order_release);
            device[0] = 1u;
            lookupBySlot[kind << 16u | slot].push_back(at);
            ++counts.lookupInserts;
            ++counts.lookupSentinels;
            return Abi::GpuLookupSentinel;
        }
        if (!std::equal(key, key + Abi::GpuLookupKeyDwords, entry + 1)) continue;
        if (entry[Abi::GpuLookupSlotDword] == Abi::GpuLookupSentinel) {
            entry[Abi::GpuLookupSlotDword] = slot;
            table[at + Abi::GpuLookupSlotDword] = slot;
            lookupBySlot[kind << 16u | slot].push_back(at);
            ++counts.lookupRevived;
        }
        if (entry[Abi::GpuLookupSlotDword] == slot) {
            ++counts.lookupSentinels;
            return Abi::GpuLookupSentinel;
        }
        ++counts.lookupExplicit;
        return slot;
    }
    ++counts.lookupFull;
    return slot;
}

std::uint32_t BindlessHeap::LookupMisses() const {
    return reinterpret_cast<const volatile std::uint32_t*>(lookup->Bytes().data())[Abi::GpuLookupMissDword];
}

BindlessHeap::Counts BindlessHeap::Statistics() const {
    std::lock_guard lock(mutex);
    return counts;
}

}
