#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <span>
#include <vector>

namespace {

constexpr std::uint32_t OpcodeNop = 0x10u;
constexpr std::uint32_t OpcodeSetBase = 0x11u;
constexpr std::uint32_t OpcodeClearState = 0x12u;
constexpr std::uint32_t OpcodeIndexBufferSize = 0x13u;
constexpr std::uint32_t OpcodeDispatchDirect = 0x15u;
constexpr std::uint32_t OpcodeDispatchIndirect = 0x16u;
constexpr std::uint32_t OpcodeSetPredication = 0x20u;
constexpr std::uint32_t OpcodeDrawIndex2 = 0x27u;
constexpr std::uint32_t OpcodeDrawIndexAuto = 0x2du;
constexpr std::uint32_t OpcodeDrawIndexOffset2 = 0x35u;
constexpr std::uint32_t OpcodeIndirectBuffer = 0x3fu;
constexpr std::uint32_t OpcodeEventWriteEop = 0x47u;
constexpr std::uint32_t OpcodeAcquireMem = 0x58u;
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

std::uint32_t MakeHeader(std::uint32_t type, std::uint32_t count, std::uint32_t opcode, std::uint32_t flags = 0u) {
    return (type << 30u) | ((count & 0x3fffu) << 16u) | ((opcode & 0xffu) << 8u) | (flags & 0xffu);
}

void TestReservedHeaderType0() {
    const auto nopType0 = std::array<std::uint32_t, 2>{MakeHeader(0u, 0u, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(nopType0, 0u); });

    const auto setShType0 = std::array<std::uint32_t, 3>{MakeHeader(0u, 1u, OpcodeSetShReg), 0x100u, 0x42u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setShType0, 0u); });

    const auto setContextType0 = std::array<std::uint32_t, 3>{MakeHeader(0u, 1u, OpcodeSetContextReg), 0x100u, 0x42u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setContextType0, 0u); });

    const auto autoDrawType0 = std::array<std::uint32_t, 3>{MakeHeader(0u, 1u, OpcodeDrawIndexAuto), 3u, 2u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(autoDrawType0, 0u); });

    const auto rawZeroHeader = std::array<std::uint32_t, 2>{0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(rawZeroHeader, 0u); });

    const auto rawZeroSingle = std::array<std::uint32_t, 1>{0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(rawZeroSingle, 0u); });

    Require(!AgcDriver::Pm4::FillerPacket(MakeHeader(0u, 0u, OpcodeNop)));
    Require(!AgcDriver::Pm4::FillerPacket(0u));
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(0u, 0u, OpcodeNop)) == 2u);
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(0u, 10u, OpcodeNop)) == 12u);
    Require(!AgcDriver::Pm4::Predicated(MakeHeader(0u, 0u, OpcodeNop, 0u)));
    Require(AgcDriver::Pm4::Predicated(MakeHeader(0u, 0u, OpcodeNop, 1u)));
}

void TestReservedHeaderType1() {
    const auto nopType1 = std::array<std::uint32_t, 2>{MakeHeader(1u, 0u, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(nopType1, 0u); });

    const auto nopType1TwoWords = std::array<std::uint32_t, 3>{MakeHeader(1u, 1u, OpcodeNop), 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(nopType1TwoWords, 0u); });

    const auto setShType1 = std::array<std::uint32_t, 3>{MakeHeader(1u, 1u, OpcodeSetShReg), 0x200u, 0x55u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setShType1, 0u); });

    const auto setContextType1 = std::array<std::uint32_t, 3>{MakeHeader(1u, 1u, OpcodeSetContextReg), 0x300u, 0x66u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setContextType1, 0u); });

    const auto dispatchDirectType1 = std::array<std::uint32_t, 4>{MakeHeader(1u, 2u, OpcodeDispatchDirect), 1u, 1u, 1u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(dispatchDirectType1, 0u); });

    const auto dispatchIndirectType1 = std::array<std::uint32_t, 4>{MakeHeader(1u, 2u, OpcodeDispatchIndirect), 1u, 1u, 1u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(dispatchIndirectType1, 0u); });

    const auto autoDrawType1 = std::array<std::uint32_t, 3>{MakeHeader(1u, 1u, OpcodeDrawIndexAuto), 3u, 2u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(autoDrawType1, 0u); });

    const auto drawIndex2Type1 = std::array<std::uint32_t, 6>{MakeHeader(1u, 4u, OpcodeDrawIndex2), 0u, 0u, 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(drawIndex2Type1, 0u); });

    const auto drawIndexOffset2Type1 = std::array<std::uint32_t, 5>{MakeHeader(1u, 3u, OpcodeDrawIndexOffset2), 0u, 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(drawIndexOffset2Type1, 0u); });

    const auto indirectBufferType1 = std::array<std::uint32_t, 4>{MakeHeader(1u, 2u, OpcodeIndirectBuffer), 0x1000u, 0u, 0x100u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(indirectBufferType1, 0u); });

    const auto type1Single = std::array<std::uint32_t, 1>{MakeHeader(1u, 0u, OpcodeNop)};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(type1Single, 0u); });

    Require(!AgcDriver::Pm4::FillerPacket(MakeHeader(1u, 0u, OpcodeNop)));
    Require(!AgcDriver::Pm4::FillerPacket(0x40000000u));
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(1u, 0u, OpcodeNop)) == 2u);
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(1u, 5u, OpcodeNop)) == 7u);
    Require(AgcDriver::Pm4::PacketWords(MakeHeader(1u, 0x3fffu, OpcodeNop)) == 16385u);
    Require(!AgcDriver::Pm4::Predicated(MakeHeader(1u, 0u, OpcodeNop, 0u)));
    Require(AgcDriver::Pm4::Predicated(MakeHeader(1u, 0u, OpcodeNop, 1u)));
}

void TestReservedHeaderType2() {
    const std::array<std::uint32_t, 1> validFiller{0x80000000u};
    AgcDriver::Pm4::Validate(validFiller, 0u);

    const std::array<std::uint32_t, 1> legacyFiller{0xffff1000u};
    AgcDriver::Pm4::Validate(legacyFiller, 0u);

    const std::array<std::uint32_t, 1> arbitraryType2{0x80001234u};
    AgcDriver::Pm4::Validate(arbitraryType2, 0u);

    const auto fillerWithSize2 = std::array<std::uint32_t, 2>{0x80000000u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(fillerWithSize2, 0u); });

    const auto fillerWithSize3 = std::array<std::uint32_t, 3>{0x80000000u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(fillerWithSize3, 0u); });

    const auto fillerWithSize4 = std::array<std::uint32_t, 4>{0x80000000u, 1u, 2u, 3u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(fillerWithSize4, 0u); });

    const auto legacyWithSize2 = std::array<std::uint32_t, 2>{0xffff1000u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(legacyWithSize2, 0u); });

    const auto legacyWithSize3 = std::array<std::uint32_t, 3>{0xffff1000u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(legacyWithSize3, 0u); });

    const auto type2CountPayload = std::array<std::uint32_t, 3>{0x80010000u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(type2CountPayload, 0u); });

    const auto type2MaxPayload = std::array<std::uint32_t, 2>{0xbfffffffu, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(type2MaxPayload, 0u); });

    Require(AgcDriver::Pm4::FillerPacket(0x80000000u));
    Require(AgcDriver::Pm4::FillerPacket(0x80000001u));
    Require(AgcDriver::Pm4::FillerPacket(0x80010000u));
    Require(AgcDriver::Pm4::FillerPacket(0xbfffffffu));
    Require(AgcDriver::Pm4::FillerPacket(0xffff1000u));
    Require(AgcDriver::Pm4::PacketWords(0x80000000u) == 1u);
    Require(AgcDriver::Pm4::PacketWords(0x80000001u) == 1u);
    Require(AgcDriver::Pm4::PacketWords(0x80010000u) == 1u);
    Require(AgcDriver::Pm4::PacketWords(0xbfffffffu) == 1u);
    Require(AgcDriver::Pm4::PacketWords(0xffff1000u) == 1u);
    Require(!AgcDriver::Pm4::Predicated(0x80000000u));
    Require(!AgcDriver::Pm4::Predicated(0x80000001u));
    Require(!AgcDriver::Pm4::Predicated(0xffff1001u));
}

void TestInvalidHeaderTypes4To7() {
    for (const std::uint32_t nibble : {0x40000000u, 0x50000000u, 0x60000000u, 0x70000000u}) {
        const auto nopPacket = std::array<std::uint32_t, 2>{nibble | (OpcodeNop << 8u), 0u};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(nopPacket, 0u); });

        const auto regPacket = std::array<std::uint32_t, 3>{nibble | (OpcodeSetShReg << 8u) | (1u << 16u), 0x100u, 0x42u};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(regPacket, 0u); });

        const auto contextPacket = std::array<std::uint32_t, 3>{nibble | (OpcodeSetContextReg << 8u) | (1u << 16u), 0x100u, 0x42u};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(contextPacket, 0u); });

        const auto singleWord = std::array<std::uint32_t, 1>{nibble};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(singleWord, 0u); });
    }

    for (const std::uint32_t type3Bit : {4u, 5u, 6u, 7u}) {
        const auto header3Bit = (type3Bit << 29u) | (OpcodeNop << 8u);
        const auto packetWithPayload = std::array<std::uint32_t, 3>{header3Bit, 0u, 0u};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(packetWithPayload, 0u); });
    }

    for (const std::uint32_t typeNibble : {4u, 5u, 6u, 7u}) {
        const auto header = (typeNibble << 28u) | (OpcodeNop << 8u);
        const auto packet = std::array<std::uint32_t, 2>{header, 0u};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(packet, 0u); });
    }
}

void TestZeroCountPayloads() {
    ExpectFailure([&] { AgcDriver::Pm4::Validate(std::span<const std::uint32_t>{}, 0u); });

    const std::uint32_t opcodes[] = {
        OpcodeNop,
        OpcodeSetBase,
        OpcodeClearState,
        OpcodeIndexBufferSize,
        OpcodeDispatchDirect,
        OpcodeDispatchIndirect,
        OpcodeSetPredication,
        OpcodeDrawIndex2,
        OpcodeDrawIndexAuto,
        OpcodeDrawIndexOffset2,
        OpcodeIndirectBuffer,
        OpcodeEventWriteEop,
        OpcodeAcquireMem,
        OpcodeSetContextReg,
        OpcodeSetShReg
    };
    for (const auto opcode : opcodes) {
        const auto headerOnly = std::array<std::uint32_t, 1>{MakeHeader(3u, 0u, opcode)};
        ExpectFailure([&] { AgcDriver::Pm4::Validate(headerOnly, 0u); });
    }

    const auto regShZeroValues = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeSetShReg), 0x100u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(regShZeroValues, 0u); });

    const auto regContextZeroValues = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeSetContextReg), 0x100u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(regContextZeroValues, 0u); });

    const auto autoDrawUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeDrawIndexAuto), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(autoDrawUnderflow, 0u); });

    const auto dispatchDirectUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeDispatchDirect), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(dispatchDirectUnderflow, 0u); });

    const auto dispatchIndirectUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeDispatchIndirect), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(dispatchIndirectUnderflow, 0u); });

    const auto drawIndex2Underflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeDrawIndex2), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(drawIndex2Underflow, 0u); });

    const auto drawIndexOffset2Underflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeDrawIndexOffset2), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(drawIndexOffset2Underflow, 0u); });

    const auto indirectBufferUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeIndirectBuffer), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(indirectBufferUnderflow, 0u); });

    const auto setBaseUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeSetBase), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setBaseUnderflow, 0u); });

    const auto setPredicationUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeSetPredication), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(setPredicationUnderflow, 0u); });

    const auto eventWriteEopUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeEventWriteEop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(eventWriteEopUnderflow, 0u); });

    const auto acquireMemUnderflow = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeAcquireMem), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(acquireMemUnderflow, 0u); });
}

void TestPayloadCountMismatchBounds() {
    const auto count0Size3 = std::array<std::uint32_t, 3>{MakeHeader(3u, 0u, OpcodeNop), 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count0Size3, 0u); });

    const auto count0Size4 = std::array<std::uint32_t, 4>{MakeHeader(3u, 0u, OpcodeNop), 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count0Size4, 0u); });

    const auto count1Size2 = std::array<std::uint32_t, 2>{MakeHeader(3u, 1u, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count1Size2, 0u); });

    const auto count1Size4 = std::array<std::uint32_t, 4>{MakeHeader(3u, 1u, OpcodeNop), 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count1Size4, 0u); });

    const auto count2Size2 = std::array<std::uint32_t, 2>{MakeHeader(3u, 2u, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count2Size2, 0u); });

    const auto count2Size3 = std::array<std::uint32_t, 3>{MakeHeader(3u, 2u, OpcodeNop), 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count2Size3, 0u); });

    const auto count2Size5 = std::array<std::uint32_t, 5>{MakeHeader(3u, 2u, OpcodeNop), 0u, 0u, 0u, 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(count2Size5, 0u); });

    const auto countMaxMismatch = std::array<std::uint32_t, 2>{MakeHeader(3u, 0x3fffu, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(countMaxMismatch, 0u); });

    const auto countMidMismatch = std::array<std::uint32_t, 2>{MakeHeader(3u, 0x1fffu, OpcodeNop), 0u};
    ExpectFailure([&] { AgcDriver::Pm4::Validate(countMidMismatch, 0u); });
}

void TestExecutionStatePreservation() {
    AgcDriver::QueueState state;

    const auto validSh = std::array<std::uint32_t, 3>{MakeHeader(3u, 1u, OpcodeSetShReg), 0x200u, 0xbeefu};
    AgcDriver::Pm4::Validate(validSh, 0u);
    AgcDriver::Pm4::Execute(validSh, state);
    Require(state.shader.at(0x200u) == 0xbeefu);

    const auto validContext = std::array<std::uint32_t, 3>{MakeHeader(3u, 1u, OpcodeSetContextReg), 0x300u, 0xfeedu};
    AgcDriver::Pm4::Validate(validContext, 0u);
    AgcDriver::Pm4::Execute(validContext, state);
    Require(state.context.at(0x300u) == 0xfeedu);

    const auto shaderSnapshot = state.shader;
    const auto contextSnapshot = state.context;

    const auto invalidType0 = std::array<std::uint32_t, 3>{MakeHeader(0u, 1u, OpcodeSetShReg), 0x200u, 0x1111u};
    ExpectFailure([&] {
        AgcDriver::Pm4::Validate(invalidType0, 0u);
        AgcDriver::Pm4::Execute(invalidType0, state);
    });
    Require(state.shader == shaderSnapshot);
    Require(state.context == contextSnapshot);

    const auto invalidType1 = std::array<std::uint32_t, 3>{MakeHeader(1u, 1u, OpcodeSetShReg), 0x200u, 0x2222u};
    ExpectFailure([&] {
        AgcDriver::Pm4::Validate(invalidType1, 0u);
        AgcDriver::Pm4::Execute(invalidType1, state);
    });
    Require(state.shader == shaderSnapshot);
    Require(state.context == contextSnapshot);

    const auto invalidType2WithPayload = std::array<std::uint32_t, 3>{0x80000000u, 0x200u, 0x3333u};
    ExpectFailure([&] {
        AgcDriver::Pm4::Validate(invalidType2WithPayload, 0u);
        AgcDriver::Pm4::Execute(invalidType2WithPayload, state);
    });
    Require(state.shader == shaderSnapshot);
    Require(state.context == contextSnapshot);

    const auto invalidTypeNibble4 = std::array<std::uint32_t, 3>{0x40000000u | (OpcodeSetShReg << 8u) | (1u << 16u), 0x200u, 0x4444u};
    ExpectFailure([&] {
        AgcDriver::Pm4::Validate(invalidTypeNibble4, 0u);
        AgcDriver::Pm4::Execute(invalidTypeNibble4, state);
    });
    Require(state.shader == shaderSnapshot);
    Require(state.context == contextSnapshot);

    const auto invalidZeroPayload = std::array<std::uint32_t, 2>{MakeHeader(3u, 0u, OpcodeSetShReg), 0x200u};
    ExpectFailure([&] {
        AgcDriver::Pm4::Validate(invalidZeroPayload, 0u);
        AgcDriver::Pm4::Execute(invalidZeroPayload, state);
    });
    Require(state.shader == shaderSnapshot);
    Require(state.context == contextSnapshot);
}

}

int main() {
    TestReservedHeaderType0();
    TestReservedHeaderType1();
    TestReservedHeaderType2();
    TestInvalidHeaderTypes4To7();
    TestZeroCountPayloads();
    TestPayloadCountMismatchBounds();
    TestExecutionStatePreservation();
    return 0;
}
