#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPLAYBUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPLAYBUFFER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver {

namespace Graphics {
enum class DccKeys;
}

struct DisplayBuffer {
    std::uint64_t address;
    std::uint64_t pixelFormat;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t tilingMode = 0;
    std::uint32_t pitchInPixel = 0;
    std::uint64_t dccAddress = 0;
    std::uint64_t dccClearColor = 0;
};

std::size_t DisplayBufferSize(const DisplayBuffer& buffer);
std::vector<std::byte> DecodeDisplayBuffer(const DisplayBuffer& buffer, std::span<const std::byte> source);
std::vector<std::byte> ReadDisplayBuffer(const DisplayBuffer& buffer);
std::array<std::byte, 4> DisplayBufferClearPixel(const DisplayBuffer& buffer, Graphics::DccKeys keys);
std::size_t DisplayBufferKeyBytes(const DisplayBuffer& buffer);
Graphics::DccKeys DisplayBufferKeys(const DisplayBuffer& buffer);

}

extern "C" std::size_t AgcDriverDisplayBufferSize_nid_postfix(const AgcDriver::DisplayBuffer& buffer);

#endif
