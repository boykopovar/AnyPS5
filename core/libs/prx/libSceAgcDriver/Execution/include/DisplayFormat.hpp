#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPLAYFORMAT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPLAYFORMAT_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace AgcDriver {

VkFormat DisplayTexelFormat(std::uint64_t pixelFormat);
bool DisplayTenBit(std::uint64_t pixelFormat);
bool DisplayRedLow(std::uint64_t pixelFormat);
bool DisplayHalfFloat(std::uint64_t pixelFormat);
bool DisplayPq(std::uint64_t pixelFormat);
std::uint32_t DisplayTexelBytes(std::uint64_t pixelFormat);

enum class ResidentPresent : std::uint8_t { None, Blit, Convert };
ResidentPresent ResidentPresentPath(VkFormat storage, std::uint64_t pixelFormat, bool blitSource);
std::optional<VkSurfaceFormatKHR> Hdr10SurfaceFormat(std::span<const VkSurfaceFormatKHR> formats, VkFormat preferred);
VkSurfaceFormatKHR DynamicRangeSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats, std::uint64_t pixelFormat, bool hdrAvailable);
std::array<float, 4> PqClearColor(std::uint32_t word, bool redLow);
std::optional<std::uint32_t> ResidentPresentSlice(std::uint64_t imageAddress, std::uint64_t imageBytes, std::uint32_t slices, std::uint64_t displayAddress, std::uint64_t displayBytes);

}

#endif
