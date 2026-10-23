#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <algorithm>
#include <cstdio>
#include <memory>

using namespace AgcDriver::Graphics;

namespace {

struct Image {
    explicit Image(const Context& context) : context(context) {}
    ~Image() {
        if (image != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory != VK_NULL_HANDLE) ReleaseImageMemory(context, memory, bytes, type);
    }
    Context context;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize bytes = 0;
    std::uint32_t type = 0;
};

std::unique_ptr<Image> makeImage(const Context& context, const char* category) {
    auto image = std::make_unique<Image>(context);
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {1024, 1024, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image->image), "vkCreateImage accounting test");
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image->image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->bytes = requirements.size;
    image->memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, category, &image->type);
    Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image->image, image->memory, 0), "vkBindImageMemory accounting test");
    return image;
}

}

int main() {
    try {
        Require(MemoryAccounting::Enabled(), "memory accounting is not enabled");
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto context = device->graphicsContext();
        context.bufferPool = std::make_shared<BufferPool>(context);
        const auto baseline = MemoryAccounting::Read(context.device).allocations.size();
        constexpr auto operations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT;
        if ((context.subgroup.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT) != 0 && (context.subgroup.supportedOperations & operations) == operations) {
            auto ldsContext = context;
            ldsContext.limits.maxStorageBufferRange = std::min<std::uint32_t>(ldsContext.limits.maxStorageBufferRange, 16u << 20u);
            PixelFunctionLdsSetLayouts(ldsContext);
            {
                CommandBatch batch(ldsContext);
                PreparePixelFunctionLds(ldsContext, batch.Handle(), ldsContext.subgroup.subgroupSize);
                const auto snapshot = MemoryAccounting::Read(context.device);
                Require(snapshot.complete && snapshot.allocations.size() == baseline + 1, "pixel function LDS allocation was not counted");
                const auto found = std::find_if(snapshot.allocations.begin(), snapshot.allocations.end(), [](const auto& allocation) { return allocation.category == "pixel function LDS"; });
                Require(found != snapshot.allocations.end() && found->bytes != 0 && found->retained == 0 && found->heap < context.memory.memoryHeapCount, "pixel function LDS memory has the wrong accounting state");
                batch.SubmitAndWait();
            }
            ReleasePixelFunctionLds(context.device);
            Require(MemoryAccounting::Read(context.device).allocations.size() == baseline, "pixel function LDS destruction left an allocation counted");
            std::puts("pixel function LDS allocation accounting and cleanup passed");
        }
        {
            FirstSampleCopy copy(context, VK_FORMAT_R8G8B8A8_UNORM, {32, 32});
            const auto snapshot = MemoryAccounting::Read(context.device);
            Require(snapshot.complete && snapshot.allocations.size() == baseline + 1, "first-sample image allocation was not counted");
            const auto found = std::find_if(snapshot.allocations.begin(), snapshot.allocations.end(), [](const auto& allocation) { return allocation.category == "vkAllocateMemory first sample"; });
            Require(found != snapshot.allocations.end() && found->bytes != 0 && found->retained == 0, "first-sample image memory has the wrong accounting state");
        }
        Require(MemoryAccounting::Read(context.device).allocations.size() == baseline, "first-sample image destruction left an allocation counted");
        auto first = makeImage(context, "accounting first image");
        const auto memory = first->memory;
        const auto bytes = first->bytes;
        const auto heap = context.memory.memoryTypes[first->type].heapIndex;
        const auto properties = context.memory.memoryTypes[first->type].propertyFlags;
        const bool pooled = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 && bytes <= (VkDeviceSize{8} << 20u);
        const auto find = [&](VkDeviceMemory handle) {
            const auto snapshot = MemoryAccounting::Read(context.device);
            Require(snapshot.complete, "real Vulkan memory accounting is incomplete");
            const auto found = std::find_if(snapshot.allocations.begin(), snapshot.allocations.end(), [&](const auto& allocation) { return allocation.memory == handle; });
            Require(found != snapshot.allocations.end(), "a real Vulkan allocation is missing");
            return *found;
        };
        Require(find(memory).bytes == bytes && find(memory).heap == heap && find(memory).retained == 0, "real image memory has the wrong byte count or heap");
        first.reset();
        if (pooled) Require(find(memory).retained == bytes, "real image pool retention was not counted");
        auto second = makeImage(context, "accounting second image");
        Require(!pooled || second->memory == memory, "the real image allocation was not reused");
        Require(find(second->memory).category == "accounting second image" && find(second->memory).retained == 0, "real image reuse kept its previous accounting state");
        Require(MemoryAccounting::Read(context.device).allocations.size() == baseline + 1, "real image reuse changed the number of Vulkan allocations");
        StorageTextureCacheBudget(context);
        second.reset();
        context.bufferPool.reset();
        Require(MemoryAccounting::Read(context.device).allocations.size() == baseline, "real image pool destruction left an allocation counted");
        std::puts("real Vulkan allocation accounting and cleanup passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
