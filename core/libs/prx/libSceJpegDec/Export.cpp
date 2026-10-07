#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"

static_assert(sizeof(JpegDecCreateParam) == 0x8);
static_assert(sizeof(JpegDecParseParam) == 0x10);
static_assert(sizeof(JpegDecDecodeParam) == 0x20);
static_assert(sizeof(JpegDecImageInfo) == 0x10);

namespace {

constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80690001);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80690002);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80690003);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80690004);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_WORK_MEMORY = static_cast<std::int32_t>(0x80690005);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_DATA = static_cast<std::int32_t>(0x80690010);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_DECODE_ERROR = static_cast<std::int32_t>(0x80690012);

constexpr std::uint32_t ATTRIBUTE_NONE = 0;
constexpr std::uint32_t MEMORY_SIZE = 0x20;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 8;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;

constexpr std::uint16_t COLOR_SPACE_RGB = 3;

constexpr std::uint32_t BYTES_PER_PIXEL = 4;
constexpr std::uint32_t MAX_PACKED_DIMENSION = 32767;

struct Context {
    Context* self;
    std::uint32_t attribute;
};

static_assert(sizeof(Context) + HANDLE_ALIGNMENT - 1 <= MEMORY_SIZE);

std::int32_t validateCreateParam(const JpegDecCreateParam* param) {
    if (!param)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->size != sizeof(JpegDecCreateParam))
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (param->attribute != ATTRIBUTE_NONE)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    return 0;
}

Context* toContext(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0)
        return nullptr;
    auto* context = reinterpret_cast<Context*>(handle);
    return context->self == context ? context : nullptr;
}

void fillImageInfo(const Decoder::Jpeg::Header& header, JpegDecImageInfo* info) {
    info->image_width = header.width;
    info->image_height = header.height;
    info->color_space = COLOR_SPACE_RGB;
    info->bit_depth = 8;
    info->image_flag = 0;
}

std::span<const std::uint8_t> toBytes(const void* data, std::uint32_t size) {
    return {static_cast<const std::uint8_t*>(data), size};
}

} // namespace

extern "C" {

int32_t APS5_VABI sceJpegDecCreate(const JpegDecCreateParam* param, void* memory_address, uint32_t memory_size,
                                   void** handle) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0)
        return result;
    if (!memory_address || !handle)
        return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (memory_size < MEMORY_SIZE)
        return SCE_JPEG_DEC_ERROR_INVALID_WORK_MEMORY;
    const auto address = reinterpret_cast<std::uintptr_t>(memory_address);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* context = new (reinterpret_cast<void*>(aligned)) Context{};
    context->self = context;
    context->attribute = param->attribute;
    *handle = context;
    return 0;
}

int32_t APS5_VABI sceJpegDecDecode(void* handle, const JpegDecDecodeParam* param, JpegDecImageInfo* image_info) {
    const Context* context = toContext(handle);
    if (!context)
        return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    (void)context;
    if (!param)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (!param->jpeg_mem_addr || !param->image_mem_addr)
        return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0 || param->image_mem_size == 0)
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (param->pixel_format != PIXEL_FORMAT_R8G8B8A8 && param->pixel_format != PIXEL_FORMAT_B8G8R8A8)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;

    const std::span<const std::uint8_t> jpeg = toBytes(param->jpeg_mem_addr, param->jpeg_mem_size);
    const std::optional<Decoder::Jpeg::Header> header = Decoder::Jpeg::ParseHeader(jpeg);
    if (!header)
        return SCE_JPEG_DEC_ERROR_INVALID_DATA;

    const std::uint64_t rowSize = static_cast<std::uint64_t>(header->width) * BYTES_PER_PIXEL;
    const std::uint64_t pitch = param->image_pitch == 0 ? rowSize : param->image_pitch;
    if (pitch < rowSize)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if ((header->height - 1) * pitch + rowSize > param->image_mem_size)
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;

    const std::optional<Decoder::Jpeg::Image> image = Decoder::Jpeg::Decode(jpeg);
    if (!image || image->width != header->width || image->height != header->height)
        return SCE_JPEG_DEC_ERROR_DECODE_ERROR;

    const bool swapRedBlue = param->pixel_format == PIXEL_FORMAT_B8G8R8A8;
    auto* output = static_cast<std::uint8_t*>(param->image_mem_addr);
    for (std::uint32_t y = 0; y < image->height; ++y) {
        std::uint8_t* out = output + y * pitch;
        for (std::uint32_t x = 0; x < image->width; ++x) {
            const std::uint8_t* in =
                image->pixels.data() + (static_cast<std::size_t>(y) * image->width + x) * image->channels;
            std::uint8_t* pixel = out + x * BYTES_PER_PIXEL;
            if (image->channels == 1) {
                pixel[0] = in[0];
                pixel[1] = in[0];
                pixel[2] = in[0];
            } else {
                pixel[0] = in[0];
                pixel[1] = in[1];
                pixel[2] = in[2];
            }
            if (swapRedBlue)
                std::swap(pixel[0], pixel[2]);
            pixel[3] = 0xFF;
        }
    }

    if (image_info)
        fillImageInfo(*header, image_info);
    if (header->width > MAX_PACKED_DIMENSION || header->height > MAX_PACKED_DIMENSION)
        return 0;
    return static_cast<std::int32_t>(header->width << 16 | header->height);
}

int32_t APS5_VABI sceJpegDecDelete(void* handle) {
    Context* context = toContext(handle);
    if (!context)
        return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    context->self = nullptr;
    return 0;
}

int32_t APS5_VABI sceJpegDecParseHeader(const JpegDecParseParam* param, JpegDecImageInfo* image_info) {
    if (!param)
        return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (!param->jpeg_mem_addr || !image_info)
        return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0)
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    const std::optional<Decoder::Jpeg::Header> header =
        Decoder::Jpeg::ParseHeader(toBytes(param->jpeg_mem_addr, param->jpeg_mem_size));
    if (!header)
        return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    fillImageInfo(*header, image_info);
    return 0;
}

int32_t APS5_VABI sceJpegDecQueryMemorySize(const JpegDecCreateParam* param) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0)
        return result;
    return static_cast<std::int32_t>(MEMORY_SIZE);
}
}
