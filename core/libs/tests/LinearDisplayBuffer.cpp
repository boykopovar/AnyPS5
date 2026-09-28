#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

static void Require(bool condition) { if (!condition) std::abort(); }
template<typename TAction> static void Reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}

int main() {
    AgcDriver::DisplayBuffer buffer{65536, 0x8000000000000000ull, 2, 2, 1, 3};
    const std::array<unsigned char, 24> input{1,2,3,4, 5,6,7,8, 99,99,99,99,
        9,10,11,12, 13,14,15,16, 99,99,99,99};
    const auto source = std::as_bytes(std::span(input));
    Require(AgcDriver::DisplayBufferSize(buffer) == input.size());
    auto pixels = AgcDriver::DecodeDisplayBuffer(buffer, source);
    Require(pixels.size() == 16);
    for (unsigned i = 0; i < pixels.size(); ++i) Require(std::to_integer<unsigned>(pixels[i]) == i + 1);
    buffer.pixelFormat = 0x8000000022000000ull;
    pixels = AgcDriver::DecodeDisplayBuffer(buffer, source);
    const std::array<unsigned char, 16> swapped{3,2,1,4, 7,6,5,8, 11,10,9,12, 15,14,13,16};
    for (unsigned i = 0; i < pixels.size(); ++i) Require(std::to_integer<unsigned>(pixels[i]) == swapped[i]);
    Reject([&] { AgcDriver::DecodeDisplayBuffer(buffer, source.first(23)); });
    buffer.pitchInPixel = 1;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.pitchInPixel = 0;
    Require(AgcDriver::DisplayBufferSize(buffer) == 16);
    buffer.tilingMode = 2;
    Reject([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.tilingMode = 0;
    Require(AgcDriver::DisplayBufferSize(buffer) == 65536);
}
