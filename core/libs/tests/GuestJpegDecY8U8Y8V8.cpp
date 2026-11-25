#include "prx/libSceJpegDec/libSceJpegDec.hpp"
#include "prx/libSceJpegEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include "turbojpeg.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t InvalidAddr = static_cast<std::int32_t>(0x80650001);
constexpr std::int32_t InvalidSize = static_cast<std::int32_t>(0x80650002);
constexpr std::int32_t InvalidParam = static_cast<std::int32_t>(0x80650003);
constexpr std::uint16_t FormatY8U8Y8V8 = 10;
constexpr std::uint8_t Sentinel = 0xCD;

void Require(bool value, const char* what = "check") {
    if (!value) {
        std::fprintf(stderr, "failed: %s\n", what);
        std::abort();
    }
}

template<typename TFunction>
bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int Difference(int left, int right) { return left > right ? left - right : right - left; }

struct Sampling {
    int subsampling;
    std::uint32_t horizontal;
    std::uint32_t vertical;
};

std::uint8_t LumaAt(std::uint32_t x, std::uint32_t y) { return static_cast<std::uint8_t>(30 + 5 * x + 4 * y); }

std::uint8_t ChromaAt(std::uint32_t component, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint8_t>(component == 1 ? 30 + 11 * x + y : 235 - 9 * x - 2 * y);
}

std::vector<std::uint8_t> PlanarJpeg(std::uint32_t width, std::uint32_t height, const Sampling& sampling) {
    const std::uint32_t lumaWidth = (width + sampling.horizontal - 1) / sampling.horizontal * sampling.horizontal;
    const std::uint32_t lumaHeight = (height + sampling.vertical - 1) / sampling.vertical * sampling.vertical;
    const std::uint32_t chromaWidth = lumaWidth / sampling.horizontal;
    const std::uint32_t chromaHeight = lumaHeight / sampling.vertical;
    Require(tj3YUVPlaneWidth(0, static_cast<int>(width), sampling.subsampling) == static_cast<int>(lumaWidth), "luma plane width");
    Require(tj3YUVPlaneHeight(1, static_cast<int>(height), sampling.subsampling) == static_cast<int>(chromaHeight), "chroma plane height");
    std::vector<std::uint8_t> planes[3];
    planes[0].resize(lumaWidth * lumaHeight);
    for (std::uint32_t y = 0; y < lumaHeight; ++y) {
        for (std::uint32_t x = 0; x < lumaWidth; ++x) planes[0][y * lumaWidth + x] = LumaAt(std::min(x, width - 1), std::min(y, height - 1));
    }
    for (std::uint32_t component = 1; component < 3; ++component) {
        planes[component].resize(chromaWidth * chromaHeight);
        for (std::uint32_t y = 0; y < chromaHeight; ++y) {
            for (std::uint32_t x = 0; x < chromaWidth; ++x) planes[component][y * chromaWidth + x] = ChromaAt(component, x, y);
        }
    }
    const std::unique_ptr<void, decltype(&tj3Destroy)> encoder(tj3Init(TJINIT_COMPRESS), tj3Destroy);
    Require(encoder && tj3Set(encoder.get(), TJPARAM_QUALITY, 100) == 0 && tj3Set(encoder.get(), TJPARAM_SUBSAMP, sampling.subsampling) == 0, "encoder setup");
    const unsigned char* sources[3] = {planes[0].data(), planes[1].data(), planes[2].data()};
    const int strides[3] = {static_cast<int>(lumaWidth), static_cast<int>(chromaWidth), static_cast<int>(chromaWidth)};
    unsigned char* encoded = nullptr;
    std::size_t size = 0;
    Require(tj3CompressFromYUVPlanes8(encoder.get(), sources, static_cast<int>(width), strides, static_cast<int>(height), &encoded, &size) == 0, "planar encode");
    std::vector<std::uint8_t> jpeg(encoded, encoded + size);
    tj3Free(encoded);
    return jpeg;
}

std::vector<std::uint8_t> ExpectedPacked(const Decoder::Jpeg::PlanarImage& image) {
    const std::uint32_t pairs = (image.width + 1) / 2;
    std::vector<std::uint8_t> packed(static_cast<std::size_t>(pairs) * 4 * image.height);
    const std::uint32_t horizontal = image.components == 1 ? 1 : image.planeWidths[0] / image.planeWidths[1];
    const std::uint32_t vertical = image.components == 1 ? 1 : image.planeHeights[0] / image.planeHeights[1];
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t pair = 0; pair < pairs; ++pair) {
            const std::uint32_t x0 = pair * 2;
            const std::uint32_t x1 = std::min(x0 + 1, image.width - 1);
            std::uint8_t* out = packed.data() + (static_cast<std::size_t>(y) * pairs + pair) * 4;
            out[0] = image.planes[0][y * image.planeWidths[0] + x0];
            out[2] = image.planes[0][y * image.planeWidths[0] + x1];
            for (std::uint32_t component = 1; component < 3; ++component) {
                std::uint8_t value = 0x80;
                if (image.components == 3) {
                    const std::uint8_t* row = image.planes[component].data() + (y / vertical) * image.planeWidths[component];
                    if (horizontal == 1 && x1 != x0) {
                        value = static_cast<std::uint8_t>((row[x0] + row[x1] + (pair % 2)) / 2);
                    } else {
                        value = row[x0 / horizontal];
                    }
                }
                out[component == 1 ? 1 : 3] = value;
            }
        }
    }
    return packed;
}

void RequireNearSource(const std::vector<std::uint8_t>& packed, std::uint32_t width, std::uint32_t height, const Sampling& sampling) {
    const std::uint32_t pairs = (width + 1) / 2;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t pair = 0; pair < pairs; ++pair) {
            const std::uint8_t* in = packed.data() + (static_cast<std::size_t>(y) * pairs + pair) * 4;
            const std::uint32_t x0 = pair * 2;
            const std::uint32_t x1 = std::min(x0 + 1, width - 1);
            Require(Difference(in[0], LumaAt(x0, y)) <= 3, "Y0 near source");
            Require(Difference(in[2], LumaAt(x1, y)) <= 3, "Y1 near source");
            for (std::uint32_t component = 1; component < 3; ++component) {
                const std::uint32_t row = y / sampling.vertical;
                int expected = ChromaAt(component, x0 / sampling.horizontal, row);
                if (sampling.horizontal == 1 && x1 != x0) expected = (ChromaAt(component, x0, row) + ChromaAt(component, x1, row) + 1) / 2;
                Require(Difference(in[component == 1 ? 1 : 3], expected) <= 3, "chroma near source");
            }
        }
    }
}

struct DecoderHandle {
    std::vector<std::uint8_t> memory;
    void* handle = nullptr;

    explicit DecoderHandle(std::uint32_t maxWidth) {
        const JpegDecCreateParam create{sizeof(JpegDecCreateParam), 2, maxWidth};
        const auto size = sceJpegDecQueryMemorySize(&create);
        Require(size > 0, "memory size");
        memory.resize(static_cast<std::size_t>(size));
        Require(sceJpegDecCreate(&create, memory.data(), static_cast<std::uint32_t>(size), &handle) == 0, "create");
    }
    ~DecoderHandle() { sceJpegDecDelete(handle); }
};

struct Source {
    const std::vector<std::uint8_t>& jpeg;
    std::size_t chunk;
    std::size_t offset = 0;
};

std::int32_t APS5_VABI ReadChunk(JpegDecInputChunk* chunk, void* argument) {
    auto& source = *static_cast<Source*>(argument);
    if (source.offset == source.jpeg.size()) return 0;
    const std::size_t count = std::min(source.chunk, source.jpeg.size() - source.offset);
    chunk->data = source.jpeg.data() + source.offset;
    chunk->size = static_cast<std::uint32_t>(count);
    source.offset += count;
    return 0;
}

std::vector<std::uint8_t> Unpitched(const std::vector<std::uint8_t>& output, std::size_t offset, std::uint32_t pitch, std::uint32_t rowSize, std::uint32_t height) {
    std::vector<std::uint8_t> rows(static_cast<std::size_t>(rowSize) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(rows.data() + static_cast<std::size_t>(y) * rowSize, output.data() + offset + static_cast<std::size_t>(y) * pitch, rowSize);
        for (std::uint32_t x = rowSize; x < pitch; ++x) Require(output[offset + static_cast<std::size_t>(y) * pitch + x] == Sentinel, "pitch padding untouched");
    }
    return rows;
}

void CheckImage(const std::vector<std::uint8_t>& jpeg, std::uint32_t width, std::uint32_t height, const Sampling* sampling) {
    DecoderHandle decoder(width);
    for (const std::uint16_t scale : {1, 2, 4, 8}) {
        const auto planar = Decoder::Jpeg::DecodePlanarYuv(jpeg, scale);
        Require(planar.has_value(), "reference planes");
        const std::uint32_t outWidth = Decoder::Jpeg::ScaledDimension(width, scale);
        const std::uint32_t outHeight = Decoder::Jpeg::ScaledDimension(height, scale);
        Require(planar->width == outWidth && planar->height == outHeight, "reference size");
        const auto expected = ExpectedPacked(*planar);
        const std::uint32_t rowSize = (outWidth + 1) / 2 * 4;
        Require(expected.size() == static_cast<std::size_t>(rowSize) * outHeight, "expected size");
        if (scale == 1 && sampling) RequireNearSource(expected, width, height, *sampling);
        const auto result = static_cast<std::int32_t>(outWidth << 16 | outHeight);

        JpegDecParseParam parse{};
        parse.jpeg_mem_addr = jpeg.data();
        parse.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
        parse.down_scale = scale;
        JpegDecImageInfo parsed{};
        Require(sceJpegDecParseHeader(&parse, &parsed) == 0, "parse header");

        for (const std::uint32_t requestedPitch : {0u, rowSize, rowSize + 8}) {
            const std::uint32_t pitch = requestedPitch == 0 ? rowSize : requestedPitch;
            std::vector<std::uint8_t> output(static_cast<std::size_t>(pitch) * outHeight + 8, Sentinel);
            JpegDecDecodeParam param{};
            param.jpeg_mem_addr = jpeg.data();
            param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
            param.image_mem_addr = output.data() + 4;
            param.image_mem_size = pitch * outHeight;
            param.down_scale = scale;
            param.pixel_format = FormatY8U8Y8V8;
            param.alpha_value = 0x33;
            param.image_pitch = requestedPitch;
            JpegDecImageInfo info{};
            Require(sceJpegDecDecode(decoder.handle, &param, &info) == result, "decode result");
            Require(std::memcmp(&info, &parsed, sizeof(info)) == 0, "image info");
            Require(std::all_of(output.begin(), output.begin() + 4, [](auto byte) { return byte == Sentinel; }), "no write before");
            Require(std::all_of(output.end() - 4, output.end(), [](auto byte) { return byte == Sentinel; }), "no write after");
            Require(Unpitched(output, 4, pitch, rowSize, outHeight) == expected, "packed output");

            std::fill(output.begin(), output.end(), Sentinel);
            Source source{jpeg, 97};
            JpegDecInputControlParam stream{};
            stream.input = ReadChunk;
            stream.input_argument = &source;
            stream.operation = 0x10;
            stream.down_scale = scale;
            JpegDecImageInfo streamed{};
            Require(sceJpegDecDecodeWithInputControl(decoder.handle, &stream, &streamed) == 0, "stream parse");
            Require(std::memcmp(&streamed, &parsed, sizeof(streamed)) == 0, "stream image info");
            stream.operation = 0x20;
            stream.image_mem_addr = param.image_mem_addr;
            stream.image_mem_size = param.image_mem_size;
            stream.pixel_format = FormatY8U8Y8V8;
            stream.image_pitch = requestedPitch;
            Require(sceJpegDecDecodeWithInputControl(decoder.handle, &stream, nullptr) == result, "stream decode result");
            Require(Unpitched(output, 4, pitch, rowSize, outHeight) == expected, "stream packed output");

            std::fill(output.begin(), output.end(), Sentinel);
            --param.image_mem_size;
            Require(sceJpegDecDecode(decoder.handle, &param, nullptr) == (requestedPitch == 0 ? InvalidSize : InvalidParam), "short image memory");
            ++param.image_mem_size;
            param.image_mem_addr = output.data() + 5;
            Require(sceJpegDecDecode(decoder.handle, &param, nullptr) == InvalidAddr, "misaligned image");
            param.image_mem_addr = output.data() + 4;
            param.image_pitch = rowSize - 4;
            if (rowSize > 4) Require(sceJpegDecDecode(decoder.handle, &param, nullptr) == InvalidParam, "pitch below row");
            param.image_pitch = rowSize + 2;
            param.image_mem_size = (rowSize + 2) * outHeight;
            Require(sceJpegDecDecode(decoder.handle, &param, nullptr) == InvalidParam, "pitch not a multiple of four");
            Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == Sentinel; }), "refusals write nothing");
        }
    }

    std::vector<std::uint8_t> output((width + 1) / 2 * 4 * height, Sentinel);
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_addr = output.data();
    param.image_mem_size = static_cast<std::uint32_t>(output.size());
    param.down_scale = 1;
    param.pixel_format = 9;
    Require(ThrowsRuntimeError([&] { sceJpegDecDecode(decoder.handle, &param, nullptr); }), "format 9 still throws");
}

void CheckEncoderRoundTrip() {
    constexpr std::uint32_t width = 23;
    constexpr std::uint32_t height = 14;
    constexpr std::uint32_t pairs = (width + 1) / 2;
    constexpr std::uint32_t pitch = pairs * 4;
    alignas(4) static std::uint8_t packed[pitch * height];
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t pair = 0; pair < pairs; ++pair) {
            std::uint8_t* out = packed + y * pitch + pair * 4;
            out[0] = LumaAt(pair * 2, y);
            out[1] = ChromaAt(1, pair, y);
            out[2] = LumaAt(std::min(pair * 2 + 1, width - 1), y);
            out[3] = ChromaAt(2, pair, y);
        }
    }
    alignas(32) static std::uint8_t encoderMemory[0x800 + 32];
    const JpegEncCreateParam create{sizeof(JpegEncCreateParam), 0};
    void* encoder = nullptr;
    Require(sceJpegEncCreate(&create, encoderMemory, 0x800, &encoder) == 0, "encoder create");
    std::vector<std::uint8_t> jpeg(0x4000);
    JpegEncEncodeParam encode{};
    encode.image = packed;
    encode.jpeg = jpeg.data();
    encode.image_size = sizeof(packed);
    encode.jpeg_size = static_cast<std::uint32_t>(jpeg.size());
    encode.image_width = width;
    encode.image_height = height;
    encode.image_pitch = pitch;
    encode.pixel_format = FormatY8U8Y8V8;
    encode.color_space = 1;
    encode.sampling_type = 1;
    encode.compression_ratio = 0;
    JpegEncOutputInfo info{};
    const std::int32_t size = sceJpegEncEncode(encoder, &encode, &info);
    Require(size > 0 && static_cast<std::uint32_t>(size) == info.size, "encode");
    Require(sceJpegEncDelete(encoder) == 0, "encoder delete");
    jpeg.resize(static_cast<std::size_t>(size));

    DecoderHandle decoder(width);
    std::vector<std::uint8_t> output(sizeof(packed), Sentinel);
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_addr = output.data();
    param.image_mem_size = static_cast<std::uint32_t>(output.size());
    param.down_scale = 1;
    param.pixel_format = FormatY8U8Y8V8;
    Require(sceJpegDecDecode(decoder.handle, &param, nullptr) == static_cast<std::int32_t>(width << 16 | height), "round trip decode");
    for (std::size_t i = 0; i < output.size(); ++i) Require(Difference(output[i], packed[i]) <= 3, "round trip sample");
}

}  // namespace

int main() {
    try {
        const Sampling samplings[] = {{TJSAMP_422, 2, 1}, {TJSAMP_444, 1, 1}, {TJSAMP_420, 2, 2}, {TJSAMP_440, 1, 2}, {TJSAMP_411, 4, 1}, {TJSAMP_441, 1, 4}};
        for (const auto& sampling : samplings) {
            for (const std::uint32_t width : {17u, 16u}) CheckImage(PlanarJpeg(width, 13, sampling), width, 13, &sampling);
        }
        std::vector<std::uint8_t> gray(19 * 11);
        for (std::size_t i = 0; i < gray.size(); ++i) gray[i] = static_cast<std::uint8_t>(i * 37 + i / 7);
        const auto grayJpeg = Decoder::Jpeg::Encode(gray, 19, 11, 1, 90);
        CheckImage(grayJpeg, 19, 11, nullptr);
        const auto grayPlanar = Decoder::Jpeg::DecodePlanarYuv(grayJpeg, 1);
        Require(grayPlanar.has_value() && grayPlanar->components == 1, "gray planes");
        const auto grayPacked = ExpectedPacked(*grayPlanar);
        for (std::size_t i = 1; i < grayPacked.size(); i += 2) Require(grayPacked[i] == 0x80, "gray chroma");
        CheckEncoderRoundTrip();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
