#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTTEXTURERESOURCE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTTEXTURERESOURCE_HPP

#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

enum class TextureTileMode {
    kLinear,
    kStandard256B,
    kStandard4KB,
    kStandard64KB,
    // 64 KiB XOR swizzles; their address equations live in TextureSwizzleEquations.hpp.
    kZ64KBX,
    kS64KBX,
    kD64KBX,
    kR64KBX,
    kD256B,
    kD4KB,
    kD64KB,
    kS64KBT,
    kD64KBT,
    kS4KBX,
    kD4KBX,
    kGcnThin1D,
    kGcnDisplay1D,
    kGcnDisplay2D,
    kGcnThin2D,
    kGcnDepth2D64,
    kGcnDepth2D128,
    kGcnDepth2D256,
    kGcnDepth2D512,
    kGcnDepth2D1K,
    kGcnDepthPrt256,
    kGcnDepthPrt1K,
    kGcnDisplayPrt,
    kGcnDisplay2DPrt,
    kGcnThinPrt,
    kGcnThin2DPrt,
    // Upstream's name for SW_64KB_R_X (tile mode 0x1b).
    RenderTarget64KB = kR64KBX
};

// The hardware SW_MODE of an XOR swizzle tile mode, or 0 for the modes addressed without an equation table.
constexpr std::uint32_t XorSwizzleMode(TextureTileMode mode) {
    switch (mode) {
        case TextureTileMode::kZ64KBX: return 24u;
        case TextureTileMode::kS64KBX: return 25u;
        case TextureTileMode::kD64KBX: return 26u;
        case TextureTileMode::kR64KBX: return 27u;
        case TextureTileMode::kS64KBT: return 17u;
        case TextureTileMode::kD64KBT: return 18u;
        case TextureTileMode::kS4KBX: return 21u;
        case TextureTileMode::kD4KBX: return 22u;
        default: return 0u;
    }
}

constexpr std::uint32_t EquationSwizzleMode(TextureTileMode mode) {
    switch (mode) {
        case TextureTileMode::kD256B: return 2u;
        case TextureTileMode::kD4KB: return 6u;
        case TextureTileMode::kD64KB: return 10u;
        default: return XorSwizzleMode(mode);
    }
}

enum class TextureDimension {
    k1D,
    k2D,
    k2DArray,
    kCube,
    k3D,
    k1DArray,
    k2DMsaa
};

struct GuestTextureResource {
    std::uint64_t baseAddress;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depthOrLastArray;
    std::uint32_t baseArray;
    std::uint32_t mipCount;
    std::uint32_t baseLevel;
    TextureTileMode tileMode;
    TextureDimension dimension;
    std::uint32_t format;
    std::uint8_t dstSelX;
    std::uint8_t dstSelY;
    std::uint8_t dstSelZ;
    std::uint8_t dstSelW;
    std::uint32_t pipeBankXor = 0;
    std::uint8_t bcSwizzle = 0;
    // Last mip level the view exposes; the surface itself holds mipCount levels.
    std::uint32_t lastLevel = 0;
    // DCC metadata of a compressed surface, or 0 (see DccMetadata.hpp).
    std::uint64_t dccAddress = 0;
    bool dccAlphaOnMsb = false;
    bool dccPipeAligned = false;
    bool iterate256 = false;
    std::uint32_t minLod = 0;
    std::uint32_t allocatedMipCount = 0;
    std::uint32_t samples = 1;
    std::uint64_t fmaskCmaskAddress = 0;
    std::uint32_t gcnPitch = 0;
    bool gcnPow2Pad = false;
    bool gcnLinearGeneral = false;
};

constexpr bool IsGcnMicroTiled(TextureTileMode mode) {
    return mode == TextureTileMode::kGcnThin1D || mode == TextureTileMode::kGcnDisplay1D;
}

constexpr bool IsGcnDepthTiled(TextureTileMode mode) {
    return mode == TextureTileMode::kGcnDepth2D64 || mode == TextureTileMode::kGcnDepth2D128 || mode == TextureTileMode::kGcnDepth2D256 || mode == TextureTileMode::kGcnDepth2D512 || mode == TextureTileMode::kGcnDepth2D1K
        || mode == TextureTileMode::kGcnDepthPrt256 || mode == TextureTileMode::kGcnDepthPrt1K;
}

constexpr bool IsGcnPrtTiled(TextureTileMode mode) {
    return mode == TextureTileMode::kGcnDepthPrt256 || mode == TextureTileMode::kGcnDepthPrt1K || mode == TextureTileMode::kGcnDisplayPrt || mode == TextureTileMode::kGcnDisplay2DPrt || mode == TextureTileMode::kGcnThinPrt || mode == TextureTileMode::kGcnThin2DPrt;
}

constexpr bool IsGcnMacroTiled(TextureTileMode mode) {
    return mode == TextureTileMode::kGcnDisplay2D || mode == TextureTileMode::kGcnThin2D || IsGcnDepthTiled(mode) || IsGcnPrtTiled(mode);
}

constexpr bool IsGcnTiled(TextureTileMode mode) {
    return IsGcnMicroTiled(mode) || IsGcnMacroTiled(mode);
}

constexpr TextureTileMode GcnMicroTileMode(TextureTileMode mode) {
    if (mode == TextureTileMode::kGcnDisplay2D || mode == TextureTileMode::kGcnDisplayPrt || mode == TextureTileMode::kGcnDisplay2DPrt) return TextureTileMode::kGcnDisplay1D;
    return IsGcnMacroTiled(mode) ? TextureTileMode::kGcnThin1D : mode;
}

float EffectiveMinLod(const GuestTextureResource& resource);

bool IsNullTextureResource(std::span<const std::uint32_t> words);
GuestTextureResource DecodeTextureResource(std::span<const std::uint32_t> words);
std::array<std::uint32_t, 8> EncodeTextureResource(const GuestTextureResource& resource);
bool DescriptorSingleLevel(std::span<const std::uint32_t> words);
bool FirstLayerDescriptor(std::span<const std::uint32_t> words, std::array<std::uint32_t, 8>& narrowed);
bool MatchesGuestDimension(ShaderRecompiler::DescriptorImageShape shape, TextureDimension dimension);
bool ArrayLayersServeView(const GuestTextureResource& surface, const GuestTextureResource& view);

}

#endif
