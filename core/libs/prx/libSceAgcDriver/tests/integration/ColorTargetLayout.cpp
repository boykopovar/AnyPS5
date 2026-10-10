#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
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
    reject([] { DecodeColorTileMode(0x4dc6c001); });
    reject([] { DecodeColorTileMode(0x4dc6e000); });
    reject([] { DecodeColorTileMode(0xcdc6c000); });
    reject([] { DecodeColorTileMode(0x09004000); });
    Require(DecodeColorTileMode(0x4cc6c000) == ColorTileMode::RenderTarget, "1D color descriptor was rejected");
    reject([] { DecodeColorTileMode(0x4cc6c001); });
    reject([] { DecodeColorTileMode(0x4fc6c000); });
    Require(DecodeColorTileMode(0x08000000) == ColorTileMode::Linear, "linear 1D color descriptor was rejected");
    reject([] { DecodeColorTileMode(0x08014000); });
    reject([] { DecodeColorTileMode(0x08024000); });
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
    Require(DecodeColorTileMode(0x4dc14000) == ColorTileMode::Standard4KB, "4 KiB standard color descriptor was rejected");
    const ColorTargetLayout standard(256, 256, ColorTileMode::Standard4KB);
    Require(standard.Bytes() == ComputeSurfaceSize(ComputeElementMipLayout(TextureTileMode::kStandard4KB, 4, 256, 256, 1), 1) && standard.Alignment() == 4096, "4 KiB standard color layout differs from the texture layout");
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
}
