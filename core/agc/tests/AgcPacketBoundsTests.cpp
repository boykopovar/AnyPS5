#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::uint32_t OpcodeSetContextReg = 0x69u;
constexpr std::uint32_t OpcodeSetShReg = 0x76u;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template<typename TAction>
void ExpectFailure(TAction action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    Require(failed);
}

std::uint32_t MakeHeader(std::uint32_t opcode, std::uint32_t count, std::uint32_t flags = 0u) {
    return (3u << 30u) | ((count & 0x3fffu) << 16u) | ((opcode & 0xffu) << 8u) | (flags & 0xffu);
}

std::vector<std::uint32_t> MakePacket(std::uint32_t opcode, std::uint32_t offset, std::initializer_list<std::uint32_t> values) {
    const auto count = static_cast<std::uint32_t>(values.size());
    std::vector<std::uint32_t> packet;
    packet.reserve(values.size() + 2u);
    packet.push_back(MakeHeader(opcode, count));
    packet.push_back(offset);
    packet.insert(packet.end(), values.begin(), values.end());
    return packet;
}

std::vector<std::uint32_t> MakePacketWithCount(std::uint32_t opcode, std::uint32_t offset, std::size_t valueCount) {
    const auto count = static_cast<std::uint32_t>(valueCount);
    std::vector<std::uint32_t> packet;
    packet.reserve(valueCount + 2u);
    packet.push_back(MakeHeader(opcode, count));
    packet.push_back(offset);
    packet.resize(valueCount + 2u, 1u);
    return packet;
}

std::vector<std::uint32_t> MakeEmptyPacket(std::uint32_t opcode, std::uint32_t offset) {
    return {MakeHeader(opcode, 0u), offset};
}

void TestInvalidRegisterOffsets() {
    const std::uint32_t invalidOffsets[] = {
        0xffffffffu,
        0x00010000u,
        0x00010001u,
        0x00020000u,
        0x00100000u,
        0x01000000u,
        0x10000000u,
        0x80000000u,
        0x80010000u,
        0xffff0000u,
        0xfffffffeu
    };

    for (const auto opcode : {OpcodeSetShReg, OpcodeSetContextReg}) {
        for (const auto offset : invalidOffsets) {
            const auto packet = MakePacket(opcode, offset, {0x42u});
            ExpectFailure([&] { AgcDriver::Pm4::Validate(packet, 0u); });

            AgcDriver::QueueState state;
            ExpectFailure([&] { AgcDriver::Pm4::Execute(packet, state); });
        }
    }
}

void TestRegisterRangeOverflow() {
    for (const auto opcode : {OpcodeSetShReg, OpcodeSetContextReg}) {
        const auto overflowFromMax = MakePacket(opcode, 0xffffu, {1u, 2u});
        ExpectFailure([&] { AgcDriver::Pm4::Validate(overflowFromMax, 0u); });
        AgcDriver::QueueState stateMax;
        ExpectFailure([&] { AgcDriver::Pm4::Execute(overflowFromMax, stateMax); });

        const auto overflowFromNearMax = MakePacket(opcode, 0xfffeu, {1u, 2u, 3u});
        ExpectFailure([&] { AgcDriver::Pm4::Validate(overflowFromNearMax, 0u); });
        AgcDriver::QueueState stateNearMax;
        ExpectFailure([&] { AgcDriver::Pm4::Execute(overflowFromNearMax, stateNearMax); });

        const auto overflowMultiWord = MakePacketWithCount(opcode, 0xff00u, 257u);
        ExpectFailure([&] { AgcDriver::Pm4::Validate(overflowMultiWord, 0u); });
        AgcDriver::QueueState stateMulti;
        ExpectFailure([&] { AgcDriver::Pm4::Execute(overflowMultiWord, stateMulti); });

        const auto overflowLargeSpan = MakePacketWithCount(opcode, 0xe000u, 0x2001u);
        ExpectFailure([&] { AgcDriver::Pm4::Validate(overflowLargeSpan, 0u); });
        AgcDriver::QueueState stateSpan;
        ExpectFailure([&] { AgcDriver::Pm4::Execute(overflowLargeSpan, stateSpan); });
    }
}

void TestMissingPayloadValues() {
    for (const auto opcode : {OpcodeSetShReg, OpcodeSetContextReg}) {
        const auto emptyPayload = MakeEmptyPacket(opcode, 0x100u);
        ExpectFailure([&] { AgcDriver::Pm4::Validate(emptyPayload, 0u); });

        const std::array<std::uint32_t, 1> headerOnly{MakeHeader(opcode, 0u)};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(headerOnly, 0u); });
    }
}

void TestQueueRestrictions() {
    const auto validContextPacket = MakePacket(OpcodeSetContextReg, 0x100u, {0x42u});
    ExpectFailure([&] { AgcDriver::Pm4::Validate(validContextPacket, 0x20u); });

    const auto validShPacket = MakePacket(OpcodeSetShReg, 0x100u, {0x42u});
    AgcDriver::Pm4::Validate(validShPacket, 0x20u);

    const auto invalidShOffset = MakePacket(OpcodeSetShReg, 0x10000u, {0x42u});
    ExpectFailure([&] { AgcDriver::Pm4::Validate(invalidShOffset, 0x20u); });

    const auto sentinelSh = MakePacket(OpcodeSetShReg, 0xffffffffu, {0x42u});
    ExpectFailure([&] { AgcDriver::Pm4::Validate(sentinelSh, 0x20u); });

    const auto overflowSh = MakePacket(OpcodeSetShReg, 0xffffu, {1u, 2u});
    ExpectFailure([&] { AgcDriver::Pm4::Validate(overflowSh, 0x20u); });
}

void TestValidRegisterExecutionAndStatePreservation() {
    AgcDriver::QueueState state;

    const auto validSh = MakePacket(OpcodeSetShReg, 0x200u, {0x1111u, 0x2222u});
    AgcDriver::Pm4::Validate(validSh, 0u);
    AgcDriver::Pm4::Execute(validSh, state);
    Require(state.shader.at(0x200u) == 0x1111u);
    Require(state.shader.at(0x201u) == 0x2222u);

    const auto validContext = MakePacket(OpcodeSetContextReg, 0x300u, {0x3333u, 0x4444u});
    AgcDriver::Pm4::Validate(validContext, 0u);
    AgcDriver::Pm4::Execute(validContext, state);
    Require(state.context.at(0x300u) == 0x3333u);
    Require(state.context.at(0x301u) == 0x4444u);

    const auto maxValidSh = MakePacket(OpcodeSetShReg, 0xffffu, {0x9999u});
    AgcDriver::Pm4::Validate(maxValidSh, 0u);
    AgcDriver::Pm4::Execute(maxValidSh, state);
    Require(state.shader.at(0xffffu) == 0x9999u);

    const auto maxValidContext = MakePacket(OpcodeSetContextReg, 0xffffu, {0x8888u});
    AgcDriver::Pm4::Validate(maxValidContext, 0u);
    AgcDriver::Pm4::Execute(maxValidContext, state);
    Require(state.context.at(0xffffu) == 0x8888u);

    const auto shaderSnapshot = state.shader;
    const auto contextSnapshot = state.context;

    const auto invalidSh = MakePacket(OpcodeSetShReg, 0x10000u, {0x5555u});
    ExpectFailure([&] { AgcDriver::Pm4::Execute(invalidSh, state); });
    Require(state.shader == shaderSnapshot);

    const auto invalidContext = MakePacket(OpcodeSetContextReg, 0xffffffffu, {0x6666u});
    ExpectFailure([&] { AgcDriver::Pm4::Execute(invalidContext, state); });
    Require(state.context == contextSnapshot);
}

}

int main() {
    TestInvalidRegisterOffsets();
    TestRegisterRangeOverflow();
    TestMissingPayloadValues();
    TestQueueRestrictions();
    TestValidRegisterExecutionAndStatePreservation();
    return 0;
}
