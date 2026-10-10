#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

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

}

void RunTextureFormatTests() {
    srgbDecodeTests();
    Require(ResolveTextureFormat(1) == VK_FORMAT_R8_UNORM, "format 1 must resolve to R8_UNORM");
    Require(BytesPerElement(1) == 1u, "format 1 must be one byte wide");
    Require(!IsBlockCompressed(1), "format 1 must not be block compressed");
    Require(BlockWidth(1) == 1u && BlockHeight(1) == 1u, "format 1 must have a one-texel block");

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
    reject([] { BytesPerElement(2); }, "unsupported guest texture format");
    reject([] { IsBlockCompressed(200); }, "unsupported guest texture format");
    for (const std::uint32_t format : {128u, 129u, 130u, 170u, 172u, 174u, 182u}) Require(IsSrgbTextureFormat(format), "guest format " + std::to_string(format) + " must be an sRGB texture format");
    for (const std::uint32_t format : {1u, 14u, 56u, 71u, 169u, 171u, 173u, 181u}) Require(!IsSrgbTextureFormat(format), "guest format " + std::to_string(format) + " must not be an sRGB texture format");
    convertedDccClearTests();
}
