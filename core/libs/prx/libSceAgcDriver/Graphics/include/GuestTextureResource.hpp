#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTTEXTURERESOURCE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTTEXTURERESOURCE_HPP

#include "Recompiler.hpp"
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
        default: return 0u;
    }
}

enum class TextureDimension {
    k1D,
    k2D,
    k2DArray,
    kCube,
    k3D
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
    // Last mip level the view exposes; the surface itself holds mipCount levels.
    std::uint32_t lastLevel = 0;
    // DCC metadata of a compressed surface, or 0 (see DccMetadata.hpp).
    std::uint64_t dccAddress = 0;
    bool dccAlphaOnMsb = false;
};

GuestTextureResource DecodeTextureResource(std::span<const std::uint32_t> words);
bool MatchesGuestDimension(ShaderRecompiler::DescriptorImageShape shape, TextureDimension dimension);

}

#endif
