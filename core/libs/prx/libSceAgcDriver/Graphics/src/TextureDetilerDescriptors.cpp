#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace AgcDriver::Graphics {

void TextureDetiler::BeginBatch() {
    GuestMemory::AssertGpuLockHeld("TextureDetiler::BeginBatch");
    if (const auto* recorder = Recorder::Active(); recorder != nullptr && !recorder->Idle()) return;
    resetRing(descriptorSets, "vkResetDescriptorPool texture detiler");
    resetRing(cmaskDescriptorSets, "vkResetDescriptorPool CMASK clear");
    resetRing(imageDescriptorSets, "vkResetDescriptorPool texture detiler");
    resetRing(readbackDescriptorSets, "vkResetDescriptorPool multisample readback");
}

void TextureDetiler::resetRing(DescriptorRing& ring, const char* resetWhat) {
    for (const auto pool : ring.pools) Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), resetWhat);
    std::fill(ring.serials.begin(), ring.serials.end(), std::uint64_t{0});
    ring.current = 0;
    ring.used = 0;
}

VkDescriptorSet TextureDetiler::allocateFrom(DescriptorRing& ring, VkDescriptorSetLayout layout, const std::vector<VkDescriptorPoolSize>& sizes, const char* createWhat, const char* allocateWhat, const char* resetWhat) {
    constexpr std::uint32_t setsPerPool = 64;
    const auto* recorder = Recorder::Active();
    if (ring.pools.empty() || ring.used == setsPerPool) {
        const auto completed = recorder != nullptr ? recorder->CompletedThrough() : 0;
        auto next = ring.pools.size();
        for (std::size_t step = 1; step <= ring.pools.size(); ++step) {
            const auto index = (ring.current + step) % ring.pools.size();
            if (ring.serials[index] <= completed) {
                next = index;
                break;
            }
        }
        if (next < ring.pools.size()) {
            Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, ring.pools[next], 0), resetWhat);
        } else {
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.maxSets = setsPerPool;
            info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
            info.pPoolSizes = sizes.data();
            ring.pools.reserve(ring.pools.size() + 1);
            ring.serials.reserve(ring.serials.size() + 1);
            VkDescriptorPool pool = VK_NULL_HANDLE;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), createWhat);
            ring.pools.push_back(pool);
            ring.serials.push_back(0);
        }
        ring.current = next;
        ring.used = 0;
    }
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = ring.pools[ring.current];
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), allocateWhat);
    ++ring.used;
    ring.serials[ring.current] = recorder != nullptr ? recorder->OpenSerial() : std::numeric_limits<std::uint64_t>::max();
    return set;
}

VkDescriptorSet TextureDetiler::allocateSet(bool image) {
    if (image) return allocateFrom(imageDescriptorSets, imageDescriptorLayout, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64}}, "vkCreateDescriptorPool texture detiler", "vkAllocateDescriptorSets texture detiler", "vkResetDescriptorPool texture detiler");
    return allocateFrom(descriptorSets, descriptorLayout, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 * 2}}, "vkCreateDescriptorPool texture detiler", "vkAllocateDescriptorSets texture detiler", "vkResetDescriptorPool texture detiler");
}

}
