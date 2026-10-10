#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREFORMAT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREFORMAT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>

namespace AgcDriver::Graphics {

struct Context;

VkFormat ResolveTextureFormat(std::uint32_t guestFormat);
VkComponentSwizzle TextureComponentChannel(std::uint32_t guestFormat, VkComponentSwizzle component);
std::uint32_t SrgbDecodeFormats(PFN_vkGetPhysicalDeviceFormatProperties formatProperties, VkPhysicalDevice physical);
VkFormat SampledTextureFormat(const Context& context, std::uint32_t guestFormat);
bool IsSrgbTextureFormat(std::uint32_t guestFormat);
std::optional<std::uint32_t> FindGuestTextureFormat(VkFormat format, std::uint32_t elementBytes);
std::optional<std::uint32_t> FindGuestColorTargetFormat(VkFormat format, std::uint32_t elementBytes);
std::uint32_t BytesPerElement(std::uint32_t guestFormat);
bool IsConvertedTextureFormat(std::uint32_t guestFormat);
bool IsBlockCompressed(std::uint32_t guestFormat);
std::uint32_t BlockWidth(std::uint32_t guestFormat);
std::uint32_t BlockHeight(std::uint32_t guestFormat);

}

#endif
