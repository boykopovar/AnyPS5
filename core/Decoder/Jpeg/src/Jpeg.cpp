#include "Decoder/Jpeg.hpp"

#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "turbojpeg.h"

namespace Decoder::Jpeg {

namespace {

constexpr std::uint32_t MAX_DIMENSION = 0xFFFF;

class Handle {
public:
    explicit Handle(int type) : handle(tj3Init(type)) {
        if (!handle) throw std::runtime_error(type == TJINIT_COMPRESS ? "Jpeg::Encode: encoder initialization failed" : "Jpeg::DecodeScaled: decoder initialization failed");
    }
    ~Handle() { tj3Destroy(handle); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    operator tjhandle() const { return handle; }

private:
    tjhandle handle;
};

class Buffer {
public:
    ~Buffer() { tj3Free(data); }
    unsigned char* data = nullptr;
};

int toSubsampling(Sampling sampling) {
    switch (sampling) {
    case Sampling::Yuv444: return TJSAMP_444;
    case Sampling::Yuv422: return TJSAMP_422;
    case Sampling::Yuv420: return TJSAMP_420;
    }
    throw std::invalid_argument("Jpeg::Encode: invalid sampling");
}

void validateEncodeOptions(std::uint32_t width, std::uint32_t height, int quality,
                           std::uint32_t restartBlocks, std::uint32_t restartRows) {
    if (width == 0 || height == 0 || width > MAX_DIMENSION || height > MAX_DIMENSION) {
        throw std::invalid_argument("Jpeg::Encode: unsupported image size");
    }
    if (quality < 1 || quality > 100) throw std::invalid_argument("Jpeg::Encode: quality must be 1-100");
    if (restartBlocks > MAX_DIMENSION || restartRows > MAX_DIMENSION || (restartBlocks != 0 && restartRows != 0)) {
        throw std::invalid_argument("Jpeg::Encode: invalid restart interval");
    }
}

void configureEncoder(tjhandle handle, int quality, int subsampling, std::uint32_t restartBlocks, std::uint32_t restartRows) {
    if (tj3Set(handle, TJPARAM_QUALITY, quality) < 0 || tj3Set(handle, TJPARAM_SUBSAMP, subsampling) < 0 ||
        (restartBlocks != 0 && tj3Set(handle, TJPARAM_RESTARTBLOCKS, static_cast<int>(restartBlocks)) < 0) ||
        (restartRows != 0 && tj3Set(handle, TJPARAM_RESTARTROWS, static_cast<int>(restartRows)) < 0)) {
        throw std::runtime_error("Jpeg::Encode: encoder configuration failed");
    }
}

}  // namespace

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, int quality, Sampling sampling, std::uint32_t restartBlocks,
                                 std::uint32_t restartRows) {
    validateEncodeOptions(width, height, quality, restartBlocks, restartRows);
    if (channels != 1 && channels != 3) throw std::invalid_argument("Jpeg::Encode: channels must be 1 or 3");
    if (pixels.size() < static_cast<std::size_t>(width) * height * channels) {
        throw std::invalid_argument("Jpeg::Encode: pixel buffer is too small");
    }
    const int subsampling = toSubsampling(sampling);
    Handle handle(TJINIT_COMPRESS);
    configureEncoder(handle, quality, channels == 1 ? TJSAMP_GRAY : subsampling, restartBlocks, restartRows);
    Buffer buffer;
    std::size_t size = 0;
    const int pixelFormat = channels == 1 ? TJPF_GRAY : TJPF_RGB;
    if (tj3Compress8(handle, pixels.data(), static_cast<int>(width), 0, static_cast<int>(height), pixelFormat, &buffer.data, &size) < 0) {
        throw std::runtime_error("Jpeg::Encode: encoding failed");
    }
    return {buffer.data, buffer.data + size};
}

std::vector<std::uint8_t> EncodePlanarYuv(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                          int quality, Sampling sampling, std::uint32_t restartBlocks, std::uint32_t restartRows) {
    validateEncodeOptions(width, height, quality, restartBlocks, restartRows);
    const int subsampling = toSubsampling(sampling);
    const int cols = static_cast<int>(width);
    const int rows = static_cast<int>(height);
    const auto required = tj3YUVBufSize(cols, 1, rows, subsampling);
    if (required == 0 || pixels.size() < required) throw std::invalid_argument("Jpeg::EncodePlanarYuv: pixel buffer is too small");
    Handle handle(TJINIT_COMPRESS);
    configureEncoder(handle, quality, subsampling, restartBlocks, restartRows);
    Buffer buffer;
    std::size_t size = 0;
    if (tj3CompressFromYUV8(handle, pixels.data(), cols, 1, rows, &buffer.data, &size) < 0) {
        throw std::runtime_error("Jpeg::EncodePlanarYuv: encoding failed");
    }
    return {buffer.data, buffer.data + size};
}

std::uint32_t ScaledDimension(std::uint32_t dimension, std::uint32_t denominator) {
    if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8) {
        throw std::invalid_argument("Jpeg::ScaledDimension: the scale must be 1/1, 1/2, 1/4 or 1/8");
    }
    const tjscalingfactor factor{1, static_cast<int>(denominator)};
    return static_cast<std::uint32_t>(TJSCALED(static_cast<std::uint64_t>(dimension), factor));
}

std::optional<Image> DecodeScaled(std::span<const std::uint8_t> jpeg, std::uint32_t denominator) {
    if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8) {
        throw std::invalid_argument("Jpeg::DecodeScaled: the scale must be 1/1, 1/2, 1/4 or 1/8");
    }
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    Handle handle(TJINIT_DECOMPRESS);
    if (tj3Set(handle, TJPARAM_STOPONWARNING, 1) < 0) throw std::runtime_error("Jpeg::DecodeScaled: decoder configuration failed");
    if (tj3DecompressHeader(handle, jpeg.data(), jpeg.size()) < 0) return std::nullopt;
    const int colorspace = tj3Get(handle, TJPARAM_COLORSPACE);
    if (colorspace == TJCS_CMYK || colorspace == TJCS_YCCK) {
        throw std::runtime_error("Jpeg::DecodeScaled: scaled decoding of a CMYK or YCCK image is not implemented");
    }
    const int width = tj3Get(handle, TJPARAM_JPEGWIDTH);
    const int height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
    if (width <= 0 || height <= 0) return std::nullopt;
    if (tj3SetScalingFactor(handle, tjscalingfactor{1, static_cast<int>(denominator)}) < 0) {
        throw std::runtime_error("Jpeg::DecodeScaled: the decoder refused the scaling factor");
    }

    const std::uint32_t channels = colorspace == TJCS_GRAY ? 1 : 3;
    Image image{ScaledDimension(static_cast<std::uint32_t>(width), denominator), ScaledDimension(static_cast<std::uint32_t>(height), denominator), channels, {}};
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * channels);
    if (tj3Decompress8(handle, jpeg.data(), jpeg.size(), image.pixels.data(), 0, channels == 1 ? TJPF_GRAY : TJPF_RGB) < 0) {
        return std::nullopt;
    }
    return image;
}

bool HasScanHeader(std::span<const std::uint8_t> jpeg) {
    Handle handle(TJINIT_DECOMPRESS);
    if (tj3Set(handle, TJPARAM_STOPONWARNING, 1) < 0) throw std::runtime_error("Jpeg::HasScanHeader: decoder configuration failed");
    return tj3DecompressHeader(handle, jpeg.data(), jpeg.size()) == 0 && tj3Get(handle, TJPARAM_JPEGWIDTH) > 0;
}

std::optional<Header> ParseHeader(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels)) return std::nullopt;
    if (width <= 0 || height <= 0 || channels <= 0) return std::nullopt;
    Header header{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels)};
    std::size_t offset = 2;
    while (offset < jpeg.size()) {
        if (jpeg[offset++] != 0xFF) return std::nullopt;
        while (offset < jpeg.size() && jpeg[offset] == 0xFF) ++offset;
        if (offset == jpeg.size()) return std::nullopt;
        const std::uint8_t marker = jpeg[offset++];
        if (marker == 0xD9 || marker == 0xDA) return std::nullopt;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) continue;
        if (jpeg.size() - offset < 2) return std::nullopt;
        const std::size_t length = (static_cast<std::size_t>(jpeg[offset]) << 8) | jpeg[offset + 1];
        if (length < 2 || length > jpeg.size() - offset) return std::nullopt;
        if (marker == 0xE0) header.app0BeforeFrame = true;
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (length < 8) return std::nullopt;
            header.channels = jpeg[offset + 7];
            if (header.channels == 0 || header.channels > 4 || length != 8 + 3 * header.channels) return std::nullopt;
            header.progressive = marker == 0xC2;
            for (std::uint32_t i = 0; i < header.channels; ++i) {
                header.componentIds[i] = jpeg[offset + 8 + 3 * i];
                header.samplingFactors[i] = jpeg[offset + 9 + 3 * i];
            }
            return header;
        }
        offset += length;
    }
    return std::nullopt;
}

std::optional<Image> Decode(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels, 0), stbi_image_free);
    if (!decoded) return std::nullopt;

    Image image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels), {}};
    image.pixels.assign(decoded.get(), decoded.get() + static_cast<std::size_t>(image.width) * image.height * image.channels);
    return image;
}

std::optional<PlanarImage> DecodePlanarYuv(std::span<const std::uint8_t> jpeg, std::uint32_t denominator) {
    if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8) {
        throw std::invalid_argument("Jpeg::DecodePlanarYuv: the scale must be 1/1, 1/2, 1/4 or 1/8");
    }
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    Handle handle(TJINIT_DECOMPRESS);
    if (tj3Set(handle, TJPARAM_STOPONWARNING, 1) < 0) throw std::runtime_error("Jpeg::DecodePlanarYuv: decoder configuration failed");
    if (tj3DecompressHeader(handle, jpeg.data(), jpeg.size()) < 0) return std::nullopt;
    const int colorspace = tj3Get(handle, TJPARAM_COLORSPACE);
    if (colorspace != TJCS_YCbCr && colorspace != TJCS_GRAY) {
        throw std::runtime_error("Jpeg::DecodePlanarYuv: planar decoding of an image that is not YCbCr or grayscale is not implemented");
    }
    const int subsampling = tj3Get(handle, TJPARAM_SUBSAMP);
    if (subsampling == TJSAMP_UNKNOWN) throw std::runtime_error("Jpeg::DecodePlanarYuv: planar decoding of an unusual chroma subsampling is not implemented");
    const int width = tj3Get(handle, TJPARAM_JPEGWIDTH);
    const int height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
    if (width <= 0 || height <= 0) return std::nullopt;
    if (tj3SetScalingFactor(handle, tjscalingfactor{1, static_cast<int>(denominator)}) < 0) {
        throw std::runtime_error("Jpeg::DecodePlanarYuv: the decoder refused the scaling factor");
    }

    PlanarImage image{ScaledDimension(static_cast<std::uint32_t>(width), denominator), ScaledDimension(static_cast<std::uint32_t>(height), denominator),
                      subsampling == TJSAMP_GRAY ? 1u : 3u};
    unsigned char* planes[3]{};
    int strides[3]{};
    for (std::uint32_t component = 0; component < image.components; ++component) {
        const int planeWidth = tj3YUVPlaneWidth(static_cast<int>(component), static_cast<int>(image.width), subsampling);
        const int planeHeight = tj3YUVPlaneHeight(static_cast<int>(component), static_cast<int>(image.height), subsampling);
        if (planeWidth <= 0 || planeHeight <= 0) throw std::runtime_error("Jpeg::DecodePlanarYuv: the decoder reported no plane size");
        image.planeWidths[component] = static_cast<std::uint32_t>(planeWidth);
        image.planeHeights[component] = static_cast<std::uint32_t>(planeHeight);
        image.planes[component].resize(static_cast<std::size_t>(planeWidth) * static_cast<std::size_t>(planeHeight));
        planes[component] = image.planes[component].data();
        strides[component] = planeWidth;
    }
    if (tj3DecompressToYUVPlanes8(handle, jpeg.data(), jpeg.size(), planes, strides) < 0) return std::nullopt;
    return image;
}

}  // namespace Decoder::Jpeg
