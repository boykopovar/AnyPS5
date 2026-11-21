#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONUPSCALER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONUPSCALER_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Execution/include/DlssUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/UpscaleMode.hpp"
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
#include <memory>

namespace AgcDriver {

struct PresentationBuffers {
    VkImageView color = VK_NULL_HANDLE;
    VkImage colorImage = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkImageView depth = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkImageView motionVectors = VK_NULL_HANDLE;
    VkImage motionVectorsImage = VK_NULL_HANDLE;
    VkFormat motionVectorsFormat = VK_FORMAT_UNDEFINED;
    float jitterX = 0.0f;
    float jitterY = 0.0f;
    bool reset = false;
};

class PresentationUpscaler {
public:
    PresentationUpscaler(const Graphics::Context& context, std::size_t slots, float sharpness, bool dlss = false, VkInstance instance = VK_NULL_HANDLE, VkExtent2D inputExtent = {}, VkExtent2D outputExtent = {}, int dlssQuality = 2);
    PresentationUpscaler(const Graphics::Context& context, std::size_t slots, const UpscaleMode& mode, VkInstance instance = VK_NULL_HANDLE, VkExtent2D inputExtent = {}, VkExtent2D outputExtent = {});
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
    void RecordPresent(VkCommandBuffer commands, std::size_t slot, std::uint32_t width, std::uint32_t height, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout, const PresentationBuffers& buffers);
    void Release(std::size_t slot) noexcept;
    float Sharpness() const { return sharpness; }
    bool IsDlss() const noexcept { return isDlss; }

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
        VkImageView storage = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        VkImageUsageFlags usage = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
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
    void ensureDepthSurface(Surface& surface, VkFormat format, VkExtent2D extent);
    void ensureFallbackSurfaces(VkCommandBuffer commands, VkExtent2D extent);
    void recordDlss(VkCommandBuffer commands, std::size_t index, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout, const PresentationBuffers* customBuffers);
    void releaseSurface(Surface& surface) noexcept;
    VkImageView createView(VkImage image, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) const;
    VkFramebuffer transientFramebuffer(Slot& slot, VkRenderPass renderPass, VkImage image, VkFormat format, VkExtent2D extent, VkImageLayout finalLayout);
    void bindSource(VkDescriptorSet set, VkImageView view) const;
    void draw(VkCommandBuffer commands, const Target& target, VkFramebuffer framebuffer, VkExtent2D extent, VkDescriptorSet set, const void* parameters, std::uint32_t size) const;
    void release() noexcept;

    Graphics::Context context;
    float sharpness;
    bool isDlss = false;
    int dlssQuality = 2;
    VkInstance instance = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule easu = VK_NULL_HANDLE;
    VkShaderModule rcas = VK_NULL_HANDLE;
    std::map<std::tuple<VkFormat, VkImageLayout, Filter>, Target> targets;
    std::vector<Slot> slots;
    std::unique_ptr<DlssUpscaler> dlssUpscaler;
    VkExtent2D dlssInputExtent{};
    VkExtent2D dlssOutputExtent{};
    Surface fallbackDepth;
    Surface fallbackMotionVectors;
    bool firstDlssFrame = true;
};

}

#endif
