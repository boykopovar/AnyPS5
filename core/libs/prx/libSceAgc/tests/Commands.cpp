#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include <string>

extern "C" std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbClearState(CommandBuffer* buf, std::uint32_t command);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer* buf, std::uint32_t handle, std::int32_t index, std::uint32_t mode, std::int64_t argument);
extern "C" int APS5_VABI sceAgcSuspendPoint();
extern "C" int APS5_VABI sceAgcInit(std::uint32_t version);
extern "C" void* APS5_VABI sceAgcGetRegisterDefaults();
extern "C" void* APS5_VABI sceAgcGetRegisterDefaultsInternal();
extern "C" void* APS5_VABI sceAgcGetRegisterDefaults2Internal(std::uint32_t version);
extern "C" int APS5_VABI sceAgcInit_0090(std::uint32_t* state, std::uint32_t version);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, std::uint32_t indexCount, std::uint64_t modifier);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, std::uint32_t dataOffsetInBytes, std::uint64_t modifier);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, std::uint32_t dataOffsetInBytes, std::uint32_t countIndirect, std::uint32_t maxCountOrCount, const volatile void* countAddress, std::uint32_t strideInBytes, std::uint64_t modifier);
extern "C" int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint64_t reference);
extern "C" int APS5_VABI sceAgcWaitRegMemPatchMask(std::uint32_t* cmd, std::uint64_t mask);
extern "C" int APS5_VABI sceAgcGetDataPacketPayloadAddress_0090(std::uint32_t** addr, std::uint32_t* cmd, int type);
extern "C" std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbContextStateOp_0100(CommandBuffer* buf, std::uint32_t operation);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, std::uint32_t color);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbPopMarker(CommandBuffer* buf);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, std::uint32_t color);
extern "C" std::uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, std::uint32_t color);
extern "C" std::uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf);
extern "C" std::uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, std::uint32_t color);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress);
extern "C" std::uint32_t APS5_VABI sceAgcDcbBeginOcclusionQueryGetSize(std::uint8_t type);
extern "C" std::uint32_t APS5_VABI sceAgcDcbEndOcclusionQueryGetSize();

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

struct Storage {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

bool APS5_VABI grow(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& storage = *static_cast<Storage*>(userData);
    check(count == 5, "incorrect callback allocation including reserved words");
    buffer->bottom = storage.words.data();
    buffer->top = storage.words.data() + storage.words.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->top;
    return true;
}

bool APS5_VABI growReserved(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& storage = *static_cast<Storage*>(userData);
    check(count == 5 + 4, "incorrect callback allocation including reserved words");
    buffer->bottom = storage.words.data();
    buffer->top = storage.words.data() + storage.words.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->top;
    return true;
}

void testPackets() {
    Storage storage;
    sceAgcDcbResetQueue(&storage.buffer, 0, 3);
    sceAgcDcbDrawIndexAuto(&storage.buffer, 17, 0);
    const std::array<std::uint32_t, 5> expected{0xc0001200u, 3, 0xc0012d00u, 17, 2};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "reset or draw packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + expected.size(), "incorrect packet cursor advance");
    const auto before = storage.words;
    expectFailure([&] { sceAgcDcbResetQueue(&storage.buffer, 0, 16); });
    check(storage.words == before, "invalid reset modified packet memory");
    Storage destination;
    CommandBuffer empty{nullptr, nullptr, nullptr, nullptr, grow, &destination, 0};
    Agc::Command::Emit(&empty, 0x15u, {1, 1, 1, 0x41u}, __func__);
    check(empty.cursor_up == destination.words.data() + 5, "guest ABI allocation callback failed");
    Storage reservedDestination;
    CommandBuffer reserved{nullptr, nullptr, nullptr, nullptr, growReserved, &reservedDestination, 4};
    Agc::Command::Emit(&reserved, 0x15u, {1, 1, 1, 0x41u}, __func__);
    check(reserved.cursor_up == reservedDestination.words.data() + 5, "buffer with less room than its reserved words did not grow");
    Storage exhausted;
    exhausted.buffer.cursor_down = exhausted.words.data() + 2;
    expectFailure([&] { Agc::Command::WriteNop(&exhausted.buffer, 3, __func__); });
    check(exhausted.buffer.cursor_up == exhausted.words.data(), "failed allocation advanced cursor");
    std::array<std::uint32_t, 8> scratch{};
    CommandBuffer noDown{scratch.data(), scratch.data() + scratch.size(), scratch.data(), nullptr, nullptr, nullptr, 0};
    Agc::Command::WriteNop(&noDown, 6, __func__);
    check(noDown.cursor_up == scratch.data() + 6, "buffer without a down cursor did not use its top as the limit");
    expectFailure([&] { Agc::Command::WriteNop(&noDown, 3, __func__); });
    check(noDown.cursor_up == scratch.data() + 6, "allocation past the top of a buffer without a down cursor advanced its cursor");
}

void testClearState() {
    Storage storage;
    storage.words.fill(0xdeadbeefu);
    for (std::uint32_t command = 0; command <= 0xfu; ++command) {
        auto* packet = sceAgcDcbClearState(&storage.buffer, command);
        check(packet == storage.words.data() + command * 2 && packet[0] == 0xc0001200u && packet[1] == command, "incorrect CLEAR_STATE packet");
    }
    check(storage.buffer.cursor_up == storage.words.data() + 32 && storage.words[32] == 0xdeadbeefu, "incorrect CLEAR_STATE cursor advance");
    const auto before = storage.words;
    expectFailure([&] { sceAgcDcbClearState(&storage.buffer, 0x10u); });
    expectFailure([&] { sceAgcDcbClearState(&storage.buffer, 0xffffffffu); });
    expectFailure([] { sceAgcDcbClearState(nullptr, 0); });
    check(storage.words == before && storage.buffer.cursor_up == storage.words.data() + 32, "invalid CLEAR_STATE modified the buffer");
}

struct ContextGrowth {
    Storage destination;
    std::uint32_t* expectedCursor;
    std::uint32_t expectedCount;
    std::uint32_t calls = 0;
    bool success = true;
};

bool APS5_VABI growContext(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& growth = *static_cast<ContextGrowth*>(userData);
    check(++growth.calls == 1, "unexpected repeated context allocation callback");
    check(buffer->cursor_up == growth.expectedCursor, "context callback at wrong packet boundary");
    check(count == growth.expectedCount, "incorrect context reservation size");
    if (!growth.success) {
        return false;
    }
    buffer->bottom = growth.destination.buffer.bottom;
    buffer->top = growth.destination.buffer.top;
    buffer->cursor_up = growth.destination.buffer.cursor_up;
    buffer->cursor_down = growth.destination.buffer.cursor_down;
    return true;
}

void testIndexedIndirectDraws() {
    Storage storage;
    const auto* single = sceAgcDcbDrawIndexIndirect(&storage.buffer, 0x40, 0);
    const std::array<std::uint32_t, 5> expectedSingle{0xc0032500u, 0x40, 0x280, 0x280, 0};
    check(single == storage.words.data() && std::equal(expectedSingle.begin(), expectedSingle.end(), single), "indexed indirect draw packet mismatch");
    alignas(4) std::uint32_t count = 0;
    const auto* multi = sceAgcDcbDrawIndexIndirectMulti(&storage.buffer, 0x80, 1, 8, &count, 20, 0);
    const auto address = reinterpret_cast<std::uintptr_t>(&count);
    const std::array<std::uint32_t, 10> expectedMulti{0xc0083800u, 0x80, 0x280, 0x280, 0x40000280u, 8, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 20, 0};
    check(multi == storage.words.data() + expectedSingle.size() && std::equal(expectedMulti.begin(), expectedMulti.end(), multi), "indexed indirect multi draw packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + expectedSingle.size() + expectedMulti.size(), "incorrect indexed indirect cursor advance");
    const auto before = storage.words;
    expectFailure([&] { sceAgcDcbDrawIndexIndirect(&storage.buffer, 2, 0); });
    expectFailure([&] { sceAgcDcbDrawIndexIndirectMulti(&storage.buffer, 0, 1, 8, &count, 16, 0); });
    expectFailure([&] { sceAgcDcbDrawIndexIndirectMulti(&storage.buffer, 0, 0, 8, &count, 20, 0); });
    check(storage.words == before, "invalid indexed indirect draw modified packet memory");
}

void testMarkers() {
    Storage dcb;
    Storage acb;
    const auto* dcbPush = sceAgcDcbPushMarker(&dcb.buffer, "frame", 0xff0000u);
    const auto* acbPush = sceAgcAcbPushMarker(&acb.buffer, "frame", 0x00ff00u);
    check(acbPush == acb.words.data() && acbPush[0] == Agc::Command::Header(0x10, 3, 0x0bu << 2u), "ACB push marker header mismatch");
    check(std::strcmp(reinterpret_cast<const char*>(acbPush + 1), "frame") == 0, "ACB push marker text mismatch");
    const auto* acbPop = sceAgcAcbPopMarker(&acb.buffer);
    check(acbPop == acb.words.data() + 3 && acbPop[0] == Agc::Command::Header(0x10, 2, 0x0cu << 2u) && acbPop[1] == 0, "ACB pop marker mismatch");
    sceAgcDcbPopMarker(&dcb.buffer);
    const auto* acbSet = sceAgcAcbSetMarker(&acb.buffer, nullptr, 0);
    const auto* dcbSet = sceAgcDcbSetMarker(&dcb.buffer, nullptr, 0);
    check(acbSet == acb.words.data() + 5 && dcbSet == dcb.words.data() + 5, "set marker did not return its push packet");
    check(acbSet[0] == Agc::Command::Header(0x10, 2, 0x0bu << 2u) && acbSet[1] == 0 && acbSet[2] == Agc::Command::Header(0x10, 2, 0x0cu << 2u), "ACB set marker is not a push and pop pair");
    check(dcbPush == dcb.words.data() && dcb.words == acb.words, "ACB and DCB markers differ");
    check(acb.buffer.cursor_up == acb.words.data() + 9, "incorrect ACB marker cursor advance");
    expectFailure([] { sceAgcAcbPushMarker(nullptr, "frame", 0); });
    expectFailure([] { sceAgcAcbPopMarker(nullptr); });
    expectFailure([] { sceAgcAcbSetMarker(nullptr, "frame", 0); });
}

void testOcclusionQuerySizes() {
    check(sceAgcDcbBeginOcclusionQueryGetSize(0) == 16, "occlusion query begin size mismatch");
    check(sceAgcDcbBeginOcclusionQueryGetSize(1) == 288, "type 1 occlusion query begin size mismatch");
    check(sceAgcDcbEndOcclusionQueryGetSize() == 16, "occlusion query end size mismatch");
    expectFailure([] { sceAgcDcbBeginOcclusionQueryGetSize(2); });
    expectFailure([] { sceAgcDcbBeginOcclusionQueryGetSize(0xff); });
}

void testIndexBuffer() {
    Storage storage;
    alignas(4) std::uint16_t indices[2]{};
    const auto address = reinterpret_cast<std::uintptr_t>(indices);
    const auto* bound = sceAgcDcbSetIndexBuffer(&storage.buffer, address);
    check(bound[1] == static_cast<std::uint32_t>(address) && bound[2] == static_cast<std::uint32_t>(address >> 32u), "index buffer address mismatch");
    const auto* unbound = sceAgcDcbSetIndexBuffer(&storage.buffer, 0);
    check(unbound == bound + 3 && unbound[0] == bound[0] && unbound[1] == 0 && unbound[2] == 0, "index buffer was not unbound");
    const auto before = storage.words;
    try {
        sceAgcDcbSetIndexBuffer(&storage.buffer, 0x1001);
    } catch (const std::runtime_error& error) {
        check(std::string(error.what()).find("0x1001") != std::string::npos, "misaligned index buffer error omits the address");
        check(storage.words == before, "misaligned index buffer modified packet memory");
        return;
    }
    throw std::runtime_error("misaligned index buffer was accepted");
}

void testContextState() {
    const std::array<std::array<std::uint32_t, 6>, 4> sizes{{{5}, {5, 8, 9, 3, 2}, {3, 5, 8, 9, 2}, {5, 8, 9, 3, 2, 5}}};
    const std::array<std::array<std::uint32_t, 4>, 4> reservations{{{5}, {22, 3, 2}, {3, 22, 2}, {22, 3, 2, 5}}};
    const std::array<std::uint32_t, 4> totals{5, 27, 27, 32};
    for (std::uint32_t operation = 0; operation < sizes.size(); ++operation) {
        for (std::uint32_t capacity = 0; capacity <= totals[operation]; ++capacity) {
            Storage source;
            source.words.fill(0xdeadbeefu);
            std::uint32_t split = 0;
            std::uint32_t requested = 0;
            for (const auto count : reservations[operation]) {
                if (count > capacity - split) {
                    requested = count;
                    break;
                }
                split += count;
            }
            ContextGrowth growth{{}, source.words.data() + split, requested + 2};
            growth.destination.words.fill(0xdeadbeefu);
            source.buffer.cursor_down = source.words.data() + capacity + 2;
            source.buffer.reserved_dw = 2;
            source.buffer.callback = growContext;
            source.buffer.user_data = &growth;
            auto* first = sceAgcDcbContextStateOp_0100(&source.buffer, operation);
            check(first == (split == 0 ? growth.destination.words.data() : source.words.data()), "incorrect first context packet address");
            check(growth.calls == (requested == 0 ? 0u : 1u), "incorrect context callback count");
            auto* end = requested == 0 ? source.words.data() + totals[operation] : growth.destination.words.data() + totals[operation] - split;
            check(source.buffer.cursor_up == end, "incorrect context cursor advance");
            check(*end == 0xdeadbeefu && source.words[split] == 0xdeadbeefu, "context allocation overwrote adjacent memory");
            std::uint32_t offset = 0;
            for (const auto count : sizes[operation]) {
                if (count == 0) {
                    break;
                }
                const auto* packet = offset < split ? source.words.data() + offset : growth.destination.words.data() + offset - split;
                const auto header = 0xc0001000u | ((count - 2u) << 16u) | (offset == 0 ? 0x68u : 0u);
                check(packet[0] == header, "incorrect context packet header");
                for (std::uint32_t i = 1; i < count; ++i) {
                    check(packet[i] == (offset == 0 && i == 1 ? operation : 0u), "incorrect context packet payload");
                }
                offset += count;
            }
        }
    }
    expectFailure([] { sceAgcDcbContextStateOp_0100(nullptr, 0); });
    Storage invalid;
    expectFailure([&] { sceAgcDcbContextStateOp_0100(&invalid.buffer, 4); });
    check(invalid.buffer.cursor_up == invalid.words.data(), "invalid context operation advanced cursor");
    invalid.buffer.cursor_up = invalid.words.data() + 1;
    invalid.buffer.cursor_down = invalid.words.data();
    expectFailure([&] { sceAgcDcbContextStateOp_0100(&invalid.buffer, 0); });
    invalid.buffer.cursor_up = invalid.words.data();
    expectFailure([&] { sceAgcDcbContextStateOp_0100(&invalid.buffer, 1); });
    ContextGrowth growth{{}, invalid.words.data(), 22};
    invalid.buffer.callback = growContext;
    invalid.buffer.user_data = &growth;
    growth.success = false;
    expectFailure([&] { sceAgcDcbContextStateOp_0100(&invalid.buffer, 1); });
    growth.calls = 0;
    growth.success = true;
    growth.destination.buffer.cursor_down = growth.destination.words.data() + 21;
    expectFailure([&] { sceAgcDcbContextStateOp_0100(&invalid.buffer, 1); });
    check(invalid.buffer.cursor_up == growth.destination.words.data(), "failed reservation advanced cursor");
}

void testFlip() {
    Storage storage;
    storage.words.fill(0xdeadbeefu);
    auto* packet = sceAgcDcbSetFlip(&storage.buffer, 0xfedcba98u, -2, 0x12345678u, -0x123456789abcdefLL);
    const std::array<std::uint32_t, 6> expected{0xc004105cu, 0xfedcba98u, 0xfffffffeu, 0x12345678u, 0x76543211u, 0xfedcba98u};
    check(packet == storage.words.data(), "flip returned wrong packet address");
    check(std::equal(expected.begin(), expected.end(), packet), "flip packet lost argument bits");
    check(storage.buffer.cursor_up == packet + 6 && packet[6] == 0xdeadbeefu, "flip packet overran allocation");
    expectFailure([] { sceAgcDcbSetFlip(nullptr, 1, 0, 1, 0); });
    Storage exhausted;
    exhausted.buffer.cursor_down = exhausted.words.data() + 5;
    expectFailure([&] { sceAgcDcbSetFlip(&exhausted.buffer, 1, 0, 1, 0); });
    check(exhausted.buffer.cursor_up == exhausted.words.data(), "failed flip allocation advanced cursor");
    check(sceAgcSuspendPoint() == 0, "empty suspend failed");
}

void testRegisters() {
    Storage storage;
    const std::array<ShaderRegister, 3> registers{{{0x10, 7}, {0x11, 8}, {0x20, 9}}};
    Agc::Command::WriteRegisters(&storage.buffer, 0x76u, registers.data(), registers.size(), true, __func__);
    const std::array<std::uint32_t, 7> expected{0xc0027600u, 0x10, 7, 8, 0xc0017600u, 0x20, 9};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "register run packet mismatch");
    auto* packet = Agc::Command::WriteIndirectRegisters(&storage.buffer, 0x63u, registers.data(), 0x3ffeu, __func__);
    Agc::Command::PatchIndirectCount(packet, 0x63u, 1, __func__);
    check(packet[4] == 0x3fffu, "indirect register count mismatch");
    const auto before = storage.words;
    expectFailure([&] { Agc::Command::PatchIndirectCount(packet, 0x63u, 1, __func__); });
    expectFailure([&] { Agc::Command::PatchIndirectAddress(packet, 0x64u, registers.data(), __func__); });
    check(storage.words == before, "invalid indirect patch modified memory");
}

void testRegisterRange() {
    Storage storage;
    storage.words.fill(0xdeadbeefu);
    auto* packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, nullptr, 4);
    auto expected = storage.words;
    expected.fill(0xdeadbeefu);
    expected[0] = 0xc0047600u;
    expected[1] = 0x8c;
    check(packet == storage.words.data(), "incorrect register range packet address");
    check(storage.buffer.cursor_up == storage.words.data() + 6, "incorrect register range allocation");
    check(storage.words == expected, "null register values modified payload or adjacent memory");
    const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
    packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x90, values.data(), values.size());
    expected[6] = 0xc0047600u;
    expected[7] = 0x90;
    std::copy(values.begin(), values.end(), expected.begin() + 8);
    check(packet == storage.words.data() + 6 && storage.buffer.cursor_up == storage.words.data() + 12, "incorrect populated register range allocation");
    check(storage.words == expected, "register values were not copied correctly");
    const auto* misaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const unsigned char*>(values.data()) + 1);
    expectFailure([&] { sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, misaligned, 4); });
    check(storage.words == expected && storage.buffer.cursor_up == storage.words.data() + 12, "misaligned register values modified command buffer");
}

void testPacketPayloadAddress() {
    Storage storage;
    auto* packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, nullptr, 4);
    std::uint32_t* payload = nullptr;
    check(sceAgcGetDataPacketPayloadAddress_0090(&payload, packet, 1) == 0 && payload == packet + 2, "incorrect register packet payload address");
    const std::array<std::uint32_t, 4> values{11, 22, 33, 44};
    std::copy(values.begin(), values.end(), payload);
    const std::array<std::uint32_t, 6> expected{0xc0047600u, 0x8c, 11, 22, 33, 44};
    check(std::equal(expected.begin(), expected.end(), packet), "payload write corrupted register packet");
    check(sceAgcGetDataPacketPayloadAddress_0090(&payload, packet, 0) == 0 && payload == packet + 1, "incorrect generic packet payload address");
    packet[0] = 0xffff1000u;
    check(sceAgcGetDataPacketPayloadAddress_0090(&payload, packet, 0) == 0 && payload == nullptr, "empty payload marker was not recognized");
    check(sceAgcGetDataPacketPayloadAddress_0090(&payload, packet, -1) == 0 && payload == packet + 2, "nonzero payload type did not skip two words");
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress_0090(nullptr, packet, 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress_0090(&payload, nullptr, 1); });
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(packet) + 1);
    expectFailure([&] { sceAgcGetDataPacketPayloadAddress_0090(&payload, misaligned, 0); });
    check(payload == packet + 2, "invalid packet changed output address");
}

void testMemory() {
    Storage storage;
    Agc::Command::WriteDma(&storage.buffer, false, 1, 0, 0, 0x2000, 2, 0, 0x12345678, 16, 0, 1, 1, __func__);
    const std::array<std::uint32_t, 7> expected{0xc0055000u, 0xc0000001u, 0x12345678, 0, 0x2000, 0, 0x80000010u};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "DMA packet mismatch");
    std::uint64_t value = 0;
    auto* packet = Agc::Command::WriteWait(&storage.buffer, 1, 3, 0, 0, &value, 0x1122334455667788ull, 0xffffffffffffffffull, 32, __func__);
    check(packet[0] == 0xc0027901u && packet[4] == 0xc0079300u && packet[5] == 0x13u, "wait packet header mismatch");
    check(packet[8] == 0x55667788u && packet[9] == 0x11223344u && packet[12] == 2u, "wait reference or poll interval mismatch");
    sceAgcWaitRegMemPatchReference(packet, 7);
    check(packet[8] == 7 && packet[9] == 0x11223344u, "reference patch changed the high word");
    const auto before = storage.words;
    expectFailure([&] { sceAgcWaitRegMemPatchReference(packet, 0x100000000ull); });
    check(storage.words == before, "invalid memory operation modified packet memory");
    auto* truncated = Agc::Command::WriteWait(&storage.buffer, 0, 3, 0, 0, &value, 0x100000000ull, 0xffffffff00000001ull, 32, __func__);
    check(truncated[8] == 0u && truncated[9] == 1u, "32-bit wait did not keep the low halves of the reference and mask");
    sceAgcWaitRegMemPatchMask(packet, 0x0f0f0f0fu);
    check(packet[10] == 0x0f0f0f0fu && packet[11] == 0xffffffffu && packet[8] == 7, "64-bit mask patch changed the wrong word");
    sceAgcWaitRegMemPatchMask(truncated, 0xff00u);
    check(truncated[9] == 0xff00u && truncated[8] == 0u && truncated[10] == 2u, "32-bit mask patch changed the wrong word");
    const auto beforeMask = storage.words;
    expectFailure([&] { sceAgcWaitRegMemPatchMask(packet, 0x100000000ull); });
    expectFailure([&] { sceAgcWaitRegMemPatchMask(truncated, 0x100000000ull); });
    expectFailure([&] { sceAgcWaitRegMemPatchMask(packet + 4, 1); });
    check(storage.words == beforeMask, "invalid mask patch modified packet memory");
}

void testDefaults() {
    std::uint32_t state = 0x12345678;
    check(sceAgcInit_0090(&state, 8) == 0 && state == 0x12345678, "AGC initialization failed or modified caller state");
    check(sceAgcInit_0090(&state, 13) == 0 && state == 0x12345678, "AGC version 13 initialization changed caller state");
    expectFailure([] { sceAgcInit_0090(nullptr, 8); });
    expectFailure([&] { sceAgcInit_0090(&state, 14); });
    check(sceAgcInit(8) == 0, "AGC version initialization failed");
    expectFailure([] { sceAgcInit(14); });
    expectFailure([] { sceAgcInit(0xffffffffu); });
    for (std::uint32_t version = 0; version < 14; ++version) {
        for (const bool internal : {false, true}) {
            auto* first = Agc::Command::GetRegisterDefaults(version, internal, __func__);
            check(first != nullptr && first == Agc::Command::GetRegisterDefaults(version, internal, __func__), "unstable register defaults pointer");
        }
    }
    auto* internalDefaults = sceAgcGetRegisterDefaultsInternal();
    check(internalDefaults != nullptr && internalDefaults == Agc::Command::GetRegisterDefaults(0, true, __func__) && internalDefaults == sceAgcGetRegisterDefaults2Internal(0), "internal register defaults are not the baseline internal table");
    check(internalDefaults != sceAgcGetRegisterDefaults(), "internal register defaults returned the public table");
    expectFailure([] { Agc::Command::GetRegisterDefaults(14, false, __func__); });
    expectFailure([] { Agc::Command::GetRegisterDefaults(0xffffffffu, true, __func__); });
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "defaults") == 0) {
            testDefaults();
            LibcRunShutdown_nid_postfix();
            std::puts("AGC initialization tests passed");
            return 0;
        }
        testPackets();
        testClearState();
        testIndexedIndirectDraws();
        testMarkers();
        testOcclusionQuerySizes();
        testIndexBuffer();
        testContextState();
        testFlip();
        testRegisters();
        testRegisterRange();
        testPacketPayloadAddress();
        testMemory();
        testDefaults();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC command tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
