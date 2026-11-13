#ifndef DECODER_JPEG_HPP
#define DECODER_JPEG_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Decoder::Jpeg {

struct Image {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t channels;
    std::vector<std::uint8_t> pixels;
};

struct Header {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t channels;
    std::uint8_t componentIds[4]{};
    std::uint8_t samplingFactors[4]{};
    bool progressive = false;
    bool app0BeforeFrame = false;
};

enum class Sampling {
    Yuv444,
    Yuv422,
    Yuv420,
};

std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t channels, int quality, Sampling sampling = Sampling::Yuv444,
                                 std::uint32_t restartBlocks = 0, std::uint32_t restartRows = 0);

std::optional<Header> ParseHeader(std::span<const std::uint8_t> jpeg);

std::optional<Image> Decode(std::span<const std::uint8_t> jpeg);

std::uint32_t ScaledDimension(std::uint32_t dimension, std::uint32_t denominator);

std::optional<Image> DecodeScaled(std::span<const std::uint8_t> jpeg, std::uint32_t denominator);

}  // namespace Decoder::Jpeg

#endif  // DECODER_JPEG_HPP
