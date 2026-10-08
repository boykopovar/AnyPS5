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

VkSamplerReductionMode toVkReductionMode(std::uint32_t raw) {
    switch (raw) {
        case 0: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT;
        case 1: return VK_SAMPLER_REDUCTION_MODE_MIN_EXT;
        case 2: return VK_SAMPLER_REDUCTION_MODE_MAX_EXT;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses an unknown reduction filter mode " + std::to_string(raw));
    }
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

bool readsBorderColor(std::uint32_t raw) {
    return raw >= 4u && raw <= 7u;
}

float toSignedLodBias(std::uint32_t raw) {
    const auto extended = static_cast<std::int32_t>((raw ^ 0x2000u) - 0x2000u);
    return static_cast<float>(extended) / 256.0f;
}

}

GuestSamplerResource DecodeSamplerResource(std::span<const std::uint32_t> words, bool unnormalizedProven, bool forceDegammaPaired) {
    Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");

    const auto clampX = (words[0] >> 0u) & 0x7u;
    const auto clampY = (words[0] >> 3u) & 0x7u;
    const auto clampZ = (words[0] >> 6u) & 0x7u;
    const auto maxAnisoRatio = (words[0] >> 9u) & 0x7u;
    const auto depthCompareFunc = (words[0] >> 12u) & 0x7u;
    const auto forceUnormCoords = ((words[0] >> 15u) & 0x1u) != 0;
    const auto anisoThreshold = (words[0] >> 16u) & 0x7u;
    const auto mcCoordTrunc = ((words[0] >> 19u) & 0x1u) != 0;
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

    Require(!forceUnormCoords || unnormalizedProven, "guest sampler descriptor uses unnormalized coordinates which are not implemented");
    if (forceUnormCoords) {
        Require(xyMagFilter == xyMinFilter, "guest sampler descriptor uses unnormalized coordinates with different minification and magnification filters, which is not implemented");
        Require(!isAnisoFilter(xyMagFilter), "guest sampler descriptor uses unnormalized coordinates with anisotropic filtering, which is not implemented");
        Require(clampX == 2u || clampX == 6u, "guest sampler descriptor uses unnormalized coordinates with clamp mode " + std::to_string(clampX) + " on X; only clamp-to-last-texel and clamp-to-border are implemented");
        Require(clampY == 2u || clampY == 6u, "guest sampler descriptor uses unnormalized coordinates with clamp mode " + std::to_string(clampY) + " on Y; only clamp-to-last-texel and clamp-to-border are implemented");
        Require(!truncCoord, "guest sampler descriptor uses unnormalized coordinates with TRUNC_COORD, which is not implemented");
        Require(!mcCoordTrunc, "guest sampler descriptor uses unnormalized coordinates with MC_COORD_TRUNC, which is not implemented");
    } else {
        Require(!unnormalizedProven, "guest sampler descriptor is bound as unnormalized without FORCE_UNNORMALIZED");
    }
    Require(!forceSrgb || forceDegammaPaired, "guest sampler descriptor forces sRGB decoding which is not implemented");
    // TRUNC_COORD picks point-sampled texels by truncation instead of rounding, and the perf fields
    // trade mip/depth precision for speed; Vulkan's nearest filtering already floors, so these only
    // move texel selection by half a texel at most and are accepted as is. ANISO_THRESHOLD and
    // ANISO_BIAS only tune how many anisotropic taps the hardware takes; the host filter picks its own.
    static_cast<void>(truncCoord);
    static_cast<void>(anisoThreshold);
    static_cast<void>(anisoBias);
    const auto reductionMode = toVkReductionMode(filterMode);
    Require(!disableDegamma, "guest sampler descriptor disables degamma which is not implemented");
    Require(lodBiasSec == 0, "guest sampler descriptor uses a secondary LOD bias which is not implemented");
    Require(!pointPreclamp, "guest sampler descriptor uses point preclamping which is not implemented");
    static_cast<void>(anisoOverride);
    Require(!blendZeroPrt, "guest sampler descriptor uses PRT blend-zero which is not implemented");
    if (mipFilter > 2u) Require(false, "guest sampler descriptor uses an unknown mip filter " + std::to_string(mipFilter));
    Require(mipFilter != 2u || reductionMode == VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT, "guest sampler descriptor combines a min or max reduction with a linear mip filter, which is not implemented");

    const auto aniso = isAnisoFilter(xyMagFilter) || isAnisoFilter(xyMinFilter);
    Require(!aniso || reductionMode == VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT, "guest sampler descriptor combines a min or max reduction with anisotropic filtering, which is not implemented");
    auto anisoRatio = 1.0f;
    if (aniso) {
        if (maxAnisoRatio > 4u) Require(false, "guest sampler descriptor uses an unknown anisotropy ratio " + std::to_string(maxAnisoRatio));
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
        default:
            if (readsBorderColor(clampX) || readsBorderColor(clampY) || readsBorderColor(clampZ)) throw std::runtime_error("AGC graphics: guest sampler descriptor uses a border color table which is not implemented");
            border = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
            break;
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
    result.reductionMode = reductionMode;
    const std::array compareOps{VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS};
    result.compareOp = compareOps.at(depthCompareFunc);
    result.forceDegamma = forceSrgb;
    result.nonSeamlessCube = disableCubeWrap;
    if (forceUnormCoords) {
        result.unnormalizedCoordinates = true;
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        result.minLod = 0.0f;
        result.maxLod = 0.0f;
        result.lodBias = 0.0f;
        result.anisotropyEnable = false;
        result.maxAnisotropy = 1.0f;
    }
    return result;
}

std::optional<std::array<std::uint32_t, 4>> SingleLevelSamplerWords(std::span<const std::uint32_t, 4> words, bool singleLevelImage, bool mipmappedImage) {
    const auto filters = words[2];
    if (((filters >> 29u) & 1u) == 0 || !singleLevelImage || (!isAnisoFilter((filters >> 20u) & 3u) && !isAnisoFilter((filters >> 22u) & 3u))) return std::nullopt;
    Require(!mipmappedImage, "guest sampler with ANISO_OVERRIDE is paired with both single-level and mipmapped images in one draw, which is not implemented");
    return std::array<std::uint32_t, 4>{words[0], words[1], filters & ~((2u << 20u) | (2u << 22u)), words[3]};
}

}
