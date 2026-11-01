#include "prx/libSceJpegDec/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(JpegDecCreateParam) == 0xC);
static_assert(sizeof(JpegDecParseParam) == 0x10);
static_assert(sizeof(JpegDecDecodeParam) == 0x30);
static_assert(sizeof(JpegDecImageInfo) == 0x24);

namespace {

constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80650001);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80650002);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80650003);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_COEFFICIENT_MEMORY = static_cast<std::int32_t>(0x80650005);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80650004);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_IMAGE_TOO_WIDE = static_cast<std::int32_t>(0x80650006);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_DATA = static_cast<std::int32_t>(0x80650010);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_DECODE_ERROR = static_cast<std::int32_t>(0x80650012);

constexpr std::uint32_t ATTRIBUTE_CSC_ORDINARY = 1;
constexpr std::uint32_t ATTRIBUTE_CSC_FULL = 2;
constexpr std::uint32_t MEMORY_OVERHEAD = 29536;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 32;

constexpr std::uint16_t DECODE_MODE_NORMAL = 0;
constexpr std::uint16_t DOWN_SCALE_NONE = 1;
constexpr std::uint16_t DOWN_SCALE_HALF = 2;
constexpr std::uint16_t DOWN_SCALE_QUARTER = 4;
constexpr std::uint16_t DOWN_SCALE_EIGHTH = 8;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;

constexpr std::uint16_t COLOR_SPACE_UNKNOWN = 0;
constexpr std::uint16_t COLOR_SPACE_YCBCR = 1;
constexpr std::uint16_t COLOR_SPACE_GRAY = 2;

constexpr std::uint32_t BYTES_PER_PIXEL = 4;
constexpr std::uint32_t MAX_PACKED_DIMENSION = 32767;

struct Context {
    Context* self;
    std::uint32_t attribute;
    std::uint32_t maxImageWidth;
    std::uint32_t rowBufferCapacity;
};

static_assert(sizeof(Context) + HANDLE_ALIGNMENT - 1 <= MEMORY_OVERHEAD);

std::int32_t validateCreateParam(const JpegDecCreateParam* param) {
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->this_size != sizeof(JpegDecCreateParam)) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (param->attribute != ATTRIBUTE_CSC_ORDINARY && param->attribute != ATTRIBUTE_CSC_FULL) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->max_image_width == 0 || param->max_image_width > 65535) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    return 0;
}

std::uint32_t workMemorySize(const JpegDecCreateParam& param) {
    const std::uint32_t alignedWidth = (param.max_image_width + 31) & ~std::uint32_t{31};
    return MEMORY_OVERHEAD + (alignedWidth + 16) * (param.attribute == ATTRIBUTE_CSC_ORDINARY ? 54 : 166);
}

bool validDecodeOptions(std::uint16_t decodeMode, std::uint16_t downScale) {
    return decodeMode <= 1 && (downScale == DOWN_SCALE_NONE || downScale == DOWN_SCALE_HALF || downScale == DOWN_SCALE_QUARTER || downScale == DOWN_SCALE_EIGHTH);
}

void requireDecodeMode(const char* function, std::uint16_t decodeMode) {
    if (decodeMode != DECODE_MODE_NORMAL) throw std::runtime_error(std::string(function) + ": decode mode " + std::to_string(decodeMode) + " is not implemented");
}

Context* toContext(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* context = reinterpret_cast<Context*>(handle);
    return context->self == context ? context : nullptr;
}

bool fillImageInfo(const Decoder::Jpeg::Header& header, std::uint32_t downScale, JpegDecImageInfo* info) {
    *info = {};
    info->image_width = header.width;
    info->image_height = header.height;
    info->color_space = COLOR_SPACE_UNKNOWN;
    if (header.channels == 1) info->color_space = COLOR_SPACE_GRAY;
    if (header.channels == 3 && (header.app0BeforeFrame || (header.componentIds[0] == 1 && header.componentIds[1] == 2 && header.componentIds[2] == 3))) info->color_space = COLOR_SPACE_YCBCR;
    info->component_number = static_cast<std::uint16_t>(header.channels);
    std::uint32_t maxHorizontal = 1;
    std::uint32_t maxVertical = 1;
    std::uint32_t blocksPerMcu = 0;
    for (std::uint32_t i = 0; i < header.channels; ++i) {
        info->component_id[i] = header.componentIds[i];
        info->sampling_factor[i] = header.samplingFactors[i];
        const std::uint32_t horizontal = header.channels == 1 ? 1 : header.samplingFactors[i] >> 4;
        const std::uint32_t vertical = header.channels == 1 ? 1 : header.samplingFactors[i] & 15;
        if (horizontal > maxHorizontal) maxHorizontal = horizontal;
        if (vertical > maxVertical) maxVertical = vertical;
        blocksPerMcu += horizontal * vertical;
    }
    if (info->color_space == COLOR_SPACE_GRAY) info->suitable_csc_attribute = ATTRIBUTE_CSC_ORDINARY;
    if (info->color_space == COLOR_SPACE_YCBCR) info->suitable_csc_attribute = (12 / maxHorizontal) * blocksPerMcu < 37 ? ATTRIBUTE_CSC_ORDINARY : ATTRIBUTE_CSC_FULL;
    if (header.progressive) {
        const std::uint32_t mcuColumns = (header.width + 8 * maxHorizontal - 1) / (8 * maxHorizontal);
        const std::uint32_t mcuRows = (header.height + 8 * maxVertical - 1) / (8 * maxVertical);
        const std::uint64_t coefficientSize = std::uint64_t{mcuColumns} * mcuRows * blocksPerMcu * 128 + 64;
        if (coefficientSize > UINT32_MAX) return false;
        info->coefficient_mem_size = static_cast<std::uint32_t>(coefficientSize);
    }
    info->output_image_width = Decoder::Jpeg::ScaledDimension(header.width, downScale);
    info->output_image_height = Decoder::Jpeg::ScaledDimension(header.height, downScale);
    return true;
}

std::uint32_t rowBufferSize(const Decoder::Jpeg::Header& header, std::uint32_t downScale) {
    std::uint32_t maxHorizontal = 1;
    if (header.channels != 1) {
        for (std::uint32_t i = 0; i < header.channels; ++i) {
            const std::uint32_t horizontal = header.samplingFactors[i] >> 4;
            if (horizontal > maxHorizontal) maxHorizontal = horizontal;
        }
    }
    const std::uint32_t mcuColumns = (header.width + 8 * maxHorizontal - 1) / (8 * maxHorizontal);
    const std::uint32_t blockSize = 8 / downScale;
    std::uint32_t size = 0;
    for (std::uint32_t i = 0; i < header.channels; ++i) {
        const std::uint32_t horizontal = header.channels == 1 ? 1 : header.samplingFactors[i] >> 4;
        const std::uint32_t vertical = header.channels == 1 ? 1 : header.samplingFactors[i] & 15;
        const std::uint32_t stride = (mcuColumns * blockSize * horizontal + 7) & ~std::uint32_t{7};
        size += stride * vertical * blockSize;
    }
    return (size + 31) & ~std::uint32_t{31};
}

std::span<const std::uint8_t> toBytes(const void* data, std::uint32_t size) {
    return {static_cast<const std::uint8_t*>(data), size};
}

}  // namespace

extern "C" {

int32_t APS5_VABI sceJpegDecCreate(const JpegDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    if (!memoryAddress) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (memorySize < workMemorySize(*param)) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memoryAddress);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* context = new (reinterpret_cast<void*>(aligned)) Context{};
    context->self = context;
    context->attribute = param->attribute;
    context->maxImageWidth = (param->max_image_width + 31) & ~std::uint32_t{31};
    context->rowBufferCapacity = (context->maxImageWidth + 16) * (param->attribute == ATTRIBUTE_CSC_ORDINARY ? 24 : 80);
    *handle = context;
    return 0;
}

int32_t APS5_VABI sceJpegDecDecode(void* handle, const JpegDecDecodeParam* param, JpegDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (!param->jpeg_mem_addr || !param->image_mem_addr) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if ((reinterpret_cast<std::uintptr_t>(param->image_mem_addr) & 3) != 0 && (param->pixel_format == 0 || param->pixel_format == 1 || param->pixel_format == 9 || param->pixel_format == 10)) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0 || param->image_mem_size == 0) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!validDecodeOptions(param->decode_mode, param->down_scale)) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->pixel_format != 0 && param->pixel_format != 1 && param->pixel_format != 9 && param->pixel_format != 10 && param->pixel_format != 11) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->alpha_value > 255) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    requireDecodeMode(__func__, param->decode_mode);
    if (param->pixel_format != PIXEL_FORMAT_R8G8B8A8 && param->pixel_format != PIXEL_FORMAT_B8G8R8A8) throw std::runtime_error("sceJpegDecDecode: pixel format is not implemented");
    const std::uint32_t downScale = param->down_scale;

    const std::span<const std::uint8_t> jpeg = toBytes(param->jpeg_mem_addr, param->jpeg_mem_size);
    const std::optional<Decoder::Jpeg::Header> header = Decoder::Jpeg::ParseHeader(jpeg);
    if (!header) return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    JpegDecImageInfo parsedInfo{};
    if (!fillImageInfo(*header, downScale, &parsedInfo)) return SCE_JPEG_DEC_ERROR_COEFFICIENT_MEMORY;
    if (parsedInfo.coefficient_mem_size != 0 && (!param->coefficient_mem_addr || param->coefficient_mem_size < parsedInfo.coefficient_mem_size)) return SCE_JPEG_DEC_ERROR_COEFFICIENT_MEMORY;
    if (parsedInfo.color_space == COLOR_SPACE_UNKNOWN) return static_cast<std::int32_t>(0x80650011);
    const std::uint32_t outputWidth = Decoder::Jpeg::ScaledDimension(header->width, downScale);
    const std::uint32_t outputHeight = Decoder::Jpeg::ScaledDimension(header->height, downScale);

    const std::uint64_t rowSize = static_cast<std::uint64_t>(outputWidth) * BYTES_PER_PIXEL;
    const std::uint64_t pitch = param->image_pitch == 0 ? rowSize : param->image_pitch;
    if (param->image_pitch != 0) {
        if (pitch < rowSize || pitch % 4 != 0 || pitch * outputHeight >= 0x80000000 || pitch * outputHeight > param->image_mem_size) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    } else if (pitch * outputHeight > param->image_mem_size) {
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (((outputWidth + 31) & ~std::uint32_t{31}) > context->maxImageWidth || rowBufferSize(*header, downScale) > context->rowBufferCapacity) return SCE_JPEG_DEC_ERROR_IMAGE_TOO_WIDE;

    const std::optional<Decoder::Jpeg::Image> image = downScale == DOWN_SCALE_NONE ? Decoder::Jpeg::Decode(jpeg) : Decoder::Jpeg::DecodeScaled(jpeg, downScale);
    if (!image || image->width != outputWidth || image->height != outputHeight) return SCE_JPEG_DEC_ERROR_DECODE_ERROR;

    const bool swapRedBlue = param->pixel_format == PIXEL_FORMAT_B8G8R8A8;
    const auto alpha = static_cast<std::uint8_t>(param->alpha_value);
    auto* output = static_cast<std::uint8_t*>(param->image_mem_addr);
    for (std::uint32_t y = 0; y < image->height; ++y) {
        std::uint8_t* out = output + y * pitch;
        for (std::uint32_t x = 0; x < image->width; ++x) {
            const std::uint8_t* in = image->pixels.data() + (static_cast<std::size_t>(y) * image->width + x) * image->channels;
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
            if (swapRedBlue) std::swap(pixel[0], pixel[2]);
            pixel[3] = alpha;
        }
    }

    if (imageInfo) fillImageInfo(*header, downScale, imageInfo);
    if (outputWidth > MAX_PACKED_DIMENSION || outputHeight > MAX_PACKED_DIMENSION) return 0;
    return static_cast<std::int32_t>(outputWidth << 16 | outputHeight);
}

int32_t APS5_VABI sceJpegDecDelete(void* handle) {
    Context* context = toContext(handle);
    if (!context) return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    context->self = nullptr;
    return 0;
}

int32_t APS5_VABI sceJpegDecParseHeader(const JpegDecParseParam* param, JpegDecImageInfo* imageInfo) {
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (!param->jpeg_mem_addr) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!validDecodeOptions(param->decode_mode, param->down_scale)) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (!imageInfo) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    requireDecodeMode(__func__, param->decode_mode);
    const std::uint32_t downScale = param->down_scale;
    const std::optional<Decoder::Jpeg::Header> header = Decoder::Jpeg::ParseHeader(toBytes(param->jpeg_mem_addr, param->jpeg_mem_size));
    if (!header) return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    if (!fillImageInfo(*header, downScale, imageInfo)) return SCE_JPEG_DEC_ERROR_COEFFICIENT_MEMORY;
    return 0;
}

int32_t APS5_VABI sceJpegDecQueryMemorySize(const JpegDecCreateParam* param) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    return static_cast<std::int32_t>(workMemorySize(*param));
}

int APS5_VABI sceJpegDecDecodeWithInputControl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
