#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>



namespace {

constexpr std::size_t PacketSize = 4096;
alignas(8192) std::array<std::uint8_t, 24576> ringStorage;
std::span<std::uint8_t, 8192> ring(reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(ringStorage.data()) + 16383) & ~std::uintptr_t{16383}), 8192);
std::array<std::uint8_t, PacketSize> output;
std::vector<char> calls;
void* lastBuffer;
std::size_t lastSize;
std::size_t lastPosition;
std::int64_t readResult;
std::int64_t writeResult;
std::int64_t seekResult;
Jxr::Stream* lastStream;

struct Mutation {
    Jxr::BitIo* io = nullptr;
    std::uint8_t* afterSeek = nullptr;
    Jxr::Stream* afterSeekStream = nullptr;
    std::uint8_t* afterRead = nullptr;
    std::uint8_t* afterWrite = nullptr;
} mutation;

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "JXR stream: %s\n", message);
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

std::int64_t APS5_VABI Read(Jxr::Stream* stream, void* buffer, std::size_t size) {
    calls.push_back('r');
    lastStream = stream;
    lastBuffer = buffer;
    lastSize = size;
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(i * 17 + lastPosition);
    if (mutation.afterRead) mutation.io->start = mutation.afterRead;
    return readResult;
}

std::int64_t APS5_VABI Write(Jxr::Stream*, const void* buffer, std::size_t size) {
    calls.push_back('w');
    lastBuffer = const_cast<void*>(buffer);
    lastSize = size;
    Require(size == output.size(), "write length differs from one packet");
    std::memcpy(output.data(), buffer, size);
    if (mutation.afterWrite) mutation.io->start = mutation.afterWrite;
    return writeResult;
}

std::int64_t APS5_VABI SetPosition(Jxr::Stream*, std::size_t position) {
    calls.push_back('s');
    lastPosition = position;
    if (mutation.afterSeek) mutation.io->start = mutation.afterSeek;
    if (mutation.afterSeekStream) mutation.io->stream = mutation.afterSeekStream;
    return seekResult;
}

void Reads(Jxr::Stream& stream) {
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::size_t position : {std::size_t{0}, std::size_t{91}, std::numeric_limits<std::size_t>::max() - 100}) {
            for (std::int64_t result : {std::int64_t{0}, std::int64_t{-73}, std::int64_t{0x100000001}}) {
                std::fill(ring.begin(), ring.end(), 0xcc);
                calls.clear();
                readResult = result;
                seekResult = -99;
                Jxr::BitIo io{0x77777777, 0x12345678, 11, ~8192, ring.data() + half, ring.data() + (half ^ PacketSize), &stream, position};
                const auto initial = io;
                Require(readIS_nid_postfix(nullptr, &io) == 0, "read helper propagated an ignored callback error");
                Require(calls == std::vector<char>({'s', 'r'}), "read callbacks are absent or out of order");
                Require(lastPosition == position && lastSize == PacketSize && lastBuffer == initial.start, "read callback arguments differ");
                Require(io.referenceOffset == position + PacketSize && io.start == ring.data() + (half ^ PacketSize), "read cursor did not advance and wrap");
                Require(io.current == initial.current && io.accumulator == initial.accumulator && io.bitsUsed == initial.bitsUsed && io.mask == initial.mask && io.stream == initial.stream, "read changed unrelated bit state");
                std::uint32_t shadow;
                std::memcpy(&shadow, initial.start, sizeof(shadow));
                Require(io.shadow == shadow, "read shadow is not the first four bytes in host order");
                for (std::size_t i = 0; i < PacketSize; ++i) {
                    Require(initial.start[i] == static_cast<std::uint8_t>(i * 17 + position), "refill data differs");
                    Require(ring[(half ^ PacketSize) + i] == 0xcc, "refill changed the other half of the ring");
                }
            }
        }
    }
}

void Writes(Jxr::Stream& stream) {
    for (std::size_t i = 0; i < ring.size(); ++i) ring[i] = static_cast<std::uint8_t>(i * 23);
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::int64_t result : {std::int64_t{0}, std::int64_t{-73}, std::int64_t{0x100000001}, std::numeric_limits<std::int64_t>::min()}) {
            calls.clear();
            writeResult = result;
            Jxr::BitIo io{7, 19, 13, ~8192, ring.data() + half, ring.data() + (half ^ PacketSize), &stream, 1234};
            const auto initial = io;
            Require(writeIS_nid_postfix(nullptr, &io) == result, "write result was truncated or changed");
            Require(calls == std::vector<char>({'w'}) && lastBuffer == initial.start && lastSize == PacketSize, "write callback arguments differ");
            Require(std::equal(output.begin(), output.end(), initial.start), "write data differs from buffered bytes");
            Require(io.start == (result < 0 ? initial.start : ring.data() + (half ^ PacketSize)), "failed write advanced or successful write did not wrap");
            Require(io.shadow == initial.shadow && io.accumulator == initial.accumulator && io.bitsUsed == initial.bitsUsed && io.mask == initial.mask && io.current == initial.current && io.stream == initial.stream && io.referenceOffset == initial.referenceOffset, "write changed unrelated bit state");
        }
    }
}

void Buffered() {
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::size_t consumed : {std::size_t{0}, PacketSize - 1}) {
            Jxr::BitIo io{123, 456, 7, ~8192, ring.data() + half, ring.data() + half + consumed, nullptr, 789};
            const auto initial = io;
            calls.clear();
            Require(readIS_nid_postfix(nullptr, &io) == 0 && writeIS_nid_postfix(nullptr, &io) == 0, "buffered data unexpectedly required a stream");
            Require(calls.empty() && std::memcmp(&initial, &io, sizeof(io)) == 0, "buffered operation mutated state or performed I/O");
        }
    }
}

void CallbackMutation(Jxr::Stream& stream) {
    Jxr::Stream replacement{};
    readResult = -73;
    seekResult = -99;
    std::fill(ring.begin(), ring.end(), 0xcc);
    calls.clear();
    Jxr::BitIo io{7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 91};
    mutation = {&io, ring.data() + PacketSize, &replacement, nullptr, nullptr};
    Require(readIS_nid_postfix(nullptr, &io) == 0, "seek mutation read failed");
    Require(calls == std::vector<char>({'s', 'r'}) && lastStream == &stream, "read did not retain the stream selected before seek");
    Require(lastBuffer == ring.data() + PacketSize && lastSize == PacketSize, "read did not use the buffer selected by seek");
    Require(io.stream == &replacement && io.start == ring.data() && io.referenceOffset == 91 + PacketSize, "seek mutation was lost before wrapping");
    std::uint32_t shadow;
    std::memcpy(&shadow, ring.data() + PacketSize, sizeof(shadow));
    Require(io.shadow == shadow && ring.front() == 0xcc, "seek-selected buffer shadow differs");

    std::fill(ring.begin(), ring.end(), 0xab);
    calls.clear();
    io = {7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 123};
    mutation = {&io, nullptr, nullptr, ring.data() + PacketSize, nullptr};
    Require(readIS_nid_postfix(nullptr, &io) == 0, "read mutation failed");
    Require(lastBuffer == ring.data() && io.start == ring.data(), "read mutation did not precede final wrap");
    Require(io.shadow == 0xabababab && io.referenceOffset == 123 + PacketSize, "shadow did not use the pointer changed by read");

    for (std::int64_t result : {std::int64_t{0}, std::int64_t{0x100000001}, std::int64_t{-73}}) {
        std::fill(ring.begin(), ring.end(), 0xef);
        calls.clear();
        io = {7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 789};
        mutation = {&io, nullptr, nullptr, nullptr, ring.data() + PacketSize};
        writeResult = result;
        Require(writeIS_nid_postfix(nullptr, &io) == result, "mutating write result differs");
        Require(calls == std::vector<char>({'w'}) && lastBuffer == ring.data(), "mutating write buffer differs");
        Require(io.start == ring.data() + (result < 0 ? PacketSize : 0), "write callback mutation did not precede success wrap or failed return");
        Require(io.referenceOffset == 789 && io.shadow == 7, "mutating write changed unrelated state");
    }
    mutation = {};
}

std::int64_t APS5_VABI ObserveRead(Jxr::Stream*, void* buffer, std::size_t size) {
    calls.push_back('r');
    lastBuffer = buffer;
    lastSize = size;
    return 0;
}

std::int64_t APS5_VABI ObserveWrite(Jxr::Stream*, const void* buffer, std::size_t size) {
    calls.push_back('w');
    lastBuffer = const_cast<void*>(buffer);
    lastSize = size;
    return writeResult;
}

void NullState(Jxr::Stream& stream) {
    RequireInvalidArgument([] { readIS_nid_postfix(nullptr, nullptr); }, "read accepted a null bit state");
    RequireInvalidArgument([] { writeIS_nid_postfix(nullptr, nullptr); }, "write accepted a null bit state");

    auto* packet = ring.data() + PacketSize;
    Jxr::Stream partial = stream;
    for (auto* selected : {static_cast<Jxr::Stream*>(nullptr), &partial}) {
        partial.setPosition = nullptr;
        Jxr::BitIo io{7, 19, 13, ~8192, ring.data(), packet, selected, 91};
        const auto initial = io;
        calls.clear();
        RequireInvalidArgument([&] { readIS_nid_postfix(nullptr, &io); }, "read accepted a null stream or seek callback");
        Require(calls.empty() && std::memcmp(&io, &initial, sizeof(io)) == 0, "rejected read seek performed I/O or mutated state");
        partial.write = nullptr;
        RequireInvalidArgument([&] { writeIS_nid_postfix(nullptr, &io); }, "write accepted a null stream or write callback");
        Require(calls.empty() && std::memcmp(&io, &initial, sizeof(io)) == 0, "rejected write performed I/O or mutated state");
        partial = stream;
    }

    partial.read = nullptr;
    seekResult = 0;
    Jxr::BitIo io{7, 19, 13, ~8192, ring.data(), packet, &partial, 91};
    auto initial = io;
    calls.clear();
    RequireInvalidArgument([&] { readIS_nid_postfix(nullptr, &io); }, "read accepted a null read callback");
    Require(calls == std::vector<char>({'s'}) && lastPosition == 91, "null read callback was not reached after the seek");
    Require(std::memcmp(&io, &initial, sizeof(io)) == 0, "null read callback mutated state");

    Jxr::Stream observed = stream;
    observed.read = ObserveRead;
    observed.write = ObserveWrite;
    io = {7, 19, 13, ~8192, nullptr, reinterpret_cast<std::uint8_t*>(PacketSize), &observed, 91};
    initial = io;
    calls.clear();
    lastBuffer = ring.data();
    RequireInvalidArgument([&] { readIS_nid_postfix(nullptr, &io); }, "read accepted a null packet");
    Require(calls == std::vector<char>({'s', 'r'}) && lastBuffer == nullptr && lastSize == PacketSize, "null packet did not reach the read callback first");
    Require(io.referenceOffset == 91 + PacketSize && io.start == nullptr && io.shadow == initial.shadow, "null packet read did not stop at the shadow copy");

    for (std::int64_t result : {std::int64_t{-73}, std::numeric_limits<std::int64_t>::min()}) {
        io = initial;
        writeResult = result;
        calls.clear();
        lastBuffer = ring.data();
        Require(writeIS_nid_postfix(nullptr, &io) == result, "failed write of a null packet did not return the callback result");
        Require(calls == std::vector<char>({'w'}) && lastBuffer == nullptr && lastSize == PacketSize, "null packet did not reach the write callback");
        Require(std::memcmp(&io, &initial, sizeof(io)) == 0, "failed write of a null packet mutated state");
    }
    for (std::int64_t result : {std::int64_t{0}, std::int64_t{0x100000001}}) {
        io = initial;
        writeResult = result;
        calls.clear();
        RequireInvalidArgument([&] { writeIS_nid_postfix(nullptr, &io); }, "successful write advanced a null packet");
        Require(calls == std::vector<char>({'w'}) && std::memcmp(&io, &initial, sizeof(io)) == 0, "successful write of a null packet mutated state");
    }
}

void GuestMapping(Jxr::Stream& stream) {
    constexpr std::size_t mappingSize = 0x4000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, mappingSize, 0, 0, &physical) == 0, "allocate guest packet backing");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, mappingSize, 3, 0, physical, 0) == 0 && mapping, "map guest packet backing");
    Require(GuestArena::GuestArenaContains_nid_postfix(mapping, mappingSize), "packet mapping is outside the guest arena");
    auto* bytes = static_cast<std::uint8_t*>(mapping);
    std::fill_n(bytes, mappingSize, 0xcc);
    calls.clear();
    readResult = 0;
    seekResult = 0;
    Jxr::BitIo io{7, 19, 13, ~8192, bytes, bytes + PacketSize, &stream, 91};
    Require(readIS_nid_postfix(nullptr, &io) == 0, "guest packet refill failed");
    Require(calls == std::vector<char>({'s', 'r'}) && lastBuffer == bytes && lastSize == PacketSize, "guest packet callback arguments differ");
    Require(io.start == bytes + PacketSize && io.referenceOffset == 91 + PacketSize, "guest packet refill did not advance");
    for (std::size_t i = 0; i < mappingSize; ++i) {
        Require(bytes[i] == (i < PacketSize ? static_cast<std::uint8_t>(i * 17 + 91) : 0xcc), "guest packet refill changed unexpected bytes");
    }
#ifdef _WIN32
    Require(sceKernelMprotect(mapping, mappingSize, 1) == 0, "protect guest packet backing read-only");
    {
        GuestArena::HostWrite access(bytes, PacketSize);
        Require(!access.Open(), "read-only guest packet backing accepted a host write");
    }
    Jxr::Stream observedStream = stream;
    observedStream.read = ObserveRead;
    io = {7, 19, 13, ~8192, bytes, bytes + PacketSize, &observedStream, 91};
    const auto initial = io;
    std::array<std::uint8_t, mappingSize> before;
    std::copy_n(bytes, mappingSize, before.begin());
    calls.clear();
    RequireInvalidArgument([&] { readIS_nid_postfix(nullptr, &io); }, "read-only guest packet backing was not rejected");
    Require(calls == std::vector<char>({'s'}), "rejected guest packet write invoked the read callback");
    Require(std::memcmp(&io, &initial, sizeof(io)) == 0 && std::equal(before.begin(), before.end(), bytes), "rejected guest packet write mutated state or backing");
    Require(sceKernelMprotect(mapping, mappingSize, 3) == 0, "restore writable guest packet backing");
#endif
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), mappingSize) == 0, "unmap guest packet backing");
    Require(sceKernelReleaseDirectMemory(physical, mappingSize) == 0, "release guest packet backing");
}

constexpr std::size_t Max = std::numeric_limits<std::size_t>::max();

bool Filled(const std::uint8_t* begin, const std::uint8_t* end, std::uint8_t value) {
    return std::all_of(begin, end, [value](std::uint8_t byte) { return byte == value; });
}

Jxr::Stream* MemoryStream(void* buffer, std::size_t size) {
    Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, buffer, size) == 0 && stream, "memory stream creation failed");
    return stream;
}

void CloseMemory(Jxr::Stream* stream) {
    const auto close = stream->close;
    Require(close(&stream) == 0 && stream == nullptr, "memory stream close did not free and clear the pointer");
    Require(close(&stream) == 0 && stream == nullptr, "memory stream close of a null pointer failed");
}

void Allocation() {
    void* memory = &calls;
    Require(WMPAlloc_nid_postfix(&memory, 24) == 0 && memory && memory != &calls, "allocation of 24 bytes failed");
    auto* bytes = static_cast<std::uint8_t*>(memory);
    Require(Filled(bytes, bytes + 24, 0), "allocation is not zeroed");
    Require(WMPFree_nid_postfix(&memory) == 0 && memory == nullptr, "free did not release and clear the pointer");
    Require(WMPFree_nid_postfix(&memory) == 0 && memory == nullptr, "free of a null pointer failed");
    memory = &calls;
    Require(WMPAlloc_nid_postfix(&memory, Max) == -101 && memory == nullptr, "failed allocation did not report out of memory");
    RequireInvalidArgument([] { WMPAlloc_nid_postfix(nullptr, 24); }, "allocation accepted a null pointer");
    RequireInvalidArgument([] { WMPFree_nid_postfix(nullptr); }, "free accepted a null pointer");
}

void MemoryCreate() {
    std::array<std::uint8_t, 16> bytes{};
    for (void* buffer : {static_cast<void*>(bytes.data()), static_cast<void*>(nullptr)}) {
        auto* stream = reinterpret_cast<Jxr::Stream*>(&calls);
        Require(CreateWS_Memory_nid_postfix(&stream, buffer, bytes.size()) == 0 && stream && stream != reinterpret_cast<Jxr::Stream*>(&calls), "memory stream creation failed");
        Require(stream->state == std::array<std::uintptr_t, 4>{reinterpret_cast<std::uintptr_t>(buffer), bytes.size(), 0, 0} && stream->memory == 0, "memory stream state differs");
        Require(stream->end && stream->read && stream->write && stream->setPosition && stream->getPosition && stream->close, "memory stream callbacks are missing");
        if (buffer) {
            const std::array<std::uint8_t, 4> source{9, 8, 7, 6};
            std::array<std::uint8_t, 4> target{};
            std::size_t position = 99;
            Require(stream->write(stream, source.data(), source.size()) == 0 && std::equal(source.begin(), source.end(), bytes.begin()), "stored write callback differs");
            Require(stream->setPosition(stream, 1) == 0 && stream->read(stream, target.data(), 3) == 0 && std::equal(target.begin(), target.begin() + 3, source.begin() + 1), "stored read callback differs");
            Require(stream->getPosition(stream, &position) == 0 && position == 4 && stream->end(stream) == 0, "stored position callbacks differ");
            Require(stream->setPosition(stream, bytes.size()) == 0 && stream->end(stream) == 1, "stored end callback differs");
        }
        CloseMemory(stream);
    }
    RequireInvalidArgument([&] { CreateWS_Memory_nid_postfix(nullptr, bytes.data(), bytes.size()); }, "memory stream creation accepted a null stream pointer");
}

void MemoryRead() {
    const std::array<std::uint8_t, 8> source{1, 2, 3, 4, 5, 6, 7, 8};
    auto* stream = MemoryStream(const_cast<std::uint8_t*>(source.data()), source.size());
    std::array<std::uint8_t, 8> target;
    target.fill(0xcc);
    Require(ReadWS_Memory_nid_postfix(stream, target.data(), 3) == 0 && stream->state[2] == 3, "in-bounds read failed or did not advance");
    Require(std::equal(source.begin(), source.begin() + 3, target.begin()) && Filled(target.data() + 3, target.data() + 8, 0xcc), "in-bounds read data differs");
    target.fill(0xcc);
    Require(ReadWS_Memory_nid_postfix(stream, target.data(), 8) == 0 && stream->state[2] == 8, "read across the end did not clamp");
    Require(std::equal(source.begin() + 3, source.end(), target.begin()) && Filled(target.data() + 5, target.data() + 8, 0xcc), "clamped read data differs");
    target.fill(0xcc);
    Require(ReadWS_Memory_nid_postfix(stream, target.data(), 1) == 0 && stream->state[2] == 8 && Filled(target.data(), target.data() + 8, 0xcc), "read at the end copied or moved");
    Require(SetPosWS_Memory_nid_postfix(stream, 20) == 0 && ReadWS_Memory_nid_postfix(stream, nullptr, 4) == 0 && stream->state[2] == 20, "read past the end failed or moved");
    Require(ReadWS_Memory_nid_postfix(stream, nullptr, Max) == 0 && stream->state[2] == 20, "read past the end checked for overflow first");
    Require(SetPosWS_Memory_nid_postfix(stream, 1) == 0 && ReadWS_Memory_nid_postfix(stream, target.data(), Max) == -103, "wrapping read did not overflow");
    Require(stream->state[2] == 1 && Filled(target.data(), target.data() + 8, 0xcc), "wrapping read copied or moved");
    RequireInvalidArgument([&] { ReadWS_Memory_nid_postfix(stream, nullptr, 1); }, "read accepted a null destination");
    Require(stream->state[2] == 1, "rejected read moved");
    CloseMemory(stream);
}

void MemoryWrite() {
    const std::array<std::uint8_t, 8> source{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 8> target;
    target.fill(0xcc);
    auto* stream = MemoryStream(target.data(), target.size());
    Require(WriteWS_Memory_nid_postfix(stream, source.data(), 5) == 0 && stream->state[2] == 5, "in-bounds write failed or did not advance");
    Require(std::equal(source.begin(), source.begin() + 5, target.begin()) && Filled(target.data() + 5, target.data() + 8, 0xcc), "in-bounds write data differs");
    Require(WriteWS_Memory_nid_postfix(stream, source.data(), 4) == -103 && stream->state[2] == 5 && Filled(target.data() + 5, target.data() + 8, 0xcc), "write past the end did not overflow cleanly");
    Require(WriteWS_Memory_nid_postfix(stream, source.data(), 3) == 0 && stream->state[2] == 8 && std::equal(source.begin(), source.begin() + 3, target.begin() + 5), "write up to the end failed");
    Require(SetPosWS_Memory_nid_postfix(stream, 1) == 0 && WriteWS_Memory_nid_postfix(stream, source.data(), Max) == -103 && stream->state[2] == 1, "wrapping write did not overflow");
    Require(WriteWS_Memory_nid_postfix(stream, nullptr, 0) == 0 && stream->state[2] == 1, "empty in-bounds write failed");
    RequireInvalidArgument([&] { WriteWS_Memory_nid_postfix(stream, nullptr, 1); }, "write accepted a null source");
    Require(SetPosWS_Memory_nid_postfix(stream, 9) == 0 && WriteWS_Memory_nid_postfix(stream, source.data(), 0) == -103 && stream->state[2] == 9, "empty write past the end did not overflow");
    Require(std::equal(source.begin(), source.begin() + 5, target.begin()) && std::equal(source.begin(), source.begin() + 3, target.begin() + 5), "failed writes changed the buffer");
    CloseMemory(stream);
}

void MemoryPosition() {
    std::array<std::uint8_t, 8> bytes{};
    auto* stream = MemoryStream(bytes.data(), bytes.size());
    std::size_t position = 99;
    for (std::size_t offset : {std::size_t{0}, std::size_t{7}, std::size_t{8}, std::size_t{9}, Max}) {
        Require(SetPosWS_Memory_nid_postfix(stream, offset) == 0 && stream->state[2] == offset, "set position failed");
        Require(EOSWS_Memory_nid_postfix(stream) == (offset >= 8 ? 1 : 0), "end of stream differs");
        Require(GetPosWS_Memory_nid_postfix(stream, &position) == 0 && position == offset, "get position differs");
    }
    Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(bytes.data()) && stream->state[1] == 8, "position calls changed the buffer");
    RequireInvalidArgument([] { EOSWS_Memory_nid_postfix(nullptr); }, "end of stream accepted a null stream");
    RequireInvalidArgument([&] { ReadWS_Memory_nid_postfix(nullptr, bytes.data(), 1); }, "read accepted a null stream");
    RequireInvalidArgument([&] { WriteWS_Memory_nid_postfix(nullptr, bytes.data(), 1); }, "write accepted a null stream");
    RequireInvalidArgument([] { SetPosWS_Memory_nid_postfix(nullptr, 0); }, "set position accepted a null stream");
    RequireInvalidArgument([&] { GetPosWS_Memory_nid_postfix(nullptr, &position); }, "get position accepted a null stream");
    RequireInvalidArgument([&] { GetPosWS_Memory_nid_postfix(stream, nullptr); }, "get position accepted a null result");
    CloseMemory(stream);
}

void MemoryBitIo() {
    std::vector<std::uint8_t> source(2 * PacketSize + 100);
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint8_t>(i * 31 + (i >> 8) * 13);
    auto* stream = MemoryStream(source.data(), source.size());
    std::fill(ring.begin(), ring.end(), 0xcc);
    Jxr::BitIo io{7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, stream, PacketSize};
    Require(readIS_nid_postfix(nullptr, &io) == 0 && io.referenceOffset == 2 * PacketSize && io.start == ring.data() + PacketSize, "memory packet read did not advance");
    Require(stream->state[2] == 2 * PacketSize && std::equal(ring.begin(), ring.begin() + PacketSize, source.begin() + PacketSize), "memory packet read data differs");
    Require(Filled(ring.data() + PacketSize, ring.data() + ring.size(), 0xcc) && std::memcmp(&io.shadow, source.data() + PacketSize, sizeof(io.shadow)) == 0, "memory packet read changed the other half or shadow");
    io.current = ring.data();
    Require(readIS_nid_postfix(nullptr, &io) == 0 && io.referenceOffset == 3 * PacketSize && io.start == ring.data(), "clamped memory packet read did not advance");
    Require(stream->state[2] == 2 * PacketSize + 100 && std::equal(ring.begin() + PacketSize, ring.begin() + PacketSize + 100, source.begin() + 2 * PacketSize), "clamped memory packet read data differs");
    Require(Filled(ring.data() + PacketSize + 100, ring.data() + ring.size(), 0xcc) && std::equal(ring.begin(), ring.begin() + PacketSize, source.begin() + PacketSize), "clamped memory packet read changed bytes past the clamp");
    CloseMemory(stream);

    for (std::size_t i = 0; i < ring.size(); ++i) ring[i] = static_cast<std::uint8_t>(i * 23 + (i >> 8) * 5);
    std::vector<std::uint8_t> target(PacketSize + 100, 0xcc);
    stream = MemoryStream(target.data(), target.size());
    io = {7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, stream, 0};
    Require(writeIS_nid_postfix(nullptr, &io) == 0 && io.start == ring.data() + PacketSize && stream->state[2] == PacketSize, "memory packet write did not advance");
    Require(std::equal(ring.begin(), ring.begin() + PacketSize, target.begin()) && Filled(target.data() + PacketSize, target.data() + target.size(), 0xcc), "memory packet write data differs");
    io.current = ring.data();
    Require(writeIS_nid_postfix(nullptr, &io) == -103 && io.start == ring.data() + PacketSize && stream->state[2] == PacketSize, "overflowing memory packet write advanced");
    Require(std::equal(ring.begin(), ring.begin() + PacketSize, target.begin()) && Filled(target.data() + PacketSize, target.data() + target.size(), 0xcc), "overflowing memory packet write changed the buffer");
    CloseMemory(stream);
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    Jxr::Stream stream{};
    stream.read = Read;
    stream.write = Write;
    stream.setPosition = SetPosition;
    Reads(stream);
    Writes(stream);
    Buffered();
    CallbackMutation(stream);
    NullState(stream);
    GuestMapping(stream);
    Allocation();
    MemoryCreate();
    MemoryRead();
    MemoryWrite();
    MemoryPosition();
    MemoryBitIo();
    return 0;
}
