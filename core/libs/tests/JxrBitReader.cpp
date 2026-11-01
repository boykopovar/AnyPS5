#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
std::uint32_t JxrReferencePeek(void* io, std::uint32_t bits);
std::uint32_t JxrReferenceFlush(void* io, std::uint32_t bits);
std::uint32_t JxrReferenceGet16(void* io, std::uint32_t bits);
std::uint32_t JxrReferenceGetBool(void* io);
std::int32_t JxrReferenceGetSigned(void* io, std::uint32_t bits);
std::uint32_t JxrReferenceGet32(void* io, std::uint32_t bits);
}

namespace {

constexpr std::size_t RingSize = 8192;
alignas(16384) std::array<std::uint8_t, 32768> ringStorage;

std::uint8_t* Ring() {
    return reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(ringStorage.data()) + 16383) & ~std::uintptr_t{16383});
}

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "JXR bit reader: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

std::uint64_t seed = 0x9e3779b97f4a7c15;

std::uint32_t Next() {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return static_cast<std::uint32_t>(seed >> 16);
}

Jxr::BitIo Attach(std::size_t offset, std::uint32_t used) {
    const std::uint8_t* bytes = Ring() + offset;
    const std::uint32_t word = (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) | bytes[3];
    return Jxr::BitIo{0x13572468, word << used, used, ~8192, Ring(), Ring() + offset, nullptr, 0};
}

void Literal() {
    std::uint8_t* ring = Ring();
    const std::uint8_t pattern[16] = {0xa5, 0x5a, 0xf0, 0x0f, 0x81, 0x7e, 0xc3, 0x3c, 0x9d};
    std::memcpy(ring, pattern, sizeof(pattern));
    auto io = Attach(0, 0);
    Require(peekBit16_nid_postfix(&io, 4) == 0xa, "peek of the first nibble");
    Require(peekBit16_nid_postfix(&io, 16) == 0xa55a, "peek of the first window");
    Require(io.current == ring && io.bitsUsed == 0, "peek moved the cursor");
    Require(getBit16_nid_postfix(&io, 4) == 0xa && io.bitsUsed == 4, "get of the first nibble");
    Require(getBool16_nid_postfix(&io) == 0 && getBool16_nid_postfix(&io) == 1 && io.bitsUsed == 6, "boolean bits");
    Require(flushBit16_nid_postfix(&io, 10) == 0 && io.current == ring + 2 && io.bitsUsed == 0, "flush across a window");
    Require(getBit32_nid_postfix(&io, 32) == 0xf00f817e, "full width read");
    Require(getBit32_nid_postfix(&io, 20) == 0xc33c9 && io.bitsUsed == 4, "read wider than a window");
    io = Attach(3, 0);
    Require(getBit16s_nid_postfix(&io, 3) == 0 && io.bitsUsed == 3, "zero prefix consumes only the magnitude width");
    io = Attach(4, 0);
    Require(getBit16s_nid_postfix(&io, 3) == 4 && io.bitsUsed == 4, "positive signed value");
    io = Attach(5, 0);
    Require(getBit16s_nid_postfix(&io, 3) == -3 && io.bitsUsed == 4, "negative signed value");
    io = Attach(6, 0);
    Require(getBit16s_nid_postfix(&io, 3) == 6 && io.bitsUsed == 4, "second positive signed value");
}

void Wrap() {
    std::uint8_t* ring = Ring();
    for (std::size_t i = 0; i < RingSize; ++i) ring[i] = static_cast<std::uint8_t>(i);
    auto io = Attach(RingSize - 1, 0);
    flushBit16_nid_postfix(&io, 16);
    Require(io.current == ring + 1, "cursor wrapped to the start");
    Require(io.accumulator == 0x01020304u << io.bitsUsed, "window reloaded after wrapping");
}

void Differential() {
    std::uint8_t* ring = Ring();
    for (std::size_t i = 0; i < RingSize; ++i) ring[i] = static_cast<std::uint8_t>(Next());
    for (int run = 0; run < 64; ++run) {
        auto actual = Attach(Next() % RingSize, Next() % 16);
        auto expected = actual;
        for (int step = 0; step < 4000; ++step) {
            const auto kind = Next() % 6;
            const auto bits16 = Next() % 16 + 1;
            const auto bits15 = Next() % 16;
            const auto bits32 = Next() % 32 + 1;
            std::uint64_t got = 0;
            std::uint64_t want = 0;
            switch (kind) {
            case 0:
                got = peekBit16_nid_postfix(&actual, bits16);
                want = JxrReferencePeek(&expected, bits16);
                break;
            case 1:
                got = flushBit16_nid_postfix(&actual, bits16);
                want = JxrReferenceFlush(&expected, bits16);
                break;
            case 2:
                got = getBit16_nid_postfix(&actual, bits16);
                want = JxrReferenceGet16(&expected, bits16);
                break;
            case 3:
                got = getBool16_nid_postfix(&actual);
                want = JxrReferenceGetBool(&expected);
                break;
            case 4:
                got = static_cast<std::uint32_t>(getBit16s_nid_postfix(&actual, bits15));
                want = static_cast<std::uint32_t>(JxrReferenceGetSigned(&expected, bits15));
                break;
            default:
                got = getBit32_nid_postfix(&actual, bits32);
                want = JxrReferenceGet32(&expected, bits32);
                break;
            }
            Require(got == want, "result differs from the reference");
            Require(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "bit state differs from the reference");
        }
    }
}

void Invalid() {
    auto io = Attach(0, 0);
    const auto initial = io;
    RequireInvalidArgument([] { peekBit16_nid_postfix(nullptr, 1); }, "null peek state");
    RequireInvalidArgument([] { flushBit16_nid_postfix(nullptr, 1); }, "null flush state");
    RequireInvalidArgument([] { getBit16_nid_postfix(nullptr, 1); }, "null get state");
    RequireInvalidArgument([] { getBool16_nid_postfix(nullptr); }, "null boolean state");
    RequireInvalidArgument([] { getBit16s_nid_postfix(nullptr, 1); }, "null signed state");
    RequireInvalidArgument([] { getBit32_nid_postfix(nullptr, 1); }, "null wide state");
    RequireInvalidArgument([&] { peekBit16_nid_postfix(&io, 0); }, "zero-width peek");
    RequireInvalidArgument([&] { peekBit16_nid_postfix(&io, 17); }, "oversized peek");
    RequireInvalidArgument([&] { flushBit16_nid_postfix(&io, 17); }, "oversized flush");
    RequireInvalidArgument([&] { getBit16_nid_postfix(&io, 0); }, "zero-width get");
    RequireInvalidArgument([&] { getBit16_nid_postfix(&io, 17); }, "oversized get");
    RequireInvalidArgument([&] { getBit16s_nid_postfix(&io, 16); }, "oversized signed width");
    RequireInvalidArgument([&] { getBit32_nid_postfix(&io, 0); }, "zero-width wide read");
    RequireInvalidArgument([&] { getBit32_nid_postfix(&io, 33); }, "oversized wide read");
    Require(std::memcmp(&io, &initial, sizeof(io)) == 0, "rejected arguments changed the state");
    io.current = nullptr;
    const auto detached = io;
    RequireInvalidArgument([&] { flushBit16_nid_postfix(&io, 0); }, "flush through a null window");
    RequireInvalidArgument([&] { getBool16_nid_postfix(&io); }, "boolean read through a null window");
    Require(std::memcmp(&io, &detached, sizeof(io)) == 0, "null window changed the state");
}

}

int main() {
    Literal();
    Wrap();
    Differential();
    Invalid();
    return 0;
}
