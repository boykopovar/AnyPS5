#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using Testing::Case;
using Testing::Require;

struct ElementAddress {
    std::uint32_t mip;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
    std::uint64_t address;
};

std::uint64_t equationOffset(const TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12) & (mask & 0xfff000u)) ^ ((z << 24) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

GuestTextureResource thickVolume(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount) {
    GuestTextureResource resource{};
    resource.width = width;
    resource.height = height;
    resource.depthOrLastArray = depth - 1u;
    resource.mipCount = mipCount;
    resource.tileMode = tileMode;
    resource.dimension = TextureDimension::k3D;
    resource.format = format;
    return resource;
}

void requireThickAddresses(const GuestTextureResource& resource, std::uint32_t bytesPerElement, std::uint32_t firstTailLevel, std::uint64_t guestBytes, std::span<const ElementAddress> expected, const std::string& what) {
    const auto geometry = DescribeSurface(resource);
    Require(geometry.thick && geometry.mips.size() == resource.mipCount && geometry.guestBytes == guestBytes, what + ": guest size differs from addrlib");
    for (std::uint32_t level = 0; level < resource.mipCount; ++level) Require(geometry.mips[level].tail == (level >= firstTailLevel), what + ": mip " + std::to_string(level) + " is on the wrong side of the mip tail");
    const auto blockBytes = resource.tileMode == TextureTileMode::kStandard4KB ? 4096u : 65536u;
    const auto* equation = FindTextureSwizzleEquation(0x100u | (resource.tileMode == TextureTileMode::kStandard4KB ? 5u : resource.tileMode == TextureTileMode::kStandard64KB ? 9u : XorSwizzleMode(resource.tileMode)), bytesPerElement);
    Require(equation != nullptr, what + ": missing thick swizzle equation");
    const auto block = ThickBlockExtent(resource.tileMode, bytesPerElement);
    for (const auto& element : expected) {
        Require(geometry.HasLayer(element.mip, element.z), what + ": sample slice lies outside its level");
        const auto& mip = geometry.mips.at(element.mip);
        auto address = geometry.GuestLayerOffset(element.z) + mip.tiledOffset;
        if (mip.tail) {
            address += equationOffset(*equation, element.x + mip.tailX, element.y + mip.tailY, element.z);
        } else {
            address += (static_cast<std::uint64_t>(element.y / block[1]) * mip.blocksPerRow + element.x / block[0]) * blockBytes + equationOffset(*equation, element.x, element.y, element.z);
        }
        Require(address == element.address, what + ": mip " + std::to_string(element.mip) + " element (" + std::to_string(element.x) + ", " + std::to_string(element.y) + ", " + std::to_string(element.z) + ") detiles from " + std::to_string(address) + " instead of addrlib's " + std::to_string(element.address));
    }
}

void requireBlockVolumeLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t blockFormat, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount, const std::string& what) {
    const auto compressed = DescribeSurface(thickVolume(tileMode, format, width, height, depth, mipCount));
    const auto blocks = DescribeSurface(thickVolume(tileMode, blockFormat, (width + 3u) / 4u, (height + 3u) / 4u, depth, mipCount));
    Require(compressed.thick == blocks.thick && compressed.guestBytes == blocks.guestBytes && compressed.layerBytes == blocks.layerBytes && compressed.blockDepth == blocks.blockDepth, what + ": the volume is not tiled as a volume of its blocks");
    for (std::uint32_t level = 0; level < mipCount; ++level) {
        const auto& mip = compressed.mips[level];
        const auto& block = blocks.mips[level];
        Require(mip.blocksPerRow == block.blocksPerRow && mip.pitchBytes == block.pitchBytes && mip.tiledOffset == block.tiledOffset && mip.tiledSize == block.tiledSize && mip.tail == block.tail && mip.tailX == block.tailX && mip.tailY == block.tailY, what + ": mip " + std::to_string(level) + " is not tiled as the same mip of a volume of its blocks");
        Require(mip.width == std::max((std::max(width >> level, 1u) + 3u) / 4u, 1u) && mip.height == std::max((std::max(height >> level, 1u) + 3u) / 4u, 1u), what + ": mip " + std::to_string(level) + " is not sized in blocks");
        Require(mip.linearSize == static_cast<std::uint64_t>(mip.pitchBytes) * mip.height, what + ": mip " + std::to_string(level) + " linear region does not cover its block rows");
    }
}

void requireThinAddresses(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount, std::uint32_t slices, std::uint64_t guestBytes, std::uint64_t layerBytes, std::span<const ElementAddress> expected, const std::string& what) {
    GuestTextureResource resource{};
    resource.width = width;
    resource.height = height;
    resource.depthOrLastArray = slices - 1u;
    resource.mipCount = mipCount;
    resource.tileMode = tileMode;
    resource.dimension = slices > 1u ? TextureDimension::k2DArray : TextureDimension::k2D;
    resource.format = format;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.guestBytes == guestBytes && geometry.layerBytes == layerBytes, what + ": surface or slice size differs from addrlib");
    const auto bytesPerElement = BytesPerElement(format);
    const auto* equation = FindTextureSwizzleEquation(EquationSwizzleMode(tileMode), bytesPerElement);
    Require(equation != nullptr, what + ": missing swizzle equation");
    const auto block = ThinBlockLayout(tileMode, bytesPerElement);
    for (const auto& element : expected) {
        const auto& mip = geometry.mips.at(element.mip);
        auto address = geometry.GuestLayerOffset(element.z) + mip.tiledOffset;
        if (mip.tail) {
            address += equationOffset(*equation, element.x + mip.tailX, element.y + mip.tailY, element.z);
        } else {
            address += (static_cast<std::uint64_t>(element.y / block[2]) * mip.blocksPerRow + element.x / block[1]) * block[0] + equationOffset(*equation, element.x, element.y, element.z);
        }
        Require(address == element.address, what + ": mip " + std::to_string(element.mip) + " element (" + std::to_string(element.x) + ", " + std::to_string(element.y) + ", " + std::to_string(element.z) + ") detiles from " + std::to_string(address) + " instead of addrlib's " + std::to_string(element.address));
    }
}

void requireSliceAddresses(const GuestTextureResource& resource, std::uint32_t bytesPerElement, std::uint64_t guestBytes, std::span<const ElementAddress> expected, const std::string& what) {
    const auto geometry = DescribeSurface(resource);
    Require(!geometry.thick && geometry.mips.size() == resource.mipCount && geometry.guestBytes == guestBytes, what + ": guest size differs from addrlib");
    const auto* equation = FindTextureSwizzleEquation(XorSwizzleMode(resource.tileMode), bytesPerElement);
    Require(equation != nullptr, what + ": missing swizzle equation");
    const auto block = ThinBlockLayout(resource.tileMode, bytesPerElement);
    for (const auto& element : expected) {
        Require(geometry.HasLayer(element.mip, element.z), what + ": sample slice lies outside its level");
        const auto& mip = geometry.mips.at(element.mip);
        auto address = geometry.GuestLayerOffset(element.z) + mip.tiledOffset;
        if (mip.tail) {
            address += equationOffset(*equation, element.x + mip.tailX, element.y + mip.tailY, element.z);
        } else {
            address += (static_cast<std::uint64_t>(element.y / block[2]) * mip.blocksPerRow + element.x / block[1]) * block[0] + equationOffset(*equation, element.x, element.y, element.z);
        }
        Require(address == element.address, what + ": mip " + std::to_string(element.mip) + " element (" + std::to_string(element.x) + ", " + std::to_string(element.y) + ", " + std::to_string(element.z) + ") detiles from " + std::to_string(address) + " instead of addrlib's " + std::to_string(element.address));
    }
}


const Case linearLayouts{"ComputeMipLayout_LinearTextures_KeepLinearAndTiledLayoutIdentical", [] {
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 2);
        Require(mips.size() == 2, "linear mip chain must contain the requested mip count");
        Require(mips[0].tiledOffset == 512 && mips[0].tiledSize == 1024, "linear mip 0 offset or size changed");
        Require(mips[0].width == 4 && mips[0].height == 4, "linear mip 0 dimensions changed");
        Require(mips[0].blocksPerRow == 256 && mips[0].pitchBytes == 256, "linear mip 0 row layout changed");
        Require(!mips[0].tail, "linear mips must never fall into a mip tail");
        Require(mips[1].tiledOffset == 0 && mips[1].tiledSize == 512, "linear mip 1 offset or size changed");
        Require(mips[1].width == 2 && mips[1].height == 2, "linear mip 1 dimensions changed");
        Require(mips[1].linearOffset == mips[1].tiledOffset && mips[1].linearSize == mips[1].tiledSize, "linear tiling must keep linear and tiled layout identical");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kLinear, 169, 8, 8, 1);
        Require(mips.size() == 1, "compressed linear layout must contain one mip");
        Require(mips[0].width == 2 && mips[0].height == 2, "compressed linear mip block dimensions changed");
        Require(mips[0].blocksPerRow == 32 && mips[0].pitchBytes == 256, "compressed linear mip row layout changed");
        Require(mips[0].tiledSize == 512 && mips[0].linearSize == 512, "compressed linear mip size changed");
    }
}};

const Case standard256B{"ComputeMipLayout_Standard256B_NeverUsesAMipTail", [] {
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard256B, 1, 64, 64, 1);
        Require(mips.size() == 1, "standard 256B layout must contain one mip");
        Require(mips[0].tiledOffset == 0 && mips[0].tiledSize == 4096, "standard 256B mip 0 offset or size changed");
        Require(mips[0].width == 64 && mips[0].height == 64, "standard 256B mip 0 dimensions changed");
        Require(mips[0].blocksPerRow == 4 && mips[0].pitchBytes == 64, "standard 256B mip 0 row layout changed");
        Require(!mips[0].tail, "standard 256B textures must never use a mip tail");

        const auto surfaceSize = ComputeSurfaceSize(mips, 3);
        Require(surfaceSize == 4096ull * 3ull, "surface size must multiply the slice size by the array layer count");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard256B, 1, 32, 32, 6);
        Require(mips.size() == 6, "standard 256B mip chain must contain the requested mip count");
        for (const auto& mip : mips) Require(!mip.tail, "standard 256B tile mode must never produce a mip tail");
    }
}};

const Case deepStandardChains{"ComputeMipLayout_DeepStandardChains_FallIntoTheMipTail", [] {
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard64KB, 1, 1024, 1024, 11);
        Require(mips.size() == 11, "standard 64KB mip chain must contain the requested mip count");
        auto tailSeen = false;
        for (const auto& mip : mips) {
            Require(mip.width != 0 && mip.height != 0, "every standard 64KB mip must have nonzero dimensions");
            Require(mip.tiledSize != 0 && mip.linearSize != 0, "every standard 64KB mip must have a nonzero size");
            if (mip.tail) {
                tailSeen = true;
                Require(mip.blocksPerRow == 1, "mip tail levels must report a single block per row");
                Require(mip.tiledOffset == 0, "mip tail levels must share the tiled tail block offset");
            }
        }
        Require(tailSeen, "a deep standard 64KB mip chain must fall into the mip tail");
        Require(!mips.front().tail, "the base level of a deep mip chain must not be in the mip tail");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard4KB, 1, 512, 512, 10);
        Require(mips.size() == 10, "standard 4KB mip chain must contain the requested mip count");
        auto tailSeen = false;
        for (const auto& mip : mips) {
            if (mip.tail) tailSeen = true;
        }
        Require(tailSeen, "a deep standard 4KB mip chain must fall into the mip tail");
    }
}};

const Case renderTargetLayouts{"ComputeMipLayout_RenderTarget64KB_PadsToWholeBlocks", [] {
    {
        const auto mips = ComputeMipLayout(TextureTileMode::RenderTarget64KB, 56, 257, 129, 1);
        Require(mips[0].blocksPerRow == 3 && mips[0].tiledSize == 393216, "render target surfaces must pad to complete 128 by 128 blocks for 32-bit pixels");
        Require(mips[0].pitchBytes == 1536 && mips[0].linearSize == 1536u * 129u, "detiled render target rows must span the padded block width");
        Require(ComputeSurfaceSize(mips, 6) == 2359296, "render target cube faces must retain the padded guest slice stride");
    }
    for (const auto format : std::array<std::uint32_t, 5>{1, 7, 56, 71, 77}) {
        const auto mips = ComputeMipLayout(TextureTileMode::RenderTarget64KB, format, 1024, 513, 11);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        bool tailSeen = false;
        for (const auto& mip : mips) {
            Require(mip.linearOffset % 4 == 0, "detiled mip levels must start word-aligned");
            Require(mip.linearSize >= static_cast<std::uint64_t>(mip.pitchBytes) * mip.height, "detiled mip allocation must contain every row");
            Require(mip.linearSize % 4 == 0, "detiled mip sizes must preserve word alignment between array layers");
            ranges.emplace_back(mip.linearOffset, mip.linearOffset + mip.linearSize);
            if (mip.tail) {
                tailSeen = true;
                Require(mip.tiledOffset == 0 && mip.tiledSize == 65536, "render target mip tails must share one guest 64KB block");
            }
        }
        std::sort(ranges.begin(), ranges.end());
        for (std::size_t index = 1; index < ranges.size(); ++index) Require(ranges[index].first >= ranges[index - 1].second, "detiled mip levels must occupy separate ranges");
        Require(tailSeen && !mips.front().tail, "render target mip chains must cover both regular blocks and mip tails");
    }
    const auto compressed = ComputeMipLayout(TextureTileMode::RenderTarget64KB, 169, 64, 64, 1);
    Require(compressed.size() == 1 && compressed[0].tiledSize == 65536 && compressed[0].linearSize != 0, "block compressed render target layout is wrong");
    Require(ComputeMipLayout(TextureTileMode::RenderTarget64KB, 132, 64, 64, 1).size() == 1, "format 132 render target layout is missing");
    RequireRejection([] { ComputeMipLayout(TextureTileMode::RenderTarget64KB, 74, 64, 64, 1); }, "unsupported bytes per element");
}};

const Case invalidLayouts{"ComputeMipLayout_InvalidDimensionsMipCountsOrLayers_AreRejected", [] {
    RequireRejection([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 0, 4, 1); }, "zero-sized texture");
    RequireRejection([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 0, 1); }, "zero-sized texture");
    RequireRejection([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 0); }, "mip count is out of range");
    RequireRejection([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 17); }, "mip count is out of range");
    RequireRejection([] { ComputeSurfaceSize({}, 1); }, "empty mip chain");
    RequireRejection([] { ComputeSurfaceSize(ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 1), 0); }, "zero array layers");
}};

const Case mipmappedVolume{"DescribeSurface_MipmappedVolume_PlacesEveryMipInEachSlab", [] {
    GuestTextureResource volume{};
    volume.width = 64;
    volume.height = 64;
    volume.depthOrLastArray = 31;
    volume.mipCount = 3;
    volume.tileMode = TextureTileMode::kStandard4KB;
    volume.dimension = TextureDimension::k3D;
    volume.format = 56;
    const auto geometry = DescribeSurface(volume);
    Require(geometry.thick && geometry.blockDepth == 8 && geometry.mips.size() == 3, "mipmapped 3D texture geometry changed");
    Require(geometry.mips[2].tiledOffset == 0 && geometry.mips[2].tiledSize == 8192, "3D mip 2 must lead each slab");
    Require(geometry.mips[1].tiledOffset == 8192 && geometry.mips[1].tiledSize == 32768, "3D mip 1 offset or size changed");
    Require(geometry.mips[0].tiledOffset == 40960 && geometry.mips[0].tiledSize == 131072, "3D mip 0 must end each slab");
    Require(geometry.layerBytes == 172032 && geometry.guestBytes == 172032ull * 4, "3D slabs must hold the whole mip chain");
    Require(geometry.HasLayer(1, 15) && !geometry.HasLayer(1, 16) && !geometry.HasLayer(2, 8), "3D mips must halve their depth");
    volume.width = 33;
    volume.height = 20;
    volume.depthOrLastArray = 11;
    volume.mipCount = 2;
    const auto odd = DescribeSurface(volume);
    Require(odd.mips[1].width == 16 && odd.mips[1].blocksPerRow == 3 && odd.mips[1].tiledSize == 12288, "a 3D level must be padded from its size rounded up, as addrlib does");
    Require(odd.mips[0].blocksPerRow == 5 && odd.mips[0].tiledOffset == 12288 && odd.mips[0].tiledSize == 40960, "3D mip 0 of a non-power-of-two volume changed");
}};

const Case thickVolumes{"DescribeSurface_ThickVolumes_DetileAtAddrlibAddresses", [] {
    constexpr ElementAddress chainInTail[] = {{0, 0, 0, 0, 0x800}, {0, 7, 7, 7, 0xffc}, {0, 4, 2, 4, 0xe20}, {0, 2, 7, 0, 0x968}, {0, 7, 0, 7, 0xed4}, {1, 0, 0, 0, 0x300}, {1, 3, 3, 3, 0x3fc}, {1, 2, 1, 2, 0x3c8}, {1, 1, 3, 0, 0x32c}, {1, 3, 0, 3, 0x3d4}};
    requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 56, 8, 8, 8, 2), 4, 0, 4096, chainInTail, "SW_4KB_S 32 bpp 8x8x8, 2 levels");
    requireBlockVolumeLayout(TextureTileMode::kStandard4KB, 175, 71, 128, 128, 128, 8, "SW_4KB_S BC4 128x128x128, 8 levels");
    requireBlockVolumeLayout(TextureTileMode::kStandard64KB, 181, 77, 130, 66, 12, 6, "SW_64KB_S BC7 130x66x12, 6 levels");
    requireBlockVolumeLayout(TextureTileMode::kS64KBX, 169, 71, 60, 36, 20, 5, "SW_64KB_S_X BC1 60x36x20, 5 levels");
    constexpr ElementAddress unevenTail[] = {{0, 0, 0, 0, 0x6000}, {0, 32, 19, 11, 0x1f0b8}, {0, 16, 6, 6, 0x85a0}, {0, 11, 19, 0, 0xc06c}, {0, 32, 0, 11, 0x1a090}, {1, 0, 0, 0, 0x3000}, {1, 15, 9, 5, 0x4e5c}, {1, 8, 3, 3, 0x40b8}, {1, 5, 9, 0, 0x3a0c}, {1, 15, 0, 5, 0x4654}, {2, 0, 0, 0, 0x1000}, {2, 7, 4, 2, 0x13c4}, {2, 4, 1, 1, 0x1218}, {2, 2, 4, 0, 0x1140}, {2, 7, 0, 2, 0x12c4}, {3, 0, 0, 0, 0x800}, {3, 3, 1, 0, 0x84c}, {3, 2, 0, 0, 0x840}, {3, 1, 1, 0, 0x80c}, {3, 3, 0, 0, 0x844}, {4, 0, 0, 0, 0x300}, {4, 1, 0, 0, 0x304}, {4, 0, 0, 0, 0x300}, {5, 0, 0, 0, 0x200}};
    requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 56, 33, 20, 12, 6), 4, 3, 131072, unevenTail, "SW_4KB_S 32 bpp 33x20x12, 6 levels");
    constexpr ElementAddress wideTail[] = {{0, 0, 0, 0, 0x20000}, {0, 39, 23, 19, 0xb0bf8}, {0, 20, 8, 10, 0x2e280}, {0, 13, 23, 0, 0x41b28}, {0, 39, 0, 19, 0x902d8}, {1, 0, 0, 0, 0x10000}, {1, 19, 11, 9, 0x1e178}, {1, 10, 4, 5, 0x11c50}, {1, 6, 11, 0, 0x14360}, {1, 19, 0, 9, 0x1a058}, {2, 0, 0, 0, 0x8000}, {2, 9, 5, 4, 0x9c28}, {2, 5, 2, 2, 0x8388}, {2, 3, 5, 0, 0x8868}, {2, 9, 0, 4, 0x9408}, {3, 0, 0, 0, 0x4000}, {3, 4, 2, 1, 0x4310}, {3, 2, 1, 1, 0x4070}, {3, 1, 2, 0, 0x4108}, {3, 4, 0, 1, 0x4210}, {4, 0, 0, 0, 0x1000}, {4, 1, 0, 0, 0x1008}, {5, 0, 0, 0, 0xa00}};
    requireThickAddresses(thickVolume(TextureTileMode::kStandard64KB, 71, 40, 24, 20, 6), 8, 2, 786432, wideTail, "SW_64KB_S 64 bpp 40x24x20, 6 levels");
    constexpr ElementAddress byteTail[] = {{0, 0, 0, 0, 0x20000}, {0, 99, 59, 69, 0x11c8af}, {0, 50, 20, 35, 0x8d116}, {0, 33, 59, 0, 0x4c829}, {0, 99, 0, 69, 0xf8087}, {1, 0, 0, 0, 0x10000}, {1, 49, 29, 34, 0x7d919}, {1, 25, 10, 17, 0x13a25}, {1, 16, 29, 0, 0x15908}, {1, 49, 0, 34, 0x79011}, {2, 0, 0, 0, 0x8000}, {2, 24, 14, 16, 0xbb20}, {2, 12, 5, 8, 0x8748}, {2, 8, 14, 0, 0x8b20}, {2, 24, 0, 16, 0xb200}, {3, 0, 0, 0, 0x4000}, {3, 11, 6, 7, 0x43b7}, {3, 6, 2, 4, 0x40e2}, {3, 4, 6, 0, 0x4160}, {3, 11, 0, 7, 0x4297}, {4, 0, 0, 0, 0x1000}, {4, 5, 2, 3, 0x1075}, {4, 3, 1, 2, 0x101b}, {4, 2, 2, 0, 0x1022}, {4, 5, 0, 3, 0x1055}, {5, 0, 0, 0, 0xa00}, {5, 2, 0, 1, 0xa06}, {5, 1, 0, 0, 0xa01}, {6, 0, 0, 0, 0x900}};
    requireThickAddresses(thickVolume(TextureTileMode::kStandard64KB, 1, 100, 60, 70, 7), 1, 2, 1179648, byteTail, "SW_64KB_S 8 bpp 100x60x70, 7 levels");
    constexpr ElementAddress wideElementTail[] = {{0, 0, 0, 0, 0x3000}, {0, 8, 4, 2, 0x5880}, {0, 4, 1, 1, 0x4030}, {0, 3, 4, 0, 0x3a40}, {0, 8, 0, 2, 0x5080}, {1, 0, 0, 0, 0x1000}, {1, 3, 1, 0, 0x1260}, {1, 2, 0, 0, 0x1200}, {1, 1, 1, 0, 0x1060}, {1, 3, 0, 0, 0x1240}, {2, 0, 0, 0, 0x800}, {2, 1, 0, 0, 0x840}, {3, 0, 0, 0, 0x300}};
    requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 77, 9, 5, 3, 4), 16, 2, 24576, wideElementTail, "SW_4KB_S 128 bpp 9x5x3, 4 levels");
    constexpr ElementAddress xorZx32[] = {{0, 0, 0, 0, 0x8400}, {0, 39, 23, 19, 0x1382fc}, {0, 20, 8, 10, 0xab240}, {0, 13, 23, 0, 0x87ec}, {0, 39, 0, 19, 0x138054}, {1, 0, 0, 0, 0x4800}, {1, 19, 11, 9, 0x9723c}, {1, 10, 4, 5, 0x54390}, {1, 6, 11, 0, 0x5978}, {1, 19, 0, 9, 0x96314}, {2, 0, 0, 0, 0x800}, {2, 9, 5, 4, 0x40b8c}, {2, 5, 2, 2, 0x20c64}, {2, 3, 5, 0, 0x89c}, {2, 9, 0, 4, 0x40b04}, {3, 0, 0, 0, 0x400}, {3, 4, 2, 1, 0x10c60}, {3, 2, 1, 1, 0x10c18}, {3, 1, 2, 0, 0x424}, {3, 4, 0, 1, 0x10c40}};
    requireSliceAddresses(thickVolume(TextureTileMode::kZ64KBX, 56, 40, 24, 20, 4), 4, 1310720, xorZx32, "thin SW_64KB_Z_X 32 bpp 40x24x20, 4 levels");
    constexpr ElementAddress xorSx8[] = {{0, 0, 0, 0, 0x20000}, {0, 99, 59, 69, 0x11caaf}, {0, 50, 20, 35, 0x8db16}, {0, 33, 59, 0, 0x4ca29}, {0, 99, 0, 69, 0xf8487}, {1, 0, 0, 0, 0x10000}, {1, 49, 29, 34, 0x7d319}, {1, 25, 10, 17, 0x13a25}, {1, 16, 29, 0, 0x15508}, {1, 49, 0, 34, 0x79e11}, {2, 0, 0, 0, 0x8400}, {2, 24, 14, 16, 0xbf20}, {2, 12, 5, 8, 0x8348}, {2, 8, 14, 0, 0x8f20}, {2, 24, 0, 16, 0xb600}, {3, 0, 0, 0, 0x4400}, {3, 11, 6, 7, 0x47b7}, {3, 6, 2, 4, 0x44e2}, {3, 4, 6, 0, 0x4560}, {3, 11, 0, 7, 0x4697}, {4, 0, 0, 0, 0x1800}, {4, 5, 2, 3, 0x1875}, {4, 3, 1, 2, 0x181b}, {4, 2, 2, 0, 0x1822}, {4, 5, 0, 3, 0x1855}, {5, 0, 0, 0, 0xa00}, {5, 2, 0, 1, 0xa06}, {5, 1, 0, 1, 0xa05}, {5, 1, 0, 0, 0xa01}, {5, 2, 0, 1, 0xa06}, {6, 0, 0, 0, 0x900}, {6, 0, 0, 0, 0x900}, {6, 0, 0, 0, 0x900}, {6, 0, 0, 0, 0x900}, {6, 0, 0, 0, 0x900}};
    requireThickAddresses(thickVolume(TextureTileMode::kS64KBX, 1, 100, 60, 70, 7), 1, 2, 1179648, xorSx8, "SW_64KB_S_X 8 bpp 100x60x70, 7 levels");
    constexpr ElementAddress xorSx128[] = {{0, 0, 0, 0, 0x10000}, {0, 8, 4, 2, 0x18c80}, {0, 4, 1, 1, 0x11830}, {0, 3, 4, 0, 0x10a40}, {0, 8, 0, 2, 0x18480}, {1, 0, 0, 0, 0x8400}, {1, 3, 1, 0, 0x8660}, {1, 2, 0, 0, 0x8600}, {1, 1, 1, 0, 0x8460}, {1, 3, 0, 0, 0x8640}, {2, 0, 0, 0, 0x4400}, {2, 1, 0, 0, 0x4440}, {2, 1, 0, 0, 0x4440}, {2, 0, 0, 0, 0x4400}, {2, 1, 0, 0, 0x4440}, {3, 0, 0, 0, 0x1800}, {3, 0, 0, 0, 0x1800}, {3, 0, 0, 0, 0x1800}, {3, 0, 0, 0, 0x1800}, {3, 0, 0, 0, 0x1800}};
    requireThickAddresses(thickVolume(TextureTileMode::kS64KBX, 77, 9, 5, 3, 4), 16, 1, 131072, xorSx128, "SW_64KB_S_X 128 bpp 9x5x3, 4 levels");
    {
        const auto linear = DescribeSurface(thickVolume(TextureTileMode::kLinear, 1, 100, 60, 8, 4));
        constexpr std::uint64_t offsets[] = {0x3500u, 0x1700u, 0x800u, 0u};
        Require(!linear.thick && linear.layerBytes == 28928u && linear.guestBytes == 231424u, "linear 8 bpp 100x60x8, 4 levels: slice size differs from addrlib");
        for (std::uint32_t level = 0; level < 4u; ++level) Require(linear.mips[level].tiledOffset == offsets[level], "linear 8 bpp 100x60x8, 4 levels: mip " + std::to_string(level) + " offset differs from addrlib");
    }
    constexpr ElementAddress xorZx64[] = {{0, 0, 0, 0, 0x8400}, {0, 32, 19, 11, 0xbc350}, {0, 16, 6, 6, 0x69040}, {0, 11, 19, 0, 0xe778}, {0, 32, 0, 11, 0xb8100}, {1, 0, 0, 0, 0x400}, {1, 15, 9, 5, 0x52eb8}, {1, 8, 3, 3, 0x32950}, {1, 5, 9, 0, 0x598}, {1, 15, 0, 5, 0x52fa8}, {2, 0, 0, 0, 0x800}, {2, 7, 4, 2, 0x21ca8}, {2, 4, 1, 1, 0x10090}, {2, 2, 4, 0, 0x1820}, {2, 7, 0, 2, 0x20ca8}, {3, 0, 0, 0, 0x4200}, {3, 3, 1, 0, 0x4238}, {3, 2, 0, 0, 0x4220}, {3, 1, 1, 0, 0x4218}, {3, 3, 0, 0, 0x4228}, {4, 0, 0, 0, 0x200}, {4, 1, 0, 0, 0x208}, {4, 1, 0, 0, 0x208}, {4, 0, 0, 0, 0x200}, {4, 1, 0, 0, 0x208}, {5, 0, 0, 0, 0x2000}, {5, 0, 0, 0, 0x2000}, {5, 0, 0, 0, 0x2000}, {5, 0, 0, 0, 0x2000}, {5, 0, 0, 0, 0x2000}};
    requireSliceAddresses(thickVolume(TextureTileMode::kZ64KBX, 71, 33, 20, 12, 6), 8, 786432, xorZx64, "thin SW_64KB_Z_X 64 bpp 33x20x12, 6 levels");
    constexpr ElementAddress xorDx32[] = {{0, 0, 0, 0, 0x0}, {0, 63, 63, 15, 0x3f0fc}, {0, 47, 4, 9, 0x13134}, {0, 54, 3, 3, 0x19af8}, {0, 23, 39, 12, 0x2ba5c}, {0, 57, 14, 7, 0x1c6e4}};
    requireThickAddresses(thickVolume(TextureTileMode::kD64KBX, 56, 64, 64, 16, 1), 4, 1, 262144, xorDx32, "SW_64KB_D_X 32 bpp 64x64x16, 1 level");
    constexpr ElementAddress xorDx8[] = {{0, 0, 0, 0, 0x20000}, {0, 99, 59, 69, 0x112b9f}, {0, 15, 52, 41, 0xa1347}, {0, 86, 43, 67, 0x11637e}, {0, 87, 59, 38, 0xb61fb}, {1, 0, 0, 0, 0x10000}, {1, 49, 29, 34, 0x76d29}, {1, 11, 28, 6, 0x126a3}, {1, 8, 27, 2, 0x12238}, {1, 30, 6, 17, 0x1c756}, {2, 0, 0, 0, 0x800}, {2, 24, 14, 16, 0xee10}, {2, 8, 3, 12, 0x1998}, {2, 15, 5, 9, 0x1d4f}, {2, 8, 8, 11, 0x3824}, {3, 0, 0, 0, 0x200}, {3, 11, 6, 7, 0x7b7}, {3, 7, 3, 6, 0x2fb}, {3, 3, 5, 4, 0x68b}, {3, 7, 2, 0, 0x253}, {4, 0, 0, 0, 0x4200}, {4, 5, 2, 3, 0x4275}, {4, 0, 2, 1, 0x4214}, {4, 2, 1, 1, 0x420e}, {4, 0, 2, 0, 0x4210}, {5, 0, 0, 0, 0x2000}, {5, 2, 0, 1, 0x2006}, {5, 1, 0, 0, 0x2001}, {6, 0, 0, 0, 0x2500}};
    requireThickAddresses(thickVolume(TextureTileMode::kD64KBX, 1, 100, 60, 70, 7), 1, 2, 1179648, xorDx8, "SW_64KB_D_X 8 bpp 100x60x70, 7 levels");
    constexpr ElementAddress xorDx128[] = {{0, 0, 0, 0, 0x10000}, {0, 8, 4, 2, 0x1c180}, {0, 0, 2, 1, 0x10820}, {0, 2, 3, 0, 0x10a40}, {0, 8, 4, 1, 0x1c120}, {1, 0, 0, 0, 0x8100}, {1, 3, 1, 0, 0x8350}, {1, 1, 0, 0, 0x8110}, {1, 0, 1, 0, 0x8140}, {1, 2, 0, 0, 0x8300}, {2, 0, 0, 0, 0x100}, {2, 1, 0, 0, 0x110}, {3, 0, 0, 0, 0x1000}};
    requireThickAddresses(thickVolume(TextureTileMode::kD64KBX, 77, 9, 5, 3, 4), 16, 1, 131072, xorDx128, "SW_64KB_D_X 128 bpp 9x5x3, 4 levels");
}};

const Case thinSurfaces{"DescribeSurface_ThinSurfaces_DetileAtAddrlibAddresses", [] {
    constexpr ElementAddress atD256B[] = {{0, 0, 0, 0, 0x300}, {0, 39, 19, 0, 0x81f}, {0, 20, 6, 0, 0x42c}, {0, 39, 0, 0, 0x507}, {0, 0, 19, 0, 0x618}, {1, 0, 0, 0, 0x100}, {1, 19, 9, 0, 0x293}, {1, 10, 3, 0, 0x15a}, {1, 19, 0, 0, 0x203}, {1, 0, 9, 0, 0x190}, {2, 0, 0, 0, 0x0}, {2, 9, 4, 0, 0x61}, {2, 5, 1, 0, 0x15}, {2, 9, 0, 0, 0x41}, {2, 0, 4, 0, 0x20}};
    requireThinAddresses(TextureTileMode::kD256B, 1, 40, 20, 3, 1, 2304, 2304, atD256B, "swizzle mode 2, 1 bytes, 40x20, 3 levels, 1 slices");
    constexpr ElementAddress atD4KB[] = {{0, 0, 0, 0, 0x6000}, {0, 99, 59, 0, 0x154b8}, {0, 50, 20, 0, 0xb920}, {0, 99, 0, 0, 0x9028}, {0, 0, 59, 0, 0x12490}, {1, 0, 0, 0, 0x2000}, {1, 49, 29, 0, 0x5d18}, {1, 25, 10, 0, 0x2e88}, {1, 49, 0, 0, 0x3808}, {1, 0, 29, 0, 0x4510}, {2, 0, 0, 0, 0x1000}, {2, 24, 14, 0, 0x1f80}, {2, 12, 5, 0, 0x1350}, {2, 24, 0, 0, 0x1a00}, {2, 0, 14, 0, 0x1580}, {3, 0, 0, 0, 0x800}, {3, 11, 6, 0, 0xba8}, {3, 6, 2, 0, 0x8e0}, {3, 11, 0, 0, 0xa28}, {3, 0, 6, 0, 0x980}, {4, 0, 0, 0, 0x600}, {4, 5, 2, 0, 0x6c8}, {4, 3, 1, 0, 0x638}, {4, 5, 0, 0, 0x648}, {4, 0, 2, 0, 0x680}, {5, 0, 0, 0, 0x500}, {5, 2, 0, 0, 0x520}, {5, 1, 0, 0, 0x508}, {5, 2, 0, 0, 0x520}, {5, 0, 0, 0, 0x500}, {6, 0, 0, 0, 0x400}, {6, 0, 0, 0, 0x400}, {6, 0, 0, 0, 0x400}, {6, 0, 0, 0, 0x400}, {6, 0, 0, 0, 0x400}};
    requireThinAddresses(TextureTileMode::kD4KB, 71, 100, 60, 7, 1, 90112, 90112, atD4KB, "swizzle mode 6, 8 bytes, 100x60, 7 levels, 1 slices");
    constexpr ElementAddress atD64KB[] = {{0, 0, 0, 0, 0x20000}, {0, 199, 149, 0, 0x584dc}, {0, 100, 50, 0, 0x2b4a0}, {0, 199, 0, 0, 0x3808c}, {0, 0, 149, 0, 0x40450}, {1, 0, 0, 0, 0x10000}, {1, 99, 74, 0, 0x1e12c}, {1, 50, 25, 0, 0x12d18}, {1, 99, 0, 0, 0x1a00c}, {1, 0, 74, 0, 0x14120}, {2, 0, 0, 0, 0x8000}, {2, 49, 36, 0, 0xb844}, {2, 25, 12, 0, 0x8b44}, {2, 49, 0, 0, 0xa804}, {2, 0, 36, 0, 0x9040}, {3, 0, 0, 0, 0x4000}, {3, 24, 17, 0, 0x4e10}, {3, 12, 6, 0, 0x42e0}, {3, 24, 0, 0, 0x4a00}, {3, 0, 17, 0, 0x4410}, {4, 0, 0, 0, 0x2000}, {4, 11, 8, 0, 0x230c}, {4, 6, 3, 0, 0x20b8}, {4, 11, 0, 0, 0x220c}, {4, 0, 8, 0, 0x2100}, {5, 0, 0, 0, 0x1000}, {5, 5, 3, 0, 0x10b4}, {5, 3, 1, 0, 0x101c}, {5, 5, 0, 0, 0x1084}, {5, 0, 3, 0, 0x1030}};
    requireThinAddresses(TextureTileMode::kD64KB, 56, 200, 150, 6, 1, 393216, 393216, atD64KB, "swizzle mode 10, 4 bytes, 200x150, 6 levels, 1 slices");
    constexpr ElementAddress atS64KBT[] = {{0, 0, 0, 0, 0x90000}, {0, 519, 259, 0, 0x1700bc}, {0, 260, 86, 0, 0xb46e0}, {0, 519, 0, 0, 0xd008c}, {0, 0, 259, 0, 0x130030}, {0, 0, 0, 1, 0x210000}, {0, 519, 259, 1, 0x2f00bc}, {0, 260, 86, 1, 0x2346e0}, {0, 519, 0, 1, 0x25008c}, {0, 0, 259, 1, 0x2b0030}, {0, 0, 0, 2, 0x390000}, {0, 519, 259, 2, 0x4700bc}, {0, 260, 86, 2, 0x3b46e0}, {0, 519, 0, 2, 0x3d008c}, {0, 0, 259, 2, 0x430030}, {1, 0, 0, 0, 0x30000}, {1, 259, 129, 0, 0x8001c}, {1, 130, 43, 0, 0x41938}, {1, 259, 0, 0, 0x5000c}, {1, 0, 129, 0, 0x60010}, {1, 0, 0, 1, 0x1b0000}, {1, 259, 129, 1, 0x20001c}, {1, 130, 43, 1, 0x1c1938}, {1, 259, 0, 1, 0x1d000c}, {1, 0, 129, 1, 0x1e0010}, {1, 0, 0, 2, 0x330000}, {1, 259, 129, 2, 0x38001c}, {1, 130, 43, 2, 0x341938}, {1, 259, 0, 2, 0x35000c}, {1, 0, 129, 2, 0x360010}, {2, 0, 0, 0, 0x10000}, {2, 129, 64, 0, 0x24204}, {2, 65, 21, 0, 0x18554}, {2, 129, 0, 0, 0x20004}, {2, 0, 64, 0, 0x14200}, {2, 0, 0, 1, 0x190000}, {2, 129, 64, 1, 0x1a4204}, {2, 65, 21, 1, 0x198554}, {2, 129, 0, 1, 0x1a0004}, {2, 0, 64, 1, 0x194200}, {2, 0, 0, 2, 0x310000}, {2, 129, 64, 2, 0x324204}, {2, 65, 21, 2, 0x318554}, {2, 129, 0, 2, 0x320004}, {2, 0, 64, 2, 0x314200}, {3, 0, 0, 0, 0x0}, {3, 64, 31, 0, 0x8470}, {3, 32, 10, 0, 0x2520}, {3, 64, 0, 0, 0x8100}, {3, 0, 31, 0, 0x570}, {3, 0, 0, 1, 0x180000}, {3, 64, 31, 1, 0x188470}, {3, 32, 10, 1, 0x182520}, {3, 64, 0, 1, 0x188100}, {3, 0, 31, 1, 0x180570}, {3, 0, 0, 2, 0x300000}, {3, 64, 31, 2, 0x308470}, {3, 32, 10, 2, 0x302520}, {3, 64, 0, 2, 0x308100}, {3, 0, 31, 2, 0x300570}};
    requireThinAddresses(TextureTileMode::kS64KBT, 56, 520, 260, 4, 3, 4718592, 1572864, atS64KBT, "swizzle mode 17, 4 bytes, 520x260, 4 levels, 3 slices");
    constexpr ElementAddress atD64KBT[] = {{0, 0, 0, 0, 0x10000}, {0, 63, 39, 0, 0x1ecf0}, {0, 32, 13, 0, 0x18420}, {0, 63, 0, 0, 0x1af50}, {0, 0, 39, 0, 0x143a0}, {0, 0, 0, 1, 0x30000}, {0, 63, 39, 1, 0x3ecf0}, {0, 32, 13, 1, 0x38420}, {0, 63, 0, 1, 0x3af50}, {0, 0, 39, 1, 0x343a0}, {1, 0, 0, 0, 0x8100}, {1, 31, 19, 0, 0xb7f0}, {1, 16, 6, 0, 0xa480}, {1, 31, 0, 0, 0xaf50}, {1, 0, 19, 0, 0x99a0}, {1, 0, 0, 1, 0x28100}, {1, 31, 19, 1, 0x2b7f0}, {1, 16, 6, 1, 0x2a480}, {1, 31, 0, 1, 0x2af50}, {1, 0, 19, 1, 0x299a0}, {2, 0, 0, 0, 0x4200}, {2, 15, 9, 0, 0x4c70}, {2, 8, 3, 0, 0x4aa0}, {2, 15, 0, 0, 0x4850}, {2, 0, 9, 0, 0x4620}, {2, 0, 0, 1, 0x24200}, {2, 15, 9, 1, 0x24c70}, {2, 8, 3, 1, 0x24aa0}, {2, 15, 0, 1, 0x24850}, {2, 0, 9, 1, 0x24620}, {3, 0, 0, 0, 0x2400}, {3, 7, 4, 0, 0x2750}, {3, 4, 1, 0, 0x2620}, {3, 7, 0, 0, 0x2650}, {3, 0, 4, 0, 0x2500}, {3, 0, 0, 1, 0x22400}, {3, 7, 4, 1, 0x22750}, {3, 4, 1, 1, 0x22620}, {3, 7, 0, 1, 0x22650}, {3, 0, 4, 1, 0x22500}, {4, 0, 0, 0, 0x1800}, {4, 3, 1, 0, 0x1870}, {4, 2, 0, 0, 0x1840}, {4, 3, 0, 0, 0x1850}, {4, 0, 1, 0, 0x1820}, {4, 0, 0, 1, 0x21800}, {4, 3, 1, 1, 0x21870}, {4, 2, 0, 1, 0x21840}, {4, 3, 0, 1, 0x21850}, {4, 0, 1, 1, 0x21820}};
    requireThinAddresses(TextureTileMode::kD64KBT, 77, 64, 40, 5, 2, 262144, 131072, atD64KBT, "swizzle mode 18, 16 bytes, 64x40, 5 levels, 2 slices");
    constexpr ElementAddress atS4KBX[] = {{0, 0, 0, 0, 0x2000}, {0, 99, 69, 0, 0x5453}, {0, 50, 23, 0, 0x2b72}, {0, 99, 0, 0, 0x3c03}, {0, 0, 69, 0, 0x4850}, {0, 0, 0, 1, 0x8800}, {0, 99, 69, 1, 0xbc53}, {0, 50, 23, 1, 0x8372}, {0, 99, 0, 1, 0x9403}, {0, 0, 69, 1, 0xa050}, {0, 0, 0, 2, 0xe400}, {0, 99, 69, 2, 0x11053}, {0, 50, 23, 2, 0xef72}, {0, 99, 0, 2, 0xf803}, {0, 0, 69, 2, 0x10c50}, {1, 0, 0, 0, 0x1000}, {1, 49, 34, 0, 0x1e21}, {1, 25, 11, 0, 0x12b9}, {1, 49, 0, 0, 0x1a01}, {1, 0, 34, 0, 0x1420}, {1, 0, 0, 1, 0x7800}, {1, 49, 34, 1, 0x7621}, {1, 25, 11, 1, 0x7ab9}, {1, 49, 0, 1, 0x7201}, {1, 0, 34, 1, 0x7c20}, {1, 0, 0, 2, 0xd400}, {1, 49, 34, 2, 0xda21}, {1, 25, 11, 2, 0xd6b9}, {1, 49, 0, 2, 0xde01}, {1, 0, 34, 2, 0xd020}, {2, 0, 0, 0, 0x800}, {2, 24, 16, 0, 0xb08}, {2, 12, 5, 0, 0x85c}, {2, 24, 0, 0, 0xa08}, {2, 0, 16, 0, 0x900}, {2, 0, 0, 1, 0x6000}, {2, 24, 16, 1, 0x6308}, {2, 12, 5, 1, 0x605c}, {2, 24, 0, 1, 0x6208}, {2, 0, 16, 1, 0x6100}, {2, 0, 0, 2, 0xcc00}, {2, 24, 16, 2, 0xcf08}, {2, 12, 5, 2, 0xcc5c}, {2, 24, 0, 2, 0xce08}, {2, 0, 16, 2, 0xcd00}, {3, 0, 0, 0, 0x600}, {3, 11, 7, 0, 0x67b}, {3, 6, 2, 0, 0x626}, {3, 11, 0, 0, 0x60b}, {3, 0, 7, 0, 0x670}, {3, 0, 0, 1, 0x6e00}, {3, 11, 7, 1, 0x6e7b}, {3, 6, 2, 1, 0x6e26}, {3, 11, 0, 1, 0x6e0b}, {3, 0, 7, 1, 0x6e70}, {3, 0, 0, 2, 0xc200}, {3, 11, 7, 2, 0xc27b}, {3, 6, 2, 2, 0xc226}, {3, 11, 0, 2, 0xc20b}, {3, 0, 7, 2, 0xc270}, {4, 0, 0, 0, 0x500}, {4, 5, 3, 0, 0x535}, {4, 3, 1, 0, 0x513}, {4, 5, 0, 0, 0x505}, {4, 0, 3, 0, 0x530}, {4, 0, 0, 1, 0x6d00}, {4, 5, 3, 1, 0x6d35}, {4, 3, 1, 1, 0x6d13}, {4, 5, 0, 1, 0x6d05}, {4, 0, 3, 1, 0x6d30}, {4, 0, 0, 2, 0xc100}, {4, 5, 3, 2, 0xc135}, {4, 3, 1, 2, 0xc113}, {4, 5, 0, 2, 0xc105}, {4, 0, 3, 2, 0xc130}, {5, 0, 0, 0, 0x400}, {5, 2, 1, 0, 0x412}, {5, 1, 0, 0, 0x401}, {5, 2, 0, 0, 0x402}, {5, 0, 1, 0, 0x410}, {5, 0, 0, 1, 0x6c00}, {5, 2, 1, 1, 0x6c12}, {5, 1, 0, 1, 0x6c01}, {5, 2, 0, 1, 0x6c02}, {5, 0, 1, 1, 0x6c10}, {5, 0, 0, 2, 0xc000}, {5, 2, 1, 2, 0xc012}, {5, 1, 0, 2, 0xc001}, {5, 2, 0, 2, 0xc002}, {5, 0, 1, 2, 0xc010}};
    requireThinAddresses(TextureTileMode::kS4KBX, 1, 100, 70, 6, 3, 73728, 24576, atS4KBX, "swizzle mode 21, 1 bytes, 100x70, 6 levels, 3 slices");
    constexpr ElementAddress atD4KBX[] = {{0, 0, 0, 0, 0x2000}, {0, 32, 19, 0, 0x3030}, {0, 16, 6, 0, 0x2860}, {0, 32, 0, 0, 0x3400}, {0, 0, 19, 0, 0x2430}, {0, 0, 0, 1, 0x6800}, {0, 32, 19, 1, 0x7830}, {0, 16, 6, 1, 0x6060}, {0, 32, 0, 1, 0x7c00}, {0, 0, 19, 1, 0x6c30}, {1, 0, 0, 0, 0x1000}, {1, 15, 9, 0, 0x139c}, {1, 8, 3, 0, 0x1230}, {1, 15, 0, 0, 0x128c}, {1, 0, 9, 0, 0x1110}, {1, 0, 0, 1, 0x5800}, {1, 15, 9, 1, 0x5b9c}, {1, 8, 3, 1, 0x5a30}, {1, 15, 0, 1, 0x5a8c}, {1, 0, 9, 1, 0x5910}, {2, 0, 0, 0, 0x800}, {2, 7, 4, 0, 0x8cc}, {2, 4, 1, 0, 0x890}, {2, 7, 0, 0, 0x88c}, {2, 0, 4, 0, 0x840}, {2, 0, 0, 1, 0x4000}, {2, 7, 4, 1, 0x40cc}, {2, 4, 1, 1, 0x4090}, {2, 7, 0, 1, 0x408c}, {2, 0, 4, 1, 0x4040}, {3, 0, 0, 0, 0x600}, {3, 3, 1, 0, 0x61c}, {3, 2, 0, 0, 0x608}, {3, 3, 0, 0, 0x60c}, {3, 0, 1, 0, 0x610}, {3, 0, 0, 1, 0x4e00}, {3, 3, 1, 1, 0x4e1c}, {3, 2, 0, 1, 0x4e08}, {3, 3, 0, 1, 0x4e0c}, {3, 0, 1, 1, 0x4e10}};
    requireThinAddresses(TextureTileMode::kD4KBX, 56, 33, 20, 4, 2, 32768, 16384, atD4KBX, "swizzle mode 22, 4 bytes, 33x20, 4 levels, 2 slices");
    constexpr ElementAddress atZ64KBX8[] = {{0, 0, 0, 0, 0x20000}, {1, 0, 0, 0, 0x10000}};
    requireThinAddresses(TextureTileMode::kZ64KBX, 1, 256, 256, 9, 1, 196608, 196608, atZ64KBX8, "swizzle mode 24, 1 bytes, 256x256, 9 levels, 1 slices");
    constexpr ElementAddress atZ64KBX16[] = {{0, 0, 0, 0, 0x10000}};
    requireThinAddresses(TextureTileMode::kZ64KBX, 7, 128, 128, 8, 1, 131072, 131072, atZ64KBX16, "swizzle mode 24, 2 bytes, 128x128, 8 levels, 1 slices");
}};

const Case coveredBytes{"CoveredMipBytes_TiledAndLinearMips_CoverExactlyTheElementSlots", [] {
    constexpr std::array<TextureTileMode, 4> modes{TextureTileMode::kZ64KBX, TextureTileMode::kS64KBX, TextureTileMode::kD64KBX, TextureTileMode::kR64KBX};
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 6> sizes{{{200, 150}, {1920, 1080}, {2432, 1368}, {960, 540}, {256, 128}, {300, 1}}};
    for (const auto mode : modes) {
        for (std::uint32_t bytesPerElement = 1; bytesPerElement <= 16; bytesPerElement *= 2) {
            const auto* equation = FindTextureSwizzleEquation(XorSwizzleMode(mode), bytesPerElement);
            Require(equation != nullptr, "missing XOR swizzle equation");
            const auto block = ThinBlockLayout(mode, bytesPerElement);
            for (const auto& [width, height] : sizes) {
                if (static_cast<std::uint64_t>(width) * height > (1u << 20) && mode != TextureTileMode::kR64KBX) continue;
                const auto what = "XOR mode " + std::to_string(XorSwizzleMode(mode)) + ", " + std::to_string(bytesPerElement) + " bytes, " + std::to_string(width) + "x" + std::to_string(height);
                for (const auto& mip : ComputeElementMipLayout(mode, bytesPerElement, width, height, 3)) {
                    const auto covered = CoveredMipBytes(mode, bytesPerElement, mip);
                    if (mip.tail) {
                        Require(covered.empty(), what + ": a tail mip has covered bytes");
                        continue;
                    }
                    std::vector<std::uint8_t> held(static_cast<std::size_t>(mip.tiledSize / bytesPerElement), 0);
                    for (std::uint32_t y = 0; y < mip.height; ++y) {
                        for (std::uint32_t x = 0; x < mip.width; ++x) {
                            const auto offset = (static_cast<std::uint64_t>(y / block[2]) * mip.blocksPerRow + x / block[1]) * block[0] + equationOffset(*equation, x, y, 0);
                            Require(offset % bytesPerElement == 0 && offset < mip.tiledSize, what + ": an element lies outside the mip");
                            held[static_cast<std::size_t>(offset / bytesPerElement)] += 1;
                        }
                    }
                    std::vector<bool> inCovered(static_cast<std::size_t>(mip.tiledSize / block[0]), false);
                    std::uint64_t previous = 0;
                    for (const auto& [begin, end] : covered) {
                        Require(begin >= previous && begin < end && end <= mip.tiledSize && begin % block[0] == 0 && end % block[0] == 0, what + ": covered ranges are not ascending whole blocks");
                        previous = end;
                        for (auto slot = begin / bytesPerElement; slot < end / bytesPerElement; ++slot) Require(held[static_cast<std::size_t>(slot)] == 1, what + ": a covered byte holds no element or several");
                        for (auto at = begin; at < end; at += block[0]) inCovered[static_cast<std::size_t>(at / block[0])] = true;
                    }
                    for (std::size_t index = 0; index < inCovered.size(); ++index) {
                        if (inCovered[index]) continue;
                        const auto first = index * block[0] / bytesPerElement;
                        const auto last = (index + 1) * block[0] / bytesPerElement;
                        Require(std::any_of(held.begin() + static_cast<std::ptrdiff_t>(first), held.begin() + static_cast<std::ptrdiff_t>(last), [](std::uint8_t count) { return count == 0; }), what + ": a block every byte of which holds an element is left out");
                    }
                }
            }
        }
    }
    const auto linear = ComputeElementMipLayout(TextureTileMode::kLinear, 4, 200, 3, 1).front();
    const auto rows = CoveredMipBytes(TextureTileMode::kLinear, 4, linear);
    Require(linear.pitchBytes > 800 && rows.size() == 3 && rows[1].first == linear.pitchBytes && rows[1].second == linear.pitchBytes + 800, "linear covered bytes are not the rows without their pitch padding");
    const auto dense = ComputeElementMipLayout(TextureTileMode::kLinear, 4, 256, 3, 1).front();
    Require(dense.pitchBytes == 1024 && CoveredMipBytes(TextureTileMode::kLinear, 4, dense) == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{0, 3072}}, "linear rows without padding are not one range");
}};

} // namespace
