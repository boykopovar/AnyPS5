#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PACKEDFLOATRESOLVE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PACKEDFLOATRESOLVE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <memory>

namespace AgcDriver::Graphics {

class Recorder;

class PackedFloatResolve {
public:
    explicit PackedFloatResolve(const Context& context);
    ~PackedFloatResolve();
    PackedFloatResolve(const PackedFloatResolve&) = delete;
    PackedFloatResolve& operator=(const PackedFloatResolve&) = delete;

    VkRenderPass RenderPass() const { return renderPass; }
    void Record(VkCommandBuffer commands, Recorder* recorder, VkImageView source, VkFramebuffer framebuffer, VkExtent2D extent, std::uint32_t samples, std::shared_ptr<void>& keep);

private:
    void release() noexcept;

    Context context;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

}

#endif
