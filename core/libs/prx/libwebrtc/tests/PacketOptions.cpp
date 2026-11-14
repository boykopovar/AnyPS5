#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc PacketOptions check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

std::array<unsigned char, sizeof(WebRtcPacketOptions)> Bytes(const WebRtcPacketOptions& options) {
    std::array<unsigned char, sizeof(WebRtcPacketOptions)> bytes {};
    std::memcpy(bytes.data(), &options, bytes.size());
    return bytes;
}

WebRtcPacketOptions Filled(unsigned char byte) {
    WebRtcPacketOptions options;
    std::memset(&options, byte, sizeof(options));
    return options;
}

void TestDefault() {
    for (const unsigned char fill : {0x00, 0xA5}) {
        WebRtcPacketOptions options = Filled(fill);
        _ZN6webrtc13PacketOptionsC1Ev_nid_postfix(&options);
        const auto bytes = Bytes(options);
        const std::array<unsigned char, 14> expected {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
        Require(std::memcmp(bytes.data(), expected.data(), expected.size()) == 0);
        Require(options.packetId == -1);
        Require(options.isMedia);
        Require(!options.includedInFeedback && !options.includedInAllocation && !options.sendAsEct1);
        Require(!options.batchable && !options.lastPacketInBatch);
    }
}

void TestCopy() {
    for (unsigned bits = 0; bits < 64; ++bits) {
        WebRtcPacketOptions source = Filled(0x00);
        source.packetId = bits % 2 == 0 ? std::int64_t {0x1234} : std::int64_t {-0x123456789A};
        source.isMedia = (bits & 1) != 0;
        source.includedInFeedback = (bits & 2) != 0;
        source.includedInAllocation = (bits & 4) != 0;
        source.sendAsEct1 = (bits & 8) != 0;
        source.batchable = (bits & 16) != 0;
        source.lastPacketInBatch = (bits & 32) != 0;
        WebRtcPacketOptions copy = Filled(0x5A);
        _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(&copy, &source);
        const auto sourceBytes = Bytes(source);
        const auto copyBytes = Bytes(copy);
        Require(std::memcmp(copyBytes.data(), sourceBytes.data(), 14) == 0);
        Require(copy.packetId == source.packetId);
        Require(copy.isMedia == ((bits & 1) != 0));
        Require(copy.includedInFeedback == ((bits & 2) != 0));
        Require(copy.includedInAllocation == ((bits & 4) != 0));
        Require(copy.sendAsEct1 == ((bits & 8) != 0));
        Require(copy.batchable == ((bits & 16) != 0));
        Require(copy.lastPacketInBatch == ((bits & 32) != 0));
    }
}

void TestCopyOfDefault() {
    WebRtcPacketOptions source = Filled(0x00);
    _ZN6webrtc13PacketOptionsC1Ev_nid_postfix(&source);
    WebRtcPacketOptions copy = Filled(0xC3);
    _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(&copy, &source);
    Require(copy.packetId == -1 && copy.isMedia);
    Require(!copy.includedInFeedback && !copy.includedInAllocation && !copy.sendAsEct1);
    Require(!copy.batchable && !copy.lastPacketInBatch);
}

void TestSelfCopy() {
    WebRtcPacketOptions options = Filled(0x00);
    options.packetId = 0x7FFF;
    options.isMedia = false;
    options.includedInFeedback = true;
    options.sendAsEct1 = true;
    options.lastPacketInBatch = true;
    const auto expected = Bytes(options);
    _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(&options, &options);
    Require(Bytes(options) == expected);
}

void TestNullArgumentsThrow() {
    WebRtcPacketOptions options = Filled(0x00);
    options.packetId = 7;
    options.batchable = true;
    const auto expected = Bytes(options);
    RequireInvalidArgument([] { _ZN6webrtc13PacketOptionsC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(nullptr, &options); });
    RequireInvalidArgument([&] { _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(&options, nullptr); });
    Require(Bytes(options) == expected);
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (std::strcmp(argv[1], "default") == 0) {
        TestDefault();
        return 0;
    }
    if (std::strcmp(argv[1], "copy") == 0) {
        TestCopy();
        TestCopyOfDefault();
        TestSelfCopy();
        return 0;
    }
    if (std::strcmp(argv[1], "null") == 0) {
        TestNullArgumentsThrow();
        return 0;
    }
    return 2;
}
