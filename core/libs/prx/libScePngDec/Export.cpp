#include "prx/libScePngDec/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include "Decoder/Png.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(PngDecCreateParam) == 0xC);
static_assert(sizeof(PngDecParseParam) == 0x10);
static_assert(sizeof(PngDecDecodeParam) == 0x20);
static_assert(sizeof(PngDecImageInfo) == 0x10);

namespace {

constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80690001);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80690002);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80690003);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80690004);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY = static_cast<std::int32_t>(0x80690005);
constexpr std::int32_t SCE_PNG_DEC_ERROR_INVALID_DATA = static_cast<std::int32_t>(0x80690010);
constexpr std::int32_t SCE_PNG_DEC_ERROR_DECODE_ERROR = static_cast<std::int32_t>(0x80690012);

constexpr std::uint32_t ATTRIBUTE_NONE = 0;
constexpr std::uint32_t ATTRIBUTE_BIT_DEPTH_16 = 1;
constexpr std::uint32_t MAX_IMAGE_WIDTH = 1000000;
constexpr std::uint32_t WORK_MEMORY_OVERHEAD = 53392;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 16;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;

constexpr std::uint16_t COLOR_SPACE_GRAYSCALE = 2;
constexpr std::uint16_t COLOR_SPACE_RGB = 3;
constexpr std::uint16_t COLOR_SPACE_CLUT = 4;
constexpr std::uint16_t COLOR_SPACE_GRAYSCALE_ALPHA = 18;
constexpr std::uint16_t COLOR_SPACE_RGBA = 19;

constexpr std::uint32_t IMAGE_FLAG_ADAM7_INTERLACE = 1;
constexpr std::uint32_t IMAGE_FLAG_TRNS_CHUNK_EXIST = 2;

constexpr std::uint32_t BYTES_PER_PIXEL = 4;
constexpr std::uint32_t MAX_PACKED_DIMENSION = 32767;

struct Context {
    Context* self;
    std::uint32_t size;
    std::uint32_t attribute;
    std::uint32_t maxImageWidth;
    std::uint32_t memorySize;
};

static_assert(sizeof(Context) == 24);

std::uint32_t requiredMemory(const PngDecCreateParam& param) {
    return (((param.max_image_width + 7u) & ~7u) + 64u) * (8u << param.attribute) + WORK_MEMORY_OVERHEAD;
}

std::int32_t validateCreateParam(const PngDecCreateParam* param) {
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (param->this_size != sizeof(PngDecCreateParam)) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (param->attribute != ATTRIBUTE_NONE && param->attribute != ATTRIBUTE_BIT_DEPTH_16) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (param->max_image_width - 1 >= MAX_IMAGE_WIDTH) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    return 0;
}

Context* toContext(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* context = reinterpret_cast<Context*>(handle);
    return context->self == context ? context : nullptr;
}

std::uint16_t toColorSpace(Decoder::Png::ColorType colorType) {
    switch (colorType) {
    case Decoder::Png::ColorType::Grayscale:
        return COLOR_SPACE_GRAYSCALE;
    case Decoder::Png::ColorType::Rgb:
        return COLOR_SPACE_RGB;
    case Decoder::Png::ColorType::Palette:
        return COLOR_SPACE_CLUT;
    case Decoder::Png::ColorType::GrayscaleAlpha:
        return COLOR_SPACE_GRAYSCALE_ALPHA;
    case Decoder::Png::ColorType::Rgba:
        return COLOR_SPACE_RGBA;
    }
    throw std::runtime_error("libScePngDec: unknown PNG color type");
}

void fillImageInfo(const Decoder::Png::Header& header, PngDecImageInfo* info) {
    info->image_width = header.width;
    info->image_height = header.height;
    info->color_space = toColorSpace(header.colorType);
    info->bit_depth = header.bitDepth;
    info->image_flag = (header.interlaced ? IMAGE_FLAG_ADAM7_INTERLACE : 0) | (header.hasTransparency ? IMAGE_FLAG_TRNS_CHUNK_EXIST : 0);
}

bool hasAlpha(const Decoder::Png::Header& header) {
    return header.hasTransparency || header.colorType == Decoder::Png::ColorType::GrayscaleAlpha
        || header.colorType == Decoder::Png::ColorType::Rgba;
}

std::span<const std::uint8_t> toBytes(const void* data, std::uint32_t size) {
    return {static_cast<const std::uint8_t*>(data), size};
}

} // namespace

extern "C" {

int32_t APS5_VABI scePngDecCreate(const PngDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    if (!memoryAddress) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (memorySize < requiredMemory(*param)) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memoryAddress);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* context = new (reinterpret_cast<void*>(aligned)) Context;
    context->self = context;
    context->size = sizeof(PngDecCreateParam);
    context->attribute = param->attribute;
    context->maxImageWidth = (param->max_image_width + 7u) & ~7u;
    context->memorySize = requiredMemory(*param) - 144u;
    *handle = context;
    return 0;
}

int32_t APS5_VABI scePngDecDecode(void* handle, const PngDecDecodeParam* param, PngDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_PNG_DEC_ERROR_INVALID_HANDLE;
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (!param->png_mem_addr || !param->image_mem_addr || (reinterpret_cast<std::uintptr_t>(param->image_mem_addr) & 3u) != 0) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (param->png_mem_size == 0 || param->image_mem_size == 0) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (param->pixel_format == 132) throw std::runtime_error("scePngDecDecode: pixel format 132 is not implemented");
    if (param->pixel_format != PIXEL_FORMAT_R8G8B8A8 && param->pixel_format != PIXEL_FORMAT_B8G8R8A8) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (param->alpha_value > 0xFF) return SCE_PNG_DEC_ERROR_INVALID_PARAM;

    const std::span<const std::uint8_t> png = toBytes(param->png_mem_addr, param->png_mem_size);
    const std::optional<Decoder::Png::Header> header = Decoder::Png::ParseHeader(png);
    if (!header) return SCE_PNG_DEC_ERROR_INVALID_DATA;
    if (imageInfo) fillImageInfo(*header, imageInfo);
    const std::uint64_t rowSize = static_cast<std::uint64_t>(header->width) * BYTES_PER_PIXEL;
    const std::uint64_t pitch = param->image_pitch == 0 ? rowSize : param->image_pitch;
    if (param->image_pitch != 0 && (pitch < rowSize || (pitch & 3u) != 0)) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (param->image_pitch == 0) {
        if (header->height * rowSize > param->image_mem_size) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    } else if (header->height * pitch >= 0x80000000ull || header->height * pitch > param->image_mem_size) {
        return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    }
    const std::uint64_t alignedWidth = (static_cast<std::uint64_t>(header->width) + 7u) & ~std::uint64_t{7u};
    const std::uint64_t alignedMaxWidth = (static_cast<std::uint64_t>(context->maxImageWidth) + 7u) & ~std::uint64_t{7u};
    if (alignedWidth > alignedMaxWidth) return SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY;
    if (header->bitDepth > (8u << (context->attribute & 31u))) return SCE_PNG_DEC_ERROR_INVALID_WORK_MEMORY;

    const std::optional<Decoder::Png::Image> image = Decoder::Png::Decode(png);
    if (!image || image->width != header->width || image->height != header->height) return SCE_PNG_DEC_ERROR_DECODE_ERROR;

    const bool swapRedBlue = param->pixel_format == PIXEL_FORMAT_B8G8R8A8;
    const bool fillAlpha = !hasAlpha(*header);
    const auto alpha = static_cast<std::uint8_t>(param->alpha_value);
    auto* output = static_cast<std::uint8_t*>(param->image_mem_addr);
    for (std::uint32_t y = 0; y < image->height; ++y) {
        const std::uint8_t* in = image->pixels.data() + y * rowSize;
        std::uint8_t* out = output + y * pitch;
        std::memcpy(out, in, rowSize);
        for (std::uint32_t x = 0; x < image->width; ++x) {
            std::uint8_t* pixel = out + x * BYTES_PER_PIXEL;
            if (swapRedBlue) std::swap(pixel[0], pixel[2]);
            if (fillAlpha) pixel[3] = alpha;
        }
    }

    if (header->width > MAX_PACKED_DIMENSION || header->height > MAX_PACKED_DIMENSION) return 0;
    return static_cast<std::int32_t>(header->width << 16 | header->height);
}

int32_t APS5_VABI scePngDecDelete(void* handle) {
    Context* context = toContext(handle);
    if (!context) return SCE_PNG_DEC_ERROR_INVALID_HANDLE;
    context->self = nullptr;
    return 0;
}

int32_t APS5_VABI scePngDecParseHeader(const PngDecParseParam* param, PngDecImageInfo* imageInfo) {
    if (!param) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (!param->png_mem_addr) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    if (param->png_mem_size == 0) return SCE_PNG_DEC_ERROR_INVALID_SIZE;
    if (param->reserved0 != 0) return SCE_PNG_DEC_ERROR_INVALID_PARAM;
    if (!imageInfo) return SCE_PNG_DEC_ERROR_INVALID_ADDR;
    const std::optional<Decoder::Png::Header> header = Decoder::Png::ParseHeader(toBytes(param->png_mem_addr, param->png_mem_size));
    if (!header) return SCE_PNG_DEC_ERROR_INVALID_DATA;
    fillImageInfo(*header, imageInfo);
    return 0;
}

int32_t APS5_VABI scePngDecQueryMemorySize(const PngDecCreateParam* param) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    return static_cast<std::int32_t>(requiredMemory(*param));
}

int APS5_VABI scePngDecDecodeWithInputControl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
