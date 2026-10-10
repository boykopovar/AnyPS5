#include "Decoder/Png.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace {

using namespace Testing;

constexpr std::size_t IHDR_WIDTH_OFFSET = 16;
constexpr std::size_t IHDR_HEIGHT_OFFSET = 20;
constexpr std::size_t IHDR_CRC_OFFSET = 29;
constexpr std::size_t IDAT_OFFSET = 33;

const std::uint8_t PALETTE_TRNS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x0F, 0xD8, 0xE5,
    0xB7, 0x00, 0x00, 0x00, 0x0C, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xD6, 0x02, 0x8F, 0x7B, 0x00, 0x00, 0x00, 0x04, 0x74, 0x52, 0x4E,
    0x53, 0xFF, 0x00, 0xFF, 0x80, 0x13, 0x0A, 0x1E, 0x39, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x63, 0x10, 0x60, 0xD8, 0x00, 0x00, 0x00, 0xE4, 0x00, 0xC1, 0x27, 0xA8, 0xE8,
    0x57, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

const std::uint8_t GRAY16[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x81, 0xD9, 0xFC,
    0x15, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32, 0x59, 0x7D,
    0x16, 0x00, 0x03, 0x0C, 0x01, 0xBF, 0x6E, 0xB9, 0xC6, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

const std::uint8_t GRAY1[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xCB, 0x7B, 0xD2,
    0xEE, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x88, 0x04, 0x00, 0x00,
    0x5B, 0x00, 0x5A, 0x7C, 0xA5, 0x93, 0x54, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
    0x42, 0x60, 0x82,
};

std::uint32_t ReadBigEndian32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) << 24 | static_cast<std::uint32_t>(bytes[offset + 1]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 8 | static_cast<std::uint32_t>(bytes[offset + 3]);
}

void WriteBigEndian32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (24 - 8 * i));
}

void AppendBigEndian32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::vector<std::uint8_t> HeaderOnly(std::uint32_t width, std::uint32_t height, std::uint8_t bitDepth,
                                     std::uint8_t colorType, std::uint8_t interlace) {
    std::vector<std::uint8_t> bytes{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    AppendBigEndian32(bytes, width);
    AppendBigEndian32(bytes, height);
    bytes.insert(bytes.end(), {bitDepth, colorType, 0, 0, interlace, 0, 0, 0, 0});
    bytes.insert(bytes.end(), {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0});
    return bytes;
}

std::vector<std::uint8_t> SmallRgbaPixels() {
    std::vector<std::uint8_t> pixels(4 * 4 * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(i * 13 + 7);
    return pixels;
}

std::vector<std::uint8_t> SmallRgbaPng() {
    return Decoder::Png::Encode(SmallRgbaPixels(), 4, 4, 4);
}

std::vector<std::uint8_t> WithDimensions(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> png = SmallRgbaPng();
    WriteBigEndian32(png, IHDR_WIDTH_OFFSET, width);
    WriteBigEndian32(png, IHDR_HEIGHT_OFFSET, height);
    return png;
}

std::vector<std::uint8_t> Gradient() {
    std::vector<std::uint8_t> gradient(64 * 64 * 3);
    for (std::size_t i = 0; i < gradient.size(); ++i) gradient[i] = static_cast<std::uint8_t>(i / 3 % 64 * 4);
    return gradient;
}

void RequireRoundTrip(std::uint32_t channels, Decoder::Png::ColorType colorType) {
    constexpr std::uint32_t width = 20;
    constexpr std::uint32_t height = 10;
    std::vector<std::uint8_t> pixels(width * height * channels);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(i * 37 + 11);

    const std::vector<std::uint8_t> png = Decoder::Png::Encode(pixels, width, height, channels);
    const auto header = Decoder::Png::ParseHeader(png);
    Require(header.has_value(), "header parses");
    RequireEqual(header->width, width, "header width");
    RequireEqual(header->height, height, "header height");
    RequireEqual(header->bitDepth, std::uint8_t{8}, "header bit depth");
    RequireEqual(header->colorType, colorType, "header color type");
    Require(!header->interlaced, "header is not interlaced");
    Require(!header->hasTransparency, "header has no transparency");

    const auto image = Decoder::Png::Decode(png);
    Require(image.has_value(), "image decodes");
    RequireEqual(image->width, width, "image width");
    RequireEqual(image->height, height, "image height");
    RequireEqual(image->pixels.size(), static_cast<std::size_t>(width * height * 4), "pixel count");
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        const std::uint8_t* in = &pixels[pixel * channels];
        const std::uint8_t* out = &image->pixels[pixel * 4];
        const bool gray = channels <= 2;
        RequireEqual(out[0], in[0], "red");
        RequireEqual(out[1], gray ? in[0] : in[1], "green");
        RequireEqual(out[2], gray ? in[0] : in[2], "blue");
        const bool hasAlpha = channels == 2 || channels == 4;
        RequireEqual(out[3], hasAlpha ? in[channels - 1] : std::uint8_t{255}, "alpha");
    }
}

std::vector<std::uint8_t> RowFilters(const std::vector<std::uint8_t>& png, std::uint32_t height, std::uint32_t rowSize) {
    std::vector<std::uint8_t> stream;
    for (std::size_t offset = 8; offset + 12 <= png.size();) {
        const std::size_t length = ReadBigEndian32(png, offset);
        Require(length <= png.size() - offset - 12, "chunk fits in the stream");
        const std::uint8_t* data = png.data() + offset + 8;
        if (std::equal(data - 4, data, "IDAT")) stream.insert(stream.end(), data, data + length);
        offset += 12 + length;
    }

    int size = 0;
    char* rows = stbi_zlib_decode_malloc(reinterpret_cast<const char*>(stream.data()), static_cast<int>(stream.size()), &size);
    Require(rows != nullptr, "image data inflates");
    const bool sizeMatches = static_cast<std::size_t>(size) == static_cast<std::size_t>(height) * (rowSize + 1);
    std::vector<std::uint8_t> filters(sizeMatches ? height : 0);
    for (std::size_t y = 0; y < filters.size(); ++y) filters[y] = static_cast<std::uint8_t>(rows[y * (rowSize + 1)]);
    stbi_image_free(rows);
    Require(sizeMatches, "inflated size matches the filtered rows");
    return filters;
}

std::vector<std::uint8_t> FilterTestPixels(std::uint32_t width, std::uint32_t height, std::uint32_t channels) {
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
    return pixels;
}

void RequireEveryFilterSetRoundTrips(std::uint32_t channels) {
    constexpr std::uint32_t width = 16;
    constexpr std::uint32_t height = 6;
    const std::uint32_t rowSize = width * channels;
    const std::vector<std::uint8_t> pixels = FilterTestPixels(width, height, channels);

    for (std::uint8_t filters = 1; filters <= Decoder::Png::FILTER_ALL; ++filters) {
        const std::vector<std::uint8_t> png = Decoder::Png::Encode(pixels, width, height, channels, {6, filters});
        for (const std::uint8_t filter : RowFilters(png, height, rowSize)) {
            Require(filter < 5 && (filters >> filter & 1) != 0, "row filter is in the allowed set");
        }

        const auto image = Decoder::Png::Decode(png);
        Require(image.has_value(), "image decodes");
        RequireEqual(image->width, width, "width");
        RequireEqual(image->height, height, "height");
        for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
            RequireEqual(image->pixels[pixel * 4], pixels[pixel * channels], "first sample");
            RequireEqual(image->pixels[pixel * 4 + 3],
                         channels % 2 == 0 ? pixels[pixel * channels + channels - 1] : std::uint8_t{255}, "alpha");
        }
    }
}

void RequireSubOrUpChoosesCheapestFilter(std::uint32_t channels) {
    constexpr std::uint32_t width = 16;
    constexpr std::uint32_t height = 6;
    const std::vector<std::uint8_t> pixels = FilterTestPixels(width, height, channels);

    const std::vector<std::uint8_t> subOrUp = RowFilters(
        Decoder::Png::Encode(pixels, width, height, channels, {6, Decoder::Png::FILTER_SUB | Decoder::Png::FILTER_UP}), height,
        width * channels);

    RequireEqual(subOrUp[0], std::uint8_t{1}, "row 0 filter");
    RequireEqual(subOrUp[1], std::uint8_t{2}, "row 1 filter");
    RequireEqual(subOrUp[3], std::uint8_t{2}, "row 3 filter");
    RequireEqual(subOrUp[4], std::uint8_t{1}, "row 4 filter");
    RequireEqual(subOrUp[5], std::uint8_t{2}, "row 5 filter");
}

void RequireGradientRoundTrips(const std::vector<std::uint8_t>& png) {
    const std::vector<std::uint8_t> gradient = Gradient();
    const auto image = Decoder::Png::Decode(png);
    Require(image.has_value(), "image decodes");
    RequireEqual(image->width, 64u, "width");
    RequireEqual(image->height, 64u, "height");
    for (std::size_t pixel = 0; pixel < 64 * 64; ++pixel) RequireEqual(image->pixels[pixel * 4], gradient[pixel * 3], "red");
}

const Case roundTripGray{"EncodeDecode_Grayscale_RoundTripsLosslessly", [] {
    RequireRoundTrip(1, Decoder::Png::ColorType::Grayscale);
}};

const Case roundTripGrayAlpha{"EncodeDecode_GrayscaleAlpha_RoundTripsLosslessly", [] {
    RequireRoundTrip(2, Decoder::Png::ColorType::GrayscaleAlpha);
}};

const Case roundTripRgb{"EncodeDecode_Rgb_RoundTripsLosslessly", [] {
    RequireRoundTrip(3, Decoder::Png::ColorType::Rgb);
}};

const Case roundTripRgba{"EncodeDecode_Rgba_RoundTripsLosslessly", [] {
    RequireRoundTrip(4, Decoder::Png::ColorType::Rgba);
}};

const Case filterSetsGray{"Encode_EveryFilterSetOneChannel_UsesOnlyAllowedFiltersAndRoundTrips", [] {
    RequireEveryFilterSetRoundTrips(1);
}};

const Case filterSetsGrayAlpha{"Encode_EveryFilterSetTwoChannels_UsesOnlyAllowedFiltersAndRoundTrips", [] {
    RequireEveryFilterSetRoundTrips(2);
}};

const Case filterSetsRgb{"Encode_EveryFilterSetThreeChannels_UsesOnlyAllowedFiltersAndRoundTrips", [] {
    RequireEveryFilterSetRoundTrips(3);
}};

const Case filterSetsRgba{"Encode_EveryFilterSetFourChannels_UsesOnlyAllowedFiltersAndRoundTrips", [] {
    RequireEveryFilterSetRoundTrips(4);
}};

const Case subOrUpGray{"Encode_SubOrUpOneChannel_ChoosesCheapestFilterPerRow", [] {
    RequireSubOrUpChoosesCheapestFilter(1);
}};

const Case subOrUpGrayAlpha{"Encode_SubOrUpTwoChannels_ChoosesCheapestFilterPerRow", [] {
    RequireSubOrUpChoosesCheapestFilter(2);
}};

const Case subOrUpRgb{"Encode_SubOrUpThreeChannels_ChoosesCheapestFilterPerRow", [] {
    RequireSubOrUpChoosesCheapestFilter(3);
}};

const Case subOrUpRgba{"Encode_SubOrUpFourChannels_ChoosesCheapestFilterPerRow", [] {
    RequireSubOrUpChoosesCheapestFilter(4);
}};

const Case parseHeaderPalette{"ParseHeader_PaletteWithTrns_ReportsPaletteAndTransparency", [] {
    const auto header = Decoder::Png::ParseHeader(PALETTE_TRNS);

    Require(header.has_value(), "header parses");
    RequireEqual(header->width, 2u, "width");
    RequireEqual(header->height, 2u, "height");
    RequireEqual(header->bitDepth, std::uint8_t{2}, "bit depth");
    RequireEqual(header->colorType, Decoder::Png::ColorType::Palette, "color type");
    Require(header->hasTransparency, "tRNS is detected");
}};

const Case decodePalette{"Decode_PaletteWithTrns_ExpandsToRgba", [] {
    const auto image = Decoder::Png::Decode(PALETTE_TRNS);

    Require(image.has_value(), "image decodes");
    Require(image->pixels == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 128}),
            "palette pixels");
}};

const Case parseHeaderGray16{"ParseHeader_Gray16_ReportsSixteenBitGrayscale", [] {
    const auto header = Decoder::Png::ParseHeader(GRAY16);

    Require(header.has_value(), "header parses");
    RequireEqual(header->bitDepth, std::uint8_t{16}, "bit depth");
    RequireEqual(header->colorType, Decoder::Png::ColorType::Grayscale, "color type");
}};

const Case decodeGray16{"Decode_Gray16_KeepsHighByte", [] {
    const auto image = Decoder::Png::Decode(GRAY16);

    Require(image.has_value(), "image decodes");
    Require(image->pixels == std::vector<std::uint8_t>({0x12, 0x12, 0x12, 255, 0xAB, 0xAB, 0xAB, 255}), "gray16 pixels");
}};

const Case parseHeaderGray1{"ParseHeader_Gray1_ReportsOneBitDepth", [] {
    const auto header = Decoder::Png::ParseHeader(GRAY1);

    Require(header.has_value(), "header parses");
    RequireEqual(header->bitDepth, std::uint8_t{1}, "bit depth");
    RequireEqual(header->width, 8u, "width");
}};

const Case decodeGray1{"Decode_Gray1_ScalesBitsToFullRange", [] {
    const std::uint8_t levels[8] = {0, 255, 0, 255, 255, 0, 0, 255};

    const auto image = Decoder::Png::Decode(GRAY1);

    Require(image.has_value(), "image decodes");
    for (int i = 0; i < 8; ++i) {
        RequireEqual(image->pixels[i * 4], levels[i], "gray level");
        RequireEqual(image->pixels[i * 4 + 3], std::uint8_t{255}, "alpha");
    }
}};

const Case parseHeaderInterlaced{"ParseHeader_Adam7_ReportsInterlacedWithoutTransparency", [] {
    const auto header = Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 6, 1));

    Require(header.has_value(), "header parses");
    Require(header->interlaced, "interlaced");
    Require(!header->hasTransparency, "no transparency");
}};

const Case parseHeaderZeroWidth{"ParseHeader_ZeroWidth_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(HeaderOnly(0, 4, 8, 6, 0)).has_value(), "zero width is rejected");
}};

const Case parseHeaderInvalidColorType{"ParseHeader_InvalidColorType_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 5, 0)).has_value(), "color type 5 is rejected");
}};

const Case parseHeaderInvalidPaletteDepth{"ParseHeader_SixteenBitPalette_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 16, 3, 0)).has_value(), "16-bit palette is rejected");
}};

const Case parseHeaderInvalidInterlace{"ParseHeader_UnknownInterlaceMethod_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(HeaderOnly(3, 4, 8, 6, 2)).has_value(), "interlace method 2 is rejected");
}};

const Case parseHeaderBadSignature{"ParseHeader_BadSignature_ReturnsNullopt", [] {
    std::vector<std::uint8_t> png = HeaderOnly(3, 4, 8, 6, 0);
    png[1] = 'X';

    Require(!Decoder::Png::ParseHeader(png).has_value(), "bad signature is rejected");
}};

const Case parseHeaderOversizedChunk{"ParseHeader_ChunkLengthPastEnd_StopsScanningWithoutTransparency", [] {
    std::vector<std::uint8_t> png = HeaderOnly(3, 4, 8, 6, 0);
    png[33] = 0xFF;
    png[37] = 't';
    png[38] = 'E';
    png[39] = 'X';
    png[40] = 't';

    const auto header = Decoder::Png::ParseHeader(png);

    Require(header.has_value(), "header parses");
    Require(!header->hasTransparency, "no transparency");
}};

const Case parseHeaderTruncated{"ParseHeader_TruncatedIhdr_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(std::span<const std::uint8_t>(PALETTE_TRNS, 32)).has_value(), "truncated IHDR is rejected");
}};

const Case decodeEmpty{"Decode_EmptyInput_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode({}).has_value(), "empty input is rejected");
}};

const Case decodeTruncated{"Decode_TruncatedStream_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode(std::span<const std::uint8_t>(PALETTE_TRNS, 60)).has_value(), "truncated stream is rejected");
}};

const Case decodeGarbage{"Decode_Garbage_ReturnsNullopt", [] {
    const std::vector<std::uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8};

    Require(!Decoder::Png::Decode(garbage).has_value(), "garbage is rejected");
}};

const Case encodeZeroChannelsThrows{"Encode_ZeroChannels_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 4, 0); }, "0 channels");
}};

const Case encodeFiveChannelsThrows{"Encode_FiveChannels_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 4, 5); }, "5 channels");
}};

const Case encodeZeroWidthThrows{"Encode_ZeroWidth_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Png::Encode(std::vector<std::uint8_t>(64), 0, 4, 4); }, "zero width");
}};

const Case encodeShortBufferThrows{"Encode_PixelBufferTooSmall_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 5, 4); }, "short buffer");
}};

const Case encodeCompressionAboveNineThrows{"Encode_CompressionLevelAboveNine_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 4, 4, {10, Decoder::Png::FILTER_ALL});
    }, "compression 10");
}};

const Case encodeNoFiltersThrows{"Encode_EmptyFilterSet_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 4, 4, {8, 0}); }, "no filters");
}};

const Case encodeUnknownFilterThrows{"Encode_UnknownFilterBit_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Png::Encode(std::vector<std::uint8_t>(64), 4, 4, 4, {8, Decoder::Png::FILTER_ALL + 1});
    }, "unknown filter bit");
}};

const Case encodeCompressionShrinks{"Encode_MaximumCompression_IsSmallerThanStored", [] {
    const auto stored = Decoder::Png::Encode(Gradient(), 64, 64, 3, {0, Decoder::Png::FILTER_NONE});
    const auto compressed = Decoder::Png::Encode(Gradient(), 64, 64, 3, {9, Decoder::Png::FILTER_SUB});

    Require(compressed.size() < stored.size(), "compressed output is smaller");
}};

const Case decodeStored{"Decode_StoredStream_RoundTrips", [] {
    RequireGradientRoundTrips(Decoder::Png::Encode(Gradient(), 64, 64, 3, {0, Decoder::Png::FILTER_NONE}));
}};

const Case decodeCompressed{"Decode_CompressedStream_RoundTrips", [] {
    RequireGradientRoundTrips(Decoder::Png::Encode(Gradient(), 64, 64, 3, {9, Decoder::Png::FILTER_SUB}));
}};

const Case decodeIdatCrcMismatch{"Decode_IdatCrcMismatch_IgnoresCrcAndDecodes", [] {
    std::vector<std::uint8_t> png = SmallRgbaPng();
    const std::size_t crcOffset = IDAT_OFFSET + 8 + ReadBigEndian32(png, IDAT_OFFSET);
    png[crcOffset] ^= 0xFF;

    const auto image = Decoder::Png::Decode(png);

    Require(image.has_value(), "stream with bad IDAT CRC decodes");
    RequireEqual(image->width, 4u, "width");
    Require(image->pixels == SmallRgbaPixels(), "pixels are unchanged");
}};

const Case decodeIhdrCrcMismatch{"Decode_IhdrCrcMismatch_IgnoresCrcAndDecodes", [] {
    std::vector<std::uint8_t> png = SmallRgbaPng();
    png[IHDR_CRC_OFFSET] ^= 0xFF;

    const auto image = Decoder::Png::Decode(png);

    Require(image.has_value(), "stream with bad IHDR CRC decodes");
    Require(image->pixels == SmallRgbaPixels(), "pixels are unchanged");
}};

const Case parseHeaderIhdrCrcMismatch{"ParseHeader_IhdrCrcMismatch_IgnoresCrc", [] {
    std::vector<std::uint8_t> png = SmallRgbaPng();
    png[IHDR_CRC_OFFSET] ^= 0xFF;

    const auto header = Decoder::Png::ParseHeader(png);

    Require(header.has_value(), "header parses");
    RequireEqual(header->width, 4u, "width");
}};

const Case decodeOverflowingSize{"Decode_DimensionsOverflowingPixelBuffer_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode(WithDimensions(0x10000, 0x10000)).has_value(), "65536x65536 RGBA is rejected");
}};

const Case decodeMaximumDimensions{"Decode_MaximumHeaderDimensions_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode(WithDimensions(0x7FFFFFFF, 0x7FFFFFFF)).has_value(), "2^31-1 square is rejected");
}};

const Case decodeWidthBeyondDecoderLimit{"Decode_WidthBeyondDecoderLimit_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode(WithDimensions((1u << 24) + 1, 1)).has_value(), "width above 2^24 is rejected");
}};

const Case parseHeaderMaximumDimensions{"ParseHeader_MaximumHeaderDimensions_ReturnsDeclaredSize", [] {
    const auto header = Decoder::Png::ParseHeader(WithDimensions(0x7FFFFFFF, 0x7FFFFFFF));

    Require(header.has_value(), "header parses");
    RequireEqual(header->width, 0x7FFFFFFFu, "width");
    RequireEqual(header->height, 0x7FFFFFFFu, "height");
}};

const Case parseHeaderWidthAboveSpecLimit{"ParseHeader_WidthAboveSpecLimit_ReturnsNullopt", [] {
    Require(!Decoder::Png::ParseHeader(WithDimensions(0x80000000u, 1)).has_value(), "width 2^31 is rejected");
}};

const Case decodeWidthAboveSpecLimit{"Decode_WidthAboveSpecLimit_ReturnsNullopt", [] {
    Require(!Decoder::Png::Decode(WithDimensions(0x80000000u, 1)).has_value(), "width 2^31 is rejected");
}};

} // namespace
