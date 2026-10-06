#include "Decoder/Jpeg.hpp"

#include <climits>
#include <cstddef>
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
    Handle() : handle(tj3Init(TJINIT_COMPRESS)) {
        if (!handle) throw std::runtime_error("Jpeg::Encode: encoder initialization failed");
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

}  // namespace

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, int quality, Sampling sampling) {
    if (width == 0 || height == 0 || width > MAX_DIMENSION || height > MAX_DIMENSION) {
        throw std::invalid_argument("Jpeg::Encode: unsupported image size");
    }
    if (channels != 1 && channels != 3) throw std::invalid_argument("Jpeg::Encode: channels must be 1 or 3");
    if (quality < 1 || quality > 100) throw std::invalid_argument("Jpeg::Encode: quality must be 1-100");
    if (pixels.size() < static_cast<std::size_t>(width) * height * channels) {
        throw std::invalid_argument("Jpeg::Encode: pixel buffer is too small");
    }
    const int subsampling = toSubsampling(sampling);
    Handle handle;
    if (tj3Set(handle, TJPARAM_QUALITY, quality) < 0 || tj3Set(handle, TJPARAM_SUBSAMP, channels == 1 ? TJSAMP_GRAY : subsampling) < 0) {
        throw std::runtime_error("Jpeg::Encode: encoder configuration failed");
    }
    Buffer buffer;
    std::size_t size = 0;
    const int pixelFormat = channels == 1 ? TJPF_GRAY : TJPF_RGB;
    if (tj3Compress8(handle, pixels.data(), static_cast<int>(width), 0, static_cast<int>(height), pixelFormat, &buffer.data, &size) < 0) {
        throw std::runtime_error("Jpeg::Encode: encoding failed");
    }
    return {buffer.data, buffer.data + size};
}

std::optional<Header> ParseHeader(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels)) return std::nullopt;
    if (width <= 0 || height <= 0 || channels <= 0) return std::nullopt;
    return Header{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels)};
}

std::optional<Image> Decode(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels, 0);
    if (!decoded) return std::nullopt;

    Image image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels), {}};
    image.pixels.assign(decoded, decoded + static_cast<std::size_t>(image.width) * image.height * image.channels);
    stbi_image_free(decoded);
    return image;
}

}  // namespace Decoder::Jpeg
