#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
void JxrReferencePutBit16z(void* io, std::uint32_t bits, std::uint32_t width);
void JxrReferencePutBit16(void* io, std::uint32_t bits, std::uint32_t width);
void JxrReferencePutBit32(void* io, std::uint32_t bits, std::uint32_t width);
void JxrReferenceFillToByte(void* io);
std::uint32_t JxrReferenceSizeRead(void* io);
std::uint32_t JxrReferenceSizeWrite(void* io);
std::uint32_t JxrReferencePosRead(void* io);
}

namespace {

constexpr std::size_t RingSize = 8192;
constexpr std::int32_t RingMask = ~8193;

struct Ring {
    alignas(64) std::array<std::uint8_t, 32768> storage;

    std::uint8_t* Data() {
        return reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 16383) & ~std::uintptr_t{16383});
    }
};

Ring actualRing;
Ring expectedRing;

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "JXR bit writer: %s\n", message);
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

Jxr::BitIo Attach(Ring& ring, std::size_t offset, std::uint32_t accumulator, std::uint32_t used) {
    return Jxr::BitIo{0, accumulator, used, RingMask, ring.Data(), ring.Data() + offset, nullptr, 0};
}

void Fill(Ring& ring, std::uint8_t value) {
    std::memset(ring.Data(), value, RingSize);
}

bool SameState(const Jxr::BitIo& actual, Ring& actualBytes, const Jxr::BitIo& expected, Ring& expectedBytes) {
    return actual.shadow == expected.shadow && actual.accumulator == expected.accumulator && actual.bitsUsed == expected.bitsUsed && actual.mask == expected.mask &&
           actual.current - actualBytes.Data() == expected.current - expectedBytes.Data() && actual.start - actualBytes.Data() == expected.start - expectedBytes.Data() &&
           actual.stream == expected.stream && actual.referenceOffset == expected.referenceOffset;
}

void Literal() {
    std::uint8_t* ring = actualRing.Data();
    Fill(actualRing, 0xee);
    auto io = Attach(actualRing, 0, 0, 0);
    putBit16z_nid_postfix(&io, 0xa, 4);
    Require(io.bitsUsed == 4 && io.accumulator == 0xa && io.current == ring, "first nibble");
    Require(ring[0] == 0xa0 && ring[1] == 0x00 && ring[2] == 0xee, "first nibble bytes");
    putBit16_nid_postfix(&io, 0xfff5, 4);
    Require(io.bitsUsed == 8 && io.accumulator == 0xa5 && io.current == ring && ring[0] == 0xa5 && ring[1] == 0x00, "masked second nibble");
    putBit16_nid_postfix(&io, 0x1234, 16);
    Require(io.bitsUsed == 8 && io.accumulator == 0x00a51234 && io.current == ring + 2, "carry into the next halfword");
    Require(ring[0] == 0xa5 && ring[1] == 0x12 && ring[2] == 0xee, "the low byte stays pending");
    putBit32_nid_postfix(&io, 0xdeadbeef, 32);
    Require(io.bitsUsed == 8 && io.accumulator == 0xdeadbeef && io.current == ring + 6, "32-bit write advances two halfwords");
    Require(ring[2] == 0x34 && ring[3] == 0xde && ring[4] == 0xad && ring[5] == 0xbe && ring[6] == 0xee, "32-bit write bytes");
    putBit32_nid_postfix(&io, 0x155, 9);
    Require(io.bitsUsed == 1 && io.accumulator == 0x5b7ddf55 && io.current == ring + 8, "write narrower than a window");
    Require(ring[6] == 0xef && ring[7] == 0xaa && ring[8] == 0xee, "bytes of the narrow write");
    fillToByte_nid_postfix(&io);
    Require(io.bitsUsed == 8 && io.current == ring + 8 && ring[8] == 0x80 && ring[9] == 0x00, "padding to a byte boundary");
    fillToByte_nid_postfix(&io);
    Require(io.bitsUsed == 8 && io.current == ring + 8 && ring[8] == 0x80, "padding an aligned state");
    putBit16_nid_postfix(&io, 3, 2);
    Require(io.bitsUsed == 10 && ring[8] == 0x80 && ring[9] == 0xc0, "two bits after the padding");
    fillToByte_nid_postfix(&io);
    Require(io.bitsUsed == 0 && io.current == ring + 10 && ring[8] == 0x80 && ring[9] == 0xc0, "padding to a halfword");
    Require(getSizeWrite_nid_postfix(&io) == 10, "bytes written");
    const auto idle = io;
    fillToByte_nid_postfix(&io);
    Require(io.bitsUsed == 0 && io.current == idle.current && io.accumulator == idle.accumulator, "padding an empty halfword");
}

void Sizes() {
    std::uint8_t* ring = actualRing.Data();
    auto io = Attach(actualRing, 0, 0, 0);
    Require(getSizeWrite_nid_postfix(&io) == 0 && getSizeRead_nid_postfix(&io) == 8192, "empty window");
    io.bitsUsed = 11;
    io.current = ring + 100;
    Require(getSizeWrite_nid_postfix(&io) == 101 && getSizeRead_nid_postfix(&io) == 8092 - 1, "partially used window");
    io.start = ring + 4096;
    Require(getSizeWrite_nid_postfix(&io) == 100 + 8192 - 4096 + 1, "write cursor behind the start");
    io.referenceOffset = 70000;
    Require(getPosRead_nid_postfix(&io) == 70000 - (4096 + 8192 - 100 - 1), "stream offset of the cursor");
    io.referenceOffset = 10;
    Require(getPosRead_nid_postfix(&io) == static_cast<std::uint32_t>(10 - (4096 + 8192 - 100 - 1)), "stream offset wraps like the reference");
}

void Wrap() {
    std::uint8_t* ring = actualRing.Data();
    Fill(actualRing, 0);
    auto io = Attach(actualRing, RingSize - 2, 0, 0);
    putBit16_nid_postfix(&io, 0xbeef, 16);
    Require(io.current == ring && ring[RingSize - 2] == 0xbe && ring[RingSize - 1] == 0xef, "cursor wrapped to the start after the last halfword");
    putBit16_nid_postfix(&io, 0xabcd, 16);
    Require(io.current == ring + 2 && ring[0] == 0xab && ring[1] == 0xcd && ring[RingSize - 1] == 0xef, "bytes at the start of the ring");
}

void Differential() {
    for (int run = 0; run < 64; ++run) {
        for (std::size_t i = 0; i < RingSize; ++i) actualRing.Data()[i] = static_cast<std::uint8_t>(Next());
        std::memcpy(expectedRing.Data(), actualRing.Data(), RingSize);
        const std::size_t offset = (Next() % (RingSize / 2)) * 2;
        const std::uint32_t accumulator = Next() ^ (Next() << 16);
        const std::uint32_t used = Next() % 16;
        auto actual = Attach(actualRing, offset, accumulator, used);
        auto expected = Attach(expectedRing, offset, accumulator, used);
        for (int step = 0; step < 4000; ++step) {
            const auto kind = Next() % 8;
            const std::uint32_t width16 = Next() % 17;
            const std::uint32_t width32 = Next() % 33;
            const std::uint32_t bits = Next() ^ (Next() << 16);
            std::uint64_t got = 0;
            std::uint64_t want = 0;
            switch (kind) {
            case 0:
                putBit16z_nid_postfix(&actual, width16 == 0 ? 0 : bits & ((1u << width16) - 1), width16);
                JxrReferencePutBit16z(&expected, width16 == 0 ? 0 : bits & ((1u << width16) - 1), width16);
                break;
            case 1:
                putBit16_nid_postfix(&actual, bits, width16);
                JxrReferencePutBit16(&expected, bits, width16);
                break;
            case 2:
                putBit32_nid_postfix(&actual, bits, width32);
                JxrReferencePutBit32(&expected, bits, width32);
                break;
            case 3:
                fillToByte_nid_postfix(&actual);
                JxrReferenceFillToByte(&expected);
                break;
            case 4:
                got = getSizeRead_nid_postfix(&actual);
                want = JxrReferenceSizeRead(&expected);
                break;
            case 5:
                got = getSizeWrite_nid_postfix(&actual);
                want = JxrReferenceSizeWrite(&expected);
                break;
            case 6:
                actual.referenceOffset = expected.referenceOffset = Next();
                got = getPosRead_nid_postfix(&actual);
                want = JxrReferencePosRead(&expected);
                break;
            default:
                putBit16_nid_postfix(&actual, bits, 0);
                JxrReferencePutBit16(&expected, bits, 0);
                break;
            }
            Require(got == want, "result differs from the reference");
            Require(SameState(actual, actualRing, expected, expectedRing), "bit state differs from the reference");
            Require(std::memcmp(actualRing.Data(), expectedRing.Data(), RingSize) == 0, "written bytes differ from the reference");
        }
    }
}

void Invalid() {
    Fill(actualRing, 0x77);
    auto io = Attach(actualRing, 0, 0x1234, 3);
    const auto initial = io;
    std::array<std::uint8_t, RingSize> bytes;
    std::memcpy(bytes.data(), actualRing.Data(), RingSize);
    RequireInvalidArgument([] { putBit16z_nid_postfix(nullptr, 0, 1); }, "null putBit16z state");
    RequireInvalidArgument([] { putBit16_nid_postfix(nullptr, 0, 1); }, "null putBit16 state");
    RequireInvalidArgument([] { putBit32_nid_postfix(nullptr, 0, 1); }, "null putBit32 state");
    RequireInvalidArgument([] { fillToByte_nid_postfix(nullptr); }, "null fillToByte state");
    RequireInvalidArgument([] { getSizeRead_nid_postfix(nullptr); }, "null getSizeRead state");
    RequireInvalidArgument([] { getSizeWrite_nid_postfix(nullptr); }, "null getSizeWrite state");
    RequireInvalidArgument([] { getPosRead_nid_postfix(nullptr); }, "null getPosRead state");
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&io, 0, 17); }, "oversized putBit16z width");
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&io, 4, 2); }, "putBit16z value wider than its width");
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&io, 1, 0); }, "putBit16z value with no width");
    RequireInvalidArgument([&] { putBit16_nid_postfix(&io, 0, 17); }, "oversized putBit16 width");
    RequireInvalidArgument([&] { putBit32_nid_postfix(&io, 0, 33); }, "oversized putBit32 width");
    Require(std::memcmp(&io, &initial, sizeof(io)) == 0 && std::memcmp(bytes.data(), actualRing.Data(), RingSize) == 0, "rejected arguments changed the state");
    io.current = nullptr;
    const auto detached = io;
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&io, 0, 0); }, "putBit16z through a null window");
    RequireInvalidArgument([&] { putBit16_nid_postfix(&io, 1, 1); }, "putBit16 through a null window");
    RequireInvalidArgument([&] { putBit32_nid_postfix(&io, 1, 20); }, "putBit32 through a null window");
    RequireInvalidArgument([&] { fillToByte_nid_postfix(&io); }, "fillToByte through a null window");
    Require(std::memcmp(&io, &detached, sizeof(io)) == 0, "null window changed the state");
    auto low = Attach(actualRing, RingSize - 2, 0, 0);
    low.mask = 0;
    const auto beforeLow = low;
    RequireInvalidArgument([&] { putBit16_nid_postfix(&low, 1, 16); }, "write that wraps the cursor to null");
    Require(std::memcmp(&low, &beforeLow, sizeof(low)) == 0 && std::memcmp(bytes.data(), actualRing.Data(), RingSize) == 0, "null wrap changed the state");
    RequireInvalidArgument([&] { putBit32_nid_postfix(&low, 1, 32); }, "wide write that wraps the cursor to null");
    Require(std::memcmp(&low, &beforeLow, sizeof(low)) == 0, "wide null wrap changed the state");
}

}

int main() {
    Literal();
    Sizes();
    Wrap();
    Differential();
    Invalid();
    return 0;
}
