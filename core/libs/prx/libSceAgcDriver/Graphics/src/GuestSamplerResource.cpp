#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

VkFilter toVkFilter(std::uint32_t raw) {
    switch (raw) {
        case 0: case 2: return VK_FILTER_NEAREST;
        case 1: case 3: return VK_FILTER_LINEAR;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses an unknown filter " + std::to_string(raw));
    }
}

bool isAnisoFilter(std::uint32_t raw) {
    return raw == 2 || raw == 3;
}

VkSamplerAddressMode toVkAddressMode(std::uint32_t raw) {
    switch (raw) {
        case 0: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case 1: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case 2: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 3: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case 4: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case 5: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case 6: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case 7: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses an unknown clamp mode " + std::to_string(raw));
    }
}

float toSignedLodBias(std::uint32_t raw) {
    const auto extended = static_cast<std::int32_t>((raw ^ 0x2000u) - 0x2000u);
    return static_cast<float>(extended) / 256.0f;
}

}

GuestSamplerResource DecodeSamplerResource(std::span<const std::uint32_t> words) {
    Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");

    const auto clampX = (words[0] >> 0u) & 0x7u;
    const auto clampY = (words[0] >> 3u) & 0x7u;
    const auto clampZ = (words[0] >> 6u) & 0x7u;
    const auto maxAnisoRatio = (words[0] >> 9u) & 0x7u;
    const auto depthCompareFunc = (words[0] >> 12u) & 0x7u;
    const auto forceUnormCoords = ((words[0] >> 15u) & 0x1u) != 0;
    const auto anisoThreshold = (words[0] >> 16u) & 0x7u;
    const auto forceSrgb = ((words[0] >> 20u) & 0x1u) != 0;
    const auto anisoBias = (words[0] >> 21u) & 0x3fu;
    const auto truncCoord = ((words[0] >> 27u) & 0x1u) != 0;
    const auto disableCubeWrap = ((words[0] >> 28u) & 0x1u) != 0;
    const auto filterMode = (words[0] >> 29u) & 0x3u;
    const auto disableDegamma = ((words[0] >> 31u) & 0x1u) != 0;

    const auto minLodRaw = (words[1] >> 0u) & 0xfffu;
    const auto maxLodRaw = (words[1] >> 12u) & 0xfffu;
    const auto perfMip = (words[1] >> 24u) & 0xfu;
    const auto perfZ = (words[1] >> 28u) & 0xfu;

    const auto lodBiasRaw = (words[2] >> 0u) & 0x3fffu;
    const auto lodBiasSec = (words[2] >> 14u) & 0x3fu;
    const auto xyMagFilter = (words[2] >> 20u) & 0x3u;
    const auto xyMinFilter = (words[2] >> 22u) & 0x3u;
    const auto mipFilter = (words[2] >> 26u) & 0x3u;
    const auto pointPreclamp = ((words[2] >> 28u) & 0x1u) != 0;
    const auto anisoOverride = ((words[2] >> 29u) & 0x1u) != 0;
    const auto blendZeroPrt = ((words[2] >> 30u) & 0x1u) != 0;

    const auto borderColorType = (words[3] >> 30u) & 0x3u;

    Require(!forceUnormCoords, "guest sampler descriptor uses unnormalized coordinates which are not implemented");
    Require(!forceSrgb, "guest sampler descriptor forces sRGB decoding which is not implemented");
    // TRUNC_COORD picks point-sampled texels by truncation instead of rounding, and the perf fields
    // trade mip/depth precision for speed; Vulkan's nearest filtering already floors, so these only
    // move texel selection by half a texel at most and are accepted as is. ANISO_THRESHOLD and
    // ANISO_BIAS only tune how many anisotropic taps the hardware takes; the host filter picks its own.
    static_cast<void>(truncCoord);
    static_cast<void>(anisoThreshold);
    static_cast<void>(anisoBias);
    Require(!disableCubeWrap, "guest sampler descriptor disables seamless cube filtering which is not implemented");
    Require(filterMode == 0, "guest sampler descriptor uses a reduction filter mode which is not implemented");
    Require(!disableDegamma, "guest sampler descriptor disables degamma which is not implemented");
    Require(lodBiasSec == 0, "guest sampler descriptor uses a secondary LOD bias which is not implemented");
    Require(!pointPreclamp, "guest sampler descriptor uses point preclamping which is not implemented");
    Require(!anisoOverride, "guest sampler descriptor uses an anisotropy override which is not implemented");
    Require(!blendZeroPrt, "guest sampler descriptor uses PRT blend-zero which is not implemented");
    Require(mipFilter <= 2u, "guest sampler descriptor uses an unknown mip filter " + std::to_string(mipFilter));

    const auto aniso = isAnisoFilter(xyMagFilter) || isAnisoFilter(xyMinFilter);
    auto anisoRatio = 1.0f;
    if (aniso) {
        Require(maxAnisoRatio <= 4u, "guest sampler descriptor uses an unknown anisotropy ratio " + std::to_string(maxAnisoRatio));
        const std::array ratios{1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
        anisoRatio = ratios[maxAnisoRatio];
    }

    auto minLod = 0.0f;
    auto maxLod = 0.0f;
    if (mipFilter != 0u) {
        Require(minLodRaw <= maxLodRaw, "guest sampler descriptor has a minimum LOD past its maximum LOD");
        minLod = static_cast<float>(minLodRaw) / 256.0f;
        maxLod = static_cast<float>(maxLodRaw) / 256.0f;
    }

    VkBorderColor border;
    switch (borderColorType) {
        case 0: border = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK; break;
        case 1: border = VK_BORDER_COLOR_INT_OPAQUE_BLACK; break;
        case 2: border = VK_BORDER_COLOR_INT_OPAQUE_WHITE; break;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses a border color table which is not implemented");
    }

    GuestSamplerResource result{};
    result.magFilter = toVkFilter(xyMagFilter);
    result.minFilter = toVkFilter(xyMinFilter);
    result.mipmapMode = mipFilter == 2u ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    result.addressModeU = toVkAddressMode(clampX);
    result.addressModeV = toVkAddressMode(clampY);
    result.addressModeW = toVkAddressMode(clampZ);
    result.anisotropyEnable = aniso;
    result.maxAnisotropy = anisoRatio;
    result.minLod = minLod;
    result.maxLod = maxLod;
    result.lodBias = toSignedLodBias(lodBiasRaw);
    result.borderColor = border;
    const std::array compareOps{VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS};
    result.compareOp = compareOps.at(depthCompareFunc);
    return result;
}

}
