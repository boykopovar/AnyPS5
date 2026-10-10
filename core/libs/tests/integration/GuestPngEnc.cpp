#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Png.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

extern "C" {
int APS5_VABI scePngEncQueryMemorySize(const PngEncCreateParam*);
int APS5_VABI scePngEncCreate(const PngEncCreateParam*, void*, std::uint32_t, void**);
int APS5_VABI scePngEncDelete(void*);
int APS5_VABI scePngEncEncode(void*, const PngEncEncodeParam*, PngEncOutputInfo*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int InvalidAddr = static_cast<int>(0x80690101);
constexpr int InvalidSize = static_cast<int>(0x80690102);
constexpr int InvalidParam = static_cast<int>(0x80690103);
constexpr int InvalidHandle = static_cast<int>(0x80690104);
constexpr int DataOverflow = static_cast<int>(0x80690110);
constexpr std::uint32_t Width = 5;
constexpr std::uint32_t Height = 3;
constexpr std::uint32_t Pitch = 24;
constexpr std::uint32_t RowSize = 16 * 4;
constexpr std::uint32_t FilteredRows = 6;

PngEncCreateParam CreateParam() {
    return {sizeof(PngEncCreateParam), 0, 16, 5};
}

int QueriedMemorySize() {
    const auto create = CreateParam();
    const int size = scePngEncQueryMemorySize(&create);
    Require(size > 0, "query memory size returned " + std::to_string(size));
    return size;
}

class Encoder {
public:
    Encoder() : memorySize(QueriedMemorySize()), memory((memorySize + 7) / 8) {
        const auto create = CreateParam();
        RequireEqual(scePngEncCreate(&create, memory.data(), memorySize, &handle), 0, "create the encoder");
        Require(handle == memory.data(), "handle is not the provided memory");
    }

    ~Encoder() {
        if (handle != nullptr) scePngEncDelete(handle);
    }

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    void* Handle() const noexcept {
        return handle;
    }

    int Delete() {
        return scePngEncDelete(std::exchange(handle, nullptr));
    }

private:
    int memorySize;
    std::vector<std::uint64_t> memory;
    void* handle = nullptr;
};

class HandleGuard {
public:
    HandleGuard() = default;

    ~HandleGuard() {
        if (handle != nullptr) scePngEncDelete(handle);
    }

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    void* handle = nullptr;
};

class SampleImage {
public:
    SampleImage() : image(Pitch * Height, 0xEE), png(4096) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                std::uint8_t* pixel = &image[y * Pitch + x * 4];
                pixel[0] = static_cast<std::uint8_t>(x * 40);
                pixel[1] = static_cast<std::uint8_t>(y * 80);
                pixel[2] = static_cast<std::uint8_t>(200 - x * 10);
                pixel[3] = static_cast<std::uint8_t>(100 + x + y);
            }
        }
    }

    PngEncEncodeParam Param(std::uint16_t pixelFormat, std::uint16_t colorSpace) {
        return {image.data(), png.data(), static_cast<std::uint32_t>(image.size()), static_cast<std::uint32_t>(png.size()),
                Width, Height, Pitch, pixelFormat, colorSpace, 8, 0, 15, 6};
    }

    std::vector<std::uint8_t> image;
    std::vector<std::uint8_t> png;
};

class FilterImage {
public:
    FilterImage() : rows(RowSize * FilteredRows), png(4096) {
        std::uint32_t noise = 12345;
        for (std::uint32_t y = 0; y < FilteredRows; ++y) {
            for (std::uint32_t index = 0; index < RowSize; ++index) {
                noise = noise * 1103515245u + 12345u;
                rows[y * RowSize + index] = y == 2 ? static_cast<std::uint8_t>(noise >> 16) : static_cast<std::uint8_t>(index / 4 * 8);
            }
        }
        std::copy_n(rows.begin() + 2 * RowSize, RowSize, rows.begin() + 3 * RowSize);
    }

    std::vector<std::uint8_t> Encode(void* handle, std::uint16_t filterType) {
        const PngEncEncodeParam param{rows.data(), png.data(), static_cast<std::uint32_t>(rows.size()), static_cast<std::uint32_t>(png.size()),
                                      16, FilteredRows, RowSize, 0, 19, 8, 0, filterType, 6};
        const int size = scePngEncEncode(handle, &param, nullptr);
        Require(size > 0, "encode with filter type " + std::to_string(filterType) + " returned " + std::to_string(size));
        return std::vector<std::uint8_t>(png.begin(), png.begin() + size);
    }

    std::vector<std::uint8_t> rows;
    std::vector<std::uint8_t> png;
};

using ParamChange = void (*)(PngEncEncodeParam&);

struct InvalidChange {
    const char* name;
    ParamChange change;
};

const Case queryValid{"QueryMemorySize_ValidParam_ReturnsAPositiveSize", [] {
    const auto create = CreateParam();
    const int size = scePngEncQueryMemorySize(&create);
    Require(size > 0, "query memory size returned " + std::to_string(size));
}};

const Case queryNull{"QueryMemorySize_NullParam_FailsWithInvalidAddr", [] {
    RequireEqual(scePngEncQueryMemorySize(nullptr), InvalidAddr, "null create param");
}};

const Case queryAttribute{"QueryMemorySize_NonZeroAttribute_FailsWithInvalidParam", [] {
    auto bad = CreateParam();
    bad.attribute = 1;
    RequireEqual(scePngEncQueryMemorySize(&bad), InvalidParam, "attribute 1");
}};

const Case queryFilters{"QueryMemorySize_TooManyFilters_FailsWithInvalidParam", [] {
    auto bad = CreateParam();
    bad.max_filter_number = 6;
    RequireEqual(scePngEncQueryMemorySize(&bad), InvalidParam, "max filter number 6");
}};

const Case queryWidth{"QueryMemorySize_WidthOutOfRange_FailsWithInvalidSize", [] {
    auto bad = CreateParam();
    for (const std::uint32_t width : {0u, 1000001u}) {
        bad.max_image_width = width;
        RequireEqual(scePngEncQueryMemorySize(&bad), InvalidSize, "max image width " + std::to_string(width));
    }
}};

const Case createNullMemory{"Create_NullMemory_FailsWithInvalidAddr", [] {
    const auto create = CreateParam();
    HandleGuard guard;
    RequireEqual(scePngEncCreate(&create, nullptr, QueriedMemorySize(), &guard.handle), InvalidAddr, "null memory");
}};

const Case createSmallMemory{"Create_MemoryOneByteTooSmall_FailsWithInvalidSize", [] {
    const auto create = CreateParam();
    const int memorySize = QueriedMemorySize();
    std::vector<std::uint64_t> memory((memorySize + 7) / 8);
    HandleGuard guard;
    RequireEqual(scePngEncCreate(&create, memory.data(), memorySize - 1, &guard.handle), InvalidSize, "memory size minus one");
}};

const Case createValid{"Create_ValidMemory_ReturnsTheMemoryAsHandle", [] {
    const auto create = CreateParam();
    const int memorySize = QueriedMemorySize();
    std::vector<std::uint64_t> memory((memorySize + 7) / 8);
    HandleGuard guard;
    RequireEqual(scePngEncCreate(&create, memory.data(), memorySize, &guard.handle), 0, "create result");
    Require(guard.handle == memory.data(), "handle is not the provided memory");
}};

const Case encodeRoundTrip{"Encode_PixelFormatsAndColorSpaces_DecodesToTheSourcePixels", [] {
    const Encoder encoder;
    SampleImage sample;
    for (const std::uint16_t pixelFormat : {0, 1}) {
        for (const std::uint16_t colorSpace : {3, 19}) {
            const auto encode = sample.Param(pixelFormat, colorSpace);
            const auto input = "pixel format " + std::to_string(pixelFormat) + " color space " + std::to_string(colorSpace);
            PngEncOutputInfo info{};
            const int size = scePngEncEncode(encoder.Handle(), &encode, &info);
            Require(size > 0, "encode failed for " + input);
            RequireEqual(info.data_size, static_cast<std::uint32_t>(size), "data size for " + input);
            RequireEqual(info.processed_height, Height, "processed height for " + input);
            const std::span<const std::uint8_t> png(sample.png.data(), size);
            const auto header = Decoder::Png::ParseHeader(png);
            Require(header.has_value(), "header parse for " + input);
            RequireEqual(header->colorType, colorSpace == 19 ? Decoder::Png::ColorType::Rgba : Decoder::Png::ColorType::Rgb, "color type for " + input);
            const auto decoded = Decoder::Png::Decode(png);
            Require(decoded.has_value(), "decode for " + input);
            RequireEqual(decoded->width, Width, "decoded width for " + input);
            RequireEqual(decoded->height, Height, "decoded height for " + input);
            for (std::uint32_t y = 0; y < Height; ++y) {
                for (std::uint32_t x = 0; x < Width; ++x) {
                    const std::uint8_t* source = &sample.image[y * Pitch + x * 4];
                    const std::uint8_t* result = &decoded->pixels[(y * Width + x) * 4];
                    const auto where = input + " at " + std::to_string(x) + "," + std::to_string(y);
                    RequireEqual(result[0], source[pixelFormat == 1 ? 2 : 0], "red channel for " + where);
                    RequireEqual(result[1], source[1], "green channel for " + where);
                    RequireEqual(result[2], source[pixelFormat == 1 ? 0 : 2], "blue channel for " + where);
                    RequireEqual(result[3], colorSpace == 19 ? source[3] : static_cast<std::uint8_t>(0xFF), "alpha channel for " + where);
                }
            }
        }
    }
}};

const Case encodeFilters{"Encode_FilterTypes_MatchTheReferenceEncoderFilterSets", [] {
    using namespace Decoder::Png;
    const Encoder encoder;
    FilterImage image;
    const std::pair<std::uint16_t, std::uint8_t> filterSets[] = {
        {0, FILTER_NONE}, {1, FILTER_SUB}, {2, FILTER_UP}, {4, FILTER_AVERAGE}, {8, FILTER_PAETH},
        {3, FILTER_SUB | FILTER_UP}, {6, FILTER_UP | FILTER_AVERAGE}, {9, FILTER_SUB | FILTER_PAETH},
        {14, FILTER_UP | FILTER_AVERAGE | FILTER_PAETH}, {15, FILTER_ALL}};
    for (const auto& [filterType, filters] : filterSets) {
        Require(image.Encode(encoder.Handle(), filterType) == Decoder::Png::Encode(image.rows, 16, FilteredRows, 4, {6, filters}),
            "filter type " + std::to_string(filterType) + " differs from the reference encoder");
    }
}};

const Case encodeFilterCombinations{"Encode_CombinedFilterTypes_DifferFromTheirSubsets", [] {
    const Encoder encoder;
    FilterImage image;
    Require(image.Encode(encoder.Handle(), 3) != image.Encode(encoder.Handle(), 1), "filter type 3 equals filter type 1");
    Require(image.Encode(encoder.Handle(), 3) != image.Encode(encoder.Handle(), 2), "filter type 3 equals filter type 2");
    Require(image.Encode(encoder.Handle(), 15) != image.Encode(encoder.Handle(), 14), "filter type 15 equals filter type 14");
}};

const Case encodeOverflow{"Encode_OutputTooSmall_FailsWithDataOverflowAndClearsInfo", [] {
    const Encoder encoder;
    SampleImage sample;
    auto encode = sample.Param(1, 19);
    encode.png_mem_size = 20;
    PngEncOutputInfo info{1, 1};
    RequireEqual(scePngEncEncode(encoder.Handle(), &encode, &info), DataOverflow, "png memory size 20");
    RequireEqual(info.data_size, 0u, "data size after overflow");
    RequireEqual(info.processed_height, 0u, "processed height after overflow");
}};

const Case encodeNullParam{"Encode_NullParam_FailsWithInvalidParam", [] {
    const Encoder encoder;
    RequireEqual(scePngEncEncode(encoder.Handle(), nullptr, nullptr), InvalidParam, "null encode param");
}};

const Case encodeNullImage{"Encode_NullImage_FailsWithInvalidAddr", [] {
    const Encoder encoder;
    SampleImage sample;
    auto invalid = sample.Param(1, 19);
    invalid.image_mem_addr = nullptr;
    RequireEqual(scePngEncEncode(encoder.Handle(), &invalid, nullptr), InvalidAddr, "null image");
}};

const Case encodeInvalidParams{"Encode_UnsupportedFormatFields_FailWithInvalidParam", [] {
    const Encoder encoder;
    SampleImage sample;
    const InvalidChange changes[] = {
        {"pixel format 2", [](PngEncEncodeParam& p) { p.pixel_format = 2; }},
        {"color space 4", [](PngEncEncodeParam& p) { p.color_space = 4; }},
        {"bit depth 16", [](PngEncEncodeParam& p) { p.bit_depth = 16; }},
        {"clut number 1", [](PngEncEncodeParam& p) { p.clut_number = 1; }},
        {"filter type 16", [](PngEncEncodeParam& p) { p.filter_type = 16; }},
        {"compression level 10", [](PngEncEncodeParam& p) { p.compression_level = 10; }}};
    for (const auto& [name, change] : changes) {
        auto invalid = sample.Param(1, 19);
        change(invalid);
        RequireEqual(scePngEncEncode(encoder.Handle(), &invalid, nullptr), InvalidParam, name);
    }
}};

const Case encodeInvalidSizes{"Encode_InvalidSizes_FailWithInvalidSize", [] {
    const Encoder encoder;
    SampleImage sample;
    const InvalidChange changes[] = {
        {"image width 0", [](PngEncEncodeParam& p) { p.image_width = 0; }},
        {"image width 17", [](PngEncEncodeParam& p) { p.image_width = 17; }},
        {"image pitch 19", [](PngEncEncodeParam& p) { p.image_pitch = 19; }},
        {"image memory size 67", [](PngEncEncodeParam& p) { p.image_mem_size = 67; }},
        {"png memory size 0", [](PngEncEncodeParam& p) { p.png_mem_size = 0; }}};
    for (const auto& [name, change] : changes) {
        auto invalid = sample.Param(1, 19);
        change(invalid);
        RequireEqual(scePngEncEncode(encoder.Handle(), &invalid, nullptr), InvalidSize, name);
    }
}};

const Case encodeUnpaddedLastRow{"Encode_ImageSizeWithoutLastRowPadding_Succeeds", [] {
    const Encoder encoder;
    SampleImage sample;
    auto encode = sample.Param(1, 19);
    encode.image_mem_size = Pitch * (Height - 1) + Width * 4;
    const int size = scePngEncEncode(encoder.Handle(), &encode, nullptr);
    Require(size > 0, "encode without last row padding returned " + std::to_string(size));
}};

const Case deleteLive{"Delete_LiveHandle_Succeeds", [] {
    Encoder encoder;
    RequireEqual(encoder.Delete(), 0, "delete");
}};

const Case deleteTwice{"Delete_DeletedHandle_FailsWithInvalidHandle", [] {
    Encoder encoder;
    void* handle = encoder.Handle();
    RequireEqual(encoder.Delete(), 0, "first delete");
    RequireEqual(scePngEncDelete(handle), InvalidHandle, "second delete");
}};

const Case encodeDeleted{"Encode_DeletedHandle_FailsWithInvalidHandle", [] {
    Encoder encoder;
    void* handle = encoder.Handle();
    SampleImage sample;
    const auto encode = sample.Param(1, 19);
    RequireEqual(encoder.Delete(), 0, "delete");
    RequireEqual(scePngEncEncode(handle, &encode, nullptr), InvalidHandle, "encode after delete");
}};

const Case deleteNull{"Delete_NullHandle_FailsWithInvalidHandle", [] {
    RequireEqual(scePngEncDelete(nullptr), InvalidHandle, "delete null");
}};

} // namespace
