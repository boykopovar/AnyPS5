#include "prx/libSceJpegEnc/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <stdexcept>
#include <vector>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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

std::int32_t validateCreateParam(const JpegEncCreateParam* param, JpegEncCreateParam* outParam = nullptr) {
    if (!param) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    JpegEncCreateParam local{};
    if (!GuestCopyIn_nid_no_patch(&local, param, sizeof(local))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (local.size != sizeof(JpegEncCreateParam)) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;
    if (local.attr != ATTRIBUTE_NONE) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (outParam) *outParam = local;
    return 0;
}

Encoder* toEncoder(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* encoder = reinterpret_cast<Encoder*>(handle);
    Encoder selfCheck{};
    if (!GuestCopyIn_nid_no_patch(&selfCheck, encoder, sizeof(selfCheck))) return nullptr;
    return selfCheck.self == encoder ? encoder : nullptr;
}

std::int32_t validateEncodeParam(const JpegEncEncodeParam* param, JpegEncEncodeParam* outParam = nullptr) {
    if (!param) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    JpegEncEncodeParam local{};
    if (!GuestCopyIn_nid_no_patch(&local, param, sizeof(local))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const bool grayscaleInput = local.pixel_format == PIXEL_FORMAT_Y8;
    if (!local.image) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (!grayscaleInput && reinterpret_cast<std::uintptr_t>(local.image) % 4 != 0) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (!local.jpeg) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;

    if (local.image_size == 0 || local.jpeg_size == 0) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;

    if (local.image_width == 0 || local.image_height == 0) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (local.image_width > MAX_IMAGE_DIMENSION || local.image_height > MAX_IMAGE_DIMENSION) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (local.image_pitch == 0 || local.image_pitch > MAX_IMAGE_PITCH) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (!grayscaleInput && local.image_pitch % 4 != 0) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    const std::uint64_t requiredSize = static_cast<std::uint64_t>(local.image_height) * local.image_pitch;
    if (requiredSize > MAX_IMAGE_SIZE || requiredSize > local.image_size) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (local.encode_mode != ENCODE_MODE_NORMAL && local.encode_mode != ENCODE_MODE_MJPEG) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (local.color_space != COLOR_SPACE_YCC && local.color_space != COLOR_SPACE_GRAYSCALE) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    if (local.sampling_type != SAMPLING_TYPE_FULL && local.sampling_type != SAMPLING_TYPE_422 && local.sampling_type != SAMPLING_TYPE_420) {
        return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (local.restart_interval < RESTART_INTERVAL_PER_ROW || local.restart_interval > static_cast<std::int32_t>(MAX_IMAGE_DIMENSION)) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;

    switch (local.pixel_format) {
    case PIXEL_FORMAT_R8G8B8A8:
    case PIXEL_FORMAT_B8G8R8A8:
        if (local.image_pitch / 4 < local.image_width) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (local.color_space != COLOR_SPACE_YCC || local.sampling_type == SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        break;
    case PIXEL_FORMAT_Y8U8Y8V8:
        if (local.image_pitch / 2 < ((local.image_width + 1) & ~1u)) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (local.color_space != COLOR_SPACE_YCC || local.sampling_type == SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        break;
    case PIXEL_FORMAT_Y8:
        if (local.image_pitch < local.image_width) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        if (local.color_space != COLOR_SPACE_GRAYSCALE || local.sampling_type != SAMPLING_TYPE_FULL) return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
        break;
    default:
        return SCE_JPEG_ENC_ERROR_INVALID_PARAM;
    }
    if (outParam) *outParam = local;
    return 0;
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

std::optional<std::vector<std::uint8_t>> toPackedPixels(const JpegEncEncodeParam& param, std::uint32_t channels) {
    const auto* image = static_cast<const std::uint8_t*>(param.image);
    const std::size_t width = param.image_width;
    std::size_t rowBytes = width;
    if (param.pixel_format == PIXEL_FORMAT_R8G8B8A8 || param.pixel_format == PIXEL_FORMAT_B8G8R8A8) {
        rowBytes = width * 4;
    } else if (param.pixel_format == PIXEL_FORMAT_Y8U8Y8V8) {
        rowBytes = ((width + 1) & ~1u) * 2;
    }
    std::vector<std::uint8_t> rowBuffer(rowBytes);
    std::vector<std::uint8_t> pixels(width * param.image_height * channels);
    for (std::size_t y = 0; y < param.image_height; ++y) {
        const std::uint8_t* rowAddr = image + y * param.image_pitch;
        if (!GuestCopyIn_nid_no_patch(rowBuffer.data(), rowAddr, rowBytes)) return std::nullopt;
        const std::uint8_t* row = rowBuffer.data();
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
    JpegEncCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    if (!memory) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (memory_size < MEMORY_SIZE) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    Encoder localEncoder{};
    localEncoder.self = reinterpret_cast<Encoder*>(aligned);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(aligned), &localEncoder, sizeof(localEncoder))) {
        return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    auto* encoder = reinterpret_cast<Encoder*>(aligned);
    if (!GuestCopyOut_nid_no_patch(handle, &encoder, sizeof(encoder))) {
        Encoder cleared{};
        GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(aligned), &cleared, sizeof(cleared));
        return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    return 0;
}

int32_t APS5_VABI sceJpegEncDelete(void* handle) {
    Encoder* encoder = toEncoder(handle);
    if (!encoder) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    Encoder cleared{};
    if (!GuestCopyOut_nid_no_patch(encoder, &cleared, sizeof(cleared))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    return 0;
}

int32_t APS5_VABI sceJpegEncEncode(void* handle, const JpegEncEncodeParam* param, JpegEncOutputInfo* outputInfo) {
    if (!toEncoder(handle)) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    JpegEncEncodeParam localParam{};
    const std::int32_t result = validateEncodeParam(param, &localParam);
    if (result != 0) return result;
    if (localParam.encode_mode == ENCODE_MODE_MJPEG) throw std::runtime_error("sceJpegEncEncode: MJPEG encode mode is not implemented");

    const std::uint32_t channels = localParam.pixel_format == PIXEL_FORMAT_Y8 ? 1 : 3;
    const std::optional<std::vector<std::uint8_t>> pixels = toPackedPixels(localParam, channels);
    if (!pixels) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const std::uint32_t restartBlocks = localParam.restart_interval > 0 ? static_cast<std::uint32_t>(localParam.restart_interval) : 0;
    const std::uint32_t restartRows = localParam.restart_interval == RESTART_INTERVAL_PER_ROW ? 1 : 0;
    const std::vector<std::uint8_t> jpeg = Decoder::Jpeg::Encode(*pixels, localParam.image_width, localParam.image_height, channels, toQuality(localParam), toSampling(localParam), restartBlocks, restartRows);
    if (jpeg.size() > localParam.jpeg_size) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;

    if (!GuestCopyOut_nid_no_patch(localParam.jpeg, jpeg.data(), jpeg.size())) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (outputInfo) {
        JpegEncOutputInfo localOutput{};
        localOutput.size = static_cast<std::uint32_t>(jpeg.size());
        localOutput.height = localParam.image_height;
        if (!GuestCopyOut_nid_no_patch(outputInfo, &localOutput, sizeof(localOutput))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    }
    return static_cast<std::int32_t>(jpeg.size());
}

int32_t APS5_VABI sceJpegEncQueryMemorySize(const JpegEncCreateParam* param) {
    JpegEncCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    return static_cast<std::int32_t>(MEMORY_SIZE);
}

}
