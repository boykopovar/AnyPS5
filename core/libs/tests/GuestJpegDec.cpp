#include "prx/libSceJpegDec/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <vector>

static_assert(sizeof(JpegDecCreateParam) == 0xC);
static_assert(sizeof(JpegDecParseParam) == 0x10);
static_assert(offsetof(JpegDecParseParam, jpeg_mem_size) == 0x8 && offsetof(JpegDecParseParam, decode_mode) == 0xC && offsetof(JpegDecParseParam, down_scale) == 0xE);
static_assert(sizeof(JpegDecDecodeParam) == 0x30);
static_assert(offsetof(JpegDecDecodeParam, coefficient_mem_addr) == 0x10 && offsetof(JpegDecDecodeParam, jpeg_mem_size) == 0x18);
static_assert(offsetof(JpegDecDecodeParam, image_mem_size) == 0x1C && offsetof(JpegDecDecodeParam, coefficient_mem_size) == 0x20);
static_assert(offsetof(JpegDecDecodeParam, decode_mode) == 0x24 && offsetof(JpegDecDecodeParam, down_scale) == 0x26);
static_assert(offsetof(JpegDecDecodeParam, pixel_format) == 0x28 && offsetof(JpegDecDecodeParam, alpha_value) == 0x2A && offsetof(JpegDecDecodeParam, image_pitch) == 0x2C);
static_assert(sizeof(JpegDecImageInfo) == 0x24);
static_assert(offsetof(JpegDecImageInfo, color_space) == 0x8 && offsetof(JpegDecImageInfo, component_number) == 0xA);
static_assert(offsetof(JpegDecImageInfo, component_id) == 0xC && offsetof(JpegDecImageInfo, sampling_factor) == 0x10);
static_assert(offsetof(JpegDecImageInfo, coefficient_mem_size) == 0x14 && offsetof(JpegDecImageInfo, suitable_csc_attribute) == 0x18);
static_assert(offsetof(JpegDecImageInfo, output_image_width) == 0x1C && offsetof(JpegDecImageInfo, output_image_height) == 0x20);

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

static std::vector<std::uint8_t> TestJpeg(std::uint32_t width, std::uint32_t height, std::uint32_t channels) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * channels);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(10 + i * 13);
    return Decoder::Jpeg::Encode(pixels, width, height, channels, 90);
}

static JpegDecDecodeParam DecodeParam(const std::vector<std::uint8_t>& jpeg, std::vector<std::uint8_t>& image, std::uint32_t pitch) {
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.image_mem_addr = image.data();
    param.coefficient_mem_addr = nullptr;
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.coefficient_mem_size = 0;
    param.decode_mode = 0;
    param.down_scale = 1;
    param.pixel_format = 0;
    param.alpha_value = 0xFF;
    param.image_pitch = pitch;
    return param;
}

int main() {
    constexpr std::int32_t invalidAddr = static_cast<std::int32_t>(0x80650001);
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80650002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80650003);
    constexpr std::int32_t invalidHandle = static_cast<std::int32_t>(0x80650004);
    constexpr std::int32_t invalidData = static_cast<std::int32_t>(0x80650010);

    const std::vector<std::uint8_t> jpeg = TestJpeg(7, 5, 3);
    JpegDecImageInfo info{};
    const JpegDecParseParam parseParam{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 0, 1};
    Require(sceJpegDecParseHeader(nullptr, &info) == invalidAddr);
    Require(sceJpegDecParseHeader(&parseParam, nullptr) == invalidAddr);
    const JpegDecParseParam emptyParam{nullptr, 0, 0, 1};
    Require(sceJpegDecParseHeader(&emptyParam, &info) == invalidAddr);
    for (const std::uint16_t downScale : {std::uint16_t{0}, std::uint16_t{3}, std::uint16_t{16}}) {
        const JpegDecParseParam unknownScaleParse{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 0, downScale};
        Require(sceJpegDecParseHeader(&unknownScaleParse, &info) == invalidParam);
    }
    const JpegDecParseParam otherModeParse{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 1, 1};
    Require(ThrowsRuntimeError([&] { sceJpegDecParseHeader(&otherModeParse, &info); }));
    for (const auto [downScale, outputWidth, outputHeight] : {std::array<std::uint32_t, 3>{2, 4, 3}, std::array<std::uint32_t, 3>{4, 2, 2}, std::array<std::uint32_t, 3>{8, 1, 1}}) {
        const JpegDecParseParam downScaledParse{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 0, static_cast<std::uint16_t>(downScale)};
        info = {};
        Require(sceJpegDecParseHeader(&downScaledParse, &info) == 0);
        Require(info.image_width == 7 && info.image_height == 5 && info.component_number == 3);
        Require(info.output_image_width == outputWidth && info.output_image_height == outputHeight);
    }
    const std::vector<std::uint8_t> grayJpeg = TestJpeg(3, 2, 1);
    const JpegDecParseParam grayParse{grayJpeg.data(), static_cast<std::uint32_t>(grayJpeg.size()), 0, 1};
    Require(sceJpegDecParseHeader(&grayParse, &info) == 0);
    Require(info.image_width == 3 && info.image_height == 2 && info.component_number == 1);
    Require(info.color_space == 2 && info.component_id[0] == 1 && info.sampling_factor[0] == 0x11);
    Require(info.component_id[1] == 0 && info.sampling_factor[1] == 0);
    Require(sceJpegDecParseHeader(&parseParam, &info) == 0);
    Require(info.image_width == 7 && info.image_height == 5 && info.component_number == 3);
    Require(info.output_image_width == 7 && info.output_image_height == 5);
    Require(info.coefficient_mem_size == 0 && info.suitable_csc_attribute == 1);
    Require(info.color_space == 1);
    for (std::uint32_t i = 0; i < 3; ++i) Require(info.component_id[i] == i + 1 && info.sampling_factor[i] == 0x11);
    std::vector<std::uint8_t> progressiveHeader = jpeg;
    bool changedFrame = false;
    for (std::size_t offset = 2; offset + 3 < progressiveHeader.size();) {
        const std::uint8_t marker = progressiveHeader[offset + 1];
        const std::size_t length = (static_cast<std::size_t>(progressiveHeader[offset + 2]) << 8) | progressiveHeader[offset + 3];
        if (marker == 0xC0) {
            progressiveHeader[offset + 1] = 0xC2;
            changedFrame = true;
            break;
        }
        offset += 2 + length;
    }
    Require(changedFrame);
    const JpegDecParseParam progressiveParse{progressiveHeader.data(), static_cast<std::uint32_t>(progressiveHeader.size()), 0, 1};
    Require(sceJpegDecParseHeader(&progressiveParse, &info) == 0);
    Require(info.coefficient_mem_size == 448);
    std::vector<std::uint8_t> hugeProgressive = progressiveHeader;
    for (std::size_t offset = 2; offset + 8 < hugeProgressive.size();) {
        if (hugeProgressive[offset + 1] == 0xC2) {
            hugeProgressive[offset + 5] = hugeProgressive[offset + 6] = hugeProgressive[offset + 7] = hugeProgressive[offset + 8] = 0xFF;
            break;
        }
        offset += 2 + ((static_cast<std::size_t>(hugeProgressive[offset + 2]) << 8) | hugeProgressive[offset + 3]);
    }
    const JpegDecParseParam hugeParse{hugeProgressive.data(), static_cast<std::uint32_t>(hugeProgressive.size()), 0, 1};
    info = {};
    Require(sceJpegDecParseHeader(&hugeParse, &info) == static_cast<std::int32_t>(0x80650005));
    Require(sceJpegDecParseHeader(&parseParam, &info) == 0);

    const JpegDecCreateParam param{sizeof(JpegDecCreateParam), info.suitable_csc_attribute, info.image_width};
    const std::int32_t memorySize = sceJpegDecQueryMemorySize(&param);
    Require(memorySize == 32128);
    const JpegDecCreateParam fullParam{sizeof(JpegDecCreateParam), 2, info.image_width};
    const std::int32_t fullMemorySize = sceJpegDecQueryMemorySize(&fullParam);
    Require(fullMemorySize == 37504);
    for (const std::uint32_t width : {0u, 65536u, UINT32_MAX}) {
        const JpegDecCreateParam badWidth{sizeof(JpegDecCreateParam), 1, width};
        Require(sceJpegDecQueryMemorySize(&badWidth) == invalidParam);
    }
    const JpegDecCreateParam largestWidth{sizeof(JpegDecCreateParam), 1, 65535};
    Require(sceJpegDecQueryMemorySize(&largestWidth) == 3569344);
    Require(sceJpegDecQueryMemorySize(nullptr) == invalidAddr);
    const JpegDecCreateParam badSize{sizeof(JpegDecCreateParam) - 4, 1, 7};
    Require(sceJpegDecQueryMemorySize(&badSize) == invalidSize);
    for (const std::uint32_t attribute : {0u, 3u}) {
        const JpegDecCreateParam badAttribute{sizeof(JpegDecCreateParam), attribute, 7};
        Require(sceJpegDecQueryMemorySize(&badAttribute) == invalidParam);
    }

    std::vector<std::uint8_t> memory(static_cast<std::size_t>(fullMemorySize) + 32);
    void* handle = nullptr;
    Require(sceJpegDecCreate(&param, memory.data(), 0, &handle) == invalidSize);
    Require(sceJpegDecCreate(&param, memory.data(), static_cast<std::uint32_t>(memorySize), nullptr) == invalidAddr);
    Require(sceJpegDecCreate(&fullParam, memory.data(), static_cast<std::uint32_t>(fullMemorySize), &handle) == 0);
    Require(handle != nullptr && reinterpret_cast<std::uintptr_t>(handle) % 32 == 0);
    Require(sceJpegDecCreate(&param, memory.data(), 0, nullptr) == invalidSize);
    Require(sceJpegDecDelete(nullptr) == invalidHandle);
    Require(sceJpegDecDelete(handle) == 0);

    Require(sceJpegDecCreate(&param, memory.data(), static_cast<std::uint32_t>(memorySize), &handle) == 0);

    const std::optional<Decoder::Jpeg::Image> reference = Decoder::Jpeg::Decode(jpeg);
    Require(reference && reference->width == 7 && reference->height == 5 && reference->channels == 3);

    std::vector<std::uint8_t> image(7 * 5 * 4, 0xCC);
    JpegDecDecodeParam decodeParam = DecodeParam(jpeg, image, 7 * 4);
    std::vector<std::uint8_t> tallSampling = TestJpeg(31, 5, 3);
    for (std::size_t offset = 2; offset + 3 < tallSampling.size();) {
        const std::uint8_t marker = tallSampling[offset + 1];
        const std::size_t length = (static_cast<std::size_t>(tallSampling[offset + 2]) << 8) | tallSampling[offset + 3];
        if (marker == 0xC0) {
            tallSampling[offset + 11] = 0x14;
            break;
        }
        offset += 2 + length;
    }
    std::vector<std::uint8_t> tallSamplingOutput(31 * 5 * 4);
    JpegDecDecodeParam tallSamplingDecode = DecodeParam(tallSampling, tallSamplingOutput, 0);
    Require(sceJpegDecDecode(handle, &tallSamplingDecode, &info) == static_cast<std::int32_t>(0x80650006));
    JpegDecDecodeParam progressiveDecode = DecodeParam(progressiveHeader, image, 7 * 4);
    Require(sceJpegDecDecode(handle, &progressiveDecode, &info) == static_cast<std::int32_t>(0x80650005));
    std::array<std::uint8_t, 447> insufficientCoefficients{};
    progressiveDecode.coefficient_mem_addr = insufficientCoefficients.data();
    progressiveDecode.coefficient_mem_size = static_cast<std::uint32_t>(insufficientCoefficients.size());
    Require(sceJpegDecDecode(handle, &progressiveDecode, &info) == static_cast<std::int32_t>(0x80650005));
    Require(sceJpegDecDecode(nullptr, &decodeParam, &info) == invalidHandle);
    Require(sceJpegDecDecode(handle, nullptr, &info) == invalidAddr);
    JpegDecDecodeParam badFormat = decodeParam;
    badFormat.pixel_format = 2;
    Require(sceJpegDecDecode(handle, &badFormat, &info) == invalidParam);
    JpegDecDecodeParam unknownScale = decodeParam;
    unknownScale.down_scale = 3;
    Require(sceJpegDecDecode(handle, &unknownScale, &info) == invalidParam);
    JpegDecDecodeParam invalidOptions = decodeParam;
    invalidOptions.alpha_value = 256;
    Require(sceJpegDecDecode(handle, &invalidOptions, &info) == invalidParam);
    invalidOptions = decodeParam;
    invalidOptions.decode_mode = 2;
    Require(sceJpegDecDecode(handle, &invalidOptions, &info) == invalidParam);
    invalidOptions = decodeParam;
    invalidOptions.image_mem_addr = image.data() + 1;
    Require(sceJpegDecDecode(handle, &invalidOptions, &info) == invalidAddr);
    std::vector<std::uint8_t> invalidAlphaImage(7 * 5 * 4, 0xCC);
    JpegDecDecodeParam invalidAlpha = DecodeParam(jpeg, invalidAlphaImage, 7 * 4);
    invalidAlpha.alpha_value = 0x100;
    Require(sceJpegDecDecode(handle, &invalidAlpha, &info) == invalidParam);
    Require(std::all_of(invalidAlphaImage.begin(), invalidAlphaImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    invalidAlpha.alpha_value = 0xFFFF;
    Require(sceJpegDecDecode(handle, &invalidAlpha, &info) == invalidParam);
    Require(std::all_of(invalidAlphaImage.begin(), invalidAlphaImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    std::vector<std::uint8_t> misalignedImage(7 * 5 * 4 + 1, 0xCC);
    JpegDecDecodeParam misaligned = DecodeParam(jpeg, misalignedImage, 7 * 4);
    misaligned.image_mem_addr = misalignedImage.data() + 1;
    misaligned.image_mem_size = static_cast<std::uint32_t>(misalignedImage.size() - 1);
    Require(sceJpegDecDecode(handle, &misaligned, &info) == invalidAddr);
    Require(std::all_of(misalignedImage.begin(), misalignedImage.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
    JpegDecDecodeParam otherMode = decodeParam;
    otherMode.decode_mode = 1;
    Require(ThrowsRuntimeError([&] { sceJpegDecDecode(handle, &otherMode, &info); }));
    info = {};
    Require(sceJpegDecDecode(handle, &decodeParam, &info) == static_cast<std::int32_t>(7 << 16 | 5));
    Require(info.image_width == 7 && info.image_height == 5);
    Require(info.output_image_width == 7 && info.output_image_height == 5);
    for (std::size_t pixel = 0; pixel < 7 * 5; ++pixel) {
        for (std::size_t c = 0; c < 3; ++c) Require(image[pixel * 4 + c] == reference->pixels[pixel * 3 + c]);
        Require(image[pixel * 4 + 3] == 0xFF);
    }

    constexpr std::uint32_t pitch = 7 * 4 + 8;
    std::vector<std::uint8_t> bgra(5 * pitch, 0xEE);
    JpegDecDecodeParam bgraParam = DecodeParam(jpeg, bgra, pitch);
    bgraParam.pixel_format = 1;
    bgraParam.alpha_value = 0x77;
    Require(sceJpegDecDecode(handle, &bgraParam, nullptr) == static_cast<std::int32_t>(7 << 16 | 5));
    for (std::size_t y = 0; y < 5; ++y) {
        for (std::size_t x = 0; x < 7; ++x) {
            const std::uint8_t* pixel = &bgra[y * pitch + x * 4];
            const std::uint8_t* rgba = &image[(y * 7 + x) * 4];
            Require(pixel[0] == rgba[2] && pixel[1] == rgba[1] && pixel[2] == rgba[0] && pixel[3] == 0x77);
        }
    }
    for (std::size_t y = 0; y < 4; ++y) {
        for (std::size_t x = 7 * 4; x < pitch; ++x) Require(bgra[y * pitch + x] == 0xEE);
    }

    const std::vector<std::uint8_t> large = TestJpeg(37, 21, 3);
    std::vector<std::uint8_t> wideOutput(37 * 21 * 4);
    JpegDecDecodeParam wideParam = DecodeParam(large, wideOutput, 0);
    Require(sceJpegDecDecode(handle, &wideParam, nullptr) == static_cast<std::int32_t>(0x80650006));
    const std::vector<std::uint8_t> largeGray = TestJpeg(19, 11, 1);
    for (const auto* source : {&large, &largeGray}) {
        const std::optional<Decoder::Jpeg::Header> sourceHeader = Decoder::Jpeg::ParseHeader(*source);
        Require(sourceHeader.has_value());
        for (const std::uint16_t downScale : {std::uint16_t{2}, std::uint16_t{4}, std::uint16_t{8}}) {
            const std::uint32_t width = (sourceHeader->width + downScale - 1) / downScale;
            const std::uint32_t height = (sourceHeader->height + downScale - 1) / downScale;
            const std::optional<Decoder::Jpeg::Image> scaled = Decoder::Jpeg::DecodeScaled(*source, downScale);
            Require(scaled && scaled->width == width && scaled->height == height);

            std::vector<std::uint8_t> exact(static_cast<std::size_t>(width) * height * 4, 0xCC);
            JpegDecDecodeParam scaledParam = DecodeParam(*source, exact, 0);
            scaledParam.down_scale = downScale;
            info = {};
            Require(sceJpegDecDecode(handle, &scaledParam, &info) == static_cast<std::int32_t>(width << 16 | height));
            Require(info.image_width == sourceHeader->width && info.image_height == sourceHeader->height);
            Require(info.output_image_width == width && info.output_image_height == height);
            for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width) * height; ++pixel) {
                for (std::size_t c = 0; c < 3; ++c) Require(exact[pixel * 4 + c] == scaled->pixels[pixel * scaled->channels + (scaled->channels == 1 ? 0 : c)]);
                Require(exact[pixel * 4 + 3] == 0xFF);
            }

            const std::uint32_t paddedPitch = width * 4 + 4;
            std::vector<std::uint8_t> padded(height * paddedPitch, 0xEE);
            JpegDecDecodeParam paddedParam = DecodeParam(*source, padded, paddedPitch);
            paddedParam.down_scale = downScale;
            paddedParam.pixel_format = 1;
            Require(sceJpegDecDecode(handle, &paddedParam, nullptr) == static_cast<std::int32_t>(width << 16 | height));
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    const std::uint8_t* pixel = &padded[y * paddedPitch + x * 4];
                    const std::uint8_t* rgba = &exact[(y * width + x) * 4];
                    Require(pixel[0] == rgba[2] && pixel[1] == rgba[1] && pixel[2] == rgba[0] && pixel[3] == 0xFF);
                }
                if (y + 1 < height) for (std::size_t x = width * 4; x < paddedPitch; ++x) Require(padded[y * paddedPitch + x] == 0xEE);
            }

            std::vector<std::uint8_t> small(exact.size() - 1, 0);
            JpegDecDecodeParam smallParam = DecodeParam(*source, small, 0);
            smallParam.down_scale = downScale;
            Require(sceJpegDecDecode(handle, &smallParam, &info) == invalidSize);
            JpegDecDecodeParam narrowParam = DecodeParam(*source, exact, width * 4 - 1);
            narrowParam.down_scale = downScale;
            Require(sceJpegDecDecode(handle, &narrowParam, &info) == invalidParam);
        }
    }

    const std::vector<std::uint8_t> garbage{0x00, 0x01, 0x02, 0x03};
    const JpegDecParseParam garbageParam{garbage.data(), static_cast<std::uint32_t>(garbage.size()), 0, 1};
    Require(sceJpegDecParseHeader(&garbageParam, &info) == invalidData);
    std::vector<std::uint8_t> garbageOut(64, 0);
    const JpegDecDecodeParam garbageDecode = DecodeParam(garbage, garbageOut, 4 * 4);
    Require(sceJpegDecDecode(handle, &garbageDecode, &info) == invalidData);
    JpegDecDecodeParam garbageScaled = garbageDecode;
    garbageScaled.down_scale = 2;
    Require(sceJpegDecDecode(handle, &garbageScaled, &info) == invalidData);

    Require(sceJpegDecDelete(handle) == 0);
    return 0;
}
