#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using ReadTimeout = std::int64_t (APS5_VABI*)(const void*);
using SetTime = std::int64_t (APS5_VABI*)(void*, std::int64_t);
using ReadVersion = int (APS5_VABI*)(const void*);

static_assert(std::is_same_v<decltype(&SSL_SESSION_get_timeout_nid_postfix), ReadTimeout>);
static_assert(std::is_same_v<decltype(&SSL_SESSION_set_time_nid_postfix), SetTime>);
static_assert(std::is_same_v<decltype(&SSL_SESSION_get_protocol_version_nid_postfix), ReadVersion>);
static_assert(sizeof(int) == 4);
static_assert(std::endian::native == std::endian::little);

std::size_t checks = 0;
std::size_t cases = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "%s at case %zu, check %zu\n", message, cases, checks);
        std::abort();
    }
}

void Write(std::array<unsigned char, 256>& bytes, std::size_t offset, std::uint64_t bits, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) bytes[offset + i] = static_cast<unsigned char>(bits >> (i * 8));
}

std::uint64_t Random(std::uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

void Exercise(std::int32_t version, std::int64_t timeout, std::int64_t oldTime, std::int64_t newTime, std::uint64_t seed) {
    ++cases;
    alignas(8) std::array<unsigned char, 256> bytes;
    for (auto& byte : bytes) byte = static_cast<unsigned char>(Random(seed));
    constexpr std::size_t Start = 24;
    Write(bytes, Start, static_cast<std::uint32_t>(version), 4);
    Write(bytes, Start + 168, static_cast<std::uint64_t>(timeout), 8);
    Write(bytes, Start + 176, static_cast<std::uint64_t>(oldTime), 8);
    auto expected = bytes;
    void* const session = bytes.data() + Start;
    ReadTimeout volatile readTimeout = SSL_SESSION_get_timeout_nid_postfix;
    ReadVersion volatile readVersion = SSL_SESSION_get_protocol_version_nid_postfix;
    SetTime volatile setTime = SSL_SESSION_set_time_nid_postfix;
    Check(readTimeout(session) == timeout, "timeout full signed value");
    Check(readVersion(session) == version, "protocol full signed value");
    Check(bytes == expected, "queries preserve all session and canary bytes");
    Check(setTime(session, newTime) == newTime, "set time returns new signed time");
    Write(expected, Start + 176, static_cast<std::uint64_t>(newTime), 8);
    Check(bytes == expected, "set time changes only eight creation-time bytes");
    Check(readTimeout(session) == timeout, "timeout unchanged by time assignment");
    Check(readVersion(session) == version, "version unchanged by time assignment");
    Check(setTime(session, newTime) == newTime, "repeated assignment return");
    Check(bytes == expected, "repeated assignment preserves bytes");
    Check(setTime(session, oldTime) == oldTime, "time restoration return");
    Write(expected, Start + 176, static_cast<std::uint64_t>(oldTime), 8);
    Check(bytes == expected, "time restoration preserves other fields");
}

}

int main() {
    constexpr std::array<std::int64_t, 16> Times{
        0, 1, -1, 2147483647, 2147483648LL, 4294967295LL, 4294967296LL,
        -2147483648LL, -2147483649LL, -4294967296LL, 4102444800LL,
        0x0123456789abcdefLL, -0x0123456789abcdefLL,
        std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(), 300
    };
    constexpr std::array<std::int32_t, 12> Versions{
        0, 1, -1, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0xfeff, 0xfefd,
        std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()
    };
    Check(SSL_SESSION_get_timeout_nid_postfix(nullptr) == 0, "null timeout");
    for (auto time : Times) Check(SSL_SESSION_set_time_nid_postfix(nullptr, time) == 0, "null time assignment");
    bool threw = false;
    try {
        SSL_SESSION_get_protocol_version_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw, "null version rejects undefined upstream input");
    for (auto version : Versions)
        for (auto timeout : Times)
            for (auto time : Times) Exercise(version, timeout, ~time, time, cases + 1);
    for (unsigned bit = 0; bit < 64; ++bit) {
        const auto time = std::bit_cast<std::int64_t>(std::uint64_t{1} << bit);
        Exercise(0x0303, time, ~time, time, bit + 1);
        Exercise(0xfefd, ~time, time, ~time, bit + 65);
    }
    std::uint64_t state = 0x7646a3812a950fcd;
    for (unsigned i = 0; i < 4096; ++i) {
        const auto version = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Random(state)));
        const auto timeout = std::bit_cast<std::int64_t>(Random(state));
        const auto oldTime = std::bit_cast<std::int64_t>(Random(state));
        const auto newTime = std::bit_cast<std::int64_t>(Random(state));
        Exercise(version, timeout, oldTime, newTime, Random(state));
    }
    std::printf("session metadata: %zu cases, %zu checks\n", cases, checks);
}
