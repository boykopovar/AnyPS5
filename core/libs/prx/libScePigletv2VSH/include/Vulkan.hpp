#ifndef CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_VULKAN_HPP
#define CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_VULKAN_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>

#define PIGLET_DEVICE_FUNCTIONS(X) \
    X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkCreateCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandBuffer) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkQueueSubmit) X(vkCreateFence) X(vkWaitForFences) X(vkResetFences) \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) X(vkBindBufferMemory) \
    X(vkMapMemory) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView) \
    X(vkDestroyImageView) X(vkCreateSampler) X(vkDestroySampler) X(vkCreateRenderPass) X(vkCreateFramebuffer) X(vkCreateShaderModule) \
    X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateGraphicsPipelines) X(vkCreateDescriptorPool) \
    X(vkResetDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) \
    X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdBindVertexBuffers) X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdDraw) \
    X(vkCmdClearAttachments) X(vkCmdPipelineBarrier) X(vkCmdCopyBufferToImage) X(vkCmdCopyImageToBuffer) \
    X(vkDestroyPipeline) X(vkDestroyPipelineLayout) X(vkDestroyDescriptorSetLayout) X(vkDestroyFramebuffer) \
    X(vkDestroyDescriptorPool) X(vkFreeCommandBuffers)

namespace Piglet {

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

class Device {
public:
    static Device& Get();

    Buffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool readback = false);
    void DestroyBuffer(Buffer& buffer);
    Image CreateImage(std::uint32_t width, std::uint32_t height, VkImageUsageFlags usage);
    void DestroyImage(Image& image);
    void Submit(VkCommandBuffer commands);
    void RunOnce(const std::function<void(VkCommandBuffer)>& record);
    VkCommandBuffer AllocateCommandBuffer();

    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDeviceSize uniformAlignment = 256;
    std::mutex submitMutex;
    bool depthClipControl = false;
#define PIGLET_DECLARE(name) PFN_##name name = nullptr;
    PIGLET_DEVICE_FUNCTIONS(PIGLET_DECLARE)
#undef PIGLET_DECLARE

private:
    Device();
    std::uint32_t MemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const;
    bool HasMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
};

void Check(VkResult result, const char* what);

}

#endif
