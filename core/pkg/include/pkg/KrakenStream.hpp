#ifndef PKG_KRAKENSTREAM_HPP
#define PKG_KRAKENSTREAM_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Pkg {

std::vector<std::uint8_t> BuildKrakenStream(const std::uint8_t* payload, std::size_t payloadSize, std::uint32_t length, std::uint32_t evenPayloadSize, std::uint32_t flags);

}

#endif
