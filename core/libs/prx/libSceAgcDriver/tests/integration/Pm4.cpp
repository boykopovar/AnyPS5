#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

extern "C" int APS5_VABI sceKernelAvailableFlexibleMemorySize(size_t* size);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

bool FailureMode() {
    const auto& arguments = Testing::Arguments();
    return !arguments.empty() && arguments[0] == "failure";
}

void RequireDefaultMode() {
    if (FailureMode()) Testing::Skip("runs only in the default registration");
}

void RequireFailureMode() {
    if (!FailureMode()) Testing::Skip("runs only in the failure registration");
}

template<typename TAction>
void RequireRejected(const TAction& action, std::string_view text, std::source_location location = std::source_location::current()) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected a PM4 rejection mentioning \"" + std::string(text) + "\"", location);
    Require(std::string_view(error.what()).find(text) != std::string_view::npos,
            "rejection \"" + std::string(error.what()) + "\" does not mention \"" + std::string(text) + "\"", location);
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

void submitWords(std::vector<std::uint32_t>& words, std::uint32_t queue = 0, std::source_location location = std::source_location::current()) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, "conditional submission failed", location);
}

std::vector<std::uint32_t> branch(std::uint32_t mode, std::uint32_t function, const std::vector<std::uint32_t>* first, const std::vector<std::uint32_t>* second) {
    const auto address = [](const std::vector<std::uint32_t>* target) { return target ? reinterpret_cast<std::uintptr_t>(target->data()) : std::uintptr_t{0}; };
    const auto size = [](const std::vector<std::uint32_t>* target) { return target ? static_cast<std::uint32_t>(target->size()) : 0u; };
    return makePacket(0x3f, {mode | (function << 8u), 0, 0, 0, 0, 0, 0, static_cast<std::uint32_t>(address(first)), static_cast<std::uint32_t>(address(first) >> 32u), size(first), static_cast<std::uint32_t>(address(second)), static_cast<std::uint32_t>(address(second) >> 32u), size(second)});
}

std::vector<std::uint32_t> condWrite(std::uint32_t control, const std::uint32_t& poll, std::uint32_t reference, std::uint32_t mask, std::uint32_t& target, std::uint32_t value) {
    return makePacket(0x45, {control, low(&poll), high(&poll), reference, mask, low(&target), high(&target), value});
}

class FlushHookReset {
public:
    FlushHookReset() = default;
    FlushHookReset(const FlushHookReset&) = delete;
    FlushHookReset& operator=(const FlushHookReset&) = delete;
    ~FlushHookReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
};

#ifdef _WIN32
class ReadOnlyPage {
public:
    ReadOnlyPage() : memory(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) {
        Require(memory != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        if (VirtualProtect(memory, 4096, PAGE_READONLY, &previous) == 0) {
            VirtualFree(memory, 0, MEM_RELEASE);
            Testing::Fail("VirtualProtect failed");
        }
    }
    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;
    ~ReadOnlyPage() { VirtualFree(memory, 0, MEM_RELEASE); }

    void* Address() const noexcept { return memory; }

private:
    void* memory;
};
#endif

const Case catalog{"Catalog_ReferenceOpcodes_AreUniqueNamedAndValidated", [] {
    RequireDefaultMode();
    std::set<std::uint32_t> values;
    for (const auto& opcode : AgcDriver::Pm4::Opcodes) {
        const auto opcodeText = "opcode " + std::to_string(opcode.value);
        Require(values.insert(opcode.value).second, "duplicate PM4 " + opcodeText);
        const auto packet = makePacket(opcode.value, {0});
        RequireEqual(AgcDriver::Pm4::Name(packet[0]), std::string(opcode.name), opcodeText + " name");
        const auto reason = AgcDriver::Pm4::UnsupportedReason(packet[0]);
        if (!reason.empty()) RequireRejected([&] { AgcDriver::Pm4::Validate(packet, 0); }, reason);
    }
    RequireEqual(values.size(), std::size_t{55}, "reference opcode catalog size");
}};

const Case unknownOpcode{"Validate_UnknownOpcode_IsRejected", [] {
    RequireDefaultMode();
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0xff, {0}), 0); }, "not known");
}};

const Case customOpcodeNames{"Name_CustomOpcodes_AreDecoded", [] {
    RequireDefaultMode();
    const std::array<std::pair<std::uint32_t, const char*>, 11> custom{{
        {5, "DRAW_RESET"}, {6, "WAIT_FLIP_DONE"}, {9, "DISPATCH_RESET"}, {11, "PUSH_MARKER"},
        {12, "POP_MARKER"}, {20, "ACQUIRE_MEM_CUSTOM"}, {21, "WRITE_DATA_CUSTOM"}, {23, "FLIP"},
        {24, "RELEASE_MEM_CUSTOM"}, {25, "DMA_DATA_CUSTOM"}, {26, "CONTEXT_STATE"}
    }};
    for (const auto& [id, name] : custom) {
        RequireEqual(AgcDriver::Pm4::Name(makePacket(0x10, {0}, id << 2)[0]), std::string(name), "custom opcode " + std::to_string(id) + " name");
    }
}};

const Case writeChangedKeepsUntouchedBytes{"WriteChanged_UntouchedBytes_AreKept", [] {
    RequireDefaultMode();
    alignas(256) static std::uint8_t guest[256];
    std::memset(guest, 0, sizeof(guest));
    std::vector<std::byte> original(sizeof(guest)), current(sizeof(guest));
    current[3] = std::byte{7};
    guest[100] = 0x55;
    AgcDriver::GuestMemory::WriteChanged(reinterpret_cast<std::uintptr_t>(guest), current, original);
    Require(guest[3] == 7 && guest[100] == 0x55, "write-back rolled back a byte the GPU did not change");
}};

const Case registerPackets{"Execute_RegisterPackets_UpdateTheirBanks", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    execute(state, makePacket(0x79, {0x242, 4}));
    Require(state.userConfig.at(0x242) == 4, "primitive type register write was lost");
    execute(state, makePacket(0x79, {0x242, 6}));
    Require(state.userConfig.at(0x242) == 6, "primitive type register update was lost");
    const std::array<std::uint32_t, 3> indirectOpcodes{0x9f, 0x63, 0x64};
    for (auto opcode : indirectOpcodes) {
        std::array<std::uint32_t, 6> pairs{0x10, 41, 0x11, 42, 0x10, 43};
        auto packet = makePacket(opcode, {low(pairs.data()), high(pairs.data()), 0x80000000, 3});
        execute(state, packet);
        const auto& registers = opcode == 0x9f ? state.context : opcode == 0x63 ? state.shader : state.userConfig;
        Require(registers.at(0x10) == 43 && registers.at(0x11) == 42, "indirect register order or bank lost");
        pairs[0] = 0x12;
        pairs[4] = 0xffffffffu;
        RequireRejected([&] { execute(state, packet); }, "sentinel");
        Require(!registers.contains(0x12), "invalid indirect packet partially changed state");
        packet[1] = 0x1000;
        packet[2] = 0;
        RequireRejected([&] { execute(state, packet); }, "guest");
        packet[3] = 0;
        RequireRejected([&] { AgcDriver::Pm4::Validate(packet, 0); }, "control");
    }
    execute(state, makePacket(0x69, {0x11, 50, 51}));
    Require(state.context.at(0x11) == 50 && state.context.at(0x12) == 51, "direct registers not sequential");
    execute(state, makePacket(0x7a, {0x10, 60}));
    Require(state.userConfig.at(0x10) == 60, "uconfig index zero failed");
    execute(state, makePacket(0x7a, {0x20000243, 0x441}));
    Require(state.indexType == 1 && state.userConfig.at(0x243) == 0x441, "indexed VGT_INDEX_TYPE write lost state");
    RequireRejected([&] { execute(state, makePacket(0x7a, {0x10000010, 1})); }, "bank selection");
    RequireRejected([&] { execute(state, makePacket(0x69, {0xffff, 1, 2})); }, "overflow");
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x9f, {0, 0, 0x80000000, 0}), 0x20); }, "compute");
}};

const Case registerFile{"Registers_MapOperations_BehaveLikeAnOrderedMap", [] {
    RequireDefaultMode();
    AgcDriver::Registers registers{{0x300, 3}, {0x10, 1}, {0x41, 2}};
    Require(registers.size() == 3 && !registers.contains(0x11) && registers.at(0x41) == 2, "register file lookup");
    Require(registers.find(0x12) == registers.end() && registers.find(0x10)->second == 1, "register file find");
    Require(!registers.emplace(0x10, 9).second && registers.at(0x10) == 1, "register file emplace replaced a value");
    Require(registers.insert_or_assign(0x10, 7).second == false && registers.at(0x10) == 7, "register file assignment");
    Require(registers.lower_bound(0x11)->first == 0x41 && registers.upper_bound(0x41)->first == 0x300 && registers.lower_bound(0x301) == registers.end(), "register file bounds");
    std::vector<std::pair<std::uint32_t, std::uint32_t>> order;
    for (const auto& [offset, value] : registers) order.emplace_back(offset, value);
    Require(order == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{0x10, 7}, {0x41, 2}, {0x300, 3}}, "register file order");
    auto copy = registers;
    copy[0x7000] = 5;
    Require(copy.size() == 4 && registers.size() == 3 && !registers.contains(0x7000) && !(copy == registers), "register file copy");
    Require(copy.erase(0x7000) == 1 && copy.erase(0x7000) == 0 && copy == registers, "register file erase");
    Testing::RequireThrows<std::out_of_range>([&] { static_cast<void>(registers.at(0x42)); }, "register file read an unset register");
}};

const Case contextAndBases{"Execute_ContextAndBasePackets_TrackQueueState", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    execute(state, makePacket(0x69, {0x10, 17}));
    execute(state, makePacket(0x76, {0x20c, 2}));
    execute(state, makePacket(0x10, {3, 0}, 0x68));
    Require(state.context == AgcDriver::InitialContextRegisters() && state.shader.at(0x20c) == 2, "push-clear reset wrong state");
    RequireRejected([&] { execute(state, makePacket(0x10, {1, 0}, 0x68)); }, "already pushed");
    execute(state, makePacket(0x69, {0x10, 19}));
    execute(state, makePacket(0x10, {2, 0}, 0x68));
    Require(state.context.at(0x10) == 17, "pop did not restore context");
    RequireRejected([&] { execute(state, makePacket(0x10, {2, 0}, 0x68)); }, "not been pushed");
    alignas(8) std::array<std::uint32_t, 4> arguments{7, 8, 9, 0};
    execute(state, makePacket(0x11, {1, low(arguments.data()), high(arguments.data())}, 2));
    auto packet = makePacket(0x16, {0, 0x8041});
    AgcDriver::Pm4::Validate(packet, 0);
    auto resolved = AgcDriver::Pm4::ResolveDispatch(packet, state);
    Require(resolved == std::array<std::uint32_t, 5>{0xc0031500, 7, 8, 9, 0x8041}, "base-relative dispatch arguments changed");
    packet = makePacket(0x16, {low(arguments.data()), high(arguments.data()), 0x41});
    AgcDriver::Pm4::Validate(packet, 0x20);
    Require(AgcDriver::Pm4::ResolveDispatch(packet, state)[3] == 9, "absolute indirect dispatch arguments changed");
    AgcDriver::Pm4::Validate(makePacket(0x15, {1, 1, 1, 0x2041}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x16, {0, 0xa041}), 0);
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x15, {1, 1, 1, 0x4041}), 0); }, "dispatch modifiers");
    execute(state, makePacket(0x13, {32}));
    execute(state, makePacket(0x26, {0x1000, 1}));
    execute(state, makePacket(0x2a, {1}));
    execute(state, makePacket(0x2f, {3}));
    Require(state.indexBufferSize == 32 && state.indexBase == 0x100001000ull && state.indexType == 1 && state.instanceCount == 3, "draw setup state lost");
    execute(state, makePacket(0x10, {0x00636261}, 0x2c));
    Require(state.markers.back() == "abc", "marker text lost");
    execute(state, makePacket(0x10, {0}, 0x30));
    execute(state, makePacket(0x10, {0}, 0x30));
    Require(state.markers.empty(), "an unbalanced marker pop changed the marker stack");
    execute(state, makePacket(0x10, {0}, 0x24));
    Require(state.shader.empty() && state.context == AgcDriver::InitialContextRegisters() && state.dispatchIndirectBase == 0 && state.indexBase == 0 && !state.savedContext, "dispatch reset retained state");
}};

const Case indexedDraw{"ResolveDraw_IndexedDraw_UsesIndexBufferState", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    alignas(4) std::array<std::uint32_t, 8> indices{};
    state.indexBase = reinterpret_cast<std::uintptr_t>(indices.data());
    state.instanceCount = 3;
    const auto packet = makePacket(0x35, {4, 2, 4, 0x20});
    for (std::uint32_t type = 0; type < 3; ++type) {
        state.indexType = type;
        const auto draw = AgcDriver::Pm4::ResolveDraw(packet, state);
        const auto size = type == 0 ? 2u : type == 1 ? 4u : 1u;
        Require(draw.indexAddress == state.indexBase + 2 * size && draw.indexSize == size && draw.indexCount == 4 && draw.instanceCount == 3 && draw.flags == 0x20, "indexed draw state mismatch");
    }
    state.indexType = 0;
    Require(AgcDriver::Pm4::ResolveDraw(packet, state).firstVertex == 0, "indexed draw invented a base vertex");
    state.userConfig[0x24a] = 0xd4d4;
    const auto offsetDraw = AgcDriver::Pm4::ResolveDraw(packet, state);
    Require(offsetDraw.indexed && offsetDraw.firstVertex == 0xd4d4, "DRAW_INDEX_OFFSET_2 ignored GE_INDX_OFFSET");
    const auto address = reinterpret_cast<std::uintptr_t>(indices.data());
    const auto explicitDraw = AgcDriver::Pm4::ResolveDraw(makePacket(0x27, {4, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 4, 0}), state);
    Require(explicitDraw.indexed && explicitDraw.firstVertex == 0xd4d4 && explicitDraw.indexAddress == address, "DRAW_INDEX_2 ignored GE_INDX_OFFSET");
    state.userConfig.erase(0x24a);
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "GE_INDX_OFFSET");
    state.userConfig[0x24a] = 0;
    RequireRejected([&] { AgcDriver::Pm4::Validate(packet, 0x20); }, "compute queue");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x35, {3, 0, 4, 0}), 0); }, "maximum index size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x35, {4, 0, 4, 1}), 0); }, "draw flags");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x35, {4, 0, 4}), 0); }, "packet size");
    state.indexType = 3;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "index type");
    state.indexType = 1;
    state.indexBase += 1;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "misaligned index base");
    state.indexBase = std::numeric_limits<std::uint64_t>::max() - 3;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "index address overflow");
    state.indexType = 2;
    state.indexBase = std::numeric_limits<std::uint64_t>::max() - 4;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "address range overflow");
    state.indexBase = 0x1000;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "guest");
}};

const Case autoDraw{"ResolveDraw_AutoDraw_IgnoresIndexBuffer", [] {
    RequireDefaultMode();
    Require(AgcDriver::Pm4::AccessesMemory(0xc0012d00u), "auto draw must synchronize guest memory");
    AgcDriver::QueueState state;
    state.instanceCount = 4;
    state.indexBase = 1;
    state.indexType = 0xffffffffu;
    state.userConfig[0x24a] = 7;
    for (const auto flags : {2u, 0x22u}) {
        const auto draw = AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {3, flags}), state);
        Require(!draw.indexed && draw.indexAddress == 0 && draw.indexSize == 0, "auto draw used the index buffer");
        Require(draw.indexCount == 3 && draw.instanceCount == 4 && draw.firstVertex == 7 && draw.firstInstance == 0 && draw.flags == (flags & 0x20u), "auto draw parameters mismatch");
    }
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}), 0x20); }, "compute queue");
    for (const auto flags : {0u, 1u, 3u, 0x20u, 0x42u}) {
        RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, flags}), 0); }, "auto draw flags");
    }
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2, 0}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}, 2), 0); }, "header flags");
    state.userConfig[0x24a] = std::numeric_limits<std::uint32_t>::max();
    Require(AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {1, 2}), state).firstVertex == std::numeric_limits<std::uint32_t>::max(), "last vertex rejected");
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {2, 2}), state); }, "vertex range overflow");
    Require(AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {0, 2}), state).indexCount == 0, "empty auto draw rejected");
    state.userConfig.erase(0x24a);
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x2d, {1, 2}), state); }, "GE_INDX_OFFSET");
}};

const Case indirectDraw{"ResolveDraw_IndirectDraw_DecodesArgumentRecords", [] {
    RequireDefaultMode();
    Require(AgcDriver::Pm4::AccessesMemory(0xc0032400u) && AgcDriver::Pm4::AccessesMemory(0xc0083800u), "indirect draws must synchronize guest memory");
    Require(AgcDriver::Pm4::UnsupportedReason(0xc0032400u).empty() && AgcDriver::Pm4::UnsupportedReason(0xc0082c00u).empty(), "indirect draws are rejected");
    AgcDriver::QueueState state;
    state.userConfig[0x24a] = 5;
    const auto packet = makePacket(0x24, {0, 0x280, 0x8e, 2});
    AgcDriver::Pm4::Validate(packet, 0);
    RequireRejected([&] { AgcDriver::Pm4::Validate(packet, 0x20); }, "compute queue");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x24, {2, 0x280, 0x8e, 2}), 0); }, "misaligned indirect draw offset");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x10280, 0x8e, 2}), 0); }, "start-index location");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x200, 0x8e, 2}), 0); }, "register location");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x280, 0x8e, 0}), 0); }, "initiator");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x25, {0, 0x280, 0x8e, 2}), 0); }, "initiator");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x24, {0, 0x280, 0x8e}), 0); }, "packet size");
    AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x8c, 0x280, 0x8d | (1u << 31u), 3, 0, 0, 16, 2}), 0);
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280 | (1u << 27u), 3, 0, 0, 16, 2}), 0); }, "control bits");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280, 3, 0, 0, 12, 2}), 0); }, "stride");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x38, {0, 0x280, 0x8e, 0x280, 3, 0, 0, 16, 0}), 0); }, "stride");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280 | (1u << 30u), 3, 0, 0, 16, 2}), 0); }, "count address");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x2c, {0, 0x280, 0x8e, 0x280, 3, 0x1000, 0, 16, 2}), 0); }, "count address");
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(packet, state); }, "base has not been set");
    execute(state, makePacket(0x11, {1, 0x5d87fc40, 0x10}));
    auto draw = AgcDriver::Pm4::ResolveDraw(packet, state);
    Require(draw.indirect.has_value() && draw.indirect->arguments == 0x105d87fc40ull && draw.indirect->opcode == 0x24 && draw.indirect->recordBytes == 16 && draw.indirect->stride == 16 && draw.indirect->count == 1 && !draw.indirect->countIndirect, "indirect draw arguments mismatch");
    Require(draw.indirect->baseVertexLocation == 0x280 && draw.indirect->startInstanceLocation == 0x8e && draw.indirect->drawIndexLocation == 0x280 && !draw.indirect->drawIndexEnabled && draw.indirect->indxOffset == 5 && draw.firstVertex == 5 && !draw.indexed && draw.indexCount == 0 && draw.instanceCount == 0, "indirect draw locations mismatch");
    Require(draw.indirect->RangeBytes() == 16 && draw.indirect->VertexDwordOffset() == 8 && draw.indirect->InstanceDwordOffset() == 12, "indirect draw record geometry mismatch");
    Require(AgcDriver::Pm4::ResolveDraw(makePacket(0x24, {0x60, 0x280, 0x8e, 2}), state).indirect->arguments == 0x105d87fca0ull, "indirect draw offset not applied");
    const auto multi = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0x20, 0x8c, 0x280, 0x8d | (1u << 31u), 3, 0, 0, 32, 0x22}), state);
    Require(multi.indirect->count == 3 && multi.indirect->stride == 32 && multi.indirect->drawIndexEnabled && multi.indirect->drawIndexLocation == 0x8d && !multi.indirect->countIndirect && multi.flags == 0x20 && multi.indirect->RangeBytes() == 80, "indirect multi-draw mismatch");
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state); }, "index base");
    alignas(4) std::array<std::uint16_t, 8> indices{};
    state.indexBase = reinterpret_cast<std::uintptr_t>(indices.data());
    state.indexType = 0;
    RequireRejected([&] { AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state); }, "INDEX_BUFFER_SIZE");
    state.indexBufferSize = 8;
    const auto indexed = AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state);
    Require(indexed.indexed && indexed.indexAddress == state.indexBase && indexed.indexCount == 8 && indexed.indexSize == 2 && indexed.indirect->recordBytes == 20 && indexed.indirect->stride == 20 && indexed.firstVertex == 0 && indexed.indirect->indxOffset == 0, "indexed indirect draw mismatch");
    Require(indexed.indirect->VertexDwordOffset() == 12 && indexed.indirect->InstanceDwordOffset() == 16, "indexed record geometry mismatch");
    alignas(16) std::array<std::uint32_t, 10> records{3, 2, 7, 9, 4, 1, 5, 6, 0, 0};
    state.drawIndirectBase = reinterpret_cast<std::uintptr_t>(records.data());
    const auto local = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0, 0x280, 0x280, 0x280, 2, 0, 0, 16, 2}), state);
    const auto first = AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 0);
    const auto second = AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 1);
    Require(first.count == 3 && first.instances == 2 && first.firstVertexOrIndex == 7 && first.vertexOffset == 0 && first.firstInstance == 9, "non-indexed record layout mismatch");
    Require(second.count == 4 && second.instances == 1 && second.firstVertexOrIndex == 5 && second.firstInstance == 6, "second record mismatch");
    RequireRejected([&] { AgcDriver::Pm4::ReadDrawArguments(*local.indirect, 2); }, "record index");
    records = {3, 2, 7, 9, 11, 0, 0, 0, 0, 0};
    const auto indexedRecord = AgcDriver::Pm4::ReadDrawArguments(*AgcDriver::Pm4::ResolveDraw(makePacket(0x25, {0, 0x8c, 0x280, 0}), state).indirect, 0);
    Require(indexedRecord.count == 3 && indexedRecord.instances == 2 && indexedRecord.firstVertexOrIndex == 7 && indexedRecord.vertexOffset == 9 && indexedRecord.firstInstance == 11, "indexed record layout mismatch");
    alignas(4) std::uint32_t countValue = 2;
    const auto counted = AgcDriver::Pm4::ResolveDraw(makePacket(0x2c, {0, 0x280, 0x280, 0x280 | (1u << 30u), 5, low(&countValue), high(&countValue), 16, 2}), state);
    Require(counted.indirect->countIndirect && counted.indirect->count == 5 && counted.indirect->countAddress == reinterpret_cast<std::uintptr_t>(&countValue) && AgcDriver::Pm4::ReadDrawCount(*counted.indirect) == 2, "indirect draw count mismatch");
    RequireRejected([&] { AgcDriver::Pm4::ReadDrawCount(*local.indirect); }, "count address");
}};

const Case memory{"Execute_MemoryWrites_StoreAndRejectInvalidForms", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    std::array<std::uint32_t, 4> data{0, 0, 0, 0};
    execute(state, makePacket(0x37, {0x100, low(data.data()), high(data.data()), 11, 12}));
    Require(data[0] == 11 && data[1] == 12, "WRITE_DATA increment failed");
    execute(state, makePacket(0x37, {0x10100, low(data.data()), high(data.data()), 21, 22}));
    Require(data[0] == 22 && data[1] == 12, "WRITE_DATA fixed destination failed");
    execute(state, makePacket(0x37, {0x40000100, low(data.data()), high(data.data()), 41, 42}));
    Require(data[0] == 41 && data[1] == 42, "WRITE_DATA from the PFP failed");
    execute(state, makePacket(0x37, {0x04100200, low(data.data()), high(data.data()), 61, 62}));
    Require(data[0] == 61 && data[1] == 62, "WRITE_DATA with a cache policy failed");
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x37, {0x08000100, low(data.data()), high(data.data()), 71}), 0); }, "reserved");
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x37, {0x80000100, low(data.data()), high(data.data()), 51}), 0); }, "engine");
    execute(state, makePacket(0x81, {4, 31, 32}));
    execute(state, makePacket(0x83, {4, 2, low(data.data()), high(data.data())}));
    Require(data[0] == 31 && data[1] == 32, "constant RAM round trip failed");
    RequireRejected([&] { execute(state, makePacket(0x81, {0xbffc, 1, 2})); }, "overflow");
    RequireRejected([&] { execute(state, makePacket(0x40, {0x10105, 0, 0, low(data.data()), high(data.data())})); }, "64-bit immediate");
}};

const Case copies{"Execute_CopyAndDmaPackets_MoveData", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    alignas(8) std::array<std::uint32_t, 4> source{11, 12, 13, 14};
    alignas(8) std::array<std::uint32_t, 4> destination{};
    execute(state, makePacket(0x40, {0x10101, low(source.data()), high(source.data()), low(destination.data()), high(destination.data())}));
    Require(destination[0] == 11 && destination[1] == 12 && destination[2] == 0, "64-bit COPY_DATA failed");
    execute(state, makePacket(0x40, {0x105, 0x12345678, 0, low(destination.data()), high(destination.data())}));
    Require(destination[0] == 0x12345678, "immediate COPY_DATA failed");
    alignas(8) std::array<std::uint64_t, 2> clock{};
    const auto clockCopy = [&](std::uint64_t* target) { return makePacket(0x40, {0x06016209, 0, 0, low(target), high(target)}); };
    execute(state, clockCopy(&clock[0]));
    execute(state, clockCopy(&clock[1]));
    Require(clock[0] != 0 && clock[1] >= clock[0], "GPU clock COPY_DATA failed");
    alignas(8) std::array<std::uint32_t, 2> clock32{0, 0xdeadbeef};
    execute(state, makePacket(0x40, {0x06006209, 0, 0, low(clock32.data()), high(clock32.data())}));
    Require(clock32[0] != 0 && clock32[1] == 0xdeadbeef, "32-bit GPU clock COPY_DATA did not write only the low half");
    const auto clockStore =AgcDriver::Pm4::ResolveStore(clockCopy(&clock[0]), state, 64);
    Require(clockStore.has_value() && clockStore->Bytes().size() == 8, "GPU clock COPY_DATA did not resolve as an 8-byte store");
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x40, {0x1020a, 0, 0, low(&clock[0]), high(&clock[0])}), 0); }, "reference-clock");
    execute(state, makePacket(0x50, {0x60000000, low(source.data()), high(source.data()), low(destination.data()), high(destination.data()), 16}));
    Require(source == destination, "DMA_DATA copy failed");
    execute(state, makePacket(0x50, {0x40000000, 0x44332211, 0, low(destination.data()), high(destination.data()), 6}));
    Require(destination[0] == 0x44332211 && destination[1] == 0x00002211, "DMA_DATA byte fill failed");
    constexpr std::uint32_t cachePolicies = (1u << 13u) | (2u << 25u);
    std::size_t flexibleBefore = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&flexibleBefore) == 0, "cannot query flexible memory");
    const auto toGds = makePacket(0x50, {0x60100000 | cachePolicies, low(source.data()), high(source.data()), 0x100, 0, 16});
    Require(!AgcDriver::Pm4::ResolveStore(toGds, state, 64).has_value(), "DMA_DATA to GDS resolved as a memory store");
    execute(state, toGds);
    std::size_t flexibleAfter = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&flexibleAfter) == 0 && flexibleAfter == flexibleBefore, "the GDS was charged to the flexible memory budget");
    execute(state, makePacket(0x50, {0x20100000, 0x104, 0, 0xfff8, 0, 8}));
    destination = {};
    execute(state, makePacket(0x50, {0x20000000 | cachePolicies, 0xfff8, 0, low(destination.data()), high(destination.data()), 8}));
    Require(destination[0] == 12 && destination[1] == 13 && destination[2] == 0, "DMA_DATA GDS to GDS round trip failed");
    const auto fromGds = makePacket(0x50, {0x20000000, 0x100, 0, low(destination.data()), high(destination.data()), 16});
    const auto store = AgcDriver::Pm4::ResolveStore(fromGds, state, 64);
    Require(store.has_value() && store->Bytes().size() == 16 && std::memcmp(store->Bytes().data(), source.data(), 16) == 0, "DMA_DATA from GDS did not resolve its source bytes");
    destination = {};
    execute(state, fromGds);
    Require(source == destination, "DMA_DATA GDS round trip failed");
    RequireRejected([&] { execute(state, makePacket(0x50, {0x60100000, low(source.data()), high(source.data()), 0xfffc, 0, 8})); }, "exceeds the GDS");
    RequireRejected([&] { execute(state, makePacket(0x50, {0x20000000, 0, 1, low(destination.data()), high(destination.data()), 4})); }, "exceeds the GDS");
    destination = {};
    const auto prefetch = makePacket(0x50, {0x60200000, low(source.data()), high(source.data()), low(source.data()), high(source.data()), 0x80000010});
    const auto prefetchStore = AgcDriver::Pm4::ResolveStore(prefetch, state, 64);
    Require(prefetchStore.has_value() && prefetchStore->Bytes().empty(), "a DMA_DATA prefetch resolved as a memory store");
    Require(!AgcDriver::Pm4::DecodeMemoryCopy(prefetch).has_value(), "a DMA_DATA prefetch decoded as a copy");
    execute(state, prefetch);
    Require(source[0] == 11 && source[1] == 12 && destination[0] == 0, "a DMA_DATA prefetch wrote memory");
    RequireRejected([&] { execute(state, makePacket(0x50, {0x40200000, 1, 0, 0, 0, 4})); }, "prefetch of a register");
    RequireRejected([&] { execute(state, makePacket(0x50, {0x60200000, low(source.data()), high(source.data()), 0, 0, 4 | (1u << 27u)})); }, "register destination");
    RequireRejected([&] { execute(state, makePacket(0x50, {0x60000000 | (1u << 15u), low(source.data()), high(source.data()), low(destination.data()), high(destination.data()), 4})); }, "reserved fields");
    RequireRejected([&] { execute(state, makePacket(0x37, {0x100, 0x1000, 0, 1})); }, "guest");
#ifdef _WIN32
    const ReadOnlyPage page;
    RequireRejected([&] { execute(state, makePacket(0x37, {0x100, low(page.Address()), high(page.Address()), 1})); }, "write permission");
#endif
}};

const Case memoryCopyDecode{"DecodeMemoryCopy_DmaDataVariants_DecodeOnlyPlainCopies", [] {
    RequireDefaultMode();
    using AgcDriver::Pm4::DecodeMemoryCopy;
    constexpr std::uint64_t source = 0x1120000000ull, destination = 0x403d7b400ull;
    const auto packet = [&](std::uint32_t control, std::uint64_t from, std::uint64_t to, std::uint32_t command) {
        return makePacket(0x50, {control, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(from >> 32u), static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(to >> 32u), command});
    };
    const auto copy = DecodeMemoryCopy(packet(0x60000000, source, destination, 0xbdd800));
    Require(copy.has_value() && copy->source == source && copy->destination == destination && copy->bytes == 0xbdd800, "a memory-to-memory DMA_DATA did not decode as a copy");
    Require(DecodeMemoryCopy(packet(0x00000000, source, destination, 64)).has_value(), "a DMA_DATA with memory selectors 0 did not decode as a copy");
    Require(!DecodeMemoryCopy(packet(0x40000000, 0x44332211, destination, 64)).has_value(), "an immediate fill decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60100000, source, 0x100, 64)).has_value(), "a DMA_DATA to the GDS decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x20000000, 0x100, destination, 64)).has_value(), "a DMA_DATA from the GDS decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 26u))).has_value(), "a register source decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 27u))).has_value(), "a register destination decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 28u))).has_value(), "a non-incrementing source decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, destination, 64 | (1u << 29u))).has_value(), "a non-incrementing destination decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, destination, 0)).has_value(), "an empty DMA_DATA decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source, source + 32, 64)).has_value(), "overlapping ranges decoded as a copy");
    Require(!DecodeMemoryCopy(packet(0x60000000, source + 32, source, 64)).has_value(), "overlapping ranges below the source decoded as a copy");
    Require(DecodeMemoryCopy(packet(0x60000000, source, source + 64, 64)).has_value(), "adjacent ranges did not decode as a copy");
    Require(!DecodeMemoryCopy(makePacket(0x40, {0x10101, 0, 0, 0, 0})).has_value(), "a COPY_DATA decoded as a DMA_DATA copy");
}};

const Case memorySynchronization{"Execute_MemoryTransfers_SynchronizeGuestRanges", [] {
    RequireDefaultMode();
    struct MemoryState {
        std::uint32_t source = 0;
        std::uint32_t destination = 0;
        bool read = false;
        bool written = false;
        bool unrelatedRange = false;
        bool unrelatedSource = false;
    };
    static MemoryState memory;
    memory = {};
    AgcDriver::QueueState state;
    const FlushHookReset reset;
    const auto resolve = [](std::uint64_t address, std::size_t bytes) {
        if (bytes != sizeof(std::uint32_t)) memory.unrelatedRange = true;
        if (address == reinterpret_cast<std::uintptr_t>(&memory.destination)) {
            memory.written = true;
        } else if (address == reinterpret_cast<std::uintptr_t>(&memory.source)) {
            memory.source = 42;
            memory.read = true;
        } else {
            memory.unrelatedSource = true;
        }
    };
    const auto requireRelatedRanges = [](std::string_view packet) {
        Require(!memory.unrelatedRange, std::string(packet) + " resolved an unrelated range");
        Require(!memory.unrelatedSource, std::string(packet) + " resolved an unrelated source");
    };
    AgcDriver::GuestMemory::SetFlushHook(resolve);
    execute(state, makePacket(0x37, {0x100, low(&memory.destination), high(&memory.destination), 17}));
    requireRelatedRanges("WRITE_DATA");
    Require(memory.written && !memory.read && memory.destination == 17, "WRITE_DATA did not synchronize its destination");
    for (const auto opcode : {0x40u, 0x50u}) {
        memory = {};
        const auto packet = opcode == 0x40
            ? makePacket(opcode, {0x101, low(&memory.source), high(&memory.source), low(&memory.destination), high(&memory.destination)})
            : makePacket(opcode, {0x60000000, low(&memory.source), high(&memory.source), low(&memory.destination), high(&memory.destination), 4});
        execute(state, packet);
        const auto name = opcode == 0x40 ? "COPY_DATA" : "DMA_DATA";
        requireRelatedRanges(name);
        Require(memory.read && memory.written && memory.destination == 42, std::string(name) + " used stale data before range synchronization");
    }
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        throw std::runtime_error("range synchronization failed");
    });
    RequireRejected([&] { execute(state, makePacket(0x37, {0x100, low(&memory.destination), high(&memory.destination), 99})); }, "range synchronization failed");
    Require(memory.destination == 42, "failed synchronization changed the destination");
}};

const Case conditionalValidation{"Validate_CondExec_RejectsReservedFields", [] {
    RequireDefaultMode();
    alignas(4) static std::uint32_t condition = 0;
    const auto valid = makePacket(0x22, {low(&condition), high(&condition), 0, 0x3fff});
    Require(AgcDriver::Pm4::UnsupportedReason(valid[0]).empty() && AgcDriver::Pm4::AccessesMemory(valid[0]), "COND_EXEC is rejected or does not synchronize guest memory");
    AgcDriver::Pm4::Validate(valid, 0);
    AgcDriver::Pm4::Validate(valid, 0x20);
    Require(AgcDriver::Pm4::ConditionalWords(valid) == 0x3fff, "COND_EXEC count decoded wrong");
    Require(AgcDriver::Pm4::ConditionalWords(makePacket(0x22, {low(&condition), high(&condition), 0, 0})) == 0, "empty COND_EXEC range decoded wrong");
    AgcDriver::Pm4::Validate(makePacket(0x22, {low(&condition), high(&condition), 3u << 25u, 5}), 0x20);
    const auto invalidWord = [&](std::size_t word, std::uint32_t value, std::uint32_t queue, const char* text) {
        auto packet = valid;
        packet[word] = value;
        RequireRejected([&] { AgcDriver::Pm4::Validate(packet, queue); }, text);
    };
    invalidWord(1, low(&condition) | 1u, 0, "reserved address bits");
    invalidWord(1, low(&condition) | 2u, 0x20, "reserved address bits");
    invalidWord(2, 0x10000u, 0, "above 48");
    invalidWord(3, 3u << 25u, 0, "reserved control fields");
    invalidWord(3, 1u, 0x20, "reserved control fields");
    invalidWord(3, 1u << 27u, 0x20, "reserved control fields");
    invalidWord(4, 0x4000u, 0, "reserved count bits");
    invalidWord(4, 0x80000005u, 0x20, "reserved count bits");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0, 0}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0}, 1), 0); }, "header flags");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x22, {0, 0, 0, 0}, 2), 0x20); }, "header flags");
    RequireRejected([] { AgcDriver::Pm4::ConditionalWords(makePacket(0x37, {0x100, 0, 0, 0})); }, "expected COND_EXEC");
    AgcDriver::QueueState state;
    RequireRejected([&] { AgcDriver::Pm4::Execute(valid, state); }, "driver execution");
}};

const Case conditionReadSynchronization{"ReadCondition_CondExec_SynchronizesTheCondition", [] {
    RequireDefaultMode();
    alignas(4) static std::uint32_t condition = 0;
    static std::size_t flushed = 0;
    static bool unrelated = false;
    condition = 0;
    flushed = 0;
    unrelated = false;
    const FlushHookReset reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        if (address != reinterpret_cast<std::uintptr_t>(&condition) || bytes != sizeof(condition)) unrelated = true;
        condition = 0x80;
        ++flushed;
    });
    const auto packet = makePacket(0x22, {low(&condition), high(&condition), 0, 5});
    const auto value = AgcDriver::Pm4::ReadCondition(packet);
    Require(!unrelated, "COND_EXEC synchronized an unrelated range");
    Require(value == 0x80 && flushed == 1, "COND_EXEC read its condition before the GPU work that writes it");
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        throw std::runtime_error("condition synchronization failed");
    });
    RequireRejected([&] { AgcDriver::Pm4::ReadCondition(packet); }, "condition synchronization failed");
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    RequireRejected([] { AgcDriver::Pm4::ReadCondition(makePacket(0x22, {0x1000, 0, 0, 5})); }, "guest");
}};

const Case eventWrite{"Validate_EventWrite_ChecksEventIndexAndQueue", [] {
    RequireDefaultMode();
    for (const auto eventType : {0x07u, 0x0fu, 0x10u}) {
        AgcDriver::Pm4::Validate(makePacket(0x46, {0x400u | eventType}), 0);
        for (std::uint32_t index = 0; index < 8; ++index) {
            if (index == 4) continue;
            RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0); }, "partial-flush event index");
        }
        if (eventType == 0x07) {
            AgcDriver::Pm4::Validate(makePacket(0x46, {0x407}), 0x20);
        } else {
            RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x400u | eventType}), 0x20); }, "compute queue");
        }
    }
    for (const auto eventType : {0x16u, 0x31u, 0x2au, 0x2cu, 0x2eu}) {
        for (const auto index : {0u, 7u}) {
            AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0);
        }
        for (std::uint32_t index = 1; index < 7; ++index) {
            RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {(index << 8u) | eventType}), 0); }, "cache-flush event index");
        }
        RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {eventType}), 0x20); }, "compute queue");
    }
    AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000, 0x2}), 0);
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000, 0x2}), 0x20); }, "compute queue");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x039, 0x1000, 0x2}), 0); }, "counter dump event index");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1004, 0x2}), 0); }, "misaligned occlusion counter");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0, 0}), 0); }, "null or misaligned occlusion counter");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x139, 0x1000}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x138, 0x1000, 0x2}), 0); }, "event type 56");
    for (const auto bit : {0x40u, 0x80u, 0x800u, 0x80000000u}) {
        RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410u | bit}), 0); }, "reserved bits");
    }
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410}, 2), 0); }, "header flags");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x410, 0, 0}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x13a, 0, 0}), 0); }, "event type 58");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x46, {0x0d}), 0); }, "event type 13");
}};

const Case acquireMem{"Validate_AcquireMem_ChecksCacheFlagsAndRanges", [] {
    RequireDefaultMode();
    const auto captured = makePacket(0x58, {0x02007fc0, 0, 0, 0, 0, 10, 0x200});
    AgcDriver::Pm4::Validate(captured, 0);
    Require(AgcDriver::Pm4::UsesGpuCacheBarrier(captured), "L1 acquire must preserve GPU render targets");
    for (const auto flags : {0u, 0x200u, 0x3ffu, 0x10200u, 0x20200u}) {
        auto packet = captured;
        packet[7] = flags;
        Require(AgcDriver::Pm4::UsesGpuCacheBarrier(packet), "GPU cache acquire requires an unnecessary host writeback");
    }
    for (const auto flags : {0x400u, 0x800u, 0x1000u, 0x4000u, 0x8000u}) {
        auto packet = captured;
        packet[7] = flags;
        Require(!AgcDriver::Pm4::UsesGpuCacheBarrier(packet), "L2 acquire lost host synchronization");
    }
    Require(!AgcDriver::Pm4::UsesGpuCacheBarrier(makePacket(0x58, {0x00800000, 0xffffffff, 0, 0, 0, 10})), "legacy acquire lost host synchronization");
    RequireRejected([] { AgcDriver::Pm4::UsesGpuCacheBarrier({}); }, "requires ACQUIRE_MEM");
    RequireRejected([] { AgcDriver::Pm4::UsesGpuCacheBarrier(makePacket(0x58, {0, 0, 0, 0, 0, 0, 0x2000})); }, "cache discard");
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x82007fc0, 1, 0, 0xffffffff, 0, 0xffff, 0x200}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x80000000, 0, 0, 0, 0, 10, 0x200}), 0x20);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x00800000, 0xffffffff, 0, 0, 0, 10}), 0);
    AgcDriver::Pm4::Validate(makePacket(0x58, {0x80800000, 16, 0, 0x1000, 0, 0}), 0x20);
    RequireRejected([&] { AgcDriver::Pm4::Validate(captured, 0x20); }, "compute queue");
    const auto invalidWord = [&](std::size_t index, std::uint32_t value, const char* reason) {
        auto packet = captured;
        packet[index] = value;
        RequireRejected([&] { AgcDriver::Pm4::Validate(packet, 0); }, reason);
    };
    invalidWord(0, captured[0] | 2u, "header flags");
    invalidWord(1, 4, "control flags");
    invalidWord(1, 0x00800000, "control flags");
    invalidWord(3, 1, "above 40 bits");
    invalidWord(5, 1, "above 40 bits");
    invalidWord(6, 0x10000, "poll interval");
    invalidWord(7, 0x40000, "GCR flags");
    invalidWord(7, 0x2000, "cache discard");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 2, 0, 0xffffffff, 0, 0, 0}), 0); }, "range exceeds");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0}), 0); }, "packet size");
    RequireRejected([] { AgcDriver::Pm4::Validate(makePacket(0x58, {0, 0, 0, 0, 0, 0, 0, 0}), 0); }, "packet size");
}};

const Case conditionalWrite{"Execute_CondWrite_ComparesTheMaskedPollValue", [] {
    RequireDefaultMode();
    AgcDriver::QueueState state;
    alignas(4) std::uint32_t poll = 0x1234u;
    alignas(4) std::uint32_t target = 0;
    const auto valid = condWrite(0x113, poll, 0x34, 0xff, target, 7);
    Require(AgcDriver::Pm4::Name(valid[0]) == "COND_WRITE" && valid[0] == 0xc0074500u, "COND_WRITE header or name mismatch");
    Require(AgcDriver::Pm4::UnsupportedReason(valid[0]).empty() && AgcDriver::Pm4::AccessesMemory(valid[0]), "COND_WRITE is rejected or does not synchronize guest memory");

    const std::array<std::pair<std::uint32_t, bool>, 7> functions{{{0, true}, {1, false}, {2, true}, {3, true}, {4, false}, {5, true}, {6, false}}};
    for (const auto& [function, writes] : functions) {
        target = 0;
        execute(state, condWrite(0x110 | function, poll, 0x34, 0xff, target, 9));
        Require(target == (writes ? 9u : 0u), "COND_WRITE compared the masked poll value incorrectly");
    }
    target = 0;
    execute(state, condWrite(0x111, poll, 0x35, 0xff, target, 10));
    Require(target == 10, "COND_WRITE less-than did not write");
    target = 0;
    execute(state, condWrite(0x116, poll, 0x1233, 0xffffffffu, target, 11));
    Require(target == 11, "COND_WRITE greater-than with a full mask did not write");

    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x45, {0x113, low(&poll), high(&poll), 0, 0, low(&target), high(&target)}), 0); }, "packet size");
    RequireRejected([&] { AgcDriver::Pm4::Validate(condWrite(0x2000113, poll, 0, 0, target, 0), 0); }, "reserved");
    RequireRejected([&] { AgcDriver::Pm4::Validate(condWrite(0x103, poll, 0, 0, target, 0), 0); }, "register-space");
    RequireRejected([&] { AgcDriver::Pm4::Validate(condWrite(0x117, poll, 0, 0, target, 0), 0); }, "compare function");
    RequireRejected([&] { AgcDriver::Pm4::Validate(condWrite(0x013, poll, 0, 0, target, 0), 0); }, "destination");
    RequireRejected([&] { AgcDriver::Pm4::Validate(condWrite(0x213, poll, 0, 0, target, 0), 0); }, "destination");
    auto misaligned = condWrite(0x113, poll, 0, 0, target, 0);
    misaligned[2] += 2;
    RequireRejected([&] { AgcDriver::Pm4::Validate(misaligned, 0); }, "misaligned");
    misaligned = condWrite(0x113, poll, 0, 0, target, 0);
    misaligned[6] += 1;
    RequireRejected([&] { AgcDriver::Pm4::Validate(misaligned, 0); }, "misaligned");
}};

const Case conditionalWriteSynchronization{"Execute_CondWrite_SynchronizesPollAndTarget", [] {
    RequireDefaultMode();
    struct MemoryState {
        std::uint32_t poll = 0;
        std::uint32_t target = 0;
        bool read = false;
        bool written = false;
        bool unrelatedRange = false;
        bool unrelatedPoll = false;
    };
    static MemoryState memory;
    memory = {};
    AgcDriver::QueueState state;
    const FlushHookReset reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        if (bytes != sizeof(std::uint32_t)) memory.unrelatedRange = true;
        if (address == reinterpret_cast<std::uintptr_t>(&memory.target)) {
            memory.written = true;
        } else if (address == reinterpret_cast<std::uintptr_t>(&memory.poll)) {
            memory.poll = 5;
            memory.read = true;
        } else {
            memory.unrelatedPoll = true;
        }
    });
    execute(state, condWrite(0x113, memory.poll, 5, 0xffffffffu, memory.target, 12));
    Require(!memory.unrelatedRange, "COND_WRITE resolved an unrelated range");
    Require(!memory.unrelatedPoll, "COND_WRITE resolved an unrelated poll address");
    Require(memory.read && memory.written && memory.target == 12, "COND_WRITE used a stale poll value or skipped synchronizing its target");
}};

const Case conditionalWriteSubmission{"Submit_CondWrite_ReadsPollAfterEarlierPackets", [] {
    RequireDefaultMode();
    alignas(4) static std::uint32_t poll = 0;
    static std::array<std::uint32_t, 2> results{};
    results.fill(0);
    poll = 0;
    auto words = joinPackets({writeWord(poll, 5), condWrite(0x113, poll, 5, 0xffffffffu, results[0], 51), condWrite(0x114, poll, 5, 0xffffffffu, results[1], 52)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    Require(results[0] == 51, "COND_WRITE read its poll value before an earlier packet of its queue stored it");
    Require(results[1] == 0, "COND_WRITE wrote although its comparison failed");
}};

const Case predication{"Execute_SetPredication_GatesPackets", [] {
    RequireDefaultMode();
    alignas(16) std::uint64_t flag[4] = {0, 0, 0, 0};
    auto* flag32 = reinterpret_cast<std::uint32_t*>(&flag[2]);
    const auto setPredication = [&](std::uint32_t operation, bool executeWhenSet, const void* address) {
        return makePacket(0x20, {(operation << 16u) | (executeWhenSet ? 0x100u : 0u) | 0x1000u, low(address), high(address)});
    };
    AgcDriver::QueueState state;
    Require(AgcDriver::Pm4::PredicationPasses(state), "inactive predication must pass");
    execute(state, setPredication(3, true, &flag[0]));
    Require(state.predication.operation == 3 && state.predication.executeWhenSet && state.predication.address == reinterpret_cast<std::uintptr_t>(&flag[0]), "BOOL64 predication state mismatch");
    Require(!AgcDriver::Pm4::PredicationPasses(state), "zero BOOL64 value must skip draw-visible packets");
    flag[0] = 1ull << 40u;
    Require(AgcDriver::Pm4::PredicationPasses(state), "upper BOOL64 bits must count");
    execute(state, setPredication(3, false, &flag[0]));
    Require(!AgcDriver::Pm4::PredicationPasses(state), "non-zero BOOL64 value must skip draw-not-visible packets");
    flag[0] = 0;
    Require(AgcDriver::Pm4::PredicationPasses(state), "zero BOOL64 value must run draw-not-visible packets");
    execute(state, setPredication(4, true, &flag[2]));
    flag[2] = 0xffffffff00000000ull;
    Require(*flag32 == 0 && !AgcDriver::Pm4::PredicationPasses(state), "BOOL32 must read only 32 bits");
    *flag32 = 7;
    Require(AgcDriver::Pm4::PredicationPasses(state), "non-zero BOOL32 value must run draw-visible packets");
    execute(state, setPredication(0, false, nullptr));
    Require(state.predication.operation == 0 && AgcDriver::Pm4::PredicationPasses(state), "clear must end predication");

    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(1, true, &flag[0]), 0); }, "query predication");
    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(2, true, &flag[0]), 0); }, "query predication");
    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(5, true, &flag[0]), 0); }, "invalid predication operation");
    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(3, true, &flag[0]), 0x20); }, "compute queue");
    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(3, true, nullptr), 0); }, "unaligned predication address");
    RequireRejected([&] { AgcDriver::Pm4::Validate(setPredication(3, true, reinterpret_cast<const std::uint8_t*>(&flag[0]) + 8), 0); }, "unaligned predication address");
    auto continued = setPredication(3, true, &flag[0]);
    continued[1] |= 1u << 31u;
    RequireRejected([&] { AgcDriver::Pm4::Validate(continued, 0); }, "SET_PREDICATION bits");
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x20, {3u << 16u, low(&flag[0]), high(&flag[0])}, 1), 0); }, "header flags");

    AgcDriver::Pm4::Validate(makePacket(0x2d, {3, 2}, 1), 0);
    AgcDriver::Pm4::Validate(makePacket(0x46, {0x410}, 1), 0);
    const auto call = makePacket(0x3f, {0x1000, 0, 0x0f200010}, 1);
    AgcDriver::Pm4::Validate(call, 0);
    RequireRejected([&] { AgcDriver::Pm4::Validate(makePacket(0x3f, {0x1000, 0, 0x0f200010}), 0); }, "nested command buffers");
    RequireRejected([&] { AgcDriver::Pm4::Execute(call, state); }, "nested command buffers");
}};

const Case unwrittenUserData{"ReadUserData_UnwrittenRegister_ReadsZero", [] {
    RequireDefaultMode();
    AgcDriver::Registers shader{{0x8c, 0x100}, {0x8d, 0}, {0x240, 0x200}};
    Require(AgcDriver::DriverDetail::readUserData(shader, 0x8c) == 0x100 && AgcDriver::DriverDetail::readUserData(shader, 0x240) == 0x200, "a written user data register was not read");
    Require(AgcDriver::DriverDetail::readUserData(shader, 0x8d) == 0, "a user data register written as zero was not read");
    Require(AgcDriver::DriverDetail::readUserData(shader, 0x95) == 0 && AgcDriver::DriverDetail::readUserData(shader, 0x241) == 0, "an unwritten user data register does not read zero");
    RequireRejected([&] { static_cast<void>(AgcDriver::DriverDetail::readRegister(shader, 0x95)); }, "required shader register");
}};

const Case driverSubmission{"Submit_Pm4CommandBuffer_ExecutesOrRejectsAtomically", [] {
    RequireDefaultMode();
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
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "PM4 submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(destination[0] == 83, "worker did not execute PM4 memory operations");
    auto rejectedCommands = commands;
    const auto unsupportedEvent = makePacket(0x46, {0x0d});
    rejectedCommands.insert(rejectedCommands.end(), unsupportedEvent.begin(), unsupportedEvent.end());
    Packet rejectedPacket{rejectedCommands.data(), static_cast<std::uint32_t>(rejectedCommands.size()), 0, {}};
    destination[0] = 0;
    RequireRejected([&] { sceAgcDriverSubmitDcb(&rejectedPacket); }, "EVENT_WRITE at DWORD");
    AgcDriverWaitIdle_nid_postfix();
    Require(destination[0] == 0, "rejected event submission executed a prefix");
    destination[0] = 0;
    const auto emptyDraw = makePacket(0x2d, {0, 2});
    commands.insert(commands.end(), emptyDraw.begin(), emptyDraw.end());
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "auto draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(destination[0] == 83, "empty auto draw prevented command execution");
    destination[0] = 0;
    const auto draw = makePacket(0x2d, {3, 3});
    commands.insert(commands.end(), draw.begin(), draw.end());
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    RequireRejected([&] { sceAgcDriverSubmitDcb(&packet); }, "DRAW_INDEX_AUTO at DWORD");
    AgcDriverWaitIdle_nid_postfix();
    Require(destination[0] == 0, "rejected submission executed a prefix");
}};

const Case registerListsReadAtSubmission{"Submit_RegisterList_IsReadAtSubmission", [] {
    RequireDefaultMode();
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
    Require(std::atomic_ref<std::uint32_t>(done).load() == 1, "register list rewritten after submission was read by the worker");
}};

const Case predicatedSubmission{"Submit_PredicatedPackets_RunOnlyWhenThePredicatePasses", [] {
    RequireDefaultMode();
    alignas(16) std::uint64_t flag[2] = {0, 0};
    alignas(16) std::array<std::uint32_t, 4> written{};
    alignas(16) std::array<std::uint32_t, 1> nestedWritten{};
    const auto nested = makePacket(0x37, {0x100, low(nestedWritten.data()), high(nestedWritten.data()), 3});
    const auto call = makePacket(0x3f, {low(nested.data()), high(nested.data()), 0x0f200000u | static_cast<std::uint32_t>(nested.size())}, 1);
    const auto submit = [](std::vector<std::uint32_t>& commands) {
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "predicated submission failed");
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
    Require(written[0] == 0, "packet with a false predicate was executed");
    Require(nestedWritten[0] == 0, "command buffer with a false predicate was executed");
    Require(written[1] == 9, "cleared predication skipped a packet");
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
    Require(written[2] == 5, "packet with a true predicate was skipped");
    Require(nestedWritten[0] == 3, "command buffer with a true predicate was skipped");
    Require(written[3] == 0, "draw-not-visible packet ran with a set value");
    auto chain = makePacket(0x3f, {low(nested.data()), high(nested.data()), 0x0f300000u | static_cast<std::uint32_t>(nested.size())}, 1);
    Packet chained{chain.data(), static_cast<std::uint32_t>(chain.size()), 0, {}};
    RequireRejected([&] { sceAgcDriverSubmitDcb(&chained); }, "predicated command buffer chains");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case conditionalSubmission{"Submit_CondExec_SkipsExactlyItsRange", [] {
    RequireDefaultMode();
    alignas(8) static std::uint32_t zero = 0, one = 1, condition = 0;
    static std::array<std::uint32_t, 16> results{};
    results.fill(0);
    const auto run = [](std::vector<std::uint32_t> words, std::uint32_t queue = 0) {
        submitWords(words, queue);
        AgcDriverWaitIdle_nid_postfix();
    };
    run(joinPackets({conditional(one, 5), writeWord(results[0], 11), writeWord(results[1], 12)}));
    Require(results[0] == 11 && results[1] == 12, "COND_EXEC skipped a range whose condition is set");
    run(joinPackets({conditional(zero, 6), {0x80000000u}, writeWord(results[0], 21), writeWord(results[1], 22)}));
    Require(results[0] == 11 && results[1] == 22, "COND_EXEC did not skip exactly its range when the condition is zero");
    run(joinPackets({conditional(zero, 0), writeWord(results[2], 23)}));
    Require(results[2] == 23, "an empty COND_EXEC range skipped the next packet");
    alignas(4) static std::uint32_t never = 0;
    never = 0;
    auto skippedWait = joinPackets({conditional(zero, 9), makePacket(0x3c, {0x13, low(&never), high(&never), 1, 0xffffffffu, 0x190}), makePacket(0x10, {0}, 0x30), writeWord(results[14], 24)});
    submitWords(skippedWait);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(results[14]).load() != 24 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool skipped = std::atomic_ref<std::uint32_t>(results[14]).load() == 24;
    std::atomic_ref<std::uint32_t>(never).store(1);
    AgcDriverWaitIdle_nid_postfix();
    Require(skipped, "a WAIT_REG_MEM inside a skipped COND_EXEC range was executed");

    condition = 1;
    run(joinPackets({writeWord(condition, 0), conditional(condition, 5), writeWord(results[0], 31), writeWord(results[1], 32)}));
    Require(condition == 0 && results[0] == 11 && results[1] == 32, "COND_EXEC read its condition before an earlier packet of its queue stored it");
    run(joinPackets({writeWord(condition, 9), conditional(condition, 5), writeWord(results[0], 41)}));
    Require(results[0] == 41, "COND_EXEC skipped a range whose condition an earlier packet of its queue set");
    condition = 0;
    run(joinPackets({conditional(condition, 5, 3u << 25u), writeWord(results[3], 42), writeWord(condition, 1), conditional(condition, 5, 1u << 25u), writeWord(results[4], 43)}), 0x20);
    Require(results[3] == 0 && results[4] == 43, "compute queue COND_EXEC with a cache policy evaluated wrong");

    run(joinPackets({conditional(one, 10), conditional(zero, 10), writeWord(results[5], 51), writeWord(results[6], 52), writeWord(results[7], 53)}));
    Require(results[5] == 0 && results[6] == 0 && results[7] == 53, "a nested COND_EXEC range reaching past the outer range was not skipped exactly");
    run(joinPackets({conditional(zero, 10), conditional(one, 10), writeWord(results[5], 54), writeWord(results[6], 55), writeWord(results[7], 56)}));
    Require(results[5] == 0 && results[6] == 55 && results[7] == 56, "a skipped COND_EXEC range did not skip the COND_EXEC inside it");

    static std::vector<std::uint32_t> target;
    target = joinPackets({writeWord(results[8], 61), conditional(zero, 5), writeWord(results[9], 62), writeWord(results[10], 63)});
    run(joinPackets({conditional(zero, 4), indirectBuffer(target), writeWord(results[11], 64)}));
    Require(results[8] == 0 && results[10] == 0 && results[11] == 64, "COND_EXEC over an INDIRECT_BUFFER did not skip the whole buffer");
    run(joinPackets({conditional(one, 4), indirectBuffer(target), writeWord(results[11], 65)}));
    Require(results[8] == 61 && results[9] == 0 && results[10] == 63 && results[11] == 65, "COND_EXEC over an INDIRECT_BUFFER or inside one evaluated wrong");

    const auto rejected = [](std::vector<std::uint32_t> words, const char* text) {
        results[12] = 0;
        RequireRejected([&] { submitWords(words); }, text);
        AgcDriverWaitIdle_nid_postfix();
        Require(results[12] == 0, "a conditional submission rejected for \"" + std::string(text) + "\" executed a prefix");
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
    Require(results[13] == 0, "a rejected conditional submission executed a guarded packet");
}};

const Case suspendPointWritesQueuedLabels{"SuspendPoint_QueuedLabel_IsWritten", [] {
    RequireDefaultMode();
    alignas(8) static std::uint32_t gate = 0, marker = 0;
    gate = 0;
    marker = 0;
    auto words = joinPackets({makePacket(0x3c, {0x13, low(&gate), high(&gate), 1, 0xffffffffu, 0x190}),
                              makePacket(0x49, {0x0030c514, 0x20000000, low(&marker), high(&marker), 1, 0, 0})});
    submitWords(words);
    AgcDriverSuspendPoint_nid_postfix();
    std::atomic_ref<std::uint32_t>(gate).store(1);
    for (int waited = 0; waited < 2000 && std::atomic_ref<std::uint32_t>(marker).load() == 0; ++waited) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(std::atomic_ref<std::uint32_t>(marker).load() == 1, "a label queued before a suspend point was never written");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case branchSubmission{"Submit_CondIndirectBuffer_RunsTheTakenBuffer", [] {
    RequireDefaultMode();
    static std::array<std::uint32_t, 4> results{};
    static std::vector<std::uint32_t> first, second;
    results.fill(0);
    first = joinPackets({writeWord(results[0], 71)});
    second = joinPackets({writeWord(results[1], 72)});
    auto words = joinPackets({branch(1, 0, &first, nullptr), writeWord(results[2], 73)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    Require(results[0] == 71 && results[1] == 0 && results[2] == 73, "an always-taken if-then COND_INDIRECT_BUFFER did not run its buffer");
    results.fill(0);
    words = joinPackets({branch(2, 0, &first, &second), writeWord(results[2], 74)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    Require(results[0] == 71 && results[1] == 0 && results[2] == 74, "an always-taken if-then-else COND_INDIRECT_BUFFER ran the wrong buffer");
    results.fill(0);
    words = joinPackets({branch(1, 0, nullptr, nullptr), writeWord(results[2], 75)});
    submitWords(words);
    AgcDriverWaitIdle_nid_postfix();
    Require(results[2] == 75, "an empty COND_INDIRECT_BUFFER skipped the next packet");
    for (const auto& [mode, function, text] : {std::tuple{1u, 3u, "with a comparison"}, std::tuple{0u, 0u, "invalid COND_INDIRECT_BUFFER mode"}}) {
        results.fill(0);
        words = joinPackets({writeWord(results[3], 1), branch(mode, function, &first, nullptr)});
        RequireRejected([&] { submitWords(words); }, text);
        AgcDriverWaitIdle_nid_postfix();
        Require(results[0] == 0 && results[3] == 0, "a rejected COND_INDIRECT_BUFFER submission executed a packet");
    }
}};

const Case asyncMemoryFailure{"WaitIdle_AsyncGuestFault_PropagatesToEveryEntryPoint", [] {
    RequireFailureMode();
    auto commands = makePacket(0x37, {0x100, 0x1000, 0, 1});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "memory packet was not submitted");
    RequireRejected([] { AgcDriverWaitIdle_nid_postfix(); }, "guest");
    RequireRejected([] { AgcDriverSuspendPoint_nid_postfix(); }, "guest");
    RequireRejected([&] { sceAgcDriverSubmitDcb(&packet); }, "guest");
    RequireRejected([] { LibcRunShutdown_nid_postfix(); }, "guest");
}};

const Case shutdown{"LibcRunShutdown_AfterSuccessfulSubmissions_Succeeds", [] {
    RequireDefaultMode();
    LibcRunShutdown_nid_postfix();
}};

} // namespace
