#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <utility>

namespace ShaderRecompiler {

namespace {

struct FormatInfo {
    IrBufferFormat format;
    bool sampled;
    bool uint32;
    bool sint32;
};

constexpr std::array<FormatInfo, 82> kFormatInfoTable {{
    {IrBufferFormat::Format8UNorm, true, false, false},
    {IrBufferFormat::Format8SNorm, true, false, false},
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
    {IrBufferFormat::Format11_11_10Float, true, true, false},
    {IrBufferFormat::Format10_11_11Float, true, true, false},
    {IrBufferFormat::Format10_10_10_2UNorm, true, false, false},
    {IrBufferFormat::Format10_10_10_2SNorm, true, false, false},
    {IrBufferFormat::Format10_10_10_2UInt, true, true, false},
    {IrBufferFormat::Format10_10_10_2SInt, true, false, true},
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

bool IsDefinedImageFormat(IrBufferFormat format) {
    const auto value = static_cast<std::uint32_t>(format);
    return std::ranges::any_of(DefinedImageFormats, [&](const ImageFormatRange& range) { return value >= range.first && value <= range.last; });
}

bool IsFmaskTextureFormat(IrBufferFormat format) {
    return format >= IrBufferFormat::Fmask8_S2_F1 && format <= IrBufferFormat::Fmask64_S16_F8;
}

std::optional<FmaskLayout> FmaskLayoutOf(IrBufferFormat format) {
    switch (format) {
        case IrBufferFormat::Fmask8_S2_F1: return FmaskLayout{2u, 1u, 1u, 8u};
        case IrBufferFormat::Fmask8_S4_F1: return FmaskLayout{4u, 1u, 1u, 8u};
        case IrBufferFormat::Fmask8_S8_F1: return FmaskLayout{8u, 1u, 1u, 8u};
        case IrBufferFormat::Fmask8_S2_F2: return FmaskLayout{2u, 2u, 1u, 8u};
        case IrBufferFormat::Fmask8_S4_F2: return FmaskLayout{4u, 2u, 2u, 8u};
        case IrBufferFormat::Fmask8_S4_F4: return FmaskLayout{4u, 4u, 2u, 8u};
        case IrBufferFormat::Fmask16_S8_F2: return FmaskLayout{8u, 2u, 2u, 16u};
        case IrBufferFormat::Fmask32_S8_F4: return FmaskLayout{8u, 4u, 4u, 32u};
        case IrBufferFormat::Fmask32_S8_F8: return FmaskLayout{8u, 8u, 4u, 32u};
        default: return std::nullopt;
    }
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
    return format == IrBufferFormat::Format11_11_10UNorm || format == IrBufferFormat::Format11_11_10UInt || format == IrBufferFormat::Format11_11_10Float || format == IrBufferFormat::Format10_11_11Float ? IrBufferFormat::Format32UInt : format;
}

IrBufferFormat RemapStorageTextureFormat(IrBufferFormat format) {
    switch (format) {
        case IrBufferFormat::Format5_6_5UNorm:
        case IrBufferFormat::Format5_5_5_1UNorm:
        case IrBufferFormat::Format4_4_4_4UNorm:
            return IrBufferFormat::Format16UInt;
        default:
            return RemapTextureFormat(format);
    }
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

const std::array<std::uint8_t, 128>& GcnBufferFormats() {
    static const auto table = [] {
        std::array<std::uint8_t, 128> formats{};
        struct Family { std::uint32_t data; std::uint32_t first; bool normalized; bool floating; };
        constexpr std::array<Family, 10> normalizedFamilies{{
            {1u, 1u, true, false}, {2u, 7u, true, true}, {3u, 14u, true, false}, {5u, 23u, true, true}, {6u, 30u, true, true},
            {7u, 37u, true, true}, {8u, 44u, true, false}, {9u, 50u, true, false}, {10u, 56u, true, false}, {12u, 65u, true, true},
        }};
        for (const auto& family : normalizedFamilies) {
            for (std::uint32_t number = 0; number < 6u; ++number) formats[(family.data << 3u) | number] = static_cast<std::uint8_t>(family.first + number);
            if (family.floating) formats[(family.data << 3u) | 7u] = static_cast<std::uint8_t>(family.first + 6u);
        }
        constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> wideFamilies{{{4u, 20u}, {11u, 62u}, {13u, 72u}, {14u, 75u}}};
        for (const auto& [data, first] : wideFamilies) {
            formats[(data << 3u) | 4u] = static_cast<std::uint8_t>(first);
            formats[(data << 3u) | 5u] = static_cast<std::uint8_t>(first + 1u);
            formats[(data << 3u) | 7u] = static_cast<std::uint8_t>(first + 2u);
        }
        return formats;
    }();
    return table;
}

std::uint32_t GcnImageFormat(std::uint32_t dataFormat, std::uint32_t numberFormat) {
    constexpr std::uint32_t Unorm = 0u, Snorm = 1u, Float = 7u, Srgb = 9u;
    if (dataFormat < 16u && numberFormat < 8u) return GcnBufferFormats()[(dataFormat << 3u) | numberFormat];
    switch (dataFormat) {
        case 1u: return numberFormat == Srgb ? 128u : 0u;
        case 3u: return numberFormat == Srgb ? 129u : 0u;
        case 10u: return numberFormat == Srgb ? 130u : 0u;
        case 16u: return numberFormat == Unorm ? 133u : 0u;
        case 17u: return numberFormat == Unorm ? 134u : 0u;
        case 18u: return numberFormat == Unorm ? 135u : 0u;
        case 19u: return numberFormat == Unorm ? 136u : 0u;
        case 34u: return numberFormat == Float ? 132u : 0u;
        case 35u: return numberFormat == Unorm ? 169u : numberFormat == Srgb ? 170u : 0u;
        case 36u: return numberFormat == Unorm ? 171u : numberFormat == Srgb ? 172u : 0u;
        case 37u: return numberFormat == Unorm ? 173u : numberFormat == Srgb ? 174u : 0u;
        case 38u: return numberFormat == Unorm ? 175u : numberFormat == Snorm ? 176u : 0u;
        case 39u: return numberFormat == Unorm ? 177u : numberFormat == Snorm ? 178u : 0u;
        case 40u: return numberFormat == Unorm ? 179u : numberFormat == Snorm ? 180u : 0u;
        case 41u: return numberFormat == Unorm ? 181u : numberFormat == Srgb ? 182u : 0u;
        default: return 0u;
    }
}

std::array<std::uint32_t, 8> GcnImageWords(std::span<const std::uint32_t, 8> gcn) {
    const auto format = GcnImageFormat((gcn[1] >> 20u) & 0x3fu, (gcn[1] >> 26u) & 0xfu);
    const auto width = gcn[2] & 0x3fffu;
    const auto height = (gcn[2] >> 14u) & 0x3fffu;
    const auto type = gcn[3] >> 28u;
    const auto lastLevel = (gcn[3] >> 16u) & 0xfu;
    const bool layered = type == 11u || type == 12u || type == 13u || type == 15u;
    std::array<std::uint32_t, 8> words{};
    words[0] = gcn[0];
    words[1] = (gcn[1] & 0x000fffffu) | (format << 20u) | ((width & 3u) << 30u);
    words[2] = (width >> 2u) | (height << 14u);
    words[3] = (gcn[3] & 0x000fffffu) | (type << 28u);
    words[4] = (layered ? (gcn[5] >> 13u) & 0x1fffu : gcn[4] & 0x1fffu) | ((gcn[5] & 0x1fffu) << 16u);
    words[5] = lastLevel << 4u;
    words[7] = ((gcn[3] >> 20u) & 0x1fu) | (((gcn[3] >> 25u) & 1u) << 5u) | (((gcn[4] >> 13u) & 0x3fffu) << 6u);
    return words;
}

std::uint32_t GcnBufferWord3(std::uint32_t word1, std::uint32_t word3) {
    const std::uint32_t format = GcnBufferFormats()[(word3 >> 12u) & 0x7fu];
    const std::uint32_t outOfBounds = ((word1 >> 16u) & 0x3fffu) != 0u ? 1u : 3u;
    return (word3 & 0xc0e00fffu) | (format << 12u) | (outOfBounds << 28u);
}

IrTextureNumericClass VertexInputNumericClass(IrBufferFormat format) {
    switch (format) {
    case IrBufferFormat::Format8UInt:
    case IrBufferFormat::Format16UInt:
    case IrBufferFormat::Format8_8UInt:
    case IrBufferFormat::Format32UInt:
    case IrBufferFormat::Format16_16UInt:
    case IrBufferFormat::Format10_10_10_2UInt:
    case IrBufferFormat::Format8_8_8_8UInt:
    case IrBufferFormat::Format32_32UInt:
    case IrBufferFormat::Format16_16_16_16UInt:
    case IrBufferFormat::Format32_32_32UInt:
    case IrBufferFormat::Format32_32_32_32UInt: return IrTextureNumericClass::Uint;
    case IrBufferFormat::Format8SInt:
    case IrBufferFormat::Format16SInt:
    case IrBufferFormat::Format8_8SInt:
    case IrBufferFormat::Format32SInt:
    case IrBufferFormat::Format16_16SInt:
    case IrBufferFormat::Format10_10_10_2SInt:
    case IrBufferFormat::Format8_8_8_8SInt:
    case IrBufferFormat::Format32_32SInt:
    case IrBufferFormat::Format16_16_16_16SInt:
    case IrBufferFormat::Format32_32_32SInt:
    case IrBufferFormat::Format32_32_32_32SInt: return IrTextureNumericClass::Sint;
    default: return IrTextureNumericClass::Float;
    }
}

}
