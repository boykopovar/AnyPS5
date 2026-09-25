#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

// Bit 56 selects the 10-bit (A2B10G10R10) variant of the format.
constexpr std::uint64_t PixelFormatUnormBit = 0x0100000000000000ull;
constexpr std::uint64_t PixelFormatB8G8R8A8 = 0x8000000000000000ull;
constexpr std::uint64_t PixelFormatR8G8B8A8 = 0x8000000022000000ull;

std::uint64_t baseFormat(std::uint64_t pixelFormat) {
    return pixelFormat & ~PixelFormatUnormBit;
}

std::uint32_t tileOffset(std::uint32_t x, std::uint32_t y) {
    return ((y << 4u) & 0x0070u) ^ ((y << 5u) & 0x0f00u) ^ ((y << 9u) & 0x1000u) ^ ((y << 8u) & 0x4000u) ^ ((x << 2u) & 0x000cu) ^ ((x << 5u) & 0x0380u) ^ ((x << 4u) & 0x0400u) ^ ((x << 6u) & 0x0800u) ^ ((x << 9u) & 0xa000u);
}

}

std::size_t DisplayBufferSize(const DisplayBuffer& buffer) {
    require(buffer.width != 0 && buffer.height != 0 && buffer.width <= 16384 && buffer.height <= 16384, "VideoOut: invalid display buffer dimensions");
    if (baseFormat(buffer.pixelFormat) != PixelFormatB8G8R8A8 && baseFormat(buffer.pixelFormat) != PixelFormatR8G8B8A8) {
        char message[96];
        std::snprintf(message, sizeof(message), "VideoOut: unsupported display pixel format 0x%016llx", static_cast<unsigned long long>(buffer.pixelFormat));
        throw std::runtime_error(message);
    }
    require(buffer.address != 0 && (buffer.address & 65535u) == 0, "VideoOut: display buffer requires 64 KiB alignment");
    const auto size = static_cast<std::uint64_t>((buffer.width + 127u) / 128u) * ((buffer.height + 127u) / 128u) * 65536u;
    require(size <= std::numeric_limits<std::size_t>::max() && size <= std::numeric_limits<std::uintptr_t>::max() - buffer.address, "VideoOut: display buffer range overflow");
    return static_cast<std::size_t>(size);
}

std::vector<std::byte> DecodeDisplayBuffer(const DisplayBuffer& buffer, std::span<const std::byte> source) {
    PerformanceTimer timing("DisplayBuffer.Decode");
    require(source.size() == DisplayBufferSize(buffer), "VideoOut: invalid tiled display buffer size");
    std::vector<std::byte> pixels(static_cast<std::size_t>(buffer.width) * buffer.height * 4);
    const auto blocksPerRow = (buffer.width + 127u) / 128u;
    const bool rgba = baseFormat(buffer.pixelFormat) == PixelFormatR8G8B8A8;
    // Bit 56 marks the 10-bit variant: A2B10G10R10 with red in the low bits (the scanout draws render
    // COLOR_2_10_10_10 into it). Presented as 8-bit BGRA by dropping the low two bits of each channel.
    const bool tenBit = (buffer.pixelFormat & PixelFormatUnormBit) != 0;
    timing.Mark("validate_allocate");
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) {
            const auto tiled = (static_cast<std::size_t>(y / 128u) * blocksPerRow + x / 128u) * 65536u + tileOffset(x, y);
            const auto linear = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
            if (tenBit) {
                std::uint32_t word = 0;
                std::memcpy(&word, source.data() + tiled, 4);
                const auto red = static_cast<std::byte>((word >> 2u) & 0xffu);
                const auto green = static_cast<std::byte>((word >> 12u) & 0xffu);
                const auto blue = static_cast<std::byte>((word >> 22u) & 0xffu);
                pixels[linear] = rgba ? blue : red;
                pixels[linear + 1] = green;
                pixels[linear + 2] = rgba ? red : blue;
                pixels[linear + 3] = std::byte{0xff};
                continue;
            }
            pixels[linear] = source[tiled + (rgba ? 2 : 0)];
            pixels[linear + 1] = source[tiled + 1];
            pixels[linear + 2] = source[tiled + (rgba ? 0 : 2)];
            pixels[linear + 3] = source[tiled + 3];
        }
    }
    timing.Mark("detile_convert");
    return pixels;
}

std::vector<std::byte> ReadDisplayBuffer(const DisplayBuffer& buffer) {
    const auto size = DisplayBufferSize(buffer);
    static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
    if (traceGpu) {
        static const auto start = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[gpu] scanout display buffer 0x%llx %ux%u format 0x%016llx at %.1f s\n", static_cast<unsigned long long>(buffer.address), buffer.width, buffer.height, static_cast<unsigned long long>(buffer.pixelFormat), std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
    GuestMemory::FlushGpuWrites(buffer.address, size);
    GuestMemory::CheckRange(reinterpret_cast<const void*>(buffer.address), size, 65536);
    const auto* source = reinterpret_cast<const std::byte*>(buffer.address);
    return DecodeDisplayBuffer(buffer, {source, size});
}

}

extern "C" std::size_t AgcDriverDisplayBufferSize_nid_postfix(const AgcDriver::DisplayBuffer& buffer) {
    return AgcDriver::DisplayBufferSize(buffer);
}
