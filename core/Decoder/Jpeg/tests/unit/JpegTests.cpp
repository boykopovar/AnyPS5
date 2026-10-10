#include "Decoder/Jpeg.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using namespace Testing;

constexpr std::uint32_t WIDTH = 32;
constexpr std::uint32_t HEIGHT = 24;
constexpr std::uint8_t SOF_MARKER = 0xC0;
constexpr std::uint8_t SOS_MARKER = 0xDA;

struct FrameInfo {
    int components;
    std::uint8_t sampling;
};

std::vector<std::uint8_t> RgbPixels() {
    std::vector<std::uint8_t> rgb(WIDTH * HEIGHT * 3);
    for (std::uint32_t y = 0; y < HEIGHT; ++y) {
        for (std::uint32_t x = 0; x < WIDTH; ++x) {
            std::uint8_t* pixel = &rgb[(y * WIDTH + x) * 3];
            pixel[0] = static_cast<std::uint8_t>(x * 8);
            pixel[1] = static_cast<std::uint8_t>(y * 10);
            pixel[2] = 128;
        }
    }
    return rgb;
}

std::vector<std::uint8_t> GrayPixels() {
    std::vector<std::uint8_t> gray(WIDTH * HEIGHT);
    for (std::uint32_t y = 0; y < HEIGHT; ++y) {
        for (std::uint32_t x = 0; x < WIDTH; ++x) gray[y * WIDTH + x] = static_cast<std::uint8_t>((x + y) * 4);
    }
    return gray;
}

std::vector<std::uint8_t> RgbJpeg() {
    return Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 90);
}

std::vector<std::uint8_t> GrayJpeg() {
    return Decoder::Jpeg::Encode(GrayPixels(), WIDTH, HEIGHT, 1, 90);
}

bool IsJpeg(const std::vector<std::uint8_t>& data) {
    return data.size() > 4 && data[0] == 0xFF && data[1] == 0xD8 && data[data.size() - 2] == 0xFF && data.back() == 0xD9;
}

int Difference(std::uint8_t left, std::uint8_t right) {
    return left > right ? left - right : right - left;
}

std::size_t SegmentOffset(const std::vector<std::uint8_t>& jpeg, std::uint8_t wanted) {
    std::size_t offset = 2;
    while (offset + 3 < jpeg.size()) {
        const std::size_t start = offset;
        Require(jpeg[offset++] == 0xFF, "segment starts with 0xFF");
        while (offset < jpeg.size() && jpeg[offset] == 0xFF) ++offset;
        Require(offset < jpeg.size(), "marker byte is inside the stream");
        const std::uint8_t marker = jpeg[offset++];
        if (marker == wanted) return start;
        Require(marker != 0xD9 && marker != SOS_MARKER, "marker found before scan data");
        Require(offset + 1 < jpeg.size(), "segment length is inside the stream");
        const std::size_t length = (static_cast<std::size_t>(jpeg[offset]) << 8) | jpeg[offset + 1];
        Require(length >= 2 && offset + length <= jpeg.size(), "segment length is valid");
        offset += length;
    }
    Fail("marker not found");
}

std::size_t SegmentEnd(const std::vector<std::uint8_t>& jpeg, std::uint8_t marker) {
    const std::size_t start = SegmentOffset(jpeg, marker);
    return start + 2 + ((static_cast<std::size_t>(jpeg[start + 2]) << 8) | jpeg[start + 3]);
}

FrameInfo ReadFrame(const std::vector<std::uint8_t>& jpeg) {
    std::size_t offset = 2;
    while (offset + 3 < jpeg.size()) {
        Require(jpeg[offset++] == 0xFF, "segment starts with 0xFF");
        while (offset < jpeg.size() && jpeg[offset] == 0xFF) ++offset;
        Require(offset < jpeg.size(), "marker byte is inside the stream");
        const std::uint8_t marker = jpeg[offset++];
        if (marker == SOS_MARKER || marker == 0xD9) break;
        Require(offset + 1 < jpeg.size(), "segment length is inside the stream");
        const std::size_t length = (static_cast<std::size_t>(jpeg[offset]) << 8) | jpeg[offset + 1];
        Require(length >= 2 && offset + length <= jpeg.size(), "segment length is valid");
        if (marker >= 0xC0 && marker <= 0xC3) return {jpeg[offset + 7], jpeg[offset + 9]};
        offset += length;
    }
    Fail("no SOF segment found");
}

std::vector<std::uint8_t> Truncated(const std::vector<std::uint8_t>& jpeg, std::size_t size) {
    return {jpeg.begin(), jpeg.begin() + static_cast<std::ptrdiff_t>(size)};
}

std::vector<std::uint8_t> WithSofDimensions(std::uint16_t width, std::uint16_t height) {
    std::vector<std::uint8_t> jpeg = RgbJpeg();
    const std::size_t sof = SegmentOffset(jpeg, SOF_MARKER);
    jpeg[sof + 5] = static_cast<std::uint8_t>(height >> 8);
    jpeg[sof + 6] = static_cast<std::uint8_t>(height);
    jpeg[sof + 7] = static_cast<std::uint8_t>(width >> 8);
    jpeg[sof + 8] = static_cast<std::uint8_t>(width);
    return jpeg;
}

std::vector<std::uint8_t> JpegMagicWithGarbage() {
    std::vector<std::uint8_t> data{0xFF, 0xD8, 0xFF};
    for (std::uint8_t i = 0; i < 61; ++i) data.push_back(static_cast<std::uint8_t>(i * 37 + 5));
    data.insert(data.end(), {0xFF, 0xD9});
    return data;
}

void RequireSamplingFactor(Decoder::Jpeg::Sampling sampling, int quality, std::uint8_t factor) {
    const auto encoded = Decoder::Jpeg::Encode(RgbPixels(), 31, 23, 3, quality, sampling);
    const FrameInfo frame = ReadFrame(encoded);
    RequireEqual(frame.components, 3, "component count");
    RequireEqual(frame.sampling, factor, "luma sampling factor");
}

void RequireDecodedDimensions(const std::vector<std::uint8_t>& jpeg) {
    const auto image = Decoder::Jpeg::Decode(jpeg);
    Require(image.has_value(), "stream decodes");
    RequireEqual(image->width, WIDTH, "width");
    RequireEqual(image->height, HEIGHT, "height");
    RequireEqual(image->channels, 3u, "channels");
    RequireEqual(image->pixels.size(), static_cast<std::size_t>(WIDTH * HEIGHT * 3), "pixel count");
}

const Case encodeRgbProducesJpeg{"Encode_RgbImage_ProducesStreamWithSoiAndEoi", [] {
    const auto jpeg = RgbJpeg();

    Require(IsJpeg(jpeg), "stream starts with SOI and ends with EOI");
}};

const Case encodeRgbWritesFullResolutionFrame{"Encode_RgbImage_WritesThreeComponentFrameWithoutSubsampling", [] {
    const FrameInfo frame = ReadFrame(RgbJpeg());

    RequireEqual(frame.components, 3, "component count");
    RequireEqual(frame.sampling, std::uint8_t{0x11}, "luma sampling factor");
}};

const Case decodeRgbReturnsDimensions{"Decode_EncodedRgb_ReturnsOriginalDimensions", [] {
    const auto jpeg = RgbJpeg();

    RequireDecodedDimensions(jpeg);
}};

const Case decodeRgbStaysClose{"Decode_EncodedRgb_AverageErrorBelowFour", [] {
    const auto rgb = RgbPixels();
    const auto jpeg = Decoder::Jpeg::Encode(rgb, WIDTH, HEIGHT, 3, 90);

    const auto image = Decoder::Jpeg::Decode(jpeg);

    Require(image.has_value(), "stream decodes");
    RequireEqual(image->pixels.size(), rgb.size(), "pixel count");
    long totalError = 0;
    for (std::size_t i = 0; i < rgb.size(); ++i) totalError += Difference(rgb[i], image->pixels[i]);
    Require(totalError / static_cast<long>(rgb.size()) < 4, "average error is below 4");
}};

const Case encodeGrayProducesJpeg{"Encode_GrayImage_ProducesStreamWithSoiAndEoi", [] {
    const auto jpeg = GrayJpeg();

    Require(IsJpeg(jpeg), "stream starts with SOI and ends with EOI");
}};

const Case encodeGrayWritesSingleComponent{"Encode_GrayImage_WritesSingleComponentFrame", [] {
    const FrameInfo frame = ReadFrame(GrayJpeg());

    RequireEqual(frame.components, 1, "component count");
}};

const Case decodeGrayReturnsDimensions{"Decode_EncodedGray_ReturnsOriginalDimensions", [] {
    const auto image = Decoder::Jpeg::Decode(GrayJpeg());

    Require(image.has_value(), "stream decodes");
    RequireEqual(image->width, WIDTH, "width");
    RequireEqual(image->height, HEIGHT, "height");
}};

const Case decodeGrayStaysClose{"Decode_EncodedGray_AverageErrorBelowFour", [] {
    const auto gray = GrayPixels();
    const auto jpeg = Decoder::Jpeg::Encode(gray, WIDTH, HEIGHT, 1, 90);

    const auto image = Decoder::Jpeg::Decode(jpeg);

    Require(image.has_value(), "stream decodes");
    Require(image->pixels.size() >= gray.size() * image->channels, "pixel buffer covers the image");
    long totalError = 0;
    for (std::size_t i = 0; i < gray.size(); ++i) totalError += Difference(gray[i], image->pixels[i * image->channels]);
    Require(totalError / static_cast<long>(gray.size()) < 4, "average error is below 4");
}};

const Case encodeFourChannelsThrows{"Encode_FourChannels_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 4, 90); }, "4 channels");
}};

const Case encodeQualityZeroThrows{"Encode_QualityZero_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 0); }, "quality 0");
}};

const Case encodeQualityAboveHundredThrows{"Encode_QualityAboveHundred_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 101); }, "quality 101");
}};

const Case encodeZeroWidthThrows{"Encode_ZeroWidth_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), 0, HEIGHT, 3, 90); }, "zero width");
}};

const Case encodeWidthAboveMaximumThrows{"Encode_WidthAboveMaximum_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), 0x10000, 1, 3, 90); }, "width 0x10000");
}};

const Case encodeShortBufferThrows{"Encode_PixelBufferTooSmall_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT + 1, 3, 90); }, "short buffer");
}};

const Case encodeYuv444SamplingFactor{"Encode_Yuv444_WritesSamplingFactor11AtAnyQuality", [] {
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv444, 1, 0x11);
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv444, 100, 0x11);
}};

const Case encodeYuv422SamplingFactor{"Encode_Yuv422_WritesSamplingFactor21AtAnyQuality", [] {
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv422, 1, 0x21);
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv422, 100, 0x21);
}};

const Case encodeYuv420SamplingFactor{"Encode_Yuv420_WritesSamplingFactor22AtAnyQuality", [] {
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv420, 1, 0x22);
    RequireSamplingFactor(Decoder::Jpeg::Sampling::Yuv420, 100, 0x22);
}};

const Case encodeInvalidSamplingThrows{"Encode_InvalidSampling_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 90, static_cast<Decoder::Jpeg::Sampling>(3));
    }, "sampling 3");
}};

const Case encodeBothRestartIntervalsThrows{"Encode_BlocksAndRowsRestartInterval_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 90, Decoder::Jpeg::Sampling::Yuv444, 1, 1);
    }, "both restart intervals");
}};

const Case encodeRestartBlocksAboveMaximumThrows{"Encode_RestartBlocksAboveMaximum_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 90, Decoder::Jpeg::Sampling::Yuv444, 0x10000, 0);
    }, "restart blocks 0x10000");
}};

const Case encodeRestartRowsAboveMaximumThrows{"Encode_RestartRowsAboveMaximum_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] {
        Decoder::Jpeg::Encode(RgbPixels(), WIDTH, HEIGHT, 3, 90, Decoder::Jpeg::Sampling::Yuv444, 0, 0x10000);
    }, "restart rows 0x10000");
}};

const Case decodeEmptyReturnsNullopt{"Decode_EmptyInput_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::Decode({}).has_value(), "empty input is rejected");
}};

const Case decodeGarbageReturnsNullopt{"Decode_Garbage_ReturnsNullopt", [] {
    const std::vector<std::uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8};

    Require(!Decoder::Jpeg::Decode(garbage).has_value(), "garbage is rejected");
}};

const Case parseHeaderRgb{"ParseHeader_EncodedRgb_ReturnsDimensionsAndThreeChannels", [] {
    const auto header = Decoder::Jpeg::ParseHeader(RgbJpeg());

    Require(header.has_value(), "header parses");
    RequireEqual(header->width, WIDTH, "width");
    RequireEqual(header->height, HEIGHT, "height");
    RequireEqual(header->channels, 3u, "channels");
}};

const Case parseHeaderGray{"ParseHeader_EncodedGray_ReturnsDimensionsAndOneChannel", [] {
    const auto header = Decoder::Jpeg::ParseHeader(GrayJpeg());

    Require(header.has_value(), "header parses");
    RequireEqual(header->width, WIDTH, "width");
    RequireEqual(header->height, HEIGHT, "height");
    RequireEqual(header->channels, 1u, "channels");
}};

const Case parseHeaderEmpty{"ParseHeader_EmptyInput_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::ParseHeader({}).has_value(), "empty input is rejected");
}};

const Case parseHeaderGarbage{"ParseHeader_Garbage_ReturnsNullopt", [] {
    const std::vector<std::uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8};

    Require(!Decoder::Jpeg::ParseHeader(garbage).has_value(), "garbage is rejected");
}};

const Case parseHeaderSoiOnly{"ParseHeader_SoiOnly_ReturnsNullopt", [] {
    const std::vector<std::uint8_t> soi{0xFF, 0xD8};

    Require(!Decoder::Jpeg::ParseHeader(soi).has_value(), "SOI without frame is rejected");
}};

const Case parseHeaderTruncatedBeforeSof{"ParseHeader_TruncatedBeforeSof_ReturnsNullopt", [] {
    const auto jpeg = RgbJpeg();
    const auto truncated = Truncated(jpeg, SegmentOffset(jpeg, SOF_MARKER));

    Require(!Decoder::Jpeg::ParseHeader(truncated).has_value(), "stream without SOF is rejected");
}};

const Case parseHeaderJpegMagicGarbage{"ParseHeader_JpegMagicFollowedByGarbage_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::ParseHeader(JpegMagicWithGarbage()).has_value(), "fake JPEG is rejected");
}};

const Case parseHeaderZeroWidth{"ParseHeader_ZeroWidthInSof_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::ParseHeader(WithSofDimensions(0, HEIGHT)).has_value(), "zero width is rejected");
}};

const Case parseHeaderZeroHeight{"ParseHeader_ZeroHeightInSof_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::ParseHeader(WithSofDimensions(WIDTH, 0)).has_value(), "zero height is rejected");
}};

const Case decodeSoiOnly{"Decode_SoiOnly_ReturnsNullopt", [] {
    const std::vector<std::uint8_t> soi{0xFF, 0xD8};

    Require(!Decoder::Jpeg::Decode(soi).has_value(), "SOI without frame is rejected");
}};

const Case decodeTruncatedBeforeSof{"Decode_TruncatedBeforeSof_ReturnsNullopt", [] {
    const auto jpeg = RgbJpeg();
    const auto truncated = Truncated(jpeg, SegmentOffset(jpeg, SOF_MARKER));

    Require(!Decoder::Jpeg::Decode(truncated).has_value(), "stream without SOF is rejected");
}};

const Case decodeTruncatedAfterSof{"Decode_TruncatedAfterFrameHeader_ReturnsNullopt", [] {
    const auto jpeg = RgbJpeg();
    const auto truncated = Truncated(jpeg, SegmentEnd(jpeg, SOF_MARKER));

    Require(!Decoder::Jpeg::Decode(truncated).has_value(), "stream without scan data is rejected");
}};

const Case decodeTruncatedIsDeterministic{"Decode_TruncatedInsideScanData_ReturnsSamePixelsEveryTime", [] {
    const auto jpeg = RgbJpeg();
    const std::size_t scanStart = SegmentEnd(jpeg, SOS_MARKER);
    const auto truncated = Truncated(jpeg, scanStart + 4);

    const auto first = Decoder::Jpeg::Decode(truncated);
    const auto second = Decoder::Jpeg::Decode(truncated);

    Require(first.has_value() && second.has_value(), "truncated scan still decodes");
    Require(first->pixels == second->pixels, "decoded pixels do not depend on heap contents");
}};

const Case decodeTruncatedAtHalf{"Decode_TruncatedInsideScanData_ReturnsImageWithHeaderDimensions", [] {
    const auto jpeg = RgbJpeg();
    const std::size_t scanStart = SegmentEnd(jpeg, SOS_MARKER);
    const auto truncated = Truncated(jpeg, scanStart + (jpeg.size() - scanStart) / 2);

    RequireDecodedDimensions(truncated);
}};

const Case decodeTruncatedAfterScanHeader{"Decode_TruncatedRightAfterScanHeader_ReturnsImageWithHeaderDimensions", [] {
    const auto jpeg = RgbJpeg();
    const auto truncated = Truncated(jpeg, SegmentEnd(jpeg, SOS_MARKER));

    RequireDecodedDimensions(truncated);
}};

const Case decodeMissingEoi{"Decode_MissingEoi_ReturnsImageWithHeaderDimensions", [] {
    const auto jpeg = RgbJpeg();
    const auto truncated = Truncated(jpeg, jpeg.size() - 2);

    RequireDecodedDimensions(truncated);
}};

const Case decodeZeroWidth{"Decode_ZeroWidthInSof_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::Decode(WithSofDimensions(0, HEIGHT)).has_value(), "zero width is rejected");
}};

const Case decodeZeroHeight{"Decode_ZeroHeightInSof_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::Decode(WithSofDimensions(WIDTH, 0)).has_value(), "zero height is rejected");
}};

const Case decodeJpegMagicGarbage{"Decode_JpegMagicFollowedByGarbage_ReturnsNullopt", [] {
    Require(!Decoder::Jpeg::Decode(JpegMagicWithGarbage()).has_value(), "fake JPEG is rejected");
}};

} // namespace
