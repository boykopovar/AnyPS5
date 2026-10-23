#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

VkFormat unsampledFormats[2]{};

void unsampledFormatProperties(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format != unsampledFormats[0] && format != unsampledFormats[1]) properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
}

std::uint32_t decodedWithout(VkFormat first = VK_FORMAT_UNDEFINED, VkFormat second = VK_FORMAT_UNDEFINED) {
    unsampledFormats[0] = first;
    unsampledFormats[1] = second;
    return SrgbDecodeFormats(unsampledFormatProperties, VK_NULL_HANDLE);
}

void srgbDecodeTests() {
    constexpr std::uint32_t srgb8 = 1u;
    constexpr std::uint32_t srgb8_8 = 2u;
    Require(decodedWithout() == 0u, "a device that samples every sRGB format needs no shader decode");
    Require(decodedWithout(VK_FORMAT_R8G8_SRGB) == srgb8_8, "a device without sampled R8G8_SRGB must decode 8_8_SRGB in the shader");
    Require(decodedWithout(VK_FORMAT_R8_SRGB) == srgb8, "a device without sampled R8_SRGB must decode 8_SRGB in the shader");
    Require(decodedWithout(VK_FORMAT_R8_SRGB, VK_FORMAT_R8G8_SRGB) == (srgb8 | srgb8_8), "a device without either 8-bit sRGB format must decode both in the shader");
    Require(decodedWithout(VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8_UNORM) == 0u, "8_8_SRGB without a sampled UNORM view must not be decoded in the shader");
    Require(decodedWithout(VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK) == 0u, "only the 8 and 8_8 sRGB formats may be decoded in the shader");
    Context context{};
    Require(SampledTextureFormat(context, 129) == VK_FORMAT_R8G8_SRGB && SampledTextureFormat(context, 128) == VK_FORMAT_R8_SRGB, "sRGB textures must keep their sRGB views without shader decode");
    context.srgbDecodeFormats = srgb8_8;
    Require(SampledTextureFormat(context, 129) == VK_FORMAT_R8G8_UNORM, "8_8_SRGB decoded in the shader must be viewed as R8G8_UNORM");
    Require(SampledTextureFormat(context, 128) == VK_FORMAT_R8_SRGB, "8_SRGB must keep its sRGB view when only 8_8_SRGB is decoded in the shader");
    context.srgbDecodeFormats = srgb8 | srgb8_8;
    Require(SampledTextureFormat(context, 128) == VK_FORMAT_R8_UNORM, "8_SRGB decoded in the shader must be viewed as R8_UNORM");
    Require(SampledTextureFormat(context, 130) == VK_FORMAT_R8G8B8A8_SRGB && SampledTextureFormat(context, 14) == VK_FORMAT_R8G8_UNORM && SampledTextureFormat(context, 170) == VK_FORMAT_BC1_RGBA_SRGB_BLOCK, "formats without a shader decode must keep their views");
}

void srgbTwinTests() {
    constexpr std::uint32_t srgb8 = 1u;
    constexpr std::uint32_t srgb8_8 = 2u;
    Context context{};
    Require(SrgbTwinFormat(context, VK_FORMAT_R8_UNORM) == VK_FORMAT_R8_SRGB && SrgbTwinFormat(context, VK_FORMAT_R8G8_UNORM) == VK_FORMAT_R8G8_SRGB, "a device that samples R8_SRGB and R8G8_SRGB must give R8 and R8G8 their sRGB twins");
    context.srgbDecodeFormats = srgb8_8;
    Require(SrgbTwinFormat(context, VK_FORMAT_R8G8_UNORM) == VK_FORMAT_UNDEFINED, "a device that decodes 8_8_SRGB in the shader must not give R8G8 an sRGB twin");
    Require(SrgbTwinFormat(context, VK_FORMAT_R8_UNORM) == VK_FORMAT_R8_SRGB, "R8 must keep its sRGB twin when only 8_8_SRGB is decoded in the shader");
    context.srgbDecodeFormats = srgb8 | srgb8_8;
    Require(SrgbTwinFormat(context, VK_FORMAT_R8_UNORM) == VK_FORMAT_UNDEFINED, "a device that decodes 8_SRGB in the shader must not give R8 an sRGB twin");
    Require(SrgbTwinFormat(context, VK_FORMAT_R8G8B8A8_UNORM) == VK_FORMAT_R8G8B8A8_SRGB && SrgbTwinFormat(context, VK_FORMAT_BC7_UNORM_BLOCK) == VK_FORMAT_BC7_SRGB_BLOCK, "formats without a shader decode must keep their sRGB twins");
    Require(SrgbTwinFormat(context, VK_FORMAT_R32_SFLOAT) == VK_FORMAT_UNDEFINED, "formats without an sRGB counterpart have no twin");
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected format test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture format rejection: ") + std::string(reason));
}

int channelLowBit(VkFormat format, VkComponentSwizzle channel) {
    const auto index = static_cast<std::size_t>(channel - VK_COMPONENT_SWIZZLE_R);
    switch (format) {
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return std::array{11, 5, 0, -1}[index];
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return std::array{10, 5, 0, 15}[index];
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return std::array{4, 8, 12, 0}[index];
        default: throw std::runtime_error("no channel layout for packed format " + std::to_string(format));
    }
}

void packedChannelTests() {
    constexpr std::array<std::pair<std::uint32_t, std::array<int, 4>>, 3> packed{{{133u, {0, 5, 11, 0}}, {134u, {0, 5, 10, 15}}, {136u, {0, 4, 8, 12}}}};
    constexpr std::array components{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    for (const auto& [format, lowBits] : packed) {
        for (std::size_t component = 0; component < components.size(); ++component) {
            const auto channel = TextureComponentChannel(format, components[component]);
            Require(channelLowBit(ResolveTextureFormat(format), channel) == lowBits[component], "format " + std::to_string(format) + " component " + std::to_string(component) + " is not read from bit " + std::to_string(lowBits[component]));
        }
    }
    Require(TextureComponentChannel(133, VK_COMPONENT_SWIZZLE_ONE) == VK_COMPONENT_SWIZZLE_ONE && TextureComponentChannel(56, VK_COMPONENT_SWIZZLE_R) == VK_COMPONENT_SWIZZLE_R, "constant selectors and other formats must keep their channels");
}

float unsignedSmallFloat(std::uint32_t bits, std::uint32_t mantissaBits) {
    const auto exponent = bits >> mantissaBits;
    const auto mantissa = bits & ((1u << mantissaBits) - 1u);
    const auto scale = static_cast<float>(1u << mantissaBits);
    if (exponent == 31u) return mantissa == 0u ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
    if (exponent == 0u) return std::ldexp(static_cast<float>(mantissa) / scale, -14);
    return std::ldexp(1.0f + static_cast<float>(mantissa) / scale, static_cast<int>(exponent) - 15);
}

bool sameValue(float left, float right) {
    return (std::isnan(left) && std::isnan(right)) || left == right;
}

void repackedFormatTests() {
    constexpr std::uint32_t guest = 43u;
    Require(IsRepackedTextureFormat(guest) && !IsRepackedTextureFormat(36u) && !IsRepackedTextureFormat(20u), "only texture format 43 is sampled through a repacked copy");
    Context context{};
    Require(SampledTextureFormat(context, guest) == VK_FORMAT_B10G11R11_UFLOAT_PACK32 && ResolveTextureFormat(guest) == VK_FORMAT_R32_UINT, "format 43 must be sampled as B10G11R11_UFLOAT and stored as R32_UINT");
    const auto field = [](std::uint32_t word, VkComponentSwizzle channel) -> std::pair<std::uint32_t, std::uint32_t> {
        switch (channel) {
            case VK_COMPONENT_SWIZZLE_R: return {word & 0x7ffu, 6u};
            case VK_COMPONENT_SWIZZLE_G: return {(word >> 11u) & 0x7ffu, 6u};
            case VK_COMPONENT_SWIZZLE_B: return {word >> 22u, 5u};
            default: throw std::runtime_error("B10G11R11 has no such channel");
        }
    };
    const auto check = [&](std::uint32_t red, std::uint32_t green, std::uint32_t blue) {
        const auto word = red | (green << 10u) | (blue << 21u);
        const auto repacked = RepackTextureWord(word);
        const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> expected{{{red, 5u}, {green, 6u}, {blue, 6u}}};
        constexpr std::array components{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B};
        for (std::size_t component = 0; component < components.size(); ++component) {
            const auto [bits, mantissaBits] = field(repacked, TextureComponentChannel(guest, components[component]));
            Require(bits == expected[component].first && mantissaBits == expected[component].second && sameValue(unsignedSmallFloat(bits, mantissaBits), unsignedSmallFloat(expected[component].first, expected[component].second)), "the repacked word " + std::to_string(repacked) + " does not read guest component " + std::to_string(component) + " of " + std::to_string(word));
        }
    };
    for (std::uint32_t red = 0; red < 1024u; ++red) check(red, 0x5a5u, 0x2c3u);
    for (std::uint32_t value = 0; value < 2048u; ++value) {
        check(0x155u, value, 0x7c0u);
        check(0x3e0u, 0x001u, value);
    }
    Require(std::isinf(unsignedSmallFloat(0x3e0u, 5u)) && std::isnan(unsignedSmallFloat(0x3e1u, 5u)) && unsignedSmallFloat(0x001u, 6u) == std::ldexp(1.0f, -20), "the reference decode misreads Inf, NaN or a denormal");
    Require(TextureComponentChannel(guest, VK_COMPONENT_SWIZZLE_A) == VK_COMPONENT_SWIZZLE_A && TextureComponentChannel(guest, VK_COMPONENT_SWIZZLE_ZERO) == VK_COMPONENT_SWIZZLE_ZERO, "format 43 keeps alpha and constant selectors");
}

void gcnBufferFormatTests() {
    using ShaderRecompiler::IrBufferFormat;
    const auto& formats = ShaderRecompiler::GcnBufferFormats();
    const auto gcn = [&](std::uint32_t data, std::uint32_t number) { return static_cast<IrBufferFormat>(formats[(data << 3u) | number]); };
    std::array<std::uint32_t, 78> seen{};
    for (const auto format : formats) ++seen[format];
    for (std::uint32_t format = 1; format < seen.size(); ++format) Require(seen[format] == 1u, "unified buffer format " + std::to_string(format) + " must come from exactly one GCN format pair");
    Require(gcn(0u, 4u) == IrBufferFormat::Invalid && gcn(15u, 7u) == IrBufferFormat::Invalid, "GCN DATA_FORMAT 0 and 15 have no buffer form");
    Require(gcn(1u, 7u) == IrBufferFormat::Invalid && gcn(4u, 0u) == IrBufferFormat::Invalid, "8-bit float and 32-bit UNORM have no buffer form");
    Require(gcn(1u, 0u) == IrBufferFormat::Format8UNorm && gcn(2u, 7u) == IrBufferFormat::Format16Float, "8 and 16-bit pairs must map");
    Require(gcn(4u, 4u) == IrBufferFormat::Format32UInt && gcn(4u, 7u) == IrBufferFormat::Format32Float, "32-bit pairs must map");
    Require(gcn(6u, 7u) == IrBufferFormat::Format11_11_10Float && gcn(7u, 7u) == IrBufferFormat::Format10_11_11Float, "GCN 10_11_11 and 11_11_10 must map by bit layout");
    Require(gcn(8u, 0u) == IrBufferFormat::Format2_10_10_10UNorm && gcn(9u, 0u) == IrBufferFormat::Format10_10_10_2UNorm, "GCN 10_10_10_2 and 2_10_10_10 must map by bit layout");
    Require(gcn(10u, 0u) == IrBufferFormat::Format8_8_8_8UNorm && gcn(14u, 7u) == IrBufferFormat::Format32_32_32_32Float, "four-channel pairs must map");
    const std::uint32_t word3 = 0x00000facu | (7u << 12u) | (14u << 15u) | (7u << 27u);
    Require(ShaderRecompiler::GcnBufferWord3(16u << 16u, word3) == (0x00000facu | (77u << 12u) | (1u << 28u)), "a strided GCN V# must check its index range");
    Require(ShaderRecompiler::GcnBufferWord3(0u, word3) == (0x00000facu | (77u << 12u) | (3u << 28u)), "a GCN V# without a stride must check its byte range");
}

void convertedDccClearTests() {
    alignas(64) std::array<std::uint8_t, 16> keys{};
    GuestTextureResource resource{};
    resource.dccAddress = reinterpret_cast<std::uint64_t>(keys.data());
    const auto read = [&](std::uint32_t format, std::uint8_t key) {
        keys.fill(key);
        resource.format = format;
        return TextureClearKeys(resource, keys.size() * 256u);
    };
    for (const std::uint32_t format : {30u, 34u}) {
        reject([&] { read(format, 0x40); }, "DCC clear code 0001 of converted texture format " + std::to_string(format));
        reject([&] { read(format, 0x80); }, "DCC clear code 1110 of converted texture format " + std::to_string(format));
        Require(read(format, 0x00) == DccKeys::Clear0000 && read(format, 0xc0) == DccKeys::Clear1111, "converted texture format " + std::to_string(format) + " must keep its 0000 and 1111 DCC clear codes");
    }
    Require(read(20, 0x40) == DccKeys::Clear0001 && read(20, 0x80) == DccKeys::Clear1110, "format 20 must keep its 0001 and 1110 DCC clear codes");
}

void singleDccKeysTests() {
    alignas(256) std::array<std::uint32_t, 256> texels{};
    for (std::size_t index = 0; index < texels.size(); ++index) texels[index] = 0x01000000u + static_cast<std::uint32_t>(index);
    alignas(64) std::array<std::uint8_t, sizeof(texels) / 256> keys{};
    keys.fill(0x10);
    GuestTextureResource resource{};
    resource.baseAddress = reinterpret_cast<std::uint64_t>(texels.data());
    resource.dccAddress = reinterpret_cast<std::uint64_t>(keys.data());
    const auto read = [&](std::uint32_t format) {
        resource.format = format;
        return TextureClearKeys(resource, sizeof(texels));
    };
    const auto* original = reinterpret_cast<const std::byte*>(texels.data());
    for (const std::uint32_t format : {1u, 56u, 77u}) {
        Require(read(format) == DccKeys::ClearSingle, "comp-to-single keys of format " + std::to_string(format) + " must read as single");
        const auto elementBytes = BytesPerElement(format);
        std::vector<std::byte> surface(sizeof(texels));
        ReadTextureSurface(resource, DccKeys::ClearSingle, surface);
        bool expanded = true;
        for (std::size_t offset = 0; offset < surface.size(); ++offset) expanded = expanded && surface[offset] == original[offset / 256 * 256 + offset % elementBytes];
        Require(expanded, "a read of format " + std::to_string(format) + " under comp-to-single keys must give every 256-byte block its first element");
    }
    Require(read(74) == DccKeys::Uncompressed, "comp-to-single keys of a format whose element does not divide 256 bytes must read as stored");
    keys[1] = 0xff;
    Require(read(56) == DccKeys::Uncompressed, "comp-to-single keys mixed with uncompressed ones must read as stored");
}

void packedFloatDccClearTests() {
    for (const bool alphaOnMsb : {false, true}) {
        for (const auto keys : {DccKeys::Clear0000, DccKeys::Clear0001, DccKeys::Clear1110, DccKeys::Clear1111}) {
            std::array<std::uint32_t, 4> texels{};
            texels.fill(0xdeadbeefu);
            Require(FillDccClear(VK_FORMAT_B10G11R11_UFLOAT_PACK32, keys, alphaOnMsb, std::as_writable_bytes(std::span(texels))), "packed float fast clear was not encoded");
            const std::uint32_t expected = keys == DccKeys::Clear1110 || keys == DccKeys::Clear1111 ? 0x781e03c0u : 0u;
            for (const auto texel : texels) Require(texel == expected, "packed float DCC clear did not encode RGB zero or one");
        }
    }
    alignas(64) std::array<std::uint8_t, 4> keys{0xc0, 0xc0, 0xc0, 0xc0};
    GuestTextureResource resource{};
    resource.format = 36;
    resource.dccAddress = reinterpret_cast<std::uint64_t>(keys.data());
    Require(TextureClearKeys(resource, 1024) == DccKeys::Clear1111, "packed float texture discarded its white fast-clear keys");
}

void fillContractTests() {
    std::array<std::uint32_t, 4> texels;
    texels.fill(0xdeadbeefu);
    auto span = std::as_writable_bytes(std::span(texels));
    Require(!FillDccClear(VK_FORMAT_R8G8B8A8_UNORM, DccKeys::ClearRegister, false, span), "a register clear encoded a texel");
    for (const auto texel : texels) Require(texel == 0u, "a declined clear left its span untouched");
    texels.fill(0xdeadbeefu);
    Require(!FillDccClear(VK_FORMAT_R8G8B8A8_UNORM, DccKeys::Uncompressed, false, span), "an uncompressed code encoded a texel");
    for (const auto texel : texels) Require(texel == 0u, "a declined clear left its span untouched");
    std::array<std::byte, 10> partial;
    partial.fill(std::byte{0x5a});
    Require(FillDccClear(VK_FORMAT_B10G11R11_UFLOAT_PACK32, DccKeys::Clear1111, false, partial), "packed float fast clear was not encoded");
    const std::array<std::byte, 4> white{std::byte{0xc0}, std::byte{0x03}, std::byte{0x1e}, std::byte{0x78}};
    for (std::size_t index = 0; index < partial.size(); ++index) Require(partial[index] == (index < 8u ? white[index % 4u] : std::byte{0}), "a clear fill over a partial trailing element did not zero the tail");
}

}

void RunTextureFormatTests() {
    srgbDecodeTests();
    srgbTwinTests();
    packedChannelTests();
    repackedFormatTests();
    Require(ResolveTextureFormat(1) == VK_FORMAT_R8_UNORM, "format 1 must resolve to R8_UNORM");
    Require(ResolveTextureFormat(2) == VK_FORMAT_R8_SNORM, "format 2 must resolve to R8_SNORM");
    Require(BytesPerElement(1) == 1u, "format 1 must be one byte wide");
    Require(!IsBlockCompressed(1), "format 1 must not be block compressed");
    Require(BlockWidth(1) == 1u && BlockHeight(1) == 1u, "format 1 must have a one-texel block");

    Require(ResolveTextureFormat(50) == VK_FORMAT_A2B10G10R10_UNORM_PACK32, "format 50 must resolve to A2B10G10R10_UNORM");
    Require(ResolveTextureFormat(51) == VK_FORMAT_A2B10G10R10_SNORM_PACK32, "format 51 must resolve to A2B10G10R10_SNORM");
    Require(ResolveTextureFormat(54) == VK_FORMAT_A2B10G10R10_UINT_PACK32, "format 54 must resolve to A2B10G10R10_UINT");
    Require(ResolveTextureFormat(55) == VK_FORMAT_A2B10G10R10_SINT_PACK32, "format 55 must resolve to A2B10G10R10_SINT");
    for (const std::uint32_t format : {50u, 51u, 54u, 55u}) Require(BytesPerElement(format) == 4u && !IsBlockCompressed(format), "packed 10-bit format " + std::to_string(format) + " must be four bytes and not block compressed");
    Require(ShaderRecompiler::SampledTextureNumericClass(ShaderRecompiler::IrBufferFormat::Format10_10_10_2SNorm) == ShaderRecompiler::IrTextureNumericClass::Float, "format 51 must sample as float");
    Require(ShaderRecompiler::SampledTextureNumericClass(ShaderRecompiler::IrBufferFormat::Format10_10_10_2SInt) == ShaderRecompiler::IrTextureNumericClass::Sint, "format 55 must sample as signed integer");
    Require(ShaderRecompiler::SampledTextureNumericClass(ShaderRecompiler::IrBufferFormat::Format10_10_10_2UInt) == ShaderRecompiler::IrTextureNumericClass::Uint, "format 54 must sample as unsigned integer");

    Require(ResolveTextureFormat(56) == VK_FORMAT_R8G8B8A8_UNORM, "format 56 must resolve to R8G8B8A8_UNORM");
    Require(BytesPerElement(56) == 4u, "format 56 must be four bytes wide");

    Require(ResolveTextureFormat(6) == VK_FORMAT_R8_SINT, "format 6 must resolve to R8_SINT");
    Require(BytesPerElement(6) == 1u, "format 6 must be one byte wide");

    Require(ResolveTextureFormat(22) == VK_FORMAT_R32_SFLOAT, "format 22 must resolve to R32_SFLOAT");
    Require(BytesPerElement(22) == 4u, "format 22 must be four bytes wide");

    Require(ResolveTextureFormat(77) == VK_FORMAT_R32G32B32A32_SFLOAT, "format 77 must resolve to R32G32B32A32_SFLOAT");
    Require(BytesPerElement(77) == 16u, "format 77 must be sixteen bytes wide");

    Require(ResolveTextureFormat(169) == VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "format 169 must resolve to BC1_RGBA_UNORM_BLOCK");
    Require(IsBlockCompressed(169), "format 169 must be block compressed");
    Require(BytesPerElement(169) == 8u, "BC1 blocks must be eight bytes");
    Require(BlockWidth(169) == 4u && BlockHeight(169) == 4u, "BC1 blocks must be four by four texels");

    Require(ResolveTextureFormat(181) == VK_FORMAT_BC7_UNORM_BLOCK, "format 181 must resolve to BC7_UNORM_BLOCK");
    Require(BytesPerElement(181) == 16u, "BC7 blocks must be sixteen bytes");

    Require(ResolveTextureFormat(34) == ResolveTextureFormat(20), "format 34 must remap to format 20");
    Require(BytesPerElement(34) == BytesPerElement(20), "remapped format 34 must share the width of format 20");
    Require(ResolveTextureFormat(30) == ResolveTextureFormat(20), "format 30 must remap to format 20");
    Require(BytesPerElement(30) == 4u, "remapped format 30 must be four bytes wide");
    for (std::uint32_t format = 0; format < 512u; ++format) {
        const auto remapped = static_cast<std::uint32_t>(ShaderRecompiler::RemapTextureFormat(static_cast<ShaderRecompiler::IrBufferFormat>(format)));
        if (remapped == format) continue;
        Require(ResolveTextureFormat(format) == ResolveTextureFormat(remapped), "guest format " + std::to_string(format) + " must resolve like the format the recompiler remaps it to");
    }

    reject([] { ResolveTextureFormat(0); }, "unsupported guest texture format");
    reject([] { ResolveTextureFormat(183); }, "unsupported guest texture format");
    reject([] { ResolveTextureFormat(9999); }, "unsupported guest texture format");
    reject([] { BytesPerElement(3); }, "unsupported guest texture format");
    reject([] { IsBlockCompressed(200); }, "unsupported guest texture format");
    for (const std::uint32_t format : {128u, 129u, 130u, 170u, 172u, 174u, 182u}) Require(IsSrgbTextureFormat(format), "guest format " + std::to_string(format) + " must be an sRGB texture format");
    for (const std::uint32_t format : {1u, 14u, 56u, 71u, 169u, 171u, 173u, 181u}) Require(!IsSrgbTextureFormat(format), "guest format " + std::to_string(format) + " must not be an sRGB texture format");
    for (const std::uint32_t format : {7u, 13u, 22u, 36u, 50u, 77u, 132u, 133u, 175u, 179u}) Require(IgnoresForcedDegamma(format), "guest format " + std::to_string(format) + " must sample the same through FORCE_DEGAMMA");
    for (const std::uint32_t format : {1u, 2u, 14u, 15u, 56u, 57u, 128u, 130u, 169u, 171u, 173u, 181u}) Require(!IgnoresForcedDegamma(format), "guest format " + std::to_string(format) + " must not sample the same through FORCE_DEGAMMA");
    convertedDccClearTests();
    singleDccKeysTests();
    packedFloatDccClearTests();
    fillContractTests();
    gcnBufferFormatTests();
}
