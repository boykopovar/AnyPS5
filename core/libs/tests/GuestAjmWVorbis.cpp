#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(WVorbisHeaderPacketParameter) == 0x48);

namespace {

constexpr int InvalidParameter = static_cast<int>(0x8A6E0001);
constexpr int InvalidSize = static_cast<int>(0x8A6E0002);
constexpr int OutputTooSmall = static_cast<int>(0x8A6E0003);
constexpr int InvalidSetup = static_cast<int>(0x8A6E0004);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

constexpr std::array<std::uint8_t, 12> Setup {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};

WVorbisHeaderPacketParameter Valid(std::uint8_t* output, std::size_t outputSize) {
    return {sizeof(WVorbisHeaderPacketParameter), 2, 48000, 16000, 8, 11, 0, Setup.data(), Setup.size(), output, outputSize, 0};
}

int Create(WVorbisHeaderPacketParameter param) { return sceAjmDecWVorbisCreateHeaderPacket(&param); }

void TestValidation() {
    std::array<std::uint8_t, 128> output {};
    Require(sceAjmDecWVorbisCreateHeaderPacket(nullptr) == InvalidParameter, "a null parameter is refused");
    auto param = Valid(output.data(), output.size());
    param.size = 0x40;
    Require(Create(param) == InvalidSize, "a wrong structure size is refused");
    param = Valid(output.data(), output.size());
    param.bytesPerSecond = -1;
    Require(Create(param) == InvalidParameter, "a negative byte rate is refused");
    param = Valid(output.data(), output.size());
    param.shortBlockExponent = 5;
    Require(Create(param) == InvalidParameter, "a block size below 64 is refused");
    param.shortBlockExponent = 38;
    param.longBlockExponent = 38;
    Require(Create(param) == 0, "block size exponents are taken modulo 32 as the console's shift does");
    param = Valid(output.data(), output.size());
    param.longBlockExponent = 7;
    Require(Create(param) == InvalidParameter, "block size 1 below block size 0 is refused");
    param.longBlockExponent = 14;
    Require(Create(param) == InvalidParameter, "a block size above 8192 is refused");
    param = Valid(output.data(), output.size());
    param.setupPacket = nullptr;
    param.setupPacketSize = 0;
    Require(Create(param) == InvalidParameter, "a null setup packet is refused before its size");
    param = Valid(output.data(), output.size());
    param.setupPacketSize = 8;
    Require(Create(param) == InvalidSetup, "a setup packet below 9 bytes is refused");
    param = Valid(nullptr, 0);
    Require(Create(param) == InvalidParameter, "a null output is refused before its size");
    param = Valid(output.data(), 9);
    Require(Create(param) == OutputTooSmall, "an output below 10 bytes is refused");
    param = Valid(output.data(), 56 + Setup.size() - 1);
    Require(Create(param) == OutputTooSmall, "an output that cannot hold the setup packet is refused");
}

void TestPacket() {
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    auto param = Valid(output.data(), output.size());
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == 0, "a valid header packet is built");
    Require(param.outputSize == 56 + Setup.size(), "the written size covers the table, both headers and the setup packet");
    constexpr std::uint8_t Expected[] {
        0, 0, 30, 0, 16, 0, 9, 0, 3, 0,
        1, 'v', 'o', 'r', 'b', 'i', 's', 0, 0, 0, 0, 2, 0x80, 0xBB, 0, 0, 0, 0, 0, 0, 0x00, 0xF4, 0x01, 0x00, 0, 0, 0, 0, 0xB8, 1,
        3, 'v', 'o', 'r', 'b', 'i', 's', 0, 0, 0, 0, 0, 0, 0, 0, 1,
    };
    static_assert(sizeof(Expected) == 56);
    Require(std::memcmp(output.data(), Expected, sizeof(Expected)) == 0, "the table, identification and comment headers match");
    Require(std::memcmp(output.data() + 56, Setup.data(), Setup.size()) == 0, "the setup packet follows unchanged");
    Require(output[56 + Setup.size()] == 0xEE, "nothing is written past the packet");
}

}

int main() {
    TestValidation();
    TestPacket();
    std::puts("guest ajm wvorbis header packet: PASS");
}
