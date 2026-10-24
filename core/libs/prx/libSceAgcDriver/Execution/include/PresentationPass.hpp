#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONPASS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONPASS_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace AgcDriver {

namespace Graphics {
class Buffer;
}

class PresentationPass {
public:
    PresentationPass(const Graphics::Context& context, std::size_t slots);
    ~PresentationPass();
    PresentationPass(const PresentationPass&) = delete;
    PresentationPass& operator=(const PresentationPass&) = delete;

    void Bind(std::size_t slot, VkImage image, std::uint32_t width, std::uint32_t height, std::uint64_t pixelFormat, float whiteNits, std::uint32_t layer = 0);
    void RecordPresent(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout finalLayout);
    void RecordStage(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout finalLayout);
    void RecordDump(VkCommandBuffer commands, std::size_t slot, std::uint32_t stride, VkBuffer destination);
    void Release(std::size_t slot) noexcept;
    static bool TenBitFormat(VkFormat format);
    static VkExtent2D DumpExtent(std::uint32_t width, std::uint32_t height, std::uint32_t stride);
    static VkSubpassDependency ExitDependency(VkImageLayout finalLayout);

private:
    struct Target {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };
    struct Slot {
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkImageView source = VK_NULL_HANDLE;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t mode = 0;
        float whiteNits = 0.0f;
        std::vector<std::pair<VkImageView, VkFramebuffer>> transient;
        VkImage dumpImage = VK_NULL_HANDLE;
        VkDeviceMemory dumpMemory = VK_NULL_HANDLE;
        VkImageView dumpView = VK_NULL_HANDLE;
        VkFramebuffer dumpFramebuffer = VK_NULL_HANDLE;
        VkExtent2D dumpExtent{};
    };

    const Target& target(VkFormat format, VkImageLayout finalLayout);
    VkImageView createView(VkImage image, VkFormat format, VkImageUsageFlags usage, std::uint32_t layer = 0) const;
    VkFramebuffer createFramebuffer(VkRenderPass renderPass, VkImageView view, VkExtent2D extent) const;
    void ensureDumpTarget(Slot& slot, VkExtent2D extent);
    void releaseDumpTarget(Slot& slot) noexcept;
    void draw(VkCommandBuffer commands, const Slot& slot, const Target& target, VkFramebuffer framebuffer, VkExtent2D extent, const std::int32_t (&parameters)[9]) const;
    void recordInto(VkCommandBuffer commands, std::size_t slot, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout finalLayout, const AspectFitRect& rect);
    void release() noexcept;

    Graphics::Context context;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    std::unique_ptr<Graphics::Buffer> pqNits;
    std::map<std::pair<VkFormat, VkImageLayout>, Target> targets;
    std::vector<Slot> slots;
};

}

#endif
