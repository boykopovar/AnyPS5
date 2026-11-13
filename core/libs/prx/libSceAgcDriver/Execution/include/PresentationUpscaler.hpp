#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONUPSCALER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONUPSCALER_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver {

class PresentationUpscaler {
public:
    PresentationUpscaler(const Graphics::Context& context, std::size_t slots, float sharpness);
    ~PresentationUpscaler();
    PresentationUpscaler(const PresentationUpscaler&) = delete;
    PresentationUpscaler& operator=(const PresentationUpscaler&) = delete;

    static bool Upscales(std::uint32_t width, std::uint32_t height, VkExtent2D extent);
    static VkExtent2D StageExtent(std::uint32_t width, std::uint32_t height, VkExtent2D extent);
    VkImage Stage(std::size_t slot, VkFormat format, VkExtent2D extent);
    void RecordStageTransferDestination(VkCommandBuffer commands, std::size_t slot);
    void RecordStageWritten(VkCommandBuffer commands, std::size_t slot, VkImageLayout layout, VkPipelineStageFlags stage, VkAccessFlags access);
    void RecordUpscale(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout);
    void RecordPresent(VkCommandBuffer commands, std::size_t slot, std::uint32_t width, std::uint32_t height, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout);
    void Release(std::size_t slot) noexcept;
    float Sharpness() const { return sharpness; }

private:
    enum class Filter { Easu, Rcas };
    struct Target {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };
    struct Surface {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView sampled = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
    };
    struct Slot {
        Surface stage;
        Surface upscaled;
        VkDescriptorSet easuSet = VK_NULL_HANDLE;
        VkDescriptorSet rcasSet = VK_NULL_HANDLE;
        std::vector<std::pair<VkImageView, VkFramebuffer>> transient;
    };

    const Target& target(VkFormat format, VkImageLayout finalLayout, Filter filter);
    void ensureSurface(Surface& surface, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage);
    void releaseSurface(Surface& surface) noexcept;
    VkImageView createView(VkImage image, VkFormat format, VkImageUsageFlags usage) const;
    VkFramebuffer transientFramebuffer(Slot& slot, VkRenderPass renderPass, VkImage image, VkFormat format, VkExtent2D extent, VkImageLayout finalLayout);
    void bindSource(VkDescriptorSet set, VkImageView view) const;
    void draw(VkCommandBuffer commands, const Target& target, VkFramebuffer framebuffer, VkExtent2D extent, VkDescriptorSet set, const void* parameters, std::uint32_t size) const;
    void release() noexcept;

    Graphics::Context context;
    float sharpness;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule easu = VK_NULL_HANDLE;
    VkShaderModule rcas = VK_NULL_HANDLE;
    std::map<std::tuple<VkFormat, VkImageLayout, Filter>, Target> targets;
    std::vector<Slot> slots;
};

}

#endif
