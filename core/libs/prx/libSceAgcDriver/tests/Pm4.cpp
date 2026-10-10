#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

extern "C" int APS5_VABI sceKernelAvailableFlexibleMemorySize(size_t* size);

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
    check(values.size() == 55, "reference opcode catalog is incomplete");
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

void testRegisterFile() {
    AgcDriver::Registers registers{{0x300, 3}, {0x10, 1}, {0x41, 2}};
    check(registers.size() == 3 && !registers.contains(0x11) && registers.at(0x41) == 2, "register file lookup");
    check(registers.find(0x12) == registers.end() && registers.find(0x10)->second == 1, "register file find");
    check(!registers.emplace(0x10, 9).second && registers.at(0x10) == 1, "register file emplace replaced a value");
    check(registers.insert_or_assign(0x10, 7).second == false && registers.at(0x10) == 7, "register file assignment");
    check(registers.lower_bound(0x11)->first == 0x41 && registers.upper_bound(0x41)->first == 0x300 && registers.lower_bound(0x301) == registers.end(), "register file bounds");
    std::vector<std::pair<std::uint32_t, std::uint32_t>> order;
    for (const auto& [offset, value] : registers) order.emplace_back(offset, value);
    check(order == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{0x10, 7}, {0x41, 2}, {0x300, 3}}, "register file order");
    auto copy = registers;
    copy[0x7000] = 5;
    check(copy.size() == 4 && registers.size() == 3 && !registers.contains(0x7000) && !(copy == registers), "register file copy");
    check(copy.erase(0x7000) == 1 && copy.erase(0x7000) == 0 && copy == registers, "register file erase");
    bool threw = false;
    try {
        static_cast<void>(registers.at(0x42));
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "register file read an unset register");
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
    AgcDriver::Pm4::Validate(makePacket(0x15, {1, 1, 1, 0x2041}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x16, {0, 0xa041}), 0);
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x15, {1, 1, 1, 0x4041}), 0); }, "dispatch modifiers");
    execute(state, makePacket(0x13, {32}));
    execute(state, makePacket(0x26, {0x1000, 1}));
    execute(state, makePacket(0x2a, {1}));
    execute(state, makePacket(0x2f, {3}));
    check(state.indexBufferSize == 32 && state.indexBase == 0x100001000ull && state.indexType == 1 && state.instanceCount == 3, "draw setup state lost");
    execute(state, makePacket(0x10, {0x00636261}, 0x2c));
    check(state.markers.back() == "abc", "marker text lost");
    execute(state, makePacket(0x10, {0}, 0x30));
    execute(state, makePacket(0x10, {0}, 0x30));
    check(state.markers.empty(), "an unbalanced marker pop changed the marker stack");
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
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}, 2), 0); }, "header flags");
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
    state.indexType = 0;
    check(AgcDriver::Pm4::ResolveDraw(packet, state).firstVertex == 0, "indexed draw invented a base vertex");
    state.userConfig[0x24a] = 0xd4d4;
    const auto offsetDraw = AgcDriver::Pm4::ResolveDraw(packet, state);
    check(offsetDraw.indexed && offsetDraw.firstVertex == 0xd4d4, "DRAW_INDEX_OFFSET_2 ignored GE_INDX_OFFSET");
    const auto address = reinterpret_cast<std::uintptr_t>(indices.data());
    const auto explicitDraw = AgcDriver::Pm4::ResolveDraw(makePacket(0x27, {4, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 4, 0}), state);
    check(explicitDraw.indexed && explicitDraw.firstVertex == 0xd4d4 && explicitDraw.indexAddress == address, "DRAW_INDEX_2 ignored GE_INDX_OFFSET");
    state.userConfig.erase(0x24a);
    expectFailure([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "GE_INDX_OFFSET");
    state.userConfig[0x24a] = 0;
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
    execute(state, makePacket(0x37, {0x40000100, low(data.data()), high(data.data()), 41, 42}));
    check(data[0] == 41 && data[1] == 42, "WRITE_DATA from the PFP failed");
    execute(state, makePacket(0x37, {0x04100200, low(data.data()), high(data.data()), 61, 62}));
    check(data[0] == 61 && data[1] == 62, "WRITE_DATA with a cache policy failed");
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x37, {0x08000100, low(data.data()), high(data.data()), 71}), 0); }, "reserved");
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x37, {0x80000100, low(data.data()), high(data.data()), 51}), 0); }, "engine");
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
    alignas(8) std::array<std::uint64_t, 2> clock{};
    const auto clockCopy = [&](std::uint64_t* target) { return makePacket(0x40, {0x06016209, 0, 0, low(target), high(target)}); };
    execute(state, clockCopy(&clock[0]));
    execute(state, clockCopy(&clock[1]));
    check(clock[0] != 0 && clock[1] >= clock[0], "GPU clock COPY_DATA failed");
    alignas(8) std::array<std::uint32_t, 2> clock32{0, 0xdeadbeef};
    execute(state, makePacket(0x40, {0x06006209, 0, 0, low(clock32.data()), high(clock32.data())}));
    check(clock32[0] != 0 && clock32[1] == 0xdeadbeef, "32-bit GPU clock COPY_DATA did not write only the low half");
    const auto clockStore =AgcDriver::Pm4::ResolveStore(clockCopy(&clock[0]), state, 64);
    check(clockStore.has_value() && clockStore->Bytes().size() == 8, "GPU clock COPY_DATA did not resolve as an 8-byte store");
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x40, {0x1020a, 0, 0, low(&clock[0]), high(&clock[0])}), 0); }, "reference-clock");
    execute(state, makePacket(0x50, {0x60000000, low(source.data()), high(source.data()), low(destination.data()), high(destination.data()), 16}));
    check(source == destination, "DMA_DATA copy failed");
    execute(state, makePacket(0x50, {0x40000000, 0x44332211, 0, low(destination.data()), high(destination.data()), 6}));
    check(destination[0] == 0x44332211 && destination[1] == 0x00002211, "DMA_DATA byte fill failed");
    constexpr std::uint32_t cachePolicies = (1u << 13u) | (2u << 25u);
    std::size_t flexibleBefore = 0;
    check(sceKernelAvailableFlexibleMemorySize(&flexibleBefore) == 0, "cannot query flexible memory");
    const auto toGds = makePacket(0x50, {0x60100000 | cachePolicies, low(source.data()), high(source.data()), 0x100, 0, 16});
    check(!AgcDriver::Pm4::ResolveStore(toGds, state, 64).has_value(), "DMA_DATA to GDS resolved as a memory store");
    execute(state, toGds);
    std::size_t flexibleAfter = 0;
    check(sceKernelAvailableFlexibleMemorySize(&flexibleAfter) == 0 && flexibleAfter == flexibleBefore, "the GDS was charged to the flexible memory budget");
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
    destination = {};
    const auto prefetch = makePacket(0x50, {0x60200000, low(source.data()), high(source.data()), low(source.data()), high(source.data()), 0x80000010});
    const auto prefetchStore = AgcDriver::Pm4::ResolveStore(prefetch, state, 64);
    check(prefetchStore.has_value() && prefetchStore->Bytes().empty(), "a DMA_DATA prefetch resolved as a memory store");
    check(!AgcDriver::Pm4::DecodeMemoryCopy(prefetch).has_value(), "a DMA_DATA prefetch decoded as a copy");
    execute(state, prefetch);
    check(source[0] == 11 && source[1] == 12 && destination[0] == 0, "a DMA_DATA prefetch wrote memory");
    expectFailure([&] { execute(state, makePacket(0x50, {0x40200000, 1, 0, 0, 0, 4})); }, "prefetch of a register");
    expectFailure([&] { execute(state, makePacket(0x50, {0x60200000, low(source.data()), high(source.data()), 0, 0, 4 | (1u << 27u)})); }, "register destination");
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

void testMemoryCopyDecode() {
    using AgcDriver::Pm4::DecodeMemoryCopy;
    constexpr std::uint64_t source = 0x1120000000ull, destination = 0x403d7b400ull;
    const auto packet = [&](std::uint32_t control, std::uint64_t from, std::uint64_t to, std::uint32_t command) {
        return makePacket(0x50, {control, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(from >> 32u), static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(to >> 32u), command});
    };
    const auto copy = DecodeMemoryCopy(packet(0x60000000, source, destination, 0xbdd800));
    check(copy.has_value() && copy->source == source && copy->destination == destination && copy->bytes == 0xbdd800, "a memory-to-memory DMA_DATA did not decode as a copy");
    check(DecodeMemoryCopy(packet(0x00000000, source, destination, 64)).has_value(), "a DMA_DATA with memory selectors 0 did not decode as a copy");
    check(!DecodeMemoryCopy(packet(0x40000000, 0x44332211, destination, 64)).has_value(), "an immediate fill decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60100000, source, 0x100, 64)).has_value(), "a DMA_DATA to the GDS decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x20000000, 0x100, destination, 64)).has_value(), "a DMA_DATA from the GDS decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 26u))).has_value(), "a register source decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 27u))).has_value(), "a register destination decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 28u))).has_value(), "a non-incrementing source decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 29u))).has_value(), "a non-incrementing destination decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, destination, 0)).has_value(), "an empty DMA_DATA decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source, source + 32, 64)).has_value(), "overlapping ranges decoded as a copy");
    check(!DecodeMemoryCopy(packet(0x60000000, source + 32, source, 64)).has_value(), "overlapping ranges below the source decoded as a copy");
    check(DecodeMemoryCopy(packet(0x60000000, source, source + 64, 64)).has_value(), "adjacent ranges did not decode as a copy");
    check(!DecodeMemoryCopy(makePacket(0x40, {0x10101, 0, 0, 0, 0})).has_value(), "a COPY_DATA decoded as a DMA_DATA copy");
}

void testMemorySynchronization() {
    struct MemoryState {
        std::uint32_t source = 0;
        std::uint32_t destination = 0;
        bool read = false;
        bool written = false;
    };
    static MemoryState memory;
    memory = {};
    AgcDriver::QueueState state;
    struct FlushHookReset {
        ~FlushHookReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
    } reset;
    const auto resolve = [](std::uint64_t address, std::size_t bytes) {
        check(bytes == sizeof(std::uint32_t), "memory transfer resolved an unrelated range");
        if (address == reinterpret_cast<std::uintptr_t>(&memory.destination)) {
            memory.written = true;
        } else {
            check(address == reinterpret_cast<std::uintptr_t>(&memory.source), "memory transfer resolved an unrelated source");
            memory.source = 42;
            memory.read = true;
        }
    };
    AgcDriver::GuestMemory::SetFlushHook(resolve);
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
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        throw std::runtime_error("range synchronization failed");
    });
    expectFailure([&] { execute(state, makePacket(0x37, {0x100, low(&memory.destination), high(&memory.destination), 99})); }, "range synchronization failed");
    check(memory.destination == 42, "failed synchronization changed the destination");
}

void testConditionalValidation() {
    alignas(4) static std::uint32_t condition = 0;
    const auto valid = makePacket(0x22, {low(&condition), high(&condition), 0, 0x3fff});
    check(AgcDriver::Pm4::UnsupportedReason(valid[0]).empty() && AgcDriver::Pm4::AccessesMemory(valid[0]), "COND_EXEC is rejected or does not synchronize guest memory");
    AgcDriver::Pm4::Validate(valid, 0);
    AgcDriver::Pm4::Validate(valid, 0x20);
    check(AgcDriver::Pm4::ConditionalWords(valid) == 0x3fff, "COND_EXEC count decoded wrong");
    check(AgcDriver::Pm4::ConditionalWords(makePacket(0x22, {low(&condition), high(&condition), 0, 0})) == 0, "empty COND_EXEC range decoded wrong");
    AgcDriver::Pm4::Validate(makePacket(0x22, {low(&condition), high(&condition), 3u << 25u, 5}), 0x20);
    const auto invalidWord = [&](std::size_t word, std::uint32_t value, std::uint32_t queue, const char* text) {
        auto packet = valid;
        packet[word] = value;
        expectFailure([&] { AgcDriver::Pm4::Validate(packet, queue); }, text);
    };
    invalidWord(1, low(&condition) | 1u, 0, "reserved address bits");
    invalidWord(1, low(&condition) | 2u, 0x20, "reserved address bits");
    invalidWord(2, 0x10000u, 0, "above 48");
    invalidWord(3, 3u << 25u, 0, "reserved control fields");
    invalidWord(3, 1u, 0x20, "reserved control fields");
    invalidWord(3, 1u << 27u, 0x20, "reserved control fields");
    invalidWord(4, 0x4000u, 0, "reserved count bits");
    invalidWord(4, 0x80000005u, 0x20, "reserved count bits");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0}, 1), 0); }, "header flags");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0}, 2), 0x20); }, "header flags");
    expectFailure([] { AgcDriver::Pm4::ConditionalWords(makePacket(0x37, {0x100, 0, 0, 0})); }, "expected COND_EXEC");
    AgcDriver::QueueState state;
    expectFailure([&] { AgcDriver::Pm4::Execute(valid, state); }, "driver execution");
}

void testConditionReadSynchronization() {
    alignas(4) static std::uint32_t condition = 0;
    static std::size_t flushed = 0;
    condition = 0;
    flushed = 0;
    struct FlushHookReset {
        ~FlushHookReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
    } reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        check(address == reinterpret_cast<std::uintptr_t>(&condition) && bytes == sizeof(condition), "COND_EXEC synchronized an unrelated range");
        condition = 0x80;
        ++flushed;
    });
    const auto packet = makePacket(0x22, {low(&condition), high(&condition), 0, 5});
    check(AgcDriver::Pm4::ReadCondition(packet) == 0x80 && flushed == 1, "COND_EXEC read its condition before the GPU work that writes it");
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        throw std::runtime_error("condition synchronization failed");
    });
    expectFailure([&] { AgcDriver::Pm4::ReadCondition(packet); }, "condition synchronization failed");
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    expectFailure([] { AgcDriver::Pm4::ReadCondition(makePacket(0x22, {0x1000, 0, 0, 5})); }, "guest");
}

void testWriteChangedKeepsUntouchedBytes() {
    alignas(256) static std::uint8_t guest[256];
    std::memset(guest, 0, sizeof(guest));
    std::vector<std::byte> original(sizeof(guest)), current(sizeof(guest));
    current[3] = std::byte{7};
    guest[100] = 0x55;
    AgcDriver::GuestMemory::WriteChanged(reinterpret_cast<std::uintptr_t>(guest), current, original);
    check(guest[3] == 7 && guest[100] == 0x55, "write-back rolled back a byte the GPU did not change");
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
    AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000, 0x2}), 0);
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000, 0x2}), 0x20); }, "compute queue");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x039, 0x1000, 0x2}), 0); }, "counter dump event index");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1004, 0x2}), 0); }, "misaligned occlusion counter");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0, 0}), 0); }, "null or misaligned occlusion counter");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000}), 0); }, "packet size");
    AgcDriver::Pm4::Validate(makePacket(0x46, {0x138, 0x07fffc00, 0}), 0);
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x138, 0x07fffc00, 0}), 0x20); }, "compute queue");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x038, 0x07fffc00, 0}), 0); }, "statistics control event index");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x138, 0x07fffc00}), 0); }, "packet size");
    for (const auto& [control, high] : {std::pair{0x07fffc01u, 0u}, std::pair{0x07fffc08u, 0u}, std::pair{0x07fffa00u, 0u}, std::pair{0x07fffe00u, 0u}, std::pair{0x03fffc00u, 0u}, std::pair{0x0ffffc00u, 0u}, std::pair{0x07fffc00u, 1u}}) {
        expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x138, control, high}), 0); }, "pixel pipe statistics control other than");
    }
    for (const auto bit : {0x40u, 0x80u, 0x800u, 0x80000000u}) {
        expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410u | bit}), 0); }, "reserved bits");
    }
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410}, 2), 0); }, "header flags");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x13a, 0, 0}), 0); }, "event type 58");
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
    invalidWord(0, captured[0] | 2u, "header flags");
    invalidWord(1, 4, "control flags");
    invalidWord(1, 0x00800000, "control flags");
    invalidWord(3, 0x100, "range high bits");
    invalidWord(5, 0x100, "range high bits");
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x86007fc0, 0xfffffffe, 0xff, 1, 0, 0x19, 0xc3e1}), 0);
    invalidWord(6, 0x10000, "poll interval");
    invalidWord(7, 0x40000, "GCR flags");
    invalidWord(7, 0x2000, "cache discard");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 2, 0, 0xffffffff, 0xff, 0, 0}), 0); }, "range exceeds");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0xffffffff, 0xff, 2, 0, 0, 0}), 0); }, "range exceeds");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0}), 0); }, "packet size");
    expectFailure([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0, 0, 0, 0}), 0); }, "packet size");
}

void testPredication() {
    alignas(16) std::uint64_t flag[4] = {0, 0, 0, 0};
    auto* flag32 = reinterpret_cast<std::uint32_t*>(&flag[2]);
    const auto setPredication = [&](std::uint32_t operation, bool executeWhenSet, const void* address) {
        return makePacket(0x20, {(operation << 16u) | (executeWhenSet ? 0x100u : 0u) | 0x1000u, low(address), high(address)});
    };
    AgcDriver::QueueState state;
    check(AgcDriver::Pm4::PredicationPasses(state), "inactive predication must pass");
    execute(state, setPredication(3, true, &flag[0]));
    check(state.predication.operation == 3 && state.predication.executeWhenSet && state.predication.address == reinterpret_cast<std::uintptr_t>(&flag[0]), "BOOL64 predication state mismatch");
    check(!AgcDriver::Pm4::PredicationPasses(state), "zero BOOL64 value must skip draw-visible packets");
    flag[0] = 1ull << 40u;
    check(AgcDriver::Pm4::PredicationPasses(state), "upper BOOL64 bits must count");
    execute(state, setPredication(3, false, &flag[0]));
    check(!AgcDriver::Pm4::PredicationPasses(state), "non-zero BOOL64 value must skip draw-not-visible packets");
    flag[0] = 0;
    check(AgcDriver::Pm4::PredicationPasses(state), "zero BOOL64 value must run draw-not-visible packets");
    execute(state, setPredication(4, true, &flag[2]));
    flag[2] = 0xffffffff00000000ull;
    check(*flag32 == 0 && !AgcDriver::Pm4::PredicationPasses(state), "BOOL32 must read only 32 bits");
    *flag32 = 7;
    check(AgcDriver::Pm4::PredicationPasses(state), "non-zero BOOL32 value must run draw-visible packets");
    execute(state, setPredication(0, false, nullptr));
    check(state.predication.operation == 0 && AgcDriver::Pm4::PredicationPasses(state), "clear must end predication");

    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(1, true, &flag[0]), 0); }, "query predication");
    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(2, true, &flag[0]), 0); }, "query predication");
    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(5, true, &flag[0]), 0); }, "invalid predication operation");
    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(3, true, &flag[0]), 0x20); }, "compute queue");
    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(3, true, nullptr), 0); }, "unaligned predication address");
    expectFailure([&] { AgcDriver::Pm4::Validate(setPredication(3, true, reinterpret_cast<const std::uint8_t*>(&flag[0]) + 8), 0); }, "unaligned predication address");
    auto continued = setPredication(3, true, &flag[0]);
    continued[1] |= 1u << 31u;
    expectFailure([&] { AgcDriver::Pm4::Validate(continued, 0); }, "SET_PREDICATION bits");
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x20, {3u << 16u, low(&flag[0]), high(&flag[0])}, 1), 0); }, "header flags");

    AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}, 1), 0);
    AgcDriver::Pm4::Validate(makePacket(0x46, {0x410}, 1), 0);
    const auto call = makePacket(0x3f, {0x1000, 0, 0x0f200010}, 1);
    AgcDriver::Pm4::Validate(call, 0);
    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x3f, {0x1000, 0, 0x0f200010}), 0); }, "nested command buffers");
    expectFailure([&] { AgcDriver::Pm4::Execute(call, state); }, "nested command buffers");
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
    const auto unsupportedEvent = makePacket(0x46, {0x0d});
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

void testResetQueueSequence() {
    check(AgcDriver::Pm4::FillerPacket(0xffff1000u) && AgcDriver::Pm4::PacketWords(0xffff1000u) == 1, "a NOP with count 0x3FFF is one dword");
    check(!AgcDriver::Pm4::FillerPacket(0xffff1001u) && !AgcDriver::Pm4::FillerPacket(0xfffe1000u), "only the one-dword NOP header is filler");
    AgcDriver::Pm4::Validate(std::vector<std::uint32_t>{0xffff1000u}, 0);
    std::array<std::uint32_t, 1> destination{};
    std::vector<std::uint32_t> commands{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce2003ffu, 0x00000000u, 0xc0039f00u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0036300u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0036400u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0002f00u, 0x00000001u, 0xc0017a00u, 0x20000243u, 0x00000480u, 0xc0012600u, 0x00000000u, 0x00000000u, 0xc0001300u, 0xffffffffu, 0xc0021102u, 0x00000001u, 0x00000000u, 0x00000000u, 0xc0021100u, 0x00000001u, 0x00000000u, 0x00000000u, 0xc0065800u, 0x86007fc0u, 0xfffffffeu, 0x000000ffu, 0x00000001u, 0x00000000u, 0x00000019u, 0x0000c3e1u, 0xc0004600u, 0x0000002eu, 0xc0004600u, 0x0000002cu, 0xc0004600u, 0x00000407u, 0xc0004600u, 0x00000410u, 0xc0024600u, 0x00000138u, 0x07fffc00u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
    for (const auto& packet : {makePacket(0x81, {0, 83}), makePacket(0x83, {0, 1, low(destination.data()), high(destination.data())})}) commands.insert(commands.end(), packet.begin(), packet.end());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "sceAgcDcbResetQueue sequence submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 83, "worker did not execute past the sceAgcDcbResetQueue sequence");
}

std::vector<std::uint32_t> joinPackets(std::initializer_list<std::vector<std::uint32_t>> packets) {
    std::vector<std::uint32_t> words;
    for (const auto& packet : packets) words.insert(words.end(), packet.begin(), packet.end());
    return words;
}

std::vector<std::uint32_t> writeWord(std::uint32_t& target, std::uint32_t value) {
    return makePacket(0x37, {0x00100200, low(&target), high(&target), value});
}

std::vector<std::uint32_t> conditional(const std::uint32_t& condition, std::uint32_t words, std::uint32_t control = 0) {
    return makePacket(0x22, {low(&condition), high(&condition), control, words});
}

std::vector<std::uint32_t> indirectBuffer(const std::vector<std::uint32_t>& target, bool chain = false) {
    return makePacket(0x3f, {low(target.data()), high(target.data()), static_cast<std::uint32_t>(target.size()) | (chain ? 1u << 20u : 0u)});
}

void submitWords(std::vector<std::uint32_t>& words, std::uint32_t queue = 0) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, "conditional submission failed");
}

void testRegisterListsReadAtSubmission() {
    alignas(8) static std::uint32_t gate = 0;
    static std::uint32_t done = 0;
    static std::array<std::uint32_t, 2> registers{0x10, 74};
    auto words = joinPackets({
        makePacket(0x3c, {0x13, low(&gate), high(&gate), 1, 0xffffffffu, 0x19}),
        makePacket(0x9f, {low(registers.data()), high(registers.data()), 0x80000000, 1}),
        writeWord(done, 1)});
    submitWords(words);
    registers[0] = 0x3a888889;
    std::atomic_ref<std::uint32_t>(gate).store(1);
    AgcDriverWaitIdle_nid_postfix();
    check(std::atomic_ref<std::uint32_t>(done).load() == 1, "register list rewritten after submission was read by the worker");
}

void testSuspendPointWritesQueuedLabels() {
    alignas(8) static std::uint32_t gate = 0, marker = 0;
    gate = 0;
    marker = 0;
    auto words = joinPackets({makePacket(0x3c, {0x13, low(&gate), high(&gate), 1, 0xffffffffu, 0x190}),
                              makePacket(0x49, {0x0030c514, 0x20000000, low(&marker), high(&marker), 1, 0, 0})});
    submitWords(words);
    AgcDriverSuspendPoint_nid_postfix();
    std::atomic_ref<std::uint32_t>(gate).store(1);
    for (int waited = 0; waited < 2000 && std::atomic_ref<std::uint32_t>(marker).load() == 0; ++waited) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(std::atomic_ref<std::uint32_t>(marker).load() == 1, "a label queued before a suspend point was never written");
    AgcDriverWaitIdle_nid_postfix();
}

void testConditionalSubmission() {
    alignas(8) static std::uint32_t zero = 0, one = 1, condition = 0;
    static std::array<std::uint32_t, 16> results{};
    results.fill(0);
    const auto run = [](std::vector<std::uint32_t> words, std::uint32_t queue = 0) {
        submitWords(words, queue);
        AgcDriverWaitIdle_nid_postfix();
    };
    run(joinPackets({conditional(one, 5), writeWord(results[0], 11), writeWord(results[1], 12)}));
    check(results[0] == 11 && results[1] == 12, "COND_EXEC skipped a range whose condition is set");
    run(joinPackets({conditional(zero, 6), {0x80000000u}, writeWord(results[0], 21), writeWord(results[1], 22)}));
    check(results[0] == 11 && results[1] == 22, "COND_EXEC did not skip exactly its range when the condition is zero");
    run(joinPackets({conditional(zero, 0), writeWord(results[2], 23)}));
    check(results[2] == 23, "an empty COND_EXEC range skipped the next packet");
    alignas(4) static std::uint32_t never = 0;
    never = 0;
    auto skippedWait = joinPackets({conditional(zero, 9), makePacket(0x3c, {0x13, low(&never), high(&never), 1, 0xffffffffu, 0x190}), makePacket(0x10, {0}, 0x30), writeWord(results[14], 24)});
    submitWords(skippedWait);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(results[14]).load() != 24 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool skipped = std::atomic_ref<std::uint32_t>(results[14]).load() == 24;
    std::atomic_ref<std::uint32_t>(never).store(1);
    AgcDriverWaitIdle_nid_postfix();
    check(skipped, "a WAIT_REG_MEM inside a skipped COND_EXEC range was executed");

    condition = 1;
    run(joinPackets({writeWord(condition, 0), conditional(condition, 5), writeWord(results[0], 31), writeWord(results[1], 32)}));
    check(condition == 0 && results[0] == 11 && results[1] == 32, "COND_EXEC read its condition before an earlier packet of its queue stored it");
    run(joinPackets({writeWord(condition, 9), conditional(condition, 5), writeWord(results[0], 41)}));
    check(results[0] == 41, "COND_EXEC skipped a range whose condition an earlier packet of its queue set");
    condition = 0;
    run(joinPackets({conditional(condition, 5, 3u << 25u), writeWord(results[3], 42), writeWord(condition, 1), conditional(condition, 5, 1u << 25u), writeWord(results[4], 43)}), 0x20);
    check(results[3] == 0 && results[4] == 43, "compute queue COND_EXEC with a cache policy evaluated wrong");

    run(joinPackets({conditional(one, 10), conditional(zero, 10), writeWord(results[5], 51), writeWord(results[6], 52), writeWord(results[7], 53)}));
    check(results[5] == 0 && results[6] == 0 && results[7] == 53, "a nested COND_EXEC range reaching past the outer range was not skipped exactly");
    run(joinPackets({conditional(zero, 10), conditional(one, 10), writeWord(results[5], 54), writeWord(results[6], 55), writeWord(results[7], 56)}));
    check(results[5] == 0 && results[6] == 55 && results[7] == 56, "a skipped COND_EXEC range did not skip the COND_EXEC inside it");

    static std::vector<std::uint32_t> target;
    target = joinPackets({writeWord(results[8], 61), conditional(zero, 5), writeWord(results[9], 62), writeWord(results[10], 63)});
    run(joinPackets({conditional(zero, 4), indirectBuffer(target), writeWord(results[11], 64)}));
    check(results[8] == 0 && results[10] == 0 && results[11] == 64, "COND_EXEC over an INDIRECT_BUFFER did not skip the whole buffer");
    run(joinPackets({conditional(one, 4), indirectBuffer(target), writeWord(results[11], 65)}));
    check(results[8] == 61 && results[9] == 0 && results[10] == 63 && results[11] == 65, "COND_EXEC over an INDIRECT_BUFFER or inside one evaluated wrong");

    const auto rejected = [](std::vector<std::uint32_t> words, const char* text) {
        results[12] = 0;
        expectFailure([&] { submitWords(words); }, text);
        AgcDriverWaitIdle_nid_postfix();
        check(results[12] == 0, "a rejected conditional submission executed a prefix");
    };
    const auto sentinel = writeWord(results[12], 1);
    rejected(joinPackets({sentinel, conditional(one, 3), writeWord(results[13], 1)}), "ends inside a packet");
    rejected(joinPackets({sentinel, conditional(one, 6), writeWord(results[13], 1)}), "exceeds its command buffer");
    rejected(joinPackets({sentinel, conditional(one, 6), {0xc004105cu, 7, 0, 1, 0, 0}}), "a flip inside a conditional execution range");
    rejected(joinPackets({sentinel, conditional(one, 2), makePacket(0x59, {0x80000000u})}), "REWIND inside a conditional execution range");
    rejected(joinPackets({sentinel, conditional(one, 4), indirectBuffer(target, true)}), "chained INDIRECT_BUFFER");
    rejected(joinPackets({sentinel, conditional(one, 2), {0x40000000u, 0}}), "whole packets");
    rejected(joinPackets({sentinel, makePacket(0x22, {low(&one), high(&one), 0, 0x4005}), writeWord(results[13], 1)}), "reserved count bits");
    static std::vector<std::uint32_t> unaligned, overlong;
    unaligned = joinPackets({conditional(one, 3), writeWord(results[13], 1)});
    overlong = joinPackets({conditional(one, 10), writeWord(results[13], 1)});
    rejected(joinPackets({sentinel, indirectBuffer(unaligned)}), "ends inside a packet");
    rejected(joinPackets({sentinel, indirectBuffer(overlong), writeWord(results[13], 1)}), "exceeds its command buffer");
    check(results[13] == 0, "a rejected conditional submission executed a guarded packet");
}

std::vector<std::uint32_t> branch(std::uint32_t mode, std::uint32_t function, const std::vector<std::uint32_t>* first, const std::vector<std::uint32_t>* second) {
    const auto address = [](const std::vector<std::uint32_t>* target) { return target ? reinterpret_cast<std::uintptr_t>(target->data()) : std::uintptr_t{0}; };
    const auto size = [](const std::vector<std::uint32_t>* target) { return target ? static_cast<std::uint32_t>(target->size()) : 0u; };
    return makePacket(0x3f, {mode | (function << 8u), 0, 0, 0, 0, 0, 0, static_cast<std::uint32_t>(address(first)), static_cast<std::uint32_t>(address(first) >> 32u), size(first), static_cast<std::uint32_t>(address(second)), static_cast<std::uint32_t>(address(second) >> 32u), size(second)});
}

void testBranchSubmission() {
    static std::array<std::uint32_t, 4> results{};
    static std::vector<std::uint32_t> first, second;
    results.fill(0);
    first = joinPackets({writeWord(results[0], 71)});
    second = joinPackets({writeWord(results[1], 72)});
    auto words = joinPackets({branch(1, 0, &first, nullptr), writeWord(results[2], 73)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    check(results[0] == 71 && results[1] == 0 && results[2] == 73, "an always-taken if-then COND_INDIRECT_BUFFER did not run its buffer");
    results.fill(0);
    words = joinPackets({branch(2, 0, &first, &second), writeWord(results[2], 74)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    check(results[0] == 71 && results[1] == 0 && results[2] == 74, "an always-taken if-then-else COND_INDIRECT_BUFFER ran the wrong buffer");
    results.fill(0);
    words = joinPackets({branch(1, 0, nullptr, nullptr), writeWord(results[2], 75)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    check(results[2] == 75, "an empty COND_INDIRECT_BUFFER skipped the next packet");
    for (const auto& [mode, function, text] : {std::tuple{1u, 3u, "with a comparison"}, std::tuple{0u, 0u, "invalid COND_INDIRECT_BUFFER mode"}}) {
        results.fill(0);
        words = joinPackets({writeWord(results[3], 1), branch(mode, function, &first, nullptr)});
        expectFailure([&] { submitWords(words); }, text);
        AgcDriverWaitIdle_nid_postfix();
        check(results[0] == 0 && results[3] == 0, "a rejected COND_INDIRECT_BUFFER submission executed a packet");
    }
}

void testPredicatedSubmission() {
    alignas(16) std::uint64_t flag[2] = {0, 0};
    alignas(16) std::array<std::uint32_t, 4> written{};
    alignas(16) std::array<std::uint32_t, 1> nestedWritten{};
    const auto nested = makePacket(0x37, {0x100, low(nestedWritten.data()), high(nestedWritten.data()), 3});
    const auto call = makePacket(0x3f, {low(nested.data()), high(nested.data()), 0x0f200000u | static_cast<std::uint32_t>(nested.size())}, 1);
    const auto submit = [](std::vector<std::uint32_t>& commands) {
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        check(sceAgcDriverSubmitDcb(&packet) == 0, "predicated submission failed");
        AgcDriverWaitIdle_nid_postfix();
    };
    std::vector<std::uint32_t> commands;
    for (const auto& packet : {
        makePacket(0x20, {0x31100, low(&flag[0]), high(&flag[0])}),
        makePacket(0x37, {0x100, low(&written[0]), high(&written[0]), 7}, 1),
        call,
        makePacket(0x20, {0, 0, 0}),
        makePacket(0x37, {0x100, low(&written[1]), high(&written[1]), 9}, 1)
    }) commands.insert(commands.end(), packet.begin(), packet.end());
    submit(commands);
    check(written[0] == 0, "packet with a false predicate was executed");
    check(nestedWritten[0] == 0, "command buffer with a false predicate was executed");
    check(written[1] == 9, "cleared predication skipped a packet");
    flag[0] = 1;
    commands.clear();
    for (const auto& packet : {
        makePacket(0x20, {0x31100, low(&flag[0]), high(&flag[0])}),
        makePacket(0x37, {0x100, low(&written[2]), high(&written[2]), 5}, 1),
        call,
        makePacket(0x20, {0x31000, low(&flag[0]), high(&flag[0])}),
        makePacket(0x37, {0x100, low(&written[3]), high(&written[3]), 6}, 1),
        makePacket(0x20, {0, 0, 0})
    }) commands.insert(commands.end(), packet.begin(), packet.end());
    submit(commands);
    check(written[2] == 5, "packet with a true predicate was skipped");
    check(nestedWritten[0] == 3, "command buffer with a true predicate was skipped");
    check(written[3] == 0, "draw-not-visible packet ran with a set value");
    auto chain = makePacket(0x3f, {low(nested.data()), high(nested.data()), 0x0f300000u | static_cast<std::uint32_t>(nested.size())}, 1);
    Packet chained{chain.data(), static_cast<std::uint32_t>(chain.size()), 0, {}};
    expectFailure([&] { sceAgcDriverSubmitDcb(&chained); }, "predicated command buffer chains");
    AgcDriverWaitIdle_nid_postfix();
}

std::vector<std::uint32_t> condWrite(std::uint32_t control, const std::uint32_t& poll, std::uint32_t reference, std::uint32_t mask, std::uint32_t& target, std::uint32_t value) {
    return makePacket(0x45, {control, low(&poll), high(&poll), reference, mask, low(&target), high(&target), value});
}

void testConditionalWrite() {
    AgcDriver::QueueState state;
    alignas(4) std::uint32_t poll = 0x1234u;
    alignas(4) std::uint32_t target = 0;
    const auto valid = condWrite(0x113, poll, 0x34, 0xff, target, 7);
    check(AgcDriver::Pm4::Name(valid[0]) == "COND_WRITE" && valid[0] == 0xc0074500u, "COND_WRITE header or name mismatch");
    check(AgcDriver::Pm4::UnsupportedReason(valid[0]).empty() && AgcDriver::Pm4::AccessesMemory(valid[0]), "COND_WRITE is rejected or does not synchronize guest memory");

    const std::array<std::pair<std::uint32_t, bool>, 7> functions{{{0, true}, {1, false}, {2, true}, {3, true}, {4, false}, {5, true}, {6, false}}};
    for (const auto& [function, writes] : functions) {
        target = 0;
        execute(state, condWrite(0x110 | function, poll, 0x34, 0xff, target, 9));
        check(target == (writes ? 9u : 0u), "COND_WRITE compared the masked poll value incorrectly");
    }
    target = 0;
    execute(state, condWrite(0x111, poll, 0x35, 0xff, target, 10));
    check(target == 10, "COND_WRITE less-than did not write");
    target = 0;
    execute(state, condWrite(0x116, poll, 0x1233, 0xffffffffu, target, 11));
    check(target == 11, "COND_WRITE greater-than with a full mask did not write");

    expectFailure([&] { AgcDriver::Pm4::Validate(makePacket(0x45, {0x113, low(&poll), high(&poll), 0, 0, low(&target), high(&target)}), 0); }, "packet size");
    expectFailure([&] { AgcDriver::Pm4::Validate(condWrite(0x2000113, poll, 0, 0, target, 0), 0); }, "reserved");
    expectFailure([&] { AgcDriver::Pm4::Validate(condWrite(0x103, poll, 0, 0, target, 0), 0); }, "register-space");
    expectFailure([&] { AgcDriver::Pm4::Validate(condWrite(0x117, poll, 0, 0, target, 0), 0); }, "compare function");
    expectFailure([&] { AgcDriver::Pm4::Validate(condWrite(0x013, poll, 0, 0, target, 0), 0); }, "destination");
    expectFailure([&] { AgcDriver::Pm4::Validate(condWrite(0x213, poll, 0, 0, target, 0), 0); }, "destination");
    auto misaligned = condWrite(0x113, poll, 0, 0, target, 0);
    misaligned[2] += 2;
    expectFailure([&] { AgcDriver::Pm4::Validate(misaligned, 0); }, "misaligned");
    misaligned = condWrite(0x113, poll, 0, 0, target, 0);
    misaligned[6] += 1;
    expectFailure([&] { AgcDriver::Pm4::Validate(misaligned, 0); }, "misaligned");

    struct MemoryState {
        std::uint32_t poll = 0;
        std::uint32_t target = 0;
        bool read = false;
        bool written = false;
    };
    static MemoryState memory;
    memory = {};
    struct FlushHookReset {
        ~FlushHookReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
    } reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        check(bytes == sizeof(std::uint32_t), "COND_WRITE resolved an unrelated range");
        if (address == reinterpret_cast<std::uintptr_t>(&memory.target)) {
            memory.written = true;
        } else {
            check(address == reinterpret_cast<std::uintptr_t>(&memory.poll), "COND_WRITE resolved an unrelated poll address");
            memory.poll = 5;
            memory.read = true;
        }
    });
    execute(state, condWrite(0x113, memory.poll, 5, 0xffffffffu, memory.target, 12));
    check(memory.read && memory.written && memory.target == 12, "COND_WRITE used a stale poll value or skipped synchronizing its target");
}

void testConditionalWriteSubmission() {
    alignas(4) static std::uint32_t poll = 0;
    static std::array<std::uint32_t, 2> results{};
    results.fill(0);
    poll = 0;
    auto words = joinPackets({writeWord(poll, 5), condWrite(0x113, poll, 5, 0xffffffffu, results[0], 51), condWrite(0x114, poll, 5, 0xffffffffu, results[1], 52)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    check(results[0] == 51, "COND_WRITE read its poll value before an earlier packet of its queue stored it");
    check(results[1] == 0, "COND_WRITE wrote although its comparison failed");
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

void testUnwrittenUserData() {
    AgcDriver::Registers shader{{0x8c, 0x100}, {0x8d, 0}, {0x240, 0x200}};
    check(AgcDriver::DriverDetail::readUserData(shader, 0x8c) == 0x100 && AgcDriver::DriverDetail::readUserData(shader, 0x240) == 0x200, "a written user data register was not read");
    check(AgcDriver::DriverDetail::readUserData(shader, 0x8d) == 0, "a user data register written as zero was not read");
    check(AgcDriver::DriverDetail::readUserData(shader, 0x95) == 0 && AgcDriver::DriverDetail::readUserData(shader, 0x241) == 0, "an unwritten user data register does not read zero");
    expectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::readRegister(shader, 0x95)); }, "required shader register");
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "failure") {
            testAsyncMemoryFailure();
            std::puts("PM4 asynchronous memory failure propagated to idle, suspend, submit and shutdown");
            return 0;
        }
        testCatalog();
        testWriteChangedKeepsUntouchedBytes();
        testRegisters();
        testRegisterFile();
        testContextAndBases();
        testIndexedDraw();
        testAutoDraw();
        testIndirectDraw();
        testMemory();
        testCopies();
        testMemoryCopyDecode();
        testMemorySynchronization();
        testConditionalValidation();
        testConditionReadSynchronization();
        testEventWrite();
        testAcquireMem();
        testConditionalWrite();
        testConditionalWriteSubmission();
        testPredication();
        testUnwrittenUserData();
        testDriverSubmission();
        testResetQueueSequence();
        testRegisterListsReadAtSubmission();
        testPredicatedSubmission();
        testConditionalSubmission();
        testSuspendPointWritesQueuedLabels();
        testBranchSubmission();
        LibcRunShutdown_nid_postfix();
        std::puts("PM4 catalog, registers, state, memory, conditional execution and submission tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
