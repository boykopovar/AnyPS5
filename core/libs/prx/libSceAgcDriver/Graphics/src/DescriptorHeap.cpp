#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeap.hpp"
#include "BindlessImageAbi.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

namespace Abi = ShaderRecompiler::BindlessImageAbi;

constexpr std::array<const char*, 3> ArrayNames{"sampler", "sampled image", "storage image"};
constexpr std::uint32_t ReservedStageResources = 1u << 12u;

static_assert(static_cast<std::uint32_t>(DescriptorHeap::Array::Samplers) == Abi::SamplerBinding);
static_assert(static_cast<std::uint32_t>(DescriptorHeap::Array::SampledImages) == Abi::SampledImageBinding);
static_assert(static_cast<std::uint32_t>(DescriptorHeap::Array::StorageImages) == Abi::StorageImageBinding);

}

DescriptorHeap::Capacities DescriptorHeap::DeviceCapacities(const Context& context) {
    const auto& limits = context.descriptorIndexingLimits;
    const auto samplers = std::min({limits.maxDescriptorSetUpdateAfterBindSamplers, limits.maxPerStageDescriptorUpdateAfterBindSamplers, context.limits.maxSamplerAllocationCount});
    const auto storage = std::min({limits.maxDescriptorSetUpdateAfterBindStorageImages, limits.maxPerStageDescriptorUpdateAfterBindStorageImages, Abi::StorageImageSlots});
    const auto resources = limits.maxPerStageUpdateAfterBindResources > storage + ReservedStageResources ? limits.maxPerStageUpdateAfterBindResources - storage - ReservedStageResources : 0u;
    const auto sampled = std::min({limits.maxDescriptorSetUpdateAfterBindSampledImages, limits.maxPerStageDescriptorUpdateAfterBindSampledImages, Abi::SampledImageSlots, resources});
    return {samplers, sampled, storage};
}

DescriptorHeap::DescriptorHeap(const Context& context, Capacities capacities) : context(context) {
    Require(capacities.samplers > 1u && capacities.sampledImages > 1u && capacities.storageImages > 1u, "the descriptor heap needs room for its null descriptors and one guest descriptor of each type");
    Require(capacities.sampledImages <= Abi::SlotMask + 1u && capacities.storageImages <= Abi::SlotMask + 1u && capacities.samplers <= Abi::SlotMask + 1u, "the descriptor heap is larger than a packed slot can name");
    Require(context.nullDescriptors, "the descriptor heap's null image descriptors need VK_EXT_robustness2 nullDescriptor");
    const std::array<std::pair<std::uint32_t, VkDescriptorType>, static_cast<std::size_t>(Array::Count)> types{{{capacities.samplers, VK_DESCRIPTOR_TYPE_SAMPLER}, {capacities.sampledImages, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE}, {capacities.storageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}};
    for (std::size_t index = 0; index < arrays.size(); ++index) {
        arrays[index].capacity = types[index].first;
        arrays[index].type = types[index].second;
    }
    try {
        std::array<VkDescriptorPoolSize, static_cast<std::size_t>(Array::Count) + 1u> sizes{};
        for (std::size_t index = 0; index < arrays.size(); ++index) sizes[index] = {arrays[index].type, arrays[index].capacity * MaxViews};
        sizes.back() = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MaxViews};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = MaxViews;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool descriptor heap");
        VkSamplerCreateInfo nullInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        nullInfo.magFilter = VK_FILTER_NEAREST;
        nullInfo.minFilter = VK_FILTER_NEAREST;
        nullInfo.addressModeU = nullInfo.addressModeV = nullInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nullInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &nullInfo, nullptr, &nullSampler), "vkCreateSampler descriptor heap null sampler");
        const std::array<VkDescriptorImageInfo, static_cast<std::size_t>(Array::Count)> nulls{{{nullSampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL}}};
        for (std::size_t index = 0; index < arrays.size(); ++index) {
            arrays[index].entries.push_back(nulls[index]);
            arrays[index].generations.push_back(0u);
            arrays[index].live.push_back(false);
        }
    } catch (...) {
        release();
        throw;
    }
}

DescriptorHeap::~DescriptorHeap() {
    release();
}

void DescriptorHeap::release() noexcept {
    if (pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    for (const auto& item : views) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, item.layout, nullptr);
    if (emptyLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, emptyLayout, nullptr);
    emptyLayout = VK_NULL_HANDLE;
    if (nullSampler != VK_NULL_HANDLE) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, nullSampler, nullptr);
    pool = VK_NULL_HANDLE;
    views.clear();
    nullSampler = VK_NULL_HANDLE;
}

void DescriptorHeap::write(const View& target, Array array, std::uint32_t first, std::uint32_t count) {
    const auto& slots = arrays[static_cast<std::size_t>(array)];
    VkWriteDescriptorSet update{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    update.dstSet = target.set;
    update.dstBinding = static_cast<std::uint32_t>(array);
    update.dstArrayElement = first;
    update.descriptorCount = count;
    update.descriptorType = slots.type;
    update.pImageInfo = slots.entries.data() + first;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &update, 0, nullptr);
}

void DescriptorHeap::writeHeader(const View& target) {
    if (header.buffer == VK_NULL_HANDLE) return;
    VkWriteDescriptorSet update{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    update.dstSet = target.set;
    update.dstBinding = Abi::HeaderBinding;
    update.descriptorCount = 1;
    update.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    update.pBufferInfo = &header;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &update, 0, nullptr);
}

const DescriptorHeap::View& DescriptorHeap::view(VkShaderStageFlags stages) {
    for (const auto& item : views) {
        if (item.stages == stages) return item;
    }
    Require(views.size() < MaxViews, "the descriptor heap has a view for every one of " + std::to_string(MaxViews) + " stage combinations");
    View created{stages, VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<VkDescriptorSetLayoutBinding, static_cast<std::size_t>(Array::Count) + 1u> bindings{};
    std::array<VkDescriptorBindingFlags, static_cast<std::size_t>(Array::Count) + 1u> flags{};
    for (std::size_t index = 0; index < arrays.size(); ++index) {
        bindings[index] = {static_cast<std::uint32_t>(index), arrays[index].type, arrays[index].capacity, stages, nullptr};
        flags[index] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    }
    bindings.back() = {Abi::HeaderBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stages, nullptr};
    flags.back() = 0;
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bindingFlags.bindingCount = static_cast<std::uint32_t>(flags.size());
    bindingFlags.pBindingFlags = flags.data();
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &bindingFlags};
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &created.layout), "vkCreateDescriptorSetLayout descriptor heap");
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = pool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &created.layout;
    if (const auto result = context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &created.set); result != VK_SUCCESS) {
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, created.layout, nullptr);
        Check(result, "vkAllocateDescriptorSets descriptor heap");
    }
    const auto& added = views.emplace_back(created);
    for (std::size_t index = 0; index < arrays.size(); ++index) {
        const auto& slots = arrays[index];
        std::vector<VkDescriptorImageInfo> current(slots.entries.size());
        for (std::size_t slot = 0; slot < current.size(); ++slot) current[slot] = slot == Abi::NullSlot || slots.live[slot] ? slots.entries[slot] : slots.entries[Abi::NullSlot];
        VkWriteDescriptorSet update{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        update.dstSet = added.set;
        update.dstBinding = static_cast<std::uint32_t>(index);
        update.descriptorCount = static_cast<std::uint32_t>(current.size());
        update.descriptorType = slots.type;
        update.pImageInfo = current.data();
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &update, 0, nullptr);
    }
    writeHeader(added);
    return added;
}

VkDescriptorSetLayout DescriptorHeap::EmptyLayout() {
    std::lock_guard lock(mutex);
    if (emptyLayout != VK_NULL_HANDLE) return emptyLayout;
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &emptyLayout), "vkCreateDescriptorSetLayout descriptor heap empty set");
    return emptyLayout;
}

void DescriptorHeap::SetHeader(VkBuffer buffer, VkDeviceSize bytes) {
    std::lock_guard lock(mutex);
    Require(buffer != VK_NULL_HANDLE && bytes != 0u, "the descriptor heap header needs a buffer");
    header = {buffer, 0, bytes};
    for (const auto& item : views) writeHeader(item);
}

VkDescriptorSetLayout DescriptorHeap::Layout(VkShaderStageFlags stages) {
    std::lock_guard lock(mutex);
    return view(stages).layout;
}

VkDescriptorSet DescriptorHeap::Set(VkShaderStageFlags stages) {
    std::lock_guard lock(mutex);
    return view(stages).set;
}

std::uint32_t DescriptorHeap::allocate(Array array, const VkDescriptorImageInfo& entry) {
    std::lock_guard lock(mutex);
    auto& slots = arrays[static_cast<std::size_t>(array)];
    auto slot = static_cast<std::uint32_t>(slots.entries.size());
    if (!slots.free.empty()) {
        slot = slots.free.back();
        slots.free.pop_back();
        slots.entries[slot] = entry;
    } else {
        Require(slot < slots.capacity, std::string("the descriptor heap has no free ") + ArrayNames[static_cast<std::size_t>(array)] + " slot: " + std::to_string(slots.capacity) + " are live or retiring");
        slots.entries.push_back(entry);
        slots.generations.push_back(1u);
        slots.live.push_back(false);
    }
    slots.live[slot] = true;
    for (const auto& item : views) write(item, array, slot, 1);
    return Abi::PackSlot(slot, slots.generations[slot]);
}

std::uint32_t DescriptorHeap::AllocateSampler(VkSampler sampler) {
    Require(sampler != VK_NULL_HANDLE, "a sampler slot needs a sampler");
    return allocate(Array::Samplers, {sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
}

std::uint32_t DescriptorHeap::AllocateImage(Array array, VkImageView view, VkImageLayout layout) {
    Require(array != Array::Samplers && view != VK_NULL_HANDLE, "an image slot needs an image view");
    return allocate(array, {VK_NULL_HANDLE, view, layout});
}

void DescriptorHeap::Release(Array array, std::uint32_t slot, std::uint64_t retireSerial) {
    std::lock_guard lock(mutex);
    auto& slots = arrays[static_cast<std::size_t>(array)];
    const auto index = Abi::SlotIndex(slot);
    Require(index != Abi::NullSlot && index < slots.entries.size() && slots.live[index] && slots.generations[index] == Abi::SlotGeneration(slot), std::string("released a ") + ArrayNames[static_cast<std::size_t>(array)] + " slot of the descriptor heap that is not live in that generation");
    Require(slots.retired.empty() || slots.retired.back().serial <= retireSerial, "descriptor heap slots retire in submission order");
    slots.live[index] = false;
    slots.retired.push_back({index, retireSerial});
}

std::uint32_t DescriptorHeap::Collect(std::uint64_t completedSerial) {
    std::lock_guard lock(mutex);
    std::uint32_t freed = 0;
    for (std::size_t index = 0; index < arrays.size(); ++index) {
        auto& slots = arrays[index];
        while (!slots.retired.empty() && slots.retired.front().serial <= completedSerial) {
            const auto slot = slots.retired.front().slot;
            slots.retired.pop_front();
            slots.entries[slot] = slots.entries[Abi::NullSlot];
            for (const auto& item : views) write(item, static_cast<Array>(index), slot, 1);
            slots.generations[slot] = (slots.generations[slot] + 1u) & Abi::GenerationMask;
            if (slots.generations[slot] == 0u) slots.generations[slot] = 1u;
            slots.free.push_back(slot);
            ++freed;
        }
    }
    return freed;
}

bool DescriptorHeap::Live(Array array, std::uint32_t slot) const {
    std::lock_guard lock(mutex);
    const auto& slots = arrays[static_cast<std::size_t>(array)];
    const auto index = Abi::SlotIndex(slot);
    return index != Abi::NullSlot && index < slots.entries.size() && slots.live[index] && slots.generations[index] == Abi::SlotGeneration(slot);
}

std::uint32_t DescriptorHeap::LiveSlots(Array array) const {
    std::lock_guard lock(mutex);
    const auto& slots = arrays[static_cast<std::size_t>(array)];
    return static_cast<std::uint32_t>(std::count(slots.live.begin(), slots.live.end(), true));
}

std::uint32_t DescriptorHeap::PendingReleases() const {
    std::lock_guard lock(mutex);
    std::size_t pending = 0;
    for (const auto& slots : arrays) pending += slots.retired.size();
    return static_cast<std::uint32_t>(pending);
}

void DescriptorHeap::Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, VkShaderStageFlags stages) {
    const auto set = Set(stages);
    context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, bindPoint, pipelineLayout, Abi::HeapDescriptorSet, 1, &set, 0, nullptr);
}

}
