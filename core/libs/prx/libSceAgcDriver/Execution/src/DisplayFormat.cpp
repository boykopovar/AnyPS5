#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace AgcDriver {
namespace {

constexpr std::uint64_t PixelFormatUnormBit = 0x0100000000000000ull;
constexpr std::uint64_t PixelFormatR8G8B8A8 = 0x8000000022000000ull;
constexpr std::uint64_t PixelFormatR16G16B16A16Float = 0xc001000622000000ull;
constexpr std::uint64_t PixelFormatB16G16R16A16Float = 0xc001000600000000ull;
constexpr std::uint64_t PixelFormatR10G10B10A2Bt2100Pq = 0x8100070422000000ull;
constexpr std::uint64_t PixelFormatB10G10R10A2Bt2100Pq = 0x8100070400000000ull;

}

bool DisplayTenBit(std::uint64_t pixelFormat) {
    return (pixelFormat & PixelFormatUnormBit) != 0;
}

bool DisplayRedLow(std::uint64_t pixelFormat) {
    return (pixelFormat & ~PixelFormatUnormBit) == PixelFormatR8G8B8A8 || pixelFormat == PixelFormatR16G16B16A16Float || pixelFormat == PixelFormatR10G10B10A2Bt2100Pq;
}

bool DisplayPq(std::uint64_t pixelFormat) {
    return pixelFormat == PixelFormatR10G10B10A2Bt2100Pq || pixelFormat == PixelFormatB10G10R10A2Bt2100Pq;
}

bool DisplayHalfFloat(std::uint64_t pixelFormat) {
    return pixelFormat == PixelFormatR16G16B16A16Float || pixelFormat == PixelFormatB16G16R16A16Float;
}

std::uint32_t DisplayTexelBytes(std::uint64_t pixelFormat) {
    return DisplayHalfFloat(pixelFormat) ? 8u : 4u;
}

VkFormat DisplayTexelFormat(std::uint64_t pixelFormat) {
    if (DisplayHalfFloat(pixelFormat)) return VK_FORMAT_R16G16B16A16_SFLOAT;
    const bool redLow = DisplayRedLow(pixelFormat);
    if (DisplayTenBit(pixelFormat)) return redLow ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    return redLow ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_B8G8R8A8_UNORM;
}

ResidentPresent ResidentPresentPath(VkFormat storage, std::uint64_t pixelFormat, bool blitSource) {
    if (DisplayHalfFloat(pixelFormat)) return storage == VK_FORMAT_R16G16B16A16_SFLOAT ? ResidentPresent::Convert : ResidentPresent::None;
    const bool tenBit = DisplayTenBit(pixelFormat);
    const bool pq = DisplayPq(pixelFormat);
    if (!tenBit && blitSource && storage == DisplayTexelFormat(pixelFormat)) return ResidentPresent::Blit;
    switch (storage) {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            return tenBit ? ResidentPresent::Convert : ResidentPresent::None;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UINT:
            return (tenBit && pq) ? ResidentPresent::None : ResidentPresent::Convert;
        default:
            return ResidentPresent::None;
    }
}

std::optional<VkSurfaceFormatKHR> Hdr10SurfaceFormat(std::span<const VkSurfaceFormatKHR> formats, VkFormat preferred) {
    const auto offered = [&](VkFormat format) {
        return std::any_of(formats.begin(), formats.end(), [&](const VkSurfaceFormatKHR& item) { return item.format == format && item.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT; });
    };
    const std::array<VkFormat, 2> tenBit{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32};
    if (std::find(tenBit.begin(), tenBit.end(), preferred) != tenBit.end() && offered(preferred)) return VkSurfaceFormatKHR{preferred, VK_COLOR_SPACE_HDR10_ST2084_EXT};
    for (const auto format : tenBit) {
        if (offered(format)) return VkSurfaceFormatKHR{format, VK_COLOR_SPACE_HDR10_ST2084_EXT};
    }
    return std::nullopt;
}

VkSurfaceFormatKHR DynamicRangeSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats, std::uint64_t pixelFormat, bool hdrAvailable) {
    if (!DisplayPq(pixelFormat) || !hdrAvailable) return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    const auto format = Hdr10SurfaceFormat(formats, DisplayTexelFormat(pixelFormat));
    if (!format) throw std::runtime_error("VideoOut: HDR output is available but the surface offers no 10-bit HDR10 format");
    return *format;
}

std::array<float, 4> PqClearColor(std::uint32_t word, bool redLow) {
    const auto channel = [&](unsigned shift) { return static_cast<float>((word >> shift) & 0x3ffu) / 1023.0f; };
    return redLow ? std::array<float, 4>{channel(0), channel(10), channel(20), 1.0f} : std::array<float, 4>{channel(20), channel(10), channel(0), 1.0f};
}

std::optional<std::uint32_t> ResidentPresentSlice(std::uint64_t imageAddress, std::uint64_t imageBytes, std::uint32_t slices, std::uint64_t displayAddress, std::uint64_t displayBytes) {
    if (slices == 0 || displayBytes == 0 || imageBytes % slices != 0 || imageBytes / slices != displayBytes || displayAddress < imageAddress) return std::nullopt;
    const auto offset = displayAddress - imageAddress;
    if (offset % displayBytes != 0 || offset / displayBytes >= slices) return std::nullopt;
    return static_cast<std::uint32_t>(offset / displayBytes);
}

}
