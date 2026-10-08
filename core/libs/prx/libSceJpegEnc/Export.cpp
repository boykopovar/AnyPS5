#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"

static_assert(sizeof(JpegEncCreateParam) == 0x8);
static_assert(sizeof(JpegEncEncodeParam) == 0x30);
static_assert(sizeof(JpegEncOutputInfo) == 0x8);

namespace {

constexpr std::int32_t SCE_JPEG_ENC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80650101);
constexpr std::int32_t SCE_JPEG_ENC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80650102);
constexpr std::int32_t SCE_JPEG_ENC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80650103);
constexpr std::int32_t SCE_JPEG_ENC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80650104);

constexpr std::uint32_t ATTRIBUTE_NONE = 0;
constexpr std::uint32_t MEMORY_SIZE = 0x800;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 0x20;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;
constexpr std::uint16_t PIXEL_FORMAT_Y8U8Y8V8 = 10;
constexpr std::uint16_t PIXEL_FORMAT_Y8 = 11;

constexpr std::uint16_t ENCODE_MODE_NORMAL = 0;
constexpr std::uint16_t ENCODE_MODE_MJPEG = 1;

constexpr std::uint16_t COLOR_SPACE_YCC = 1;
constexpr std::uint16_t COLOR_SPACE_GRAYSCALE = 2;

constexpr std::uint8_t SAMPLING_TYPE_FULL = 0;
constexpr std::uint8_t SAMPLING_TYPE_422 = 1;
constexpr std::uint8_t SAMPLING_TYPE_420 = 2;

constexpr std::int32_t RESTART_INTERVAL_PER_ROW = -1;

constexpr std::uint32_t MAX_IMAGE_DIMENSION = 0xFFFF;
constexpr std::uint32_t MAX_IMAGE_PITCH = 0xFFFFFFF;
constexpr std::uint64_t MAX_IMAGE_SIZE = 0x7FFFFFFF;

struct Encoder {
    Encoder* self;
};

std::int32_t validateCreateParam(const JpegEncCreateParam* param) {
    if (!param) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (param->size != sizeof(JpegEncCreateParam)) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;
    if (param->attr != ATTRIBUTE_NONE) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    return 0;
}

Encoder* toEncoder(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* encoder = reinterpret_cast<Encoder*>(handle);
    return encoder->self == encoder ? encoder : nullptr;
}

std::int32_t validateEncodeParam(const JpegEncEncodeParam* param) {
    if (!param) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const bool grayscaleInput = param->pixel_format == PIXEL_FORMAT_Y8;
    if (!param->image) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (!grayscaleInput && reinterpret_cast<std::uintptr_t>(param->image) % 4 != 0) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (!param->jpeg) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;

    if (param->image_size == 0 || param->jpeg_size == 0) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;

    if (param->image_width == 0 || param->image_height == 0) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (param->image_width > MAX_IMAGE_DIMENSION || param->image_height > MAX_IMAGE_DIMENSION) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (param->image_pitch == 0 || param->image_pitch > MAX_IMAGE_PITCH) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (!grayscaleInput && param->image_pitch % 4 != 0) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    const std::uint64_t requiredSize = static_cast<std::uint64_t>(param->image_height) * param->image_pitch;
    if (requiredSize > MAX_IMAGE_SIZE || requiredSize > param->image_size) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (param->encode_mode != ENCODE_MODE_NORMAL && param->encode_mode != ENCODE_MODE_MJPEG) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (param->color_space != COLOR_SPACE_YCC && param->color_space != COLOR_SPACE_GRAYSCALE) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (param->sampling_type != SAMPLING_TYPE_FULL && param->sampling_type != SAMPLING_TYPE_422 && param->sampling_type != SAMPLING_TYPE_420) {
        return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (param->restart_interval > static_cast<std::int32_t>(MAX_IMAGE_DIMENSION)) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;

    switch (param->pixel_format) {
    case PIXEL_FORMAT_R8G8B8A8:
    case PIXEL_FORMAT_B8G8R8A8:
        if (param->image_pitch / 4 < param->image_width) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (param->color_space != COLOR_SPACE_YCC || param->sampling_type == SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        return 0;
    case PIXEL_FORMAT_Y8U8Y8V8:
        if (param->image_pitch / 2 < ((param->image_width + 1) & ~1u)) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (param->color_space != COLOR_SPACE_YCC || param->sampling_type == SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        return 0;
    case PIXEL_FORMAT_Y8:
        if (param->image_pitch < param->image_width) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (param->color_space != COLOR_SPACE_GRAYSCALE || param->sampling_type != SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        return 0;
    default:
        return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    }
}

std::uint8_t clampToByte(float value) {
    return static_cast<std::uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
}

void yuvToRgb(std::uint8_t y, std::uint8_t u, std::uint8_t v, std::uint8_t* rgb) {
    const float cb = static_cast<float>(u) - 128.0f;
    const float cr = static_cast<float>(v) - 128.0f;
    rgb[0] = clampToByte(y + 1.402f * cr);
    rgb[1] = clampToByte(y - 0.344136f * cb - 0.714136f * cr);
    rgb[2] = clampToByte(y + 1.772f * cb);
}

std::vector<std::uint8_t> toPackedPixels(const JpegEncEncodeParam& param, std::uint32_t channels) {
    const auto* image = static_cast<const std::uint8_t*>(param.image);
    const std::size_t width = param.image_width;
    std::vector<std::uint8_t> pixels(width * param.image_height * channels);
    for (std::size_t y = 0; y < param.image_height; ++y) {
        const std::uint8_t* row = image + y * param.image_pitch;
        std::uint8_t* out = pixels.data() + y * width * channels;
        for (std::size_t x = 0; x < width; ++x) {
            switch (param.pixel_format) {
            case PIXEL_FORMAT_R8G8B8A8:
                out[x * 3 + 0] = row[x * 4 + 0];
                out[x * 3 + 1] = row[x * 4 + 1];
                out[x * 3 + 2] = row[x * 4 + 2];
                break;
            case PIXEL_FORMAT_B8G8R8A8:
                out[x * 3 + 0] = row[x * 4 + 2];
                out[x * 3 + 1] = row[x * 4 + 1];
                out[x * 3 + 2] = row[x * 4 + 0];
                break;
            case PIXEL_FORMAT_Y8U8Y8V8: {
                const std::uint8_t* pair = row + (x / 2) * 4;
                yuvToRgb(pair[(x % 2) * 2], pair[1], pair[3], out + x * 3);
                break;
            }
            default:
                out[x] = row[x];
                break;
            }
        }
    }
    return pixels;
}

int toQuality(const JpegEncEncodeParam& param) {
    return 100 - param.compression_ratio * 99 / 255;
}

Decoder::Jpeg::Sampling toSampling(const JpegEncEncodeParam& param) {
    switch (param.sampling_type) {
    case SAMPLING_TYPE_FULL: return Decoder::Jpeg::Sampling::Yuv444;
    case SAMPLING_TYPE_422: return Decoder::Jpeg::Sampling::Yuv422;
    case SAMPLING_TYPE_420: return Decoder::Jpeg::Sampling::Yuv420;
    }
    throw std::invalid_argument("sceJpegEncEncode: invalid sampling type");
}

}  // namespace

extern "C" {

int32_t APS5_VABI sceJpegEncCreate(const JpegEncCreateParam* param, void* memory, uint32_t memory_size, void** handle) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    if (!memory) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (memory_size < MEMORY_SIZE) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* encoder = new (reinterpret_cast<void*>(aligned)) Encoder{};
    encoder->self = encoder;
    *handle = encoder;
    return 0;
}

int32_t APS5_VABI sceJpegEncDelete(void* handle) {
    Encoder* encoder = toEncoder(handle);
    if (!encoder) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    encoder->self = nullptr;
    return 0;
}

int32_t APS5_VABI sceJpegEncEncode(void* handle, const JpegEncEncodeParam* param, JpegEncOutputInfo* output_info) {
    if (!toEncoder(handle)) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    const std::int32_t result = validateEncodeParam(param);
    if (result != 0) return result;
    if (param->encode_mode == ENCODE_MODE_MJPEG) throw std::runtime_error("sceJpegEncEncode: MJPEG encode mode is not implemented");
    if (param->restart_interval < RESTART_INTERVAL_PER_ROW) throw std::runtime_error("sceJpegEncEncode: unknown negative restart interval");

    const std::uint32_t channels = param->pixel_format == PIXEL_FORMAT_Y8 ? 1 : 3;
    const std::vector<std::uint8_t> pixels = toPackedPixels(*param, channels);
    const std::uint32_t restartBlocks = param->restart_interval > 0 ? static_cast<std::uint32_t>(param->restart_interval) : 0;
    const std::uint32_t restartRows = param->restart_interval == RESTART_INTERVAL_PER_ROW ? 1 : 0;
    const std::vector<std::uint8_t> jpeg = Decoder::Jpeg::Encode(pixels, param->image_width, param->image_height, channels, toQuality(*param), toSampling(*param),
                                                                 restartBlocks, restartRows);
    if (jpeg.size() > param->jpeg_size) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;

    std::memcpy(param->jpeg, jpeg.data(), jpeg.size());
    if (output_info) {
        output_info->size = static_cast<std::uint32_t>(jpeg.size());
        output_info->height = param->image_height;
    }
    return 0;
}

int32_t APS5_VABI sceJpegEncQueryMemorySize(const JpegEncCreateParam* param) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    return static_cast<std::int32_t>(MEMORY_SIZE);
}

}
