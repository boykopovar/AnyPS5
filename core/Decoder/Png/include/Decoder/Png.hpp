#ifndef DECODER_PNG_HPP
#define DECODER_PNG_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Decoder::Png {

enum class ColorType : std::uint8_t {
    Grayscale = 0,
    Rgb = 2,
    Palette = 3,
    GrayscaleAlpha = 4,
    Rgba = 6,
};

struct Header {
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t bitDepth;
    ColorType colorType;
    bool interlaced;
    bool hasTransparency;
};

struct Image {
    std::uint32_t width;
    std::uint32_t height;
    std::vector<std::uint8_t> pixels;
};

std::optional<Header> ParseHeader(std::span<const std::uint8_t> png);

std::optional<Image> Decode(std::span<const std::uint8_t> png);

inline constexpr std::uint8_t FILTER_NONE = 1 << 0;
inline constexpr std::uint8_t FILTER_SUB = 1 << 1;
inline constexpr std::uint8_t FILTER_UP = 1 << 2;
inline constexpr std::uint8_t FILTER_AVERAGE = 1 << 3;
inline constexpr std::uint8_t FILTER_PAETH = 1 << 4;
inline constexpr std::uint8_t FILTER_ALL = FILTER_NONE | FILTER_SUB | FILTER_UP | FILTER_AVERAGE | FILTER_PAETH;

struct EncodeOptions {
    int compressionLevel = 8;
    std::uint8_t filters = FILTER_ALL;
};

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, EncodeOptions options = {});

}  // namespace Decoder::Png

#endif  // DECODER_PNG_HPP
