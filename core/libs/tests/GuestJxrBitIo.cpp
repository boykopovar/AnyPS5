#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

extern "C" std::uint32_t JxrBitIoReference_nid_no_patch(int operation, void* state, std::uint32_t value, std::uint32_t bits);

namespace {

constexpr std::size_t RingAlignment = 16384;
constexpr std::int32_t WriteMask = ~8192;
constexpr std::int32_t ReadMask = WriteMask & ~1;
constexpr std::size_t PacketSize = 4096;

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

Jxr::BitIo Attach(Ring& ring, std::size_t offset, std::uint32_t bitsUsed, std::size_t reference);

struct StreamState {
    Jxr::BitIo* io = nullptr;
    Jxr::Stream* replacement = nullptr;
    std::vector<std::size_t> positions;
    std::int64_t seekResult = 0;
    std::int64_t readResult = 0;
    bool throwRead = false;
    std::vector<char> calls;
};

StreamState* State(Jxr::Stream* stream) {
    return reinterpret_cast<StreamState*>(stream->state[0]);
}

std::int64_t APS5_VABI SetPosition(Jxr::Stream* stream, std::size_t position) {
    auto* state = State(stream);
    state->calls.push_back('s');
    state->positions.push_back(position);
    return state->seekResult;
}

std::int64_t APS5_VABI ReadPacket(Jxr::Stream* stream, void* buffer, std::size_t bytes) {
    auto* state = State(stream);
    state->calls.push_back('r');
    if (state->throwRead) throw std::runtime_error("packet read");
    Require(bytes == PacketSize, "detach read packet length");
    std::memset(buffer, 0x5a, bytes);
    state->io->stream = state->replacement;
    return state->readResult;
}

Jxr::Stream MakeStream(StreamState& state) {
    Jxr::Stream stream{};
    stream.state[0] = reinterpret_cast<std::uintptr_t>(&state);
    stream.read = ReadPacket;
    stream.setPosition = SetPosition;
    return stream;
}

void RunDetachRead() {
    Ring ring;
    std::memset(ring.base, 0xcc, RingAlignment);
    StreamState state{};
    auto stream = MakeStream(state);
    auto io = Attach(ring, 128, 8, 20000);
    io.stream = &stream;
    state.io = &io;
    state.seekResult = -0x123456789ll;
    const auto expectedPosition = io.referenceOffset - (8192 - (128 + 1));
    Require(detachISRead_nid_postfix(nullptr, &io) == 0, "detach without refill result");
    Require(state.calls == std::vector<char>({'s'}) && state.positions == std::vector<std::size_t>({expectedPosition}),
            "detach without refill seeks to buffered position");
    Require(io.stream == nullptr && io.current == ring.base + 128 && io.bitsUsed == 8, "detach without refill state");
    Require(ring.base[127] == 0xcc && ring.base[128] == 0xcc && ring.base[129] == 0xcc, "detach without refill keeps guards");

    StreamState pastWindowState{};
    auto pastWindowStream = MakeStream(pastWindowState);
    auto pastWindow = Attach(ring, 16384 + 8190, 8, 10000);
    pastWindow.start = ring.base + 16384 - 16;
    pastWindow.stream = &pastWindowStream;
    Require(detachISRead_nid_postfix(nullptr, &pastWindow) == 0, "detach permits a signed negative cached-byte difference");
    Require(pastWindowState.positions == std::vector<std::size_t>({10015}),
            "detach converts negative pointer difference to size_t as source does");

    constexpr std::size_t GuestBytes = 0x4000;
    auto* guest = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(GuestBytes, 16384));
    Require(guest != nullptr, "allocate detach packet buffer");
    GuestArena::GuestArenaCommit_nid_postfix(guest, GuestBytes, 4, 4096);
    std::memset(guest, 0xcc, GuestBytes);
    StreamState refillState{};
    StreamState replacementState{};
    auto refillStream = MakeStream(refillState);
    auto replacement = MakeStream(replacementState);
    auto refill = Jxr::BitIo{0x11223344, 0, 0, ReadMask, guest + PacketSize, guest + 2 * PacketSize, &refillStream, 9000};
    refillState.io = &refill;
    refillState.replacement = &replacement;
    refillState.seekResult = 0x123456789ll;
    refillState.readResult = -0x23456789ll;
    Require(detachISRead_nid_postfix(reinterpret_cast<void*>(1), &refill) == 0, "detach with refill result");
    Require(refillState.calls == std::vector<char>({'s', 'r', 's'}) &&
                refillState.positions == std::vector<std::size_t>({9000, 4904}),
            "detach refill callback order and positions");
    Require(refill.stream == nullptr && refill.current == guest && refill.referenceOffset == 13096 &&
                refill.start == guest && refill.shadow == 0x5a5a5a5a,
            "detach refill state and callback stream mutation");
    Require(guest[PacketSize - 1] == 0xcc && guest[PacketSize] == 0x5a && guest[2 * PacketSize - 1] == 0x5a &&
                guest[2 * PacketSize] == 0xcc,
            "detach refill respects packet guards");
    Require(replacementState.calls.empty(), "detach final seek uses original stream");

    StreamState exceptionState{};
    auto exceptionStream = MakeStream(exceptionState);
    auto exceptionIo = Jxr::BitIo{0, 0, 0, ReadMask, guest + PacketSize, guest + 2 * PacketSize, &exceptionStream, 5000};
    exceptionState.io = &exceptionIo;
    exceptionState.throwRead = true;
    bool propagated = false;
    try {
        detachISRead_nid_postfix(nullptr, &exceptionIo);
    } catch (const std::runtime_error&) {
        propagated = true;
    }
    Require(propagated && exceptionState.calls == std::vector<char>({'s', 'r'}) && exceptionIo.stream == &exceptionStream,
            "detach propagates refill exceptions before final seek");

    GuestArena::GuestArenaReset_nid_postfix(guest, GuestBytes);
    GuestArena::GuestArenaRelease_nid_postfix(guest, GuestBytes);

    StreamState throwingState{};
    auto throwingStream = MakeStream(throwingState);
    auto invalid = Attach(ring, 128, 8, 10000);
    invalid.stream = &throwingStream;
    const auto lastRegion = UINTPTR_MAX & ~std::uintptr_t{8191};
    const auto currentPacketBit = reinterpret_cast<std::uintptr_t>(invalid.current) & std::uintptr_t{4096};
    invalid.start = reinterpret_cast<std::uint8_t*>(lastRegion | currentPacketBit);
    throwingState.io = &invalid;
    RequireInvalidArgument([&] { detachISRead_nid_postfix(nullptr, &invalid); }, "detach rejects overflowing packet end");
    Require(invalid.stream == &throwingStream && throwingState.calls.empty(), "invalid detach leaves stream attached");
    RequireInvalidArgument([] { detachISRead_nid_postfix(nullptr, nullptr); }, "null detach state");
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

constexpr std::size_t AttachPacketSize = 8192;
constexpr std::size_t AttachGuardSize = 16;
constexpr std::uint8_t AttachGuardValue = 0xa7;
constexpr std::uintptr_t GetPositionMarker = 0x13579;
constexpr std::uintptr_t SetPositionMarker = 0x2468a;

struct AttachReadProbe {
    Jxr::BitIo* io;
    Jxr::Stream* stream;
    std::uint8_t* start;
    std::uint8_t* readStart;
    std::uint8_t* accumulatorStart;
    std::size_t position;
    std::int64_t getPositionResult;
    std::int64_t setPositionResult;
    std::int64_t readResult;
    bool writeBuffer;
    int stage;
};

AttachReadProbe* activeAttachReadProbe = nullptr;

std::int64_t APS5_VABI AttachGetPosition(Jxr::Stream* stream, std::size_t* position) {
    auto& probe = *activeAttachReadProbe;
    Require(stream == probe.stream && position == &probe.io->referenceOffset, "attach read gets original position output");
    Require(probe.stage == 0, "attach read calls get-position first");
    Require(probe.io->start != probe.start && probe.io->current != probe.start, "get-position precedes packet pointer updates");
    probe.stage = 1;
    stream->state[0] = GetPositionMarker;
    *position = probe.position;
    return probe.getPositionResult;
}

std::int64_t APS5_VABI AttachSetPosition(Jxr::Stream* stream, std::size_t position) {
    auto& probe = *activeAttachReadProbe;
    Require(stream == probe.stream && position == probe.position, "attach read sets the callback-provided position");
    Require(probe.stage == 1 && stream->state[0] == GetPositionMarker, "set-position sees get-position mutations");
    Require(probe.io->start == probe.start && probe.io->current == probe.start, "packet pointers precede set-position");
    probe.stage = 2;
    stream->state[1] = SetPositionMarker;
    probe.io->start = probe.readStart;
    return probe.setPositionResult;
}

std::int64_t APS5_VABI AttachRead(Jxr::Stream* stream, void* buffer, std::size_t size) {
    auto& probe = *activeAttachReadProbe;
    Require(stream == probe.stream && buffer == probe.readStart && size == AttachPacketSize, "attach read forwards the full packet");
    Require(probe.stage == 2 && stream->state[1] == SetPositionMarker, "read sees set-position mutations");
    probe.stage = 3;
    if (probe.writeBuffer) {
        auto* const bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(i * 37 + 11);
    }
    probe.io->start = probe.accumulatorStart;
    probe.io->referenceOffset = probe.position + 7;
    return probe.readResult;
}

void RunAttachRead(bool writeBuffer, std::size_t position, std::int64_t getPositionResult,
                   std::int64_t setPositionResult, std::int64_t readResult, bool mutateStart = false) {
    alignas(16) std::array<std::byte, AttachGuardSize + AttachPacketSize + sizeof(Jxr::BitIo) + AttachGuardSize> storage{};
    std::fill(storage.begin(), storage.begin() + AttachGuardSize, std::byte{AttachGuardValue});
    std::fill(storage.end() - AttachGuardSize, storage.end(), std::byte{AttachGuardValue});
    auto* const start = reinterpret_cast<std::uint8_t*>(storage.data() + AttachGuardSize);
    std::fill(start, start + AttachPacketSize, writeBuffer ? std::uint8_t{0} : std::uint8_t{0x5a});
    auto* const io = ::new (storage.data() + AttachGuardSize + AttachPacketSize) Jxr::BitIo{};
    io->shadow = 0x89abcdef;
    io->accumulator = 0x76543210;
    io->bitsUsed = 7;
    io->mask = 0x12345678;
    io->start = start + 3;
    io->current = start + 6;
    io->stream = nullptr;
    io->referenceOffset = 19;
    const auto shadow = io->shadow;
    Jxr::Stream stream{};
    stream.getPosition = &AttachGetPosition;
    stream.setPosition = &AttachSetPosition;
    stream.read = &AttachRead;
    std::array<std::uint8_t, AttachPacketSize> alternateRead{};
    std::array<std::uint8_t, 4> alternateAccumulator{0x12, 0x34, 0x56, 0x78};
    auto* const readStart = mutateStart ? alternateRead.data() : start;
    auto* const accumulatorStart = mutateStart ? alternateAccumulator.data() : start;
    AttachReadProbe probe{io, &stream, start, readStart, accumulatorStart, position, getPositionResult,
                         setPositionResult, readResult, writeBuffer, 0};
    activeAttachReadProbe = &probe;
    Require(attachISRead_nid_postfix(io, &stream, reinterpret_cast<void*>(std::uintptr_t{1})) == 0,
        "attach read returns source success despite callback results");
    activeAttachReadProbe = nullptr;
    Require(probe.stage == 3, "attach read invokes callbacks in order");
    Require(io->shadow == shadow, "attach read preserves shadow");
    Require(io->start == accumulatorStart && io->current == start && io->stream == &stream, "attach read installs packet state");
    Require(io->referenceOffset == position + 7 + AttachPacketSize, "attach read advances the callback-written offset with unsigned wrap");
    Require(io->bitsUsed == 0 && io->mask == ReadMask, "attach read initializes read bit state");
    Require(io->accumulator == (mutateStart ? 0x12345678 : (writeBuffer ? LoadBigEndian(start) : 0x5a5a5a5a)), "attach read loads the big-endian accumulator after the read callback");
    Require(std::all_of(storage.begin(), storage.begin() + AttachGuardSize,
        [](std::byte value) { return value == std::byte{AttachGuardValue}; }), "attach read preserves the leading guard");
    Require(std::all_of(storage.end() - AttachGuardSize, storage.end(),
        [](std::byte value) { return value == std::byte{AttachGuardValue}; }), "attach read preserves the trailing guard");
}

void RunAttachReadEdges() {
    RunAttachRead(true, std::numeric_limits<std::size_t>::max() - 4095,
        INT64_C(0x123456789abcdef), -INT64_C(0x123456789abcdef), -INT64_C(0x23456789abcdef));
    RunAttachRead(false, 0x1122334455667788, -INT64_C(0x123456789abcdef),
        INT64_C(0x23456789abcdef), -INT64_C(0x3456789abcdef0));
    RunAttachRead(true, 123, 0, 0, 0, true);
    Jxr::BitIo io{};
    Jxr::Stream stream{};
    RequireInvalidArgument([&] { attachISRead_nid_postfix(&io, &stream, nullptr); }, "attach read rejects a missing callback");
    RequireInvalidArgument([] { attachISRead_nid_postfix(nullptr, nullptr, nullptr); }, "attach read rejects null state and stream");
}

Jxr::BitIo* attachingIo = nullptr;
Jxr::Stream* attachingStream = nullptr;
std::int64_t positionResult = 0;
bool storePosition = true;
bool throwPosition = false;
int positionCalls = 0;

std::int64_t APS5_VABI AttachmentPosition(Jxr::Stream* stream, std::size_t* position) {
    Require(stream == attachingStream, "attachment forwards original stream");
    Require(position == &attachingIo->referenceOffset, "attachment forwards original offset field");
    Require(attachingIo->bitsUsed == 13 && attachingIo->mask == 37,
            "position callback precedes bit-state initialization");
    ++positionCalls;
    if (throwPosition) throw std::invalid_argument("position callback failure");
    if (storePosition) *position = std::numeric_limits<std::size_t>::max() - 17;
    attachingIo->shadow = 0x76543210;
    return positionResult;
}

void RunAttachWrite() {
    alignas(Jxr::BitIo) std::array<std::uint8_t, 8192 + sizeof(Jxr::BitIo) + 16> storage;
    storage.fill(0xa5);
    auto* io = std::construct_at(reinterpret_cast<Jxr::BitIo*>(storage.data() + 8192));
    Jxr::Stream stream{};
    stream.getPosition = &AttachmentPosition;
    attachingIo = io;
    attachingStream = &stream;
    for (const auto result : {std::int64_t{0}, std::int64_t{1} << 40, -(std::int64_t{1} << 40)}) {
        for (const bool store : {false, true}) {
            std::memset(io, 0x5c, sizeof(*io));
            io->bitsUsed = 13;
            io->mask = 37;
            io->referenceOffset = 12345;
            positionResult = result;
            storePosition = store;
            positionCalls = 0;
            Require(attachISWrite_nid_postfix(io, &stream) == 0, "attachment ignores position callback result");
            Require(positionCalls == 1, "attachment queries position once");
            Require(io->referenceOffset == (store ? std::numeric_limits<std::size_t>::max() - 17 : 12345),
                    "attachment preserves callback offset effect");
            Require(io->shadow == 0x76543210, "attachment preserves untouched shadow field");
            Require(io->start == storage.data() && io->current == storage.data(), "attachment locates preceding buffer");
            Require(io->accumulator == 0 && io->bitsUsed == 0 && io->mask == WriteMask && io->stream == &stream,
                    "attachment initializes writer state");
            for (std::size_t i = 0; i < 8192; ++i) Require(storage[i] == 0xa5, "attachment keeps buffer bytes");
            for (std::size_t i = 8192 + sizeof(*io); i < storage.size(); ++i)
                Require(storage[i] == 0xa5, "attachment keeps trailing guard bytes");
        }
    }
    io->bitsUsed = 13;
    io->mask = 37;
    std::array<std::byte, sizeof(*io)> before;
    std::memcpy(before.data(), io, sizeof(*io));
    throwPosition = true;
    RequireInvalidArgument([&] { attachISWrite_nid_postfix(io, &stream); }, "attachment propagates callback exceptions");
    Require(std::memcmp(before.data(), io, sizeof(*io)) == 0, "throwing position callback prevents initialization");
    throwPosition = false;
    stream.getPosition = nullptr;
    RequireInvalidArgument([&] { attachISWrite_nid_postfix(io, &stream); }, "attachment needs a position callback");
    RequireInvalidArgument([&] { attachISWrite_nid_postfix(io, nullptr); }, "attachment needs a stream");
    RequireInvalidArgument([&] { attachISWrite_nid_postfix(nullptr, &stream); }, "attachment needs bit state");
    std::destroy_at(io);
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
            const bool zeroBits = bits == 0 && (operation == 0 || operation == 2 || operation == 5);
            const auto expected = zeroBits ? 0u : JxrBitIoReference_nid_no_patch(operation, &native, 0, bits);
            if (zeroBits && operation != 0) JxrBitIoReference_nid_no_patch(1, &native, 0, 0);
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
            const bool emptyStore = native.bitsUsed == 0 && (operation == 12 || (operation != 13 && bits == 0));
            const auto expected = emptyStore ? 0u : JxrBitIoReference_nid_no_patch(operation, &native, value, bits);
            const auto actual = Guest(operation, &guest, value, bits);
            Require(actual == expected, "write result differs from jxrlib");
            RequireSame(guest, guestRing, native, nativeRing, "write state differs from jxrlib");
        }
    }
}

void RunZeroBits(Ring& ring) {
    for (std::size_t i = 0; i < RingAlignment; ++i) ring.base[i] = static_cast<std::uint8_t>(i * 13 + 5);
    for (std::uint32_t used = 0; used < 16; ++used) {
        auto io = Attach(ring, 64, used, 0);
        const auto before = io;
        Require(peekBit16_nid_postfix(&io, 0) == 0, "zero-bit peek");
        Require(std::memcmp(&io, &before, sizeof(io)) == 0, "zero-bit peek keeps the state");
        Require(getBit16_nid_postfix(&io, 0) == 0, "zero-bit read");
        Require(io.current == before.current && io.bitsUsed == used && io.accumulator == before.accumulator,
                "zero-bit read keeps the position");
        Require(getBit32_nid_postfix(&io, 0) == 0, "zero-bit 32-bit read");
        Require(io.current == before.current && io.bitsUsed == used && io.accumulator == before.accumulator,
                "zero-bit 32-bit read keeps the position");
    }

    std::vector<std::uint8_t> snapshot(ring.base, ring.base + RingAlignment);
    Jxr::BitIo io{};
    io.mask = WriteMask;
    io.start = ring.base;
    io.current = ring.base + 128;
    const auto before = io;
    putBit16z_nid_postfix(&io, 0, 0);
    putBit16_nid_postfix(&io, 0xffffffffu, 0);
    putBit32_nid_postfix(&io, 0xffffffffu, 0);
    fillToByte_nid_postfix(&io);
    Require(std::memcmp(&io, &before, sizeof(io)) == 0, "aligned empty stores keep the state");
    Require(std::memcmp(snapshot.data(), ring.base, RingAlignment) == 0, "aligned empty stores keep the buffer");

    putBit16_nid_postfix(&io, 0xfff5u, 4);
    Require(io.accumulator == 5 && io.bitsUsed == 4 && io.current == ring.base + 128, "masked 4-bit store state");
    Require(ring.base[128] == 0x50 && ring.base[129] == 0x00, "masked 4-bit store bytes");
    fillToByte_nid_postfix(&io);
    Require(io.bitsUsed == 8 && ring.base[128] == 0x50, "fill to the byte boundary");
    fillToByte_nid_postfix(&io);
    putBit16_nid_postfix(&io, 0x1234u, 0);
    Require(io.bitsUsed == 8 && io.current == ring.base + 128, "empty stores at a byte boundary");
    putBit32_nid_postfix(&io, 0xfabcdeu, 20);
    Require(io.bitsUsed == 12 && io.current == ring.base + 130, "20-bit store position");
    Require(ring.base[128] == 0x50 && ring.base[129] == 0xab && ring.base[130] == 0xcd && ring.base[131] == 0xe0,
            "20-bit store bytes");
    putBit32_nid_postfix(&io, 0xffffffffu, 32);
    Require(io.bitsUsed == 12 && io.current == ring.base + 134, "32-bit store position");
    Require(ring.base[130] == 0xcd && ring.base[131] == 0xef && ring.base[132] == 0xff && ring.base[133] == 0xff,
            "32-bit store bytes");
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

    RunZeroBits(ring);

    for (const std::uint32_t used : {0u, 1u, 8u, 15u}) {
        auto odd = Attach(ring, 129, used, 0);
        odd.mask = WriteMask;
        const auto before = odd;
        const std::vector<std::uint8_t> snapshot(ring.base, ring.base + RingAlignment);
        RequireInvalidArgument([&] { putBit16z_nid_postfix(&odd, 0, 0); }, "odd-position unmasked store");
        RequireInvalidArgument([&] { putBit16_nid_postfix(&odd, 1, 1); }, "odd-position masked store");
        RequireInvalidArgument([&] { putBit32_nid_postfix(&odd, 0xffffffffu, 32); }, "odd-position 32-bit store");
        RequireInvalidArgument([&] { fillToByte_nid_postfix(&odd); }, "odd-position byte fill");
        Require(std::memcmp(&odd, &before, sizeof(odd)) == 0, "rejected odd-position stores keep the state");
        Require(std::memcmp(ring.base, snapshot.data(), RingAlignment) == 0, "rejected odd-position stores keep the buffer");
    }

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
    edge.mask = WriteMask;
    edge.bitsUsed = 16;
    RequireInvalidArgument([&] { putBit16z_nid_postfix(&edge, 0, 0); }, "write position past the word");
    RequireInvalidArgument([&] { fillToByte_nid_postfix(&edge); }, "fill position past the word");
    edge.bitsUsed = 0;
    edge.mask = -1;
    RequireInvalidArgument([&] { flushBit16_nid_postfix(&edge, 1); }, "odd wrap mask");
    edge.mask = ReadMask;
    edge.current = nullptr;
    RequireInvalidArgument([&] { getBit16_nid_postfix(&edge, 1); }, "null current pointer");
}

} // namespace

int main() {
    RunAttachReadEdges();
    RunAttachWrite();
    RunReads();
    RunWrites();
    RunDetachRead();
    RunEdges();
    return 0;
}
