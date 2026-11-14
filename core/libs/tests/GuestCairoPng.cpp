#include "prx/libcairo/include/Surface.hpp"

#include "Decoder/Png.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Cairo PNG check failed at line %u\n", location.line());
    std::abort();
}

using Surface = std::unique_ptr<CairoSurface, decltype(&cairo_surface_destroy)>;

struct Stream {
    std::vector<std::uint8_t> bytes;
    unsigned calls{};
    unsigned rejectCall{};
    int error{};
};

struct InputStream {
    const std::vector<std::uint8_t>& bytes;
    std::size_t position{};
    int error{10};
};

int APS5_VABI Read(void* closure, unsigned char* data, unsigned length) {
    auto& stream = *static_cast<InputStream*>(closure);
    if (length > stream.bytes.size() - stream.position) return stream.error;
    std::memcpy(data, stream.bytes.data() + stream.position, length);
    stream.position += length;
    return 0;
}

int APS5_VABI Collect(void* closure, const unsigned char* data, unsigned length) {
    auto& stream = *static_cast<Stream*>(closure);
    Require(length != 0);
    if (++stream.calls == stream.rejectCall) return stream.error;
    stream.bytes.insert(stream.bytes.end(), data, data + length);
    return 0;
}

struct Source {
    const std::vector<std::uint8_t>* bytes;
    std::size_t position{};
    unsigned calls{};
    unsigned failCall{};
};

int APS5_VABI Supply(void* closure, unsigned char* data, unsigned length) {
    auto& source = *static_cast<Source*>(closure);
    if (++source.calls == source.failCall || length > source.bytes->size() - source.position) return 10;
    std::copy_n(source.bytes->begin() + static_cast<std::ptrdiff_t>(source.position), length, data);
    source.position += length;
    return 0;
}

Surface Borrow(void* pixels, int format, int width, int height, int stride) {
    return Surface(cairo_image_surface_create_for_data(static_cast<unsigned char*>(pixels), format, width, height, stride),
                   cairo_surface_destroy);
}

void CheckImage(CairoSurface* surface, unsigned width, unsigned height, unsigned bitDepth,
                Decoder::Png::ColorType type, std::initializer_list<unsigned char> pixels) {
    Stream stream;
    Require(cairo_surface_write_to_png_stream(surface, Collect, &stream) == 0);
    Require(stream.calls >= 8);
    const auto header = Decoder::Png::ParseHeader(stream.bytes);
    Require(header && header->width == width && header->height == height &&
            header->bitDepth == bitDepth && header->colorType == type);
    constexpr std::array<unsigned char, 4> background{'b', 'K', 'G', 'D'};
    Require(std::search(stream.bytes.begin(), stream.bytes.end(), background.begin(), background.end()) != stream.bytes.end());
    const auto image = Decoder::Png::Decode(stream.bytes);
    Require(image && image->width == width && image->height == height &&
            std::equal(image->pixels.begin(), image->pixels.end(), pixels.begin(), pixels.end()));
}

void CheckFormats() {
    using enum Decoder::Png::ColorType;
    std::array<std::uint32_t, 3> argb{0x80402010, 0x00010203, 0xdeadbeef};
    auto surface = Borrow(argb.data(), 0, 2, 1, 12);
    CheckImage(surface.get(), 2, 1, 8, Rgba, {128, 64, 32, 128, 0, 0, 0, 0});
    Require(argb == std::array<std::uint32_t, 3>{0x80402010, 0x00010203, 0xdeadbeef});
    argb[0] = 0xff010203;
    argb[1] = 0xff040506;
    CheckImage(surface.get(), 2, 1, 8, Rgb, {1, 2, 3, 255, 4, 5, 6, 255});

    std::array<std::uint32_t, 4> rgb{0x00040506, 0xffeeeeee, 0x00010203, 0xffdddddd};
    surface = Borrow(rgb.data() + 2, 1, 1, 2, -8);
    CheckImage(surface.get(), 1, 2, 8, Rgb, {1, 2, 3, 255, 4, 5, 6, 255});

    alignas(4) std::array<unsigned char, 4> gray{13, 235, 3, 4};
    surface = Borrow(gray.data(), 2, 2, 1, 4);
    CheckImage(surface.get(), 2, 1, 8, Grayscale, {13, 13, 13, 255, 235, 235, 235, 255});

    std::uint32_t bits = std::endian::native == std::endian::little ? 0b101u : 0xa0000000u;
    surface = Borrow(&bits, 3, 3, 1, 4);
    CheckImage(surface.get(), 3, 1, 8, Grayscale, {255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255});

    alignas(4) std::array<std::uint16_t, 2> rgb565{0x7bef, 0x07e0};
    surface = Borrow(rgb565.data(), 4, 2, 1, 4);
    CheckImage(surface.get(), 2, 1, 8, Rgb, {123, 125, 123, 255, 0, 255, 0, 255});

    std::uint32_t rgb30 = (100u << 20) | (515u << 10) | 1023u;
    surface = Borrow(&rgb30, 5, 1, 1, 4);
    CheckImage(surface.get(), 1, 1, 8, Rgb, {25, 128, 255, 255});

    std::array<float, 3> floatRgb{0.5f, 0.25f, 1.5f};
    surface = Borrow(floatRgb.data(), 6, 1, 1, 12);
    CheckImage(surface.get(), 1, 1, 16, Rgb, {127, 63, 255, 255});

    std::array<float, 4> floatRgba{0.5f, 0.25f, 0.125f, 0.5f};
    surface = Borrow(floatRgba.data(), 7, 1, 1, 16);
    CheckImage(surface.get(), 1, 1, 16, Rgba, {255, 127, 63, 127});
}

void CheckSubsurfaces() {
    using Decoder::Png::ColorType::Rgb;
    std::array<std::uint32_t, 16> pixels{};
    for (unsigned i = 0; i < pixels.size(); ++i) pixels[i] = (i + 1) << 16;
    auto root = Borrow(pixels.data(), 1, 4, 4, 16);
    cairo_surface_set_device_scale(root.get(), 2, 2);
    Surface crop(cairo_surface_create_for_rectangle(root.get(), 0.5, 0.5, 1, 1), cairo_surface_destroy);
    Surface nested(cairo_surface_create_for_rectangle(crop.get(), 0.5, 0.5, 1, 1), cairo_surface_destroy);
    Surface edge(cairo_surface_create_for_rectangle(root.get(), -0.5, -0.5, 1, 1), cairo_surface_destroy);
    cairo_surface_set_device_scale(root.get(), 1, 1);
    root.reset();
    CheckImage(crop.get(), 2, 2, 8, Rgb, {6, 0, 0, 255, 7, 0, 0, 255, 10, 0, 0, 255, 11, 0, 0, 255});
    crop.reset();
    CheckImage(nested.get(), 2, 2, 8, Rgb, {11, 0, 0, 255, 12, 0, 0, 255, 15, 0, 0, 255, 16, 0, 0, 255});
    CheckImage(edge.get(), 2, 2, 8, Rgb, {0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 1, 0, 0, 255});
}

void CheckFailures() {
    for (int width : {0, -1}) {
        Surface surface(cairo_image_surface_create(1, width, 1), cairo_surface_destroy);
        Stream stream;
        Require(cairo_surface_write_to_png_stream(surface.get(), Collect, &stream) == (width == 0 ? 11 : 32));
        Require(stream.calls == 0 && stream.bytes.empty());
    }
    Surface surface(cairo_image_surface_create(0, 3, 2), cairo_surface_destroy);
    for (const unsigned rejected : {1u, 4u, 10u}) {
        Stream stream{{}, 0, rejected, rejected == 4 ? 1 : 11};
        Require(cairo_surface_write_to_png_stream(surface.get(), Collect, &stream) == stream.error);
        Require(stream.calls == rejected);
        Require(cairo_surface_status(surface.get()) == 0);
    }
    Stream healthy;
    Require(cairo_surface_write_to_png_stream(surface.get(), Collect, &healthy) == 0);
}

void CheckRead() {
    auto rgba = Decoder::Png::Encode(std::array<std::uint8_t, 8>{128, 64, 32, 128, 255, 1, 2, 0}, 2, 1, 4);
    InputStream stream{rgba};
    const auto length = rgba.size();
    rgba.push_back(0x5a);
    Surface surface(cairo_image_surface_create_from_png_stream(Read, &stream), cairo_surface_destroy);
    Require(stream.position == length && cairo_surface_status(surface.get()) == 0);
    Require(cairo_image_surface_get_format(surface.get()) == 0);
    Require(cairo_image_surface_get_width(surface.get()) == 2 && cairo_image_surface_get_height(surface.get()) == 1);
    std::array<std::uint32_t, 2> words{};
    std::memcpy(words.data(), cairo_image_surface_get_data(surface.get()), sizeof(words));
    Require(words == std::array<std::uint32_t, 2>{0x80402010, 0});

    const auto rgb = Decoder::Png::Encode(std::array<std::uint8_t, 6>{0xff, 0xff, 0, 0, 0x80, 0}, 1, 1, 3,
                                          {.bitDepth = 16});
    stream.position = 0;
    InputStream wide{rgb};
    surface.reset(cairo_image_surface_create_from_png_stream(Read, &wide));
    Require(cairo_surface_status(surface.get()) == 0 && cairo_image_surface_get_format(surface.get()) == 6);
    std::array<float, 3> samples{};
    std::memcpy(samples.data(), cairo_image_surface_get_data(surface.get()), sizeof(samples));
    Require(samples == std::array<float, 3>{1.f, 0.f, 32768.f / 65535.f});

    auto invalidCrc = rgba;
    invalidCrc.resize(length);
    invalidCrc[29] ^= 1;
    rgba.resize(length - 1);
    surface.reset(cairo_image_surface_create_from_png_stream(Read, &stream));
    Require(cairo_surface_status(surface.get()) == 10);
    stream.position = 0;
    stream.error = 1;
    surface.reset(cairo_image_surface_create_from_png_stream(Read, &stream));
    Require(cairo_surface_status(surface.get()) == 1);
    InputStream malformed{invalidCrc};
    surface.reset(cairo_image_surface_create_from_png_stream(Read, &malformed));
    Require(cairo_surface_status(surface.get()) == 1);
}

}

void CheckRoundTrip(std::vector<std::uint32_t> pixels, int format, int readFormat, std::vector<std::uint32_t> expected) {
    std::vector<std::uint8_t> bytes(pixels.size() * 4);
    std::memcpy(bytes.data(), pixels.data(), bytes.size());
    const int width = static_cast<int>(pixels.size());
    const int stride = cairo_format_stride_for_width(format, width);
    auto surface = Borrow(bytes.data(), format, format == 2 ? 4 : width, 1, stride);
    Stream stream;
    Require(cairo_surface_write_to_png_stream(surface.get(), Collect, &stream) == 0);
    Source source{&stream.bytes};
    Surface read(cairo_image_surface_create_from_png_stream(Supply, &source), cairo_surface_destroy);
    Require(cairo_surface_status(read.get()) == 0);
    Require(source.position == stream.bytes.size());
    Require(cairo_image_surface_get_format(read.get()) == readFormat);
    Require(cairo_image_surface_get_width(read.get()) == static_cast<int>(expected.size()));
    Require(cairo_image_surface_get_height(read.get()) == 1);
    std::vector<std::uint32_t> actual(expected.size());
    std::memcpy(actual.data(), cairo_image_surface_get_data(read.get()), actual.size() * 4);
    Require(actual == expected);
}

void CheckReading() {
    CheckRoundTrip({0xff102030, 0x00000000, 0x80402010}, 0, 0, {0xff102030, 0x00000000, 0x80402010});
    CheckRoundTrip({0xff010203, 0xff040506}, 1, 1, {0xff010203, 0xff040506});
    CheckRoundTrip({0x04eb0d00}, 2, 1, {0xff000000, 0xff0d0d0d, 0xffebebeb, 0xff040404});

    Surface surface(cairo_image_surface_create(0, 3, 2), cairo_surface_destroy);
    Stream stream;
    Require(cairo_surface_write_to_png_stream(surface.get(), Collect, &stream) == 0);
    for (const unsigned failCall : {1u, 2u, 3u}) {
        Source source{&stream.bytes, 0, 0, failCall};
        Surface failed(cairo_image_surface_create_from_png_stream(Supply, &source), cairo_surface_destroy);
        Require(cairo_surface_status(failed.get()) == 10);
        Require(source.calls == failCall);
    }
    const std::vector<std::uint8_t> truncated(stream.bytes.begin(), stream.bytes.end() - 13);
    Source source{&truncated};
    Surface failed(cairo_image_surface_create_from_png_stream(Supply, &source), cairo_surface_destroy);
    Require(cairo_surface_status(failed.get()) == 10);
}

int main() {
    CheckFormats();
    CheckSubsurfaces();
    CheckFailures();
    CheckRead();
    CheckReading();
    std::puts("guest Cairo PNG stream checks passed");
}
