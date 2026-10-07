#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
std::int32_t APS5_VABI sceJpegDecQueryMemorySize(const JpegDecCreateParam*);
std::int32_t APS5_VABI sceJpegDecCreate(const JpegDecCreateParam*, void*, std::uint32_t, void**);
std::int32_t APS5_VABI sceJpegDecDelete(void*);
std::int32_t APS5_VABI sceJpegDecParseHeader(const JpegDecParseParam*, JpegDecImageInfo*);
std::int32_t APS5_VABI sceJpegDecDecode(void*, const JpegDecDecodeParam*, JpegDecImageInfo*);
}

static void Require(bool value) {
    if (!value)
        std::abort();
}

static std::vector<std::uint8_t> TestJpeg(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i)
        rgb[i] = static_cast<std::uint8_t>(10 + i * 13);
    return Decoder::Jpeg::Encode(rgb, width, height, 3, 90);
}

static JpegDecDecodeParam DecodeParam(const std::vector<std::uint8_t>& jpeg, std::vector<std::uint8_t>& image) {
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.image_mem_addr = image.data();
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.pixel_format = 0;
    param.reserved0 = 0;
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

    const JpegDecCreateParam param{sizeof(JpegDecCreateParam), 0};
    const std::int32_t memorySize = sceJpegDecQueryMemorySize(&param);
    Require(memorySize > 0);
    Require(sceJpegDecQueryMemorySize(nullptr) == invalidParam);
    const JpegDecCreateParam badSize{sizeof(JpegDecCreateParam) - 1, 0};
    Require(sceJpegDecQueryMemorySize(&badSize) == invalidSize);
    const JpegDecCreateParam badAttribute{sizeof(JpegDecCreateParam), 1};
    Require(sceJpegDecQueryMemorySize(&badAttribute) == invalidParam);

    std::vector<std::uint8_t> memory(static_cast<std::size_t>(memorySize) + 8);
    void* handle = nullptr;
    Require(sceJpegDecCreate(&param, memory.data(), 0, &handle) == invalidWorkMemory);
    Require(sceJpegDecCreate(&param, memory.data(), static_cast<std::uint32_t>(memorySize), nullptr) == invalidAddr);
    Require(sceJpegDecCreate(&param, memory.data(), static_cast<std::uint32_t>(memorySize), &handle) == 0);
    Require(handle != nullptr);
    Require(sceJpegDecDelete(nullptr) == invalidHandle);
    Require(sceJpegDecDelete(handle) == 0);

    Require(sceJpegDecCreate(&param, memory.data(), static_cast<std::uint32_t>(memorySize), &handle) == 0);

    const std::vector<std::uint8_t> jpeg = TestJpeg(7, 5);
    JpegDecImageInfo info{};
    const JpegDecParseParam parseParam{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 0};
    Require(sceJpegDecParseHeader(nullptr, &info) == invalidParam);
    Require(sceJpegDecParseHeader(&parseParam, nullptr) == invalidAddr);
    const JpegDecParseParam emptyParam{nullptr, 0, 0};
    Require(sceJpegDecParseHeader(&emptyParam, &info) == invalidAddr);
    Require(sceJpegDecParseHeader(&parseParam, &info) == 0);
    Require(info.image_width == 7);
    Require(info.image_height == 5);

    std::vector<std::uint8_t> image(7 * 5 * 4, 0xCC);
    JpegDecDecodeParam decodeParam = DecodeParam(jpeg, image);
    Require(sceJpegDecDecode(nullptr, &decodeParam, &info) == invalidHandle);
    Require(sceJpegDecDecode(handle, nullptr, &info) == invalidParam);
    JpegDecDecodeParam badFormat = decodeParam;
    badFormat.pixel_format = 2;
    Require(sceJpegDecDecode(handle, &badFormat, &info) == invalidParam);
    const std::int32_t packed = sceJpegDecDecode(handle, &decodeParam, &info);
    Require(packed == static_cast<std::int32_t>(7 << 16 | 5));
    Require(info.image_width == 7);
    Require(info.image_height == 5);
    Require(image[3] == 0xFF);

    std::vector<std::uint8_t> bgra(7 * 5 * 4, 0);
    JpegDecDecodeParam bgraParam = DecodeParam(jpeg, bgra);
    bgraParam.pixel_format = 1;
    Require(sceJpegDecDecode(handle, &bgraParam, nullptr) == static_cast<std::int32_t>(7 << 16 | 5));
    Require(bgra[0] == image[2]);
    Require(bgra[2] == image[0]);

    const std::vector<std::uint8_t> garbage{0x00, 0x01, 0x02, 0x03};
    JpegDecParseParam garbageParam{garbage.data(), static_cast<std::uint32_t>(garbage.size()), 0};
    Require(sceJpegDecParseHeader(&garbageParam, &info) == invalidData);
    std::vector<std::uint8_t> garbageOut(64, 0);
    JpegDecDecodeParam garbageDecode = DecodeParam(garbage, garbageOut);
    Require(sceJpegDecDecode(handle, &garbageDecode, &info) == invalidData);

    Require(sceJpegDecDelete(handle) == 0);
    return 0;
}
