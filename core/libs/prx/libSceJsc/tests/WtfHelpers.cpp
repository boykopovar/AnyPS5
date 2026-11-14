#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

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

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF helper check failed at line %d\n", line);
        std::abort();
    }
}

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
    Check(SixCharacterHash(0) == "AAAAAA", __LINE__);
    Check(SixCharacterHash(1) == "AAAAAB", __LINE__);
    Check(SixCharacterHash(61) == "AAAAA9", __LINE__);
    Check(SixCharacterHash(62) == "AAAABA", __LINE__);
    Check(SixCharacterHash(916132832) == "BAAAAA", __LINE__);
    Check(SixCharacterHash(0x12345678u) == "AUpfpW", __LINE__);
    Check(SixCharacterHash(0xFFFFFFFFu) == "EqpPMD", __LINE__);
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
    Check(result.first == buffer.bytes + 16 && result.second == 4, __LINE__);
    Check(Text(result) == "1A2B", __LINE__);
    Check(buffer.bytes[15] == '#' && buffer.bytes[20] == '#', __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0xABCDEF, 0, Jsc::WtfAbi::Lowercase);
    Check(Text(result) == "abcdef" && result.first == buffer.bytes + 14, __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0, 0, Jsc::WtfAbi::Uppercase);
    Check(Text(result) == "0" && result.first == buffer.bytes + 19, __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0x1a2b, 8, Jsc::WtfAbi::Lowercase);
    Check(Text(result) == "00001a2b" && result.first == buffer.bytes + 12, __LINE__);
    Check(buffer.bytes[11] == '#', __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0xABC, 3, Jsc::WtfAbi::Uppercase);
    Check(Text(result) == "ABC", __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, 0x1A2B, 100, Jsc::WtfAbi::Uppercase);
    Check(Text(result) == "0000000000001A2B" && result.first == buffer.bytes + 4, __LINE__);
    Check(buffer.bytes[3] == '#' && buffer.bytes[20] == '#', __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 16, std::numeric_limits<std::uintmax_t>::max(), 0, Jsc::WtfAbi::Lowercase);
    Check(Text(result) == "ffffffffffffffff" && result.first == buffer.bytes + 4, __LINE__);

    buffer = FreshBuffer();
    result = _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 4, 0xBEEF, 0, static_cast<Jsc::WtfAbi::HexConversionMode>(7));
    Check(Text(result) == "BEEF" && result.first == buffer.bytes + 4, __LINE__);
    Check(buffer.bytes[3] == '#' && buffer.bytes[8] == '#', __LINE__);

    buffer = FreshBuffer();
    Check(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(nullptr, 16, 1, 0, Jsc::WtfAbi::Uppercase); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 3, 0xBEEF, 0, Jsc::WtfAbi::Uppercase); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(buffer.bytes + 4, 0, 0, 0, Jsc::WtfAbi::Uppercase); }), __LINE__);
    for (auto byte : buffer.bytes) Check(byte == '#', __LINE__);
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

    Check(!NotEqual(Time(1, 2), Time(1, 2)), __LINE__);
    Check(!NotEqual(Time(1, 2), Time(2, 4)), __LINE__);
    Check(!NotEqual(Time(2, 4), Time(1, 2)), __LINE__);
    Check(!NotEqual(Time(-1, 2), Time(-500, 1000)), __LINE__);
    Check(!NotEqual(Time(0, 1), Time(0, 1000)), __LINE__);
    Check(!NotEqual(Time(1, 2), Time(1, 2, MediaTime::Valid | MediaTime::HasBeenRounded)), __LINE__);
    Check(NotEqual(Time(1, 2), Time(1, 3)), __LINE__);
    Check(NotEqual(Time(1, 3), Time(1, 2)), __LINE__);
    Check(NotEqual(Time(1, 2), Time(2, 2)), __LINE__);
    Check(NotEqual(Time(-1, 2), Time(1, 2)), __LINE__);
    Check(NotEqual(Time(-1, 2), Time(-1, 3)), __LINE__);
    Check(NotEqual(Time(3, 1000), Time(1, 1001)), __LINE__);
    Check(NotEqual(Time(1001, 1000), Time(1000, 1001)), __LINE__);

    Check(!NotEqual(Time(5, 1, 0), Time(-7, 3, 0)), __LINE__);
    Check(NotEqual(Time(1, 2, 0), Time(1, 2)), __LINE__);
    Check(NotEqual(Time(1, 2), Time(1, 2, 0)), __LINE__);

    Check(!NotEqual(Time(1, 1, PositiveInfinite), Time(-9, 7, PositiveInfinite)), __LINE__);
    Check(!NotEqual(Time(1, 1, NegativeInfinite), Time(4, 2, NegativeInfinite)), __LINE__);
    Check(!NotEqual(Time(0, 1, Indefinite), Time(3, 5, Indefinite)), __LINE__);
    Check(NotEqual(Time(0, 1, PositiveInfinite), Time(0, 1, NegativeInfinite)), __LINE__);
    Check(NotEqual(Time(0, 1, PositiveInfinite), Time(0, 1)), __LINE__);
    Check(NotEqual(Time(0, 1), Time(0, 1, NegativeInfinite)), __LINE__);
    Check(NotEqual(Time(0, 1, Indefinite), Time(0, 1, PositiveInfinite)), __LINE__);
    Check(NotEqual(Time(0, 1, Indefinite), Time(0, 1)), __LINE__);

    Check(!NotEqual(DoubleTime(0.5), DoubleTime(0.5)), __LINE__);
    Check(NotEqual(DoubleTime(0.5), DoubleTime(0.25)), __LINE__);
    Check(!NotEqual(DoubleTime(0.5), Time(1, 2)), __LINE__);
    Check(!NotEqual(Time(3, 4), DoubleTime(0.75)), __LINE__);
    Check(NotEqual(DoubleTime(0.5), Time(1, 3)), __LINE__);

    constexpr std::int64_t Large = 4000000000000000000;
    Check(!NotEqual(Time(Large, 2000000000u), Time(2 * Large, 4000000000u)), __LINE__);
    Check(NotEqual(Time(Large, 2000000000u), Time(2 * Large + 1, 4000000000u)), __LINE__);
    Check(NotEqual(Time(Large + 1, 2000000000u), Time(2 * Large + 1, 4000000000u)), __LINE__);
    Check(!NotEqual(Time(Large + 1, 2000000000u), Time(2 * Large + 2, 4000000000u)), __LINE__);
    Check(NotEqual(Time(Large, 2000000000u), Time(2 * Large + 4000000000, 4000000000u)), __LINE__);
    Check(!NotEqual(Time(-Large, 2000000000u), Time(-2 * Large, 4000000000u)), __LINE__);
    Check(NotEqual(Time(-Large, 2000000000u), Time(-2 * Large - 1, 4000000000u)), __LINE__);

    auto time = Time(1, 2);
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF9MediaTimeneERKS0_(nullptr, &time); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF9MediaTimeneERKS0_(&time, nullptr); }), __LINE__);
    Check(ThrowsInvalidArgument([] { NotEqual(Time(std::int64_t{1} << 62, 0), Time((std::int64_t{1} << 62) + 1, 4)); }), __LINE__);
}

} // namespace

int main() {
    TestSixCharacterHash();
    TestAppendHex();
    TestMediaTime();
    return 0;
}
