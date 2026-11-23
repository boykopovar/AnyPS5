#include "prx/libScePngEnc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
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

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

bool CopyIn(void* dst, const void* src, std::size_t size) {
    return GuestCopyIn_nid_no_patch(dst, src, size);
}

bool CopyOut(void* dst, const void* src, std::size_t size) {
    return GuestCopyOut_nid_no_patch(dst, src, size);
}

bool CanCopyOut(void* dst, std::size_t size) {
    if (size == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(dst);
    if (start == 0 || size > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    const GuestArena::HostWrite guard(dst, size);
    return guard.Open() && GuestWritable_nid_no_patch(dst, size);
}

int validateCreateParam(const PngEncCreateParam* param, PngEncCreateParam* outParam = nullptr) {
    if (!param) return PNG_ENC_ERROR_INVALID_ADDR;
    PngEncCreateParam local{};
    if (!CopyIn(&local, param, sizeof(local))) return PNG_ENC_ERROR_INVALID_ADDR;
    if (local.this_size != sizeof(PngEncCreateParam)) return PNG_ENC_ERROR_INVALID_SIZE;
    if (local.attribute != 0 || local.max_image_width == 0 || local.max_image_width > MAX_IMAGE_WIDTH || local.max_filter_number > MAX_FILTER_NUMBER) return PNG_ENC_ERROR_INVALID_PARAM;
    if (outParam) *outParam = local;
    return 0;
}

std::uint32_t workMemorySize(const PngEncCreateParam& param) {
    return 285808 + ((4 * param.max_image_width + 28) & ~std::uint32_t{31}) * (param.max_filter_number + 2);
}

std::optional<PngEncContext> context(void* handle) {
    if (reinterpret_cast<std::uintptr_t>(handle) == 0 || reinterpret_cast<std::uintptr_t>(handle) % HANDLE_ALIGNMENT != 0) return std::nullopt;
    PngEncContext localCtx{};
    if (!CopyIn(&localCtx, handle, sizeof(localCtx))) return std::nullopt;
    if (localCtx.self != handle) return std::nullopt;
    return localCtx;
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
    const ErrnoGuard errnoGuard;
    PngEncCreateParam local{};
    if (const int result = validateCreateParam(param, &local); result != 0) return result;
    return static_cast<int>(workMemorySize(local));
}

int APS5_VABI scePngEncCreate(const PngEncCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    const ErrnoGuard errnoGuard;
    PngEncCreateParam local{};
    if (const int result = validateCreateParam(param, &local); result != 0) return result;
    if (!memoryAddress) return PNG_ENC_ERROR_INVALID_ADDR;
    const std::uint32_t requiredSize = workMemorySize(local);
    if (memorySize < requiredSize) return PNG_ENC_ERROR_INVALID_SIZE;
    if (!handle) return PNG_ENC_ERROR_INVALID_ADDR;
    const std::uintptr_t aligned = (reinterpret_cast<std::uintptr_t>(memoryAddress) + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    if (!CanCopyOut(handle, sizeof(void*))) {
        return PNG_ENC_ERROR_INVALID_ADDR;
    }
    PngEncContext localCtx{};
    localCtx.self = reinterpret_cast<PngEncContext*>(aligned);
    localCtx.size = sizeof(PngEncCreateParam);
    localCtx.maxImageWidth = (local.max_image_width + 7) & ~std::uint32_t{7};
    localCtx.maxFilterNumber = local.max_filter_number;
    localCtx.scratchSize = requiredSize - 112;
    if (!CopyOut(reinterpret_cast<void*>(aligned), &localCtx, sizeof(localCtx))) {
        return PNG_ENC_ERROR_INVALID_ADDR;
    }
    auto* ctx = reinterpret_cast<PngEncContext*>(aligned);
    if (!CopyOut(handle, &ctx, sizeof(ctx))) {
        PngEncContext cleared{};
        CopyOut(reinterpret_cast<void*>(aligned), &cleared, sizeof(cleared));
        return PNG_ENC_ERROR_INVALID_ADDR;
    }
    return 0;
}

int APS5_VABI scePngEncDelete(void* handle) {
    const ErrnoGuard errnoGuard;
    const std::optional<PngEncContext> ctx = context(handle);
    if (!ctx) return PNG_ENC_ERROR_INVALID_HANDLE;
    PngEncContext cleared{};
    if (!CopyOut(handle, &cleared, sizeof(cleared))) return PNG_ENC_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI scePngEncEncode(void* handle, const PngEncEncodeParam* param, PngEncOutputInfo* outputInfo) {
    const ErrnoGuard errnoGuard;
    const std::optional<PngEncContext> ctx = context(handle);
    if (!ctx) return PNG_ENC_ERROR_INVALID_HANDLE;
    if (!param) return PNG_ENC_ERROR_INVALID_ADDR;
    PngEncEncodeParam localParam{};
    if (!CopyIn(&localParam, param, sizeof(localParam))) return PNG_ENC_ERROR_INVALID_ADDR;
    if (!localParam.image_mem_addr || reinterpret_cast<std::uintptr_t>(localParam.image_mem_addr) % 4 != 0 || !localParam.png_mem_addr) return PNG_ENC_ERROR_INVALID_ADDR;
    if (localParam.image_mem_size == 0 || localParam.png_mem_size == 0) return PNG_ENC_ERROR_INVALID_SIZE;
    const std::uint32_t width = localParam.image_width;
    const std::uint32_t height = localParam.image_height;
    const std::uint64_t inputSize = static_cast<std::uint64_t>(localParam.image_pitch) * height;
    if (width == 0 || width > ctx->maxImageWidth || height == 0 || height > MAX_IMAGE_HEIGHT
        || localParam.image_pitch == 0 || localParam.image_pitch % 4 != 0 || localParam.image_pitch > 0x0FFFFFFF || localParam.image_pitch / 4 < width
        || inputSize > 0x7FFFFFFF || inputSize > localParam.image_mem_size) return PNG_ENC_ERROR_INVALID_PARAM;
    if ((localParam.pixel_format != PIXEL_FORMAT_R8G8B8A8 && localParam.pixel_format != PIXEL_FORMAT_B8G8R8A8)
        || (localParam.color_space != COLOR_SPACE_RGB && localParam.color_space != COLOR_SPACE_RGBA) || localParam.bit_depth != 8
        || localParam.clut_number != 0 || (localParam.filter_type & ~FILTER_ALL) != 0
        || static_cast<std::uint32_t>(std::popcount(localParam.filter_type)) > ctx->maxFilterNumber || localParam.compression_level > 9) {
        return PNG_ENC_ERROR_INVALID_PARAM;
    }

    const std::uint32_t channels = localParam.color_space == COLOR_SPACE_RGBA ? 4 : 3;
    const bool bgr = localParam.pixel_format == PIXEL_FORMAT_B8G8R8A8;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> png;
    try {
        pixels.resize(static_cast<std::size_t>(width) * height * channels);
        std::uint8_t* destination = pixels.data();
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        std::vector<std::uint8_t> rowBuffer(rowBytes);
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint8_t* sourceRow = localParam.image_mem_addr + static_cast<std::size_t>(y) * localParam.image_pitch;
            if (!CopyIn(rowBuffer.data(), sourceRow, rowBytes)) return PNG_ENC_ERROR_INVALID_ADDR;
            const std::uint8_t* source = rowBuffer.data();
            for (std::uint32_t x = 0; x < width; ++x, source += 4) {
                *destination++ = source[bgr ? 2 : 0];
                *destination++ = source[1];
                *destination++ = source[bgr ? 0 : 2];
                if (channels == 4) *destination++ = source[3];
            }
        }
        png = Decoder::Png::Encode(pixels, width, height, channels, {localParam.compression_level, filterSet(localParam.filter_type)});
    } catch (const std::exception&) {
        return PNG_ENC_ERROR_FATAL;
    }

    const bool overflow = png.size() > localParam.png_mem_size;
    if (outputInfo && !CanCopyOut(outputInfo, sizeof(*outputInfo))) {
        return PNG_ENC_ERROR_INVALID_ADDR;
    }
    if (!overflow && !CanCopyOut(localParam.png_mem_addr, png.size())) {
        return PNG_ENC_ERROR_INVALID_ADDR;
    }
    if (overflow) {
        if (outputInfo) {
            PngEncOutputInfo localOutput{0, 0};
            if (!CopyOut(outputInfo, &localOutput, sizeof(localOutput))) return PNG_ENC_ERROR_INVALID_ADDR;
        }
        return PNG_ENC_ERROR_DATA_OVERFLOW;
    }
    if (!CopyOut(localParam.png_mem_addr, png.data(), png.size())) return PNG_ENC_ERROR_INVALID_ADDR;
    if (outputInfo) {
        PngEncOutputInfo localOutput{static_cast<std::uint32_t>(png.size()), height};
        if (!CopyOut(outputInfo, &localOutput, sizeof(localOutput))) return PNG_ENC_ERROR_INVALID_ADDR;
    }
    return static_cast<int>(png.size());
}

}
