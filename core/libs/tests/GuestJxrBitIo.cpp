#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" std::uint32_t JxrBitIoReference_nid_no_patch(int operation, void* state, std::uint32_t value, std::uint32_t bits);

namespace {

constexpr std::size_t RingAlignment = 16384;
constexpr std::int32_t WriteMask = ~8192;
constexpr std::int32_t ReadMask = WriteMask & ~1;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR bit I/O: %s\n", message);
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

struct Ring {
    std::vector<std::uint8_t> storage = std::vector<std::uint8_t>(RingAlignment * 3);
    std::uint8_t* base = reinterpret_cast<std::uint8_t*>(
        (reinterpret_cast<std::uintptr_t>(storage.data()) + RingAlignment - 1) & ~std::uintptr_t{RingAlignment - 1});
};

std::uint32_t seed = 0x2468ace1;
std::uint32_t Next() {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

std::uint32_t LoadBigEndian(const std::uint8_t* pointer) {
    return (std::uint32_t{pointer[0]} << 24) | (std::uint32_t{pointer[1]} << 16) | (std::uint32_t{pointer[2]} << 8) | pointer[3];
}

Jxr::BitIo Attach(Ring& ring, std::size_t offset, std::uint32_t bitsUsed, std::size_t reference) {
    Jxr::BitIo io{};
    io.mask = ReadMask;
    io.start = ring.base;
    io.current = ring.base + offset;
    io.bitsUsed = bitsUsed;
    io.accumulator = LoadBigEndian(io.current) << bitsUsed;
    io.referenceOffset = reference;
    return io;
}

void RequireSame(const Jxr::BitIo& guest, const Ring& guestRing, const Jxr::BitIo& native, const Ring& nativeRing,
                 const char* message) {
    Require(guest.accumulator == native.accumulator && guest.bitsUsed == native.bitsUsed && guest.mask == native.mask &&
                guest.current - guestRing.base == native.current - nativeRing.base &&
                guest.start - guestRing.base == native.start - nativeRing.base &&
                guest.referenceOffset == native.referenceOffset,
            message);
    Require(std::memcmp(guestRing.base, nativeRing.base, RingAlignment) == 0, message);
}

std::uint32_t Guest(int operation, Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    switch (operation) {
    case 0: return peekBit16_nid_postfix(io, bits);
    case 1: return flushBit16_nid_postfix(io, bits);
    case 2: return getBit16_nid_postfix(io, bits);
    case 3: return getBool16_nid_postfix(io);
    case 4: return static_cast<std::uint32_t>(getBit16s_nid_postfix(io, bits));
    case 5: return getBit32_nid_postfix(io, bits);
    case 6: return flushToByte_nid_postfix(io);
    case 7: return getSizeRead_nid_postfix(io);
    case 8: return getPosRead_nid_postfix(io);
    case 9: putBit16z_nid_postfix(io, value, bits); return 0;
    case 10: putBit16_nid_postfix(io, value, bits); return 0;
    case 11: putBit32_nid_postfix(io, value, bits); return 0;
    case 12: fillToByte_nid_postfix(io); return 0;
    case 13: return getSizeWrite_nid_postfix(io);
    default: return 0xffffffffu;
    }
}

void RunReads() {
    Ring guestRing, nativeRing;
    for (std::size_t i = 0; i < RingAlignment; ++i) guestRing.base[i] = nativeRing.base[i] = static_cast<std::uint8_t>(Next());
    for (int round = 0; round < 64; ++round) {
        const auto offset = (Next() % 4096) * 2;
        const auto used = Next() % 16;
        const std::size_t reference = 100000 + Next() % 100000;
        auto guest = Attach(guestRing, offset, used, reference);
        auto native = Attach(nativeRing, offset, used, reference);
        for (int step = 0; step < 2000; ++step) {
            const int operation = static_cast<int>(Next() % 9);
            std::uint32_t bits = Next() % 17;
            if (operation == 4) bits %= 16;
            if (operation == 5) bits = Next() % 33;
            const auto expected = JxrBitIoReference_nid_no_patch(operation, &native, 0, bits);
            const auto actual = Guest(operation, &guest, 0, bits);
            Require(actual == expected, "read result differs from jxrlib");
            RequireSame(guest, guestRing, native, nativeRing, "read state differs from jxrlib");
        }
    }
}

void RunWrites() {
    Ring guestRing, nativeRing;
    for (int round = 0; round < 64; ++round) {
        for (std::size_t i = 0; i < RingAlignment; ++i) guestRing.base[i] = nativeRing.base[i] = static_cast<std::uint8_t>(Next());
        const auto offset = (Next() % 4096) * 2;
        const auto used = Next() % 16;
        const auto accumulator = Next() & (used == 0 ? 0u : 0xffffffffu >> (32 - used));
        auto guest = Attach(guestRing, offset, used, 0);
        guest.start = guestRing.base + (Next() % 2) * 4096;
        guest.accumulator = accumulator;
        guest.mask = WriteMask;
        auto native = guest;
        native.start = nativeRing.base + (guest.start - guestRing.base);
        native.current = nativeRing.base + offset;
        for (int step = 0; step < 2000; ++step) {
            const int operation = 9 + static_cast<int>(Next() % 5);
            std::uint32_t bits = operation == 11 ? Next() % 33 : Next() % 17;
            std::uint32_t value = Next() ^ (Next() << 24);
            if (operation == 9) value &= bits == 0 ? 0u : 0xffffffffu >> (32 - bits);
            const auto expected = JxrBitIoReference_nid_no_patch(operation, &native, value, bits);
            const auto actual = Guest(operation, &guest, value, bits);
            Require(actual == expected, "write result differs from jxrlib");
            RequireSame(guest, guestRing, native, nativeRing, "write state differs from jxrlib");
        }
    }
}

void RunEdges() {
    for (std::uint32_t qp = 0; qp < 256; ++qp)
        Require(dquantBits_nid_postfix(static_cast<std::uint8_t>(qp)) ==
                    JxrBitIoReference_nid_no_patch(14, nullptr, qp, 0),
                "dquantBits differs from jxrlib");

    Ring ring;
    for (std::size_t i = 0; i < RingAlignment; ++i) ring.base[i] = static_cast<std::uint8_t>(i * 7);
    auto io = Attach(ring, 8190, 4, 0);
    Require(getBit16_nid_postfix(&io, 12) == (LoadBigEndian(ring.base + 8190) << 4 >> 20), "wrap read value");
    Require(io.current == ring.base && io.bitsUsed == 0, "read wraps to the ring start");

    auto copy = Attach(ring, 10, 3, 0);
    Require(getBit16_S_nid_postfix(nullptr, &copy, 5) == ((LoadBigEndian(ring.base + 10) << 3) >> 27), "getBit16_S value");
    Require(copy.current == ring.base + 10 && copy.bitsUsed == 8, "getBit16_S state");
    auto crossing = Attach(ring, 4096, 0, 0);
    RequireInvalidArgument([&] { getBit16_S_nid_postfix(nullptr, &crossing, 1); }, "getBit16_S packet without stream");

    RequireInvalidArgument([] { getBit16_nid_postfix(nullptr, 1); }, "null read state");
    RequireInvalidArgument([] { putBit16_nid_postfix(nullptr, 1, 1); }, "null write state");
    RequireInvalidArgument([] { getSizeRead_nid_postfix(nullptr); }, "null size state");
    RequireInvalidArgument([] { getSizeWrite_nid_postfix(nullptr); }, "null size write state");
    RequireInvalidArgument([] { getPosRead_nid_postfix(nullptr); }, "null position state");
    RequireInvalidArgument([] { flushToByte_nid_postfix(nullptr); }, "null flush state");
    RequireInvalidArgument([] { fillToByte_nid_postfix(nullptr); }, "null fill state");
    auto edge = Attach(ring, 0, 0, 0);
    RequireInvalidArgument([&] { peekBit16_nid_postfix(&edge, 17); }, "17-bit peek");
    RequireInvalidArgument([&] { flushBit16_nid_postfix(&edge, 17); }, "17-bit flush");
    RequireInvalidArgument([&] { getBit16s_nid_postfix(&edge, 16); }, "16-bit signed read");
    RequireInvalidArgument([&] { getBit32_nid_postfix(&edge, 33); }, "33-bit read");
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&edge, 2, 1); }, "wide unmasked value");
    RequireInvalidArgument([&] { putBit16_nid_postfix(&edge, 0, 17); }, "17-bit write");
    RequireInvalidArgument([&] { putBit32_nid_postfix(&edge, 0, 33); }, "33-bit write");
    edge.mask = -1;
    RequireInvalidArgument([&] { flushBit16_nid_postfix(&edge, 1); }, "odd wrap mask");
    edge.mask = ReadMask;
    edge.current = nullptr;
    RequireInvalidArgument([&] { getBit16_nid_postfix(&edge, 1); }, "null current pointer");
}

} // namespace

int main() {
    RunReads();
    RunWrites();
    RunEdges();
    return 0;
}
