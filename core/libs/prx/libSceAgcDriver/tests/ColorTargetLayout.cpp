#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

template<typename TAction>
void reject(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("expected color layout rejection");
}

}

void RunColorTargetLayoutTests() {
    Require(DecodeColorTileMode(0x4dc6c000) == ColorTileMode::RenderTarget, "logged color descriptor was rejected");
    Require(DecodeColorTileMode(0x09000000) == ColorTileMode::Linear, "linear descriptor changed");
    Require(DecodeColorTileMode(0x4dc6c001) == ColorTileMode::RenderTarget, "2D array color descriptor with MIP0_DEPTH 1 was rejected");
    reject([] { DecodeColorTileMode(0x4dc6e000); });
    reject([] { DecodeColorTileMode(0xcdc6c000); });
    reject([] { DecodeColorTileMode(0x09004000); });
    Require(DecodeColorTileMode(0x4cc6c000) == ColorTileMode::RenderTarget, "1D color descriptor was rejected");
    reject([] { DecodeColorTileMode(0x4cc6c001); });
    reject([] { DecodeColorTileMode(0x4fc6c000); });
    Require(DecodeColorTileMode(0x08000000) == ColorTileMode::Linear, "linear 1D color descriptor was rejected");
    reject([] { DecodeColorTileMode(0x08014000); });
    reject([] { DecodeColorTileMode(0x08024000); });
    Require(DecodeColorTileMode(0x0da58000) == ColorTileMode::D4KBX, "a SW_4KB_D_X color descriptor with its Z-mode FMASK swizzle was rejected");
    Require(DecodeColorTileMode(0x0da54000) == ColorTileMode::S4KBX, "a SW_4KB_S_X color descriptor with its Z-mode FMASK swizzle was rejected");
    Require(DecodeColorTileMode(0x09058000) == ColorTileMode::D4KBX, "a SW_4KB_D_X color descriptor without an FMASK swizzle was rejected");
    reject([] { DecodeColorTileMode(0x0d458000); });
    reject([] { ColorTargetLayout(0, 1, ColorTileMode::RenderTarget); });
    const ColorTargetLayout padded(63, 2, ColorTileMode::Linear);
    Require(padded.Bytes() == 512 && padded.LinearBytes() == 504 && padded.Offset(0, 1) == 256, "linear rows are not padded to 256 bytes");
    const ColorTargetLayout screen(3840, 2160, ColorTileMode::RenderTarget);
    Require(screen.Bytes() == 33423360 && screen.LinearBytes() == 33177600 && screen.Alignment() == 65536, "4K color backing layout is incorrect");
    const ColorTargetLayout layout(257, 129, ColorTileMode::RenderTarget);
    Require(layout.Offset(0, 0) == 0 && layout.Offset(1, 0) == 4 && layout.Offset(0, 1) == 16, "microtile address is incorrect");
    Require(layout.Offset(128, 0) == 65536 && layout.Offset(0, 128) == 3 * 65536, "block raster order is incorrect");
    Require(layout.Offset(16, 0) == 0x2200 && layout.Offset(0, 8) == 0x1100, "render-target XOR addressing is incorrect");
    Require(layout.Offset(256, 0) == 2 * 65536, "third block address is incorrect");
    const ColorTargetLayout r64(33, 65, ColorTileMode::RenderTarget, 8);
    const ColorTargetLayout r128(129, 67, ColorTileMode::RenderTarget, 16);
    Require(r64.Offset(0, 64) == 67584, "64 bpp SW_64KB_R_X dropped a cross-block y-coordinate XOR bit");
    Require(r128.Offset(64, 0) == 66560, "128 bpp SW_64KB_R_X dropped a cross-block x-coordinate XOR bit");
    Require(r64.Offset(32, 64) == 65536, "64 bpp SW_64KB_R_X did not XOR overlapping coordinate contributions");
    Require(r128.Offset(32, 64) == 3 * 65536, "128 bpp SW_64KB_R_X did not XOR overlapping coordinate contributions");
    Require(DecodeColorTileMode(0x4dc14000) == ColorTileMode::Standard4KB, "4 KiB standard color descriptor was rejected");
    const ColorTargetLayout standard(256, 256, ColorTileMode::Standard4KB);
    Require(standard.Bytes() == ComputeSurfaceSize(ComputeElementMipLayout(TextureTileMode::kStandard4KB, 4, 256, 256, 1), 1) && standard.Alignment() == 4096, "4 KiB standard color layout differs from the texture layout");
    Require(ColorTargetLayout(1920, 1080, ColorTileMode::RenderTarget, 16).Bytes() == 0x1fe0000u, "four-sample 1920x1080 color target should fit before the next 32 MiB allocation");
    Require(standard.Offset(32, 0) == 4096 && standard.Offset(0, 32) == 8 * 4096 && standard.Offset(1, 0) == 4 && standard.Offset(0, 1) == 16, "4 KiB standard block or element addressing is incorrect");
    std::vector<bool> standardSeen(1024);
    for (std::uint32_t y = 0; y < 32; ++y) {
        for (std::uint32_t x = 0; x < 32; ++x) {
            const auto address = standard.Offset(x, y);
            Require(address < 4096 && address % 4 == 0 && !standardSeen[address / 4], "4 KiB standard block aliases or leaves its texels");
            standardSeen[address / 4] = true;
        }
    }
    for (const auto mode : {ColorTileMode::Standard4KB, ColorTileMode::Standard64KB}) {
        for (const std::uint32_t bpe : {1u, 2u, 4u, 8u, 16u}) {
            const auto block = mode == ColorTileMode::Standard64KB ? 65536u : 4096u;
            const ColorTargetLayout sized(512, 512, mode, bpe);
            Require(sized.Alignment() == block && sized.Bytes() == ComputeSurfaceSize(ComputeElementMipLayout(ColorTextureTileMode(mode), bpe, 512, 512, 1), 1), "standard color layout differs from the texture layout");
            std::vector<bool> seen(block / bpe);
            std::uint32_t count = 0;
            for (std::uint32_t y = 0; y < 512 && count < seen.size(); ++y) {
                for (std::uint32_t x = 0; x < 512; ++x) {
                    const auto address = sized.Offset(x, y);
                    if (address >= block) continue;
                    Require(address % bpe == 0 && !seen[address / bpe], "standard block aliases its texels");
                    seen[address / bpe] = true;
                    ++count;
                }
            }
            Require(count == seen.size(), "standard block leaves texels unaddressed");
        }
    }
    Require(DecodeColorTileMode(0x4dc24000) == ColorTileMode::Standard64KB, "64 KiB standard color descriptor was rejected");
    reject([&] { layout.Offset(257, 0); });
    Require(CmaskLayout(1, 1).Bytes() == 4096 && CmaskLayout(1024, 512).Bytes() == 4096 && CmaskLayout(1025, 513).Bytes() == 16384 && CmaskLayout(1920, 1080).Bytes() == 24576 && CmaskLayout(2432, 1368).Bytes() == 36864 && CmaskLayout(3328, 1872).Bytes() == 65536 && CmaskLayout(3840, 2160).Bytes() == 81920, "CMASK size differs from addrlib's pipe-aligned SW_64KB_Z_X CMASK");
    struct CmaskReference {
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t tileX;
        std::uint32_t tileY;
        std::size_t nibble;
    };
    constexpr std::array<CmaskReference, 10> cmaskReferences{{
        {20, 12, 2, 1, 0x600u}, {61, 13, 7, 1, 0x1401u}, {61, 13, 5, 0, 0x1201u}, {1920, 1080, 239, 134, 0xb347u}, {1920, 1080, 128, 64, 0x6000u},
        {1920, 1080, 1, 1, 0x1u}, {3840, 2160, 479, 269, 0x2651du}, {3840, 2160, 200, 77, 0xb30cu}, {2432, 1368, 303, 170, 0x10acfu}, {1025, 513, 128, 64, 0x6000u},
    }};
    for (const auto& reference : cmaskReferences) Require(CmaskLayout(reference.width, reference.height).Nibble(reference.tileX, reference.tileY) == reference.nibble, "CMASK nibble address differs from addrlib's");
    for (const auto [width, height] : {std::pair{61u, 13u}, std::pair{1920u, 1080u}}) {
        const CmaskLayout cmask(width, height);
        std::vector<bool> seen(cmask.Bytes() * 2u);
        for (std::uint32_t tileY = 0; tileY < cmask.TilesY(); ++tileY) {
            for (std::uint32_t tileX = 0; tileX < cmask.TilesX(); ++tileX) {
                const auto nibble = cmask.Nibble(tileX, tileY);
                Require(nibble < seen.size() && !seen[nibble], "CMASK tiles alias or leave the CMASK");
                seen[nibble] = true;
            }
        }
    }
    reject([] { CmaskLayout(0, 1); });
    reject([] { CmaskLayout(61, 13).Nibble(8, 0); });
    reject([] { CmaskLayout(61, 13).Nibble(0, 2); });
    std::vector<std::byte> tiled(layout.Bytes(), std::byte{0x5a});
    std::vector<std::byte> linear(layout.LinearBytes());
    std::vector<std::byte> restored(linear.size());
    std::vector<bool> visited(tiled.size() / 4);
    for (std::uint32_t y = 0; y < 129; ++y) {
        for (std::uint32_t x = 0; x < 257; ++x) {
            const auto address = layout.Offset(x, y);
            Require(address % 4 == 0 && address + 4 <= tiled.size() && !visited[address / 4], "color address is out of range or aliases another pixel");
            visited[address / 4] = true;
            const auto value = y * 257 + x;
            std::memcpy(linear.data() + static_cast<std::size_t>(value) * 4, &value, 4);
        }
    }
    layout.Tile(linear, tiled);
    layout.Detile(tiled, restored);
    Require(restored == linear, "color tiling round trip lost pixels");
    for (std::size_t i = 0; i < tiled.size(); ++i) {
        if (!visited[i / 4]) Require(tiled[i] == std::byte{0x5a}, "color tiling overwrote padding");
    }
    reject([&] { layout.Detile(std::span(tiled).first(4), restored); });
    reject([&] { layout.Tile(std::span(linear).first(4), tiled); });
    static std::vector<std::byte> storage(2 * 65536);
    const std::span guest(reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu}), 65536);
    std::fill(guest.begin(), guest.end(), std::byte{0x6b});
    ColorTarget target{reinterpret_cast<std::uintptr_t>(guest.data()), {2, 2}, VK_FORMAT_R8G8B8A8_UNORM, guest.size(), 0xe4, ColorTileMode::RenderTarget};
    std::array<std::byte, 16> pixels{};
    pixels.fill(std::byte{0x32});
    WriteColorTarget(target, pixels);
    Require(guest[0] == std::byte{0x32} && guest[16] == std::byte{0x32} && guest[8] == std::byte{0x6b}, "guest transfer layout or padding preservation failed");
    std::array<std::byte, 16> readback{};
    ReadColorTarget(target, readback);
    Require(readback == pixels, "guest color transfer round trip failed");
    const ColorTargetLayout linearLayout(64, 2, ColorTileMode::Linear);
    std::array<std::byte, 512> linearPixels{};
    for (std::size_t i = 0; i < linearPixels.size(); ++i) linearPixels[i] = static_cast<std::byte>(i & 255u);
    target = {reinterpret_cast<std::uintptr_t>(guest.data()), {64, 2}, VK_FORMAT_R8G8B8A8_UNORM, linearLayout.Bytes(), 0xe4, ColorTileMode::Linear};
    WriteColorTarget(target, linearPixels);
    std::array<std::byte, 512> linearReadback{};
    ReadColorTarget(target, linearReadback);
    Require(linearReadback == linearPixels && guest[512] == std::byte{0x6b}, "linear guest color transfer changed");

    const ColorTargetLayout plainLayout(256, 70, ColorTileMode::RenderTarget, 4);
    const ColorTargetLayout xorLayout(256, 70, ColorTileMode::RenderTarget, 4, 0x5600);
    for (const auto& [x, y] : {std::pair{0u, 0u}, std::pair{1u, 0u}, std::pair{0u, 1u}, std::pair{127u, 69u}, std::pair{128u, 0u}, std::pair{255u, 69u}, std::pair{200u, 33u}}) {
        const auto plain = plainLayout.Offset(x, y);
        Require(xorLayout.Offset(x, y) == (plain & ~std::size_t{0xffff}) + ((plain & 0xffffu) ^ 0x5600u), "a pipe/bank XOR did not apply to the offset inside the SW_64KB_R_X block");
    }
    std::vector<std::byte> xorLinear(xorLayout.LinearBytes());
    for (std::size_t i = 0; i < xorLinear.size(); ++i) xorLinear[i] = static_cast<std::byte>((i * 7u) & 255u);
    std::vector<std::byte> xorTiled(xorLayout.Bytes(), std::byte{0});
    std::vector<std::byte> xorRestored(xorLinear.size());
    xorLayout.Tile(xorLinear, xorTiled);
    xorLayout.Detile(xorTiled, xorRestored);
    Require(xorRestored == xorLinear, "color tiling round trip with a pipe/bank XOR lost pixels");
    reject([] { static_cast<void>(ColorTargetLayout(64, 64, ColorTileMode::Standard64KB, 4, 0x100)); });
    reject([] { static_cast<void>(ColorTargetLayout(64, 64, ColorTileMode::Linear, 4, 0x100)); });
    reject([] { static_cast<void>(ColorTargetLayout(64, 64, ColorTileMode::RenderTarget, 4, 0x80)); });
    reject([] { static_cast<void>(ColorTargetLayout(64, 64, ColorTileMode::RenderTarget, 4, 0x10000)); });

    std::fill(guest.begin(), guest.end(), std::byte{0x6b});
    target = {reinterpret_cast<std::uintptr_t>(guest.data()), {2, 2}, VK_FORMAT_R8G8B8A8_UNORM, guest.size(), 0xe4, ColorTileMode::RenderTarget};
    target.pipeBankXor = 0x3200;
    WriteColorTarget(target, pixels);
    Require(guest[0x3200] == std::byte{0x32} && guest[0x3210] == std::byte{0x32} && guest[0] == std::byte{0x6b} && guest[16] == std::byte{0x6b}, "a color target with a pipe/bank XOR was written at the unswizzled offsets");
    readback.fill(std::byte{0});
    ReadColorTarget(target, readback);
    Require(readback == pixels, "guest color transfer round trip with a pipe/bank XOR failed");

    for (const auto mode : {ColorTileMode::S4KBX, ColorTileMode::D4KBX}) {
        const auto* equation = FindTextureSwizzleEquation(static_cast<std::uint32_t>(mode), 4u);
        Require(equation != nullptr, "the 4 KiB XOR equation is missing");
        const auto expected = [&](std::uint32_t x, std::uint32_t y) {
            std::size_t offset = 0;
            for (std::uint32_t bit = 0; bit < 16u; ++bit) offset |= static_cast<std::size_t>(std::popcount((x & equation->bits[bit] & 0xfffu) ^ ((y << 12u) & equation->bits[bit] & 0xfff000u)) & 1) << bit;
            return offset;
        };
        const ColorTargetLayout xor4Kb(40, 20, mode, 4, 0xa00);
        Require(xor4Kb.Alignment() == 4096u && xor4Kb.Bytes() == 64u * 32u * 4u && xor4Kb.BlocksPerRow() == 2u, "the 4 KiB XOR color layout has the wrong block geometry");
        for (const auto& [x, y] : {std::pair{0u, 0u}, std::pair{1u, 0u}, std::pair{0u, 1u}, std::pair{31u, 19u}, std::pair{32u, 0u}, std::pair{39u, 19u}}) {
            Require(xor4Kb.Offset(x, y) == (x / 32u) * 4096u + (expected(x % 32u, y) ^ 0xa00u), "a 4 KiB XOR color offset does not follow its swizzle equation and pipe/bank XOR");
        }
        std::vector<std::byte> source(xor4Kb.LinearBytes());
        for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::byte>((i * 13u) & 255u);
        std::vector<std::byte> tiled4Kb(xor4Kb.Bytes(), std::byte{0});
        std::vector<std::byte> back(source.size());
        xor4Kb.Tile(source, tiled4Kb);
        xor4Kb.Detile(tiled4Kb, back);
        Require(back == source, "4 KiB XOR color tiling round trip lost pixels");
        reject([mode] { static_cast<void>(ColorTargetLayout(40, 20, mode, 4, 0x1000)); });
    }

    Require(DecodeColorTileMode(0x0dc68000u) == ColorTileMode::D64KBX, "the observed SW_64KB_D_X color descriptor was rejected");
    Require(DecodeColorTileMode(0x4dc60000u) == ColorTileMode::Z64KBX, "the observed SW_64KB_Z_X color descriptor was rejected");
    Require(ColorTextureTileMode(ColorTileMode::Z64KBX) == TextureTileMode::kZ64KBX, "a Z-order XOR color target did not map to its texture mode");
    Require(ColorTileModeIsXor(ColorTileMode::Z64KBX) && ColorTileModeBlockBytes(ColorTileMode::Z64KBX) == 65536u, "a Z-order XOR color target lost its block alignment or XOR flag");
    reject([] { DecodeColorTileMode((0x4dc60000u & ~(0x1fu << 19u)) | (1u << 19u)); });
    Require(ColorTextureTileMode(ColorTileMode::D64KBX) == TextureTileMode::kD64KBX, "a display XOR color target did not map to its texture mode");
    Require(ColorTileModeIsXor(ColorTileMode::D64KBX) && ColorTileModeBlockBytes(ColorTileMode::D64KBX) == 65536u, "a display XOR color target lost its block alignment or XOR flag");
    reject([] { DecodeColorTileMode(0x0cc68000u); });
    reject([] { DecodeColorTileMode((0x0dc68000u & ~(0x1fu << 19u)) | (1u << 19u)); });
    reject([] { ColorTargetLayout(64, 64, ColorTileMode::D64KBX, 4, 0x80); });
    reject([] { ColorTargetLayout(64, 64, ColorTileMode::D64KBX, 4, 0x10000); });
    for (const std::uint32_t columnBits : {0x1000u, 0x2000u, 0x3000u}) reject([columnBits] { ColorTargetLayout(64, 64, ColorTileMode::D64KBX, 4, columnBits); });
    const std::array<std::array<std::size_t, 2>, 5> sourceOffsets{{{27647u, 66048u}, {29694u, 212992u}, {62204u, 344064u}, {45304u, 540672u}, {110832u, 933888u}}};
    for (const std::uint32_t bpe : {1u, 2u, 4u, 8u, 16u}) {
        const ColorTargetLayout sourceLayout(257, 129, ColorTileMode::D64KBX, bpe, 0x4000);
        const auto& offsets = sourceOffsets[std::countr_zero(bpe)];
        for (std::uint32_t xorTag = 0; xorTag < 256u; ++xorTag) {
            if ((xorTag & ~0xcfu) != 0) continue;
            const ColorTargetLayout xorLayout(1, 1, ColorTileMode::D64KBX, bpe, xorTag << 8u);
            Require(xorLayout.Offset(0, 0) == (xorTag << 8u), "a display XOR color origin differs from its pipe/bank bits");
        }
        Require(sourceLayout.Offset(127, 63) == offsets[0] && sourceLayout.Offset(256, 128) == offsets[1], "display XOR color offsets differ from the AMD GFX10 non-RB+ source reference");
        const auto block = ThinBlockLayout(TextureTileMode::kD64KBX, bpe);
        const auto width = 2u * block[1] + 1u;
        const auto height = 2u * block[2] + 1u;
        for (const auto mode : {ColorTileMode::Z64KBX, ColorTileMode::D64KBX, ColorTileMode::RenderTarget}) {
            const auto* equation = FindTextureSwizzleEquation(static_cast<std::uint32_t>(mode), bpe);
            Require(equation != nullptr, "a 64 KiB color swizzle equation is missing");
            const ColorTargetLayout layout(width, height, mode, bpe, 0x4600);
            Require(layout.Alignment() == block[0] && layout.BlocksPerRow() == 3u && layout.Bytes() == ComputeSurfaceSize(ComputeElementMipLayout(ColorTextureTileMode(mode), bpe, width, height, 1), 1), "a 64 KiB color layout differs from its texture geometry");
            for (const auto x : {0u, 1u, block[1] - 1u, block[1], block[1] + 1u, width - 1u}) {
                for (const auto y : {0u, 1u, block[2] - 1u, block[2], block[2] + 1u, height - 1u}) {
                    std::size_t expected = 0;
                    for (std::uint32_t bit = 0; bit < 16u; ++bit) expected |= static_cast<std::size_t>(std::popcount((x & equation->bits[bit] & 0xfffu) ^ ((y << 12u) & equation->bits[bit] & 0xfff000u)) & 1) << bit;
                    const auto blockIndex = (y / block[2]) * 3u + x / block[1];
                    Require(layout.Offset(x, y) == blockIndex * 65536u + (expected ^ 0x4600u), "a 64 KiB color offset differs from its mode's equation across block boundaries");
                }
            }
            std::vector<std::byte> source(layout.LinearBytes());
            for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<std::byte>((index * 31u + index / 251u) & 255u);
            std::vector<std::byte> tiled(layout.Bytes());
            std::vector<std::byte> restored(source.size());
            layout.Tile(source, tiled);
            layout.Detile(tiled, restored);
            Require(restored == source, "64 KiB display/render-target XOR color tiling lost pixels");
        }
    }
    std::fill(guest.begin(), guest.end(), std::byte{0x6b});
    target = {reinterpret_cast<std::uintptr_t>(guest.data()), {2, 2}, VK_FORMAT_R8G8B8A8_UNORM, guest.size(), 0xe4, ColorTileMode::D64KBX};
    target.pipeBankXor = 0x0200;
    WriteColorTarget(target, pixels);
    readback.fill(std::byte{0});
    ReadColorTarget(target, readback);
    Require(readback == pixels && guest[0] == std::byte{0x6b}, "display XOR guest color transfer lost pixels or wrote the untagged block origin");
}
