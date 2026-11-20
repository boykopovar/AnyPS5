#include "prx/libSceJpegEnc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include "turbojpeg.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
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

static void CheckYcbcrSamples(void* handle) {
    constexpr int height = 17;
    constexpr int pitch = 40;
    constexpr int chromaWidth = (kWidth + 1) / 2;
    alignas(4) unsigned char packed[height * pitch];
    auto chroma = [](int component, int x, int y) {
        if (component == 1) return (y % 2 ? 208 : 164) + (x == chromaWidth - 1 ? 16 : 0);
        return (y % 2 ? 240 : 212) - (x == chromaWidth - 1 ? 24 : 0);
    };
    for (int y = 0; y < height; ++y) {
        unsigned char* row = packed + y * pitch;
        for (int x = 0; x < chromaWidth; ++x) {
            row[4 * x] = 32;
            row[4 * x + 1] = static_cast<unsigned char>(chroma(1, x, y));
            row[4 * x + 2] = x == chromaWidth - 1 ? 0xEE : 64;
            row[4 * x + 3] = static_cast<unsigned char>(chroma(2, x, y));
        }
        for (int x = 4 * chromaWidth; x < pitch; ++x) row[x] = 0xEE;
    }
    auto param = YuyvParam(packed, sizeof(packed), pitch);
    param.image_height = height;
    for (const int vertical : {1, 2}) {
        param.sampling_type = static_cast<std::uint8_t>(vertical);
        JpegEncOutputInfo info{};
        const auto rc = sceJpegEncEncode(handle, &param, &info);
        Require(rc == static_cast<std::int32_t>(info.size) && IsJpeg(jpeg, info.size) && info.height == height);
        const std::unique_ptr<void, decltype(&tj3Destroy)> decoder(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
        Require(decoder && tj3DecompressHeader(decoder.get(), jpeg, info.size) == 0);
        const int sampling = vertical == 1 ? TJSAMP_422 : TJSAMP_420;
        Require(tj3Get(decoder.get(), TJPARAM_SUBSAMP) == sampling);
        Require(tj3Get(decoder.get(), TJPARAM_JPEGWIDTH) == static_cast<int>(kWidth) &&
                tj3Get(decoder.get(), TJPARAM_JPEGHEIGHT) == height);
        std::vector<unsigned char> decoded(tj3YUVBufSize(kWidth, 1, height, sampling));
        Require(!decoded.empty() && tj3DecompressToYUV8(decoder.get(), jpeg, info.size, decoded.data(), 1) == 0);

        std::size_t offset = 0;
        for (int component = 0; component < 3; ++component) {
            const int cols = component == 0 ? kWidth : chromaWidth;
            const int rows = component == 0 ? height : (height + vertical - 1) / vertical;
            const int stride = tj3YUVPlaneWidth(component, kWidth, sampling);
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    int expected = x % 2 ? 64 : 32;
                    if (component != 0) {
                        const int first = y * vertical;
                        expected = chroma(component, x, first);
                        if (vertical == 2 && first + 1 < height) expected = (expected + chroma(component, x, first + 1)) / 2;
                    }
                    const int actual = decoded[offset + y * stride + x];
                    if (std::abs(actual - expected) > 2) {
                        std::fprintf(stderr, "YCbCr sample %d at %d,%d (vertical factor %d): got %d, expected %d\n",
                                     component, x, y, vertical, actual, expected);
                        Require(false);
                    }
                }
            }
            offset += static_cast<std::size_t>(stride) * tj3YUVPlaneHeight(component, height, sampling);
        }
    }
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

    CheckYcbcrSamples(handle);
    Require(sceJpegEncDelete(handle) == 0);

    return 0;
}
