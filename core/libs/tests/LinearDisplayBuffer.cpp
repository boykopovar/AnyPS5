#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/HalfFloatScanout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <vector>

static void Require(bool condition) { if (!condition) std::abort(); }
template<typename TAction> static void Reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}

static std::size_t EquationOffset(std::uint32_t elementBytes, std::uint32_t blocksPerRow, std::uint32_t blockHeight, std::uint32_t x, std::uint32_t y) {
    const auto* equation = AgcDriver::Graphics::FindTextureSwizzleEquation(27u, elementBytes);
    Require(equation != nullptr);
    std::size_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16u; ++bit) {
        const auto mask = equation->bits[bit];
        offset |= static_cast<std::size_t>(std::popcount((x & mask & 0xfffu) ^ ((y << 12u) & mask & 0xfff000u)) & 1) << bit;
    }
    const auto blockWidth = 65536u / elementBytes / blockHeight;
    return (static_cast<std::size_t>(y / blockHeight) * blocksPerRow + x / blockWidth) * 65536u + offset;
}

static void HalfFloatCodes() {
    for (std::uint32_t bits = 0; bits <= 0xffffu; ++bits) {
        const auto exponent = static_cast<int>((bits >> 10u) & 0x1fu);
        const auto mantissa = static_cast<double>(bits & 0x3ffu);
        const double magnitude = exponent == 0 ? std::ldexp(mantissa, -24) : exponent == 31 ? (mantissa == 0 ? INFINITY : NAN) : std::ldexp(1024.0 + mantissa, exponent - 25);
        const double value = (bits & 0x8000u) != 0u ? -magnitude : magnitude;
        unsigned color = 0;
        unsigned alpha = 0;
        if (value > 0) {
            const double clamped = std::min(value, 1.0);
            const double encoded = clamped <= 0.0031308 ? 12.92 * clamped : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
            color = static_cast<unsigned>(std::floor(encoded * 255.0 + 0.5));
            alpha = static_cast<unsigned>(std::floor(clamped * 255.0 + 0.5));
        }
        Require(AgcDriver::Graphics::HalfFloatScanoutCode(static_cast<std::uint16_t>(bits), false) == color);
        Require(AgcDriver::Graphics::HalfFloatScanoutCode(static_cast<std::uint16_t>(bits), true) == alpha);
    }
}

static void HalfFloatDisplay() {
    constexpr std::uint64_t rgba16Float = 0xc001000622000000ull;
    const std::array<std::uint16_t, 24> halves{0x3c00, 0x3800, 0x0000, 0x3c00, 0xbc00, 0x4000, 0x7e00, 0x3800, 0x1234, 0x1234, 0x1234, 0x1234,
        0x7c00, 0x3400, 0x1419, 0x0000, 0x0001, 0x3555, 0x3c00, 0x3c00, 0x1234, 0x1234, 0x1234, 0x1234};
    std::array<std::byte, 48> input{};
    std::memcpy(input.data(), halves.data(), input.size());
    AgcDriver::DisplayBuffer buffer{65536, rgba16Float, 2, 2, 1, 3};
    Require(AgcDriver::DisplayBufferSize(buffer) == input.size());
    const auto pixels = AgcDriver::DecodeDisplayBuffer(buffer, input);
    const std::array<unsigned, 16> expected{0, 188, 255, 255, 0, 255, 0, 128, 3, 137, 255, 0, 255, 156, 0, 255};
    Require(pixels.size() == expected.size());
    for (unsigned i = 0; i < pixels.size(); ++i) Require(std::to_integer<unsigned>(pixels[i]) == expected[i]);
    Require(AgcDriver::DisplayBufferOffset(buffer, 1, 1) == 32);
    buffer.pixelFormat = 0xc001000600000000ull;
    const auto swapped = AgcDriver::DecodeDisplayBuffer(buffer, input);
    const std::array<unsigned, 16> blueFirst{255, 188, 0, 255, 0, 255, 0, 128, 255, 137, 3, 0, 0, 156, 255, 255};
    for (unsigned i = 0; i < swapped.size(); ++i) Require(std::to_integer<unsigned>(swapped[i]) == blueFirst[i]);
    Require(AgcDriver::DisplayHalfFloat(buffer.pixelFormat) && !AgcDriver::DisplayRedLow(buffer.pixelFormat) && AgcDriver::DisplayTexelFormat(buffer.pixelFormat) == VK_FORMAT_R16G16B16A16_SFLOAT);
    Require(AgcDriver::DisplayTexelFormat(rgba16Float) == VK_FORMAT_R16G16B16A16_SFLOAT && AgcDriver::DisplayTexelBytes(rgba16Float) == 8 && AgcDriver::DisplayRedLow(rgba16Float) && !AgcDriver::DisplayTenBit(rgba16Float));
    Require(AgcDriver::ResidentPresentPath(VK_FORMAT_R16G16B16A16_SFLOAT, rgba16Float, true) == AgcDriver::ResidentPresent::Convert);
    Require(AgcDriver::ResidentPresentPath(VK_FORMAT_R8G8B8A8_UNORM, rgba16Float, true) == AgcDriver::ResidentPresent::None);
    Require(AgcDriver::ResidentPresentPath(VK_FORMAT_R16G16B16A16_SFLOAT, 0x8000000022000000ull, true) == AgcDriver::ResidentPresent::None);
    buffer = {65536, rgba16Float, 3840, 2160};
    Require(AgcDriver::DisplayBufferSize(buffer) == 30u * 34u * 65536u);
    buffer.dccAddress = 0x20000;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.dccAddress = 0;
    buffer.pixelFormat = 0xc001000700000000ull;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    for (const auto [pixelFormat, elementBytes, blockHeight] : {std::tuple{rgba16Float, 8u, 64u}, std::tuple{std::uint64_t{0x8000000000000000ull}, 4u, 128u}}) {
        buffer = {65536, pixelFormat, 300, 200};
        for (std::uint32_t y = 0; y < buffer.height; ++y) {
            for (std::uint32_t x = 0; x < buffer.width; ++x) Require(AgcDriver::DisplayBufferOffset(buffer, x, y) == EquationOffset(elementBytes, 3, blockHeight, x, y));
        }
    }
}

static double ReferencePqNits(std::uint32_t code) {
    const double m1 = 2610.0 / 16384.0, m2 = 2523.0 / 4096.0 * 128.0, c1 = 3424.0 / 4096.0, c2 = 2413.0 / 4096.0 * 32.0, c3 = 2392.0 / 4096.0 * 32.0;
    const double e = std::pow(code / 1023.0, 1.0 / m2);
    return 10000.0 * std::pow(std::max(e - c1, 0.0) / (c2 - c3 * e), 1.0 / m1);
}

static std::array<double, 3> ReferenceSdr(std::uint32_t red, std::uint32_t green, std::uint32_t blue, double whiteNits) {
    const double referenceWhite = std::min(203.0, whiteNits);
    const double r = ReferencePqNits(red) / referenceWhite, g = ReferencePqNits(green) / referenceWhite, b = ReferencePqNits(blue) / referenceWhite;
    std::array<double, 3> rgb{std::max(1.6604910 * r - 0.5876411 * g - 0.0728499 * b, 0.0), std::max(-0.1245505 * r + 1.1328999 * g - 0.0083494 * b, 0.0), std::max(-0.0181508 * r - 0.1005789 * g + 1.1187297 * b, 0.0)};
    const double luminance = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    const double knee = 0.75;
    if (luminance > knee) {
        const double t = (luminance - knee) / (1.0 - knee), w = (whiteNits / referenceWhite - knee) / (1.0 - knee);
        const double mapped = knee + (1.0 - knee) * t * (1.0 + t / (w * w)) / (1.0 + t);
        for (auto& channel : rgb) channel *= mapped / luminance;
    }
    for (auto& channel : rgb) {
        const double v = std::clamp(channel, 0.0, 1.0);
        channel = 255.0 * (v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055);
    }
    return rgb;
}

static std::uint32_t PqWord(std::uint32_t red, std::uint32_t green, std::uint32_t blue, bool redLow) {
    return (redLow ? red : blue) | (green << 10u) | ((redLow ? blue : red) << 20u) | (3u << 30u);
}

static void PqDisplay() {
    constexpr std::uint64_t rgbPq = 0x8100070422000000ull;
    constexpr std::uint64_t bgrPq = 0x8100070400000000ull;
    for (const auto pixelFormat : {rgbPq, bgrPq}) {
        const bool redLow = pixelFormat == rgbPq;
        Require(AgcDriver::DisplayPq(pixelFormat) && AgcDriver::DisplayTenBit(pixelFormat) && !AgcDriver::DisplayHalfFloat(pixelFormat) && AgcDriver::DisplayRedLow(pixelFormat) == redLow);
        Require(AgcDriver::DisplayTexelBytes(pixelFormat) == 4 && AgcDriver::DisplayTexelFormat(pixelFormat) == (redLow ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_A2R10G10B10_UNORM_PACK32));
        for (const auto storage : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) Require(AgcDriver::ResidentPresentPath(storage, pixelFormat, true) == AgcDriver::ResidentPresent::Convert);
        Require(AgcDriver::ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, pixelFormat, true) == AgcDriver::ResidentPresent::None && AgcDriver::ResidentPresentPath(VK_FORMAT_R16G16B16A16_SFLOAT, pixelFormat, true) == AgcDriver::ResidentPresent::None);
        Require(AgcDriver::DisplayBufferSize({65536, pixelFormat, 3840, 2160}) == 30u * 17u * 65536u);
        struct Anchor {
            std::uint32_t red, green, blue;
            unsigned r, g, b;
        };
        const std::array<Anchor, 8> anchors{{{0, 0, 0, 0, 0, 0}, {1023, 1023, 1023, 255, 255, 255}, {520, 520, 520, 255, 255, 255}, {450, 450, 450, 187, 187, 187},
            {600, 500, 300, 255, 213, 0}, {480, 470, 350, 225, 204, 95}, {350, 420, 500, 16, 167, 241}, {1023, 0, 0, 255, 0, 0}}};
        std::array<std::byte, 4 * anchors.size()> input{};
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            const auto word = PqWord(anchors[i].red, anchors[i].green, anchors[i].blue, redLow);
            std::memcpy(input.data() + i * 4, &word, 4);
        }
        const AgcDriver::DisplayBuffer buffer{65536, pixelFormat, 4, 2, 1, 0};
        Require(AgcDriver::DisplayBufferSize(buffer) == input.size());
        const auto pixels = AgcDriver::DecodeDisplayBuffer(buffer, input);
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            Require(std::to_integer<unsigned>(pixels[i * 4]) == anchors[i].b && std::to_integer<unsigned>(pixels[i * 4 + 1]) == anchors[i].g && std::to_integer<unsigned>(pixels[i * 4 + 2]) == anchors[i].r && std::to_integer<unsigned>(pixels[i * 4 + 3]) == 255);
        }
    }
    struct WhiteAnchor {
        std::uint32_t red, green, blue;
        unsigned r, g, b;
    };
    const std::array<WhiteAnchor, 6> shoulder{{{770, 770, 770, 255, 255, 255}, {700, 700, 700, 252, 252, 252}, {650, 650, 650, 249, 249, 249}, {560, 560, 560, 221, 221, 221}, {450, 450, 450, 136, 136, 136}, {600, 500, 300, 255, 155, 0}}};
    for (const auto& anchor : shoulder) {
        const auto pixel = AgcDriver::PqToSdrPixel(PqWord(anchor.red, anchor.green, anchor.blue, true), true, 1000.0f);
        Require(pixel[2] == anchor.r && pixel[1] == anchor.g && pixel[0] == anchor.b && pixel[3] == 255);
    }
    for (const auto pixelFormat : {0x8100070000000000ull, 0x8100000400000000ull, 0x8000070400000000ull, 0x8100070400000001ull, 0x8100070600000000ull, 0xc001070700000000ull}) {
        Require(!AgcDriver::DisplayPq(pixelFormat));
        Reject([&] { AgcDriver::DisplayBufferSize({65536, pixelFormat, 64, 64}); });
    }
    for (const float white : {100.0f, 150.0f, 1000.0f}) {
        for (std::uint32_t red = 0; red < 1024; red += 31) {
            for (std::uint32_t green = 0; green < 1024; green += 31) {
                for (std::uint32_t blue = 0; blue < 1024; blue += 31) {
                    const auto expected = ReferenceSdr(red, green, blue, white);
                    const auto pixel = AgcDriver::PqToSdrPixel(PqWord(red, green, blue, true), true, white);
                    Require(std::fabs(pixel[2] - expected[0]) <= 1.0 && std::fabs(pixel[1] - expected[1]) <= 1.0 && std::fabs(pixel[0] - expected[2]) <= 1.0 && pixel[3] == 255);
                    Require(AgcDriver::PqToSdrPixel(PqWord(red, green, blue, false), false, white) == pixel);
                }
            }
        }
    }
}

static void GcnDisplay() {
    AgcDriver::DisplayBuffer buffer{65536, 0x8000000000000000ull, 1920, 1024, AgcDriver::DisplayTilingGcn, 1920};
    Require(AgcDriver::DisplayBufferOffset(buffer, 4, 0) == 32 && AgcDriver::DisplayBufferOffset(buffer, 0, 1) == 16);
    std::vector<std::byte> source(AgcDriver::DisplayBufferSize(buffer));
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) {
            const std::uint32_t texel = x | y << 16u;
            std::memcpy(source.data() + AgcDriver::DisplayBufferOffset(buffer, x, y), &texel, sizeof(texel));
        }
    }
    const auto pixels = AgcDriver::DecodeDisplayBuffer(buffer, source);
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) {
            const std::uint32_t texel = x | y << 16u;
            Require(std::memcmp(pixels.data() + (static_cast<std::size_t>(y) * buffer.width + x) * 4u, &texel, sizeof(texel)) == 0);
        }
    }
}

int main() {
    AgcDriver::DisplayBuffer buffer{65536, 0x8000000000000000ull, 2, 2, 1, 3};
    const std::array<unsigned char, 24> input{1,2,3,4, 5,6,7,8, 99,99,99,99,
        9,10,11,12, 13,14,15,16, 99,99,99,99};
    const auto source = std::as_bytes(std::span(input));
    Require(AgcDriver::DisplayBufferSize(buffer) == input.size());
    auto pixels = AgcDriver::DecodeDisplayBuffer(buffer, source);
    Require(pixels.size() == 16);
    for (unsigned i = 0; i < pixels.size(); ++i) Require(std::to_integer<unsigned>(pixels[i]) == i + 1);
    buffer.pixelFormat = 0x8000000022000000ull;
    pixels = AgcDriver::DecodeDisplayBuffer(buffer, source);
    const std::array<unsigned char, 16> swapped{3,2,1,4, 7,6,5,8, 11,10,9,12, 15,14,13,16};
    for (unsigned i = 0; i < pixels.size(); ++i) Require(std::to_integer<unsigned>(pixels[i]) == swapped[i]);
    Reject([&] { AgcDriver::DecodeDisplayBuffer(buffer, source.first(23)); });
    buffer.pitchInPixel = 1;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.pitchInPixel = 0;
    Require(AgcDriver::DisplayBufferSize(buffer) == 16);
    buffer.tilingMode = 2;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.tilingMode = 0;
    Require(AgcDriver::DisplayBufferSize(buffer) == 65536);
    HalfFloatCodes();
    HalfFloatDisplay();
    PqDisplay();
    GcnDisplay();
}
