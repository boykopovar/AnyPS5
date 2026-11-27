#include "prx/libSceJpegDec/libSceJpegDec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Jpeg.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

struct Source {
    const std::vector<std::uint8_t>& jpeg;
    bool sent = false;
};

std::int32_t APS5_VABI ReadChunk(JpegDecInputChunk* chunk, void* argument) {
    auto& source = *static_cast<Source*>(argument);
    if (source.sent) return 0;
    chunk->data = source.jpeg.data();
    chunk->size = static_cast<std::uint32_t>(source.jpeg.size());
    source.sent = true;
    return 0;
}

struct FragmentSource {
    const std::vector<std::uint8_t>& bytes;
    std::size_t chunkSize;
    std::size_t offset = 0;
    std::size_t calls = 0;
};

std::int32_t APS5_VABI ReadFragment(JpegDecInputChunk* chunk, void* argument) {
    auto& source = *static_cast<FragmentSource*>(argument);
    ++source.calls;
    if (source.offset == source.bytes.size()) return 0;
    const auto count = std::min(source.chunkSize, source.bytes.size() - source.offset);
    chunk->data = source.bytes.data() + source.offset;
    chunk->size = static_cast<std::uint32_t>(count);
    source.offset += count;
    return 0;
}

void Check(std::uint32_t channels, std::uint16_t scale) {
    constexpr std::uint32_t width = 19, height = 11;
    constexpr std::uint8_t sentinel = 0xCD;
    constexpr std::int32_t invalidSize = static_cast<std::int32_t>(0x80650002);
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x80650003);
    std::vector<std::uint8_t> pixels(width * height * channels);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(i * 37 + i / 7);
    const auto jpeg = Decoder::Jpeg::Encode(pixels, width, height, channels, 90);
    const JpegDecCreateParam create{sizeof(JpegDecCreateParam), 2, width};
    const auto memorySize = sceJpegDecQueryMemorySize(&create);
    Require(memorySize > 0);
    std::vector<std::uint8_t> memory(memorySize);
    void* handle = nullptr;
    Require(sceJpegDecCreate(&create, memory.data(), memorySize, &handle) == 0);
    const std::uint32_t outWidth = (width + scale - 1) / scale;
    const std::uint32_t outHeight = (height + scale - 1) / scale;
    const auto result = static_cast<std::int32_t>(outWidth << 16 | outHeight);
    std::vector<std::uint8_t> rgba(outWidth * outHeight * 4);
    JpegDecDecodeParam param{};
    param.jpeg_mem_addr = jpeg.data();
    param.jpeg_mem_size = static_cast<std::uint32_t>(jpeg.size());
    param.image_mem_addr = rgba.data();
    param.image_mem_size = static_cast<std::uint32_t>(rgba.size());
    param.down_scale = scale;
    param.alpha_value = 0x7F;
    JpegDecImageInfo expectedInfo{};
    Require(sceJpegDecDecode(handle, &param, &expectedInfo) == result);
    Require(expectedInfo.image_width == width && expectedInfo.image_height == height);
    Require(expectedInfo.output_image_width == outWidth && expectedInfo.output_image_height == outHeight);
    Require(expectedInfo.component_number == channels && expectedInfo.color_space == (channels == 1 ? 2 : 1));
    for (const std::uint32_t requestedPitch : {0u, outWidth, outWidth + 2}) {
        const auto pitch = requestedPitch == 0 ? outWidth : requestedPitch;
        std::vector<std::uint8_t> output(pitch * outHeight + 2, sentinel);
        param.image_mem_addr = output.data() + 1;
        Require(reinterpret_cast<std::uintptr_t>(param.image_mem_addr) % 4 != 0);
        param.image_mem_size = pitch * outHeight;
        param.image_pitch = requestedPitch;
        param.pixel_format = 11;
        JpegDecImageInfo info{};
        Require(sceJpegDecDecode(handle, &param, &info) == result);
        Require(std::memcmp(&info, &expectedInfo, sizeof(info)) == 0);
        const auto checkOutput = [&] {
            Require(output.front() == sentinel && output.back() == sentinel);
            for (std::uint32_t y = 0; y < outHeight; ++y) {
                for (std::uint32_t x = 0; x < outWidth; ++x) {
                    const auto* rgb = rgba.data() + (y * outWidth + x) * 4;
                    Require(output[1 + y * pitch + x] == (77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2]) >> 8);
                }
                for (auto x = outWidth; x < pitch; ++x) Require(output[1 + y * pitch + x] == sentinel);
            }
        };
        checkOutput();
        Source source{jpeg};
        JpegDecInputControlParam stream{};
        stream.input = ReadChunk;
        stream.input_argument = &source;
        stream.operation = 0x10;
        stream.down_scale = scale;
        info = {};
        Require(sceJpegDecDecodeWithInputControl(handle, &stream, &info) == 0);
        Require(std::memcmp(&info, &expectedInfo, sizeof(info)) == 0);
        stream.operation = 0x20;
        stream.image_mem_addr = param.image_mem_addr;
        stream.image_mem_size = param.image_mem_size;
        stream.pixel_format = 11;
        stream.image_pitch = requestedPitch;
        std::fill(output.begin(), output.end(), sentinel);
        Require(sceJpegDecDecodeWithInputControl(handle, &stream, nullptr) == result);
        checkOutput();
        std::fill(output.begin(), output.end(), sentinel);
        --param.image_mem_size;
        --stream.image_mem_size;
        Require(sceJpegDecDecode(handle, &param, nullptr) == (requestedPitch == 0 ? invalidSize : invalidParam));
        Require(sceJpegDecDecodeWithInputControl(handle, &stream, nullptr) == (requestedPitch == 0 ? invalidSize : invalidParam));
        Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == sentinel; }));
        ++param.image_mem_size;
        ++stream.image_mem_size;
        if (outWidth > 1) {
            param.image_pitch = outWidth - 1;
            stream.image_pitch = outWidth - 1;
            Require(sceJpegDecDecode(handle, &param, nullptr) == invalidParam);
            Require(sceJpegDecDecodeWithInputControl(handle, &stream, nullptr) == invalidParam);
            Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == sentinel; }));
        }
    }
    std::vector<std::uint8_t> streamed = jpeg;
    const std::uint8_t sos[] = {0xFF, 0xDA};
    const auto scan = std::search(streamed.begin(), streamed.end(), sos, sos + 2);
    Require(scan != streamed.end());
    constexpr std::size_t commentBytes = 3000;
    std::vector<std::uint8_t> comment(commentBytes + 4, 0x5A);
    comment[0] = 0xFF;
    comment[1] = 0xFE;
    comment[2] = static_cast<std::uint8_t>((commentBytes + 2) >> 8);
    comment[3] = static_cast<std::uint8_t>(commentBytes + 2);
    streamed.insert(scan, comment.begin(), comment.end());
    Require(streamed.size() > 0x800);
    FragmentSource source{streamed, std::size_t{64}};
    JpegDecInputControlParam stream{};
    stream.input = ReadFragment;
    stream.input_argument = &source;
    stream.operation = 0x10;
    stream.down_scale = scale;
    JpegDecImageInfo parsed{};
    Require(sceJpegDecDecodeWithInputControl(handle, &stream, &parsed) == 0);
    Require(std::memcmp(&parsed, &expectedInfo, sizeof(parsed)) == 0);
    Require(source.calls > 1 && source.offset < streamed.size());
    const auto callsAfterParse = source.calls;
    std::vector<std::uint8_t> shortOutput(outWidth * outHeight - 1, sentinel);
    stream.operation = 0x20;
    stream.pixel_format = 11;
    stream.image_pitch = 0;
    stream.image_mem_addr = shortOutput.data();
    stream.image_mem_size = static_cast<std::uint32_t>(shortOutput.size());
    Require(sceJpegDecDecodeWithInputControl(handle, &stream, nullptr) == invalidSize);
    Require(source.calls == callsAfterParse);
    Require(std::all_of(shortOutput.begin(), shortOutput.end(), [](auto byte) { return byte == sentinel; }));
    const std::uint32_t retryPitch = outWidth + 2;
    std::vector<std::uint8_t> output(retryPitch * outHeight + 2, sentinel);
    stream.image_pitch = retryPitch;
    stream.image_mem_addr = output.data() + 1;
    Require(reinterpret_cast<std::uintptr_t>(stream.image_mem_addr) % 4 != 0);
    stream.image_mem_size = retryPitch * outHeight;
    Require(sceJpegDecDecodeWithInputControl(handle, &stream, nullptr) == result);
    Require(source.calls > callsAfterParse);
    Require(output.front() == sentinel && output.back() == sentinel);
    for (std::uint32_t y = 0; y < outHeight; ++y) {
        for (std::uint32_t x = 0; x < outWidth; ++x) {
            const auto* rgb = rgba.data() + (y * outWidth + x) * 4;
            Require(output[1 + y * retryPitch + x] == (77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2]) >> 8);
        }
        for (auto x = outWidth; x < retryPitch; ++x) Require(output[1 + y * retryPitch + x] == sentinel);
    }
    Require(sceJpegDecDelete(handle) == 0);
}

}  // namespace

int main() {
    try {
        for (const auto channels : {1u, 3u}) {
            for (const std::uint16_t scale : {1, 2, 4, 8}) Check(channels, scale);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
