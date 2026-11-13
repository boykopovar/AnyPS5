#include "PngSurface.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libc/include/General.hpp"

#include "Decoder/Png.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kNoMemory = 1;
constexpr int kReadError = 10;
constexpr int kWriteError = 11;
constexpr int kInvalidSize = 32;
constexpr int kPngError = 39;
constexpr std::array<int, 8> kBitsPerPixel{32, 32, 8, 1, 16, 32, 96, 128};

enum class Format : int { Argb32, Rgb24, A8, A1, Rgb565, Rgb30, Rgb96F, Rgba128F };

using Pixel = std::array<std::uint16_t, 4>;

std::uint32_t ReadBigEndian32(const std::uint8_t* bytes) {
    return std::uint32_t{bytes[0]} << 24 | std::uint32_t{bytes[1]} << 16 |
        std::uint32_t{bytes[2]} << 8 | bytes[3];
}

template <typename T>
T Load(const unsigned char* data) {
    T value{};
    std::memcpy(&value, data, sizeof(value));
    return value;
}

std::uint16_t FloatSample(float value, bool wide) {
    if (!(value > 0)) return 0;
    if (wide) return static_cast<std::uint16_t>(value >= 1 ? 65535 : value * 65535.f);
    return static_cast<std::uint16_t>(value >= 1 ? 255 : value * 256.f);
}

Pixel ReadPixel(const unsigned char* row, int x, Format format, bool wide) {
    switch (format) {
    case Format::Argb32:
    case Format::Rgb24: {
        const auto word = Load<std::uint32_t>(row + 4 * x);
        return {static_cast<std::uint8_t>(word >> 16), static_cast<std::uint8_t>(word >> 8),
                static_cast<std::uint8_t>(word),
                static_cast<std::uint16_t>(format == Format::Argb32 ? word >> 24 : 255)};
    }
    case Format::A8:
        return {row[x], 0, 0, 0};
    case Format::A1: {
        const int bit = std::endian::native == std::endian::little ? x % 8 : 7 - x % 8;
        return {static_cast<std::uint16_t>(((row[x / 8] >> bit) & 1) * 255), 0, 0, 0};
    }
    case Format::Rgb565: {
        const auto word = Load<std::uint16_t>(row + 2 * x);
        const unsigned r = word >> 11;
        const unsigned g = (word >> 5) & 63;
        const unsigned b = word & 31;
        return {static_cast<std::uint16_t>((r << 3) | (r >> 2)),
                static_cast<std::uint16_t>((g << 2) | (g >> 4)),
                static_cast<std::uint16_t>((b << 3) | (b >> 2)), 255};
    }
    case Format::Rgb30: {
        const auto word = Load<std::uint32_t>(row + 4 * x);
        return {static_cast<std::uint8_t>(word >> 22), static_cast<std::uint8_t>(word >> 12),
                static_cast<std::uint8_t>(word >> 2), 255};
    }
    case Format::Rgb96F:
    case Format::Rgba128F: {
        const bool alphaFormat = format == Format::Rgba128F;
        const auto* pixel = row + (alphaFormat ? 16 : 12) * x;
        const float alpha = alphaFormat ? Load<float>(pixel + 12) : 1.f;
        if (wide && !(alpha > 0)) return {};
        const float divisor = wide ? alpha : 1.f;
        return {FloatSample(Load<float>(pixel) / divisor, wide),
                FloatSample(Load<float>(pixel + 4) / divisor, wide),
                FloatSample(Load<float>(pixel + 8) / divisor, wide),
                FloatSample(alpha, wide)};
    }
    }
    NotImplemented_nid_no_patch(__func__);
}

const void* RowAddress(const CairoPngView& view, int y) {
    const auto base = reinterpret_cast<std::uintptr_t>(view.data);
    const auto offset = std::int64_t{view.stride} * y;
    if (offset < 0) {
        const auto magnitude = static_cast<std::uint64_t>(-offset);
        return base >= magnitude ? reinterpret_cast<const void*>(base - magnitude) : nullptr;
    }
    return static_cast<std::uint64_t>(offset) <= std::numeric_limits<std::uintptr_t>::max() - base
        ? reinterpret_cast<const void*>(base + offset) : nullptr;
}

int EncodeImage(const CairoPngView& view, std::vector<std::uint8_t>& png) {
    if (view.format < 0 || view.format >= static_cast<int>(kBitsPerPixel.size()))
        NotImplemented_nid_no_patch(__func__);
    if (!std::isfinite(view.x) || !std::isfinite(view.y) ||
        !std::isfinite(view.width) || !std::isfinite(view.height))
        NotImplemented_nid_no_patch(__func__);
    if (view.width > 32767 || view.height > 32767 || view.width < 0 || view.height < 0)
        return kInvalidSize;
    const int width = static_cast<int>(view.width);
    const int height = static_cast<int>(view.height);
    if (!width || !height) return kWriteError;

    const auto format = static_cast<Format>(view.format);
    const bool gray = format == Format::A8 || format == Format::A1;
    const bool alpha = format == Format::Argb32 || format == Format::Rgba128F;
    const bool wide = !view.subsurface && (format == Format::Rgb96F || format == Format::Rgba128F);
    unsigned channels = gray ? 1 : alpha ? 4 : 3;
    const unsigned sampleSize = wide ? 2 : 1;
    const auto rowBytes = std::uint64_t{static_cast<unsigned>(width)} * channels * sampleSize;
    if ((rowBytes + 1) * height > std::numeric_limits<int>::max()) return kNoMemory;
    std::vector<std::uint8_t> samples(static_cast<std::size_t>(rowBytes) * height);
    std::vector<unsigned char> row((static_cast<std::size_t>(view.sourceWidth) * kBitsPerPixel[view.format] + 7) / 8);
    const bool horizontalOverlap = view.x < view.sourceWidth && view.x + width > 0;
    bool opaque = alpha && !wide;
    std::size_t offset = 0;
    for (int y = 0; y < height; ++y) {
        const double sourceY = view.y + y;
        const bool hasRow = horizontalOverlap && sourceY >= 0 && sourceY < view.sourceHeight;
        if (hasRow) {
            const auto* address = RowAddress(view, static_cast<int>(sourceY));
            if (!address || !GuestCopyIn_nid_no_patch(row.data(), address, row.size()))
                NotImplemented_nid_no_patch(__func__);
        }
        for (int x = 0; x < width; ++x) {
            const double sourceX = view.x + x;
            Pixel pixel{};
            if (hasRow && sourceX >= 0 && sourceX < view.sourceWidth)
                pixel = ReadPixel(row.data(), static_cast<int>(sourceX), format, wide);
            if (alpha && !wide) {
                const auto a = pixel[3];
                opaque &= a == 255;
                for (unsigned channel = 0; channel < 3; ++channel)
                    pixel[channel] = a ? (pixel[channel] * 255u + a / 2u) / a : 0;
            }
            for (unsigned channel = 0; channel < channels; ++channel) {
                if (wide) samples[offset++] = static_cast<std::uint8_t>(pixel[channel] >> 8);
                samples[offset++] = static_cast<std::uint8_t>(pixel[channel]);
            }
        }
    }
    if (opaque) {
        const auto pixels = samples.size() / 4;
        for (std::size_t i = 0; i < pixels; ++i)
            std::memmove(samples.data() + 3 * i, samples.data() + 4 * i, 3);
        samples.resize(pixels * 3);
        channels = 3;
    }
    try {
        png = Decoder::Png::Encode(samples, width, height, channels,
                                   {.bitDepth = static_cast<std::uint8_t>(wide ? 16 : 8), .whiteBackground = true});
    } catch (const std::runtime_error&) {
        return kPngError;
    }
    return 0;
}

int WriteChunks(const std::vector<std::uint8_t>& png, CairoWriteFunc write, void* closure) {
    if (const int status = write(closure, png.data(), 8)) return status;
    for (std::size_t position = 8; position < png.size();) {
        const auto* chunk = png.data() + position;
        const auto length = ReadBigEndian32(chunk);
        if (const int status = write(closure, chunk, 8)) return status;
        if (length) {
            if (const int status = write(closure, chunk + 8, length)) return status;
        }
        if (const int status = write(closure, chunk + 8 + length, 4)) return status;
        position += std::size_t{length} + 12;
    }
    return 0;
}

int ReadChunks(CairoReadFunc read, void* closure, std::vector<std::uint8_t>& png) {
    constexpr std::array<std::uint8_t, 8> signature{0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    png.resize(signature.size());
    if (const int status = read(closure, png.data(), png.size())) return status;
    if (!std::ranges::equal(png, signature)) return kPngError;

    for (bool first = true;; first = false) {
        std::array<std::uint8_t, 8> chunk{};
        if (const int status = read(closure, chunk.data(), chunk.size())) return status;
        const std::uint32_t length = ReadBigEndian32(chunk.data());
        const char* type = reinterpret_cast<const char*>(chunk.data() + 4);
        const bool isHeader = std::memcmp(type, "IHDR", 4) == 0;
        const bool isEnd = std::memcmp(type, "IEND", 4) == 0;
        if (first != isHeader || (isHeader && length != 13) || (isEnd && length != 0))
            return kPngError;
        if (png.size() + std::uint64_t{length} + 12 > std::numeric_limits<int>::max())
            return kNoMemory;

        const auto start = png.size();
        png.insert(png.end(), chunk.begin(), chunk.end());
        for (std::uint32_t remaining = length; remaining != 0;) {
            const auto count = std::min<std::uint32_t>(remaining, 8192);
            const auto offset = png.size();
            png.resize(offset + count);
            if (const int status = read(closure, png.data() + offset, count)) return status;
            remaining -= count;
        }
        const auto crcOffset = png.size();
        png.resize(crcOffset + 4);
        if (const int status = read(closure, png.data() + crcOffset, 4)) return status;
        if (!Decoder::Png::CheckChunkCrc(std::span(png).subspan(start + 4, length + 4),
                                         ReadBigEndian32(png.data() + crcOffset)))
            return kPngError;
        if (isHeader) {
            const auto header = Decoder::Png::ParseHeader(png);
            if (!header) return kPngError;
            if (header->width > 32767 || header->height > 32767) return kInvalidSize;
        }
        if (isEnd) return 0;
    }
}

template <typename Sample>
void WritePixels(CairoSurface* surface, const Sample* pixels, bool alpha) {
    const int width = cairo_image_surface_get_width(surface);
    const int height = cairo_image_surface_get_height(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    auto* data = cairo_image_surface_get_data(surface);
    for (int y = 0; y < height; ++y) {
        auto* row = data + static_cast<std::size_t>(stride) * y;
        for (int x = 0; x < width; ++x, pixels += 4) {
            if constexpr (sizeof(Sample) == 1) {
                const std::uint32_t opacity = alpha ? pixels[3] : 255;
                const auto channel = [opacity](unsigned value) { return (value * opacity + 127) / 255; };
                const std::uint32_t pixel = opacity << 24 | channel(pixels[0]) << 16 |
                    channel(pixels[1]) << 8 | channel(pixels[2]);
                std::memcpy(row + x * sizeof(pixel), &pixel, sizeof(pixel));
            } else {
                const float opacity = alpha ? pixels[3] / 65535.f : 1.f;
                const std::array pixel{pixels[0] / 65535.f * opacity, pixels[1] / 65535.f * opacity,
                                       pixels[2] / 65535.f * opacity, opacity};
                const std::size_t bytes = (alpha ? 4 : 3) * sizeof(float);
                std::memcpy(row + x * bytes, pixel.data(), bytes);
            }
        }
    }
}

template <typename Sample>
CairoSurface* ImageSurface(const Decoder::Png::Header& header, const Sample* pixels) {
    using enum Decoder::Png::ColorType;
    const bool alpha = header.hasTransparency || header.colorType == GrayscaleAlpha || header.colorType == Rgba;
    const auto format = sizeof(Sample) == 1 ? (alpha ? Format::Argb32 : Format::Rgb24)
                                            : (alpha ? Format::Rgba128F : Format::Rgb96F);
    auto* surface = cairo_image_surface_create(static_cast<int>(format), header.width, header.height);
    if (cairo_surface_status(surface) == 0) WritePixels(surface, pixels, alpha);
    return surface;
}

}

extern "C" CairoSurface* APS5_VABI cairo_image_surface_create_from_png_stream(CairoReadFunc read, void* closure) {
    if (!read) NotImplemented_nid_no_patch(__func__);
    try {
        std::vector<std::uint8_t> png;
        if (const int status = ReadChunks(read, closure, png)) return CairoSurfaceError(status);
        const auto header = Decoder::Png::ParseHeader(png);
        if (!header) return CairoSurfaceError(kPngError);
        if (header->bitDepth == 16) {
            const auto pixels = Decoder::Png::DecodeRgba16(png);
            if (pixels.empty()) return CairoSurfaceError(kPngError);
            return ImageSurface(*header, pixels.data());
        }
        const auto image = Decoder::Png::Decode(png);
        if (!image || image->width != header->width || image->height != header->height)
            return CairoSurfaceError(kPngError);
        return ImageSurface(*header, image->pixels.data());
    } catch (const std::bad_alloc&) {
        return CairoSurfaceError(kNoMemory);
    } catch (const std::length_error&) {
        return CairoSurfaceError(kNoMemory);
    }
}

extern "C" int APS5_VABI cairo_surface_write_to_png_stream(CairoSurface* surface, CairoWriteFunc write, void* closure) {
    if (!surface) NotImplemented_nid_no_patch(__func__);
    if (const int status = cairo_surface_status(surface)) return status;
    if (!write) NotImplemented_nid_no_patch(__func__);
    std::vector<std::uint8_t> png;
    try {
        if (const int status = EncodeImage(CairoPngSource_nid_no_patch(surface), png)) return status;
    } catch (const std::bad_alloc&) {
        return kNoMemory;
    } catch (const std::length_error&) {
        return kNoMemory;
    }
    return WriteChunks(png, write, closure);
}
