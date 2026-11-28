#include "prx/libSceJpegEnc/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <vector>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

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

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

bool canCopyOut(void* dst, std::size_t size) {
    if (size == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(dst);
    if (start == 0 || size > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    const int savedErrno = errno;
    const GuestArena::HostWrite guard(dst, size);
    const bool writable = guard.Open() && GuestWritable_nid_no_patch(dst, size);
    errno = savedErrno;
    return writable;
}

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

std::optional<std::vector<std::uint8_t>> toPackedPixels(const JpegEncEncodeParam& param, std::uint32_t channels) {
    const auto* image = static_cast<const std::uint8_t*>(param.image);
    const std::size_t width = param.image_width;
    const std::size_t rowBytes = channels == 1 ? width : width * 4;
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

std::optional<std::vector<std::uint8_t>> encodeYuyv(const JpegEncEncodeParam& param, Decoder::Jpeg::Sampling sampling,
                                                   std::uint32_t restartBlocks, std::uint32_t restartRows) {
    const std::size_t width = param.image_width;
    const std::size_t height = param.image_height;
    const std::size_t chromaStride = (width + 1) / 2;
    const std::size_t lumaStride = chromaStride * 2;
    const bool halfChromaHeight = sampling == Decoder::Jpeg::Sampling::Yuv420;
    const std::size_t lumaRows = height + (halfChromaHeight && (height & 1));
    const std::size_t lumaSize = lumaStride * lumaRows;
    const std::size_t chromaSize = chromaStride * (halfChromaHeight ? lumaRows / 2 : lumaRows);
    const std::size_t size = lumaSize + 2 * chromaSize;
    const auto planes = std::make_unique_for_overwrite<std::uint8_t[]>(size);
    const std::size_t rowBytes = lumaStride * 2;
    const auto row = std::make_unique_for_overwrite<std::uint8_t[]>(rowBytes);
    const auto* image = static_cast<const std::uint8_t*>(param.image);
    for (std::size_t y = 0; y < height; ++y) {
        if (!GuestCopyIn_nid_no_patch(row.get(), image + y * param.image_pitch, rowBytes)) return std::nullopt;
        auto* luma = planes.get() + y * lumaStride;
        auto* cb = planes.get() + lumaSize + (halfChromaHeight ? y / 2 : y) * chromaStride;
        auto* cr = cb + chromaSize;
        const bool average = halfChromaHeight && (y & 1);
        for (std::size_t pair = 0; pair < chromaStride; ++pair) {
            const auto* source = row.get() + pair * 4;
            luma[pair * 2] = source[0];
            luma[pair * 2 + 1] = source[pair * 2 + 1 < width ? 2 : 0];
            if (average) {
                const auto bias = static_cast<unsigned>(pair & 1);
                cb[pair] = static_cast<std::uint8_t>((cb[pair] + source[1] + bias) / 2);
                cr[pair] = static_cast<std::uint8_t>((cr[pair] + source[3] + bias) / 2);
            } else {
                cb[pair] = source[1];
                cr[pair] = source[3];
            }
        }
    }
    if (lumaRows != height) {
        std::memcpy(planes.get() + height * lumaStride, planes.get() + (height - 1) * lumaStride, lumaStride);
    }
    return Decoder::Jpeg::EncodePlanarYuv({planes.get(), size}, param.image_width, param.image_height,
                                          toQuality(param), sampling, restartBlocks, restartRows);
}

std::optional<std::vector<std::uint8_t>> encodePixels(const JpegEncEncodeParam& param) {
    const auto sampling = toSampling(param);
    const auto restartBlocks = param.restart_interval > 0 ? static_cast<std::uint32_t>(param.restart_interval) : 0;
    const std::uint32_t restartRows = param.restart_interval == RESTART_INTERVAL_PER_ROW ? 1 : 0;
    if (param.pixel_format == PIXEL_FORMAT_Y8U8Y8V8) return encodeYuyv(param, sampling, restartBlocks, restartRows);
    const std::uint32_t channels = param.pixel_format == PIXEL_FORMAT_Y8 ? 1 : 3;
    const auto pixels = toPackedPixels(param, channels);
    if (!pixels) return std::nullopt;
    return Decoder::Jpeg::Encode(*pixels, param.image_width, param.image_height, channels, toQuality(param),
                                 sampling, restartBlocks, restartRows);
}

}  // namespace

extern "C" {

int32_t APS5_VABI sceJpegEncCreate(const JpegEncCreateParam* param, void* memory, uint32_t memory_size, void** handle) {
    const ErrnoGuard errnoGuard;
    JpegEncCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    if (!memory) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (memory_size < MEMORY_SIZE) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    if (!canCopyOut(handle, sizeof(void*))) {
        return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    }
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
    const ErrnoGuard errnoGuard;
    Encoder* encoder = toEncoder(handle);
    if (!encoder) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    Encoder cleared{};
    if (!GuestCopyOut_nid_no_patch(encoder, &cleared, sizeof(cleared))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    return 0;
}

int32_t APS5_VABI sceJpegEncEncode(void* handle, const JpegEncEncodeParam* param, JpegEncOutputInfo* outputInfo) {
    const ErrnoGuard errnoGuard;
    if (!toEncoder(handle)) return SCE_JPEG_ENC_ERROR_INVALID_HANDLE;
    JpegEncEncodeParam localParam{};
    const std::int32_t result = validateEncodeParam(param, &localParam);
    if (result != 0) return result;
    if (localParam.encode_mode == ENCODE_MODE_MJPEG) throw std::runtime_error("sceJpegEncEncode: MJPEG encode mode is not implemented");

    const auto encoded = encodePixels(localParam);
    if (!encoded) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    const auto& jpeg = *encoded;
    if (jpeg.size() > localParam.jpeg_size) return SCE_JPEG_ENC_ERROR_INVALID_SIZE;

    if (!canCopyOut(localParam.jpeg, jpeg.size())) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
    if (outputInfo != nullptr && !canCopyOut(outputInfo, sizeof(*outputInfo))) return SCE_JPEG_ENC_ERROR_INVALID_ADDR;
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
    const ErrnoGuard errnoGuard;
    JpegEncCreateParam localParam{};
    const std::int32_t result = validateCreateParam(param, &localParam);
    if (result != 0) return result;
    return static_cast<std::int32_t>(MEMORY_SIZE);
}

}
