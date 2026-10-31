#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_IMAGEMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_IMAGEMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <memory>

namespace AgcDriver::Graphics {

class ImageAllocation {
public:
    ImageAllocation() = default;
    ~ImageAllocation();
    ImageAllocation(const ImageAllocation&) = delete;
    ImageAllocation& operator=(const ImageAllocation&) = delete;
    ImageAllocation(ImageAllocation&& other) noexcept;
    ImageAllocation& operator=(ImageAllocation&& other) noexcept;
    void Reset() noexcept;
    explicit operator bool() const noexcept { return memory != VK_NULL_HANDLE; }
    VkDeviceMemory Memory() const noexcept { return memory; }
    VkDeviceSize Offset() const noexcept { return offset; }
    VkDeviceSize Size() const noexcept { return bytes; }
    std::uint32_t MemoryType() const noexcept { return memoryType; }

private:
    friend class BufferPool;
    std::shared_ptr<BufferPool> owner;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize bytes = 0;
    std::uint32_t memoryType = 0;
    bool slab = false;
    bool dedicated = false;
};

ImageAllocation AllocateImageMemory(const Context& context, VkImage image, const VkImageCreateInfo& imageInfo, const char* operation, bool newImage = false, bool budgeted = false);
std::uint64_t ImageMemoryReleasedBytes() noexcept;

}

#endif
