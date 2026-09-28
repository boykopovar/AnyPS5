#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

template<typename TAction>
void expectFailure(TAction action, const char* text) {
    try { action(); }
    catch (const std::runtime_error& error) {
        check(std::string(error.what()).find(text) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error("expected PM4 rejection");
}

std::vector<std::uint32_t> makePacket(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload, std::uint32_t flags = 0) {
    std::vector<std::uint32_t> result{0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u) | flags};
    result.insert(result.end(), payload);
    return result;
}

std::uint32_t low(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t high(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); }

void execute(AgcDriver::QueueState& state, const std::vector<std::uint32_t>& packet) {
    AgcDriver::Pm4::Validate(packet, 0);
    AgcDriver::Pm4::Execute(packet, state);
}

void testCatalog() {
    std::set<std::uint32_t> values;
    for (const auto& opcode : AgcDriver::Pm4::Opcodes) {
        check(values.insert(opcode.value).second, "duplicate PM4 opcode");
        const auto packet = makePacket(opcode.value, {0});
        check(AgcDriver::Pm4::Name(packet[0]) == opcode.name, "opcode name mismatch");
        const auto reason = AgcDriver::Pm4::UnsupportedReason(packet[0]);
        if (!reason.empty()) expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0); }, std::string(reason).c_str());
    }
    check(values.size() == 54, "reference opcode catalog is incomplete");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0xff, {0}), 0); }, "not known");
    const std::array<std::pair<std::uint32_t, const char*>, 11> custom{{
        {5, "DRAW_RESET"}, {6, "WAIT_FLIP_DONE"}, {9, "DISPATCH_RESET"}, {11, "PUSH_MARKER"},
        {12, "POP_MARKER"}, {20, "ACQUIRE_MEM_CUSTOM"}, {21, "WRITE_DATA_CUSTOM"}, {23, "FLIP"},
        {24, "RELEASE_MEM_CUSTOM"}, {25, "DMA_DATA_CUSTOM"}, {26, "CONTEXT_STATE"}
    }};
    for (const auto& [id, name] : custom) check(AgcDriver::Pm4::Name(makePacket(0x10, {0}, id << 2)[0]) == name, "custom opcode name mismatch");
}

void testRegisters() {
    AgcDriver::QueueState state;
    execute(state, makePacket(0x79, {0x242, 4}));
    check(state.userConfig.at(0x242) == 4, "primitive type register write was lost");
    execute(state, makePacket(0x79, {0x242, 6}));
    check(state.userConfig.at(0x242) == 6, "primitive type register update was lost");
    const std::array<std::uint32_t, 3> indirectOpcodes{0x9f, 0x63, 0x64};
    for (auto opcode : indirectOpcodes) {
        std::array<std::uint32_t, 6> pairs{0x10, 41, 0x11, 42, 0x10, 43};
        auto packet = makePacket(opcode, {low(pairs.data()), high(pairs.data()), 0x80000000, 3});
        execute(state, packet);
        const auto& registers = opcode == 0x9f ? state.context : opcode == 0x63 ? state.shader : state.userConfig;
        check(registers.at(0x10) == 43 && registers.at(0x11) == 42, "indirect register order or bank lost");
        pairs[0] = 0x12;
        pairs[4] = 0xffffffffu;
        expectFailure([&] { execute(state, packet); }, "sentinel");
        check(!registers.contains(0x12), "invalid indirect packet partially changed state");
        packet[1] = 0x1000;
        packet[2] = 0;
        expectFailure([&] { execute(state, packet); }, "guest");
        packet[3] = 0;
        expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0); }, "control");
    }
    execute(state, makePacket(0x69, {0x11, 50, 51}));
    check(state.context.at(0x11) == 50 && state.context.at(0x12) == 51, "direct registers not sequential");
    execute(state, makePacket(0x7a, {0x10, 60}));
    check(state.userConfig.at(0x10) == 60, "uconfig index zero failed");
    execute(state, makePacket(0x7a, {0x20000243, 0x441}));
    check(state.indexType == 1 && state.userConfig.at(0x243) == 0x441, "indexed VGT_INDEX_TYPE write lost state");
    expectFailure([&] { execute(state, makePacket(0x7a, {0x10000010, 1})); }, "bank selection");
    expectFailure([&] { execute(state, makePacket(0x69, {0xffff, 1, 2})); }, "overflow");
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x9f, {0, 0, 0x80000000, 0}), 0x20); }, "compute");
}

void testContextAndBases() {
    AgcDriver::QueueState state;
    execute(state, makePacket(0x69, {0x10, 17}));
    execute(state, makePacket(0x76, {0x20c, 2}));
    execute(state, makePacket(0x10, {3, 0}, 0x68));
    check(state.context == AgcDriver::InitialContextRegisters() && state.shader.at(0x20c) == 2, "push-clear reset wrong state");
    expectFailure([&] { execute(state, makePacket(0x10, {1, 0}, 0x68)); }, "already pushed");
    execute(state, makePacket(0x69, {0x10, 19}));
    execute(state, makePacket(0x10, {2, 0}, 0x68));
    check(state.context.at(0x10) == 17, "pop did not restore context");
    expectFailure([&] { execute(state, makePacket(0x10, {2, 0}, 0x68)); }, "not been pushed");
    alignas(8) std::array<std::uint32_t, 4> arguments{7, 8, 9, 0};
    execute(state, makePacket(0x11, {1, low(arguments.data()), high(arguments.data())}, 2));
    auto packet = makePacket(0x16, {0, 0x8041});
    AgcDriver::Pm4::Validate(packet, 0);
    auto resolved = AgcDriver::Pm4::ResolveDispatch(packet, state);
    check(resolved == std::array<std::uint32_t, 5>{0xc0031500, 7, 8, 9, 0x8041}, "base-relative dispatch arguments changed");
    packet = makePacket(0x16, {low(arguments.data()), high(arguments.data()), 0x41});
    AgcDriver::Pm4::Validate(packet, 0x20);
    check(AgcDriver::Pm4::ResolveDispatch(packet, state)[3] == 9, "absolute indirect dispatch arguments changed");
    execute(state, makePacket(0x13, {32}));
    execute(state, makePacket(0x26, {0x1000, 1}));
    execute(state, makePacket(0x2a, {1}));
    execute(state, makePacket(0x2f, {3}));
    check(state.indexBufferSize == 32 && state.indexBase == 0x100001000ull && state.indexType == 1 && state.instanceCount == 3, "draw setup state lost");
    execute(state, makePacket(0x10, {0x00636261}, 0x2c));
    check(state.markers.back() == "abc", "marker text lost");
    execute(state, makePacket(0x10, {0}, 0x30));
    expectFailure([&] { execute(state, makePacket(0x10, {0}, 0x30)); }, "underflow");
    execute(state, makePacket(0x10, {0}, 0x24));
    check(state.shader.empty() && state.context == AgcDriver::InitialContextRegisters() && state.dispatchIndirectBase == 0 && state.indexBase == 0 && !state.savedContext, "dispatch reset retained state");
}

void testAutoDraw() {
    check(AgcDriver::Pm4::AccessesMemory(0xc0012d00u), "auto draw must synchronize guest memory");
    AgcDriver::QueueState state;
    state.instanceCount = 4;
    state.indexBase = 1;
    state.indexType = 0xffffffffu;
    state.userConfig[0x24a] = 7;
    for (const auto flags : {2u, 0x22u}) {
        const auto draw = AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {3, flags}), state);
        check(!draw.indexed && draw.indexAddress == 0 && draw.indexSize == 0, "auto draw used the index buffer");
        check(draw.indexCount == 3 && draw.instanceCount == 4 && draw.firstVertex == 7 && draw.firstInstance == 0 && draw.flags == (flags & 0x20u), "auto draw parameters mismatch");
    }
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}), 0x20); }, "compute queue");
    for (const auto flags : {0u, 1u, 3u, 0x20u, 0x42u}) {
        expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, flags}), 0); }, "auto draw flags");
    }
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}, 1), 0); }, "header flags");
    state.userConfig[0x24a] = std::numeric_limits<std::uint32_t>::max();
    check(AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {1, 2}), state).firstVertex == std::numeric_limits<std::uint32_t>::max(), "last vertex rejected");
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {2, 2}), state); }, "vertex range overflow");
    check(AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {0, 2}), state).indexCount == 0, "empty auto draw rejected");
    state.userConfig.erase(0x24a);
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {1, 2}), state); }, "GE_INDX_OFFSET");
}

void testIndexedDraw() {
    AgcDriver::QueueState state;
    alignas(4) std::array<std::uint32_t, 8> indices{};
    state.indexBase = reinterpret_cast<std::uintptr_t>(indices.data());
    state.instanceCount = 3;
    const auto packet = makePacket(0x35, {4, 2, 4, 0x20});
    for (std::uint32_t type = 0; type < 3; ++type) {
        state.indexType = type;
        const auto draw = AgcDriver::Pm4::ResolveDraw(packet, state);
        const auto size = type == 0 ? 2u : type == 1 ? 4u : 1u;
        check(draw.indexAddress == state.indexBase + 2 * size && draw.indexSize == size && draw.indexCount == 4 && draw.instanceCount == 3 && draw.flags == 0x20, "indexed draw state mismatch");
    }
    expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0x20); }, "compute queue");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x35, {3, 0, 4, 0}), 0); }, "maximum index size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x35, {4, 0, 4, 1}), 0); }, "draw flags");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x35, {4, 0, 4}), 0); }, "packet size");
    state.indexType = 3;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "index type");
    state.indexType = 1;
    state.indexBase += 1;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "misaligned index base");
    state.indexBase = std::numeric_limits<std::uint64_t>::max() - 3;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "index address overflow");
    state.indexType = 2;
    state.indexBase = std::numeric_limits<std::uint64_t>::max() - 4;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "address range overflow");
    state.indexBase = 0x1000;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "guest");
}

void testIndirectDraw() {
    check(AgcDriver::Pm4::AccessesMemory(0xc0032400u) && AgcDriver::Pm4::AccessesMemory(0xc0083800u), "indirect draws must synchronize guest memory");
    check(AgcDriver::Pm4::UnsupportedReason(0xc0032400u).empty() && AgcDriver::Pm4::UnsupportedReason(0xc0082c00u).empty(), "indirect draws are rejected");
    AgcDriver::QueueState state;
    state.userConfig[0x24a] = 5;
    const auto packet = makePacket(0x24, {0, 0x280, 0x8e, 2});
    AgcDriver::Pm4::Validate(packet, 0);
    expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0x20); }, "compute queue");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x24, {2, 0x280, 0x8e, 2}), 0); }, "misaligned indirect draw offset");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x10280, 0x8e, 2}), 0); }, "start-index location");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x200, 0x8e, 2}), 0); }, "register location");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x280, 0x8e, 0}), 0); }, "initiator");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x25, {0, 0x280, 0x8e, 2}), 0); }, "initiator");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x280, 0x8e}), 0); }, "packet size");
    AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x8c, 0x280, 0x8d | (1u << 31u), 3, 0, 0, 16, 2}), 0);
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280 | (1u << 27u), 3, 0, 0, 16, 2}), 0); }, "control bits");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280, 3, 0, 0, 12, 2}), 0); }, "stride");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x38, {0, 0x280, 0x8e, 0x280, 3, 0, 0, 16, 0}), 0); }, "stride");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280 | (1u << 30u), 3, 0, 0, 16, 2}), 0); }, "count address");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280, 3, 0x1000, 0, 16, 2}), 0); }, "count address");
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "base has not been set");
    execute(state, makePacket(0x11, {1, 0x5d87fc40, 0x10}));
    auto draw = AgcDriver::Pm4::ResolveDraw(packet, state);
    check(draw.indirect.has_value() && draw.indirect->arguments == 0x105d87fc40ull && draw.indirect->opcode == 0x24 && draw.indirect->recordBytes == 16 && draw.indirect->stride == 16 && draw.indirect->count == 1 && !draw.indirect->countIndirect, "indirect draw arguments mismatch");
    check(draw.indirect->baseVertexLocation == 0x280 && draw.indirect->startInstanceLocation == 0x8e && draw.indirect->drawIndexLocation == 0x280 && !draw.indirect->drawIndexEnabled && draw.indirect->indxOffset == 5 && draw.firstVertex == 5 && !draw.indexed && draw.indexCount == 0 && draw.instanceCount == 0, "indirect draw locations mismatch");
    check(draw.indirect->RangeBytes() == 16 && draw.indirect->VertexDwordOffset() == 8 && draw.indirect->InstanceDwordOffset() == 12, "indirect draw record geometry mismatch");
    check(AgcDriver::Pm4::ResolveDraw(makePacket(0x24, {0x60, 0x280, 0x8e, 2}), state).indirect->arguments == 0x105d87fca0ull, "indirect draw offset not applied");
    const auto multi = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0x20, 0x8c, 0x280, 0x8d | (1u << 31u), 3, 0, 0, 32, 0x22}), state);
    check(multi.indirect->count == 3 && multi.indirect->stride == 32 && multi.indirect->drawIndexEnabled && multi.indirect->drawIndexLocation == 0x8d && !multi.indirect->countIndirect && multi.flags == 0x20 && multi.indirect->RangeBytes() == 80, "indirect multi-draw mismatch");
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state); }, "index base");
    alignas(4) std::array<std::uint16_t, 8> indices{};
    state.indexBase = reinterpret_cast<std::uintptr_t>(indices.data());
    state.indexType = 0;
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state); }, "INDEX_BUFFER_SIZE");
    state.indexBufferSize = 8;
    const auto indexed = AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state);
    check(indexed.indexed && indexed.indexAddress == state.indexBase && indexed.indexCount == 8 && indexed.indexSize == 2 && indexed.indirect->recordBytes == 20 && indexed.indirect->stride == 20 && indexed.firstVertex == 0 && indexed.indirect->indxOffset == 0, "indexed indirect draw mismatch");
    check(indexed.indirect->VertexDwordOffset() == 12 && indexed.indirect->InstanceDwordOffset() == 16, "indexed record geometry mismatch");
    alignas(16) std::array<std::uint32_t, 10> records{3, 2, 7, 9, 4, 1, 5, 6, 0, 0};
    state.drawIndirectBase = reinterpret_cast<std::uintptr_t>(records.data());
    const auto local = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0, 0x280, 0x280, 0x280, 2, 0, 0, 16, 2}), state);
    const auto first = AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 0);
    const auto second = AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 1);
    check(first.count == 3 && first.instances == 2 && first.firstVertexOrIndex == 7 && first.vertexOffset == 0 && first.firstInstance == 9, "non-indexed record layout mismatch");
    check(second.count == 4 && second.instances == 1 && second.firstVertexOrIndex == 5 && second.firstInstance == 6, "second record mismatch");
    expectFailure([&] { AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 2); }, "record index");
    records = {3, 2, 7, 9, 11, 0, 0, 0, 0, 0};
    const auto indexedRecord = AgcDriver::Pm4::ReadDrawArguments(*AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state).indirect, 0);
    check(indexedRecord.count == 3 && indexedRecord.instances == 2 && indexedRecord.firstVertexOrIndex == 7 && indexedRecord.vertexOffset == 9 && indexedRecord.firstInstance == 11, "indexed record layout mismatch");
    alignas(4) std::uint32_t countValue = 2;
    const auto counted = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0, 0x280, 0x280, 0x280 | (1u << 30u), 5, low(&countValue), high(&countValue), 16, 2}), state);
    check(counted.indirect->countIndirect && counted.indirect->count == 5 && counted.indirect->countAddress == reinterpret_cast<std::uintptr_t>(&countValue) && AgcDriver::Pm4::ReadDrawCount(*counted.indirect) == 2, "indirect draw count mismatch");
    expectFailure([&] { AgcDriver::Pm4::ReadDrawCount(*local.indirect); }, "count address");
}

void testMemory() {
    AgcDriver::QueueState state;
    std::array<std::uint32_t, 4> data{0, 0, 0, 0};
    execute(state, makePacket(0x37, {0x100, low(data.data()), high(data.data()), 11, 12}));
    check(data[0] == 11 && data[1] == 12, "WRITE_DATA increment failed");
    execute(state, makePacket(0x37, {0x10100, low(data.data()), high(data.data()), 21, 22}));
    check(data[0] == 22 && data[1] == 12, "WRITE_DATA fixed destination failed");
    execute(state, makePacket(0x81, {4, 31, 32}));
    execute(state, makePacket(0x83, {4, 2, low(data.data()), high(data.data())}));
    check(data[0] == 31 && data[1] == 32, "constant RAM round trip failed");
    expectFailure([&] { execute(state, makePacket(0x81, {0xbffc, 1, 2})); }, "overflow");
    expectFailure([&] { execute(state, makePacket(0x40, {0x10105, 0, 0, low(data.data()), high(data.data())})); }, "64-bit immediate");
}

void testCopies() {
    AgcDriver::QueueState state;
    alignas(8) std::array<std::uint32_t, 4> source{11, 12, 13, 14};
    alignas(8) std::array<std::uint32_t, 4> destination{};
    execute(state, makePacket(0x40, {0x10101, low(source.data()), high(source.data()), low(destination.data()), high(destination.data())}));
    check(destination[0] == 11 && destination[1] == 12 && destination[2] == 0, "64-bit COPY_DATA failed");
    execute(state, makePacket(0x40, {0x105, 0x12345678, 0, low(destination.data()), high(destination.data())}));
    check(destination[0] == 0x12345678, "immediate COPY_DATA failed");
    execute(state, makePacket(0x50, {0x60000000, low(source.data()), high(source.data()), low(destination.data()), high(destination.data()), 16}));
    check(source == destination, "DMA_DATA copy failed");
    execute(state, makePacket(0x50, {0x40000000, 0x44332211, 0, low(destination.data()), high(destination.data()), 6}));
    check(destination[0] == 0x44332211 && destination[1] == 0x00002211, "DMA_DATA byte fill failed");
    constexpr std::uint32_t cachePolicies = (1u << 13u) | (2u << 25u);
    const auto toGds = makePacket(0x50, {0x60100000 | cachePolicies, low(source.data()), high(source.data()), 0x100, 0, 16});
    check(!AgcDriver::Pm4::ResolveStore(toGds, state, 64).has_value(), "DMA_DATA to GDS resolved as a memory store");
    execute(state, toGds);
    execute(state, makePacket(0x50, {0x20100000, 0x104, 0, 0xfff8, 0, 8}));
    destination = {};
    execute(state, makePacket(0x50, {0x20000000 | cachePolicies, 0xfff8, 0, low(destination.data()), high(destination.data()), 8}));
    check(destination[0] == 12 && destination[1] == 13 && destination[2] == 0, "DMA_DATA GDS to GDS round trip failed");
    const auto fromGds = makePacket(0x50, {0x20000000, 0x100, 0, low(destination.data()), high(destination.data()), 16});
    const auto store = AgcDriver::Pm4::ResolveStore(fromGds, state, 64);
    check(store.has_value() && store->Bytes().size() == 16 && std::memcmp(store->Bytes().data(), source.data(), 16) == 0, "DMA_DATA from GDS did not resolve its source bytes");
    destination = {};
    execute(state, fromGds);
    check(source == destination, "DMA_DATA GDS round trip failed");
    expectFailure([&] { execute(state, makePacket(0x50, {0x60100000, low(source.data()), high(source.data()), 0xfffc, 0, 8})); }, "exceeds the GDS");
    expectFailure([&] { execute(state, makePacket(0x50, {0x20000000, 0, 1, low(destination.data()), high(destination.data()), 4})); }, "exceeds the GDS");
    expectFailure([&] { execute(state, makePacket(0x50, {0x60200000, low(source.data()), high(source.data()), 0, 0, 4})); }, "destination is not implemented");
    expectFailure([&] { execute(state, makePacket(0x50, {0x60000000 | (1u << 15u), low(source.data()), high(source.data()), low(destination.data()), high(destination.data()), 4})); }, "reserved fields");
    expectFailure([&] { execute(state, makePacket(0x37, {0x100, 0x1000, 0, 1})); }, "guest");
#ifdef _WIN32
    auto* memory = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    check(memory != nullptr, "VirtualAlloc failed");
    DWORD previous = 0;
    check(VirtualProtect(memory, 4096, PAGE_READONLY, &previous) != 0, "VirtualProtect failed");
    try {
        expectFailure([&] { execute(state, makePacket(0x37, {0x100, low(memory), high(memory), 1})); }, "write permission");
    } catch (...) { VirtualFree(memory, 0, MEM_RELEASE); throw; }
    check(VirtualFree(memory, 0, MEM_RELEASE) != 0, "VirtualFree failed");
#endif
}

void testMemorySynchronization() {
    struct MemoryState {
        std::uint32_t source = 0;
        std::uint32_t destination = 0;
        bool read = false;
        bool written = false;
    } memory;
    AgcDriver::QueueState state;
    const auto resolve = [](void* context, std::uint64_t address, std::size_t bytes, bool writable) {
        auto& memory = *static_cast<MemoryState*>(context);
        check(bytes == sizeof(std::uint32_t), "memory transfer resolved an unrelated range");
        if (writable) {
            check(address == reinterpret_cast<std::uintptr_t>(&memory.destination), "memory transfer resolved an unrelated destination");
            memory.written = true;
        } else {
            check(address == reinterpret_cast<std::uintptr_t>(&memory.source), "memory transfer resolved an unrelated source");
            memory.source = 42;
            memory.read = true;
        }
    };
    const AgcDriver::GuestMemory::MemoryAccessScope scope(&memory, resolve);
    execute(state, makePacket(0x37, {0x100, low(&memory.destination), high(&memory.destination), 17}));
    check(memory.written && !memory.read && memory.destination == 17, "WRITE_DATA did not synchronize its destination");
    for (const auto opcode : {0x40u, 0x50u}) {
        memory = {};
        const auto packet = opcode == 0x40
            ? makePacket(opcode, {0x101, low(&memory.source), high(&memory.source), low(&memory.destination), high(&memory.destination)})
            : makePacket(opcode, {0x60000000, low(&memory.source), high(&memory.source), low(&memory.destination), high(&memory.destination), 4});
        execute(state, packet);
        check(memory.read && memory.written && memory.destination == 42, "memory copy used stale data before range synchronization");
    }
    const AgcDriver::GuestMemory::MemoryAccessScope rejecting(&memory, [](void*, std::uint64_t, std::size_t, bool) {
        throw std::runtime_error("range synchronization failed");
    });
    expectFailure([&] { execute(state, makePacket(0x37, {0x100, low(&memory.destination), high(&memory.destination), 99})); }, "range synchronization failed");
    check(memory.destination == 42, "failed synchronization changed the destination");
}

void testEventWrite() {
    for (const auto eventType : {0x07u, 0x0fu, 0x10u}) {
        AgcDriver::Pm4::Validate(makePacket(0x46, {0x400u | eventType}), 0);
        for (std::uint32_t index = 0; index < 8; ++index) {
            if (index == 4) continue;
            expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0); }, "partial-flush event index");
        }
        if (eventType == 0x07) {
            AgcDriver::Pm4::Validate(makePacket(0x46, {0x407}), 0x20);
        } else {
            expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x400u | eventType}), 0x20); }, "compute queue");
        }
    }
    for (const auto eventType : {0x16u, 0x31u, 0x2au, 0x2cu, 0x2eu}) {
        for (const auto index : {0u, 7u}) {
            AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0);
        }
        for (std::uint32_t index = 1; index < 7; ++index) {
            expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0); }, "cache-flush event index");
        }
        expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {eventType}), 0x20); }, "compute queue");
    }
    for (const auto bit : {0x40u, 0x80u, 0x800u, 0x80000000u}) {
        expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410u | bit}), 0); }, "reserved bits");
    }
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410}, 1), 0); }, "header flags");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0, 0}), 0); }, "event type 57");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x0d}), 0); }, "event type 13");
}

void testAcquireMem() {
    const auto captured = makePacket(0x58, {0x02007fc0, 0, 0, 0, 0, 10, 0x200});
    AgcDriver::Pm4::Validate(captured, 0);
    check(AgcDriver::Pm4::UsesGpuCacheBarrier(captured), "L1 acquire must preserve GPU render targets");
    for (const auto flags : {0u, 0x200u, 0x3ffu, 0x10200u, 0x20200u}) {
        auto packet = captured;
        packet[7] = flags;
        check(AgcDriver::Pm4::UsesGpuCacheBarrier(packet), "GPU cache acquire requires an unnecessary host writeback");
    }
    for (const auto flags : {0x400u, 0x800u, 0x1000u, 0x4000u, 0x8000u}) {
        auto packet = captured;
        packet[7] = flags;
        check(!AgcDriver::Pm4::UsesGpuCacheBarrier(packet), "L2 acquire lost host synchronization");
    }
    check(!AgcDriver::Pm4::UsesGpuCacheBarrier(makePacket(0x58, {0x00800000, 0xffffffff, 0, 0, 0, 10})), "legacy acquire lost host synchronization");
    expectFailure([] { AgcDriver::Pm4::UsesGpuCacheBarrier({}); }, "requires ACQUIRE_MEM");
    expectFailure([] { AgcDriver::Pm4::UsesGpuCacheBarrier(makePacket(0x58, {0, 0, 0, 0, 0, 0, 0x2000})); }, "cache discard");
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x82007fc0, 1, 0, 0xffffffff, 0, 0xffff, 0x200}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x80000000, 0, 0, 0, 0, 10, 0x200}), 0x20);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x00800000, 0xffffffff, 0, 0, 0, 10}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x80800000, 16, 0, 0x1000, 0, 0}), 0x20);
    expectFailure([&] { AgcDriver::Pm4::Validate(captured, 0x20); }, "compute queue");
    const auto invalidWord = [&](std::size_t index, std::uint32_t value, const char* reason) {
        auto packet = captured;
        packet[index] = value;
        expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0); }, reason);
    };
    invalidWord(0, captured[0] | 1u, "header flags");
    invalidWord(1, 4, "control flags");
    invalidWord(1, 0x00800000, "control flags");
    invalidWord(3, 1, "above 40 bits");
    invalidWord(5, 1, "above 40 bits");
    invalidWord(6, 0x10000, "poll interval");
    invalidWord(7, 0x40000, "GCR flags");
    invalidWord(7, 0x2000, "cache discard");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 2, 0, 0xffffffff, 0, 0, 0}), 0); }, "range exceeds");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0, 0, 0, 0}), 0); }, "packet size");
}

void testDriverSubmission() {
    std::array<std::uint32_t, 2> source{0x10, 73};
    std::array<std::uint32_t, 1> destination{};
    std::vector<std::uint32_t> commands;
    for (const auto& packet : {
        makePacket(0x9f, {low(source.data()), high(source.data()), 0x80000000, 1}),
        makePacket(0x81, {0, 83}),
        makePacket(0x42, {0}),
        makePacket(0x46, {0x410}),
        makePacket(0x46, {0x407}),
        makePacket(0x46, {0x40f}),
        makePacket(0x46, {0x16}),
        makePacket(0x46, {0x731}),
        makePacket(0x46, {0x2a}),
        makePacket(0x46, {0x72c}),
        makePacket(0x46, {0x2e}),
        makePacket(0x58, {0x02007fc0, 0, 0, 0, 0, 10, 0x200}),
        makePacket(0x58, {0x00800000, 0xffffffff, 0, 0, 0, 10}),
        makePacket(0x83, {0, 1, low(destination.data()), high(destination.data())})
    }) commands.insert(commands.end(), packet.begin(), packet.end());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "PM4 submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 83, "worker did not execute PM4 memory operations");
    auto rejectedCommands = commands;
    const auto unsupportedEvent = makePacket(0x46, {0x139, 0, 0});
    rejectedCommands.insert(rejectedCommands.end(), unsupportedEvent.begin(), unsupportedEvent.end());
    Packet rejectedPacket{rejectedCommands.data(), static_cast<std::uint32_t>(rejectedCommands.size()), 0, {}};
    destination[0] = 0;
    expectFailure([&] { sceAgcDriverSubmitDcb(&rejectedPacket); }, "EVENT_WRITE at DWORD");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 0, "rejected event submission executed a prefix");
    destination[0] = 0;
    const auto emptyDraw = makePacket(0x2d, {0, 2});
    commands.insert(commands.end(), emptyDraw.begin(), emptyDraw.end());
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "auto draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 83, "empty auto draw prevented command execution");
    destination[0] = 0;
    const auto draw = makePacket(0x2d, {3, 3});
    commands.insert(commands.end(), draw.begin(), draw.end());
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); }, "DRAW_INDEX_AUTO at DWORD");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 0, "rejected submission executed a prefix");
}

void testAsyncMemoryFailure() {
    auto commands = makePacket(0x37, {0x100, 0x1000, 0, 1});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "memory packet was not submitted");
    expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "guest");
    expectFailure([] { AgcDriverSuspendPoint_nid_postfix(); }, "guest");
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); }, "guest");
    expectFailure([] { LibcRunShutdown_nid_postfix(); }, "guest");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "failure") {
            testAsyncMemoryFailure();
            std::puts("PM4 asynchronous memory failure propagated to idle, suspend, submit and shutdown");
            return 0;
        }
        testCatalog();
        testRegisters();
        testContextAndBases();
        testIndexedDraw();
        testAutoDraw();
        testIndirectDraw();
        testMemory();
        testCopies();
        testMemorySynchronization();
        testEventWrite();
        testAcquireMem();
        testDriverSubmission();
        LibcRunShutdown_nid_postfix();
        std::puts("PM4 catalog, registers, state, memory and submission tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
