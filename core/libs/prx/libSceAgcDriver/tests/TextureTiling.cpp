#include "GraphicsTests.hpp"
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

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected texture tiling test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture tiling rejection: ") + std::string(reason));
}

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
    const auto* equation = FindTextureSwizzleEquation(resource.tileMode == TextureTileMode::kStandard4KB ? 0x105u : 0x109u, bytesPerElement);
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

}

void RunTextureTilingTests() {
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
    reject([] { ComputeMipLayout(TextureTileMode::RenderTarget64KB, 74, 64, 64, 1); }, "unsupported bytes per element");

    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 0, 4, 1); }, "zero-sized texture");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 0, 1); }, "zero-sized texture");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 0); }, "mip count is out of range");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 17); }, "mip count is out of range");

    {
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
    }

    {
        constexpr ElementAddress chainInTail[] = {{0, 0, 0, 0, 0x800}, {0, 7, 7, 7, 0xffc}, {0, 4, 2, 4, 0xe20}, {0, 2, 7, 0, 0x968}, {0, 7, 0, 7, 0xed4}, {1, 0, 0, 0, 0x300}, {1, 3, 3, 3, 0x3fc}, {1, 2, 1, 2, 0x3c8}, {1, 1, 3, 0, 0x32c}, {1, 3, 0, 3, 0x3d4}};
        requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 56, 8, 8, 8, 2), 4, 0, 4096, chainInTail, "SW_4KB_S 32 bpp 8x8x8, 2 levels");
        constexpr ElementAddress unevenTail[] = {{0, 0, 0, 0, 0x6000}, {0, 32, 19, 11, 0x1f0b8}, {0, 16, 6, 6, 0x85a0}, {0, 11, 19, 0, 0xc06c}, {0, 32, 0, 11, 0x1a090}, {1, 0, 0, 0, 0x3000}, {1, 15, 9, 5, 0x4e5c}, {1, 8, 3, 3, 0x40b8}, {1, 5, 9, 0, 0x3a0c}, {1, 15, 0, 5, 0x4654}, {2, 0, 0, 0, 0x1000}, {2, 7, 4, 2, 0x13c4}, {2, 4, 1, 1, 0x1218}, {2, 2, 4, 0, 0x1140}, {2, 7, 0, 2, 0x12c4}, {3, 0, 0, 0, 0x800}, {3, 3, 1, 0, 0x84c}, {3, 2, 0, 0, 0x840}, {3, 1, 1, 0, 0x80c}, {3, 3, 0, 0, 0x844}, {4, 0, 0, 0, 0x300}, {4, 1, 0, 0, 0x304}, {4, 0, 0, 0, 0x300}, {5, 0, 0, 0, 0x200}};
        requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 56, 33, 20, 12, 6), 4, 3, 131072, unevenTail, "SW_4KB_S 32 bpp 33x20x12, 6 levels");
        constexpr ElementAddress wideTail[] = {{0, 0, 0, 0, 0x20000}, {0, 39, 23, 19, 0xb0bf8}, {0, 20, 8, 10, 0x2e280}, {0, 13, 23, 0, 0x41b28}, {0, 39, 0, 19, 0x902d8}, {1, 0, 0, 0, 0x10000}, {1, 19, 11, 9, 0x1e178}, {1, 10, 4, 5, 0x11c50}, {1, 6, 11, 0, 0x14360}, {1, 19, 0, 9, 0x1a058}, {2, 0, 0, 0, 0x8000}, {2, 9, 5, 4, 0x9c28}, {2, 5, 2, 2, 0x8388}, {2, 3, 5, 0, 0x8868}, {2, 9, 0, 4, 0x9408}, {3, 0, 0, 0, 0x4000}, {3, 4, 2, 1, 0x4310}, {3, 2, 1, 1, 0x4070}, {3, 1, 2, 0, 0x4108}, {3, 4, 0, 1, 0x4210}, {4, 0, 0, 0, 0x1000}, {4, 1, 0, 0, 0x1008}, {5, 0, 0, 0, 0xa00}};
        requireThickAddresses(thickVolume(TextureTileMode::kStandard64KB, 71, 40, 24, 20, 6), 8, 2, 786432, wideTail, "SW_64KB_S 64 bpp 40x24x20, 6 levels");
        constexpr ElementAddress byteTail[] = {{0, 0, 0, 0, 0x20000}, {0, 99, 59, 69, 0x11c8af}, {0, 50, 20, 35, 0x8d116}, {0, 33, 59, 0, 0x4c829}, {0, 99, 0, 69, 0xf8087}, {1, 0, 0, 0, 0x10000}, {1, 49, 29, 34, 0x7d919}, {1, 25, 10, 17, 0x13a25}, {1, 16, 29, 0, 0x15908}, {1, 49, 0, 34, 0x79011}, {2, 0, 0, 0, 0x8000}, {2, 24, 14, 16, 0xbb20}, {2, 12, 5, 8, 0x8748}, {2, 8, 14, 0, 0x8b20}, {2, 24, 0, 16, 0xb200}, {3, 0, 0, 0, 0x4000}, {3, 11, 6, 7, 0x43b7}, {3, 6, 2, 4, 0x40e2}, {3, 4, 6, 0, 0x4160}, {3, 11, 0, 7, 0x4297}, {4, 0, 0, 0, 0x1000}, {4, 5, 2, 3, 0x1075}, {4, 3, 1, 2, 0x101b}, {4, 2, 2, 0, 0x1022}, {4, 5, 0, 3, 0x1055}, {5, 0, 0, 0, 0xa00}, {5, 2, 0, 1, 0xa06}, {5, 1, 0, 0, 0xa01}, {6, 0, 0, 0, 0x900}};
        requireThickAddresses(thickVolume(TextureTileMode::kStandard64KB, 1, 100, 60, 70, 7), 1, 2, 1179648, byteTail, "SW_64KB_S 8 bpp 100x60x70, 7 levels");
        constexpr ElementAddress wideElementTail[] = {{0, 0, 0, 0, 0x3000}, {0, 8, 4, 2, 0x5880}, {0, 4, 1, 1, 0x4030}, {0, 3, 4, 0, 0x3a40}, {0, 8, 0, 2, 0x5080}, {1, 0, 0, 0, 0x1000}, {1, 3, 1, 0, 0x1260}, {1, 2, 0, 0, 0x1200}, {1, 1, 1, 0, 0x1060}, {1, 3, 0, 0, 0x1240}, {2, 0, 0, 0, 0x800}, {2, 1, 0, 0, 0x840}, {3, 0, 0, 0, 0x300}};
        requireThickAddresses(thickVolume(TextureTileMode::kStandard4KB, 77, 9, 5, 3, 4), 16, 2, 24576, wideElementTail, "SW_4KB_S 128 bpp 9x5x3, 4 levels");
    }

    {
        // CoveredMipBytes against the XOR address equations: every element slot of a covered range
        // holds exactly one element of the mip, and every tile block left out holds a slot no
        // element does (the bytes a write-back must keep).
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
    }

    reject([] { ComputeSurfaceSize({}, 1); }, "empty mip chain");
    reject([] { ComputeSurfaceSize(ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 1), 0); }, "zero array layers");
}
