#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::HexResult;
using Jsc::WtfAbi::LChar;
using Jsc::WtfAbi::MediaTime;

namespace {

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::string SixCharacterHash(unsigned hash) {
    auto result = _ZN3WTF31integerToSixCharacterHashStringEj(hash);
    if (result[6] != '\0') return "unterminated";
    return std::string(result.data());
}

void TestSixCharacterHash() {
    Require(SixCharacterHash(0) == "AAAAAA");
    Require(SixCharacterHash(1) == "AAAAAB");
    Require(SixCharacterHash(61) == "AAAAA9");
    Require(SixCharacterHash(62) == "AAAABA");
    Require(SixCharacterHash(916132832) == "BAAAAA");
    Require(SixCharacterHash(0x12345678u) == "AUpfpW");
    Require(SixCharacterHash(0xFFFFFFFFu) == "EqpPMD");
}

struct HexBuffer {
    LChar bytes[24];
};

HexBuffer FreshBuffer() {
    HexBuffer buffer;
    std::memset(buffer.bytes, '#', sizeof(buffer.bytes));
    return buffer;
}

std::string Text(const HexResult& result) {
    return std::string(reinterpret_cast<const char*>(result.first), result.second);
}

void TestAppendHex() {
    auto buffer = FreshBuffer();
    auto result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0x1A2B, 0, Jsc::WtfAbi::Uppercase);
    Require(result.first == buffer.bytes + 16 && result.second == 4);
    Require(Text(result) == "1A2B");
    Require(buffer.bytes[15] == '#' && buffer.bytes[20] == '#');

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0xABCDEF, 0, Jsc::WtfAbi::Lowercase);
    Require(Text(result) == "abcdef" && result.first == buffer.bytes + 14);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0, 0, Jsc::WtfAbi::Uppercase);
    Require(Text(result) == "0" && result.first == buffer.bytes + 19);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0x1a2b, 8, Jsc::WtfAbi::Lowercase);
    Require(Text(result) == "00001a2b" && result.first == buffer.bytes + 12);
    Require(buffer.bytes[11] == '#');

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0xABC, 3, Jsc::WtfAbi::Uppercase);
    Require(Text(result) == "ABC");

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0x1A2B, 100, Jsc::WtfAbi::Uppercase);
    Require(Text(result) == "0000000000001A2B" && result.first == buffer.bytes + 4);
    Require(buffer.bytes[3] == '#' && buffer.bytes[20] == '#');

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, std::numeric_limits<std::uintmax_t>::max(), 0, Jsc::WtfAbi::Lowercase);
    Require(Text(result) == "ffffffffffffffff" && result.first == buffer.bytes + 4);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 4, 0xBEEF, 0, static_cast<Jsc::WtfAbi::HexConversionMode>(7));
    Require(Text(result) == "BEEF" && result.first == buffer.bytes + 4);
    Require(buffer.bytes[3] == '#' && buffer.bytes[8] == '#');

    buffer = FreshBuffer();
    Require(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(nullptr, 16, 1, 0, Jsc::WtfAbi::Uppercase); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 3, 0xBEEF, 0, Jsc::WtfAbi::Uppercase); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 0, 0, 0, Jsc::WtfAbi::Uppercase); }));
    for (auto byte : buffer.bytes) Require(byte == '#');
}

MediaTime Time(std::int64_t value, std::uint32_t scale, std::uint8_t flags = MediaTime::Valid) {
    MediaTime time{};
    time.timeValue = value;
    time.timeScale = scale;
    time.timeFlags = flags;
    return time;
}

MediaTime DoubleTime(double value) {
    MediaTime time{};
    time.timeValueAsDouble = value;
    time.timeScale = 1;
    time.timeFlags = MediaTime::Valid | MediaTime::DoubleValue;
    return time;
}

bool NotEqual(const MediaTime& lhs, const MediaTime& rhs) {
    return _ZNK3WTF9MediaTimeneERKS0_(&lhs, &rhs);
}

void TestMediaTime() {
    constexpr auto PositiveInfinite = MediaTime::Valid | MediaTime::PositiveInfinite;
    constexpr auto NegativeInfinite = MediaTime::Valid | MediaTime::NegativeInfinite;
    constexpr auto Indefinite = MediaTime::Valid | MediaTime::Indefinite;

    Require(!NotEqual(Time(1, 2), Time(1, 2)));
    Require(!NotEqual(Time(1, 2), Time(2, 4)));
    Require(!NotEqual(Time(2, 4), Time(1, 2)));
    Require(!NotEqual(Time(-1, 2), Time(-500, 1000)));
    Require(!NotEqual(Time(0, 1), Time(0, 1000)));
    Require(!NotEqual(Time(1, 2), Time(1, 2, MediaTime::Valid | MediaTime::HasBeenRounded)));
    Require(NotEqual(Time(1, 2), Time(1, 3)));
    Require(NotEqual(Time(1, 3), Time(1, 2)));
    Require(NotEqual(Time(1, 2), Time(2, 2)));
    Require(NotEqual(Time(-1, 2), Time(1, 2)));
    Require(NotEqual(Time(-1, 2), Time(-1, 3)));
    Require(NotEqual(Time(3, 1000), Time(1, 1001)));
    Require(NotEqual(Time(1001, 1000), Time(1000, 1001)));

    Require(!NotEqual(Time(5, 1, 0), Time(-7, 3, 0)));
    Require(NotEqual(Time(1, 2, 0), Time(1, 2)));
    Require(NotEqual(Time(1, 2), Time(1, 2, 0)));

    Require(!NotEqual(Time(1, 1, PositiveInfinite), Time(-9, 7, PositiveInfinite)));
    Require(!NotEqual(Time(1, 1, NegativeInfinite), Time(4, 2, NegativeInfinite)));
    Require(!NotEqual(Time(0, 1, Indefinite), Time(3, 5, Indefinite)));
    Require(NotEqual(Time(0, 1, PositiveInfinite), Time(0, 1, NegativeInfinite)));
    Require(NotEqual(Time(0, 1, PositiveInfinite), Time(0, 1)));
    Require(NotEqual(Time(0, 1), Time(0, 1, NegativeInfinite)));
    Require(NotEqual(Time(0, 1, Indefinite), Time(0, 1, PositiveInfinite)));
    Require(NotEqual(Time(0, 1, Indefinite), Time(0, 1)));

    Require(!NotEqual(DoubleTime(0.5), DoubleTime(0.5)));
    Require(NotEqual(DoubleTime(0.5), DoubleTime(0.25)));
    Require(!NotEqual(DoubleTime(0.5), Time(1, 2)));
    Require(!NotEqual(Time(3, 4), DoubleTime(0.75)));
    Require(NotEqual(DoubleTime(0.5), Time(1, 3)));

    constexpr std::int64_t Large = 4000000000000000000;
    Require(!NotEqual(Time(Large, 2000000000u), Time(2 * Large, 4000000000u)));
    Require(NotEqual(Time(Large, 2000000000u), Time(2 * Large + 1, 4000000000u)));
    Require(NotEqual(Time(Large + 1, 2000000000u), Time(2 * Large + 1, 4000000000u)));
    Require(!NotEqual(Time(Large + 1, 2000000000u), Time(2 * Large + 2, 4000000000u)));
    Require(NotEqual(Time(Large, 2000000000u), Time(2 * Large + 4000000000, 4000000000u)));
    Require(!NotEqual(Time(-Large, 2000000000u), Time(-2 * Large, 4000000000u)));
    Require(NotEqual(Time(-Large, 2000000000u), Time(-2 * Large - 1, 4000000000u)));

    auto time = Time(1, 2);
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF9MediaTimeneERKS0_(nullptr, &time); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF9MediaTimeneERKS0_(&time, nullptr); }));
    Require(ThrowsInvalidArgument([] { NotEqual(Time(std::int64_t{1} << 62, 0), Time((std::int64_t{1} << 62) + 1, 4)); }));
}

} // namespace

int main() {
    TestSixCharacterHash();
    TestAppendHex();
    TestMediaTime();
    return 0;
}
