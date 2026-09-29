#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
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
    reject([] { DecodeColorTileMode(0x09014000); });
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
    // Aligned by hand: a PE section cannot ask for 64 KiB alignment.
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
