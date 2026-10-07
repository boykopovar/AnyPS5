#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

using AgcDriver::DisplayBuffer;
using AgcDriver::Graphics::DccKeys;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template <typename TAction> std::string Rejection(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

void ExpectPixels(const DisplayBuffer& buffer, DccKeys keys, std::array<unsigned, 4> expected, const char* what) {
    const auto pixel = AgcDriver::DisplayBufferClearPixel(buffer, keys);
    for (std::size_t i = 0; i < pixel.size(); ++i) {
        if (std::to_integer<unsigned>(pixel[i]) == expected[i])
            continue;
        Expect(false, std::string(what) + ": byte " + std::to_string(i) + " is " +
                          std::to_string(std::to_integer<unsigned>(pixel[i])) + ", expected " +
                          std::to_string(expected[i]));
        return;
    }
}

}

int main() {
    constexpr std::uint64_t bgra = 0x8000000000000000ull;
    constexpr std::uint64_t rgba = 0x8000000022000000ull;
    constexpr std::uint64_t tenBit = 0x0100000000000000ull;
    DisplayBuffer buffer{65536, bgra, 131, 3, 0, 0, 0x7f0000, 0x11223344};
    ExpectPixels(buffer, DccKeys::Clear0000, {0, 0, 0, 0}, "8-bit 0000");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "8-bit 0001");
    ExpectPixels(buffer, DccKeys::Clear1110, {255, 255, 255, 0}, "8-bit 1110");
    ExpectPixels(buffer, DccKeys::Clear1111, {255, 255, 255, 255}, "8-bit 1111");
    ExpectPixels(buffer, DccKeys::ClearRegister, {0x44, 0x33, 0x22, 0x11}, "B8G8R8A8 register clear");
    buffer.pixelFormat = rgba;
    ExpectPixels(buffer, DccKeys::ClearRegister, {0x22, 0x33, 0x44, 0x11}, "R8G8B8A8 register clear");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "R8G8B8A8 0001");
    buffer.pixelFormat = rgba | tenBit;
    ExpectPixels(buffer, DccKeys::Clear1110, {255, 255, 255, 255}, "A2B10G10R10 1110");
    ExpectPixels(buffer, DccKeys::Clear0001, {0, 0, 0, 255}, "A2B10G10R10 0001");
    buffer.dccClearColor = 0x3ff;
    ExpectPixels(buffer, DccKeys::ClearRegister, {0, 0, 255, 255}, "A2B10G10R10 register clear");
    buffer.pixelFormat = bgra | tenBit;
    ExpectPixels(buffer, DccKeys::ClearRegister, {255, 0, 0, 255}, "A2R10G10B10 register clear");
    for (const auto keys : {DccKeys::Uncompressed, DccKeys::Mixed, DccKeys::Unreadable}) {
        const auto message = Rejection([&] { AgcDriver::DisplayBufferClearPixel(buffer, keys); });
        Expect(message.find(AgcDriver::Graphics::DccKeysName(keys)) != std::string::npos &&
                   message.find("0x7f0000") != std::string::npos,
               std::string("keys without a clear value were presented as one: ") +
                   AgcDriver::Graphics::DccKeysName(keys));
    }
    buffer.dccClearColor = 0x100000000ull;
    Expect(Rejection([&] {
               AgcDriver::DisplayBufferClearPixel(buffer, DccKeys::ClearRegister);
           }).find("register clear color") != std::string::npos,
           "a register clear color wider than the texel was truncated");
    buffer.dccClearColor = 0;
    buffer.dccAddress = 0;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferClearPixel(buffer, DccKeys::Clear0000); }).empty(),
           "a buffer without DCC metadata was presented as fast-cleared");
    buffer.dccClearColor = 1;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferSize(buffer); }).empty(),
           "a DCC clear color without metadata was accepted");
    buffer.dccClearColor = 0;
    buffer.dccAddress = 0x7f0000;
    buffer.tilingMode = 1;
    Expect(!Rejection([&] { AgcDriver::DisplayBufferSize(buffer); }).empty(),
           "a linear display buffer with DCC metadata was accepted");
    buffer.tilingMode = 0;
    Expect(AgcDriver::DisplayBufferSize(buffer) == 2 * 65536, "DCC metadata changed the display footprint");
    if (failures != 0)
        return 1;
    std::puts("display buffer DCC tests passed");
    return 0;
}
