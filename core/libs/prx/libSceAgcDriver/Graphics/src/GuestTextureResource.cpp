#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <algorithm>
#include <array>
#include <bit>
#include <optional>
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
        case 0x02: return TextureTileMode::kD256B;
        case 0x05: return TextureTileMode::kStandard4KB;
        case 0x06: return TextureTileMode::kD4KB;
        case 0x09: return TextureTileMode::kStandard64KB;
        case 0x0a: return TextureTileMode::kD64KB;
        case 0x11: return TextureTileMode::kS64KBT;
        case 0x12: return TextureTileMode::kD64KBT;
        case 0x15: return TextureTileMode::kS4KBX;
        case 0x16: return TextureTileMode::kD4KBX;
        case 0x18: return TextureTileMode::kZ64KBX;
        case 0x19: return TextureTileMode::kS64KBX;
        case 0x1a: return TextureTileMode::kD64KBX;
        case 0x1b: return TextureTileMode::kR64KBX;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor uses an unsupported tile mode " + std::to_string(raw));
    }
}

TextureTileMode resolveGcnTileMode(std::uint32_t index) {
    switch (index) {
        case 0u: return TextureTileMode::kGcnDepth2D64;
        case 1u: return TextureTileMode::kGcnDepth2D128;
        case 2u: return TextureTileMode::kGcnDepth2D256;
        case 3u: return TextureTileMode::kGcnDepth2D512;
        case 4u: return TextureTileMode::kGcnDepth2D1K;
        case 6u: return TextureTileMode::kGcnDepthPrt256;
        case 7u: return TextureTileMode::kGcnDepthPrt1K;
        case 8u: case 31u: return TextureTileMode::kLinear;
        case 11u: return TextureTileMode::kGcnDisplayPrt;
        case 12u: return TextureTileMode::kGcnDisplay2DPrt;
        case 16u: return TextureTileMode::kGcnThinPrt;
        case 17u: return TextureTileMode::kGcnThin2DPrt;
        case 5u: case 13u: return TextureTileMode::kGcnThin1D;
        case 9u: return TextureTileMode::kGcnDisplay1D;
        case 10u: return TextureTileMode::kGcnDisplay2D;
        case 14u: return TextureTileMode::kGcnThin2D;
        default: throw std::runtime_error("AGC graphics: PS4 texture tile mode index " + std::to_string(index) + " (3D or thick) is not implemented");
    }
}

TextureDimension resolveDimension(std::uint32_t raw) {
    switch (raw) {
        case 8: return TextureDimension::k1D;
        case 9: return TextureDimension::k2D;
        case 10: return TextureDimension::k3D;
        case 11: return TextureDimension::kCube;
        case 12: return TextureDimension::k1DArray;
        case 13: return TextureDimension::k2DArray;
        case 14: return TextureDimension::k2DMsaa;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor uses an unsupported image type " + std::to_string(raw));
    }
}

bool ZeroTail(std::span<const std::uint32_t> words) {
    return words[4] == 0u && words[5] == 0u && words[6] == 0u && words[7] == 0u;
}

bool SameLevels(const TileMipLayout& a, const TileMipLayout& b) {
    return a.tiledOffset == b.tiledOffset && a.tiledSize == b.tiledSize && a.width == b.width && a.height == b.height && a.blocksPerRow == b.blocksPerRow && a.pitchBytes == b.pitchBytes && a.tail == b.tail && a.tailX == b.tailX && a.tailY == b.tailY;
}

std::optional<std::array<std::uint32_t, 8>> R128ResidentLevelWords(std::span<const std::uint32_t> words) {
    const auto baseLevel = (words[3] >> 12u) & 0xfu;
    const auto lastLevel = (words[3] >> 16u) & 0xfu;
    const auto typeRaw = (words[3] >> 28u) & 0xfu;
    if (baseLevel == 0u || baseLevel > lastLevel || (typeRaw != 8u && typeRaw != 9u)) return std::nullopt;
    const std::array<std::uint32_t, 8> chainWords{words[0], words[1], words[2], words[3], 0u, lastLevel << 4u, 0u, 0u};
    const auto width = (((words[1] >> 30u) & 0x3u) | ((words[2] & 0xfffu) << 2u)) + 1u;
    const auto height = ((words[2] >> 14u) & 0x3fffu) + 1u;
    const auto minLod = (words[1] >> 8u) & 0xfffu;
    const auto widthMinus1 = std::max(width >> baseLevel, 1u) - 1u;
    const auto heightMinus1 = std::max(height >> baseLevel, 1u) - 1u;
    const auto rebasedMinLod = minLod > baseLevel * 256u ? minLod - baseLevel * 256u : 0u;
    const auto levels = lastLevel - baseLevel;
    auto rebased = chainWords;
    rebased[1] = (words[1] & ~((0xfffu << 8u) | (0x3u << 30u))) | (rebasedMinLod << 8u) | ((widthMinus1 & 0x3u) << 30u);
    rebased[2] = (words[2] & ~(0xfffu | (0x3fffu << 14u))) | ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u);
    rebased[3] = (words[3] & ~(0xffu << 12u)) | (levels << 16u);
    rebased[5] = levels << 4u;
    try {
        const auto chain = DescribeSurface(DecodeTextureResource(chainWords));
        const auto view = DescribeSurface(DecodeTextureResource(rebased));
        if (chain.mips.size() != lastLevel + 1u || view.mips.size() != levels + 1u || chain.layers != view.layers) return std::nullopt;
        for (std::uint32_t level = baseLevel; level <= lastLevel; ++level) {
            if (!SameLevels(chain.mips[level], view.mips[level - baseLevel])) return std::nullopt;
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }
    return rebased;
}

}

bool IsNullTextureResource(std::span<const std::uint32_t> words) {
    Require(words.size() == 8, "guest texture descriptor must contain 8 dwords");
    const auto base40 = (static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffull;
    const auto type = (words[3] >> 28u) & 0xfu;
    return base40 == 0 || type <= 1;
}

GuestTextureResource DecodeTextureResource(std::span<const std::uint32_t> descriptor) {
    Require(descriptor.size() == 8, "guest texture descriptor must contain 8 dwords");
    const bool gcn = ShaderRecompiler::GcnGuestShaders();
    std::array<std::uint32_t, 8> local{};
    std::copy(descriptor.begin(), descriptor.end(), local.begin());
    const auto gcnTiling = gcn ? local[7] : 0u;
    if (gcn) local[6] = local[7] = 0u;
    const std::span<const std::uint32_t> words(local);

    const auto base40 = (static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffull;
    auto baseAddress = base40 << 8u;
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

    auto depth = (words[4] >> 0u) & 0x1fffu;
    const auto baseArray = (words[4] >> 16u) & 0x1fffu;

    const auto arrayPitch = (words[5] >> 0u) & 0xfu;
    auto maxMip = (words[5] >> 4u) & 0xfu;
    const auto minLodWarn = (words[5] >> 8u) & 0xfffu;
    const auto cornerSample = ((words[5] >> 23u) & 0x1u) != 0;
    const auto mipStatsCntEn = ((words[5] >> 25u) & 0x1u) != 0;
    const auto prtDefColor = ((words[5] >> 26u) & 0x1u) != 0;

    const auto mipStatsCntId = words[6] & 0xffu;
    const auto iterate256 = ((words[6] >> 10u) & 0x1u) != 0;
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
    Require(!iterate256 || (typeRaw == 14u && tileModeRaw == 24u && (format == 7u || format == 22u)), "guest texture descriptor ITERATE_256 is implemented only for multisampled Z depth");
    static_cast<void>(maxUncompBlkSize);
    static_cast<void>(maxCompBlkSize);
    // Surfaces are always written uncompressed here (render targets and storage images bypass DCC), so
    // only the fast-clear keys in DCC metadata change what a read returns (see DccMetadata.hpp).
    static_cast<void>(writeCompress);
    if (dccColorTransf) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] texture DCC color transform is ignored (word6=0x%08x word7=0x%08x)\n", words[6], words[7]);
        }
    }
    Require(bcSwizzle <= 5u, "guest texture descriptor uses a reserved BC swizzle");

    Require(baseLevel <= lastLevel, "guest texture descriptor has a base mip level past its last mip level");

    const bool fmask = ShaderRecompiler::IsFmaskTextureFormat(static_cast<ShaderRecompiler::IrBufferFormat>(format));
    const auto fmaskLayout = ShaderRecompiler::FmaskLayoutOf(static_cast<ShaderRecompiler::IrBufferFormat>(format));
    if (fmask) {
        Require(fmaskLayout.has_value(), "guest FMASK format is not implemented");
        Require(!writeCompress && (!metaCompress || metaAddr != 0u), "FMASK CMASK compression is not implemented without readable metadata");
        Require(!metaCompress || metaPipeAligned, "FMASK CMASK metadata must be pipe aligned");
        Require(!metaCompress || format == static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Fmask32_S8_F8), "FMASK CMASK compression is implemented only for 8-sample, 8-fragment FMASK");
        Require((typeRaw == 9u || typeRaw == 13u) && tileModeRaw == 24u && baseLevel == 0u && lastLevel == 0u && maxMip == 0u, "FMASK reads require a single-mip 2D or 2D-array Z_64KB_X surface");
        const bool identity = format != static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Fmask32_S8_F8) && dstSelX == 4u && dstSelY == 5u && dstSelZ == 6u && dstSelW == 7u;
        for (const auto selector : {dstSelX, dstSelY, dstSelZ, dstSelW}) Require(identity || selector == 0u || selector == 1u || selector == 4u, "FMASK destination channel selector is not implemented");
    }

    const auto describe = [&] { return " (format " + std::to_string(format) + ", " + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth + 1) + ", type " + std::to_string(typeRaw) + (gcn ? ", PS4 tile index " + std::to_string(ShaderRecompiler::GcnImageTilingIndex(gcnTiling)) : ", tile " + std::to_string(tileModeRaw)) + ", mips " + std::to_string(baseLevel) + ".." + std::to_string(lastLevel) + " of " + std::to_string(maxMip + 1) + ")"; };
    TextureTileMode tileMode;
    TextureDimension dimension;
    try {
        tileMode = gcn ? resolveGcnTileMode(ShaderRecompiler::GcnImageTilingIndex(gcnTiling)) : resolveTileMode(tileModeRaw);
        dimension = resolveDimension(typeRaw);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + describe());
    }
    std::uint32_t samples = 1;
    if (dimension == TextureDimension::k2DMsaa) {
        Require(baseLevel == 0 && lastLevel >= 1u && lastLevel <= 4u && maxMip == lastLevel, "guest multisampled texture descriptor does not hold the same sample count of 2 to 16 in its last level and its last mip, which is not implemented" + describe());
        samples = 1u << lastLevel;
        lastLevel = 0;
        maxMip = 0;
    }
    // Views may name levels past MAX_MIP (a 512x512 view through 1x1 over a 9-level surface); the
    // hardware never addresses them, so the view ends at the surface's last level.
    if (baseLevel <= maxMip) lastLevel = std::min(lastLevel, maxMip);
    std::uint32_t pipeBankXor = 0;
    if (XorSwizzleMode(tileMode) != 0) {
        pipeBankXor = static_cast<std::uint32_t>(baseAddress & (tileMode == TextureTileMode::kS4KBX || tileMode == TextureTileMode::kD4KBX ? 0xfffu : 0xffffu));
        baseAddress -= pipeBankXor;
        Require(baseAddress != 0, "guest texture descriptor has a null base address");
    }

    switch (dimension) {
        case TextureDimension::k1D:
            Require(height == 1 && depth == 0 && baseArray == 0, "guest 1D texture descriptor has a nonzero height, depth or base array");
            break;
        case TextureDimension::k2D:
            if (depth != 0 && tileMode == TextureTileMode::kLinear) {
                const auto elementsPerPitchUnit = 256u / BytesPerElement(format);
                const auto defaultPitch = (width + elementsPerPitchUnit - 1u) / elementsPerPitchUnit * elementsPerPitchUnit;
                Require(!IsBlockCompressed(format) && maxMip == 0 && baseArray == 0 && depth + 1u == defaultPitch, "guest 2D linear texture descriptor sets a custom pitch other than its default pitch, which is not implemented" + describe());
                depth = 0;
            }
            Require(depth == 0 && baseArray == 0, "guest 2D texture descriptor has a nonzero depth or base array");
            break;
        case TextureDimension::k2DMsaa:
            Require(depth == 0 && baseArray == 0, "guest multisampled 2D texture descriptor has a nonzero depth or base array");
            break;
        case TextureDimension::k3D:
            // word4 DEPTH holds the depth minus one; BASE_ARRAY has no meaning for volumes.
            Require(baseArray == 0, "guest 3D texture descriptor has a nonzero base array");
            break;
        case TextureDimension::k2DArray:
            Require(baseArray <= depth, "guest 2D array texture descriptor has a base array past its last array slice");
            break;
        case TextureDimension::k1DArray:
            Require(height == 1, "guest 1D array texture descriptor has a nonzero height");
            Require(baseArray <= depth, "guest 1D array texture descriptor has a base array past its last array slice");
            Require(tileMode == TextureTileMode::kLinear || tileMode == TextureTileMode::kZ64KBX || tileMode == TextureTileMode::kR64KBX, "guest 1D array texture descriptor uses a tile mode other than linear, Z or R, which 1D resources cannot use");
            break;
        case TextureDimension::kCube:
            Require(width == height, "guest cube texture descriptor is not square");
            Require(baseArray <= depth, "guest cube texture descriptor has a base array past its last array slice");
            if (baseArray == depth) depth += 5u;
            if (depth - baseArray + 1u < 6u) {
                Require(false, "guest cube texture descriptor holds fewer than 6 array slices" +
                    describe() + " (base array " + std::to_string(baseArray) + ")");
            }
            break;
    }

    GuestTextureResource result{};
    result.baseAddress = baseAddress;
    result.pipeBankXor = pipeBankXor;
    result.width = width;
    result.height = height;
    result.depthOrLastArray = depth;
    result.baseArray = baseArray;
    result.mipCount = maxMip + 1u;
    result.allocatedMipCount = maxMip + 1u;
    result.baseLevel = baseLevel;
    result.lastLevel = lastLevel;
    result.tileMode = tileMode;
    result.dimension = dimension;
    result.format = !fmask ? format : static_cast<std::uint32_t>(fmaskLayout->storageBits == 8u ? ShaderRecompiler::IrBufferFormat::Format8UInt : fmaskLayout->storageBits == 16u ? ShaderRecompiler::IrBufferFormat::Format16UInt : ShaderRecompiler::IrBufferFormat::Format32UInt);
    result.dstSelX = static_cast<std::uint8_t>(dstSelX);
    result.dstSelY = static_cast<std::uint8_t>(dstSelY);
    result.dstSelZ = static_cast<std::uint8_t>(dstSelZ);
    result.dstSelW = static_cast<std::uint8_t>(dstSelW);
    result.bcSwizzle = static_cast<std::uint8_t>(bcSwizzle);
    result.dccAddress = metaCompress && !fmask ? metaAddr << 8u : 0u;
    result.fmaskCmaskAddress = metaCompress && fmask ? metaAddr << 8u : 0u;
    result.dccAlphaOnMsb = dccAlphaPos;
    result.dccPipeAligned = metaPipeAligned;
    result.iterate256 = iterate256;
    result.minLod = minLod;
    result.samples = samples;
    if (gcn) {
        result.gcnPitch = ShaderRecompiler::GcnImagePitch(gcnTiling);
        result.gcnPow2Pad = ShaderRecompiler::GcnImagePow2Pad(gcnTiling);
        result.gcnLinearGeneral = ShaderRecompiler::GcnImageTilingIndex(gcnTiling) == 31u;
        Require(result.gcnPitch >= width, "PS4 texture descriptor has a pitch below its width" + describe());
    }
    if (baseLevel > maxMip) {
        const bool fits = LevelsFitAllocation(result, lastLevel + 1u);
        if (!fits && ZeroTail(words)) {
            if (const auto resident = R128ResidentLevelWords(words)) return DecodeTextureResource(*resident);
        }
        Require(fits, "guest texture descriptor starts past the surface's last mip level at levels that would move the surface's own" + describe());
        result.mipCount = lastLevel + 1u;
    }
    return result;
}

std::array<std::uint32_t, 8> EncodeTextureResource(const GuestTextureResource& resource) {
    const bool multisampled = resource.dimension == TextureDimension::k2DMsaa;
    Require((resource.dimension == TextureDimension::k2D && resource.samples == 1u) || (multisampled && resource.samples >= 2u && resource.samples <= 16u && std::has_single_bit(resource.samples)), "only 2D and multisampled 2D surfaces have a texture descriptor encoding");
    Require(resource.mipCount == 1u && resource.allocatedMipCount <= 1u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.depthOrLastArray == 0u && resource.baseArray == 0u && resource.dccAddress == 0u && resource.minLod == 0u && !resource.iterate256 && resource.bcSwizzle == 0u, "only single-level, single-layer surfaces without metadata have a texture descriptor encoding");
    Require(resource.width >= 1u && resource.width <= 0x4000u && resource.height >= 1u && resource.height <= 0x4000u && resource.format < 0x200u, "the surface's extent or format has no texture descriptor encoding");
    const auto xorMask = resource.tileMode == TextureTileMode::kS4KBX || resource.tileMode == TextureTileMode::kD4KBX ? 0xfffull : 0xffffull;
    Require(XorSwizzleMode(resource.tileMode) != 0 ? (resource.baseAddress & xorMask) == 0 && (resource.pipeBankXor & ~xorMask) == 0 : resource.pipeBankXor == 0, "the surface's pipe and bank XOR has no texture descriptor encoding");
    const auto base = (resource.baseAddress | resource.pipeBankXor) >> 8u;
    Require(base != 0 && (resource.baseAddress & 0xffu) == 0 && base <= 0xffffffffffull, "the surface's address has no texture descriptor encoding");
    std::uint32_t tileMode = 0;
    switch (resource.tileMode) {
        case TextureTileMode::kLinear: tileMode = 0x00; break;
        case TextureTileMode::kStandard256B: tileMode = 0x01; break;
        case TextureTileMode::kD256B: tileMode = 0x02; break;
        case TextureTileMode::kStandard4KB: tileMode = 0x05; break;
        case TextureTileMode::kD4KB: tileMode = 0x06; break;
        case TextureTileMode::kStandard64KB: tileMode = 0x09; break;
        case TextureTileMode::kD64KB: tileMode = 0x0a; break;
        case TextureTileMode::kS64KBT: tileMode = 0x11; break;
        case TextureTileMode::kD64KBT: tileMode = 0x12; break;
        case TextureTileMode::kS4KBX: tileMode = 0x15; break;
        case TextureTileMode::kD4KBX: tileMode = 0x16; break;
        case TextureTileMode::kZ64KBX: tileMode = 0x18; break;
        case TextureTileMode::kS64KBX: tileMode = 0x19; break;
        case TextureTileMode::kD64KBX: tileMode = 0x1a; break;
        case TextureTileMode::kR64KBX: tileMode = 0x1b; break;
    }
    const auto sampleLevel = static_cast<std::uint32_t>(std::countr_zero(resource.samples));
    std::array<std::uint32_t, 8> words{};
    words[0] = static_cast<std::uint32_t>(base);
    words[1] = static_cast<std::uint32_t>(base >> 32u) | (resource.format << 20u) | (((resource.width - 1u) & 3u) << 30u);
    words[2] = ((resource.width - 1u) >> 2u) | ((resource.height - 1u) << 14u);
    words[3] = resource.dstSelX | (resource.dstSelY << 3u) | (resource.dstSelZ << 6u) | (resource.dstSelW << 9u) | (sampleLevel << 16u) | (tileMode << 20u) | ((multisampled ? 14u : 9u) << 28u);
    words[5] = sampleLevel << 4u;
    return words;
}

float EffectiveMinLod(const GuestTextureResource& resource) {
    if (resource.minLod <= resource.baseLevel * 256u) return 0.0f;
    return std::min(static_cast<float>(resource.minLod) / 256.0f, static_cast<float>(resource.lastLevel));
}

bool DescriptorSingleLevel(std::span<const std::uint32_t> words) {
    return ((words[3] >> 12u) & 0xfu) == ((words[3] >> 16u) & 0xfu);
}

bool FirstLayerDescriptor(std::span<const std::uint32_t> words, std::array<std::uint32_t, 8>& narrowed) {
    if (words.size() != narrowed.size()) return false;
    const auto resource = DecodeTextureResource(words);
    if (resource.dimension != TextureDimension::k2DArray || resource.depthOrLastArray <= resource.baseArray) return false;
    std::copy(words.begin(), words.end(), narrowed.begin());
    narrowed[4] = (narrowed[4] & ~0x1fffu) | resource.baseArray;
    return true;
}

bool MatchesGuestDimension(ShaderRecompiler::DescriptorImageShape shape, TextureDimension dimension) {
    switch (shape) {
        case ShaderRecompiler::DescriptorImageShape::Image1D: return dimension == TextureDimension::k1D;
        case ShaderRecompiler::DescriptorImageShape::Image2D: return dimension == TextureDimension::k2D;
        case ShaderRecompiler::DescriptorImageShape::Image2DArray: return dimension == TextureDimension::k2DArray || dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::ImageCube: return dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::Image3D: return dimension == TextureDimension::k3D;
        case ShaderRecompiler::DescriptorImageShape::Image1DArray: return dimension == TextureDimension::k1DArray;
        case ShaderRecompiler::DescriptorImageShape::Image2DMultisample: return dimension == TextureDimension::k2DMsaa;
        case ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray: return dimension == TextureDimension::k2DMsaa;
    }
    throw std::runtime_error("AGC graphics: MatchesGuestDimension encountered an unknown descriptor image shape");
}

bool ArrayLayersServeView(const GuestTextureResource& surface, const GuestTextureResource& view) {
    if (view.dimension == surface.dimension && view.depthOrLastArray == surface.depthOrLastArray) return true;
    if (surface.dimension != TextureDimension::k2DArray) return false;
    if (view.dimension == TextureDimension::k2D) return view.baseArray == 0;
    return view.dimension == TextureDimension::k2DArray && view.depthOrLastArray <= surface.depthOrLastArray;
}

}
