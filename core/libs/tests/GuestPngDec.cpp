#include "prx/libScePngDec/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Png.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <span>
#include <vector>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: PNG decoder assertion failed\n", location.file_name(), location.line());
    std::abort();
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

static const std::uint8_t PALETTE_OPAQUE[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x03, 0x00, 0x00, 0x00, 0x45, 0x68, 0xFD,
    0x16, 0x00, 0x00, 0x00, 0x0C, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xD6, 0x02, 0x8F, 0x7B, 0x00, 0x00, 0x00, 0x0E, 0x49, 0x44, 0x41,
    0x54, 0x78, 0xDA, 0x63, 0x60, 0x60, 0x64, 0x60, 0x62, 0x06, 0x00, 0x00, 0x11, 0x00, 0x07, 0x83,
    0xCA, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t PALETTE_1BIT_ADAM7[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x01, 0x3F, 0x7F, 0xAF,
    0xF1, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4C, 0x54, 0x45, 0x10, 0x20, 0x30, 0xC0, 0xB0, 0xA0, 0x34,
    0xE7, 0xF7, 0xE1, 0x00, 0x00, 0x00, 0x0E, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0x68, 0x60,
    0x60, 0x60, 0x70, 0x00, 0x00, 0x02, 0xC6, 0x00, 0xC1, 0x12, 0x35, 0x36, 0x6C, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t GRAY16[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x81, 0xD9, 0xFC,
    0x15, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32, 0x59, 0x7D,
    0x16, 0x00, 0x03, 0x0C, 0x01, 0xBF, 0x6E, 0xB9, 0xC6, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t GRAY16_TRNS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x07, 0x4D, 0x8E,
    0xBB, 0x00, 0x00, 0x00, 0x02, 0x74, 0x52, 0x4E, 0x53, 0x12, 0x34, 0x2F, 0xD3, 0x49, 0x5E, 0x00,
    0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32, 0x11, 0x5A, 0xCD, 0xB0,
    0xFA, 0x2C, 0x03, 0x03, 0x00, 0x0B, 0xDF, 0x02, 0x7C, 0x4E, 0x3C, 0x25, 0x55, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t RGB16_TRNS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x10, 0x02, 0x00, 0x00, 0x00, 0xAD, 0x44, 0x46,
    0x30, 0x00, 0x00, 0x00, 0x06, 0x74, 0x52, 0x4E, 0x53, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0x89,
    0xE4, 0x4E, 0xE6, 0x00, 0x00, 0x00, 0x23, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32,
    0x09, 0xAB, 0x98, 0xB5, 0x47, 0xE8, 0x7F, 0xD8, 0xFF, 0x59, 0xFF, 0x19, 0xFE, 0x33, 0x30, 0x30,
    0x36, 0x30, 0x32, 0x30, 0x34, 0x30, 0xFC, 0xFF, 0x0F, 0x00, 0x88, 0xFB, 0x0A, 0x69, 0xC3, 0x3F,
    0x47, 0x19, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const std::uint8_t GRAY_ALPHA16[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x10, 0x04, 0x00, 0x00, 0x00, 0x88, 0x2F, 0x19,
    0xEC, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x32, 0xB9, 0xF7,
    0x61, 0xF5, 0x59, 0x06, 0x20, 0xF8, 0xF7, 0xFF, 0xFF, 0x7F, 0xC6, 0x86, 0x46, 0x00, 0x48, 0x58,
    0x08, 0x8A, 0x8F, 0xC8, 0x5C, 0x92, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42,
    0x60, 0x82,
};

static const std::uint8_t RGBA16[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x10, 0x06, 0x00, 0x00, 0x00, 0x22, 0x26, 0xD1,
    0x67, 0x00, 0x00, 0x00, 0x2D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x01, 0x22, 0x00, 0xDD, 0xFF,
    0x00, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0x0D, 0xEF, 0xAB, 0xCD, 0x20, 0x01, 0xEE, 0xFE, 0xFF,
    0xFF, 0x00, 0x00, 0x01, 0x80, 0x40, 0xFF, 0xAB, 0x00, 0x00, 0xFF, 0xFF, 0x01, 0x02, 0x7F, 0xFE,
    0x80, 0xFF, 0x07, 0xD0, 0x10, 0x52, 0x90, 0xA2, 0xAC, 0x25, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static std::vector<std::uint8_t> RgbPng() {
    std::vector<std::uint8_t> rgb(3 * 2 * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>(10 + i * 13);
    return Decoder::Png::Encode(rgb, 3, 2, 3);
}

static std::vector<std::uint8_t> RgbPngWidth(std::uint32_t width) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>(i * 17 + 3);
    return Decoder::Png::Encode(rgb, width, 1, 3);
}

static PngDecDecodeParam DecodeParam(const std::vector<std::uint8_t>& png, std::vector<std::uint8_t>& image) {
    PngDecDecodeParam param{};
    param.png_mem_addr = png.data();
    param.image_mem_addr = image.data();
    param.png_mem_size = static_cast<std::uint32_t>(png.size());
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.pixel_format = 0;
    param.alpha_value = 0x77;
    param.image_pitch = 0;
    return param;
}

static void Check16BitImage(void* limitedHandle, void* enabledHandle, std::span<const std::uint8_t> png,
                            std::initializer_list<std::uint8_t> expected, std::uint16_t colorSpace,
                            std::uint32_t flags, std::uint32_t height = 2) {
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80690002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80690003);
    constexpr std::int32_t invalidWorkMemory = static_cast<std::int32_t>(0x80690005);
    const std::vector<std::uint8_t> input(png.begin(), png.end());
    const std::vector<std::uint8_t> expectedPixels(expected);
    Require(expectedPixels.size() == 2 * height * 4);
    for (const std::uint16_t format : {0, 1}) {
        for (const std::uint32_t padding : {0, 8}) {
            const std::uint32_t pitch = 8 + padding;
            const std::uint32_t size = pitch * height;
            std::vector<std::uint8_t> output(size + 16, 0xCC);
            PngDecDecodeParam decode = DecodeParam(input, output);
            decode.pixel_format = format;
            decode.image_pitch = padding == 0 ? 0 : pitch;
            decode.image_mem_size = size;
            PngDecImageInfo info{};
            Require(scePngDecDecode(limitedHandle, &decode, &info) == invalidWorkMemory);
            Require(info.image_width == 2 && info.image_height == height && info.bit_depth == 16);
            Require(info.color_space == colorSpace && info.image_flag == flags);
            Require(std::all_of(output.begin(), output.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
            Require(scePngDecDecode(enabledHandle, &decode, format == 0 ? &info : nullptr) == static_cast<std::int32_t>(2 << 16 | height));
            Require(info.image_width == 2 && info.image_height == height && info.bit_depth == 16);
            Require(info.color_space == colorSpace && info.image_flag == flags);
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < 2; ++x) {
                    for (std::uint32_t channel = 0; channel < 4; ++channel) {
                        const std::uint32_t inputChannel = format == 1 && channel < 3 ? 2 - channel : channel;
                        Require(output[y * pitch + x * 4 + channel] == expectedPixels[y * 8 + x * 4 + inputChannel]);
                    }
                }
                for (std::uint32_t offset = 8; offset < pitch; ++offset) Require(output[y * pitch + offset] == 0xCC);
            }
            Require(std::all_of(output.begin() + size, output.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
            std::fill(output.begin(), output.end(), 0xCC);
            decode.image_mem_size = size - 1;
            for (void* handle : {limitedHandle, enabledHandle}) {
                Require(scePngDecDecode(handle, &decode, &info) == (padding == 0 ? invalidSize : invalidParam));
                Require(std::all_of(output.begin(), output.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
                const auto validPitch = decode.image_pitch;
                decode.image_pitch = 7;
                Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
                Require(std::all_of(output.begin(), output.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
                decode.image_pitch = validPitch;
            }
        }
    }
}

static void CheckDimensionReporting(std::uint32_t width, std::uint32_t height) {
    constexpr std::int32_t invalidAddr = static_cast<std::int32_t>(0x80690001);
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80690002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80690003);
    constexpr std::int32_t invalidData = static_cast<std::int32_t>(0x80690010);
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>(i * 13 + 7);
    const auto png = Decoder::Png::Encode(rgb, width, height, 3);
    const PngDecCreateParam create{sizeof(PngDecCreateParam), 0, width};
    const auto memorySize = scePngDecQueryMemorySize(&create);
    Require(memorySize > 0);
    std::vector<std::uint8_t> memory(memorySize);
    void* handle = nullptr;
    Require(scePngDecCreate(&create, memory.data(), memorySize, &handle) == 0);
    const bool packed = width <= 32767 && height <= 32767;
    const auto result = packed ? static_cast<std::int32_t>(width << 16 | height) : 0;
    for (const std::uint16_t format : {0, 1}) {
        for (const std::uint32_t padding : {0, 8}) {
            const std::uint32_t pitch = width * 4 + padding;
            const std::uint32_t size = pitch * height;
            std::vector<std::uint8_t> image(size + 16, 0xCC);
            auto decode = DecodeParam(png, image);
            decode.pixel_format = format;
            decode.image_pitch = padding == 0 ? 0 : pitch;
            decode.image_mem_size = size;
            Require(scePngDecDecode(handle, &decode, nullptr) == (packed ? result : invalidAddr));
            if (!packed) Require(std::all_of(image.begin(), image.end(), [](auto byte) { return byte == 0xCC; }));
            PngDecImageInfo info{};
            Require(scePngDecDecode(handle, &decode, &info) == result);
            Require(info.image_width == width && info.image_height == height && info.color_space == 3 && info.bit_depth == 8 && info.image_flag == 0);
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    for (std::uint32_t channel = 0; channel < 3; ++channel) {
                        const auto inputChannel = format == 0 ? channel : 2 - channel;
                        Require(image[y * pitch + x * 4 + channel] == rgb[(static_cast<std::size_t>(y) * width + x) * 3 + inputChannel]);
                    }
                    Require(image[y * pitch + x * 4 + 3] == 0x77);
                }
                for (std::uint32_t offset = width * 4; offset < pitch; ++offset) Require(image[y * pitch + offset] == 0xCC);
            }
            Require(std::all_of(image.begin() + size, image.end(), [](auto byte) { return byte == 0xCC; }));
            if (packed) continue;
            std::fill(image.begin(), image.end(), 0xCC);
            decode.image_mem_size = 1;
            Require(scePngDecDecode(handle, &decode, nullptr) == invalidAddr);
            Require(scePngDecDecode(handle, &decode, &info) == (padding == 0 ? invalidSize : invalidParam));
            decode.image_mem_size = size;
            decode.image_pitch = 1;
            Require(scePngDecDecode(handle, &decode, nullptr) == invalidAddr);
            Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
            decode.image_mem_size = 0;
            Require(scePngDecDecode(handle, &decode, nullptr) == invalidSize);
            decode.image_mem_size = size;
            decode.pixel_format = 2;
            Require(scePngDecDecode(handle, &decode, nullptr) == invalidParam);
            decode.pixel_format = format;
            decode.png_mem_size = 8;
            Require(scePngDecDecode(handle, &decode, nullptr) == invalidData);
            Require(std::all_of(image.begin(), image.end(), [](auto byte) { return byte == 0xCC; }));
        }
    }
    Require(scePngDecDelete(handle) == 0);
}

int main() {
    constexpr std::int32_t invalidAddr = static_cast<std::int32_t>(0x80690001);
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80690002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80690003);
    constexpr std::int32_t invalidHandle = static_cast<std::int32_t>(0x80690004);
    constexpr std::int32_t invalidWorkMemory = static_cast<std::int32_t>(0x80690005);
    constexpr std::int32_t invalidData = static_cast<std::int32_t>(0x80690010);
    constexpr std::int32_t decodeError = static_cast<std::int32_t>(0x80690012);

    const PngDecCreateParam param{sizeof(PngDecCreateParam), 0, 1920};
    const std::int32_t memorySize = scePngDecQueryMemorySize(&param);
    Require(memorySize == 69264);
    Require(scePngDecQueryMemorySize(nullptr) == invalidAddr);
    const PngDecCreateParam badSize{sizeof(PngDecCreateParam) - 1, 0, 1920};
    Require(scePngDecQueryMemorySize(&badSize) == invalidSize);
    const PngDecCreateParam badAttribute{sizeof(PngDecCreateParam), 2, 1920};
    Require(scePngDecQueryMemorySize(&badAttribute) == invalidParam);
    const PngDecCreateParam zeroWidth{sizeof(PngDecCreateParam), 0, 0};
    Require(scePngDecQueryMemorySize(&zeroWidth) == invalidParam);
    const PngDecCreateParam hugeWidth{sizeof(PngDecCreateParam), 0, 1000002};
    Require(scePngDecQueryMemorySize(&hugeWidth) == invalidParam);
    const PngDecCreateParam overWidth{sizeof(PngDecCreateParam), 1, 1000001};
    Require(scePngDecQueryMemorySize(&overWidth) == invalidParam);
    const PngDecCreateParam maxWidth{sizeof(PngDecCreateParam), 1, 1000000};
    Require(scePngDecQueryMemorySize(&maxWidth) == 16054416);

    std::vector<unsigned char> memory(static_cast<std::size_t>(memorySize) + 1);
    unsigned char* unaligned = memory.data() + 1;
    void* handle = nullptr;
    Require(scePngDecCreate(nullptr, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == invalidAddr);
    Require(scePngDecCreate(&badSize, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == invalidSize);
    Require(scePngDecCreate(&overWidth, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == invalidParam);
    Require(scePngDecCreate(&param, nullptr, static_cast<std::uint32_t>(memorySize), &handle) == invalidAddr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize), nullptr) == invalidAddr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize) - 1, &handle) == invalidSize);
    Require(handle == nullptr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == 0);
    Require(reinterpret_cast<std::uintptr_t>(handle) % 16 == 0);
    Require(static_cast<unsigned char*>(handle) >= unaligned && static_cast<unsigned char*>(handle) < unaligned + 16);

    const std::vector<std::uint8_t> rgbPng = RgbPng();
    PngDecImageInfo info{};
    const PngDecParseParam parse{rgbPng.data(), static_cast<std::uint32_t>(rgbPng.size()), 0};
    Require(scePngDecParseHeader(&parse, &info) == 0);
    Require(info.image_width == 3 && info.image_height == 2 && info.color_space == 3 && info.bit_depth == 8 && info.image_flag == 0);
    const PngDecParseParam parsePalette{PALETTE_TRNS, sizeof(PALETTE_TRNS), 0};
    Require(scePngDecParseHeader(&parsePalette, &info) == 0);
    Require(info.color_space == 4 && info.bit_depth == 2 && info.image_flag == 2);
    for (const std::uint32_t size : {69u, 71u}) {
        const PngDecParseParam truncatedPalette{PALETTE_TRNS, size, 0};
        Require(scePngDecParseHeader(&truncatedPalette, &info) == 0);
        Require(info.color_space == 4 && info.bit_depth == 2 && info.image_flag == 0);
    }
    const PngDecParseParam completeTransparency{PALETTE_TRNS, 73, 0};
    Require(scePngDecParseHeader(&completeTransparency, &info) == 0 && info.image_flag == 2);
    Require(scePngDecParseHeader(nullptr, &info) == invalidAddr);
    Require(scePngDecParseHeader(&parse, nullptr) == invalidAddr);
    const PngDecParseParam parseNull{nullptr, 16, 0};
    Require(scePngDecParseHeader(&parseNull, &info) == invalidAddr);
    const PngDecParseParam parseEmpty{rgbPng.data(), 0, 0};
    Require(scePngDecParseHeader(&parseEmpty, &info) == invalidSize);
    const std::uint8_t garbage[40] = {1, 2, 3};
    const PngDecParseParam parseGarbage{garbage, sizeof(garbage), 0};
    Require(scePngDecParseHeader(&parseGarbage, &info) == invalidData);

    std::vector<std::uint8_t> image(3 * 2 * 4);
    PngDecDecodeParam decode = DecodeParam(rgbPng, image);
    info = {};
    Require(scePngDecDecode(handle, &decode, &info) == (3 << 16 | 2));
    Require(info.image_width == 3 && info.image_height == 2 && info.color_space == 3);
    for (std::size_t pixel = 0; pixel < 6; ++pixel) {
        for (std::size_t c = 0; c < 3; ++c) Require(image[pixel * 4 + c] == static_cast<std::uint8_t>(10 + (pixel * 3 + c) * 13));
        Require(image[pixel * 4 + 3] == 0x77);
    }

    const PngDecCreateParam narrowParam{sizeof(PngDecCreateParam), 0, 9};
    std::vector<unsigned char> narrowMemory(static_cast<std::size_t>(memorySize));
    void* narrowHandle = nullptr;
    Require(scePngDecCreate(&narrowParam, narrowMemory.data(), static_cast<std::uint32_t>(memorySize), &narrowHandle) == 0);
    const std::vector<std::uint8_t> alignedWidthPng = RgbPngWidth(16);
    std::vector<std::uint8_t> alignedWidthImage(16 * 4, 0xCC);
    PngDecDecodeParam narrowDecode = DecodeParam(alignedWidthPng, alignedWidthImage);
    Require(scePngDecDecode(narrowHandle, &narrowDecode, &info) == (16 << 16 | 1));
    Require(info.image_width == 16 && info.image_height == 1);

    const std::vector<std::uint8_t> oversizedWidthPng = RgbPngWidth(17);
    std::vector<std::uint8_t> oversizedWidthImage(17 * 4, 0xCC);
    narrowDecode = DecodeParam(oversizedWidthPng, oversizedWidthImage);
    info = {};
    Require(scePngDecDecode(narrowHandle, &narrowDecode, &info) == invalidWorkMemory);
    Require(info.image_width == 17 && info.image_height == 1);
    Require(std::all_of(oversizedWidthImage.begin(), oversizedWidthImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    Require(scePngDecDelete(narrowHandle) == 0);

    constexpr std::uint32_t pitch = 3 * 4 + 8;
    std::vector<std::uint8_t> padded(pitch * 2, 0xEE);
    decode = DecodeParam(rgbPng, padded);
    decode.pixel_format = 1;
    decode.image_pitch = pitch;
    Require(scePngDecDecode(handle, &decode, nullptr) == (3 << 16 | 2));
    for (std::size_t y = 0; y < 2; ++y) {
        for (std::size_t x = 0; x < 3; ++x) {
            const std::uint8_t* pixel = &padded[y * pitch + x * 4];
            const std::size_t source = (y * 3 + x) * 3;
            Require(pixel[0] == static_cast<std::uint8_t>(10 + (source + 2) * 13));
            Require(pixel[2] == static_cast<std::uint8_t>(10 + source * 13));
        }
    }
    for (std::size_t x = 3 * 4; x < pitch; ++x) Require(padded[x] == 0xEE);

    std::vector<std::uint8_t> invalidPitchImage(3 * 2 * 4 + 8, 0xCC);
    decode = DecodeParam(rgbPng, invalidPitchImage);
    decode.image_pitch = 3 * 4 + 2;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    Require(std::all_of(invalidPitchImage.begin(), invalidPitchImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));

    decode = DecodeParam(rgbPng, invalidPitchImage);
    decode.image_pitch = pitch;
    decode.image_mem_size = pitch + 3 * 4;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    Require(std::all_of(invalidPitchImage.begin(), invalidPitchImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));

    std::vector<std::uint8_t> fullPitchImage(pitch * 2, 0xCC);
    decode = DecodeParam(rgbPng, fullPitchImage);
    decode.image_pitch = pitch;
    Require(scePngDecDecode(handle, &decode, &info) == (3 << 16 | 2));

    const std::vector<std::uint8_t> palettePng(PALETTE_TRNS, PALETTE_TRNS + sizeof(PALETTE_TRNS));
    std::vector<std::uint8_t> paletteImage(2 * 2 * 4);
    decode = DecodeParam(palettePng, paletteImage);
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 2));
    Require(paletteImage == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 128}));
    Require(info.image_flag == 2);

    const std::vector<std::uint8_t> opaquePalettePng(PALETTE_OPAQUE, PALETTE_OPAQUE + sizeof(PALETTE_OPAQUE));
    std::vector<std::uint8_t> opaquePaletteImage(2 * 2 * 4, 0xCC);
    decode = DecodeParam(opaquePalettePng, opaquePaletteImage);
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 2));
    Require(opaquePaletteImage == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}));
    Require(info.color_space == 4 && info.image_flag == 0);
    decode.pixel_format = 1;
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 2));
    Require(opaquePaletteImage == std::vector<std::uint8_t>({0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 255, 255}));

    const std::vector<std::uint8_t> lowDepthPalettePng(PALETTE_1BIT_ADAM7, PALETTE_1BIT_ADAM7 + sizeof(PALETTE_1BIT_ADAM7));
    decode = DecodeParam(lowDepthPalettePng, opaquePaletteImage);
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 2));
    Require(opaquePaletteImage == std::vector<std::uint8_t>({192, 176, 160, 255, 16, 32, 48, 255, 16, 32, 48, 255, 192, 176, 160, 255}));
    Require(info.color_space == 4 && info.bit_depth == 1 && info.image_flag == 1);

    const std::vector<std::uint8_t> gray16Png(GRAY16, GRAY16 + sizeof(GRAY16));
    std::vector<std::uint8_t> gray16Image(2 * 4, 0xCC);
    decode = DecodeParam(gray16Png, gray16Image);
    Require(scePngDecDecode(handle, &decode, &info) == invalidWorkMemory);
    Require(std::all_of(gray16Image.begin(), gray16Image.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    Require(info.bit_depth == 16);

    decode = DecodeParam(rgbPng, image);
    Require(scePngDecDecode(nullptr, &decode, &info) == invalidHandle);
    Require(scePngDecDecode(handle, nullptr, &info) == invalidAddr);
    decode.png_mem_addr = nullptr;
    Require(scePngDecDecode(handle, &decode, &info) == invalidAddr);
    decode = DecodeParam(rgbPng, image);
    decode.image_mem_addr = nullptr;
    Require(scePngDecDecode(handle, &decode, &info) == invalidAddr);
    std::vector<std::uint8_t> misalignedImage(3 * 2 * 4 + 1, 0xCC);
    decode = DecodeParam(rgbPng, misalignedImage);
    decode.image_mem_addr = misalignedImage.data() + 1;
    decode.image_mem_size = static_cast<std::uint32_t>(misalignedImage.size() - 1);
    Require(scePngDecDecode(handle, &decode, &info) == invalidAddr);
    Require(std::all_of(misalignedImage.begin(), misalignedImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    decode = DecodeParam(rgbPng, image);
    decode.png_mem_size = 0;
    Require(scePngDecDecode(handle, &decode, &info) == invalidSize);
    decode = DecodeParam(rgbPng, image);
    decode.pixel_format = 2;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    std::vector<std::uint8_t> invalidAlphaImage(3 * 2 * 4, 0xCC);
    decode = DecodeParam(rgbPng, invalidAlphaImage);
    decode.alpha_value = 0x100;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    Require(std::all_of(invalidAlphaImage.begin(), invalidAlphaImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    decode.alpha_value = 0xFFFF;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    Require(std::all_of(invalidAlphaImage.begin(), invalidAlphaImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    decode = DecodeParam(rgbPng, image);
    decode.image_pitch = 3 * 4 - 1;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
    decode = DecodeParam(rgbPng, image);
    decode.image_mem_size = 3 * 2 * 4 - 1;
    Require(scePngDecDecode(handle, &decode, &info) == invalidSize);
    const std::vector<std::uint8_t> garbagePng(garbage, garbage + sizeof(garbage));
    decode = DecodeParam(garbagePng, image);
    Require(scePngDecDecode(handle, &decode, &info) == invalidData);
    const std::vector<std::uint8_t> truncatedPng(rgbPng.begin(), rgbPng.begin() + 40);
    decode = DecodeParam(truncatedPng, image);
    Require(scePngDecDecode(handle, &decode, &info) == decodeError);

    void* deepHandle = nullptr;
    const PngDecCreateParam deepParam{sizeof(PngDecCreateParam), 1, 1920};
    const auto deepSize = scePngDecQueryMemorySize(&deepParam);
    Require(deepSize == 85136);
    std::vector<unsigned char> deepMemory(static_cast<std::size_t>(deepSize));
    Require(scePngDecCreate(&deepParam, deepMemory.data(), static_cast<std::uint32_t>(deepSize), &deepHandle) == 0);
    Check16BitImage(handle, deepHandle, GRAY16, {0x12, 0x12, 0x12, 0x77, 0xAB, 0xAB, 0xAB, 0x77}, 2, 0, 1);
    Check16BitImage(handle, deepHandle, GRAY16_TRNS, {0x12, 0x12, 0x12, 0, 0x12, 0x12, 0x12, 0xFF, 0xAB, 0xAB, 0xAB, 0xFF, 0, 0, 0, 0xFF}, 2, 2);
    Check16BitImage(handle, deepHandle, RGB16_TRNS, {0x12, 0x56, 0x9A, 0, 0x12, 0x56, 0x9A, 0xFF, 0xFF, 0, 0x80, 0xFF, 0, 0x80, 0xFF, 0xFF}, 3, 2);
    Check16BitImage(handle, deepHandle, GRAY_ALPHA16, {0x12, 0x12, 0x12, 0xDE, 0xAB, 0xAB, 0xAB, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0x80}, 18, 0);
    Check16BitImage(handle, deepHandle, RGBA16, {0x12, 0x56, 0x9A, 0x0D, 0xAB, 0x20, 0xEE, 0xFF, 0, 0x80, 0xFF, 0, 0xFF, 1, 0x7F, 0x80}, 19, 0);
    decode = DecodeParam(rgbPng, image);
    Require(scePngDecDecode(deepHandle, &decode, &info) == (3 << 16 | 2));
    Require(scePngDecDelete(deepHandle) == 0);

    Require(scePngDecDelete(nullptr) == invalidHandle);
    Require(scePngDecDelete(static_cast<unsigned char*>(handle) + 1) == invalidHandle);
    Require(scePngDecDelete(handle) == 0);
    Require(scePngDecDelete(handle) == invalidHandle);
    Require(scePngDecDecode(handle, &decode, &info) == invalidHandle);

    for (const std::uint32_t dimension : {32767, 32768, 65536}) {
        CheckDimensionReporting(dimension, 1);
        CheckDimensionReporting(1, dimension);
    }
    return 0;
}
