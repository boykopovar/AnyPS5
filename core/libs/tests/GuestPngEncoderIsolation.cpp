#include "prx/libScePngEnc/include/Api.hpp"
#include "Decoder/Png.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

struct Encoder {
    PngEncCreateParam create;
    std::vector<std::uint64_t> workspace;
    void* handle = nullptr;
    explicit Encoder(std::uint32_t width, std::uint32_t filters) : create{sizeof(create), 0, width, filters} {
        const int size = scePngEncQueryMemorySize(&create);
        Require(size > 0);
        workspace.resize((size + 7) / 8);
        Require(scePngEncCreate(&create, workspace.data(), size, &handle) == 0);
    }
    void Close() { Require(scePngEncDelete(handle) == 0); handle = nullptr; }
};

static void Encode(Encoder& encoder, std::uint32_t width, std::uint32_t height, std::uint8_t marker, std::uint16_t filter) {
    std::vector<std::uint8_t> pixels(width * height * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>(marker + i * 17);
    const auto before = pixels;
    std::vector<std::uint8_t> bytes(8194, 0xa5);
    PngEncEncodeParam parameters{pixels.data(), bytes.data() + 1, static_cast<std::uint32_t>(pixels.size()), 8192,
                                width, height, width * 4, 0, 19, 8, 0, filter, static_cast<std::uint16_t>(marker % 10)};
    PngEncOutputInfo info{};
    const int size = scePngEncEncode(encoder.handle, &parameters, &info);
    Require(size > 0 && size <= 8192 && info.data_size == static_cast<unsigned>(size) && info.processed_height == height);
    const auto decoded = Decoder::Png::Decode(std::span<const std::uint8_t>(bytes.data() + 1, size));
    Require(decoded && decoded->width == width && decoded->height == height && decoded->pixels == pixels);
    Require(before == pixels && bytes.front() == 0xa5 && bytes.back() == 0xa5);
    Require(std::all_of(bytes.begin() + 1 + size, bytes.end(), [](auto byte) { return byte == 0xa5; }));
}

int main() {
    Encoder one(16, 0), two(32, 4);
    Require(one.handle != two.handle);
    for (unsigned round = 0; round < 8; ++round) {
        Encode(one, 5, 3, static_cast<std::uint8_t>(10 + round), 0);
        Encode(two, 19, 7, static_cast<std::uint8_t>(40 + round), 15);
    }
    one.Close();
    Encode(two, 23, 2, 72, 3);
    two.Close();
}
