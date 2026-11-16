#include "prx/libSceJpegEnc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

alignas(4) static unsigned char oddMin[8 * 36 + 64];
alignas(4) static unsigned char oddWide[8 * 40 + 64];
alignas(4) static unsigned char rgbaOdd[17 * 8 * 4 + 64];
static unsigned char jpeg[4096];

constexpr std::uint32_t kWidth = 17;
constexpr std::uint32_t kHeight = 8;

static bool IsJpeg(const unsigned char* data, std::uint32_t size) {
    return size > 4 && data[0] == 0xFF && data[1] == 0xD8 && data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

static std::uint8_t ColumnLuma(std::size_t x) {
    const bool evenPair = (x / 2) % 2 == 0;
    if ((x & 1u) == 0) return evenPair ? 80 : 120;
    return evenPair ? 120 : 80;
}

static void FillYuyvBuffer(unsigned char* buffer, std::size_t size, std::uint32_t pitch) {
    for (std::size_t i = 0; i < size; ++i) {
        buffer[i] = (i & 3u) == 1 || (i & 3u) == 3 ? 128 : 100;
    }
    for (std::uint32_t y = 0; y < kHeight; ++y) {
        unsigned char* row = buffer + static_cast<std::size_t>(y) * pitch;
        for (std::uint32_t x = 0; x < kWidth; ++x) {
            row[(x / 2) * 4 + (x % 2) * 2] = ColumnLuma(x);
        }
    }
}

static int AverageError(const std::vector<std::uint8_t>& expected, const std::vector<std::uint8_t>& actual) {
    long total = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) total += expected[i] > actual[i] ? expected[i] - actual[i] : actual[i] - expected[i];
    return static_cast<int>(total / static_cast<long>(expected.size()));
}

static int MaximumError(const std::vector<std::uint8_t>& expected, const std::vector<std::uint8_t>& actual) {
    int worst = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const long diff = expected[i] > actual[i] ? expected[i] - actual[i] : actual[i] - expected[i];
        if (diff > worst) worst = static_cast<int>(diff);
    }
    return worst;
}

static std::vector<std::uint8_t> DecodeOutput(const JpegEncOutputInfo& info) {
    Require(IsJpeg(jpeg, info.size));
    const auto decoded = Decoder::Jpeg::Decode({jpeg, info.size});
    Require(decoded.has_value());
    Require(decoded->width == kWidth && decoded->height == kHeight && decoded->channels == 3);
    return decoded->pixels;
}

static JpegEncEncodeParam YuyvParam(unsigned char* image, std::size_t imageSize, std::uint32_t pitch) {
    JpegEncEncodeParam param{};
    param.image = image;
    param.jpeg = jpeg;
    param.image_size = static_cast<std::uint32_t>(imageSize);
    param.jpeg_size = sizeof(jpeg);
    param.image_width = kWidth;
    param.image_height = kHeight;
    param.image_pitch = pitch;
    param.pixel_format = 10;
    param.encode_mode = 0;
    param.color_space = 1;
    param.sampling_type = 2;
    param.compression_ratio = 0;
    param.restart_interval = 0;
    return param;
}

static JpegEncEncodeParam RgbaParam(std::uint32_t pitch) {
    JpegEncEncodeParam param{};
    param.image = rgbaOdd;
    param.jpeg = jpeg;
    param.image_size = sizeof(rgbaOdd);
    param.jpeg_size = sizeof(jpeg);
    param.image_width = kWidth;
    param.image_height = kHeight;
    param.image_pitch = pitch;
    param.pixel_format = 0;
    param.encode_mode = 0;
    param.color_space = 1;
    param.sampling_type = 2;
    param.compression_ratio = 0;
    param.restart_interval = 0;
    return param;
}

int main() {
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80650103);

    JpegEncCreateParam create{sizeof(JpegEncCreateParam), 0};
    alignas(32) static unsigned char memory[0x800 + 32];
    void* handle = nullptr;
    Require(sceJpegEncCreate(&create, memory + 1, 0x800, &handle) == 0);

    std::vector<std::uint8_t> expected(kWidth * kHeight * 3);
    for (std::uint32_t y = 0; y < kHeight; ++y) {
        for (std::uint32_t x = 0; x < kWidth; ++x) {
            const std::uint8_t luma = ColumnLuma(x);
            expected[(y * kWidth + x) * 3 + 0] = luma;
            expected[(y * kWidth + x) * 3 + 1] = luma;
            expected[(y * kWidth + x) * 3 + 2] = luma;
        }
    }

    FillYuyvBuffer(oddMin, sizeof(oddMin), 36);
    JpegEncOutputInfo info{};
    const JpegEncEncodeParam minPitch = YuyvParam(oddMin, sizeof(oddMin), 36);
    Require(sceJpegEncEncode(handle, &minPitch, &info) == static_cast<std::int32_t>(info.size) && info.size > 0);
    const auto decodedMin = DecodeOutput(info);
    Require(AverageError(expected, decodedMin) < 4);
    Require(MaximumError(expected, decodedMin) < 12);

    FillYuyvBuffer(oddWide, sizeof(oddWide), 40);
    info = {};
    const JpegEncEncodeParam widePitch = YuyvParam(oddWide, sizeof(oddWide), 40);
    Require(sceJpegEncEncode(handle, &widePitch, &info) == static_cast<std::int32_t>(info.size) && info.size > 0);
    const auto decodedWide = DecodeOutput(info);
    Require(decodedWide == decodedMin);

    info = {};
    const JpegEncEncodeParam shortPitch = YuyvParam(oddMin, sizeof(oddMin), 32);
    Require(sceJpegEncEncode(handle, &shortPitch, &info) == invalidParam);

    for (std::size_t i = 0; i < sizeof(rgbaOdd); ++i) {
        rgbaOdd[i] = i % 4 == 3 ? 255 : 96;
    }
    const JpegEncEncodeParam rgbaShort = RgbaParam(64);
    info = {};
    Require(sceJpegEncEncode(handle, &rgbaShort, &info) == invalidParam);
    const JpegEncEncodeParam rgbaMin = RgbaParam(68);
    info = {};
    Require(sceJpegEncEncode(handle, &rgbaMin, &info) == static_cast<std::int32_t>(info.size) && info.size > 0);
    Require(DecodeOutput(info).size() == kWidth * kHeight * 3);

    Require(sceJpegEncDelete(handle) == 0);

    return 0;
}
