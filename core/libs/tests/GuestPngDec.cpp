#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Png.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
std::int32_t APS5_VABI scePngDecQueryMemorySize(const PngDecCreateParam*);
std::int32_t APS5_VABI scePngDecCreate(const PngDecCreateParam*, void*, std::uint32_t, void**);
std::int32_t APS5_VABI scePngDecDelete(void*);
std::int32_t APS5_VABI scePngDecParseHeader(const PngDecParseParam*, PngDecImageInfo*);
std::int32_t APS5_VABI scePngDecDecode(void*, const PngDecDecodeParam*, PngDecImageInfo*);
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
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

static std::vector<std::uint8_t> RgbPng() {
    std::vector<std::uint8_t> rgb(3 * 2 * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>(10 + i * 13);
    return Decoder::Png::Encode(rgb, 3, 2, 3);
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
    Require(memorySize > 0);
    Require(scePngDecQueryMemorySize(nullptr) == invalidParam);
    const PngDecCreateParam badAttribute{sizeof(PngDecCreateParam), 2, 1920};
    Require(scePngDecQueryMemorySize(&badAttribute) == invalidParam);
    const PngDecCreateParam zeroWidth{sizeof(PngDecCreateParam), 0, 0};
    Require(scePngDecQueryMemorySize(&zeroWidth) == invalidSize);
    const PngDecCreateParam hugeWidth{sizeof(PngDecCreateParam), 0, 1000002};
    Require(scePngDecQueryMemorySize(&hugeWidth) == invalidSize);
    const PngDecCreateParam maxWidth{sizeof(PngDecCreateParam), 1, 1000001};
    Require(scePngDecQueryMemorySize(&maxWidth) == memorySize);

    std::vector<unsigned char> memory(static_cast<std::size_t>(memorySize) + 1);
    unsigned char* unaligned = memory.data() + 1;
    void* handle = nullptr;
    Require(scePngDecCreate(nullptr, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == invalidParam);
    Require(scePngDecCreate(&param, nullptr, static_cast<std::uint32_t>(memorySize), &handle) == invalidAddr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize), nullptr) == invalidAddr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize) - 1, &handle) == invalidWorkMemory);
    Require(handle == nullptr);
    Require(scePngDecCreate(&param, unaligned, static_cast<std::uint32_t>(memorySize), &handle) == 0);
    Require(reinterpret_cast<std::uintptr_t>(handle) % 8 == 0);
    Require(static_cast<unsigned char*>(handle) >= unaligned && static_cast<unsigned char*>(handle) < unaligned + 8);

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
    Require(scePngDecParseHeader(nullptr, &info) == invalidParam);
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

    constexpr std::uint32_t pitch = 3 * 4 + 8;
    std::vector<std::uint8_t> padded(pitch + 3 * 4, 0xEE);
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

    const std::vector<std::uint8_t> palettePng(PALETTE_TRNS, PALETTE_TRNS + sizeof(PALETTE_TRNS));
    std::vector<std::uint8_t> paletteImage(2 * 2 * 4);
    decode = DecodeParam(palettePng, paletteImage);
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 2));
    Require(paletteImage == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 128}));
    Require(info.image_flag == 2);

    const std::vector<std::uint8_t> gray16Png(GRAY16, GRAY16 + sizeof(GRAY16));
    std::vector<std::uint8_t> gray16Image(2 * 4);
    decode = DecodeParam(gray16Png, gray16Image);
    Require(scePngDecDecode(handle, &decode, &info) == (2 << 16 | 1));
    Require(gray16Image == std::vector<std::uint8_t>({0x12, 0x12, 0x12, 0x77, 0xAB, 0xAB, 0xAB, 0x77}));
    Require(info.bit_depth == 16);

    decode = DecodeParam(rgbPng, image);
    Require(scePngDecDecode(nullptr, &decode, &info) == invalidHandle);
    Require(scePngDecDecode(handle, nullptr, &info) == invalidParam);
    decode.png_mem_addr = nullptr;
    Require(scePngDecDecode(handle, &decode, &info) == invalidAddr);
    decode = DecodeParam(rgbPng, image);
    decode.image_mem_addr = nullptr;
    Require(scePngDecDecode(handle, &decode, &info) == invalidAddr);
    decode = DecodeParam(rgbPng, image);
    decode.png_mem_size = 0;
    Require(scePngDecDecode(handle, &decode, &info) == invalidSize);
    decode = DecodeParam(rgbPng, image);
    decode.pixel_format = 2;
    Require(scePngDecDecode(handle, &decode, &info) == invalidParam);
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
    std::vector<unsigned char> deepMemory(static_cast<std::size_t>(memorySize));
    const PngDecCreateParam deepParam{sizeof(PngDecCreateParam), 1, 1920};
    Require(scePngDecCreate(&deepParam, deepMemory.data(), static_cast<std::uint32_t>(memorySize), &deepHandle) == 0);
    decode = DecodeParam(gray16Png, gray16Image);
    Require(ThrowsRuntimeError([&] { scePngDecDecode(deepHandle, &decode, &info); }));
    decode = DecodeParam(rgbPng, image);
    Require(scePngDecDecode(deepHandle, &decode, &info) == (3 << 16 | 2));
    Require(scePngDecDelete(deepHandle) == 0);

    Require(scePngDecDelete(nullptr) == invalidHandle);
    Require(scePngDecDelete(static_cast<unsigned char*>(handle) + 1) == invalidHandle);
    Require(scePngDecDelete(handle) == 0);
    Require(scePngDecDelete(handle) == invalidHandle);
    Require(scePngDecDecode(handle, &decode, &info) == invalidHandle);

    return 0;
}
