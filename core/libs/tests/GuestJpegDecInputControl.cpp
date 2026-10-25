#include "SceTypes.hpp"
#include "prx/libSceJpegDec/libSceJpegDec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct Source {
    std::vector<std::uint8_t> bytes;
    std::size_t chunkSize = 0;
    std::size_t offset = 0;
    std::size_t calls = 0;
    std::int32_t failure = 0;
};

std::int32_t APS5_VABI ReadChunk(JpegDecInputChunk* chunk, void* argument) {
    auto* source = static_cast<Source*>(argument);
    ++source->calls;
    Require(chunk->data == nullptr && chunk->size == 0);
    if (source->failure != 0) return source->failure;
    if (source->offset == source->bytes.size()) return 0;
    const std::size_t count = std::min(source->chunkSize, source->bytes.size() - source->offset);
    chunk->data = source->bytes.data() + source->offset;
    chunk->size = static_cast<std::uint32_t>(count);
    source->offset += count;
    return 7;
}

std::vector<std::uint8_t> TestJpeg(std::uint32_t width, std::uint32_t height, std::uint32_t channels) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * channels);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>((i * 7) ^ (i >> 5));
    return Decoder::Jpeg::Encode(pixels, width, height, channels, 90);
}

JpegDecInputControlParam StreamParam(std::uint16_t operation, Source& source) {
    JpegDecInputControlParam param{};
    param.input = ReadChunk;
    param.input_argument = &source;
    param.operation = operation;
    param.down_scale = 1;
    param.alpha_value = 0x80;
    return param;
}

struct JpegHandle {
    std::vector<std::uint8_t> memory;
    void* handle = nullptr;
    explicit JpegHandle(std::uint32_t maxWidth) {
        const JpegDecCreateParam create{sizeof(JpegDecCreateParam), 2, maxWidth};
        memory.resize(static_cast<std::size_t>(sceJpegDecQueryMemorySize(&create)) + 64);
        Require(sceJpegDecCreate(&create, memory.data(), static_cast<std::uint32_t>(memory.size()), &handle) == 0);
    }
};

std::vector<std::uint8_t> Reference(const std::vector<std::uint8_t>& jpeg, std::uint32_t width, std::uint32_t height, std::uint16_t downScale, std::int32_t* result) {
    JpegHandle decoder(width);
    std::vector<std::uint8_t> image(static_cast<std::size_t>(width) * height * 4, 0xCD);
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_addr = image.data();
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.down_scale = downScale;
    param.alpha_value = 0x80;
    *result = sceJpegDecDecode(decoder.handle, &param, nullptr);
    return image;
}

void RequireStreamMatches(const std::vector<std::uint8_t>& jpeg, std::uint32_t width, std::uint32_t height, std::uint16_t downScale, std::size_t chunkSize) {
    std::int32_t expected = 0;
    const std::vector<std::uint8_t> reference = Reference(jpeg, width, height, downScale, &expected);
    Require(expected > 0);

    JpegHandle decoder(width);
    Source header{jpeg, chunkSize};
    JpegDecInputControlParam parse = StreamParam(0x10, header);
    parse.down_scale = downScale;
    JpegDecImageInfo info{}, expectedInfo{};
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &parse, &info) == 0);
    const JpegDecParseParam parseHeader{jpeg.data(), static_cast<std::uint32_t>(jpeg.size()), 0, downScale};
    Require(sceJpegDecParseHeader(&parseHeader, &expectedInfo) == 0);
    Require(std::equal(reinterpret_cast<const std::uint8_t*>(&info), reinterpret_cast<const std::uint8_t*>(&info + 1), reinterpret_cast<const std::uint8_t*>(&expectedInfo)));

    Source rest{std::vector<std::uint8_t>(jpeg.begin() + static_cast<std::ptrdiff_t>(header.offset), jpeg.end()), chunkSize};
    std::vector<std::uint8_t> image(reference.size(), 0xCD);
    JpegDecInputControlParam decode = StreamParam(0x20, rest);
    decode.down_scale = downScale;
    decode.image_mem_addr = image.data();
    decode.image_mem_size = static_cast<std::uint32_t>(image.size());
    JpegDecImageInfo untouched{};
    untouched.image_width = 0x1234;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &decode, &untouched) == expected);
    Require(untouched.image_width == 0x1234);
    Require(image == reference);
    Require(sceJpegDecDelete(decoder.handle) == 0);
}

}  // namespace

int main() {
    constexpr std::int32_t invalidAddr = static_cast<std::int32_t>(0x80650001);
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80650002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80650003);
    constexpr std::int32_t invalidHandle = static_cast<std::int32_t>(0x80650004);
    constexpr std::int32_t invalidData = static_cast<std::int32_t>(0x80650010);

    const std::vector<std::uint8_t> jpeg = TestJpeg(96, 64, 3);
    Require(jpeg.size() > 2 * 0x800);
    JpegHandle decoder(96);
    Source source{jpeg, 100};
    JpegDecImageInfo info{};
    JpegDecInputControlParam parse = StreamParam(0x10, source);

    Require(sceJpegDecDecodeWithInputControl(nullptr, &parse, &info) == invalidHandle);
    Require(sceJpegDecDecodeWithInputControl(static_cast<std::uint8_t*>(decoder.handle) + 1, &parse, &info) == invalidHandle);
    std::array<void*, 4> notHandle{};
    Require(sceJpegDecDecodeWithInputControl(notHandle.data(), &parse, &info) == invalidHandle);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, nullptr, &info) == invalidAddr);
    JpegDecInputControlParam noInput = parse;
    noInput.input = nullptr;
    noInput.operation = 0x30;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &noInput, &info) == invalidAddr);
    for (std::uint16_t operation : {0x00, 0x01, 0x12, 0x22, 0x30, 0x31}) {
        JpegDecInputControlParam unknown = parse;
        unknown.operation = operation;
        Require(sceJpegDecDecodeWithInputControl(decoder.handle, &unknown, nullptr) == invalidParam);
    }
    JpegDecInputControlParam reserved = parse;
    reserved.reserved = 1;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &reserved, nullptr) == invalidParam);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &parse, nullptr) == invalidAddr);
    JpegDecInputControlParam otherMode = parse;
    otherMode.operation = 0x11;
    Require(ThrowsRuntimeError([&] { sceJpegDecDecodeWithInputControl(decoder.handle, &otherMode, &info); }));
    Require(source.calls == 0);

    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &parse, &info) == 0);
    Require(info.image_width == 96 && info.image_height == 64 && info.output_image_width == 96);
    Require(source.offset < jpeg.size() && source.offset <= 0x800 + source.chunkSize);
    const std::size_t parseCalls = source.calls;

    std::vector<std::uint8_t> image(96 * 64 * 4);
    JpegDecInputControlParam decode = StreamParam(0x20, source);
    decode.image_mem_addr = image.data();
    decode.image_mem_size = static_cast<std::uint32_t>(image.size());
    JpegDecInputControlParam bad = decode;
    bad.image_mem_addr = nullptr;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidAddr);
    bad = decode;
    bad.image_mem_addr = image.data() + 1;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidAddr);
    bad.pixel_format = 11;
    bad.image_mem_size = 0;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidSize);
    bad = decode;
    bad.image_mem_size = 0;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidSize);
    bad = decode;
    bad.down_scale = 3;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidParam);
    bad = decode;
    bad.pixel_format = 2;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidParam);
    bad = decode;
    bad.alpha_value = 256;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidParam);
    bad = decode;
    bad.image_mem_size = 96 * 64 * 4 - 1;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr) == invalidSize);
    bad = decode;
    bad.down_scale = 2;
    Require(ThrowsRuntimeError([&] { sceJpegDecDecodeWithInputControl(decoder.handle, &bad, nullptr); }));
    Require(source.calls == parseCalls);

    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &decode, nullptr) == (96 << 16 | 64));
    Require(image[3] == 0x80);
    std::int32_t expected = 0;
    Require(image == Reference(jpeg, 96, 64, 1, &expected) && expected == (96 << 16 | 64));

    Source failing{jpeg, 100};
    failing.failure = static_cast<std::int32_t>(0x8000BEEF);
    JpegDecInputControlParam failingParse = StreamParam(0x10, failing);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &failingParse, &info) == static_cast<std::int32_t>(0x8000BEEF));
    Require(failing.calls == 1);
    Source garbage{std::vector<std::uint8_t>(5000, 0x11), 1000};
    JpegDecInputControlParam garbageParse = StreamParam(0x10, garbage);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &garbageParse, &info) == invalidData);
    Require(garbage.calls == 3 && garbage.offset == 3000);
    std::vector<std::uint8_t> imageEnd{0xFF, 0xD8, 0xFF, 0xD9};
    imageEnd.resize(10000, 0x00);
    Source ended{imageEnd, 1000};
    JpegDecInputControlParam endedParse = StreamParam(0x10, ended);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &endedParse, &info) == invalidData);
    Require(ended.calls == 3);
    std::vector<std::uint8_t> broken{0xFF, 0xD8, 0x12, 0x34};
    broken.resize(10000, 0xFF);
    Source brokenSource{broken, 1000};
    JpegDecInputControlParam brokenParse = StreamParam(0x10, brokenSource);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &brokenParse, &info) == invalidData);
    Require(brokenSource.calls == 3);
    const std::size_t endCalls = garbage.calls;
    Require(ThrowsRuntimeError([&] { sceJpegDecDecodeWithInputControl(decoder.handle, &decode, nullptr); }));
    Require(garbage.calls == endCalls);

    Source again{jpeg, 512};
    JpegDecInputControlParam againParse = StreamParam(0x10, again);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &againParse, &info) == 0);
    again.failure = -2;
    JpegDecInputControlParam againDecode = decode;
    againDecode.input_argument = &again;
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &againDecode, nullptr) == -2);

    std::vector<std::uint8_t> withApp(jpeg.begin(), jpeg.begin() + 2);
    const std::array<std::uint8_t, 12> app{0xFF, 0xE1, 0x00, 0x0A, 0xFF, 0xD8, 0x00, 0xFF, 0xD9, 0x00, 0xFF, 0xD9};
    withApp.insert(withApp.end(), app.begin(), app.end());
    withApp.insert(withApp.end(), jpeg.begin() + 2, jpeg.end());
    const std::size_t realEnd = withApp.size();
    withApp.resize(realEnd + 0x10000, 0x5A);
    Source appSource{withApp, 300};
    JpegDecInputControlParam appParse = StreamParam(0x10, appSource);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &appParse, &info) == 0);
    JpegDecInputControlParam appDecode = decode;
    appDecode.input_argument = &appSource;
    std::fill(image.begin(), image.end(), 0);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &appDecode, nullptr) == (96 << 16 | 64));
    Require(appSource.offset >= realEnd && appSource.offset <= realEnd + 0x800 + appSource.chunkSize);
    Require(image == Reference(jpeg, 96, 64, 1, &expected));

    Require(sceJpegDecDelete(decoder.handle) == 0);
    Require(sceJpegDecDecodeWithInputControl(decoder.handle, &decode, nullptr) == invalidHandle);

    RequireStreamMatches(TestJpeg(40, 24, 1), 40, 24, 1, 1);
    RequireStreamMatches(TestJpeg(64, 48, 3), 64, 48, 2, 37);
    RequireStreamMatches(TestJpeg(64, 48, 3), 64, 48, 8, 0x10000);
}
