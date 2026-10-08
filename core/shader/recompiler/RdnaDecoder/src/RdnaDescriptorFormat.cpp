#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>

namespace ShaderRecompiler {

namespace {

struct FormatInfo {
    IrBufferFormat format;
    bool sampled;
    bool uint32;
    bool sint32;
};

constexpr std::array<FormatInfo, 80> kFormatInfoTable {{
    {IrBufferFormat::Format8UNorm, true, false, false},
    {IrBufferFormat::Format8SNorm, false, false, false},
    {IrBufferFormat::Format8UInt, true, true, false},
    {IrBufferFormat::Format8SInt, true, false, true},
    {IrBufferFormat::Format16UNorm, true, false, false},
    {IrBufferFormat::Format16SNorm, true, false, false},
    {IrBufferFormat::Format16UInt, true, true, false},
    {IrBufferFormat::Format16SInt, true, false, true},
    {IrBufferFormat::Format16Float, true, false, false},
    {IrBufferFormat::Format8_8UNorm, true, false, false},
    {IrBufferFormat::Format8_8SNorm, true, false, false},
    {IrBufferFormat::Format8_8UInt, true, true, false},
    {IrBufferFormat::Format8_8SInt, true, false, true},
    {IrBufferFormat::Format32UInt, true, true, false},
    {IrBufferFormat::Format32SInt, true, false, true},
    {IrBufferFormat::Format32Float, true, false, false},
    {IrBufferFormat::Format16_16UNorm, true, false, false},
    {IrBufferFormat::Format16_16SNorm, true, false, false},
    {IrBufferFormat::Format16_16UInt, true, true, false},
    {IrBufferFormat::Format16_16SInt, true, false, true},
    {IrBufferFormat::Format16_16Float, true, false, false},
    {IrBufferFormat::Format11_11_10UNorm, true, true, false},
    {IrBufferFormat::Format11_11_10UInt, true, true, false},
    {IrBufferFormat::Format11_11_10Float, true, false, false},
    {IrBufferFormat::Format10_11_11Float, true, true, false},
    {IrBufferFormat::Format10_10_10_2UNorm, true, false, false},
    {IrBufferFormat::Format10_10_10_2UInt, true, true, false},
    {IrBufferFormat::Format8_8_8_8UNorm, true, false, false},
    {IrBufferFormat::Format8_8_8_8SNorm, true, false, false},
    {IrBufferFormat::Format8_8_8_8UInt, true, true, false},
    {IrBufferFormat::Format8_8_8_8SInt, true, false, true},
    {IrBufferFormat::Format32_32UInt, true, true, false},
    {IrBufferFormat::Format32_32SInt, true, false, true},
    {IrBufferFormat::Format32_32Float, true, false, false},
    {IrBufferFormat::Format16_16_16_16UNorm, true, false, false},
    {IrBufferFormat::Format16_16_16_16SNorm, true, false, false},
    {IrBufferFormat::Format16_16_16_16UInt, true, true, false},
    {IrBufferFormat::Format16_16_16_16SInt, true, false, true},
    {IrBufferFormat::Format16_16_16_16Float, true, false, false},
    {IrBufferFormat::Format32_32_32UInt, true, true, false},
    {IrBufferFormat::Format32_32_32SInt, true, false, true},
    {IrBufferFormat::Format32_32_32Float, true, false, false},
    {IrBufferFormat::Format32_32_32_32UInt, true, true, false},
    {IrBufferFormat::Format32_32_32_32SInt, true, false, true},
    {IrBufferFormat::Format32_32_32_32Float, true, false, false},
    {IrBufferFormat::Format8Srgb, true, false, false},
    {IrBufferFormat::Format8_8Srgb, true, false, false},
    {IrBufferFormat::Format8_8_8_8Srgb, true, false, false},
    {IrBufferFormat::Format9_9_9_5Float, true, false, false},
    {IrBufferFormat::Format5_6_5UNorm, true, false, false},
    {IrBufferFormat::Format5_5_5_1UNorm, true, false, false},
    {IrBufferFormat::Format1_5_5_5UNorm, false, false, false},
    {IrBufferFormat::Format4_4_4_4UNorm, true, false, false},
    {IrBufferFormat::Fmask8_S2_F1, true, false, false},
    {IrBufferFormat::Fmask8_S4_F1, true, false, false},
    {IrBufferFormat::Fmask8_S8_F1, true, false, false},
    {IrBufferFormat::Fmask8_S2_F2, true, false, false},
    {IrBufferFormat::Fmask8_S4_F2, true, false, false},
    {IrBufferFormat::Fmask8_S4_F4, true, false, false},
    {IrBufferFormat::Fmask16_S16_F1, true, false, false},
    {IrBufferFormat::Fmask16_S8_F2, true, false, false},
    {IrBufferFormat::Fmask32_S16_F2, true, false, false},
    {IrBufferFormat::Fmask32_S8_F4, true, false, false},
    {IrBufferFormat::Fmask32_S8_F8, true, false, false},
    {IrBufferFormat::Fmask64_S16_F4, true, false, false},
    {IrBufferFormat::Fmask64_S16_F8, true, false, false},
    {IrBufferFormat::Bc1UNorm, true, false, false},
    {IrBufferFormat::Bc1Srgb, true, false, false},
    {IrBufferFormat::Bc2UNorm, true, false, false},
    {IrBufferFormat::Bc2Srgb, true, false, false},
    {IrBufferFormat::Bc3UNorm, true, false, false},
    {IrBufferFormat::Bc3Srgb, true, false, false},
    {IrBufferFormat::Bc4UNorm, true, false, false},
    {IrBufferFormat::Bc4SNorm, true, false, false},
    {IrBufferFormat::Bc5UNorm, true, false, false},
    {IrBufferFormat::Bc5SNorm, true, false, false},
    {IrBufferFormat::Bc6UFloat, true, false, false},
    {IrBufferFormat::Bc6SFloat, true, false, false},
    {IrBufferFormat::Bc7UNorm, true, false, false},
    {IrBufferFormat::Bc7Srgb, true, false, false}
}};

const FormatInfo* FindFormatInfo(IrBufferFormat format) {
    const auto it = std::ranges::find_if(kFormatInfoTable, [format](const FormatInfo& entry) {
        return entry.format == format;
    });
    return it != kFormatInfoTable.end() ? &(*it) : nullptr;
}

}

bool IsFmaskTextureFormat(IrBufferFormat format) {
    return format >= IrBufferFormat::Fmask8_S2_F1 && format <= IrBufferFormat::Fmask64_S16_F8;
}

IrTextureNumericClass SampledTextureNumericClass(IrBufferFormat format) {
    const FormatInfo* info = FindFormatInfo(format);
    if (info == nullptr || !info->sampled) {
        return IrTextureNumericClass::Unsupported;
    }
    if (info->sint32) {
        return IrTextureNumericClass::Sint;
    }
    return info->uint32 ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
}

IrBufferFormat RemapTextureFormat(IrBufferFormat format) {
    return format == IrBufferFormat::Format11_11_10UNorm || format == IrBufferFormat::Format11_11_10UInt || format == IrBufferFormat::Format10_11_11Float ? IrBufferFormat::Format32UInt : format;
}

std::uint32_t DepthBitsTextureWidth(std::uint32_t word1, std::uint32_t word3) {
    const auto format = static_cast<IrBufferFormat>((word1 >> 20u) & 0x1ffu);
    const auto swizzle = (word3 >> 20u) & 0x1fu;
    const bool depthLayout = swizzle == 4u || swizzle == 8u || swizzle == 16u || swizzle == 20u || swizzle == 24u;
    if (!depthLayout) return 0u;
    if (format == IrBufferFormat::Format32UInt || format == IrBufferFormat::Format32SInt) return 32u;
    return format == IrBufferFormat::Format16UInt ? 16u : 0u;
}

bool IsDepthBitsTexture(std::uint32_t word1, std::uint32_t word3) {
    return DepthBitsTextureWidth(word1, word3) != 0u;
}

}
