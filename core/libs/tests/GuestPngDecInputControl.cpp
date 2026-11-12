#include "prx/libScePngDec/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Decoder/Png.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: PNG input-control assertion failed\n", location.file_name(), location.line());
    std::abort();
}

namespace {

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        memory = static_cast<std::uint8_t*>(mmap(nullptr, pageSize * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Protect(false);
        std::memset(memory + pageSize, 0xA5, pageSize);
    }

    ~GuardedPage() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, pageSize * 3) == 0);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + pageSize, pageSize, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + pageSize, pageSize, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }

    std::uint8_t* End(std::size_t bytes = 0) const {
        Require(bytes <= pageSize);
        return memory + pageSize * 2 - bytes;
    }

private:
    std::uint8_t* memory = nullptr;
    std::size_t pageSize = 0;
};

struct Source {
    const std::vector<std::uint8_t>* png = nullptr;
    std::size_t offset = 0;
    std::size_t chunkSize = 5;
    std::int32_t errorAt = -1;
    std::size_t calls = 0;
};

std::int32_t APS5_VABI ReadChunk(PngDecInputChunk* chunk, void* argument) {
    auto* source = static_cast<Source*>(argument);
    ++source->calls;
    chunk->data = nullptr;
    chunk->size = 0;
    chunk->reserved = 0;
    if (source->errorAt >= 0 && source->offset >= static_cast<std::size_t>(source->errorAt)) return -123;
    if (source->offset >= source->png->size()) return 0;
    const std::size_t count = std::min(source->chunkSize, source->png->size() - source->offset);
    chunk->data = source->png->data() + source->offset;
    chunk->size = static_cast<std::uint32_t>(count);
    source->offset += count;
    return 0;
}

struct TestDecoder {
    std::vector<std::uint8_t> work;
    void* handle = nullptr;

    explicit TestDecoder(std::uint32_t maxWidth) {
        PngDecCreateParam param{sizeof(PngDecCreateParam), 0, maxWidth};
        const std::int32_t size = scePngDecQueryMemorySize(&param);
        Require(size > 0);
        work.resize(static_cast<std::size_t>(size));
        Require(scePngDecCreate(&param, work.data(), static_cast<std::uint32_t>(work.size()), &handle) == 0);
    }
    ~TestDecoder() { if (handle) scePngDecDelete(handle); }
    TestDecoder(const TestDecoder&) = delete;
    TestDecoder& operator=(const TestDecoder&) = delete;
};

static const std::uint8_t PALETTE_TRNS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x0F, 0xD8, 0xE5,
    0xB7, 0x00, 0x00, 0x00, 0x0C, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xD6, 0x02, 0x8F, 0x7B, 0x00, 0x00, 0x00, 0x04, 0x74, 0x52, 0x4E,
    0x53, 0xFF, 0x00, 0xFF, 0x80, 0x13, 0x0A, 0x1E, 0x39, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x63, 0x10, 0x60, 0xD8, 0x00, 0x00, 0x00, 0xE4, 0x00, 0xC1, 0x27, 0xA8, 0xE8,
    0x57, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

constexpr std::uint32_t IMAGE_WIDTH = 16;
constexpr std::uint32_t IMAGE_HEIGHT = 16;

std::vector<std::uint8_t> RgbPng() {
    std::vector<std::uint8_t> rgb(IMAGE_WIDTH * IMAGE_HEIGHT * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>(10 + i * 13);
    return Decoder::Png::Encode(rgb, IMAGE_WIDTH, IMAGE_HEIGHT, 3);
}

void DecodeDirect(void* handle, const std::vector<std::uint8_t>& png, std::vector<std::uint8_t>& image, PngDecImageInfo& info) {
    PngDecDecodeParam param{};
    param.png_mem_addr = png.data();
    param.image_mem_addr = image.data();
    param.png_mem_size = static_cast<std::uint32_t>(png.size());
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.pixel_format = 0;
    param.alpha_value = 0x77;
    const std::int32_t result = scePngDecDecode(handle, &param, &info);
    Require(result == static_cast<std::int32_t>((IMAGE_WIDTH << 16) | IMAGE_HEIGHT));
}

void CheckGuestMemory(const std::vector<std::uint8_t>& png, const std::vector<std::uint8_t>& expectedImage) {
    constexpr auto invalidAddr = static_cast<std::int32_t>(0x80690001);
    constexpr auto invalidHandle = static_cast<std::int32_t>(0x80690004);
    TestDecoder decoder(IMAGE_WIDTH);
    Source source{&png};
    PngDecInputControlParam param{};
    param.input = &ReadChunk;
    param.input_argument = &source;
    param.operation = 0x10;
    PngDecImageInfo info{};
    GuardedPage parameters;
    for (const std::size_t readable : {std::size_t{0}, sizeof(param) - 8}) {
        auto* address = parameters.End(readable);
        if (readable != 0) std::memcpy(address, &param, readable);
        const auto* guestParam = reinterpret_cast<const PngDecInputControlParam*>(address);
        Require(scePngDecDecodeWithInputControl(nullptr, guestParam, &info) == invalidHandle);
        Require(scePngDecDecodeWithInputControl(decoder.handle, guestParam, &info) == invalidAddr);
        Require(source.calls == 0);
    }
    auto* guestParam = reinterpret_cast<PngDecInputControlParam*>(parameters.End(sizeof(param)));
    std::memcpy(guestParam, &param, sizeof(param));
    parameters.Protect(true);
    Require(scePngDecDecodeWithInputControl(decoder.handle, guestParam, &info) == 0);
    Require(info.image_width == IMAGE_WIDTH && info.image_height == IMAGE_HEIGHT);

    GuardedPage output;
    for (const std::size_t writable : {std::size_t{0}, sizeof(info) / 2}) {
        source.offset = 0;
        source.calls = 0;
        auto* guestInfo = reinterpret_cast<PngDecImageInfo*>(output.End(writable));
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, guestInfo) == invalidAddr);
        Require(source.calls > 0);
    }
    auto* guestInfo = reinterpret_cast<PngDecImageInfo*>(output.End(sizeof(info)));
    std::memset(guestInfo, 0xA5, sizeof(info));
    output.Protect(true);
    source.offset = 0;
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, guestInfo) == invalidAddr);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(guestInfo);
    Require(std::all_of(bytes, bytes + sizeof(info), [](auto byte) { return byte == 0xA5; }));

    source.offset = 0;
    source.errorAt = 0;
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, guestInfo) == -123);
    Require(std::all_of(bytes, bytes + sizeof(info), [](auto byte) { return byte == 0xA5; }));
    source.errorAt = -1;
    source.offset = 0;
    const std::vector<std::uint8_t> truncated(png.begin(), png.begin() + 10);
    source.png = &truncated;
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, guestInfo) == static_cast<std::int32_t>(0x80690010));
    Require(std::all_of(bytes, bytes + sizeof(info), [](auto byte) { return byte == 0xA5; }));

    output.Protect(false);
    source.png = &png;
    source.offset = 0;
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, guestInfo) == 0);
    Require(guestInfo->image_width == IMAGE_WIDTH && guestInfo->image_height == IMAGE_HEIGHT);
    Require(output.End(sizeof(info) + 1)[0] == 0xA5);
    std::vector<std::uint8_t> image(expectedImage.size(), 0);
    param.operation = 0x20;
    param.image_mem_addr = image.data();
    param.image_mem_size = static_cast<std::uint32_t>(image.size());
    param.alpha_value = 0x77;
    parameters.Protect(false);
    std::memcpy(guestParam, &param, sizeof(param));
    parameters.Protect(true);
    Require(scePngDecDecodeWithInputControl(decoder.handle, guestParam, guestInfo) == static_cast<std::int32_t>((IMAGE_WIDTH << 16) | IMAGE_HEIGHT));
    Require(image == expectedImage);
}

std::vector<std::uint8_t> WithBoundaryChunk(const std::vector<std::uint8_t>& png) {
    std::vector<std::uint8_t> data(2048, 0xA5);
    const std::uint8_t trailer[] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
    std::copy(std::begin(trailer), std::end(trailer), data.begin());
    std::vector<std::uint8_t> chunk;
    const auto append32 = [&chunk](std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) chunk.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    append32(static_cast<std::uint32_t>(data.size()));
    chunk.insert(chunk.end(), {'t', 'e', 'S', 't'});
    chunk.insert(chunk.end(), data.begin(), data.end());
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t offset = 4; offset < chunk.size(); ++offset) {
        crc ^= chunk[offset];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320u : 0u);
    }
    append32(~crc);
    std::vector<std::uint8_t> result;
    result.reserve(png.size() + chunk.size());
    result.insert(result.end(), png.begin(), png.end() - 12);
    result.insert(result.end(), chunk.begin(), chunk.end());
    result.insert(result.end(), png.end() - 12, png.end());
    return result;
}

void CheckImageBoundary(const std::vector<std::uint8_t>& png, std::size_t chunkSize, bool trailingBytes = false) {
    TestDecoder decoder(2);
    std::vector<std::uint8_t> input = png;
    if (trailingBytes) input.insert(input.end(), 29, 0xA5);
    Source source{&input};
    source.chunkSize = chunkSize;
    source.errorAt = static_cast<std::int32_t>(png.size());
    PngDecInputControlParam param{};
    param.input = &ReadChunk;
    param.input_argument = &source;
    param.operation = 0x10;
    PngDecImageInfo info{};
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == 0);
    Require(info.image_width == 2 && info.image_height == 2 && info.color_space == 4 && info.image_flag == 2);
    alignas(4) std::uint8_t image[16] = {};
    param.operation = 0x20;
    param.image_mem_addr = image;
    param.image_mem_size = sizeof(image);
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == (2 << 16 | 2));
    const std::uint8_t expected[] = {255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 128};
    Require(std::equal(std::begin(image), std::end(image), std::begin(expected)));
    Require(source.calls == (png.size() + chunkSize - 1) / chunkSize);
    const std::size_t calls = source.calls;
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == (2 << 16 | 2));
    Require(source.calls == calls);
    Require(std::equal(std::begin(image), std::end(image), std::begin(expected)));
}

void CheckIncompleteBoundary(const std::vector<std::uint8_t>& png, bool callbackError) {
    const std::vector<std::uint8_t> truncated(png.begin(), png.end() - 1);
    TestDecoder decoder(2);
    Source source{&truncated};
    source.chunkSize = 1;
    if (callbackError) source.errorAt = static_cast<std::int32_t>(truncated.size());
    PngDecInputControlParam param{};
    param.input = &ReadChunk;
    param.input_argument = &source;
    param.operation = 0x10;
    PngDecImageInfo info{};
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == 0);
    alignas(4) std::uint8_t image[16] = {};
    param.operation = 0x20;
    param.image_mem_addr = image;
    param.image_mem_size = sizeof(image);
    Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info)
        == (callbackError ? -123 : static_cast<std::int32_t>(0x80690012)));
}

}

int main() {
    const std::vector<std::uint8_t> boundaryPng(std::begin(PALETTE_TRNS), std::end(PALETTE_TRNS));
    for (const std::size_t chunkSize : {std::size_t{1}, std::size_t{3}, std::size_t{7}, std::size_t{12}, std::size_t{1024}}) {
        CheckImageBoundary(boundaryPng, chunkSize);
        CheckImageBoundary(WithBoundaryChunk(boundaryPng), chunkSize);
    }
    CheckImageBoundary(boundaryPng, boundaryPng.size() + 29, true);
    CheckIncompleteBoundary(boundaryPng, false);
    CheckIncompleteBoundary(boundaryPng, true);

    const std::vector<std::uint8_t> png = RgbPng();
    constexpr std::uint32_t width = IMAGE_WIDTH;
    constexpr std::uint32_t height = IMAGE_HEIGHT;

    std::vector<std::uint8_t> expectedImage(width * height * 4, 0);
    PngDecImageInfo expectedInfo{};
    {
        TestDecoder decoder(width);
        DecodeDirect(decoder.handle, png, expectedImage, expectedInfo);
    }
    Require(expectedInfo.image_width == width && expectedInfo.image_height == height);
    Require(expectedInfo.color_space == 3 && expectedInfo.bit_depth == 8 && expectedInfo.image_flag == 0);

    CheckGuestMemory(png, expectedImage);

    TestDecoder decoder(width);
    std::vector<std::uint8_t> image(width * height * 4, 0);
    PngDecImageInfo info{};

    {
        Source source{&png};
        PngDecInputControlParam param{};
        param.input = &ReadChunk;
        param.input_argument = &source;
        param.operation = 0x10;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == 0);
        Require(info.image_width == width && info.image_height == height);
        Require(info.color_space == 3 && info.bit_depth == 8 && info.image_flag == 0);
        Require(source.calls > 0);
        Require(source.offset < png.size());

        param.operation = 0x20;
        param.image_mem_addr = image.data();
        param.image_mem_size = static_cast<std::uint32_t>(image.size());
        param.pixel_format = 0;
        param.alpha_value = 0x77;
        PngDecImageInfo decodedInfo{};
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &decodedInfo) == static_cast<std::int32_t>((width << 16) | height));
        Require(decodedInfo.image_width == width && decodedInfo.image_height == height);
        Require(image == expectedImage);
    }

    {
        const std::vector<std::uint8_t> palette(std::begin(PALETTE_TRNS), std::end(PALETTE_TRNS));
        TestDecoder paletteDecoder(2);
        Source source{&palette};
        source.chunkSize = 3;
        PngDecInputControlParam param{};
        param.input = &ReadChunk;
        param.input_argument = &source;
        param.operation = 0x10;
        PngDecImageInfo paletteInfo{};
        Require(scePngDecDecodeWithInputControl(paletteDecoder.handle, &param, &paletteInfo) == 0);
        Require(paletteInfo.image_width == 2 && paletteInfo.image_height == 2);
        Require(paletteInfo.color_space == 4 && paletteInfo.bit_depth == 2);
        Require(paletteInfo.image_flag == 2);
    }

    {
        Source source{&png};
        source.errorAt = 10;
        PngDecInputControlParam param{};
        param.input = &ReadChunk;
        param.input_argument = &source;
        param.operation = 0x10;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == -123);
    }

    {
        const std::vector<std::uint8_t> truncated(png.begin(), png.begin() + 10);
        Source source{&truncated};
        PngDecInputControlParam param{};
        param.input = &ReadChunk;
        param.input_argument = &source;
        param.operation = 0x10;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690010));
    }

    {
        Source source{&png};
        PngDecInputControlParam param{};
        param.input = &ReadChunk;
        param.input_argument = &source;
        param.operation = 0x10;
        Require(scePngDecDecodeWithInputControl(decoder.handle, nullptr, &info) == static_cast<std::int32_t>(0x80690001));
        param.input = nullptr;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690001));
        param.input = &ReadChunk;
        param.operation = 0x30;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690003));
        param.operation = 0x10;
        param.reserved = 1;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690003));
        param.reserved = 0;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, nullptr) == static_cast<std::int32_t>(0x80690001));
        param.operation = 0x20;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690001));
        param.image_mem_addr = image.data() + 1;
        param.image_mem_size = static_cast<std::uint32_t>(image.size() - 1);
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690001));
        param.image_mem_addr = image.data();
        param.image_mem_size = 0;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690002));
        param.image_mem_size = static_cast<std::uint32_t>(image.size());
        param.pixel_format = 2;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690003));
        param.pixel_format = 0;
        param.alpha_value = 0x100;
        Require(scePngDecDecodeWithInputControl(decoder.handle, &param, &info) == static_cast<std::int32_t>(0x80690003));
    }

    alignas(16) std::uint8_t bogusHandle[16] = {};
    Require(scePngDecDecodeWithInputControl(bogusHandle, nullptr, nullptr) == static_cast<std::int32_t>(0x80690004));
}
