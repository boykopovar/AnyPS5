#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using AgcDriver::DisplayBuffer;
using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::GuestTextureResource;
using AgcDriver::Graphics::TextureDimension;
using AgcDriver::Graphics::TextureTileMode;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename TAction>
std::string Rejection(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

void ExpectPixels(const DisplayBuffer& buffer, DccKeys keys, std::array<unsigned, 4> expected, const char* what) {
    const auto pixel = AgcDriver::DisplayBufferClearPixel(buffer, keys);
    for (std::size_t i = 0; i < pixel.size(); ++i) {
        if (std::to_integer<unsigned>(pixel[i]) == expected[i]) continue;
        Expect(false, std::string(what) + ": byte " + std::to_string(i) + " is " + std::to_string(std::to_integer<unsigned>(pixel[i])) + ", expected " + std::to_string(expected[i]));
        return;
    }
}

struct PatternBit {
    std::uint32_t x;
    std::uint32_t y;
};

constexpr PatternBit X(std::uint32_t bit) { return {1u << bit, 0}; }
constexpr PatternBit Y(std::uint32_t bit) { return {0, 1u << bit}; }
constexpr PatternBit operator^(PatternBit a, PatternBit b) { return {a.x ^ b.x, a.y ^ b.y}; }

using DccPattern = std::array<PatternBit, 12>;
constexpr DccPattern PipeAligned{X(3), Y(4), X(6), Y(6), X(7), Y(7), X(8), Y(8), X(3) ^ Y(3), X(4) ^ Y(4), X(6) ^ Y(5), X(5) ^ Y(6)};
constexpr DccPattern Displayable{X(3), Y(3), X(4), Y(4), X(5), Y(5), X(6), Y(6), X(7), Y(7), X(8), Y(8)};

std::size_t PatternKey(const DccPattern& pattern, std::uint32_t width, std::uint32_t x, std::uint32_t y) {
    std::size_t offset = 0;
    for (std::size_t bit = 0; bit < pattern.size(); ++bit) offset |= static_cast<std::size_t>((std::popcount(x & pattern[bit].x) + std::popcount(y & pattern[bit].y)) & 1) << bit;
    return (static_cast<std::size_t>(y / 512u) * ((width + 511u) / 512u) + x / 512u) * 4096u + offset;
}

void Retile(std::span<const std::uint8_t> render, std::span<std::uint8_t> display, std::uint32_t width, std::uint32_t height) {
    for (std::uint32_t y = 0; y < (height + 511u) / 512u * 512u; y += 8) {
        for (std::uint32_t x = 0; x < (width + 511u) / 512u * 512u; x += 8) display[PatternKey(Displayable, width, x, y)] = render[PatternKey(PipeAligned, width, x, y)];
    }
}

struct KeyHistogram {
    std::size_t uncompressed = 0;
    std::size_t clear0000 = 0;
    std::size_t firstDiffering = 0;
    std::size_t zeroRuns = 0;
    bool operator==(const KeyHistogram&) const = default;
};

KeyHistogram Histogram(std::span<const std::uint8_t> keys) {
    KeyHistogram histogram;
    for (std::size_t key = 0; key < keys.size(); ++key) {
        histogram.uncompressed += keys[key] == 0xff;
        histogram.clear0000 += keys[key] == 0x00;
        histogram.zeroRuns += keys[key] == 0x00 && (key == 0 || keys[key - 1] != 0x00);
        if (histogram.firstDiffering == 0 && keys[key] != keys[0]) histogram.firstDiffering = key;
    }
    return histogram;
}

std::string Describe(const KeyHistogram& histogram) {
    return std::to_string(histogram.uncompressed) + " x 0xff, " + std::to_string(histogram.clear0000) + " x 0x00, first differing key " + std::to_string(histogram.firstDiffering) + ", " + std::to_string(histogram.zeroRuns) + " runs of 0x00";
}

std::uint64_t TiledBytes(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint64_t>((width + 127u) / 128u) * ((height + 127u) / 128u) * 65536u;
}

GuestTextureResource Surface(std::uint32_t width, std::uint32_t height, std::uint64_t dccAddress, bool pipeAligned) {
    GuestTextureResource surface{};
    surface.baseAddress = 0x10000;
    surface.width = width;
    surface.height = height;
    surface.mipCount = 1;
    surface.tileMode = TextureTileMode::kR64KBX;
    surface.dimension = TextureDimension::k2D;
    surface.format = AgcDriver::Graphics::FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_UNORM, 4).value();
    surface.dccAddress = dccAddress;
    surface.dccPipeAligned = pipeAligned;
    return surface;
}

std::uint8_t* ReserveKeys(std::size_t writable, std::size_t reserved) {
#ifdef _WIN32
    auto* block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, reserved, MEM_RESERVE, PAGE_NOACCESS));
    if (block == nullptr || VirtualAlloc(block, writable, MEM_COMMIT, PAGE_READWRITE) == nullptr) return nullptr;
#else
    void* mapped = mmap(nullptr, reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED || mprotect(mapped, writable, PROT_READ | PROT_WRITE) != 0) return nullptr;
    auto* block = static_cast<std::uint8_t*>(mapped);
#endif
    return block;
}

void ReleaseKeys(std::uint8_t* block, std::size_t reserved) {
#ifdef _WIN32
    static_cast<void>(reserved);
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, reserved);
#endif
}

void KeyCountTests() {
    struct Extent {
        std::uint32_t width;
        std::uint32_t height;
        std::size_t keys;
    };
    for (const auto& extent : {Extent{1920, 1080, 49152}, Extent{3840, 2160, 163840}, Extent{1280, 720, 24576}, Extent{259, 137, 4096}, Extent{128, 128, 4096}, Extent{2560, 1440, 61440}}) {
        const auto surfaceBytes = TiledBytes(extent.width, extent.height);
        const auto name = std::to_string(extent.width) + "x" + std::to_string(extent.height);
        const auto keys = AgcDriver::Graphics::DccKeyCount(TextureTileMode::kR64KBX, 4, extent.width, extent.height, surfaceBytes);
        Expect(keys == extent.keys, name + " SW_64KB_R_X 4-byte DCC covers " + std::to_string(keys) + " key bytes, expected " + std::to_string(extent.keys));
        if (extent.keys == surfaceBytes / 256) continue;
        const auto aligned = AgcDriver::Graphics::DccKeyCount(Surface(extent.width, extent.height, 0, true), surfaceBytes);
        const auto unaligned = AgcDriver::Graphics::DccKeyCount(Surface(extent.width, extent.height, 0, false), surfaceBytes);
        Expect(aligned == extent.keys && unaligned == surfaceBytes / 256, name + " surface descriptors' DCC covers " + std::to_string(aligned) + " key bytes pipe-aligned and " + std::to_string(unaligned) + " unaligned, expected " + std::to_string(extent.keys) + " and " + std::to_string(surfaceBytes / 256));
    }
    Expect(TiledBytes(2560, 1440) / 256 == 61440, "2560x1440 does not fill its meta blocks");
    const auto bytes = TiledBytes(1920, 1080);
    Expect(AgcDriver::Graphics::DccKeyCount(TextureTileMode::kD64KBX, 4, 1920, 1080, bytes) == bytes / 256, "an SW_64KB_D_X surface's DCC left one key per 256 bytes");
    Expect(AgcDriver::Graphics::DccKeyCount(TextureTileMode::kR64KBX, 8, 1920, 1080, 2 * bytes) == 2 * bytes / 256, "an 8-byte surface's DCC left one key per 256 bytes");
    const auto surface = Surface(1920, 1080, 0, true);
    auto shape = surface;
    shape.mipCount = 2;
    Expect(AgcDriver::Graphics::DccKeyCount(shape, bytes + 65536) == (bytes + 65536) / 256, "a mip chain's DCC left one key per 256 bytes");
    shape = surface;
    shape.dimension = TextureDimension::k2DArray;
    shape.depthOrLastArray = 1;
    Expect(AgcDriver::Graphics::DccKeyCount(shape, 2 * bytes) == 2 * bytes / 256, "a two-layer surface's DCC left one key per 256 bytes");
    shape = surface;
    shape.format = AgcDriver::Graphics::FindGuestTextureFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 8).value();
    Expect(AgcDriver::Graphics::DccKeyCount(shape, 2 * bytes) == 2 * bytes / 256, "an 8-byte surface descriptor's DCC left one key per 256 bytes");
    Expect(AgcDriver::DisplayBufferKeyBytes({65536, 0x8000000000000000ull, 1920, 1080}) == 49152 && AgcDriver::DisplayBufferKeyBytes({65536, 0x8000000000000000ull, 259, 137}) == 4096, "display buffers do not read the console's DCC extent");
}

void RetileTests() {
    constexpr std::uint32_t width = 1920;
    constexpr std::uint32_t height = 1080;
    const auto surfaceBytes = TiledBytes(width, height);
    const auto driverKeys = static_cast<std::size_t>(surfaceBytes / 256);
    std::vector<std::uint8_t> render(49152);
    std::vector<std::uint8_t> display(render.size());
    std::fill_n(render.begin(), driverKeys, 0xff);
    Retile(render, display, width, height);
    const auto reported = Histogram(std::span(display).first(driverKeys));
    Expect(reported == KeyHistogram{33552, 1008, 32784, 105}, "a retile of 0xff over the first 34560 render keys does not give the PPSA12544 display keys: " + Describe(reported));
    std::fill(render.begin(), render.end(), 0x00);
    std::fill(display.begin(), display.end(), 0x00);
    const auto surface = Surface(width, height, reinterpret_cast<std::uint64_t>(render.data()), true);
    AgcDriver::Graphics::MarkDccUncompressed(surface.dccAddress, surfaceBytes, AgcDriver::Graphics::DccKeyCount(surface, surfaceBytes));
    Retile(render, display, width, height);
    DisplayBuffer buffer{65536, 0x8000000000000000ull, width, height};
    buffer.dccAddress = reinterpret_cast<std::uint64_t>(display.data());
    auto keys = DccKeys::Unreadable;
    const auto message = Rejection([&] { keys = AgcDriver::DisplayBufferKeys(buffer); });
    Expect(message.empty() && keys == DccKeys::Uncompressed, "the driver's store over a 1920x1080 render DCC, retiled, does not present: " + message + " (display keys over the first " + std::to_string(driverKeys) + ": " + Describe(Histogram(std::span(display).first(driverKeys))) + ")");

    constexpr std::size_t reserved = 65536;
    constexpr std::size_t writable = 0x9000;
    auto* small = ReserveKeys(writable, reserved);
    Expect(small != nullptr, "cannot reserve metadata with an inaccessible tail");
    if (small == nullptr) return;
    const auto stored = Rejection([&] { AgcDriver::Graphics::MarkDccUncompressed(reinterpret_cast<std::uint64_t>(small), surfaceBytes, AgcDriver::Graphics::DccKeyCount(surface, surfaceBytes)); });
    Expect(stored.find("not writable over the 0xc000 key bytes") != std::string::npos && small[0] == 0x00, "a key store over metadata smaller than the console's extent did not throw: " + stored);
    const auto unaligned = Rejection([&] { AgcDriver::Graphics::MarkDccUncompressed(reinterpret_cast<std::uint64_t>(small), surfaceBytes, AgcDriver::Graphics::DccKeyCount(Surface(width, height, reinterpret_cast<std::uint64_t>(small), false), surfaceBytes)); });
    Expect(unaligned.empty() && std::all_of(small, small + driverKeys, [](std::uint8_t key) { return key == 0xff; }) && small[driverKeys] == 0x00, "an unaligned DCC key store over metadata smaller than the console's extent did not cover one key per 256 bytes: " + unaligned);
    ReleaseKeys(small, reserved);
}

DccKeys ReadKeys(DisplayBuffer buffer, std::vector<std::uint8_t>& keys) {
    buffer.dccAddress = reinterpret_cast<std::uint64_t>(keys.data());
    return AgcDriver::DisplayBufferKeys(buffer);
}

void ExtentTests() {
    const DisplayBuffer full{65536, 0x8000000000000000ull, 1920, 1080};
    std::vector<std::uint8_t> keys(49152);
    for (const auto& [code, expected] : {std::pair{std::uint8_t{0xff}, DccKeys::Uncompressed}, std::pair{std::uint8_t{0x00}, DccKeys::Clear0000}, std::pair{std::uint8_t{0xc0}, DccKeys::Clear1111}, std::pair{std::uint8_t{0x20}, DccKeys::ClearRegister}}) {
        std::fill(keys.begin(), keys.end(), code);
        auto read = DccKeys::Unreadable;
        const auto message = Rejection([&] { read = ReadKeys(full, keys); });
        Expect(message.empty() && read == expected, std::string("uniform ") + AgcDriver::Graphics::DccKeysName(expected) + " keys over the extent read " + AgcDriver::Graphics::DccKeysName(read) + ": " + message);
    }
    std::fill(keys.begin(), keys.end(), 0xff);
    const auto corner = PatternKey(Displayable, 1920, 1919, 1079);
    Expect(corner == 46205, "pixel (1919, 1079) is not displayable key 46205");
    keys[corner] = 0x00;
    auto message = Rejection([&] { ReadKeys(full, keys); });
    Expect(message.find("mixed") != std::string::npos && message.find("key 46205 (pixels 1912..1919 x 1072..1079 in the displayable order) is 0x00, key 0 is 0xff") != std::string::npos, "a 0000 key past one key per 256 bytes was not read or not named: " + message);
    std::fill(keys.begin(), keys.end(), 0x40);
    keys[0] = 0x80;
    message = Rejection([&] { ReadKeys(full, keys); });
    Expect(message.find("key 1 (pixels 8..15 x 0..7 in the displayable order) is 0x40, key 0 is 0x80") != std::string::npos, "a 1110 key 0 among 0001 keys was not named: " + message);
    const DisplayBuffer odd{65536, 0x8000000000000000ull, 259, 137};
    keys.assign(4096, 0x40);
    keys.back() = 0xff;
    message = Rejection([&] { ReadKeys(odd, keys); });
    Expect(message.find("key 4095 (pixels 504..511 x 504..511 in the displayable order) is 0xff") != std::string::npos, "259x137: an uncompressed padding key among 0001 keys was ignored: " + message);
    Expect(!Rejection([&] { AgcDriver::DisplayBufferKeys(full); }).empty(), "a display buffer without DCC metadata had keys read");
}

}

int main() {
    constexpr std::uint64_t bgra = 0x8000000000000000ull;
    constexpr std::uint64_t rgba = 0x8000000022000000ull;
    constexpr std::uint64_t tenBit = 0x0100000000000000ull;
    DisplayBuffer buffer{65536, bgra, 131, 3, 0, 0, 0x7f0000, 0x11223344};
    ExpectPixels(buffer, DccKeys::Clear0000, {0, 0, 0, 0}, "8-bit 0000");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "8-bit 0001");
    ExpectPixels(buffer, DccKeys::Clear1110, {255, 255, 255, 0}, "8-bit 1110");
    ExpectPixels(buffer, DccKeys::Clear1111, {255, 255, 255, 255}, "8-bit 1111");
    ExpectPixels(buffer, DccKeys::ClearRegister, {0x44, 0x33, 0x22, 0x11}, "B8G8R8A8 register clear");
    buffer.pixelFormat = rgba;
    ExpectPixels(buffer, DccKeys::ClearRegister, {0x22, 0x33, 0x44, 0x11}, "R8G8B8A8 register clear");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "R8G8B8A8 0001");
    buffer.pixelFormat = rgba | tenBit;
    ExpectPixels(buffer, DccKeys::Clear1110, {255, 255, 255, 255}, "A2B10G10R10 1110");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "A2B10G10R10 0001");
    buffer.dccClearColor = 0x3ff;
    ExpectPixels(buffer, DccKeys::ClearRegister, {0, 0, 255, 255}, "A2B10G10R10 register clear");
    buffer.pixelFormat = bgra | tenBit;
    ExpectPixels(buffer, DccKeys::ClearRegister, {255, 0, 0, 255}, "A2R10G10B10 register clear");
    for (const auto keys : {DccKeys::Uncompressed, DccKeys::Mixed, DccKeys::Unreadable}) {
        const auto message = Rejection([&] { AgcDriver::DisplayBufferClearPixel(buffer, keys); });
        Expect(message.find(AgcDriver::Graphics::DccKeysName(keys)) != std::string::npos && message.find("0x7f0000") != std::string::npos, std::string("keys without a clear value were presented as one: ") + AgcDriver::Graphics::DccKeysName(keys));
    }
    buffer.dccClearColor = 0x100000000ull;
    Expect(Rejection([&] { AgcDriver::DisplayBufferClearPixel(buffer, DccKeys::ClearRegister); }).find("register clear color") != std::string::npos, "a register clear color wider than the texel was truncated");
    buffer.dccClearColor = 0;
    buffer.dccAddress = 0;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferClearPixel(buffer, DccKeys::Clear0000); }).empty(), "a buffer without DCC metadata was presented as fast-cleared");
    buffer.dccClearColor = 1;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferSize(buffer); }).empty(), "a DCC clear color without metadata was accepted");
    buffer.dccClearColor = 0;
    buffer.dccAddress = 0x7f0000;
    buffer.tilingMode = 1;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferSize(buffer); }).empty(), "a linear display buffer with DCC metadata was accepted");
    buffer.tilingMode = 0;
    Expect(AgcDriver::DisplayBufferSize(buffer) == 2 * 65536, "DCC metadata changed the display footprint");
    KeyCountTests();
    RetileTests();
    ExtentTests();
    if (failures != 0) return 1;
    std::puts("display buffer DCC tests passed");
    return 0;
}
