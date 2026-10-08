#include "prx/libSceAgcDriver/Graphics/include/ImageTable.hpp"
#include <algorithm>
#include <cstdio>

namespace AgcDriver::Graphics {

void ImageTable::FrameStaging::Add(StagedDescriptor descriptor) {
    std::lock_guard lock(mutex);
    descriptors.push_back(std::move(descriptor));
}

std::vector<ImageTable::StagedDescriptor> ImageTable::FrameStaging::Take() {
    std::lock_guard lock(mutex);
    auto result = std::move(descriptors);
    descriptors.clear();
    return result;
}

std::size_t ImageTable::FrameStaging::Size() const {
    std::lock_guard lock(mutex);
    return descriptors.size();
}

ImageTable::ImageTable(std::uint32_t physicalCapacity) {
    initializeSlots(std::min(physicalCapacity, LogicalCapacity));
}

ImageTable::ImageTable(const Context& context) : context(context) {
    if (!context.descriptorTableUpdateAfterBind || context.descriptorTableCapacity == 0) return;
    initializeSlots(std::min(context.descriptorTableCapacity, LogicalCapacity));

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding.descriptorCount = physicalCapacity;
    binding.stageFlags = VK_SHADER_STAGE_ALL;
    const VkDescriptorBindingFlagsEXT bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT flags{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    flags.bindingCount = 1;
    flags.pBindingFlags = &bindingFlags;
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.pNext = &flags;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    const auto createLayout = context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout");
    auto result = createLayout(context.device, &layoutInfo, nullptr, &layout);
    if (result != VK_SUCCESS) {
        layout = VK_NULL_HANDLE;
        physicalCapacity = 0;
        freeList.clear();
        std::fprintf(stderr, "[image-table] disabled: vkCreateDescriptorSetLayout returned %d\n", result);
        return;
    }

    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, physicalCapacity};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &size;
    const auto createPool = context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
    result = createPool(context.device, &poolInfo, nullptr, &pool);
    if (result != VK_SUCCESS) {
        pool = VK_NULL_HANDLE;
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
        physicalCapacity = 0;
        freeList.clear();
        std::fprintf(stderr, "[image-table] disabled: vkCreateDescriptorPool returned %d\n", result);
        return;
    }

    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = pool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &layout;
    const auto allocate = context.Resolved(&DeviceFunctions::allocateDescriptorSets, "vkAllocateDescriptorSets");
    result = allocate(context.device, &allocation, &set);
    if (result != VK_SUCCESS) {
        set = VK_NULL_HANDLE;
        context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, layout, nullptr);
        pool = VK_NULL_HANDLE;
        layout = VK_NULL_HANDLE;
        physicalCapacity = 0;
        freeList.clear();
        std::fprintf(stderr, "[image-table] disabled: vkAllocateDescriptorSets returned %d\n", result);
    }
}

ImageTable::~ImageTable() {
    if (pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    if (layout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, layout, nullptr);
}

void ImageTable::initializeSlots(std::uint32_t capacity) {
    physicalCapacity = capacity;
    logicalToPhysical.fill(InvalidSlot);
    freeList.reserve(capacity);
    dirty.reserve(capacity);
    retiring.reserve(capacity);
    for (std::uint32_t slot = capacity; slot != 0; --slot) freeList.push_back(slot - 1);
}

std::optional<std::uint32_t> ImageTable::Acquire(std::uint32_t logicalIndex, std::uint64_t contentHash, std::uint64_t frame) {
    Require(logicalIndex < LogicalCapacity, "image table logical index exceeds capacity");
    Require(frame != 0, "image table frame serial must be nonzero");
    std::lock_guard lock(mutex);
    Require(frame > completedFrame, "image table frame serial is already complete");
    auto physicalIndex = logicalToPhysical[logicalIndex];
    if (physicalIndex != InvalidSlot && slots[physicalIndex].state == SlotState::Resident) {
        auto& slot = slots[physicalIndex];
        if (slot.contentHash == contentHash) {
            slot.used = true;
            slot.lastUsedFrame = std::max(slot.lastUsedFrame, frame);
            residentLru.splice(residentLru.end(), residentLru, lruPositions[physicalIndex]);
            return physicalIndex;
        }
        retire(physicalIndex);
    }
    reclaimCompleted();
    if (freeList.empty() && !residentLru.empty()) {
        retire(residentLru.front());
        reclaimCompleted();
    }
    if (freeList.empty()) return std::nullopt;

    physicalIndex = freeList.back();
    freeList.pop_back();
    auto& slot = slots[physicalIndex];
    slot.contentHash = contentHash;
    slot.logicalIndex = logicalIndex;
    slot.state = SlotState::Resident;
    slot.used = false;
    ++slot.version;
    logicalToPhysical[logicalIndex] = physicalIndex;
    residentLru.push_back(physicalIndex);
    lruPositions[physicalIndex] = std::prev(residentLru.end());
    slot.used = true;
    slot.lastUsedFrame = frame;
    residentLru.splice(residentLru.end(), residentLru, lruPositions[physicalIndex]);
    if (!dirtyBits.test(physicalIndex)) {
        dirtyBits.set(physicalIndex);
        dirty.push_back(physicalIndex);
    }
    return physicalIndex;
}

void ImageTable::MarkUsed(std::uint32_t physicalIndex, std::uint64_t frame) {
    Require(physicalIndex < physicalCapacity, "image table physical index exceeds capacity");
    Require(frame != 0, "image table frame serial must be nonzero");
    std::lock_guard lock(mutex);
    Require(frame > completedFrame, "image table frame serial is already complete");
    auto& slot = slots[physicalIndex];
    Require(slot.state == SlotState::Resident, "image table slot is not resident");
    slot.used = true;
    slot.lastUsedFrame = std::max(slot.lastUsedFrame, frame);
    residentLru.splice(residentLru.end(), residentLru, lruPositions[physicalIndex]);
}

void ImageTable::CompleteFrame(std::uint64_t frame) {
    std::lock_guard lock(mutex);
    completedFrame = std::max(completedFrame, frame);
    reclaimCompleted();
}

void ImageTable::MarkDirty(std::uint32_t physicalIndex) {
    Require(physicalIndex < physicalCapacity, "image table physical index exceeds capacity");
    std::lock_guard lock(mutex);
    Require(slots[physicalIndex].state == SlotState::Resident, "image table slot is not resident");
    if (!dirtyBits.test(physicalIndex)) {
        dirtyBits.set(physicalIndex);
        dirty.push_back(physicalIndex);
    }
}

std::vector<ImageTable::DirtyRun> ImageTable::DirtyRuns() const {
    std::lock_guard lock(mutex);
    auto indices = dirty;
    std::sort(indices.begin(), indices.end());
    std::vector<DirtyRun> runs;
    for (const auto index : indices) {
        if (!runs.empty() && runs.back().first + runs.back().count == index) {
            ++runs.back().count;
            runs.back().versions.push_back(slots[index].version);
        } else {
            runs.push_back({index, 1, {slots[index].version}});
        }
    }
    return runs;
}

void ImageTable::ClearDirty(std::span<const DirtyRun> runs) {
    std::lock_guard lock(mutex);
    for (const auto& run : runs) {
        Require(run.first <= physicalCapacity && run.count <= physicalCapacity - run.first, "image table dirty run exceeds capacity");
        Require(run.versions.size() == run.count, "image table dirty run is missing version snapshots");
        for (std::uint32_t offset = 0; offset < run.count; ++offset) {
            const auto index = run.first + offset;
            if (slots[index].version == run.versions[offset]) dirtyBits.reset(index);
        }
    }
    std::erase_if(dirty, [&](std::uint32_t index) { return !dirtyBits.test(index); });
}

ImageTable::Slot ImageTable::GetSlot(std::uint32_t physicalIndex) const {
    Require(physicalIndex < physicalCapacity, "image table physical index exceeds capacity");
    std::lock_guard lock(mutex);
    return slots[physicalIndex];
}

std::uint32_t ImageTable::PhysicalSlot(std::uint32_t logicalIndex) const {
    Require(logicalIndex < LogicalCapacity, "image table logical index exceeds capacity");
    std::lock_guard lock(mutex);
    return logicalToPhysical[logicalIndex];
}

ImageTable::FrameStaging& ImageTable::Staging(std::size_t frameIndex) {
    Require(frameIndex < staging.size(), "image table frame staging index exceeds capacity");
    return staging[frameIndex];
}

void ImageTable::retire(std::uint32_t physicalIndex) {
    auto& slot = slots[physicalIndex];
    if (slot.state != SlotState::Resident) return;
    if (slot.logicalIndex != InvalidSlot && logicalToPhysical[slot.logicalIndex] == physicalIndex) logicalToPhysical[slot.logicalIndex] = InvalidSlot;
    residentLru.erase(lruPositions[physicalIndex]);
    slot.state = SlotState::Retiring;
    retiring.push_back(physicalIndex);
}

void ImageTable::reclaimCompleted() {
    for (auto it = retiring.begin(); it != retiring.end();) {
        const auto physicalIndex = *it;
        const auto& slot = slots[physicalIndex];
        if (slot.used && slot.lastUsedFrame > completedFrame) {
            ++it;
            continue;
        }
        releaseSlot(physicalIndex);
        it = retiring.erase(it);
    }
}

void ImageTable::releaseSlot(std::uint32_t physicalIndex) {
    auto& slot = slots[physicalIndex];
    slot.contentHash = 0;
    slot.lastUsedFrame = 0;
    slot.logicalIndex = InvalidSlot;
    slot.state = SlotState::Free;
    slot.used = false;
    freeList.push_back(physicalIndex);
}

}
