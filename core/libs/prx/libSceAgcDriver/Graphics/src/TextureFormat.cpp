#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

struct FormatEntry {
    std::uint32_t guestFormat;
    VkFormat vkFormat;
    std::uint32_t bytesPerElement;
    bool blockCompressed;
};

constexpr FormatEntry kFormatLookup[] = {
    {1, VK_FORMAT_R8_UNORM, 1, false},
    {2, VK_FORMAT_R8_SNORM, 1, false},
    {5, VK_FORMAT_R8_UINT, 1, false},
    {6, VK_FORMAT_R8_SINT, 1, false},
    {7, VK_FORMAT_R16_UNORM, 2, false},
    {8, VK_FORMAT_R16_SNORM, 2, false},
    {11, VK_FORMAT_R16_UINT, 2, false},
    {12, VK_FORMAT_R16_SINT, 2, false},
    {13, VK_FORMAT_R16_SFLOAT, 2, false},
    {14, VK_FORMAT_R8G8_UNORM, 2, false},
    {15, VK_FORMAT_R8G8_SNORM, 2, false},
    {18, VK_FORMAT_R8G8_UINT, 2, false},
    {19, VK_FORMAT_R8G8_SINT, 2, false},
    {20, VK_FORMAT_R32_UINT, 4, false},
    {21, VK_FORMAT_R32_SINT, 4, false},
    {22, VK_FORMAT_R32_SFLOAT, 4, false},
    {23, VK_FORMAT_R16G16_UNORM, 4, false},
    {24, VK_FORMAT_R16G16_SNORM, 4, false},
    {27, VK_FORMAT_R16G16_UINT, 4, false},
    {28, VK_FORMAT_R16G16_SINT, 4, false},
    {29, VK_FORMAT_R16G16_SFLOAT, 4, false},
    {36, VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4, false},
    {50, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4, false},
    {54, VK_FORMAT_A2B10G10R10_UINT_PACK32, 4, false},
    {56, VK_FORMAT_R8G8B8A8_UNORM, 4, false},
    {57, VK_FORMAT_R8G8B8A8_SNORM, 4, false},
    {60, VK_FORMAT_R8G8B8A8_UINT, 4, false},
    {61, VK_FORMAT_R8G8B8A8_SINT, 4, false},
    {62, VK_FORMAT_R32G32_UINT, 8, false},
    {63, VK_FORMAT_R32G32_SINT, 8, false},
    {64, VK_FORMAT_R32G32_SFLOAT, 8, false},
    {65, VK_FORMAT_R16G16B16A16_UNORM, 8, false},
    {66, VK_FORMAT_R16G16B16A16_SNORM, 8, false},
    {69, VK_FORMAT_R16G16B16A16_UINT, 8, false},
    {70, VK_FORMAT_R16G16B16A16_SINT, 8, false},
    {71, VK_FORMAT_R16G16B16A16_SFLOAT, 8, false},
    {72, VK_FORMAT_R32G32B32_UINT, 12, false},
    {73, VK_FORMAT_R32G32B32_SINT, 12, false},
    {74, VK_FORMAT_R32G32B32_SFLOAT, 12, false},
    {75, VK_FORMAT_R32G32B32A32_UINT, 16, false},
    {76, VK_FORMAT_R32G32B32A32_SINT, 16, false},
    {77, VK_FORMAT_R32G32B32A32_SFLOAT, 16, false},
    {128, VK_FORMAT_R8_SRGB, 1, false},
    {129, VK_FORMAT_R8G8_SRGB, 2, false},
    {130, VK_FORMAT_R8G8B8A8_SRGB, 4, false},
    {132, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 4, false},
    {133, VK_FORMAT_R5G6B5_UNORM_PACK16, 2, false},
    {134, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 2, false},
    {136, VK_FORMAT_R4G4B4A4_UNORM_PACK16, 2, false},
    {169, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, true},
    {170, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8, true},
    {171, VK_FORMAT_BC2_UNORM_BLOCK, 16, true},
    {172, VK_FORMAT_BC2_SRGB_BLOCK, 16, true},
    {173, VK_FORMAT_BC3_UNORM_BLOCK, 16, true},
    {174, VK_FORMAT_BC3_SRGB_BLOCK, 16, true},
    {175, VK_FORMAT_BC4_UNORM_BLOCK, 8, true},
    {176, VK_FORMAT_BC4_SNORM_BLOCK, 8, true},
    {177, VK_FORMAT_BC5_UNORM_BLOCK, 16, true},
    {178, VK_FORMAT_BC5_SNORM_BLOCK, 16, true},
    {179, VK_FORMAT_BC6H_UFLOAT_BLOCK, 16, true},
    {180, VK_FORMAT_BC6H_SFLOAT_BLOCK, 16, true},
    {181, VK_FORMAT_BC7_UNORM_BLOCK, 16, true},
    {182, VK_FORMAT_BC7_SRGB_BLOCK, 16, true},
};

constexpr std::uint32_t kMaxGuestFormat = 182;

constexpr auto MakeFormatLookupTable() {
    std::array<const FormatEntry*, kMaxGuestFormat + 1> table{};
    for (const auto& entry : kFormatLookup) {
        table[entry.guestFormat] = &entry;
    }
    return table;
}

constexpr auto kFormatLookupTable = MakeFormatLookupTable();

std::uint32_t remapGuestFormat(std::uint32_t guestFormat) {
    return guestFormat == 30 || guestFormat == 34 || guestFormat == 43 ? 20 : guestFormat;
}

const FormatEntry& findFormatEntry(std::uint32_t guestFormat) {
    const auto remapped = remapGuestFormat(guestFormat);
    const auto* entry = remapped <= kMaxGuestFormat ? kFormatLookupTable[remapped] : nullptr;
    if (entry == nullptr) Require(false, "unsupported guest texture format " + std::to_string(guestFormat));
    return *entry;
}

}

VkFormat ResolveTextureFormat(std::uint32_t guestFormat) {
    return findFormatEntry(guestFormat).vkFormat;
}

std::uint32_t SrgbDecodeFormats(PFN_vkGetPhysicalDeviceFormatProperties formatProperties, VkPhysicalDevice physical) {
    const char* forced = std::getenv("APS5_SRGB_SHADER_DECODE");
    const bool always = forced != nullptr && std::strcmp(forced, "1") == 0;
    const auto sampled = [&](ShaderRecompiler::IrBufferFormat format) {
        VkFormatProperties properties{};
        formatProperties(physical, ResolveTextureFormat(static_cast<std::uint32_t>(format)), &properties);
        return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
    };
    std::uint32_t formats = 0;
    for (const auto format : {ShaderRecompiler::IrBufferFormat::Format8Srgb, ShaderRecompiler::IrBufferFormat::Format8_8Srgb}) {
        if ((always || !sampled(format)) && sampled(ShaderRecompiler::SrgbUnormFormat(format))) formats |= ShaderRecompiler::SrgbDecodeBit(format);
    }
    return formats;
}

VkFormat SampledTextureFormat(const Context& context, std::uint32_t guestFormat) {
    const auto format = static_cast<ShaderRecompiler::IrBufferFormat>(guestFormat);
    if ((context.srgbDecodeFormats & ShaderRecompiler::SrgbDecodeBit(format)) == 0u) return ResolveTextureFormat(guestFormat);
    return ResolveTextureFormat(static_cast<std::uint32_t>(ShaderRecompiler::SrgbUnormFormat(format)));
}

bool IsSrgbTextureFormat(std::uint32_t guestFormat) {
    switch (findFormatEntry(guestFormat).vkFormat) {
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK: return true;
        default: return false;
    }
}

std::optional<std::uint32_t> FindGuestTextureFormat(VkFormat format, std::uint32_t elementBytes) {
    for (const auto& entry : kFormatLookup) {
        if (!entry.blockCompressed && entry.vkFormat == format && entry.bytesPerElement == elementBytes) return entry.guestFormat;
    }
    return std::nullopt;
}

VkComponentSwizzle TextureComponentChannel(std::uint32_t guestFormat, VkComponentSwizzle component) {
    if (component < VK_COMPONENT_SWIZZLE_R || component > VK_COMPONENT_SWIZZLE_A) return component;
    const auto index = static_cast<std::size_t>(component - VK_COMPONENT_SWIZZLE_R);
    constexpr std::array<VkComponentSwizzle, 4> bgrb{VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_B};
    constexpr std::array<VkComponentSwizzle, 4> bgra{VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A};
    constexpr std::array<VkComponentSwizzle, 4> abgr{VK_COMPONENT_SWIZZLE_A, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R};
    switch (guestFormat) {
        case 133: return bgrb[index];
        case 134: return bgra[index];
        case 136: return abgr[index];
        default: return component;
    }
}

std::uint32_t BytesPerElement(std::uint32_t guestFormat) {
    return findFormatEntry(guestFormat).bytesPerElement;
}

bool IsConvertedTextureFormat(std::uint32_t guestFormat) {
    return remapGuestFormat(guestFormat) != guestFormat;
}

std::optional<std::uint32_t> FindGuestColorTargetFormat(VkFormat format, std::uint32_t elementBytes) {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_UNORM: format = VK_FORMAT_R8G8B8A8_UNORM; break;
        case VK_FORMAT_B8G8R8A8_SRGB: format = VK_FORMAT_R8G8B8A8_SRGB; break;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: format = VK_FORMAT_A2B10G10R10_UNORM_PACK32; break;
        default: break;
    }
    return FindGuestTextureFormat(format, elementBytes);
}

bool IsBlockCompressed(std::uint32_t guestFormat) {
    return findFormatEntry(guestFormat).blockCompressed;
}

std::uint32_t BlockWidth(std::uint32_t guestFormat) {
    return IsBlockCompressed(guestFormat) ? 4u : 1u;
}

std::uint32_t BlockHeight(std::uint32_t guestFormat) {
    return IsBlockCompressed(guestFormat) ? 4u : 1u;
}

}
