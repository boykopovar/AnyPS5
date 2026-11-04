#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONSCALER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONSCALER_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <vector>

namespace AgcDriver {

class PresentationScaler {
public:
    PresentationScaler(const Graphics::Context& context, VkFormat sourceFormat, VkFormat destinationFormat);
    ~PresentationScaler();
    PresentationScaler(const PresentationScaler&) = delete;
    PresentationScaler& operator=(const PresentationScaler&) = delete;

    void EnsureSourceImage(std::uint32_t width, std::uint32_t height);
    void RecordUpload(VkCommandBuffer commands, VkBuffer uploadBuffer);
    void RecordClear(VkCommandBuffer commands, const VkClearColorValue& color);
    void RecordImage(VkCommandBuffer commands, VkImage image);
    // Fills the source image from another image of the same extent (any blittable format, layout
    // unchanged), leaving the source image ready to blit or read back.
    void RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter);
    void RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter, std::uint32_t width, std::uint32_t height, std::uint32_t layer = 0);
    // Copies the source image's texels (tightly packed rows of its format) into a host-readable buffer
    // and makes them visible to the host once the submission completes.
    void RecordReadback(VkCommandBuffer commands, VkBuffer destination);
    void RecordBlit(VkCommandBuffer commands, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight);
    // The aspect-fitting blit of RecordBlit from any image: presenting a resident render target needs
    // no copy into the source image first.
    static void RecordBlitFrom(const Graphics::Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout layout, std::uint32_t width, std::uint32_t height, VkFilter filter, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight, std::uint32_t layer = 0);
    void RecordLetterbox(VkCommandBuffer commands, std::uint32_t width, std::uint32_t height, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight);
    static std::vector<AspectFitRect> LetterboxBands(std::uint32_t width, std::uint32_t height, std::uint32_t destinationWidth, std::uint32_t destinationHeight);
    std::uint32_t SourceWidth() const { return sourceWidth; }
    std::uint32_t SourceHeight() const { return sourceHeight; }
    VkImage SourceImage() const { return sourceImage; }
    VkFormat SourceFormat() const { return sourceFormat; }

private:
    void release() noexcept;

    Graphics::Context context;
    VkFormat sourceFormat;
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    VkImage sourceImage = VK_NULL_HANDLE;
    VkDeviceMemory sourceMemory = VK_NULL_HANDLE;
    VkDeviceSize sourceBytes = 0;
    std::uint32_t sourceMemoryType = 0;
    VkImage blackImage = VK_NULL_HANDLE;
    VkDeviceMemory blackMemory = VK_NULL_HANDLE;
};

}

#endif
