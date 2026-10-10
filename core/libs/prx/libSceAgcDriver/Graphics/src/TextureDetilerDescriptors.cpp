#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>

namespace AgcDriver::Graphics {

void TextureDetiler::BeginBatch() {
    GuestMemory::AssertGpuLockHeld("TextureDetiler::BeginBatch");
    // Sets of recorded work that has not completed stay allocated; the pools are recycled once the
    // recorder is idle again.
    if (const auto* recorder = Recorder::Active(); recorder != nullptr && !recorder->Idle()) return;
    for (const auto pool : descriptorPools) {
        Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), "vkResetDescriptorPool texture detiler");
    }
    allocatedSets = 0;
    for (const auto pool : cmaskDescriptorPools) {
        Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), "vkResetDescriptorPool CMASK clear");
    }
    allocatedCmaskSets = 0;
    for (const auto pool : imageDescriptorPools) {
        Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), "vkResetDescriptorPool texture detiler");
    }
    allocatedImageSets = 0;
}

VkDescriptorSet TextureDetiler::allocateSet(bool image) {
    constexpr std::uint32_t setsPerPool = 64;
    auto& pools = image ? imageDescriptorPools : descriptorPools;
    auto& allocated = image ? allocatedImageSets : allocatedSets;
    const auto poolIndex = allocated / setsPerPool;
    if (poolIndex == pools.size()) {
        std::array<VkDescriptorPoolSize, 2> sizes{};
        sizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, image ? setsPerPool : setsPerPool * 2};
        sizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setsPerPool};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets = setsPerPool;
        info.poolSizeCount = image ? 2u : 1u;
        info.pPoolSizes = sizes.data();
        pools.reserve(pools.size() + 1);
        VkDescriptorPool pool = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), "vkCreateDescriptorPool texture detiler");
        pools.push_back(pool);
    }
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = pools.at(poolIndex);
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = image ? &imageDescriptorLayout : &descriptorLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets texture detiler");
    ++allocated;
    return set;
}

}
