#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" {
std::int32_t APS5_VABI sceJpegEncQueryMemorySize(const JpegEncCreateParam*);
std::int32_t APS5_VABI sceJpegEncCreate(const JpegEncCreateParam*, void*, std::uint32_t, void**);
std::int32_t APS5_VABI sceJpegEncDelete(void*);
std::int32_t APS5_VABI sceJpegEncEncode(void*, const JpegEncEncodeParam*, JpegEncOutputInfo*);
}

static void Require(bool value) { if (!value) std::abort(); }

alignas(4) static unsigned char image[16 * 16 * 4];
static unsigned char jpeg[4096];

static bool IsJpeg(const unsigned char* data, std::uint32_t size) {
    return size > 4 && data[0] == 0xFF && data[1] == 0xD8 && data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

static std::pair<int, std::uint8_t> ReadFrame(const unsigned char* jpeg, std::uint32_t size) {
    std::size_t offset = 2;
    while (offset + 3 < size) {
        Require(jpeg[offset++] == 0xFF);
        while (offset < size && jpeg[offset] == 0xFF) ++offset;
        Require(offset < size);
        const std::uint8_t marker = jpeg[offset++];
        if (marker == 0xDA || marker == 0xD9) break;
        Require(offset + 1 < size);
        const std::size_t length = (static_cast<std::size_t>(jpeg[offset]) << 8) | jpeg[offset + 1];
        Require(length >= 2 && offset + length <= size);
        if (marker >= 0xC0 && marker <= 0xC3) return {jpeg[offset + 7], jpeg[offset + 9]};
        offset += length;
    }
    std::abort();
}

static std::pair<int, int> ReadRestart(const unsigned char* jpeg, std::uint32_t size) {
    int interval = 0;
    std::size_t offset = 2;
    while (true) {
        Require(offset + 3 < size && jpeg[offset] == 0xFF);
        const std::uint8_t marker = jpeg[offset + 1];
        const std::size_t length = (static_cast<std::size_t>(jpeg[offset + 2]) << 8) | jpeg[offset + 3];
        Require(length >= 2 && offset + 2 + length <= size);
        if (marker == 0xDD) {
            Require(length == 4);
            interval = (jpeg[offset + 4] << 8) | jpeg[offset + 5];
        }
        offset += 2 + length;
        if (marker == 0xDA) break;
    }
    int markers = 0;
    for (; offset + 1 < size; ++offset) {
        if (jpeg[offset] != 0xFF || jpeg[offset + 1] < 0xD0 || jpeg[offset + 1] > 0xD7) continue;
        Require(jpeg[offset + 1] == 0xD0 + markers % 8);
        ++markers;
    }
    return {interval, markers};
}

static int AverageError(const std::vector<std::uint8_t>& expected, const std::vector<std::uint8_t>& actual) {
    long total = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) total += expected[i] > actual[i] ? expected[i] - actual[i] : actual[i] - expected[i];
    return static_cast<int>(total / static_cast<long>(expected.size()));
}

static std::vector<std::uint8_t> DecodeOutput(const JpegEncOutputInfo& info, std::uint32_t channels = 3) {
    Require(IsJpeg(jpeg, info.size));
    const auto decoded = Decoder::Jpeg::Decode({jpeg, info.size});
    Require(decoded.has_value());
    Require(decoded->width == 16 && decoded->height == 16 && decoded->channels == channels);
    return decoded->pixels;
}

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static JpegEncEncodeParam ValidEncodeParam() {
    JpegEncEncodeParam param{};
    param.image = image;
    param.jpeg = jpeg;
    param.image_size = sizeof(image);
    param.jpeg_size = sizeof(jpeg);
    param.image_width = 16;
    param.image_height = 16;
    param.image_pitch = 16 * 4;
    param.pixel_format = 0;
    param.encode_mode = 0;
    param.color_space = 1;
    param.sampling_type = 2;
    param.compression_ratio = 80;
    param.restart_interval = 0;
    return param;
}

int main() {
    constexpr std::int32_t invalidAddr = static_cast<std::int32_t>(0x80650101);
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80650102);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80650103);
    constexpr std::int32_t invalidHandle = static_cast<std::int32_t>(0x80650104);

    JpegEncCreateParam param{sizeof(JpegEncCreateParam), 0};
    Require(sceJpegEncQueryMemorySize(&param) == 0x800);
    Require(sceJpegEncQueryMemorySize(nullptr) == invalidAddr);

    JpegEncCreateParam badSize{sizeof(JpegEncCreateParam) - 1, 0};
    Require(sceJpegEncQueryMemorySize(&badSize) == invalidSize);

    JpegEncCreateParam badAttr{sizeof(JpegEncCreateParam), 1};
    Require(sceJpegEncQueryMemorySize(&badAttr) == invalidParam);

    alignas(32) static unsigned char memory[0x800 + 32];
    unsigned char* unaligned = memory + 1;
    void* handle = nullptr;

    Require(sceJpegEncCreate(nullptr, unaligned, 0x800, &handle) == invalidAddr);
    Require(sceJpegEncCreate(&badSize, unaligned, 0x800, &handle) == invalidSize);
    Require(sceJpegEncCreate(&badAttr, unaligned, 0x800, &handle) == invalidParam);
    Require(sceJpegEncCreate(&param, nullptr, 0x800, &handle) == invalidAddr);
    Require(sceJpegEncCreate(&param, unaligned, 0x7FF, &handle) == invalidSize);
    Require(sceJpegEncCreate(&param, unaligned, 0x800, nullptr) == invalidAddr);
    Require(handle == nullptr);

    Require(sceJpegEncCreate(&param, unaligned, 0x800, &handle) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    Require(address % 32 == 0);
    Require(address >= reinterpret_cast<std::uintptr_t>(unaligned));
    Require(address < reinterpret_cast<std::uintptr_t>(unaligned) + 32);

    Require(sceJpegEncDelete(nullptr) == invalidHandle);
    Require(sceJpegEncDelete(static_cast<unsigned char*>(handle) + 1) == invalidHandle);
    alignas(32) static unsigned char garbage[64] = {};
    Require(sceJpegEncDelete(garbage) == invalidHandle);

    Require(sceJpegEncDelete(handle) == 0);
    Require(sceJpegEncDelete(handle) == invalidHandle);

    JpegEncOutputInfo info{};
    const JpegEncEncodeParam valid = ValidEncodeParam();
    Require(sceJpegEncEncode(handle, &valid, &info) == invalidHandle);
    Require(sceJpegEncEncode(nullptr, &valid, &info) == invalidHandle);

    Require(sceJpegEncCreate(&param, unaligned, 0x800, &handle) == 0);
    Require(sceJpegEncEncode(handle, nullptr, &info) == invalidAddr);

    auto encodeWith = [&](auto change) {
        JpegEncEncodeParam changed = ValidEncodeParam();
        change(changed);
        return sceJpegEncEncode(handle, &changed, &info);
    };

    Require(encodeWith([](JpegEncEncodeParam& p) { p.image = nullptr; }) == invalidAddr);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image = image + 1; }) == invalidAddr);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.jpeg = nullptr; }) == invalidAddr);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_size = 0; }) == invalidSize);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.jpeg_size = 0; }) == invalidSize);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_width = 0x10000; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_height = 0x10000; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_pitch = 0; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_pitch = 66; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_size = sizeof(image) - 1; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_height = 0xFFFF; p.image_pitch = 0xFFFFFFC; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.encode_mode = 2; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.color_space = 0; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.sampling_type = 3; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.restart_interval = 0x10000; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.pixel_format = 2; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_width = 17; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.color_space = 2; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.sampling_type = 0; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.pixel_format = 10; p.image_width = 33; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.pixel_format = 11; p.color_space = 2; p.sampling_type = 0; p.image_width = 65; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.pixel_format = 11; p.color_space = 2; p.sampling_type = 2; }) == invalidParam);
    Require(encodeWith([](JpegEncEncodeParam& p) { p.pixel_format = 11; p.sampling_type = 0; }) == invalidParam);

    constexpr std::uint32_t pitch = 16 * 4 + 16;
    alignas(4) static unsigned char padded[16 * pitch];
    std::vector<std::uint8_t> expected(16 * 16 * 3);
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const std::uint8_t color[3] = {static_cast<std::uint8_t>(x * 16), static_cast<std::uint8_t>(y * 16), 96};
            unsigned char* pixel = padded + y * pitch + x * 4;
            pixel[0] = color[0];
            pixel[1] = color[1];
            pixel[2] = color[2];
            pixel[3] = 255;
            for (int c = 0; c < 3; ++c) expected[(y * 16 + x) * 3 + c] = color[c];
        }
        for (std::uint32_t x = 16 * 4; x < pitch; ++x) padded[y * pitch + x] = 0xEE;
    }

    JpegEncEncodeParam rgba = ValidEncodeParam();
    rgba.image = padded;
    rgba.image_size = sizeof(padded);
    rgba.image_pitch = pitch;
    rgba.compression_ratio = 0;
    info = {};
    Require(sceJpegEncEncode(handle, &rgba, &info) == 0);
    Require(info.height == 16);
    Require(ReadFrame(jpeg, info.size) == std::pair{3, std::uint8_t{0x22}});
    Require(AverageError(expected, DecodeOutput(info)) < 6);

    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            unsigned char* pixel = padded + y * pitch + x * 4;
            const unsigned char red = pixel[0];
            pixel[0] = pixel[2];
            pixel[2] = red;
        }
    }
    JpegEncEncodeParam bgra = rgba;
    bgra.pixel_format = 1;
    info = {};
    Require(sceJpegEncEncode(handle, &bgra, &info) == 0);
    Require(ReadFrame(jpeg, info.size) == std::pair{3, std::uint8_t{0x22}});
    Require(AverageError(expected, DecodeOutput(info)) < 6);

    alignas(4) static unsigned char yuyv[16 * 16 * 2];
    for (std::size_t i = 0; i < sizeof(yuyv); i += 4) {
        yuyv[i + 0] = 100;
        yuyv[i + 1] = 128;
        yuyv[i + 2] = 100;
        yuyv[i + 3] = 128;
    }
    JpegEncEncodeParam yuv = ValidEncodeParam();
    yuv.image = yuyv;
    yuv.image_size = sizeof(yuyv);
    yuv.image_pitch = 16 * 2;
    yuv.pixel_format = 10;
    info = {};
    Require(sceJpegEncEncode(handle, &yuv, &info) == 0);
    Require(AverageError(std::vector<std::uint8_t>(16 * 16 * 3, 100), DecodeOutput(info)) < 3);

    static unsigned char gray[16 * 16];
    for (std::size_t i = 0; i < sizeof(gray); ++i) {
        gray[i] = static_cast<unsigned char>(((i % 16) + (i / 16)) * 8);
    }
    JpegEncEncodeParam y8 = ValidEncodeParam();
    y8.image = gray;
    y8.image_size = sizeof(gray);
    y8.image_pitch = 16;
    y8.pixel_format = 11;
    y8.color_space = 2;
    y8.sampling_type = 0;
    info = {};
    Require(sceJpegEncEncode(handle, &y8, &info) == 0);
    Require(ReadFrame(jpeg, info.size) == std::pair{1, std::uint8_t{0x11}});
    const auto decodedGray = DecodeOutput(info, 1);
    Require(AverageError(std::vector<std::uint8_t>(gray, gray + sizeof(gray)), decodedGray) < 6);

    rgba.sampling_type = 1;
    rgba.compression_ratio = 0;
    info = {};
    Require(sceJpegEncEncode(handle, &rgba, &info) == 0);
    Require(ReadFrame(jpeg, info.size) == std::pair{3, std::uint8_t{0x21}});
    std::vector<std::uint8_t> swapped = expected;
    for (std::size_t i = 0; i < swapped.size(); i += 3) std::swap(swapped[i], swapped[i + 2]);
    Require(AverageError(swapped, DecodeOutput(info)) < 6);

    alignas(4) static unsigned char stripes[16 * 16 * 4];
    std::vector<std::uint8_t> stripeSource(16 * 16 * 3);
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const std::uint8_t color[3] = {static_cast<std::uint8_t>(y % 2 ? 40 : 200), 40, static_cast<std::uint8_t>(y % 2 ? 200 : 40)};
            unsigned char* pixel = stripes + (y * 16 + x) * 4;
            pixel[3] = 255;
            for (int c = 0; c < 3; ++c) {
                pixel[c] = color[c];
                stripeSource[(y * 16 + x) * 3 + c] = color[c];
            }
        }
    }
    JpegEncEncodeParam striped = ValidEncodeParam();
    striped.image = stripes;
    striped.image_size = sizeof(stripes);
    striped.sampling_type = 1;
    striped.compression_ratio = 0;
    info = {};
    Require(sceJpegEncEncode(handle, &striped, &info) == 0);
    Require(ReadFrame(jpeg, info.size) == std::pair{3, std::uint8_t{0x21}});
    Require(AverageError(stripeSource, DecodeOutput(info)) < 6);

    std::memset(jpeg, 0, sizeof(jpeg));
    Require(sceJpegEncEncode(handle, &rgba, nullptr) == 0);
    Require(jpeg[0] == 0xFF && jpeg[1] == 0xD8);

    JpegEncEncodeParam tooSmall = rgba;
    tooSmall.jpeg_size = 16;
    Require(sceJpegEncEncode(handle, &tooSmall, &info) == invalidSize);

    Require(encodeWith([](JpegEncEncodeParam& p) { p.image_width = 0; }) == invalidParam);
    Require(ThrowsRuntimeError([&] { encodeWith([](JpegEncEncodeParam& p) { p.encode_mode = 1; }); }));
    Require(ThrowsRuntimeError([&] { encodeWith([](JpegEncEncodeParam& p) { p.restart_interval = -2; }); }));
    Require(ThrowsRuntimeError([&] { encodeWith([](JpegEncEncodeParam& p) { p.restart_interval = INT32_MIN; }); }));

    constexpr std::uint32_t wideWidth = 48;
    constexpr std::uint32_t wideHeight = 32;
    alignas(4) static unsigned char wide[wideWidth * wideHeight * 4];
    for (std::size_t i = 0; i < sizeof(wide); ++i) wide[i] = static_cast<unsigned char>((i * 37) ^ (i / 192));
    static unsigned char wideGray[wideWidth * wideHeight];
    for (std::size_t i = 0; i < sizeof(wideGray); ++i) wideGray[i] = static_cast<unsigned char>((i * 53) ^ (i / 48));
    static unsigned char restarted[8192];
    auto encodeWide = [&](std::uint8_t sampling, std::int32_t restart, bool grayscale) {
        JpegEncEncodeParam p = ValidEncodeParam();
        p.image = grayscale ? wideGray : wide;
        p.image_size = grayscale ? sizeof(wideGray) : sizeof(wide);
        p.image_width = wideWidth;
        p.image_height = wideHeight;
        p.image_pitch = grayscale ? wideWidth : wideWidth * 4;
        p.pixel_format = grayscale ? 11 : 0;
        p.color_space = grayscale ? 2 : 1;
        p.sampling_type = sampling;
        p.jpeg = restarted;
        p.jpeg_size = sizeof(restarted);
        p.restart_interval = restart;
        JpegEncOutputInfo out{};
        Require(sceJpegEncEncode(handle, &p, &out) == 0);
        Require(IsJpeg(restarted, out.size) && out.height == wideHeight);
        const auto decoded = Decoder::Jpeg::Decode({restarted, out.size});
        Require(decoded.has_value() && decoded->width == wideWidth && decoded->height == wideHeight);
        return std::pair{ReadRestart(restarted, out.size), decoded->pixels};
    };

    const auto plain420 = encodeWide(2, 0, false);
    Require(plain420.first == std::pair{0, 0});
    const auto blocks420 = encodeWide(2, 2, false);
    Require(blocks420.first == std::pair{2, 2});
    Require(blocks420.second == plain420.second);
    const auto rows420 = encodeWide(2, -1, false);
    Require(rows420.first == std::pair{3, 1});
    Require(rows420.second == plain420.second);
    const auto plain422 = encodeWide(1, 0, false);
    const auto rows422 = encodeWide(1, -1, false);
    Require(rows422.first == std::pair{3, 3});
    Require(rows422.second == plain422.second);
    const auto blocks422 = encodeWide(1, 5, false);
    Require(blocks422.first == std::pair{5, 2});
    Require(blocks422.second == plain422.second);
    const auto plainGray = encodeWide(0, 0, true);
    const auto rowsGray = encodeWide(0, -1, true);
    Require(rowsGray.first == std::pair{6, 3});
    Require(rowsGray.second == plainGray.second);
    const auto blocksGray = encodeWide(0, 0xFFFF, true);
    Require(blocksGray.first == std::pair{0xFFFF, 0});
    Require(blocksGray.second == plainGray.second);

    Require(sceJpegEncDelete(handle) == 0);

    return 0;
}
