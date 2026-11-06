#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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

std::vector<std::uint8_t> LargeSetup(std::size_t size, std::uint16_t part1) {
    std::vector<std::uint8_t> setup(size, 0x5A);
    std::memcpy(setup.data(), Setup.data(), 7);
    setup[7] = static_cast<std::uint8_t>(part1 & 0xFF);
    setup[8] = static_cast<std::uint8_t>(part1 >> 8);
    return setup;
}

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
    std::vector<std::uint8_t> longRemainder = LargeSetup(0x10009, 9);
    std::vector<std::uint8_t> longRemainderOutput(56 + longRemainder.size());
    param = Valid(longRemainderOutput.data(), longRemainderOutput.size());
    param.setupPacket = longRemainder.data();
    param.setupPacketSize = longRemainder.size();
    Require(Create(param) == InvalidSetup, "a setup remainder that does not fit the 16-bit length field is refused");
    auto shortSetup = Setup;
    shortSetup[7] = 13;
    param = Valid(output.data(), output.size());
    param.setupPacket = shortSetup.data();
    Require(Create(param) == InvalidSetup, "a first setup part longer than the packet is refused");
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

void TestLargePacket() {
    const std::vector<std::uint8_t> setup = LargeSetup(0x10000, 0x8000);
    std::vector<std::uint8_t> output(56 + setup.size() + 1, 0xEE);
    auto param = Valid(output.data(), output.size());
    param.setupPacket = setup.data();
    param.setupPacketSize = setup.size();
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == 0, "a 65536-byte setup packet whose two parts each fit a 16-bit length is built");
    Require(param.outputSize == 56 + setup.size(), "the written size covers the table, both headers and the large setup packet");
    constexpr std::uint8_t Lengths[] {0x00, 0x80, 0x00, 0x80};
    Require(std::memcmp(output.data() + 6, Lengths, sizeof(Lengths)) == 0, "both setup lengths are 32768");
    Require(std::memcmp(output.data() + 56, setup.data(), setup.size()) == 0, "the large setup packet follows unchanged");
    Require(output[56 + setup.size()] == 0xEE, "nothing is written past the large packet");
}

}

int main() {
    TestValidation();
    TestPacket();
    TestLargePacket();
    std::puts("guest ajm wvorbis header packet: PASS");
}
