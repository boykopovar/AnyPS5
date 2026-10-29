#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <string>
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
        case TextureTileMode::kStandard256B:
        case TextureTileMode::kD256B: return {256u, 1u << kLog2BlockThin256B[index].width, 1u << kLog2BlockThin256B[index].height};
        case TextureTileMode::kStandard4KB:
        case TextureTileMode::kD4KB:
        case TextureTileMode::kS4KBX:
        case TextureTileMode::kD4KBX: return {4096u, 1u << kLog2BlockThin4KB[index].width, 1u << kLog2BlockThin4KB[index].height};
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kD64KB:
        case TextureTileMode::kS64KBT:
        case TextureTileMode::kD64KBT:
        case TextureTileMode::kZ64KBX:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX:
        case TextureTileMode::kR64KBX: return {65536u, 1u << kLog2BlockThin64KB[index].width, 1u << kLog2BlockThin64KB[index].height};
        case TextureTileMode::kGcnThin1D:
        case TextureTileMode::kGcnDisplay1D:
        case TextureTileMode::kGcnDisplay2D:
        case TextureTileMode::kGcnThin2D:
        case TextureTileMode::kGcnDepth2D64:
        case TextureTileMode::kGcnDepth2D128:
        case TextureTileMode::kGcnDepth2D256:
        case TextureTileMode::kGcnDepth2D512:
        case TextureTileMode::kGcnDepth2D1K:
        case TextureTileMode::kGcnDepthPrt256:
        case TextureTileMode::kGcnDepthPrt1K:
        case TextureTileMode::kGcnDisplayPrt:
        case TextureTileMode::kGcnDisplay2DPrt:
        case TextureTileMode::kGcnThinPrt:
        case TextureTileMode::kGcnThin2DPrt: {
            const auto geometry = GcnTiling(tileMode, bytesPerElement);
            return {geometry.blockBytes, geometry.blockWidth, geometry.blockHeight};
        }
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
        case TextureTileMode::kStandard256B:
        case TextureTileMode::kD256B:
        case TextureTileMode::kGcnThin1D:
        case TextureTileMode::kGcnDisplay1D:
        case TextureTileMode::kGcnDisplay2D:
        case TextureTileMode::kGcnThin2D:
        case TextureTileMode::kGcnDepth2D64:
        case TextureTileMode::kGcnDepth2D128:
        case TextureTileMode::kGcnDepth2D256:
        case TextureTileMode::kGcnDepth2D512:
        case TextureTileMode::kGcnDepth2D1K:
        case TextureTileMode::kGcnDepthPrt256:
        case TextureTileMode::kGcnDepthPrt1K:
        case TextureTileMode::kGcnDisplayPrt:
        case TextureTileMode::kGcnDisplay2DPrt:
        case TextureTileMode::kGcnThinPrt:
        case TextureTileMode::kGcnThin2DPrt: return false;
        case TextureTileMode::kStandard4KB:
        case TextureTileMode::kD4KB:
        case TextureTileMode::kS4KBX:
        case TextureTileMode::kD4KBX:
            out = MakeMipTailLayout(kMipTailThin4KB[index], block.blockWidth >> 1u, block.blockHeight);
            return true;
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kD64KB:
        case TextureTileMode::kS64KBT:
        case TextureTileMode::kD64KBT:
        case TextureTileMode::kZ64KBX:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX:
        case TextureTileMode::kR64KBX:
            out = MakeMipTailLayout(kMipTailThin64KB[index], block.blockWidth >> 1u, block.blockHeight);
            if (tileMode == TextureTileMode::kZ64KBX && bytesPerElement <= 2u) {
                out.widthLimit >>= 1u;
                if (bytesPerElement == 1u) out.heightLimit >>= 1u;
            }
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

    Require(!IsGcnTiled(tileMode), "PS4 surfaces are laid out by ComputeGcnMipLayout from their pitch");
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
        case TextureTileMode::kStandard4KB:
        case TextureTileMode::kS4KBX: return {1u << thick4KB[index][0], 1u << thick4KB[index][1], 1u << thick4KB[index][2]};
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX: return {1u << thick64KB[index][0], 1u << thick64KB[index][1], 1u << thick64KB[index][2]};
        default: throw std::runtime_error("AGC graphics: thick 3D textures are only supported in SW_4KB_S, SW_4KB_S_X, SW_64KB_S, SW_64KB_S_X and SW_64KB_D_X, not in swizzle mode " + std::to_string(EquationSwizzleMode(tileMode)) + " at " + std::to_string(bytesPerElement) + " bytes per element");
    }
}

std::array<std::uint32_t, 3> ThinBlockLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    const auto block = GetBlockLayout(tileMode, bytesPerElement);
    return {block.blockSize, block.blockWidth, block.blockHeight};
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> CoveredMipBytes(TextureTileMode tileMode, std::uint32_t bytesPerElement, const TileMipLayout& mip) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> covered;
    if (mip.tail || mip.width == 0 || mip.height == 0 || mip.gcnSplitStride != 0) return covered;
    const auto add = [&](std::uint64_t begin, std::uint64_t end) {
        end = std::min(end, mip.tiledSize);
        if (begin >= end) return;
        if (!covered.empty() && covered.back().second == begin) covered.back().second = end;
        else covered.emplace_back(begin, end);
    };
    if (tileMode == TextureTileMode::kLinear) {
        const auto rowBytes = static_cast<std::uint64_t>(mip.width) * bytesPerElement;
        for (std::uint32_t row = 0; row < mip.height; ++row) add(static_cast<std::uint64_t>(row) * mip.pitchBytes, static_cast<std::uint64_t>(row) * mip.pitchBytes + rowBytes);
        return covered;
    }
    const auto block = ThinBlockLayout(MipTileMode(tileMode, mip), bytesPerElement);
    const auto wholeColumns = std::min(mip.width / block[1], mip.blocksPerRow);
    const auto wholeRows = mip.height / block[2];
    for (std::uint32_t row = 0; row < wholeRows && wholeColumns != 0; ++row) {
        const auto first = static_cast<std::uint64_t>(row) * mip.blocksPerRow;
        add(first * block[0], (first + wholeColumns) * block[0]);
    }
    return covered;
}

ThickLayout ComputeThickLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount) {
    Require(width != 0 && height != 0 && depth != 0, "cannot compute layout for a zero-sized 3D texture");
    Require(mipCount != 0 && mipCount <= 16u, "3D texture mip count is out of range");
    const auto bytesPerElement = BytesPerElement(format);
    const auto texelWidth = BlockWidth(format);
    const auto texelHeight = BlockHeight(format);
    const auto elementsWidth0 = (width + texelWidth - 1u) / texelWidth;
    const auto elementsHeight0 = (height + texelHeight - 1u) / texelHeight;
    ThickLayout result{};
    result.depth = depth;
    result.mips.resize(mipCount);
    std::array<std::uint32_t, 3> block{CalcLinearBlockWidth(bytesPerElement), 1u, 1u};
    std::uint32_t blockBytes = 0;
    std::uint32_t maxTailLevels = 0;
    auto firstTailLevel = mipCount;
    if (tileMode != TextureTileMode::kLinear) {
        block = ThickBlockExtent(tileMode, bytesPerElement);
        blockBytes = tileMode == TextureTileMode::kStandard4KB || tileMode == TextureTileMode::kS4KBX ? 4096u : 65536u;
        const auto log2BlockBytes = static_cast<std::uint32_t>(std::countr_zero(blockBytes));
        const auto tailWidth = log2BlockBytes % 3u == 1u ? block[0] >> 1u : block[0];
        const auto tailHeight = log2BlockBytes % 3u == 0u ? block[1] >> 1u : block[1];
        const auto effectiveLog2 = log2BlockBytes - (log2BlockBytes - 8u) / 3u;
        maxTailLevels = effectiveLog2 <= 11u ? 1u + (1u << (effectiveLog2 - 9u)) : effectiveLog2 - 4u;
        for (std::uint32_t level = 0; mipCount > 1 && level < mipCount; ++level) {
            if (ShiftCeil(elementsWidth0, level) <= tailWidth && ShiftCeil(elementsHeight0, level) <= tailHeight && mipCount - level <= maxTailLevels) {
                firstTailLevel = level;
                break;
            }
        }
    }
    result.blockDepth = block[2];
    std::uint64_t tiledOffset = firstTailLevel < mipCount ? blockBytes : 0u;
    std::uint64_t linearOffset = 0;
    for (auto level = mipCount; level-- > 0;) {
        auto& mip = result.mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.tail = level >= firstTailLevel;
        mip.tailX = 0;
        mip.tailY = 0;
        const bool tiled = tileMode != TextureTileMode::kLinear;
        const auto levelWidth = std::max(ShiftCeil(elementsWidth0, level), 1u);
        const auto levelHeight = std::max(ShiftCeil(elementsHeight0, level), 1u);
        const auto paddedWidth = AlignUp(levelWidth, block[0]);
        mip.pitchBytes = paddedWidth * bytesPerElement;
        if (mip.tail) {
            constexpr std::uint32_t microBlock[5][2] = {{8, 4}, {4, 4}, {4, 4}, {4, 2}, {2, 2}};
            const auto element = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
            const auto slot = maxTailLevels - 1u - (level - firstTailLevel);
            const auto slotOffset = slot > 6u ? 16u << slot : slot << 8u;
            for (std::uint32_t bit = 0; bit < 6u; ++bit) {
                mip.tailX |= ((slotOffset >> (9u + 2u * bit)) & 1u) << bit;
                mip.tailY |= ((slotOffset >> (8u + 2u * bit)) & 1u) << bit;
            }
            mip.tailX *= microBlock[element][0];
            mip.tailY *= microBlock[element][1];
            mip.blocksPerRow = 1u;
            mip.pitchBytes = block[0] * bytesPerElement;
            mip.tiledOffset = 0;
            mip.tiledSize = blockBytes;
        } else if (!tiled) {
            mip.blocksPerRow = paddedWidth;
            mip.tiledSize = static_cast<std::uint64_t>(mip.pitchBytes) * levelHeight;
        } else {
            mip.blocksPerRow = paddedWidth / block[0];
            mip.tiledSize = static_cast<std::uint64_t>(mip.blocksPerRow) * (AlignUp(levelHeight, block[1]) / block[1]) * blockBytes;
        }
        if (!mip.tail) {
            mip.tiledOffset = tiledOffset;
            tiledOffset += mip.tiledSize;
        }
        mip.linearSize = static_cast<std::uint64_t>(mip.pitchBytes) * mip.height;
        mip.linearOffset = linearOffset;
        linearOffset += mip.linearSize;
    }
    result.slabBytes = tiledOffset;
    result.sliceLinearBytes = linearOffset;
    result.guestBytes = result.slabBytes * ((depth + result.blockDepth - 1u) / result.blockDepth);
    return result;
}

std::vector<TileMipLayout> ComputeGcnMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t pitch, bool pow2Pad, std::uint32_t mipCount, bool linearGeneral, std::uint32_t layers) {
    Require(layers != 0, "a PS4 surface has at least one slice");
    Require(!linearGeneral || tileMode == TextureTileMode::kLinear, "LINEAR_GENERAL is a linear layout");
    Require(width != 0 && height != 0 && pitch >= width, "cannot compute the mip layout of an empty PS4 texture");
    Require(mipCount != 0 && mipCount <= 16u, "texture mip count is out of range");
    Require(tileMode == TextureTileMode::kLinear || IsGcnTiled(tileMode), "PS4 texture tile mode has no modeled layout");
    const auto bytes = BytesPerElement(format);
    const auto texelWidth = BlockWidth(format);
    const auto texelHeight = BlockHeight(format);
    std::vector<TileMipLayout> mips(mipCount);
    std::uint64_t offset = 0, linearOffset = 0;
    for (std::uint32_t level = 0; level < mipCount; ++level) {
        auto levelWidth = std::max(pitch >> level, 1u);
        auto levelHeight = std::max(height >> level, 1u);
        if (pow2Pad && level != 0) {
            levelWidth = std::bit_ceil(levelWidth);
            levelHeight = std::bit_ceil(levelHeight);
        }
        auto rowElements = (levelWidth + texelWidth - 1u) / texelWidth;
        auto rows = (levelHeight + texelHeight - 1u) / texelHeight;
        bool macro = false;
        std::uint32_t macroWidth = 0;
        std::uint32_t splitSlices = 1;
        if (IsGcnMacroTiled(tileMode)) {
            const auto geometry = GcnTiling(tileMode, bytes);
            macro = level == 0 || (rowElements >= geometry.blockWidth && rows >= geometry.blockHeight);
            if (macro) {
                macroWidth = geometry.blockWidth;
                splitSlices = geometry.splitSlices;
                rowElements = AlignUp(rowElements, geometry.blockWidth);
                rows = AlignUp(rows, geometry.blockHeight);
            }
        }
        const bool micro = !macro && tileMode != TextureTileMode::kLinear;
        if (micro) {
            rowElements = AlignUp(rowElements, 8u);
            rows = AlignUp(rows, 8u);
            while (static_cast<std::uint64_t>(rowElements) * rows * bytes % 256u != 0) rowElements += 8u;
        } else if (!macro && !linearGeneral) {
            const auto pitchAlign = std::max(8u, 64u / bytes);
            const auto sliceAlign = std::max(64u, 256u / bytes);
            rowElements = AlignUp(rowElements, pitchAlign);
            while (static_cast<std::uint64_t>(rowElements) * rows % sliceAlign != 0) rowElements += pitchAlign;
        }
        if (level == 0) Require(rowElements == (pitch + texelWidth - 1u) / texelWidth, "PS4 texture pitch " + std::to_string(pitch) + " is not padded the way its tile mode pads a row (" + std::to_string(rowElements * texelWidth) + ")");
        auto& mip = mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.blocksPerRow = macro ? rowElements / macroWidth : micro ? rowElements / 8u : rowElements;
        mip.gcnMicroTiled = micro && IsGcnMacroTiled(tileMode);
        mip.gcnSplitStride = splitSlices > 1u ? static_cast<std::uint32_t>(static_cast<std::uint64_t>(rowElements) * rows * bytes / splitSlices) : 0u;
        mip.pitchBytes = rowElements * bytes;
        mip.tiledSize = static_cast<std::uint64_t>(rowElements) * rows * bytes;
        mip.linearSize = static_cast<std::uint64_t>(mip.pitchBytes) * mip.height;
        mip.tiledOffset = offset;
        mip.linearOffset = linearOffset;
        mip.tail = false;
        mip.tailX = 0;
        mip.tailY = 0;
        offset += mip.tiledSize * layers;
        linearOffset += mip.tiledSize;
    }
    return mips;
}

GcnTileGeometry GcnTiling(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    Require(IsGcnTiled(tileMode), "PS4 tile geometry needs a PS4 tiled mode");
    Require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "unsupported bytes per element for PS4 tiling");
    const auto x = [](std::uint32_t bit) { return 1u << bit; };
    const auto y = [](std::uint32_t bit) { return 1u << (12u + bit); };
    const std::array<std::uint32_t, 6> thin{x(0), y(0), x(1), y(1), x(2), y(2)};
    const std::array<std::array<std::uint32_t, 6>, 5> display{{
        {x(0), x(1), x(2), y(1), y(0), y(2)}, {x(0), x(1), x(2), y(0), y(1), y(2)}, {x(0), x(1), y(0), x(2), y(1), y(2)},
        {x(0), y(0), x(1), x(2), y(1), y(2)}, {x(0), y(0), x(1), y(1), x(2), y(2)},
    }};
    const auto log2Bytes = static_cast<std::uint32_t>(std::countr_zero(bytesPerElement));
    const bool displayOrder = tileMode == TextureTileMode::kGcnDisplay1D || tileMode == TextureTileMode::kGcnDisplay2D || tileMode == TextureTileMode::kGcnDisplayPrt || tileMode == TextureTileMode::kGcnDisplay2DPrt;
    const auto& order = displayOrder ? display[log2Bytes] : thin;
    const auto microBytes = 64u * bytesPerElement;
    GcnTileGeometry geometry{};
    if (IsGcnMicroTiled(tileMode)) {
        geometry = {microBytes, 8u, 8u, {}};
        for (std::uint32_t bit = 0; bit < order.size(); ++bit) geometry.equation[log2Bytes + bit] = order[bit];
        return geometry;
    }
    struct MacroMode { std::uint32_t bankHeight; std::uint32_t banks; std::uint32_t aspect; };
    constexpr std::array<MacroMode, 5> macroModes{{{4u, 16u, 4u}, {2u, 16u, 2u}, {1u, 16u, 2u}, {1u, 16u, 2u}, {1u, 8u, 1u}}};
    constexpr std::array<MacroMode, 5> prtMacroModes{{{8u, 16u, 4u}, {4u, 16u, 4u}, {2u, 16u, 2u}, {1u, 16u, 2u}, {1u, 8u, 1u}}};
    std::uint32_t tileSplit = std::min(1024u, std::max(256u, 2u * microBytes));
    switch (tileMode) {
        case TextureTileMode::kGcnDepth2D64: tileSplit = 64u; break;
        case TextureTileMode::kGcnDepth2D128: tileSplit = 128u; break;
        case TextureTileMode::kGcnDepth2D256:
        case TextureTileMode::kGcnDepthPrt256: tileSplit = 256u; break;
        case TextureTileMode::kGcnDepth2D512: tileSplit = 512u; break;
        case TextureTileMode::kGcnDepth2D1K:
        case TextureTileMode::kGcnDepthPrt1K: tileSplit = 1024u; break;
        default: break;
    }
    const bool prt = IsGcnPrtTiled(tileMode);
    const bool splitRotation = tileMode != TextureTileMode::kGcnDisplayPrt && tileMode != TextureTileMode::kGcnThinPrt;
    const bool pipe8x16 = tileMode == TextureTileMode::kGcnDisplayPrt || tileMode == TextureTileMode::kGcnThinPrt;
    const auto tileBytes = std::min(tileSplit, microBytes);
    const auto log2TileBytes = static_cast<std::uint32_t>(std::countr_zero(tileBytes));
    const auto& mode = (prt ? prtMacroModes : macroModes)[log2TileBytes - 6u];
    constexpr std::uint32_t pipes = 8u, log2Pipes = 3u, pipeInterleaveBits = 8u;
    const auto log2BankHeight = static_cast<std::uint32_t>(std::countr_zero(mode.bankHeight));
    const auto bankBits = static_cast<std::uint32_t>(std::countr_zero(mode.banks));
    geometry.blockWidth = 8u * pipes * mode.aspect;
    geometry.blockHeight = 8u * mode.bankHeight * mode.banks / mode.aspect;
    const auto macroBytes = tileBytes * mode.bankHeight;
    Require(macroBytes % 256u == 0, "PS4 macro tile is not a whole number of pipe interleaves");
    geometry.blockBytes = macroBytes * pipes * mode.banks;
    const auto elementBits = log2TileBytes - log2Bytes;
    geometry.splitSlices = microBytes / tileBytes;
    for (auto bit = elementBits; bit < order.size(); ++bit) geometry.splitMasks[bit - elementBits] = order[bit];
    std::array<std::uint32_t, 16> inner{};
    for (std::uint32_t bit = 0; bit < elementBits; ++bit) inner[log2Bytes + bit] = order[bit];
    auto innerBits = log2TileBytes;
    for (std::uint32_t bit = 0; bit < log2BankHeight; ++bit) inner[innerBits++] = y(3u + bit);
    const auto pipe = pipe8x16 ? std::array<std::uint32_t, 3>{x(4) | y(3) | x(5), x(3) | y(4), x(5) | y(5)} : std::array<std::uint32_t, 3>{x(3) | y(3) | x(4), x(4) | y(4), x(5) | y(5)};
    const auto log2Aspect = static_cast<std::uint32_t>(std::countr_zero(mode.aspect));
    const auto tx = [&](std::uint32_t bit) { return prt && bit >= log2Aspect ? 0u : x(3u + log2Pipes + bit); };
    const auto ty = [&](std::uint32_t bit) { return prt && bit >= bankBits - log2Aspect ? 0u : y(3u + log2BankHeight + bit); };
    std::array<std::uint32_t, 4> bank{};
    switch (mode.banks) {
        case 16u: bank = {tx(0) | ty(3), tx(1) | ty(2) | ty(3), tx(2) | ty(1), tx(3) | ty(0)}; break;
        case 8u: bank = {tx(0) | ty(2), tx(1) | ty(1) | ty(2), tx(2) | ty(0), 0u}; break;
        default: Require(false, "PS4 macro tile bank count is not modeled");
    }
    for (std::uint32_t split = 0; splitRotation && split < 4u && geometry.splitMasks[split] != 0; ++split) {
        const auto rotation = ((mode.banks / 2u + 1u) << split) & (mode.banks - 1u);
        for (std::uint32_t bit = 0; bit < bankBits; ++bit) {
            if ((rotation >> bit) & 1u) bank[bit] ^= geometry.splitMasks[split];
        }
    }
    Require(innerBits + log2Pipes + bankBits <= 16u, "PS4 macro tile exceeds 64 KiB");
    for (std::uint32_t bit = 0; bit < pipeInterleaveBits; ++bit) geometry.equation[bit] = inner[bit];
    for (std::uint32_t bit = 0; bit < log2Pipes; ++bit) geometry.equation[pipeInterleaveBits + bit] = pipe[bit];
    for (std::uint32_t bit = 0; bit < bankBits; ++bit) geometry.equation[pipeInterleaveBits + log2Pipes + bit] = bank[bit];
    for (auto bit = pipeInterleaveBits; bit < innerBits; ++bit) geometry.equation[bit + log2Pipes + bankBits] = inner[bit];
    return geometry;
}

namespace {

SurfaceGeometry describeGcnSurface(const GuestTextureResource& descriptor) {
    Require(descriptor.dimension != TextureDimension::k3D && descriptor.dimension != TextureDimension::k2DMsaa, "PS4 3D and multisampled textures are not implemented");
    const bool layered = descriptor.dimension == TextureDimension::k2DArray || descriptor.dimension == TextureDimension::k1DArray || descriptor.dimension == TextureDimension::kCube;
    SurfaceGeometry geometry;
    geometry.layers = layered ? descriptor.depthOrLastArray + 1u : 1u;
    geometry.levelMajor = geometry.layers > 1u && descriptor.mipCount > 1u;
    geometry.mips = ComputeGcnMipLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, descriptor.gcnPitch, descriptor.gcnPow2Pad, descriptor.mipCount, descriptor.gcnLinearGeneral, geometry.layers);
    geometry.imageLayers = geometry.layers;
    geometry.guestBytes = geometry.mips.back().tiledOffset + geometry.mips.back().tiledSize * geometry.layers;
    geometry.layerBytes = geometry.guestBytes / geometry.layers;
    for (const auto& mip : geometry.mips) geometry.sliceLinearBytes = std::max(geometry.sliceLinearBytes, mip.linearOffset + mip.linearSize);
    return geometry;
}

}

SurfaceGeometry DescribeSurface(const GuestTextureResource& descriptor) {
    if (descriptor.gcnPitch != 0) return describeGcnSurface(descriptor);
    SurfaceGeometry geometry;
    if (descriptor.samples != 1u && descriptor.tileMode != TextureTileMode::kLinear) {
        Require(descriptor.samples == 2u || descriptor.samples == 4u || descriptor.samples == 8u, "multisampled surface fragment count is not implemented");
        Require(descriptor.dimension == TextureDimension::k2DMsaa && descriptor.baseArray == 0u && descriptor.depthOrLastArray == 0u, "multisampled surface dimension or array range is not implemented");
        Require(descriptor.mipCount == 1u && descriptor.allocatedMipCount == 1u && descriptor.baseLevel == 0u && descriptor.lastLevel == 0u, "multisampled surface mip levels are not implemented");
        Require(descriptor.width != 0u && descriptor.width <= 16384u && descriptor.height != 0u && descriptor.height <= 16384u, "invalid multisampled surface extent");
        Require(!IsBlockCompressed(descriptor.format), "multisampled block-compressed surface is not implemented");
    }
    if (descriptor.dimension == TextureDimension::k3D && (descriptor.tileMode == TextureTileMode::kZ64KBX || descriptor.tileMode == TextureTileMode::kR64KBX)) {
        const auto depth = descriptor.depthOrLastArray + 1u;
        geometry.mips = ComputeMipLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, descriptor.mipCount);
        geometry.layers = depth;
        geometry.imageDepth = depth;
        geometry.guestBytes = ComputeSurfaceSize(geometry.mips, depth);
        geometry.layerBytes = geometry.guestBytes / depth;
        for (const auto& mip : geometry.mips) geometry.sliceLinearBytes = std::max(geometry.sliceLinearBytes, mip.linearOffset + mip.linearSize);
        return geometry;
    }
    if (descriptor.dimension == TextureDimension::k3D) {
        const auto depth = descriptor.depthOrLastArray + 1u;
        const auto thick = ComputeThickLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, depth, descriptor.mipCount);
        geometry.mips = thick.mips;
        geometry.layers = depth;
        geometry.imageDepth = depth;
        geometry.guestBytes = thick.guestBytes;
        geometry.sliceLinearBytes = thick.sliceLinearBytes;
        geometry.thick = descriptor.tileMode != TextureTileMode::kLinear;
        geometry.blockDepth = thick.blockDepth;
        geometry.layerBytes = thick.slabBytes;
        return geometry;
    }
    if (descriptor.dimension == TextureDimension::k2DMsaa && descriptor.tileMode != TextureTileMode::kLinear) {
        Require(descriptor.samples > 1u && std::has_single_bit(descriptor.samples), "multisampled texture has an invalid sample count");
        const auto bytesPerElement = BytesPerElement(descriptor.format);
        Require(std::has_single_bit(bytesPerElement), "multisampled texture format has an unsupported element size");
        const auto block = GetBlockLayout(descriptor.tileMode, bytesPerElement);
        Require(block.blockSize == 4096u || block.blockSize == 65536u, "multisampled surface requires 4 KiB or 64 KiB blocks");
        const auto log2Samples = static_cast<std::uint32_t>(std::countr_zero(descriptor.samples));
        const auto log2Pixels = static_cast<std::uint32_t>(std::countr_zero(block.blockSize)) - static_cast<std::uint32_t>(std::countr_zero(bytesPerElement)) - log2Samples;
        const auto log2Width = (log2Pixels + ((log2Samples & 1u) == 0u ? 1u : 0u)) / 2u;
        const auto blockWidth = 1u << log2Width;
        const auto blockHeight = 1u << (log2Pixels - log2Width);
        const auto align = [](std::uint32_t extent, std::uint32_t blockDimension) {
            return (static_cast<std::uint64_t>(extent) + blockDimension - 1u) / blockDimension * blockDimension;
        };
        geometry.guestBytes = align(descriptor.width, blockWidth) * align(descriptor.height, blockHeight) * bytesPerElement * descriptor.samples;
        geometry.layerBytes = geometry.guestBytes;
        geometry.layers = 1;
        geometry.imageLayers = 1;
        geometry.imageDepth = 1;
        geometry.sliceLinearBytes = static_cast<std::uint64_t>(descriptor.width) * descriptor.height * descriptor.samples * bytesPerElement;
        geometry.mips.push_back({0u, geometry.guestBytes, 0u, geometry.sliceLinearBytes, descriptor.width, descriptor.height, static_cast<std::uint32_t>(align(descriptor.width, blockWidth) / blockWidth), descriptor.width * descriptor.samples * bytesPerElement, false, 0u, 0u});
        return geometry;
    }
    geometry.mips = ComputeMipLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, descriptor.mipCount);
    const bool layered = descriptor.dimension == TextureDimension::k2DArray || descriptor.dimension == TextureDimension::k1DArray || descriptor.dimension == TextureDimension::kCube;
    geometry.layers = layered ? descriptor.depthOrLastArray + 1u : 1u;
    geometry.imageLayers = geometry.layers;
    geometry.guestBytes = ComputeSurfaceSize(geometry.mips, geometry.layers);
    geometry.layerBytes = geometry.guestBytes / geometry.layers;
    for (const auto& mip : geometry.mips) geometry.sliceLinearBytes = std::max(geometry.sliceLinearBytes, mip.linearOffset + mip.linearSize);
    return geometry;
}

bool LevelsFitAllocation(const GuestTextureResource& surface, std::uint32_t levels) {
    Require(levels >= surface.mipCount && levels <= 16u, "a view's mip chain must cover the surface's levels and at most 16");
    auto view = surface;
    view.mipCount = levels;
    const auto allocated = DescribeSurface(surface);
    const auto extended = DescribeSurface(view);
    if (allocated.guestBytes != extended.guestBytes || allocated.layerBytes != extended.layerBytes || allocated.layers != extended.layers) return false;
    for (std::uint32_t level = 0; level < surface.mipCount; ++level) {
        const auto& before = allocated.mips[level];
        const auto& after = extended.mips[level];
        if (before.tiledOffset != after.tiledOffset || before.tiledSize != after.tiledSize || before.blocksPerRow != after.blocksPerRow || before.tail != after.tail || before.tailX != after.tailX || before.tailY != after.tailY) return false;
    }
    for (auto level = surface.mipCount; level < levels; ++level) {
        const auto& mip = extended.mips[level];
        if (mip.tiledOffset + mip.tiledSize > extended.layerBytes) return false;
    }
    return true;
}

GuestTextureResource ViewedArrayWindow(const GuestTextureResource& viewed, bool keysReadAsStored) {
    const bool layered = viewed.dimension == TextureDimension::k2DArray || viewed.dimension == TextureDimension::k1DArray || viewed.dimension == TextureDimension::kCube;
    if (!layered || viewed.baseArray == 0 || viewed.baseArray > viewed.depthOrLastArray || (viewed.dccAddress != 0 && !keysReadAsStored)) return viewed;
    std::uint32_t sliceMask = 0;
    if (const auto xorMode = XorSwizzleMode(viewed.tileMode); xorMode != 0u) {
        const auto* equation = FindTextureSwizzleEquation(xorMode, BytesPerElement(viewed.format));
        if (equation == nullptr) return viewed;
        std::uint32_t sliceBits = 0;
        for (const auto bit : equation->bits) sliceBits |= bit >> 24u;
        sliceMask = sliceBits == 0 ? 0u : (1u << std::bit_width(sliceBits)) - 1u;
    }
    const auto first = viewed.baseArray & ~sliceMask;
    if (first == 0) return viewed;
    const auto geometry = DescribeSurface(viewed);
    if (geometry.thick || geometry.layers != viewed.depthOrLastArray + 1u) return viewed;
    auto window = viewed;
    window.baseAddress = viewed.baseAddress + geometry.GuestLayerOffset(first);
    window.depthOrLastArray = viewed.depthOrLastArray - first;
    window.baseArray = viewed.baseArray - first;
    window.dccAddress = 0;
    return window;
}

GuestTextureResource ArrayLayerWindow(const GuestTextureResource& viewed, std::uint32_t maxArrayLayers) {
    const bool layered = viewed.dimension == TextureDimension::k2DArray || viewed.dimension == TextureDimension::k1DArray || viewed.dimension == TextureDimension::kCube;
    if (viewed.depthOrLastArray < maxArrayLayers || !layered) return viewed;
    Require(viewed.dimension != TextureDimension::kCube, "an image with more layers than maxImageArrayLayers that is not a 1D or 2D array is not implemented");
    Require(viewed.baseArray <= viewed.depthOrLastArray, "an image view's first array slice lies past its last");
    Require(viewed.depthOrLastArray - viewed.baseArray < maxArrayLayers, "an image view of more array slices than maxImageArrayLayers is not implemented");
    if (const auto xorMode = XorSwizzleMode(viewed.tileMode); xorMode != 0u) {
        const auto* equation = FindTextureSwizzleEquation(xorMode, BytesPerElement(viewed.format));
        Require(equation != nullptr && std::none_of(equation->bits.begin(), equation->bits.end(), [](std::uint32_t bit) { return (bit & 0xff000000u) != 0; }), "an image view past maxImageArrayLayers of a swizzle that depends on the slice is not implemented");
    }
    const auto geometry = DescribeSurface(viewed);
    Require(!geometry.thick && geometry.layers == viewed.depthOrLastArray + 1u, "an array past maxImageArrayLayers must be a thin array");
    auto window = viewed;
    window.baseAddress = viewed.baseAddress + geometry.GuestLayerOffset(viewed.baseArray);
    window.depthOrLastArray = viewed.depthOrLastArray - viewed.baseArray;
    window.baseArray = 0;
    return window;
}

std::vector<TileMipLayout> ComputeElementMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    Require(width != 0 && height != 0 && mipCount != 0 && mipCount <= 16u, "invalid surface mip chain");
    Require(!IsGcnTiled(tileMode), "PS4 surfaces are laid out by ComputeGcnMipLayout from their pitch");
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
