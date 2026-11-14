#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void ExpectFailure(TAction action, const std::string& expectedSubstring) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(expectedSubstring) == std::string::npos) {
            throw std::runtime_error("expected exception containing '" + expectedSubstring + "', got: '" + error.what() + "'");
        }
        return;
    }
    throw std::runtime_error("expected failure containing '" + expectedSubstring + "', but succeeded");
}

std::uint32_t MakeHeader(std::uint32_t type, std::uint32_t count, std::uint32_t opcode, std::uint32_t flags = 0) {
    return (type << 30u) | ((count & 0x3fffu) << 16u) | ((opcode & 0xffu) << 8u) | (flags & 0xffu);
}

std::vector<std::uint32_t> MakePacket(std::uint32_t opcode, std::size_t payloadWords, std::uint32_t flags = 0) {
    const auto count = payloadWords == 0 ? 0u : static_cast<std::uint32_t>(payloadWords - 1u);
    std::vector<std::uint32_t> packet;
    packet.reserve(payloadWords + 1u);
    packet.push_back(MakeHeader(3u, count, opcode, flags));
    packet.resize(payloadWords + 1u, 0u);
    return packet;
}

std::string ExpectedUnknownName(std::uint32_t opcode) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "UNKNOWN_0x%02x", opcode & 0xffu);
    return buffer;
}

void TestType3HeaderFieldBounds() {
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 0u, 0x10u)) == 2u, "count 0 should yield 2 packet words");
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 1u, 0x10u)) == 3u, "count 1 should yield 3 packet words");
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 2u, 0x10u)) == 4u, "count 2 should yield 4 packet words");
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 100u, 0x10u)) == 102u, "count 100 should yield 102 packet words");
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 0x1fffu, 0x10u)) == 0x2001u, "count 0x1fff should yield 0x2001 packet words");
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(3u, 0x3fffu, 0x10u)) == 16385u, "maximum count 0x3fff should yield 16385 packet words");

    Require(!AgcDriver::Pm4::FillerPacket(MakeHeader(3u, 0u, 0x10u)), "type 3 packet must not be recognized as filler");
    Require(!AgcDriver::Pm4::FillerPacket(MakeHeader(3u, 0x3fffu, 0xffu)), "type 3 packet with max count must not be filler");
    Require(AgcDriver::Pm4::FillerPacket(0x80000000u), "type 2 packet must be recognized as filler");
    Require(AgcDriver::Pm4::FillerPacket(0xffff1000u), "legacy filler header 0xffff1000 must be recognized as filler");
    Require(AgcDriver::Pm4::PacketWords(0x80000000u) == 1u, "filler packet must have exactly 1 word");
    Require(AgcDriver::Pm4::PacketWords(0xffff1000u) == 1u, "legacy filler packet must have exactly 1 word");

    Require(!AgcDriver::Pm4::Predicated(MakeHeader(3u, 0u, 0x10u, 0u)), "unpredicated type 3 packet must report not predicated");
    Require(AgcDriver::Pm4::Predicated(MakeHeader(3u, 0u, 0x10u, 1u)), "predicated type 3 packet must report predicated");
    Require(!AgcDriver::Pm4::Predicated(0x80000001u), "filler packet must never report predicated");
}

void TestInvalidOpcodeRejection() {
    std::set<std::uint32_t> known;
    for (const auto& entry : AgcDriver::Pm4::Opcodes) {
        known.insert(entry.value);
    }
    Require(known.size() == 56u, "expected reference opcode catalog to contain 56 entries");

    const std::uint32_t boundaryOpcodes[] = {0x00u, 0x0fu, 0x14u, 0x21u, 0x7bu, 0x80u, 0xa0u, 0xffu};
    for (const auto opcode : boundaryOpcodes) {
        Require(known.find(opcode) == known.end(), "boundary opcode should not be in known catalog");
    }

    for (std::uint32_t opcode = 0; opcode <= 0xffu; ++opcode) {
        if (known.find(opcode) != known.end()) continue;

        const auto header = MakeHeader(3u, 0u, opcode);
        Require(AgcDriver::Pm4::KnownName(header).empty(), "invalid opcode must have empty KnownName");
        Require(AgcDriver::Pm4::Name(header) == ExpectedUnknownName(opcode), "invalid opcode name must match UNKNOWN_0x%02x pattern");
        Require(AgcDriver::Pm4::UnsupportedReason(header) == "opcode is not known in the reference", "invalid opcode must report unknown reference reason");

        const auto packet = MakePacket(opcode, 1u);
        ExpectFailure([&] { AgcDriver::Pm4::Validate(packet, 0u); }, "opcode is not known in the reference");
        ExpectFailure([&] { AgcDriver::Pm4::Validate(packet, 1u); }, "opcode is not known in the reference");
    }
}

void TestKnownOpcodeBounds() {
    for (const auto& entry : AgcDriver::Pm4::Opcodes) {
        const auto header = MakeHeader(3u, 0u, entry.value);
        Require(AgcDriver::Pm4::KnownName(header) == entry.name, "known opcode must match catalog entry name");
        Require(AgcDriver::Pm4::Name(header) == entry.name, "known opcode Name() must match catalog entry name");
    }

    const std::uint32_t unhandledOpcodes[] = {0x41u, 0x68u, 0x78u};
    for (const auto opcode : unhandledOpcodes) {
        const auto header = MakeHeader(3u, 0u, opcode);
        Require(AgcDriver::Pm4::UnsupportedReason(header) == "opcode is named but has no handler in the reference dispatch table", "unhandled opcode reason mismatch");
        const auto packet = MakePacket(opcode, 1u);
        ExpectFailure([&] { AgcDriver::Pm4::Validate(packet, 0u); }, "opcode is named but has no handler in the reference dispatch table");
    }

    const auto header3a = MakeHeader(3u, 0u, 0x3au);
    Require(!AgcDriver::Pm4::UnsupportedReason(header3a).empty(), "opcode 0x3a must report unsupported reason");
    ExpectFailure([&] { AgcDriver::Pm4::Validate(MakePacket(0x3au, 1u), 0u); }, "DRAW_INDEX_MULTI_INST is not implemented");

    const auto header8d = MakeHeader(3u, 0u, 0x8du);
    Require(!AgcDriver::Pm4::UnsupportedReason(header8d).empty(), "opcode 0x8d must report unsupported reason");
    ExpectFailure([&] { AgcDriver::Pm4::Validate(MakePacket(0x8du, 1u), 0u); }, "graphics draw, shader stages and guest render-target materialization are not implemented");

    const auto header8e = MakeHeader(3u, 0u, 0x8eu);
    Require(!AgcDriver::Pm4::UnsupportedReason(header8e).empty(), "opcode 0x8e must report unsupported reason");
    ExpectFailure([&] { AgcDriver::Pm4::Validate(MakePacket(0x8eu, 1u), 0u); }, "GPU LOD statistics are not implemented");
}

void TestCustomNopSubtypeBounds() {
    const auto unknownCustom = MakeHeader(3u, 0u, 0x10u, 0x01u << 2u);
    Require(AgcDriver::Pm4::KnownName(unknownCustom) == "UNKNOWN_CUSTOM", "custom NOP subtype 1 KnownName mismatch");
    Require(AgcDriver::Pm4::Name(unknownCustom) == "UNKNOWN_CUSTOM", "custom NOP subtype 1 Name mismatch");
    Require(AgcDriver::Pm4::UnsupportedReason(unknownCustom) == "custom packet has no implemented contract in the reference dispatch table", "custom NOP subtype 1 reason mismatch");
    ExpectFailure([&] { AgcDriver::Pm4::Validate(MakePacket(0x10u, 1u, 0x01u << 2u), 0u); }, "custom packet has no implemented contract in the reference dispatch table");

    const auto acquireMemCustom = MakeHeader(3u, 0u, 0x10u, 0x14u << 2u);
    Require(AgcDriver::Pm4::KnownName(acquireMemCustom) == "ACQUIRE_MEM_CUSTOM", "custom NOP subtype 0x14 KnownName mismatch");
    Require(AgcDriver::Pm4::UnsupportedReason(acquireMemCustom) == "guest cache actions and GPU release events are not implemented", "custom NOP subtype 0x14 reason mismatch");
    ExpectFailure([&] { AgcDriver::Pm4::Validate(MakePacket(0x10u, 1u, 0x14u << 2u), 0u); }, "guest cache actions and GPU release events are not implemented");
}

void TestPacketValidationBounds() {
    ExpectFailure([] { AgcDriver::Pm4::Validate({}, 0u); }, "truncated PM4 header");

    const std::uint32_t singleDwordType3[] = {MakeHeader(3u, 0u, 0x10u)};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(singleDwordType3, 0u); }, "truncated PM4 header or payload");

    const std::uint32_t fillerValid[] = {0x80000000u};
    AgcDriver::Pm4::Validate(fillerValid, 0u);

    const std::uint32_t fillerInvalidSize[] = {0x80000000u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(fillerInvalidSize, 0u); }, "invalid PM4 filler size");

    const std::uint32_t type0Packet[] = {MakeHeader(0u, 0u, 0x10u), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(type0Packet, 0u); }, "unsupported PM4 packet type");

    const std::uint32_t type1Packet[] = {MakeHeader(1u, 0u, 0x10u), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(type1Packet, 0u); }, "unsupported PM4 packet type");

    const std::uint32_t sizeMismatchTooShort[] = {MakeHeader(3u, 2u, 0x10u), 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(sizeMismatchTooShort, 0u); }, "invalid PM4 packet size");

    const std::uint32_t sizeMismatchTooLong[] = {MakeHeader(3u, 0u, 0x10u), 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(sizeMismatchTooLong, 0u); }, "invalid PM4 packet size");
}

void TestHeaderFlagsValidation() {
    const std::uint32_t nopWithBit0Flag[] = {MakeHeader(3u, 0u, 0x10u, 1u), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(nopWithBit0Flag, 0u); }, "unsupported NOP header flags");

    const std::uint32_t nopWithBit1Flag[] = {MakeHeader(3u, 0u, 0x10u, 2u), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(nopWithBit1Flag, 0u); }, "unsupported NOP header flags");

    const std::uint32_t setPredicationWithBadFlag[] = {MakeHeader(3u, 2u, 0x20u, 1u), 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setPredicationWithBadFlag, 0u); }, "PM4 header flags 0x1 are not implemented");
}

}

int main() {
    try {
        TestType3HeaderFieldBounds();
        TestInvalidOpcodeRejection();
        TestKnownOpcodeBounds();
        TestCustomNopSubtypeBounds();
        TestPacketValidationBounds();
        TestHeaderFlagsValidation();
    } catch (const std::exception& error) {
        std::cerr << "pm4 header bounds tests failed: " << error.what() << "\n";
        return 1;
    }
    std::cout << "pm4 header bounds tests passed\n";
    return 0;
}
