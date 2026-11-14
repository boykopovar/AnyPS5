#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_FIRSTSAMPLECOPY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_FIRSTSAMPLECOPY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

namespace AgcDriver::Graphics {

class Recorder;
class Texture;

class FirstSampleCopy {
public:
    FirstSampleCopy(const Context& context, VkFormat format, VkExtent2D extent);
    ~FirstSampleCopy();
    FirstSampleCopy(const FirstSampleCopy&) = delete;
    FirstSampleCopy& operator=(const FirstSampleCopy&) = delete;

    void Record(Recorder& recorder, Texture& source);
    // Through a classic descriptor set, for a view without a texture.
    void Record(Recorder& recorder, VkImageView source, VkImageLayout layout);
    VkImage Image() const { return image; }

private:
    void release() noexcept;

    Context context;
    VkExtent2D extent;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipeline heapPipeline = VK_NULL_HANDLE;
};

}

#endif
