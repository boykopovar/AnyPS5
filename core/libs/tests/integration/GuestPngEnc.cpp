#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Png.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

extern "C" {
int APS5_VABI scePngEncQueryMemorySize(const PngEncCreateParam*);
int APS5_VABI scePngEncCreate(const PngEncCreateParam*, void*, std::uint32_t, void**);
int APS5_VABI scePngEncDelete(void*);
int APS5_VABI scePngEncEncode(void*, const PngEncEncodeParam*, PngEncOutputInfo*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidAddr = static_cast<int>(0x80690101);
    constexpr int invalidSize = static_cast<int>(0x80690102);
    constexpr int invalidParam = static_cast<int>(0x80690103);
    constexpr int invalidHandle = static_cast<int>(0x80690104);
    constexpr int dataOverflow = static_cast<int>(0x80690110);
    constexpr std::uint32_t width = 5;
    constexpr std::uint32_t height = 3;
    constexpr std::uint32_t pitch = 24;

    PngEncCreateParam create{sizeof(PngEncCreateParam), 0, 16, 5};
    const int memorySize = scePngEncQueryMemorySize(&create);
    Require(memorySize > 0);
    Require(scePngEncQueryMemorySize(nullptr) == invalidAddr);
    PngEncCreateParam bad = create;
    bad.attribute = 1;
    Require(scePngEncQueryMemorySize(&bad) == invalidParam);
    bad = create;
    bad.max_filter_number = 6;
    Require(scePngEncQueryMemorySize(&bad) == invalidParam);
    bad = create;
    bad.max_image_width = 0;
    Require(scePngEncQueryMemorySize(&bad) == invalidSize);
    bad.max_image_width = 1000001;
    Require(scePngEncQueryMemorySize(&bad) == invalidSize);

    std::vector<std::uint64_t> memory((memorySize + 7) / 8);
    void* handle = nullptr;
    Require(scePngEncCreate(&create, nullptr, memorySize, &handle) == invalidAddr);
    Require(scePngEncCreate(&create, memory.data(), memorySize - 1, &handle) == invalidSize);
    Require(scePngEncCreate(&create, memory.data(), memorySize, &handle) == 0 && handle == memory.data());

    std::vector<std::uint8_t> image(pitch * height, 0xEE);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint8_t* pixel = &image[y * pitch + x * 4];
            pixel[0] = static_cast<std::uint8_t>(x * 40);
            pixel[1] = static_cast<std::uint8_t>(y * 80);
            pixel[2] = static_cast<std::uint8_t>(200 - x * 10);
            pixel[3] = static_cast<std::uint8_t>(100 + x + y);
        }
    }
    std::vector<std::uint8_t> png(4096);
    PngEncEncodeParam encode{image.data(), png.data(), static_cast<std::uint32_t>(image.size()), static_cast<std::uint32_t>(png.size()),
                             width, height, pitch, 0, 19, 8, 0, 15, 6};

    for (const std::uint16_t pixelFormat : {0, 1}) {
        for (const std::uint16_t colorSpace : {3, 19}) {
            encode.pixel_format = pixelFormat;
            encode.color_space = colorSpace;
            PngEncOutputInfo info{};
            const int size = scePngEncEncode(handle, &encode, &info);
            Require(size > 0 && info.data_size == static_cast<std::uint32_t>(size) && info.processed_height == height);
            const auto header = Decoder::Png::ParseHeader(std::span<const std::uint8_t>(png.data(), size));
            Require(header.has_value());
            Require(header->colorType == (colorSpace == 19 ? Decoder::Png::ColorType::Rgba : Decoder::Png::ColorType::Rgb));
            const auto decoded = Decoder::Png::Decode(std::span<const std::uint8_t>(png.data(), size));
            Require(decoded.has_value() && decoded->width == width && decoded->height == height);
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    const std::uint8_t* source = &image[y * pitch + x * 4];
                    const std::uint8_t* result = &decoded->pixels[(y * width + x) * 4];
                    Require(result[0] == source[pixelFormat == 1 ? 2 : 0] && result[1] == source[1]);
                    Require(result[2] == source[pixelFormat == 1 ? 0 : 2]);
                    Require(result[3] == (colorSpace == 19 ? source[3] : 0xFF));
                }
            }
        }
    }

    constexpr std::uint32_t rowSize = 16 * 4;
    std::vector<std::uint8_t> rows(rowSize * 6);
    std::uint32_t noise = 12345;
    for (std::uint32_t y = 0; y < 6; ++y) {
        for (std::uint32_t i = 0; i < rowSize; ++i) {
            noise = noise * 1103515245u + 12345u;
            rows[y * rowSize + i] = y == 2 ? static_cast<std::uint8_t>(noise >> 16) : static_cast<std::uint8_t>(i / 4 * 8);
        }
    }
    std::copy_n(rows.begin() + 2 * rowSize, rowSize, rows.begin() + 3 * rowSize);
    PngEncEncodeParam filtered{rows.data(), png.data(), static_cast<std::uint32_t>(rows.size()), static_cast<std::uint32_t>(png.size()),
                               16, 6, rowSize, 0, 19, 8, 0, 0, 6};
    const auto encodeWith = [&](std::uint16_t filterType) {
        filtered.filter_type = filterType;
        const int size = scePngEncEncode(handle, &filtered, nullptr);
        Require(size > 0);
        return std::vector<std::uint8_t>(png.begin(), png.begin() + size);
    };
    using namespace Decoder::Png;
    const std::pair<std::uint16_t, std::uint8_t> filterSets[] = {
        {0, FILTER_NONE}, {1, FILTER_SUB}, {2, FILTER_UP}, {4, FILTER_AVERAGE}, {8, FILTER_PAETH},
        {3, FILTER_SUB | FILTER_UP}, {6, FILTER_UP | FILTER_AVERAGE}, {9, FILTER_SUB | FILTER_PAETH},
        {14, FILTER_UP | FILTER_AVERAGE | FILTER_PAETH}, {15, FILTER_ALL}};
    for (const auto& [filterType, filters] : filterSets) Require(encodeWith(filterType) == Encode(rows, 16, 6, 4, {6, filters}));
    Require(encodeWith(3) != encodeWith(1) && encodeWith(3) != encodeWith(2) && encodeWith(15) != encodeWith(14));

    encode.png_mem_size = 20;
    PngEncOutputInfo info{1, 1};
    Require(scePngEncEncode(handle, &encode, &info) == dataOverflow && info.data_size == 0 && info.processed_height == 0);
    encode.png_mem_size = static_cast<std::uint32_t>(png.size());

    PngEncEncodeParam invalid = encode;
    Require(scePngEncEncode(handle, nullptr, nullptr) == invalidParam);
    invalid.image_mem_addr = nullptr;
    Require(scePngEncEncode(handle, &invalid, nullptr) == invalidAddr);
    for (auto change : {+[](PngEncEncodeParam& p) { p.pixel_format = 2; }, +[](PngEncEncodeParam& p) { p.color_space = 4; },
                        +[](PngEncEncodeParam& p) { p.bit_depth = 16; }, +[](PngEncEncodeParam& p) { p.clut_number = 1; },
                        +[](PngEncEncodeParam& p) { p.filter_type = 16; }, +[](PngEncEncodeParam& p) { p.compression_level = 10; }}) {
        invalid = encode;
        change(invalid);
        Require(scePngEncEncode(handle, &invalid, nullptr) == invalidParam);
    }
    for (auto change : {+[](PngEncEncodeParam& p) { p.image_width = 0; }, +[](PngEncEncodeParam& p) { p.image_width = 17; },
                        +[](PngEncEncodeParam& p) { p.image_pitch = 19; }, +[](PngEncEncodeParam& p) { p.image_mem_size = 67; },
                        +[](PngEncEncodeParam& p) { p.png_mem_size = 0; }}) {
        invalid = encode;
        change(invalid);
        Require(scePngEncEncode(handle, &invalid, nullptr) == invalidSize);
    }
    invalid = encode;
    invalid.image_mem_size = pitch * (height - 1) + width * 4;
    Require(scePngEncEncode(handle, &invalid, nullptr) > 0);

    Require(scePngEncDelete(handle) == 0);
    Require(scePngEncDelete(handle) == invalidHandle);
    Require(scePngEncEncode(handle, &encode, nullptr) == invalidHandle);
    Require(scePngEncDelete(nullptr) == invalidHandle);
}
