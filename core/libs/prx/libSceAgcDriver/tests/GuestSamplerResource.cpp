#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

struct Fields {
    std::uint32_t clampX = 2;
    std::uint32_t clampY = 2;
    std::uint32_t clampZ = 2;
    std::uint32_t maxAnisoRatio = 0;
    std::uint32_t depthCompareFunc = 0;
    bool forceUnormCoords = false;
    std::uint32_t anisoThreshold = 0;
    bool forceSrgb = false;
    std::uint32_t anisoBias = 0;
    bool truncCoord = false;
    bool disableCubeWrap = false;
    std::uint32_t filterMode = 0;
    bool disableDegamma = false;
    std::uint32_t minLodRaw = 0;
    std::uint32_t maxLodRaw = 0x3c00;
    std::uint32_t perfMip = 0;
    std::uint32_t perfZ = 0;
    std::uint32_t lodBiasRaw = 0;
    std::uint32_t lodBiasSec = 0;
    std::uint32_t xyMagFilter = 1;
    std::uint32_t xyMinFilter = 1;
    std::uint32_t zFilter = 1;
    std::uint32_t mipFilter = 2;
    bool pointPreclamp = false;
    bool anisoOverride = false;
    bool blendZeroPrt = false;
    std::uint32_t borderColorType = 0;
};

std::array<std::uint32_t, 4> pack(const Fields& f) {
    std::array<std::uint32_t, 4> words{};
    words[0] = (f.clampX & 0x7u) | ((f.clampY & 0x7u) << 3u) | ((f.clampZ & 0x7u) << 6u) | ((f.maxAnisoRatio & 0x7u) << 9u)
        | ((f.depthCompareFunc & 0x7u) << 12u) | ((f.forceUnormCoords ? 1u : 0u) << 15u) | ((f.anisoThreshold & 0x7u) << 16u)
        | ((f.forceSrgb ? 1u : 0u) << 20u) | ((f.anisoBias & 0x3fu) << 21u) | ((f.truncCoord ? 1u : 0u) << 27u)
        | ((f.disableCubeWrap ? 1u : 0u) << 28u) | ((f.filterMode & 0x3u) << 29u) | ((f.disableDegamma ? 1u : 0u) << 31u);
    words[1] = (f.minLodRaw & 0xfffu) | ((f.maxLodRaw & 0xfffu) << 12u) | ((f.perfMip & 0xfu) << 24u) | ((f.perfZ & 0xfu) << 28u);
    words[2] = (f.lodBiasRaw & 0x3fffu) | ((f.lodBiasSec & 0x3fu) << 14u) | ((f.xyMagFilter & 0x3u) << 20u)
        | ((f.xyMinFilter & 0x3u) << 22u) | ((f.zFilter & 0x3u) << 24u) | ((f.mipFilter & 0x3u) << 26u)
        | ((f.pointPreclamp ? 1u : 0u) << 28u) | ((f.anisoOverride ? 1u : 0u) << 29u) | ((f.blendZeroPrt ? 1u : 0u) << 30u);
    words[3] = (f.borderColorType & 0x3u) << 30u;
    return words;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected guest sampler test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected guest sampler rejection: ") + std::string(reason));
}

void rejectFields(const Fields& f, std::string_view reason) {
    const auto words = pack(f);
    reject([&] { DecodeSamplerResource(words); }, reason);
}

bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

}

void RunGuestSamplerResourceTests() {
    Fields base;
    auto result = DecodeSamplerResource(pack(base));
    Require(result.magFilter == VK_FILTER_LINEAR && result.minFilter == VK_FILTER_LINEAR, "linear filter fields decoded incorrectly");
    Require(result.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR, "linear mip filter must decode to linear mipmap mode");
    Require(result.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, "clamp mode 2 must decode to clamp-to-edge");
    Require(!result.anisotropyEnable && result.maxAnisotropy == 1.0f, "non-anisotropic filter must leave anisotropy disabled");
    Require(nearlyEqual(result.maxLod, static_cast<float>(0x3c00) / 256.0f), "max LOD decoded incorrectly");
    Require(result.borderColor == VK_BORDER_COLOR_INT_TRANSPARENT_BLACK, "border color type 0 must decode to transparent black");

    Fields nearest = base;
    nearest.xyMagFilter = 0;
    nearest.xyMinFilter = 0;
    nearest.zFilter = 0;
    nearest.mipFilter = 0;
    result = DecodeSamplerResource(pack(nearest));
    Require(result.magFilter == VK_FILTER_NEAREST && result.minFilter == VK_FILTER_NEAREST, "filter 0 must decode to nearest");
    Require(result.mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST, "mip filter 0 must decode to nearest mipmap mode");
    Require(result.minLod == 0.0f && result.maxLod == 0.0f, "disabled mip filtering must clear LOD range");

    Fields addressModes = base;
    addressModes.clampX = 0;
    addressModes.clampY = 1;
    addressModes.clampZ = 4;
    result = DecodeSamplerResource(pack(addressModes));
    Require(result.addressModeU == VK_SAMPLER_ADDRESS_MODE_REPEAT, "clamp mode 0 must decode to repeat");
    Require(result.addressModeV == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, "clamp mode 1 must decode to mirrored repeat");
    Require(result.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, "clamp mode 4 must decode to clamp-to-border");

    Fields aniso = base;
    aniso.xyMagFilter = 2;
    aniso.xyMinFilter = 2;
    aniso.zFilter = 2;
    aniso.maxAnisoRatio = 3;
    result = DecodeSamplerResource(pack(aniso));
    Require(result.anisotropyEnable, "anisotropic filter must enable anisotropy");
    Require(nearlyEqual(result.maxAnisotropy, 8.0f), "anisotropy ratio 3 must decode to 8x");
    aniso.maxAnisoRatio = 5;
    rejectFields(aniso, "unknown anisotropy ratio");

    Fields badLod = base;
    badLod.minLodRaw = 100;
    badLod.maxLodRaw = 50;
    rejectFields(badLod, "minimum LOD past its maximum LOD");

    const std::array<std::uint32_t, 4> capturedSampler{0u, 0x00fff000u, 0x09000000u, 0u};
    const auto captured = DecodeSamplerResource(capturedSampler);
    Require(captured.magFilter == VK_FILTER_NEAREST && captured.minFilter == VK_FILTER_NEAREST, "captured 2D sampler filters decoded incorrectly");
    Require(captured.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR && nearlyEqual(captured.maxLod, 4095.0f / 256.0f), "captured 2D sampler mip settings decoded incorrectly");
    Require(captured.addressModeU == VK_SAMPLER_ADDRESS_MODE_REPEAT && captured.addressModeV == VK_SAMPLER_ADDRESS_MODE_REPEAT, "captured 2D sampler address modes decoded incorrectly");
    for (std::uint32_t zFilter = 0; zFilter < 4; ++zFilter) {
        Fields twoDimensional = base;
        twoDimensional.zFilter = zFilter;
        const auto decoded = DecodeSamplerResource(pack(twoDimensional));
        Require(decoded.magFilter == VK_FILTER_LINEAR && decoded.minFilter == VK_FILTER_LINEAR, "Z filter changed 2D filtering");
    }

    Fields badMipFilter = base;
    badMipFilter.mipFilter = 3;
    rejectFields(badMipFilter, "unknown mip filter");

    const std::array compareOps{VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS};
    for (std::uint32_t function = 0; function < compareOps.size(); ++function) {
        Fields comparison = base;
        comparison.depthCompareFunc = function;
        const auto decoded = DecodeSamplerResource(pack(comparison));
        Require(decoded.compareOp == compareOps.at(function), "sampler depth comparison function decoded incorrectly");
        Require(!decoded.compareEnable, "sampler descriptor enabled comparison without shader metadata");
    }

    Fields badUnorm = base;
    badUnorm.forceUnormCoords = true;
    rejectFields(badUnorm, "unnormalized coordinates");

    Fields badSrgb = base;
    badSrgb.forceSrgb = true;
    rejectFields(badSrgb, "forces sRGB decoding");

    Fields truncated = base;
    truncated.truncCoord = true;
    truncated.perfMip = 1;
    truncated.perfZ = 1;
    truncated.anisoThreshold = 1;
    truncated.anisoBias = 1;
    static_cast<void>(DecodeSamplerResource(pack(truncated)));

    Fields badCubeWrap = base;
    badCubeWrap.disableCubeWrap = true;
    rejectFields(badCubeWrap, "seamless cube filtering");

    Fields badFilterMode = base;
    badFilterMode.filterMode = 1;
    rejectFields(badFilterMode, "reduction filter mode");

    Fields badDegamma = base;
    badDegamma.disableDegamma = true;
    rejectFields(badDegamma, "disables degamma");

    Fields badLodBiasSec = base;
    badLodBiasSec.lodBiasSec = 1;
    rejectFields(badLodBiasSec, "secondary LOD bias");

    Fields badPreclamp = base;
    badPreclamp.pointPreclamp = true;
    rejectFields(badPreclamp, "point preclamping");

    Fields badAnisoOverride = base;
    badAnisoOverride.anisoOverride = true;
    rejectFields(badAnisoOverride, "anisotropy override");

    Fields badBlendZero = base;
    badBlendZero.blendZeroPrt = true;
    rejectFields(badBlendZero, "PRT blend-zero");

    Fields badBorder = base;
    badBorder.borderColorType = 3;
    rejectFields(badBorder, "border color table");

    Fields opaqueBlack = base;
    opaqueBlack.borderColorType = 1;
    Require(DecodeSamplerResource(pack(opaqueBlack)).borderColor == VK_BORDER_COLOR_INT_OPAQUE_BLACK, "border color type 1 must decode to opaque black");
    Fields opaqueWhite = base;
    opaqueWhite.borderColorType = 2;
    Require(DecodeSamplerResource(pack(opaqueWhite)).borderColor == VK_BORDER_COLOR_INT_OPAQUE_WHITE, "border color type 2 must decode to opaque white");

    Fields positiveBias = base;
    positiveBias.lodBiasRaw = 256;
    Require(nearlyEqual(DecodeSamplerResource(pack(positiveBias)).lodBias, 1.0f), "positive LOD bias decoded incorrectly");
    Fields negativeBias = base;
    negativeBias.lodBiasRaw = static_cast<std::uint32_t>(-256) & 0x3fffu;
    Require(nearlyEqual(DecodeSamplerResource(pack(negativeBias)).lodBias, -1.0f), "negative LOD bias decoded incorrectly");

    std::array<std::uint32_t, 3> shortWords{};
    reject([&] { DecodeSamplerResource(shortWords); }, "4 dwords");
}
