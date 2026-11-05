#include "prx/libScePngEnc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "Decoder/Png.hpp"

namespace {

constexpr int PNG_ENC_ERROR_INVALID_ADDR = static_cast<int>(0x80690101);
constexpr int PNG_ENC_ERROR_INVALID_SIZE = static_cast<int>(0x80690102);
constexpr int PNG_ENC_ERROR_INVALID_PARAM = static_cast<int>(0x80690103);
constexpr int PNG_ENC_ERROR_INVALID_HANDLE = static_cast<int>(0x80690104);
constexpr int PNG_ENC_ERROR_DATA_OVERFLOW = static_cast<int>(0x80690110);
constexpr int PNG_ENC_ERROR_FATAL = static_cast<int>(0x80690120);

constexpr std::uint32_t MAX_IMAGE_WIDTH = 1000000;
constexpr std::uint32_t MAX_IMAGE_HEIGHT = 1000000;
constexpr std::uint32_t MAX_FILTER_NUMBER = 4;
constexpr std::uint16_t COLOR_SPACE_RGB = 3;
constexpr std::uint16_t COLOR_SPACE_RGBA = 19;
constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;
constexpr std::uint16_t FILTER_SUB = 1;
constexpr std::uint16_t FILTER_UP = 2;
constexpr std::uint16_t FILTER_AVERAGE = 4;
constexpr std::uint16_t FILTER_PAETH = 8;
constexpr std::uint16_t FILTER_ALL = FILTER_SUB | FILTER_UP | FILTER_AVERAGE | FILTER_PAETH;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 16;

struct PngEncContext {
    PngEncContext* self;
    std::uint32_t size;
    std::uint32_t reserved;
    std::uint32_t maxImageWidth;
    std::uint32_t maxFilterNumber;
    std::uint32_t scratchSize;
};

static_assert(sizeof(PngEncCreateParam) == 16);
static_assert(sizeof(PngEncEncodeParam) == 48);
static_assert(sizeof(PngEncOutputInfo) == 8);

int validateCreateParam(const PngEncCreateParam* param) {
    if (!param) return PNG_ENC_ERROR_INVALID_ADDR;
    if (param->this_size != sizeof(PngEncCreateParam)) return PNG_ENC_ERROR_INVALID_SIZE;
    if (param->attribute != 0 || param->max_image_width == 0 || param->max_image_width > MAX_IMAGE_WIDTH || param->max_filter_number > MAX_FILTER_NUMBER) return PNG_ENC_ERROR_INVALID_PARAM;
    return 0;
}

std::uint32_t workMemorySize(const PngEncCreateParam& param) {
    return 285808 + ((4 * param.max_image_width + 28) & ~std::uint32_t{31}) * (param.max_filter_number + 2);
}

PngEncContext* context(void* handle) {
    if (reinterpret_cast<std::uintptr_t>(handle) % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* ctx = static_cast<PngEncContext*>(handle);
    return ctx && ctx->self == ctx ? ctx : nullptr;
}

std::uint8_t filterSet(std::uint16_t filterType) {
    if (filterType == FILTER_ALL) return Decoder::Png::FILTER_ALL;
    if (filterType == 0) return Decoder::Png::FILTER_NONE;
    std::uint8_t filters = Decoder::Png::FILTER_NONE;
    if (filterType & FILTER_SUB) filters |= Decoder::Png::FILTER_SUB;
    if (filterType & FILTER_UP) filters |= Decoder::Png::FILTER_UP;
    if (filterType & FILTER_AVERAGE) filters |= Decoder::Png::FILTER_AVERAGE;
    if (filterType & FILTER_PAETH) filters |= Decoder::Png::FILTER_PAETH;
    return filters;
}

}

extern "C" {

int APS5_VABI scePngEncQueryMemorySize(const PngEncCreateParam* param) {
    if (const int result = validateCreateParam(param); result != 0) return result;
    return static_cast<int>(workMemorySize(*param));
}

int APS5_VABI scePngEncCreate(const PngEncCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    if (const int result = validateCreateParam(param); result != 0) return result;
    if (!memoryAddress) return PNG_ENC_ERROR_INVALID_ADDR;
    const std::uint32_t requiredSize = workMemorySize(*param);
    if (memorySize < requiredSize) return PNG_ENC_ERROR_INVALID_SIZE;
    if (!handle) return PNG_ENC_ERROR_INVALID_ADDR;
    const std::uintptr_t aligned = (reinterpret_cast<std::uintptr_t>(memoryAddress) + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* ctx = new (reinterpret_cast<void*>(aligned)) PngEncContext{};
    ctx->self = ctx;
    ctx->size = sizeof(PngEncCreateParam);
    ctx->maxImageWidth = (param->max_image_width + 7) & ~std::uint32_t{7};
    ctx->maxFilterNumber = param->max_filter_number;
    ctx->scratchSize = requiredSize - 112;
    *handle = ctx;
    return 0;
}

int APS5_VABI scePngEncDelete(void* handle) {
    PngEncContext* ctx = context(handle);
    if (!ctx) return PNG_ENC_ERROR_INVALID_HANDLE;
    ctx->self = nullptr;
    return 0;
}

int APS5_VABI scePngEncEncode(void* handle, const PngEncEncodeParam* param, PngEncOutputInfo* outputInfo) {
    const PngEncContext* ctx = context(handle);
    if (!ctx) return PNG_ENC_ERROR_INVALID_HANDLE;
    if (!param) return PNG_ENC_ERROR_INVALID_ADDR;
    if (!param->image_mem_addr || reinterpret_cast<std::uintptr_t>(param->image_mem_addr) % 4 != 0 || !param->png_mem_addr) return PNG_ENC_ERROR_INVALID_ADDR;
    if (param->image_mem_size == 0 || param->png_mem_size == 0) return PNG_ENC_ERROR_INVALID_SIZE;
    const std::uint32_t width = param->image_width;
    const std::uint32_t height = param->image_height;
    const std::uint64_t inputSize = static_cast<std::uint64_t>(param->image_pitch) * height;
    if (width == 0 || width > ctx->maxImageWidth || height == 0 || height > MAX_IMAGE_HEIGHT
        || param->image_pitch == 0 || param->image_pitch % 4 != 0 || param->image_pitch > 0x0FFFFFFF || param->image_pitch / 4 < width
        || inputSize > 0x7FFFFFFF || inputSize > param->image_mem_size) return PNG_ENC_ERROR_INVALID_PARAM;
    if ((param->pixel_format != PIXEL_FORMAT_R8G8B8A8 && param->pixel_format != PIXEL_FORMAT_B8G8R8A8)
        || (param->color_space != COLOR_SPACE_RGB && param->color_space != COLOR_SPACE_RGBA) || param->bit_depth != 8
        || param->clut_number != 0 || (param->filter_type & ~FILTER_ALL) != 0
        || static_cast<std::uint32_t>(std::popcount(param->filter_type)) > ctx->maxFilterNumber || param->compression_level > 9) {
        return PNG_ENC_ERROR_INVALID_PARAM;
    }

    const std::uint32_t channels = param->color_space == COLOR_SPACE_RGBA ? 4 : 3;
    const bool bgr = param->pixel_format == PIXEL_FORMAT_B8G8R8A8;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> png;
    try {
        pixels.resize(static_cast<std::size_t>(width) * height * channels);
        std::uint8_t* destination = pixels.data();
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint8_t* source = param->image_mem_addr + static_cast<std::size_t>(y) * param->image_pitch;
            for (std::uint32_t x = 0; x < width; ++x, source += 4) {
                *destination++ = source[bgr ? 2 : 0];
                *destination++ = source[1];
                *destination++ = source[bgr ? 0 : 2];
                if (channels == 4) *destination++ = source[3];
            }
        }
        png = Decoder::Png::Encode(pixels, width, height, channels, {param->compression_level, filterSet(param->filter_type)});
    } catch (const std::exception&) {
        return PNG_ENC_ERROR_FATAL;
    }

    const bool overflow = png.size() > param->png_mem_size;
    if (outputInfo) *outputInfo = {overflow ? 0 : static_cast<std::uint32_t>(png.size()), overflow ? 0 : height};
    if (overflow) return PNG_ENC_ERROR_DATA_OVERFLOW;
    std::copy(png.begin(), png.end(), param->png_mem_addr);
    return static_cast<int>(png.size());
}

}
