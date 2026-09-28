#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GPUCOLORTRANSFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GPUCOLORTRANSFER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <memory>

namespace AgcDriver::Graphics {

class GpuColorTransfer {
public:
    explicit GpuColorTransfer(const Context& context);
    ~GpuColorTransfer();
    GpuColorTransfer(const GpuColorTransfer&) = delete;
    GpuColorTransfer& operator=(const GpuColorTransfer&) = delete;
    void Upload(std::uint64_t address, std::uint32_t width, std::uint32_t height, ColorTileMode mode);
    void Detile(VkCommandBuffer commands, bool swapRedBlue = false, bool tenBit = false);
    void Tile(VkCommandBuffer commands);
    void WriteBack(std::uint64_t address);
    VkBuffer LinearBuffer() const;
    RenderTarget& Target(const ColorTarget& color, bool blending);

private:
    void prepare(std::uint32_t width, std::uint32_t height, ColorTileMode mode);
    void convert(VkCommandBuffer commands, bool toTiled, bool swapRedBlue, bool tenBit = false);
    void release() noexcept;
    Context context;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    // Guest bytes in host memory, and the device-local copies the conversion shader works on.
    std::unique_ptr<Buffer> tiled;
    std::unique_ptr<DeviceBuffer> tiledDevice;
    std::unique_ptr<DeviceBuffer> linear;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ColorTileMode mode = ColorTileMode::Linear;
    std::unique_ptr<RenderTarget> target;
    VkExtent2D targetExtent{};
    VkFormat targetFormat = VK_FORMAT_UNDEFINED;
    bool targetBlending = false;
};

}

#endif
