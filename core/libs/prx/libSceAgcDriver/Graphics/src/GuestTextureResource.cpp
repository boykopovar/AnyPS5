#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

void requireValidDstSel(std::uint32_t value) {
    Require(value == 0 || value == 1 || (value >= 4 && value <= 7), "guest texture descriptor has an invalid destination channel selector");
}

TextureTileMode resolveTileMode(std::uint32_t raw) {
    switch (raw) {
        case 0x00: return TextureTileMode::kLinear;
        case 0x01: return TextureTileMode::kStandard256B;
        case 0x05: return TextureTileMode::kStandard4KB;
        case 0x09: return TextureTileMode::kStandard64KB;
        case 0x18: return TextureTileMode::kZ64KBX;
        case 0x19: return TextureTileMode::kS64KBX;
        case 0x1a: return TextureTileMode::kD64KBX;
        case 0x1b: return TextureTileMode::kR64KBX;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor uses an unsupported tile mode " + std::to_string(raw));
    }
}

TextureDimension resolveDimension(std::uint32_t raw) {
    switch (raw) {
        case 8: return TextureDimension::k1D;
        case 9: return TextureDimension::k2D;
        case 10: return TextureDimension::k3D;
        case 11: return TextureDimension::kCube;
        case 13: return TextureDimension::k2DArray;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor uses an unsupported image type " + std::to_string(raw));
    }
}

}

GuestTextureResource DecodeTextureResource(std::span<const std::uint32_t> words) {
    Require(words.size() == 8, "guest texture descriptor must contain 8 dwords");

    const auto base40 = (static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffull;
    const auto baseAddress = base40 << 8u;
    Require(baseAddress != 0, "guest texture descriptor has a null base address");

    const auto minLod = (words[1] >> 8u) & 0xfffu;
    const auto format = (words[1] >> 20u) & 0x1ffu;
    const auto width = (((words[1] >> 30u) & 0x3u) | (((words[2] >> 0u) & 0xfffu) << 2u)) + 1u;
    const auto height = ((words[2] >> 14u) & 0x3fffu) + 1u;

    const auto dstSelX = (words[3] >> 0u) & 0x7u;
    const auto dstSelY = (words[3] >> 3u) & 0x7u;
    const auto dstSelZ = (words[3] >> 6u) & 0x7u;
    const auto dstSelW = (words[3] >> 9u) & 0x7u;
    const auto baseLevel = (words[3] >> 12u) & 0xfu;
    auto lastLevel = (words[3] >> 16u) & 0xfu;
    const auto tileModeRaw = (words[3] >> 20u) & 0x1fu;
    const auto bcSwizzle = (words[3] >> 25u) & 0x7u;
    const auto typeRaw = (words[3] >> 28u) & 0xfu;

    const auto depth = (words[4] >> 0u) & 0x1fffu;
    const auto baseArray = (words[4] >> 16u) & 0x1fffu;

    const auto arrayPitch = (words[5] >> 0u) & 0xfu;
    const auto maxMip = (words[5] >> 4u) & 0xfu;
    const auto minLodWarn = (words[5] >> 8u) & 0xfffu;
    const auto cornerSample = ((words[5] >> 23u) & 0x1u) != 0;
    const auto mipStatsCntEn = ((words[5] >> 25u) & 0x1u) != 0;
    const auto prtDefColor = ((words[5] >> 26u) & 0x1u) != 0;

    const auto mipStatsCntId = words[6] & 0xffu;
    const auto msaaDepth = ((words[6] >> 10u) & 0x1u) != 0;
    const auto maxUncompBlkSize = (words[6] >> 15u) & 0x3u;
    const auto maxCompBlkSize = (words[6] >> 17u) & 0x3u;
    const auto metaPipeAligned = ((words[6] >> 19u) & 0x1u) != 0;
    const auto writeCompress = ((words[6] >> 20u) & 0x1u) != 0;
    const auto metaCompress = ((words[6] >> 21u) & 0x1u) != 0;
    const auto dccAlphaPos = ((words[6] >> 22u) & 0x1u) != 0;
    const auto dccColorTransf = ((words[6] >> 23u) & 0x1u) != 0;
    const auto metaAddr = ((static_cast<std::uint64_t>(words[6]) >> 24u) & 0xffu) | (static_cast<std::uint64_t>(words[7]) << 8u);

    requireValidDstSel(dstSelX);
    requireValidDstSel(dstSelY);
    requireValidDstSel(dstSelZ);
    requireValidDstSel(dstSelW);

    Require(minLod == 0, "guest texture descriptor uses a nonzero minimum LOD clamp which is not implemented");
    // The LOD warning threshold and mip statistics counters are texture-streaming feedback: the GPU
    // reports which mips were wanted. Nothing is reported back here, which only affects what the title
    // chooses to stream, not what this access returns.
    if (minLodWarn != 0 || mipStatsCntId != 0 || mipStatsCntEn) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] texture streaming feedback (LOD warning / mip statistics) is not reported\n");
        }
    }
    Require(!cornerSample, "guest texture descriptor uses corner sampling which is not implemented");
    Require(!prtDefColor, "guest texture descriptor uses a partially resident default color which is not implemented");
    Require(arrayPitch == 0, "guest texture descriptor uses a nonzero array pitch which is not implemented");
    Require(!msaaDepth, "guest texture descriptor uses MSAA which is not implemented");
    // DCC block sizes only apply with metadata compression, which is rejected above.
    static_cast<void>(maxUncompBlkSize);
    static_cast<void>(maxCompBlkSize);
    // Surfaces are always written uncompressed here (render targets and storage images bypass DCC), so
    // only the fast-clear keys in DCC metadata change what a read returns (see DccMetadata.hpp).
    static_cast<void>(metaPipeAligned);
    static_cast<void>(writeCompress);
    if (dccColorTransf) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] texture DCC color transform is ignored (word6=0x%08x word7=0x%08x)\n", words[6], words[7]);
        }
    }
    Require(bcSwizzle == 0, "guest texture descriptor uses a BC swizzle which is not implemented");

    Require(baseLevel <= lastLevel, "guest texture descriptor has a base mip level past its last mip level");


    const auto describe = [&] { return " (format " + std::to_string(format) + ", " + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth + 1) + ", type " + std::to_string(typeRaw) + ", tile " + std::to_string(tileModeRaw) + ", mips " + std::to_string(baseLevel) + ".." + std::to_string(lastLevel) + " of " + std::to_string(maxMip + 1) + ")"; };
    TextureTileMode tileMode;
    TextureDimension dimension;
    try {
        tileMode = resolveTileMode(tileModeRaw);
        dimension = resolveDimension(typeRaw);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + describe());
    }
    Require(baseLevel <= maxMip, "guest texture descriptor starts past the surface's last mip level" + describe());
    // Views may name levels past MAX_MIP (a 512x512 view through 1x1 over a 9-level surface); the
    // hardware never addresses them, so the view ends at the surface's last level.
    lastLevel = std::min(lastLevel, maxMip);
    // XOR swizzles fold a pipe/bank XOR into the low address bits; only unmodified 64 KiB bases are modeled.
    Require(XorSwizzleMode(tileMode) == 0 || (baseAddress & 0xffffu) == 0, "guest texture descriptor combines an XOR swizzle with a pipe/bank XOR base which is not implemented");

    switch (dimension) {
        case TextureDimension::k1D:
            Require(height == 1 && depth == 0 && baseArray == 0, "guest 1D texture descriptor has a nonzero height, depth or base array");
            break;
        case TextureDimension::k2D:
            Require(depth == 0 && baseArray == 0, "guest 2D texture descriptor has a nonzero depth or base array");
            break;
        case TextureDimension::k3D:
            // word4 DEPTH holds the depth minus one; BASE_ARRAY has no meaning for volumes.
            Require(baseArray == 0, "guest 3D texture descriptor has a nonzero base array");
            break;
        case TextureDimension::k2DArray:
            Require(baseArray <= depth, "guest 2D array texture descriptor has a base array past its last array slice");
            break;
        case TextureDimension::kCube:
            Require(width == height, "guest cube texture descriptor is not square");
            Require(baseArray <= depth, "guest cube texture descriptor has a base array past its last array slice");
            Require((depth - baseArray + 1u) % 6u == 0, "guest cube texture descriptor does not contain a multiple of 6 array slices");
            break;
    }

    GuestTextureResource result{};
    result.baseAddress = baseAddress;
    result.width = width;
    result.height = height;
    result.depthOrLastArray = depth;
    result.baseArray = baseArray;
    result.mipCount = maxMip + 1u;
    result.baseLevel = baseLevel;
    result.lastLevel = lastLevel;
    result.tileMode = tileMode;
    result.dimension = dimension;
    result.format = format;
    result.dstSelX = static_cast<std::uint8_t>(dstSelX);
    result.dstSelY = static_cast<std::uint8_t>(dstSelY);
    result.dstSelZ = static_cast<std::uint8_t>(dstSelZ);
    result.dstSelW = static_cast<std::uint8_t>(dstSelW);
    result.dccAddress = metaCompress ? metaAddr << 8u : 0u;
    result.dccAlphaOnMsb = dccAlphaPos;
    return result;
}

bool MatchesGuestDimension(ShaderRecompiler::DescriptorImageShape shape, TextureDimension dimension) {
    switch (shape) {
        case ShaderRecompiler::DescriptorImageShape::Image1D: return dimension == TextureDimension::k1D;
        case ShaderRecompiler::DescriptorImageShape::Image2D: return dimension == TextureDimension::k2D;
        case ShaderRecompiler::DescriptorImageShape::Image2DArray: return dimension == TextureDimension::k2DArray || dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::ImageCube: return dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::Image3D: return dimension == TextureDimension::k3D;
    }
    throw std::runtime_error("AGC graphics: MatchesGuestDimension encountered an unknown descriptor image shape");
}

}
