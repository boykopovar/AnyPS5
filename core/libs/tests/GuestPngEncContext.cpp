#include "prx/libScePngEnc/include/Api.hpp"
#include "Decoder/Png.hpp"
#include <cstdint>
#include <cstdlib>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

struct PngEncContextMirror {
    std::uintptr_t self;
    std::uint32_t size;
    std::uint32_t reserved;
    std::uint32_t maxImageWidth;
    std::uint32_t maxFilterNumber;
    std::uint32_t scratchSize;
};

int main() {
    constexpr int invalidParam = static_cast<int>(0x80690103);
    constexpr int invalidHandle = static_cast<int>(0x80690104);

    for (const std::uint32_t width : {1u, 3u, 7u, 9u}) {
        for (std::uint32_t filterNumber = 0; filterNumber <= 4; ++filterNumber) {
            PngEncCreateParam create{sizeof(PngEncCreateParam), 0, width, filterNumber};
            const int memorySize = scePngEncQueryMemorySize(&create);
            std::vector<std::uint8_t> memory(memorySize + 16);
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(memory.data()) + 5;
            void* handle = nullptr;
            Require(scePngEncCreate(&create, reinterpret_cast<void*>(address), static_cast<std::uint32_t>(memory.size()), &handle) == 0);
            const std::uintptr_t aligned = (address + 15) & ~std::uintptr_t{15};
            const auto* context = reinterpret_cast<const PngEncContextMirror*>(handle);
            Require(context->self == aligned);
            Require(context->size == 16u);
            Require(context->reserved == 0u);
            Require(context->maxImageWidth == ((width + 7) & ~std::uint32_t{7}));
            Require(context->maxFilterNumber == filterNumber);
            Require(context->scratchSize == static_cast<std::uint32_t>(memorySize) - 112u);
            Require(scePngEncDelete(handle) == 0);
            Require(scePngEncDelete(handle) == invalidHandle);
        }
    }

    PngEncCreateParam narrow{sizeof(PngEncCreateParam), 0, 1u, 4};
    const int narrowSize = scePngEncQueryMemorySize(&narrow);
    std::vector<std::uint8_t> narrowMemory(narrowSize + 16);
    void* narrowHandle = nullptr;
    Require(scePngEncCreate(&narrow, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(narrowMemory.data()) + 5), static_cast<std::uint32_t>(narrowMemory.size()), &narrowHandle) == 0);

    constexpr std::uint32_t rows = 4;
    std::vector<std::uint8_t> image(rows * 36, 0);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            std::uint8_t* pixel = &image[y * 36 + x * 4];
            pixel[0] = static_cast<std::uint8_t>(x * 40);
            pixel[1] = static_cast<std::uint8_t>(y * 80);
            pixel[2] = static_cast<std::uint8_t>(200 - x * 10);
            pixel[3] = static_cast<std::uint8_t>(100 + x + y);
        }
    }
    std::vector<std::uint8_t> png(4096);
    PngEncEncodeParam encode{image.data(), png.data(), static_cast<std::uint32_t>(image.size()), static_cast<std::uint32_t>(png.size()),
                             8, rows, 36u, 0, 19, 8, 0, 15, 6};
    PngEncOutputInfo info{};
    const int encoded = scePngEncEncode(narrowHandle, &encode, &info);
    Require(encoded > 1 && static_cast<std::size_t>(encoded) < png.size()
            && info.data_size == static_cast<std::uint32_t>(encoded) && info.processed_height == rows);
    const auto decoded = Decoder::Png::Decode(std::span<const std::uint8_t>(png.data(), encoded));
    Require(decoded.has_value() && decoded->width == 8u && decoded->height == rows);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const std::uint8_t* source = &image[y * 36 + x * 4];
            const std::uint8_t* result = &decoded->pixels[(y * 8u + x) * 4u];
            Require(result[0] == source[0] && result[1] == source[1]);
            Require(result[2] == source[2] && result[3] == source[3]);
        }
    }
    encode.image_width = 9;
    Require(scePngEncEncode(narrowHandle, &encode, &info) == invalidParam);
    Require(scePngEncDelete(narrowHandle) == 0);

    PngEncCreateParam tall{sizeof(PngEncCreateParam), 0, 8u, 4};
    const int tallSize = scePngEncQueryMemorySize(&tall);
    std::vector<std::uint8_t> tallMemory(tallSize + 16);
    void* tallHandle = nullptr;
    Require(scePngEncCreate(&tall, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(tallMemory.data()) + 5), static_cast<std::uint32_t>(tallMemory.size()), &tallHandle) == 0);

    constexpr std::uint32_t maxHeight = 1000000;
    const std::uint8_t channel[4] = {0x11, 0x22, 0x33, 0xFF};
    std::vector<std::uint8_t> tallImage(maxHeight * 32);
    for (std::size_t i = 0; i < tallImage.size(); ++i) tallImage[i] = channel[i % 4];
    std::vector<std::uint8_t> tallPng(4u * 1024u * 1024u);
    PngEncEncodeParam tallEncode{tallImage.data(), tallPng.data(), static_cast<std::uint32_t>(tallImage.size()), static_cast<std::uint32_t>(tallPng.size()),
                                 8, maxHeight, 32u, 0, 19, 8, 0, 0, 6};
    PngEncOutputInfo tallInfo{};
    const int tallEncoded = scePngEncEncode(tallHandle, &tallEncode, &tallInfo);
    Require(tallEncoded > 1 && static_cast<std::size_t>(tallEncoded) < tallPng.size()
            && tallInfo.data_size == static_cast<std::uint32_t>(tallEncoded) && tallInfo.processed_height == maxHeight);
    const auto tallDecoded = Decoder::Png::Decode(std::span<const std::uint8_t>(tallPng.data(), tallEncoded));
    Require(tallDecoded.has_value() && tallDecoded->width == 8u && tallDecoded->height == maxHeight);
    for (std::size_t i = 0; i < tallImage.size(); ++i) {
        Require(tallDecoded->pixels[i] == channel[i % 4]);
    }
    Require(scePngEncDelete(tallHandle) == 0);
}
