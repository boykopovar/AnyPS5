#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/HalfFloatScanout.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

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

std::uint32_t tileOffset64(std::uint32_t x, std::uint32_t y) {
    return ((x << 3u) & 0x0008u) ^ ((x << 4u) & 0x0460u) ^ ((x << 5u) & 0x0300u) ^ ((x << 6u) & 0x0800u) ^ ((x << 9u) & 0x8000u) ^ ((x << 10u) & 0x2000u) ^ ((y << 4u) & 0x0010u) ^ ((y << 6u) & 0x0080u) ^ ((y << 5u) & 0x0f00u) ^ ((y << 10u) & 0x5000u);
}

const Graphics::ColorTargetLayout& gcnLayout(const DisplayBuffer& buffer, std::uint32_t texelBytes) {
    thread_local std::optional<Graphics::ColorTargetLayout> layout;
    thread_local std::array<std::uint32_t, 3> key{};
    const std::array<std::uint32_t, 3> wanted{buffer.pitchInPixel, buffer.height, texelBytes};
    if (!layout || key != wanted) {
        layout.emplace(buffer.pitchInPixel, buffer.height, Graphics::ColorTileMode::GcnDisplay2D, texelBytes);
        key = wanted;
    }
    return *layout;
}

std::size_t texelOffset(const DisplayBuffer& buffer, std::uint32_t x, std::uint32_t y, std::uint32_t texelBytes) {
    if (buffer.tilingMode == DisplayTilingGcn) return gcnLayout(buffer, texelBytes).Offset(x, y);
    if (buffer.tilingMode == 1) {
        const auto pitch = buffer.pitchInPixel == 0 ? buffer.width : buffer.pitchInPixel;
        return (static_cast<std::size_t>(y) * pitch + x) * texelBytes;
    }
    const auto blocksPerRow = (buffer.width + 127u) / 128u;
    const auto blockHeight = 65536u / 128u / texelBytes;
    const auto block = static_cast<std::size_t>(y / blockHeight) * blocksPerRow + x / 128u;
    return block * 65536u + (texelBytes == 8u ? tileOffset64(x, y) : tileOffset(x, y));
}

void decodeHalfFloatTexel(const std::byte* texel, std::byte* pixel, bool redLow) {
    std::array<std::uint16_t, 4> halves{};
    std::memcpy(halves.data(), texel, sizeof(halves));
    pixel[0] = static_cast<std::byte>(Graphics::HalfFloatScanoutCode(halves[redLow ? 2 : 0], false));
    pixel[1] = static_cast<std::byte>(Graphics::HalfFloatScanoutCode(halves[1], false));
    pixel[2] = static_cast<std::byte>(Graphics::HalfFloatScanoutCode(halves[redLow ? 0 : 2], false));
    pixel[3] = static_cast<std::byte>(Graphics::HalfFloatScanoutCode(halves[3], true));
}

void decodeTexel(const std::byte* texel, std::byte* pixel, bool rgba, bool tenBit, bool pq, float whiteNits) {
    if (pq) {
        std::uint32_t word = 0;
        std::memcpy(&word, texel, 4);
        const auto converted = PqToSdrPixel(word, rgba, whiteNits);
        for (std::size_t i = 0; i < converted.size(); ++i) pixel[i] = static_cast<std::byte>(converted[i]);
        return;
    }
    if (tenBit) {
        std::uint32_t word = 0;
        std::memcpy(&word, texel, 4);
        const auto red = static_cast<std::byte>((word >> 2u) & 0xffu);
        const auto green = static_cast<std::byte>((word >> 12u) & 0xffu);
        const auto blue = static_cast<std::byte>((word >> 22u) & 0xffu);
        pixel[0] = rgba ? blue : red;
        pixel[1] = green;
        pixel[2] = rgba ? red : blue;
        pixel[3] = std::byte{0xff};
        return;
    }
    pixel[0] = texel[rgba ? 2 : 0];
    pixel[1] = texel[1];
    pixel[2] = texel[rgba ? 0 : 2];
    pixel[3] = texel[3];
}

constexpr std::uint32_t MetaBlockEdge = 512;
constexpr std::size_t MetaBlockKeys = 4096;
constexpr std::uint32_t KeyEdge = 8;

std::array<std::uint32_t, 2> displayableKeyPixel(const DisplayBuffer& buffer, std::size_t key) {
    const auto pitch = (buffer.width + MetaBlockEdge - 1u) / MetaBlockEdge;
    const auto block = static_cast<std::uint32_t>(key / MetaBlockKeys);
    const auto offset = static_cast<std::uint32_t>(key % MetaBlockKeys);
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    for (std::uint32_t bit = 0; bit < 6; ++bit) {
        x |= ((offset >> (2u * bit)) & 1u) << bit;
        y |= ((offset >> (2u * bit + 1u)) & 1u) << bit;
    }
    return {block % pitch * MetaBlockEdge + x * KeyEdge, block / pitch * MetaBlockEdge + y * KeyEdge};
}

void traceDisplayKeys(const DisplayBuffer& buffer, const std::uint8_t* keys, std::size_t count) {
    static const bool trace = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    if (!trace) return;
    constexpr std::size_t MaxRuns = 256;
    std::string runs;
    std::size_t begin = 0;
    for (std::size_t run = 0; begin < count && run < MaxRuns; ++run) {
        const auto end = static_cast<std::size_t>(std::find_if(keys + begin, keys + count, [&](std::uint8_t key) { return key != keys[begin]; }) - keys);
        char text[32];
        std::snprintf(text, sizeof(text), "%s%02x x%zu", run == 0 ? "" : ", ", keys[begin], end - begin);
        runs += text;
        begin = end;
    }
    if (begin < count) runs += ", ...";
    std::fprintf(stderr, "[dcc-keys] display keys 0x%llx+0x%zx: %s\n", static_cast<unsigned long long>(buffer.dccAddress), count, runs.c_str());
}

}

std::size_t DisplayBufferSize(const DisplayBuffer& buffer) {
    require(buffer.width != 0 && buffer.height != 0 && buffer.width <= 16384 && buffer.height <= 16384, "VideoOut: invalid display buffer dimensions");
    const bool halfFloat = DisplayHalfFloat(buffer.pixelFormat);
    if (!halfFloat && !DisplayPq(buffer.pixelFormat) && baseFormat(buffer.pixelFormat) != PixelFormatB8G8R8A8 && baseFormat(buffer.pixelFormat) != PixelFormatR8G8B8A8) {
        char message[96];
        std::snprintf(message, sizeof(message), "VideoOut: unsupported display pixel format 0x%016llx", static_cast<unsigned long long>(buffer.pixelFormat));
        throw std::runtime_error(message);
    }
    require(!halfFloat || buffer.dccAddress == 0, "VideoOut: DCC metadata on a 16-bit float display buffer is not implemented");
    require(buffer.alignment == 65536u || (buffer.alignment == 4096u && (buffer.tilingMode == 1 || buffer.tilingMode == DisplayTilingGcn)), "VideoOut: 4 KiB display buffer alignment is only supported for linear and PS4 buffers");
    require(buffer.address != 0 && (buffer.address & (buffer.alignment - 1u)) == 0, buffer.alignment == 65536u ? "VideoOut: display buffer requires 64 KiB alignment" : "VideoOut: display buffer requires 4 KiB alignment");
    require(buffer.tilingMode <= 1 || buffer.tilingMode == DisplayTilingGcn, "VideoOut: unsupported display tiling mode");
    if (buffer.tilingMode == DisplayTilingGcn) {
        require(buffer.dccAddress == 0 && !DisplayHalfFloat(buffer.pixelFormat), "VideoOut: PS4 display buffers carry no DCC and no half-float texels");
        require(buffer.pitchInPixel >= buffer.width && buffer.pitchInPixel % 128u == 0, "VideoOut: PS4 tiled display pitch is not padded to 128 pixels");
        return gcnLayout(buffer, DisplayTexelBytes(buffer.pixelFormat)).Bytes();
    }
    require(buffer.tilingMode != 0 || buffer.pitchInPixel == 0, "VideoOut: tiled display pitch is unsupported");
    require(buffer.dccAddress == 0 || buffer.tilingMode == 0, "VideoOut: a linear display buffer cannot carry DCC metadata");
    require(buffer.dccAddress != 0 || buffer.dccClearColor == 0, "VideoOut: a DCC clear color needs DCC metadata");
    const auto pitch = buffer.pitchInPixel == 0 ? buffer.width : buffer.pitchInPixel;
    require(pitch >= buffer.width && pitch <= 16384, "VideoOut: invalid linear display pitch");
    const auto texelBytes = DisplayTexelBytes(buffer.pixelFormat);
    const auto blockHeight = 65536u / 128u / texelBytes;
    const auto size = buffer.tilingMode == 1 ? static_cast<std::uint64_t>(pitch) * buffer.height * texelBytes
        : static_cast<std::uint64_t>((buffer.width + 127u) / 128u) * ((buffer.height + blockHeight - 1u) / blockHeight) * 65536u;
    require(size <= std::numeric_limits<std::size_t>::max() && size <= std::numeric_limits<std::uintptr_t>::max() - buffer.address, "VideoOut: display buffer range overflow");
    return static_cast<std::size_t>(size);
}

std::size_t DisplayBufferOffset(const DisplayBuffer& buffer, std::uint32_t x, std::uint32_t y) {
    static_cast<void>(DisplayBufferSize(buffer));
    require(x < buffer.width && y < buffer.height, "VideoOut: display buffer coordinate out of range");
    return texelOffset(buffer, x, y, DisplayTexelBytes(buffer.pixelFormat));
}

std::vector<std::byte> DecodeDisplayBuffer(const DisplayBuffer& buffer, std::span<const std::byte> source) {
    PerformanceTimer timing("DisplayBuffer.Decode");
    require(source.size() == DisplayBufferSize(buffer), "VideoOut: invalid display buffer size");
    std::vector<std::byte> pixels(static_cast<std::size_t>(buffer.width) * buffer.height * 4);
    if (DisplayHalfFloat(buffer.pixelFormat)) {
        const bool redLow = DisplayRedLow(buffer.pixelFormat);
        timing.Mark("validate_allocate");
        for (std::uint32_t y = 0; y < buffer.height; ++y) {
            for (std::uint32_t x = 0; x < buffer.width; ++x) decodeHalfFloatTexel(source.data() + texelOffset(buffer, x, y, 8u), pixels.data() + (static_cast<std::size_t>(y) * buffer.width + x) * 4, redLow);
        }
        timing.Mark("detile_convert");
        return pixels;
    }
    const bool rgba = DisplayRedLow(buffer.pixelFormat);
    const bool pq = DisplayPq(buffer.pixelFormat);
    const float whiteNits = pq ? PqToSdrWhiteNits(AgcDriver::CurrentHostHdr()) : 0.0f;
    // Bit 56 marks the 10-bit variant: A2B10G10R10 with red in the low bits (the scanout draws render
    // COLOR_2_10_10_10 into it). Presented as 8-bit BGRA by dropping the low two bits of each channel.
    const bool tenBit = (buffer.pixelFormat & PixelFormatUnormBit) != 0;
    timing.Mark("validate_allocate");
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) {
            const auto tiled = texelOffset(buffer, x, y, 4u);
            const auto linear = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
            decodeTexel(source.data() + tiled, pixels.data() + linear, rgba, tenBit, pq, whiteNits);
        }
    }
    timing.Mark("detile_convert");
    return pixels;
}

std::array<std::byte, 4> DisplayBufferClearTexel(const DisplayBuffer& buffer, Graphics::DccKeys keys) {
    static_cast<void>(DisplayBufferSize(buffer));
    require(buffer.dccAddress != 0, "VideoOut: a display buffer without DCC metadata has no fast-clear value");
    std::array<std::byte, 4> texel{};
    if (keys == Graphics::DccKeys::ClearRegister) {
        if ((buffer.dccClearColor >> 32u) != 0) {
            char message[160];
            std::snprintf(message, sizeof(message), "VideoOut: DCC register clear color 0x%016llx of display buffer 0x%llx does not fit its 32-bit texel", static_cast<unsigned long long>(buffer.dccClearColor), static_cast<unsigned long long>(buffer.address));
            throw std::runtime_error(message);
        }
        const auto word = static_cast<std::uint32_t>(buffer.dccClearColor);
        std::memcpy(texel.data(), &word, texel.size());
    } else if (!Graphics::IsDccClear(keys) || !Graphics::FillDccClear(DisplayTexelFormat(buffer.pixelFormat), keys, true, texel)) {
        char message[256];
        std::snprintf(message, sizeof(message), "VideoOut: display buffer 0x%llx (format 0x%016llx) reads %s DCC keys at 0x%llx: only uncompressed keys or a uniform clear code that the display format encodes can be presented", static_cast<unsigned long long>(buffer.address), static_cast<unsigned long long>(buffer.pixelFormat), Graphics::DccKeysName(keys), static_cast<unsigned long long>(buffer.dccAddress));
        throw std::runtime_error(message);
    }
    return texel;
}

std::array<std::byte, 4> DisplayBufferClearPixel(const DisplayBuffer& buffer, Graphics::DccKeys keys) {
    const auto texel = DisplayBufferClearTexel(buffer, keys);
    std::array<std::byte, 4> pixel{};
    decodeTexel(texel.data(), pixel.data(), DisplayRedLow(buffer.pixelFormat) && !DisplayHalfFloat(buffer.pixelFormat), (buffer.pixelFormat & PixelFormatUnormBit) != 0, DisplayPq(buffer.pixelFormat), PqToSdrWhiteNits(AgcDriver::CurrentHostHdr()));
    return pixel;
}

std::size_t DisplayBufferKeyBytes(const DisplayBuffer& buffer) {
    const auto bytes = DisplayBufferSize(buffer);
    require(buffer.tilingMode == 0, "VideoOut: a linear display buffer has no DCC keys");
    return Graphics::DccKeyCount(Graphics::TextureTileMode::kR64KBX, 4, buffer.width, buffer.height, bytes);
}

Graphics::DccKeys DisplayBufferKeys(const DisplayBuffer& buffer) {
    const auto count = DisplayBufferKeyBytes(buffer);
    require(buffer.dccAddress != 0, "VideoOut: a display buffer without DCC metadata has no keys");
    const auto keys = Graphics::CurrentDccKeys(buffer.dccAddress, DisplayBufferSize(buffer), count);
    if (keys == Graphics::DccKeys::Uncompressed || Graphics::IsDccClear(keys)) return keys;
    char differing[128] = "";
    if (keys == Graphics::DccKeys::Mixed && GuestMemory::Accessible(reinterpret_cast<const void*>(buffer.dccAddress), count)) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(buffer.dccAddress);
        traceDisplayKeys(buffer, bytes, count);
        const auto* other = std::find_if(bytes + 1, bytes + count, [&](std::uint8_t key) { return key != bytes[0]; });
        if (other == bytes + count) {
            std::snprintf(differing, sizeof(differing), ": every key is 0x%02x", bytes[0]);
        } else {
            const auto key = static_cast<std::size_t>(other - bytes);
            const auto [x, y] = displayableKeyPixel(buffer, key);
            std::snprintf(differing, sizeof(differing), ": key %zu (pixels %u..%u x %u..%u in the displayable order) is 0x%02x, key 0 is 0x%02x", key, x, x + KeyEdge - 1u, y, y + KeyEdge - 1u, *other, bytes[0]);
        }
    }
    char message[384];
    std::snprintf(message, sizeof(message), "VideoOut: display buffer 0x%llx reads %s DCC keys at 0x%llx over its 0x%zx key bytes%s: presenting DCC metadata that is not uniformly uncompressed or fast-cleared is not implemented", static_cast<unsigned long long>(buffer.address), Graphics::DccKeysName(keys), static_cast<unsigned long long>(buffer.dccAddress), count, differing);
    throw std::runtime_error(message);
}

std::vector<std::byte> ReadDisplayBuffer(const DisplayBuffer& buffer) {
    const auto size = DisplayBufferSize(buffer);
    static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
    if (traceGpu) {
        static const auto start = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[gpu] scanout display buffer 0x%llx %ux%u format 0x%016llx at %.1f s\n", static_cast<unsigned long long>(buffer.address), buffer.width, buffer.height, static_cast<unsigned long long>(buffer.pixelFormat), std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
    GuestMemory::FlushGpuWrites(buffer.address, size);
    GuestMemory::CheckRange(reinterpret_cast<const void*>(buffer.address), size, buffer.alignment);
    const auto* source = reinterpret_cast<const std::byte*>(buffer.address);
    return DecodeDisplayBuffer(buffer, {source, size});
}

std::vector<std::byte> DisplayBufferTexels(const DisplayBuffer& buffer, std::span<const std::byte> source) {
    require(source.size() == DisplayBufferSize(buffer), "VideoOut: invalid display buffer size");
    require(DisplayTexelBytes(buffer.pixelFormat) == 4u, "VideoOut: raw display texels are only read for 4-byte formats");
    std::vector<std::byte> texels(static_cast<std::size_t>(buffer.width) * buffer.height * 4);
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) std::memcpy(texels.data() + (static_cast<std::size_t>(y) * buffer.width + x) * 4, source.data() + texelOffset(buffer, x, y, 4u), 4);
    }
    return texels;
}

std::vector<std::byte> ReadDisplayBufferTexels(const DisplayBuffer& buffer) {
    const auto size = DisplayBufferSize(buffer);
    GuestMemory::FlushGpuWrites(buffer.address, size);
    GuestMemory::CheckRange(reinterpret_cast<const void*>(buffer.address), size, buffer.alignment);
    return DisplayBufferTexels(buffer, {reinterpret_cast<const std::byte*>(buffer.address), size});
}

}

extern "C" std::size_t AgcDriverDisplayBufferSize_nid_postfix(const AgcDriver::DisplayBuffer& buffer) {
    return AgcDriver::DisplayBufferSize(buffer);
}
