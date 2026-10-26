#include "prx/libSceJxr/libSceJxr.hpp"
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
alignas(8192) std::array<std::uint8_t, 16384> ringStorage;
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

#ifdef _WIN32
std::int64_t APS5_VABI ObserveRead(Jxr::Stream*, void*, std::size_t) {
    calls.push_back('r');
    return 0;
}
#endif

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
    bool rejected = false;
    try {
        readIS_nid_postfix(nullptr, &io);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && calls == std::vector<char>({'s'}), "rejected guest packet write invoked the read callback");
    Require(std::memcmp(&io, &initial, sizeof(io)) == 0 && std::equal(before.begin(), before.end(), bytes), "rejected guest packet write mutated state or backing");
    Require(sceKernelMprotect(mapping, mappingSize, 3) == 0, "restore writable guest packet backing");
#endif
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), mappingSize) == 0, "unmap guest packet backing");
    Require(sceKernelReleaseDirectMemory(physical, mappingSize) == 0, "release guest packet backing");
}

}

int main() {
    Jxr::Stream stream{};
    stream.read = Read;
    stream.write = Write;
    stream.setPosition = SetPosition;
    Reads(stream);
    Writes(stream);
    Buffered();
    CallbackMutation(stream);
    GuestMapping(stream);
    return 0;
}
