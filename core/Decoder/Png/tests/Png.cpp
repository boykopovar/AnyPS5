#include "Decoder/Png.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

static void Require(bool value) { if (!value) std::abort(); }

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static const std::uint8_t PALETTE_TRNS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x0F, 0xD8, 0xE5,
    0xB7, 0x00, 0x00, 0x00, 0x0C, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xD6, 0x02, 0x8F, 0x7B, 0x00, 0x00, 0x00, 0x04, 0x74, 0x52, 0x4E,
    0x53, 0xFF, 0x00, 0xFF, 0x80, 0x13, 0x0A, 0x1E, 0x39, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x63, 0x10, 0x60, 0xD8, 0x00, 0x00, 0x00, 0xE4, 0x00, 0xC1, 0x27, 0xA8, 0xE8,
    0x57, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t GRAY16[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x81, 0xD9, 0xFC,
    0x15, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32, 0x59, 0x7D,
    0x16, 0x00, 0x03, 0x0C, 0x01, 0xBF, 0x6E, 0xB9, 0xC6, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t GRAY1[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xCB, 0x7B, 0xD2,
    0xEE, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x88, 0x04, 0x00, 0x00,
    0x5B, 0x00, 0x5A, 0x7C, 0xA5, 0x93, 0x54, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
    0x42, 0x60, 0x82,
};

static void AppendBigEndian32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

static std::vector<std::uint8_t> HeaderOnly(std::uint32_t width, std::uint32_t height, std::uint8_t bitDepth,
                                            std::uint8_t colorType, std::uint8_t interlace) {
    std::vector<std::uint8_t> bytes{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    AppendBigEndian32(bytes, width);
    AppendBigEndian32(bytes, height);
    bytes.insert(bytes.end(), {bitDepth, colorType, 0, 0, interlace, 0, 0, 0, 0});
    bytes.insert(bytes.end(), {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0});
    return bytes;
}

static void RequireRoundTrip(std::uint32_t channels, Decoder::Png::ColorType colorType) {
    constexpr std::uint32_t width = 20;
    constexpr std::uint32_t height = 10;
    std::vector<std::uint8_t> pixels(width * height * channels);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(i * 37 + 11);

    const std::vector<std::uint8_t> png = Decoder::Png::Encode(pixels, width, height, channels);
    const auto header = Decoder::Png::ParseHeader(png);
    Require(header.has_value());
    Require(header->width == width && header->height == height && header->bitDepth == 8);
    Require(header->colorType == colorType && !header->interlaced && !header->hasTransparency);

    const auto image = Decoder::Png::Decode(png);
    Require(image.has_value());
    Require(image->width == width && image->height == height && image->pixels.size() == width * height * 4);
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        const std::uint8_t* in = &pixels[pixel * channels];
        const std::uint8_t* out = &image->pixels[pixel * 4];
        const bool gray = channels <= 2;
        Require(out[0] == in[0]);
        Require(out[1] == (gray ? in[0] : in[1]));
        Require(out[2] == (gray ? in[0] : in[2]));
        const bool hasAlpha = channels == 2 || channels == 4;
        Require(out[3] == (hasAlpha ? in[channels - 1] : 255));
    }
}

static std::vector<std::uint8_t> RowFilters(const std::vector<std::uint8_t>& png, std::uint32_t height, std::uint32_t rowSize) {
    std::vector<std::uint8_t> stream;
    for (std::size_t offset = 8; offset + 12 <= png.size();) {
        const std::size_t length = static_cast<std::size_t>(png[offset]) << 24 | static_cast<std::size_t>(png[offset + 1]) << 16
            | static_cast<std::size_t>(png[offset + 2]) << 8 | png[offset + 3];
        Require(length <= png.size() - offset - 12);
        const std::uint8_t* data = png.data() + offset + 8;
        if (std::equal(data - 4, data, "IDAT")) stream.insert(stream.end(), data, data + length);
        offset += 12 + length;
    }

    int size = 0;
    char* rows = stbi_zlib_decode_malloc(reinterpret_cast<const char*>(stream.data()), static_cast<int>(stream.size()), &size);
    Require(rows && static_cast<std::size_t>(size) == static_cast<std::size_t>(height) * (rowSize + 1));
    std::vector<std::uint8_t> filters;
    for (std::uint32_t y = 0; y < height; ++y) filters.push_back(static_cast<std::uint8_t>(rows[y * (rowSize + 1)]));
    stbi_image_free(rows);
    return filters;
}

static void RequireFilterSets(std::uint32_t channels) {
    constexpr std::uint32_t width = 16;
    constexpr std::uint32_t height = 6;
    const std::uint32_t rowSize = width * channels;
    std::vector<std::uint8_t> pixels(rowSize * height);
    std::uint32_t noise = 12345;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t i = 0; i < rowSize; ++i) {
            noise = noise * 1103515245u + 12345u;
            pixels[y * rowSize + i] = y == 2 ? static_cast<std::uint8_t>(noise >> 16) : static_cast<std::uint8_t>(i / channels * 8);
        }
    }
    std::copy_n(pixels.begin() + 2 * rowSize, rowSize, pixels.begin() + 3 * rowSize);

    for (std::uint8_t filters = 1; filters <= Decoder::Png::FILTER_ALL; ++filters) {
        const std::vector<std::uint8_t> png = Decoder::Png::Encode(pixels, width, height, channels, {6, filters});
        for (const std::uint8_t filter : RowFilters(png, height, rowSize)) Require(filter < 5 && (filters >> filter & 1) != 0);

        const auto image = Decoder::Png::Decode(png);
        Require(image.has_value() && image->width == width && image->height == height);
        for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
            Require(image->pixels[pixel * 4] == pixels[pixel * channels]);
            Require(image->pixels[pixel * 4 + 3] == (channels % 2 == 0 ? pixels[pixel * channels + channels - 1] : 255));
        }
    }

    const std::vector<std::uint8_t> subOrUp = RowFilters(
        Decoder::Png::Encode(pixels, width, height, channels, {6, Decoder::Png::FILTER_SUB | Decoder::Png::FILTER_UP}), height, rowSize);
    Require(subOrUp[0] == 1 && subOrUp[1] == 2 && subOrUp[3] == 2 && subOrUp[4] == 1 && subOrUp[5] == 2);
}

int main() {
    RequireRoundTrip(1, Decoder::Png::ColorType::Grayscale);
    RequireRoundTrip(2, Decoder::Png::ColorType::GrayscaleAlpha);
    RequireRoundTrip(3, Decoder::Png::ColorType::Rgb);
    RequireRoundTrip(4, Decoder::Png::ColorType::Rgba);
    for (const std::uint32_t channels : {1, 2, 3, 4}) RequireFilterSets(channels);

    const auto paletteHeader = Decoder::Png::ParseHeader(PALETTE_TRNS);
    Require(paletteHeader.has_value());
    Require(paletteHeader->width == 2 && paletteHeader->height == 2 && paletteHeader->bitDepth == 2);
    Require(paletteHeader->colorType == Decoder::Png::ColorType::Palette && paletteHeader->hasTransparency);
    for (const std::size_t size : {std::size_t{69}, std::size_t{71}}) {
        const auto truncated = Decoder::Png::ParseHeader(std::span<const std::uint8_t>(PALETTE_TRNS, size));
        Require(truncated.has_value() && !truncated->hasTransparency);
    }
    const auto completeTransparency = Decoder::Png::ParseHeader(std::span<const std::uint8_t>(PALETTE_TRNS, 73));
    Require(completeTransparency.has_value() && completeTransparency->hasTransparency);
    const auto palette = Decoder::Png::Decode(PALETTE_TRNS);
    Require(palette.has_value());
    Require(palette->pixels == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 128}));

    const auto gray16Header = Decoder::Png::ParseHeader(GRAY16);
    Require(gray16Header.has_value() && gray16Header->bitDepth == 16 && gray16Header->colorType == Decoder::Png::ColorType::Grayscale);
    const auto gray16 = Decoder::Png::Decode(GRAY16);
    Require(gray16.has_value());
    Require(gray16->pixels == std::vector<std::uint8_t>({0x12, 0x12, 0x12, 255, 0xAB, 0xAB, 0xAB, 255}));

    const auto gray1Header = Decoder::Png::ParseHeader(GRAY1);
    Require(gray1Header.has_value() && gray1Header->bitDepth == 1 && gray1Header->width == 8);
    const auto gray1 = Decoder::Png::Decode(GRAY1);
    Require(gray1.has_value());
    const std::uint8_t levels[8] = {0, 255, 0, 255, 255, 0, 0, 255};
    for (int i = 0; i < 8; ++i) Require(gray1->pixels[i * 4] == levels[i] && gray1->pixels[i * 4 + 3] == 255);

    const auto interlaced = Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 6, 1));
    Require(interlaced.has_value() && interlaced->interlaced && !interlaced->hasTransparency);
    Require(!Decoder::Png::ParseHeader(HeaderOnly(0, 4, 8, 6, 0)).has_value());
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 5, 0)).has_value());
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 16, 3, 0)).has_value());
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 6, 2)).has_value());
    std::vector<std::uint8_t> badSignature = HeaderOnly(3, 4, 8, 6, 0);
    badSignature[1] = 'X';
    Require(!Decoder::Png::ParseHeader(badSignature).has_value());
    std::vector<std::uint8_t> hugeChunk = HeaderOnly(3, 4, 8, 6, 0);
    hugeChunk[33] = 0xFF;
    hugeChunk[37] = 't';
    hugeChunk[38] = 'E';
    hugeChunk[39] = 'X';
    hugeChunk[40] = 't';
    const auto hugeChunkHeader = Decoder::Png::ParseHeader(hugeChunk);
    Require(hugeChunkHeader.has_value() && !hugeChunkHeader->hasTransparency);
    Require(!Decoder::Png::ParseHeader(std::span<const std::uint8_t>(PALETTE_TRNS, 32)).has_value());

    Require(!Decoder::Png::Decode({}).has_value());
    Require(!Decoder::Png::Decode(std::span<const std::uint8_t>(PALETTE_TRNS, 60)).has_value());
    const std::vector<std::uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8};
    Require(!Decoder::Png::Decode(garbage).has_value());

    const std::vector<std::uint8_t> pixels(16 * 4);
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 4, 0); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 4, 5); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 0, 4, 4); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 5, 4); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 4, 4, {10, Decoder::Png::FILTER_ALL}); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 4, 4, {8, 0}); }));
    Require(ThrowsInvalidArgument([&] { Decoder::Png::Encode(pixels, 4, 4, 4, {8, Decoder::Png::FILTER_ALL + 1}); }));

    std::vector<std::uint8_t> gradient(64 * 64 * 3);
    for (std::size_t i = 0; i < gradient.size(); ++i) gradient[i] = static_cast<std::uint8_t>(i / 3 % 64 * 4);
    const auto stored = Decoder::Png::Encode(gradient, 64, 64, 3, {0, Decoder::Png::FILTER_NONE});
    const auto compressed = Decoder::Png::Encode(gradient, 64, 64, 3, {9, Decoder::Png::FILTER_SUB});
    Require(compressed.size() < stored.size());
    for (const auto& png : {stored, compressed}) {
        const auto image = Decoder::Png::Decode(png);
        Require(image.has_value() && image->width == 64 && image->height == 64);
        for (std::size_t pixel = 0; pixel < 64 * 64; ++pixel) Require(image->pixels[pixel * 4] == gradient[pixel * 3]);
    }

    return 0;
}
