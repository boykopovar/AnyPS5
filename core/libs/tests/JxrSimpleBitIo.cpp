#include "prx/libSceJxr/libSceJxr.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" void JxrReferenceSimpleRead_nid_no_patch(const std::uint8_t* data, std::size_t size, const std::uint32_t* counts, std::size_t count, std::uint32_t* results, std::uint32_t* state);

namespace {

constexpr std::uint32_t FlushMarker = std::numeric_limits<std::uint32_t>::max();
std::vector<std::uint8_t> source;
std::size_t sourceOffset;
std::size_t readCalls;
std::size_t lastSize;
Jxr::Stream* lastStream;

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "JXR simple bit I/O: %s\n", message);
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

std::int64_t APS5_VABI ReadSource(Jxr::Stream* stream, void* buffer, std::size_t size) {
    ++readCalls;
    lastStream = stream;
    lastSize = size;
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    for (std::size_t i = 0; i < size; ++i, ++sourceOffset) {
        bytes[i] = sourceOffset < source.size() ? source[sourceOffset] : 0;
    }
    return -1;
}

Jxr::Stream SourceStream() {
    Jxr::Stream stream{};
    stream.read = ReadSource;
    sourceOffset = 0;
    readCalls = 0;
    return stream;
}

void KnownPattern() {
    source = {0xb5, 0x79, 0xa2, 0x46, 0x8a, 0xcf, 0x10};
    auto stream = SourceStream();
    Jxr::SimpleBitIo io{nullptr, 99, 0xff, 7};
    Require(attach_SB_nid_postfix(&io, &stream) == 0, "attach failed");
    Require(io.stream == &stream && io.bytesRead == 0 && io.accumulator == 0 && io.bitsLeft == 0, "attach did not reset the reader");
    Require(getBit32_SB_nid_postfix(&io, 0) == 0 && readCalls == 0, "zero-bit read consumed input");
    Require(getBit32_SB_nid_postfix(&io, 3) == 5, "3-bit read differs");
    Require(readCalls == 1 && lastStream == &stream && lastSize == 1, "read callback arguments differ");
    Require(getBit32_SB_nid_postfix(&io, 16) == 0xabcd, "16-bit read differs");
    Require(getBit32_SB_nid_postfix(&io, 32) == 0x12345678, "32-bit read differs");
    Require(getBit32_SB_nid_postfix(&io, 1) == 1, "1-bit read differs");
    Require(getByteRead_SB_nid_postfix(&io) == 7 && readCalls == 7 && io.bitsLeft == 4 && io.accumulator == 0, "byte count or buffered bits differ");
    RequireInvalidArgument([&] { detach_SB_nid_postfix(&io); }, "detach accepted unread bits");
    Require(io.stream == &stream, "rejected detach changed the reader");
    flushToByte_SB_nid_postfix(&io);
    Require(io.accumulator == 0 && io.bitsLeft == 0 && io.bytesRead == 7, "flush differs");
    Require(detach_SB_nid_postfix(&io) == 0 && io.stream == nullptr && io.bytesRead == 7, "detach did not clear only the stream");
}

void Reference() {
    std::uint32_t state = 0x6c8e9cf5;
    auto next = [&] {
        state = state * 1664525 + 1013904223;
        return state;
    };
    source.resize(4096);
    for (auto& byte : source) byte = static_cast<std::uint8_t>(next() >> 24);
    std::vector<std::uint32_t> counts;
    std::size_t bits = 0;
    while (bits < (source.size() - 8) * 8) {
        const auto choice = (next() >> 16) % 16;
        const auto count = choice == 0 ? FlushMarker : (next() >> 16) % 41;
        counts.push_back(count);
        if (count != FlushMarker) bits += count + 8;
    }
    std::vector<std::uint32_t> expected(counts.size());
    std::vector<std::uint32_t> expectedState(counts.size() * 3);
    JxrReferenceSimpleRead_nid_no_patch(source.data(), source.size(), counts.data(), counts.size(), expected.data(), expectedState.data());

    auto stream = SourceStream();
    Jxr::SimpleBitIo io{};
    Require(attach_SB_nid_postfix(&io, &stream) == 0, "reference attach failed");
    for (std::size_t i = 0; i < counts.size(); ++i) {
        std::uint32_t result = 0;
        if (counts[i] == FlushMarker) {
            flushToByte_SB_nid_postfix(&io);
        } else {
            result = getBit32_SB_nid_postfix(&io, counts[i]);
        }
        Require(result == expected[i], "read differs from jxrlib");
        Require(getByteRead_SB_nid_postfix(&io) == expectedState[i * 3] && io.accumulator == expectedState[i * 3 + 1] && io.bitsLeft == expectedState[i * 3 + 2], "reader state differs from jxrlib");
    }
    Require(readCalls == io.bytesRead && sourceOffset == io.bytesRead, "read callback count differs from the byte count");
}

void Rejected() {
    Jxr::SimpleBitIo io{nullptr, 3, 0x80, 0};
    RequireInvalidArgument([&] { getBit32_SB_nid_postfix(&io, 1); }, "read accepted a null stream");
    Jxr::Stream stream{};
    io.stream = &stream;
    RequireInvalidArgument([&] { getBit32_SB_nid_postfix(&io, 1); }, "read accepted a null read callback");
    Require(io.bytesRead == 3 && io.accumulator == 0x80 && io.bitsLeft == 0, "rejected read changed the reader");
    io.bitsLeft = 9;
    RequireInvalidArgument([&] { getBit32_SB_nid_postfix(&io, 0); }, "read accepted more than 8 buffered bits");
    RequireInvalidArgument([] { attach_SB_nid_postfix(nullptr, nullptr); }, "attach accepted a null reader");
    RequireInvalidArgument([] { getBit32_SB_nid_postfix(nullptr, 1); }, "read accepted a null reader");
    RequireInvalidArgument([] { getByteRead_SB_nid_postfix(nullptr); }, "byte count accepted a null reader");
    RequireInvalidArgument([] { flushToByte_SB_nid_postfix(nullptr); }, "flush accepted a null reader");
    RequireInvalidArgument([] { detach_SB_nid_postfix(nullptr); }, "detach accepted a null reader");
}

void GuestPointers() {
    auto* const unmapped = reinterpret_cast<Jxr::SimpleBitIo*>(0x10);
    errno = 71;
    RequireInvalidArgument([&] { attach_SB_nid_postfix(unmapped, nullptr); }, "attach accepted an unmapped reader");
    RequireInvalidArgument([&] { getBit32_SB_nid_postfix(unmapped, 1); }, "read accepted an unmapped reader");
    RequireInvalidArgument([&] { getByteRead_SB_nid_postfix(unmapped); }, "byte count accepted an unmapped reader");
    RequireInvalidArgument([&] { flushToByte_SB_nid_postfix(unmapped); }, "flush accepted an unmapped reader");
    RequireInvalidArgument([&] { detach_SB_nid_postfix(unmapped); }, "detach accepted an unmapped reader");
    Jxr::SimpleBitIo io{reinterpret_cast<Jxr::Stream*>(0x10), 3, 0x80, 0};
    RequireInvalidArgument([&] { getBit32_SB_nid_postfix(&io, 1); }, "read accepted an unmapped stream");
    Require(io.bytesRead == 3 && io.accumulator == 0x80 && io.bitsLeft == 0, "rejected unmapped stream changed the reader");
    Require(errno == 71, "unmapped pointers changed errno");

    source = {0xc3};
    auto stream = SourceStream();
    alignas(Jxr::SimpleBitIo) std::uint8_t raw[sizeof(Jxr::SimpleBitIo)];
    std::memset(raw, 0xa5, sizeof(raw));
    auto* padded = reinterpret_cast<Jxr::SimpleBitIo*>(raw);
    Require(attach_SB_nid_postfix(padded, &stream) == 0 && getBit32_SB_nid_postfix(padded, 4) == 0xc, "padded reader differs");
    flushToByte_SB_nid_postfix(padded);
    Require(detach_SB_nid_postfix(padded) == 0, "padded detach failed");
    Require(raw[13] == 0xa5 && raw[14] == 0xa5 && raw[15] == 0xa5 && raw[20] == 0xa5 && raw[23] == 0xa5, "padding bytes were overwritten");
}

}

int main() {
    KnownPattern();
    Reference();
    Rejected();
    GuestPointers();
    return 0;
}
