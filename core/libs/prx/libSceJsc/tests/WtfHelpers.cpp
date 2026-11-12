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
using Jsc::WtfAbi::SHA1;
using Jsc::WtfAbi::SHA1Digest;

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

SHA1 InitialState() {
    SHA1 sha1;
    std::memset(&sha1, 0xA5, sizeof(sha1));
    sha1.cursor = 0;
    sha1.totalBytes = 0;
    sha1.hash[0] = 0x67452301;
    sha1.hash[1] = 0xEFCDAB89;
    sha1.hash[2] = 0x98BADCFE;
    sha1.hash[3] = 0x10325476;
    sha1.hash[4] = 0xC3D2E1F0;
    std::memset(sha1.buffer, 0, sizeof(sha1.buffer));
    return sha1;
}

SHA1 StateWith(const std::string& message) {
    auto sha1 = InitialState();
    std::memcpy(sha1.buffer, message.data(), message.size());
    sha1.cursor = message.size();
    sha1.totalBytes = message.size();
    return sha1;
}

std::string Hex(const SHA1Digest& digest) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (auto byte : digest) {
        text += Digits[byte >> 4];
        text += Digits[byte & 0xF];
    }
    return text;
}

std::string ComputeHash(SHA1& sha1) {
    SHA1Digest digest;
    digest.fill(0xEE);
    _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest);
    return Hex(digest);
}

bool IsReset(const SHA1& sha1) {
    for (auto byte : sha1.buffer) {
        if (byte) return false;
    }
    return sha1.cursor == 0 && sha1.totalBytes == 0 && sha1.hash[0] == 0x67452301 && sha1.hash[1] == 0xEFCDAB89 && sha1.hash[2] == 0x98BADCFE && sha1.hash[3] == 0x10325476 && sha1.hash[4] == 0xC3D2E1F0;
}

void TestSha1() {
    auto sha1 = InitialState();
    Check(ComputeHash(sha1) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", __LINE__);
    Check(IsReset(sha1), __LINE__);

    sha1 = StateWith("abc");
    Check(ComputeHash(sha1) == "a9993e364706816aba3e25717850c26c9cd0d89d", __LINE__);
    Check(IsReset(sha1), __LINE__);
    Check(ComputeHash(sha1) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", __LINE__);

    sha1 = StateWith(std::string(55, 'a'));
    Check(ComputeHash(sha1) == "c1c8bbdc22796e28c0e15163d20899b65621d65a", __LINE__);

    sha1 = StateWith("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    Check(ComputeHash(sha1) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", __LINE__);
    Check(IsReset(sha1), __LINE__);

    sha1 = StateWith(std::string(63, 'a'));
    Check(ComputeHash(sha1) == "03f09f5b158a7a8cdad920bddc29b81c18a551f5", __LINE__);
    Check(IsReset(sha1), __LINE__);

    sha1 = StateWith("abc");
    std::memset(sha1.buffer + 3, 0x5A, sizeof(sha1.buffer) - 3);
    Check(ComputeHash(sha1) == "a9993e364706816aba3e25717850c26c9cd0d89d", __LINE__);

    SHA1Digest digest;
    sha1 = InitialState();
    Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(nullptr, &digest); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, nullptr); }), __LINE__);
    sha1.cursor = 64;
    Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest); }), __LINE__);
}

} // namespace

int main() {
    TestSixCharacterHash();
    TestAppendHex();
    TestMediaTime();
    TestSha1();
    return 0;
}
