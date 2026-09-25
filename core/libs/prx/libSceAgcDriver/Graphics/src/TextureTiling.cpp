#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace AgcDriver::Graphics {

namespace {

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1u) / alignment * alignment;
}

std::uint32_t ShiftCeil(std::uint32_t value, std::uint32_t shift) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) + (1ull << shift) - 1ull) >> shift);
}

std::uint32_t CalcLinearBlockWidth(std::uint32_t bytesPerElement) {
    return 256u / bytesPerElement;
}

struct BlockLayout {
    std::uint32_t blockSize;
    std::uint32_t blockWidth;
    std::uint32_t blockHeight;
};

struct Log2BlockDimensions {
    std::uint8_t width;
    std::uint8_t height;
};

constexpr Log2BlockDimensions kLog2BlockThin256B[] = {{4, 4}, {4, 3}, {3, 3}, {3, 2}, {2, 2}};
constexpr Log2BlockDimensions kLog2BlockThin4KB[] = {{6, 6}, {6, 5}, {5, 5}, {5, 4}, {4, 4}};
constexpr Log2BlockDimensions kLog2BlockThin64KB[] = {{8, 8}, {8, 7}, {7, 7}, {7, 6}, {6, 6}};

BlockLayout GetBlockLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    Require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "unsupported bytes per element for tiled texture geometry");
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    switch (tileMode) {
        case TextureTileMode::kLinear: throw std::runtime_error("AGC graphics: GetBlockLayout does not apply to linear tiling");
        case TextureTileMode::kStandard256B: return {256u, 1u << kLog2BlockThin256B[index].width, 1u << kLog2BlockThin256B[index].height};
        case TextureTileMode::kStandard4KB: return {4096u, 1u << kLog2BlockThin4KB[index].width, 1u << kLog2BlockThin4KB[index].height};
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kZ64KBX:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX:
        case TextureTileMode::kR64KBX: return {65536u, 1u << kLog2BlockThin64KB[index].width, 1u << kLog2BlockThin64KB[index].height};
    }
    throw std::runtime_error("AGC graphics: GetBlockLayout encountered an unknown tile mode");
}

struct MipTailLocation {
    std::uint32_t x;
    std::uint32_t y;
};

constexpr MipTailLocation kMipTailThin4KB[5][8] = {
    {{32, 0}, {16, 32}, {0, 48}, {0, 32}, {16, 16}, {16, 0}, {0, 16}, {0, 0}},
    {{32, 0}, {16, 16}, {0, 24}, {0, 16}, {16, 8}, {16, 0}, {0, 8}, {0, 0}},
    {{16, 0}, {8, 16}, {0, 24}, {0, 16}, {8, 8}, {8, 0}, {0, 8}, {0, 0}},
    {{16, 0}, {8, 8}, {0, 12}, {0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
};

constexpr MipTailLocation kMipTailThin64KB[5][12] = {
    {{128, 0}, {0, 128}, {64, 0}, {0, 64}, {32, 0}, {16, 32}, {0, 48}, {0, 32}, {16, 16}, {16, 0}, {0, 16}, {0, 0}},
    {{128, 0}, {0, 64}, {64, 0}, {0, 32}, {32, 0}, {16, 16}, {0, 24}, {0, 16}, {16, 8}, {16, 0}, {0, 8}, {0, 0}},
    {{64, 0}, {0, 64}, {32, 0}, {0, 32}, {16, 0}, {8, 16}, {0, 24}, {0, 16}, {8, 8}, {8, 0}, {0, 8}, {0, 0}},
    {{64, 0}, {0, 32}, {32, 0}, {0, 16}, {16, 0}, {8, 8}, {0, 12}, {0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{32, 0}, {0, 32}, {16, 0}, {0, 16}, {8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
};

struct MipTailLayout {
    const MipTailLocation* locations;
    std::uint32_t maxLevels;
    std::uint32_t widthLimit;
    std::uint32_t heightLimit;
};

template<std::size_t TLevels>
MipTailLayout MakeMipTailLayout(const MipTailLocation (&locations)[TLevels], std::uint32_t widthLimit, std::uint32_t heightLimit) {
    return {locations, static_cast<std::uint32_t>(TLevels), widthLimit, heightLimit};
}

bool GetMipTailLayout(TextureTileMode tileMode, const BlockLayout& block, std::uint32_t bytesPerElement, MipTailLayout& out) {
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    switch (tileMode) {
        case TextureTileMode::kLinear:
        case TextureTileMode::kStandard256B: return false;
        case TextureTileMode::kStandard4KB:
            out = MakeMipTailLayout(kMipTailThin4KB[index], block.blockWidth >> 1u, block.blockHeight);
            return true;
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kZ64KBX:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX:
        case TextureTileMode::kR64KBX:
            out = MakeMipTailLayout(kMipTailThin64KB[index], block.blockWidth >> 1u, block.blockHeight);
            return true;
    }
    throw std::runtime_error("AGC graphics: GetMipTailLayout encountered an unknown tile mode");
}

std::uint32_t TexelLevelDimension(std::uint32_t guestDimension, std::uint32_t level, std::uint32_t texelScale) {
    return std::max((std::max(guestDimension >> level, 1u) + texelScale - 1u) / texelScale, 1u);
}

std::vector<TileMipLayout> ComputeLinearMipLayout(std::uint32_t bytesPerElement, std::uint32_t texelWidth, std::uint32_t texelHeight, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    const auto compressed = texelWidth != 1u || texelHeight != 1u;
    const auto elementsWidth0 = (width + texelWidth - 1u) / texelWidth;
    const auto elementsHeight0 = (height + texelHeight - 1u) / texelHeight;
    const auto blockWidth = CalcLinearBlockWidth(bytesPerElement);

    std::vector<TileMipLayout> mips(mipCount);
    std::uint64_t offset = 0;
    for (auto level = mipCount; level-- > 0;) {
        const auto elementsLevelWidth = std::max(ShiftCeil(elementsWidth0, level), 1u);
        const auto elementsLevelHeight = std::max(ShiftCeil(elementsHeight0, level), 1u);
        const auto paddedElementsWidth = AlignUp(elementsLevelWidth, blockWidth);
        const auto size = static_cast<std::uint64_t>(paddedElementsWidth) * elementsLevelHeight * bytesPerElement;
        Require(size != 0, "computed a zero-sized linear texture mip level");

        auto pitchBytes = paddedElementsWidth * bytesPerElement;
        if (compressed) pitchBytes = std::max(pitchBytes, 32u);

        auto& mip = mips[level];
        mip.tiledOffset = offset;
        mip.tiledSize = size;
        mip.linearOffset = offset;
        mip.linearSize = size;
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.blocksPerRow = paddedElementsWidth;
        mip.pitchBytes = pitchBytes;
        mip.tail = false;
        mip.tailX = 0;
        mip.tailY = 0;
        offset += size;
    }
    return mips;
}

std::vector<TileMipLayout> ComputeTiledMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t texelWidth, std::uint32_t texelHeight, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    const auto block = GetBlockLayout(tileMode, bytesPerElement);
    const auto elementsWidth0 = (width + texelWidth - 1u) / texelWidth;
    const auto elementsHeight0 = (height + texelHeight - 1u) / texelHeight;

    MipTailLayout tail{};
    const auto hasTail = GetMipTailLayout(tileMode, block, bytesPerElement, tail);

    auto firstTailLevel = mipCount;
    if (hasTail && mipCount > 1) {
        for (std::uint32_t level = 0; level < mipCount; ++level) {
            if (ShiftCeil(elementsWidth0, level) <= tail.widthLimit && ShiftCeil(elementsHeight0, level) <= tail.heightLimit && mipCount - level <= tail.maxLevels) {
                firstTailLevel = level;
                break;
            }
        }
    }

    std::vector<TileMipLayout> mips(mipCount);
    std::uint64_t blockSliceSize = 0;

    for (std::uint32_t level = 0; level < firstTailLevel; ++level) {
        auto& mip = mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        const auto paddedWidth = AlignUp(std::max(ShiftCeil(elementsWidth0, level), 1u), block.blockWidth);
        const auto paddedHeight = AlignUp(std::max(ShiftCeil(elementsHeight0, level), 1u), block.blockHeight);
        mip.blocksPerRow = paddedWidth / block.blockWidth;
        mip.pitchBytes = paddedWidth * bytesPerElement;
        mip.tiledSize = static_cast<std::uint64_t>(paddedWidth) * paddedHeight * bytesPerElement;
        // The detiler writes linear rows at pitchBytes, so the region spans whole padded rows.
        mip.linearSize = static_cast<std::uint64_t>(mip.pitchBytes) * mip.height;
        mip.tail = false;
        mip.tailX = 0;
        mip.tailY = 0;
        blockSliceSize += mip.tiledSize;
    }

    if (firstTailLevel < mipCount) blockSliceSize += block.blockSize;

    for (auto level = firstTailLevel; level < mipCount; ++level) {
        auto& mip = mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.blocksPerRow = 1u;
        mip.pitchBytes = block.blockWidth * bytesPerElement;
        mip.tiledSize = block.blockSize;
        mip.linearSize = static_cast<std::uint64_t>(mip.pitchBytes) * mip.height;
        mip.tail = true;
        mip.tailX = tail.locations[level - firstTailLevel].x;
        mip.tailY = tail.locations[level - firstTailLevel].y;
    }

    std::uint64_t offset = firstTailLevel < mipCount ? block.blockSize : 0;
    for (auto level = firstTailLevel; level-- > 0;) {
        mips[level].tiledOffset = offset;
        mips[level].linearOffset = offset;
        offset += mips[level].tiledSize;
    }
    // Tail mips share one tiled block but each needs its own linear region, placed after the chain.
    std::uint64_t linearCursor = 0;
    for (auto level = std::uint32_t{0}; level < firstTailLevel; ++level) linearCursor = std::max(linearCursor, mips[level].linearOffset + mips[level].linearSize);
    linearCursor = std::max(linearCursor, offset);
    for (auto level = firstTailLevel; level < mipCount; ++level) {
        mips[level].tiledOffset = 0;
        mips[level].linearOffset = linearCursor;
        linearCursor += mips[level].linearSize;
    }

    Require(offset == blockSliceSize, "tiled texture mip chain geometry is inconsistent");
    for (const auto& mip : mips) {
        Require(mip.width != 0 && mip.height != 0, "computed a zero-sized tiled texture mip level");
        Require(mip.tiledSize != 0 && mip.linearSize != 0, "computed a zero-sized tiled texture mip level");
    }
    return mips;
}

}

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    Require(width != 0 && height != 0, "cannot compute mip layout for a zero-sized texture");
    Require(mipCount != 0 && mipCount <= 16u, "texture mip count is out of range");

    const auto bytesPerElement = BytesPerElement(format);
    const auto texelWidth = BlockWidth(format);
    const auto texelHeight = BlockHeight(format);

    if (tileMode == TextureTileMode::kLinear) return ComputeLinearMipLayout(bytesPerElement, texelWidth, texelHeight, width, height, mipCount);
    return ComputeTiledMipLayout(tileMode, bytesPerElement, texelWidth, texelHeight, width, height, mipCount);
}

std::array<std::uint32_t, 3> ThickBlockExtent(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    // addrlib Block4K_Log2_3d / Block64K_Log2_3d.
    constexpr std::uint8_t thick4KB[5][3] = {{4, 4, 4}, {3, 4, 4}, {3, 4, 3}, {3, 3, 3}, {2, 3, 3}};
    constexpr std::uint8_t thick64KB[5][3] = {{6, 5, 5}, {5, 5, 5}, {5, 5, 4}, {5, 4, 4}, {4, 4, 4}};
    Require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "unsupported bytes per element for thick texture geometry");
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    switch (tileMode) {
        case TextureTileMode::kStandard4KB: return {1u << thick4KB[index][0], 1u << thick4KB[index][1], 1u << thick4KB[index][2]};
        case TextureTileMode::kStandard64KB: return {1u << thick64KB[index][0], 1u << thick64KB[index][1], 1u << thick64KB[index][2]};
        default: throw std::runtime_error("AGC graphics: 3D textures are only supported linear or in SW_4KB_S / SW_64KB_S");
    }
}

ThickLayout ComputeThickLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    Require(width != 0 && height != 0 && depth != 0, "cannot compute layout for a zero-sized 3D texture");
    const auto bytesPerElement = BytesPerElement(format);
    Require(BlockWidth(format) == 1u && BlockHeight(format) == 1u, "block-compressed 3D textures are not implemented");
    ThickLayout result{};
    result.depth = depth;
    auto& mip = result.mip;
    mip.width = width;
    mip.height = height;
    mip.tail = false;
    if (tileMode == TextureTileMode::kLinear) {
        const auto paddedWidth = AlignUp(width, CalcLinearBlockWidth(bytesPerElement));
        mip.blocksPerRow = paddedWidth;
        mip.pitchBytes = paddedWidth * bytesPerElement;
        mip.tiledSize = static_cast<std::uint64_t>(mip.pitchBytes) * height;
        result.blockDepth = 1;
        result.slabBytes = mip.tiledSize;
    } else {
        const auto block = ThickBlockExtent(tileMode, bytesPerElement);
        const auto paddedWidth = AlignUp(width, block[0]);
        const auto paddedHeight = AlignUp(height, block[1]);
        mip.blocksPerRow = paddedWidth / block[0];
        mip.pitchBytes = paddedWidth * bytesPerElement;
        result.blockDepth = block[2];
        result.slabBytes = static_cast<std::uint64_t>(mip.blocksPerRow) * (paddedHeight / block[1]) * (tileMode == TextureTileMode::kStandard4KB ? 4096u : 65536u);
        mip.tiledSize = result.slabBytes;
    }
    mip.linearSize = static_cast<std::uint64_t>(mip.pitchBytes) * height;
    result.sliceLinearBytes = mip.linearSize;
    result.guestBytes = result.slabBytes * ((depth + result.blockDepth - 1u) / result.blockDepth);
    return result;
}

SurfaceGeometry DescribeSurface(const GuestTextureResource& descriptor) {
    SurfaceGeometry geometry;
    if (descriptor.dimension == TextureDimension::k3D) {
        Require(descriptor.mipCount == 1, "mipmapped 3D textures are not implemented");
        const auto depth = descriptor.depthOrLastArray + 1u;
        const auto thick = ComputeThickLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, depth);
        geometry.mips = {thick.mip};
        geometry.layers = depth;
        geometry.imageDepth = depth;
        geometry.guestBytes = thick.guestBytes;
        geometry.sliceLinearBytes = thick.sliceLinearBytes;
        geometry.thick = descriptor.tileMode != TextureTileMode::kLinear;
        geometry.blockDepth = thick.blockDepth;
        geometry.layerBytes = thick.slabBytes;
        return geometry;
    }
    geometry.mips = ComputeMipLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, descriptor.mipCount);
    const bool layered = descriptor.dimension == TextureDimension::k2DArray || descriptor.dimension == TextureDimension::kCube;
    geometry.layers = layered ? descriptor.depthOrLastArray + 1u : 1u;
    geometry.imageLayers = geometry.layers;
    geometry.guestBytes = ComputeSurfaceSize(geometry.mips, geometry.layers);
    geometry.layerBytes = geometry.guestBytes / geometry.layers;
    for (const auto& mip : geometry.mips) geometry.sliceLinearBytes = std::max(geometry.sliceLinearBytes, mip.linearOffset + mip.linearSize);
    return geometry;
}

std::vector<TileMipLayout> ComputeElementMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    Require(width != 0 && height != 0 && mipCount != 0 && mipCount <= 16u, "invalid surface mip chain");
    if (tileMode == TextureTileMode::kLinear) return ComputeLinearMipLayout(bytesPerElement, 1u, 1u, width, height, mipCount);
    return ComputeTiledMipLayout(tileMode, bytesPerElement, 1u, 1u, width, height, mipCount);
}

std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers) {
    Require(!mips.empty(), "cannot compute surface size for an empty mip chain");
    Require(arrayLayers != 0, "cannot compute surface size for zero array layers");

    std::uint64_t sliceSize = 0;
    for (const auto& mip : mips) {
        Require(mip.tiledSize != 0, "encountered a zero-sized mip level while computing surface size");
        sliceSize = std::max(sliceSize, mip.tiledOffset + mip.tiledSize);
    }

    Require(sliceSize <= UINT64_MAX / arrayLayers, "tiled texture surface size overflows");
    return sliceSize * arrayLayers;
}

}
