#ifndef DECODER_JPEGENCODER_HPP
#define DECODER_JPEGENCODER_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace Decoder::Jpeg {

enum class Sampling {
    Full,
    Ycc422,
    Ycc420
};

std::vector<std::uint8_t> EncodeBaseline(std::span<const std::uint8_t> pixels, std::uint32_t width, std::uint32_t height,
                                         std::uint32_t channels, int quality, Sampling sampling);

}

#endif
