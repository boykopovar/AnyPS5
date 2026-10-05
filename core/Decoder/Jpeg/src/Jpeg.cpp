#include "Decoder/Jpeg.hpp"

#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace Decoder::Jpeg {

namespace {

constexpr std::uint32_t MAX_DIMENSION = 0xFFFF;

void appendBytes(void* context, void* data, int size) {
    auto* output = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    output->insert(output->end(), bytes, bytes + size);
}

}  // namespace

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, int quality) {
    if (width == 0 || height == 0 || width > MAX_DIMENSION || height > MAX_DIMENSION) {
        throw std::invalid_argument("Jpeg::Encode: unsupported image size");
    }
    if (channels != 1 && channels != 3) throw std::invalid_argument("Jpeg::Encode: channels must be 1 or 3");
    if (quality < 1 || quality > 100) throw std::invalid_argument("Jpeg::Encode: quality must be 1-100");
    if (pixels.size() < static_cast<std::size_t>(width) * height * channels) {
        throw std::invalid_argument("Jpeg::Encode: pixel buffer is too small");
    }

    std::vector<std::uint8_t> output;
    const int written = stbi_write_jpg_to_func(appendBytes, &output, static_cast<int>(width), static_cast<int>(height),
                                               static_cast<int>(channels), pixels.data(), quality);
    if (written == 0) throw std::runtime_error("Jpeg::Encode: encoding failed");
    return output;
}

std::optional<Header> ParseHeader(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels)) return std::nullopt;
    if (width <= 0 || height <= 0 || channels <= 0) return std::nullopt;
    if (static_cast<std::uint32_t>(width) > MAX_DIMENSION || static_cast<std::uint32_t>(height) > MAX_DIMENSION) return std::nullopt;
    return Header{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels)};
}

std::optional<Image> Decode(std::span<const std::uint8_t> jpeg) {
    if (jpeg.empty() || jpeg.size() > INT_MAX) return std::nullopt;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels, 0);
    if (!decoded) return std::nullopt;

    if (width <= 0 || height <= 0 || channels <= 0 ||
        static_cast<std::uint32_t>(width) > MAX_DIMENSION ||
        static_cast<std::uint32_t>(height) > MAX_DIMENSION) {
        stbi_image_free(decoded);
        return std::nullopt;
    }

    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> guard(decoded, stbi_image_free);

    Image image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(channels), {}};
    image.pixels.assign(guard.get(), guard.get() + static_cast<std::size_t>(image.width) * image.height * image.channels);
    return image;
}

}  // namespace Decoder::Jpeg
