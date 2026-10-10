#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>

extern "C" {
int APS5_VABI sceRandomGetRandomNumber(void*, std::size_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalid = static_cast<int>(0x817C0016);
constexpr unsigned char filler = 0xAA;

std::array<unsigned char, 80> FilledBuffer() {
    std::array<unsigned char, 80> buffer{};
    buffer.fill(filler);
    return buffer;
}

const Case nullBuffer{"GetRandomNumber_NullBuffer_ReturnsInvalid", [] {
    RequireEqual(sceRandomGetRandomNumber(nullptr, 16), invalid, "null buffer");
}};

const Case oversizedRequest{"GetRandomNumber_SizeAbove64_ReturnsInvalidAndLeavesBufferUntouched", [] {
    auto buffer = FilledBuffer();
    RequireEqual(sceRandomGetRandomNumber(buffer.data(), 65), invalid, "65 byte request");
    RequireEqual(buffer[0], filler, "first byte untouched");
}};

const Case zeroSize{"GetRandomNumber_ZeroSize_SucceedsWithoutWriting", [] {
    auto buffer = FilledBuffer();
    RequireEqual(sceRandomGetRandomNumber(buffer.data(), 0), 0, "zero byte request");
    RequireEqual(buffer[0], filler, "first byte untouched");
}};

const Case partialRequest{"GetRandomNumber_SevenBytes_LeavesFollowingByteUntouched", [] {
    auto buffer = FilledBuffer();
    RequireEqual(sceRandomGetRandomNumber(buffer.data(), 7), 0, "seven byte request");
    RequireEqual(buffer[7], filler, "byte after the requested range");
}};

const Case distinctResults{"GetRandomNumber_TwoFullRequests_ProduceDifferentBytes", [] {
    std::array<unsigned char, 64> first{};
    std::array<unsigned char, 64> second{};
    RequireEqual(sceRandomGetRandomNumber(first.data(), first.size()), 0, "first request");
    RequireEqual(sceRandomGetRandomNumber(second.data(), second.size()), 0, "second request");
    Require(first != second, "two 64 byte requests returned identical bytes");
}};

} // namespace
