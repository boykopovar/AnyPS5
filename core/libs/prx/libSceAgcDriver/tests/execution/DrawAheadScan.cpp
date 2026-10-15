#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawAhead.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRegisterKey.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::QueueState;
using AgcDriver::DriverDetail::DrawScan;
using AgcDriver::DriverDetail::PublishedDraw;
using AgcDriver::DriverDetail::DrawPrecheckOutcome;

alignas(64) std::array<std::uint16_t, 16> FirstIndices{};
alignas(64) std::array<std::uint16_t, 16> SecondIndices{};
std::size_t Compared = 0;

void Check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct Commands {
    std::vector<std::uint32_t> words;
    std::map<std::size_t, std::vector<std::uint32_t>> registerLists;
    std::map<std::size_t, std::size_t> conditionalEnds;

    std::size_t Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload, std::uint32_t flags = 0) {
        const auto at = words.size();
        words.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u) | flags);
        words.insert(words.end(), payload);
        return at;
    }
    void Words(std::uint32_t header, std::size_t payload) {
        words.push_back(header);
        words.insert(words.end(), payload, 0u);
    }
    std::size_t Custom(std::uint32_t operation, std::initializer_list<std::uint32_t> payload) {
        return Packet(0x10, payload, operation << 2u);
    }
    void Context(std::uint32_t offset, std::uint32_t value, std::uint32_t flags = 0) { Packet(0x69, {offset, value}, flags); }
    void Shader(std::uint32_t offset, std::uint32_t value) { Packet(0x76, {offset, value}); }
    std::size_t Draw() { return Packet(0x2d, {3, 2}); }
    std::size_t CondExec() { return Packet(0x22, {0x1000, 0, 0, 0}); }
    void EndCondition(std::size_t at) {
        words[at + 4] = static_cast<std::uint32_t>(words.size() - at - 5);
        conditionalEnds[at] = words.size();
    }
    std::size_t CopyToContext(std::uint32_t offset) { return Packet(0x40, {4u, 0x1000, 0, 0xa000u + offset, 0}); }
    std::size_t IndirectContext(std::vector<std::uint32_t> pairs) {
        const auto at = Packet(0x9f, {0x1000, 0, 0x80000000u, static_cast<std::uint32_t>(pairs.size() / 2)});
        registerLists[at] = std::move(pairs);
        return at;
    }
};

struct Outcome {
    std::function<bool(std::size_t)> condition = [](std::size_t) { return true; };
    std::function<bool(std::size_t)> predicate = [](std::size_t) { return true; };
    std::function<std::uint32_t(std::uint32_t)> copied = [](std::uint32_t) { return 0u; };
};

std::map<std::size_t, QueueState> Execute(const Commands& commands, const QueueState& initial, const Outcome& outcome) {
    std::map<std::size_t, QueueState> draws;
    QueueState state = initial;
    for (std::size_t cursor = 0; cursor < commands.words.size();) {
        const auto header = commands.words[cursor];
        const auto count = AgcDriver::Pm4::PacketWords(header);
        const auto packet = std::span(commands.words).subspan(cursor, count);
        const auto opcode = (header >> 8u) & 0xffu;
        auto next = cursor + count;
        if (AgcDriver::Pm4::Predicated(header) && state.predication.operation != 0 && !outcome.predicate(cursor)) {
            cursor = opcode == 0x3f ? commands.conditionalEnds.at(cursor) : next;
            continue;
        }
        if (opcode == 0x22) {
            if (!outcome.condition(cursor)) next = commands.conditionalEnds.at(cursor);
        } else if (AgcDriver::Pm4::DrawOpcode(opcode)) {
            draws[cursor] = state;
        } else if (opcode == 0x40) {
            const auto written = AgcDriver::Pm4::CopyDataContextRegisters(packet);
            Check(written.has_value(), "the test's COPY_DATA does not write a register");
            for (std::uint32_t i = 0; i < written->second; ++i) state.context.insert_or_assign(written->first + i, outcome.copied(written->first + i));
        } else if (AgcDriver::Pm4::IndirectRegisterOpcode(opcode)) {
            AgcDriver::Pm4::ExecuteIndirectRegisters(packet, commands.registerLists.at(cursor), state);
        } else if (opcode != 0x3f && header != AgcDriver::FlipPacketHeader && header != AgcDriver::RenderingWaitPacketHeader) {
            AgcDriver::Pm4::Execute(packet, state);
        }
        cursor = next;
    }
    return draws;
}

struct Resolution {
    bool resolved = false;
    AgcDriver::Pm4::DrawParameters parameters{};
    DrawPrecheckOutcome precheck = DrawPrecheckOutcome::Rejected;
    std::uint64_t key = 0;
    std::uint64_t shape = 0;
};

Resolution Resolve(std::span<const std::uint32_t> packet, const QueueState& state) {
    Resolution resolution;
    try {
        std::string rejected;
        resolution.parameters = AgcDriver::Pm4::ResolveDraw(packet, state);
        resolution.precheck = AgcDriver::DriverDetail::PrecheckDrawState(state, resolution.parameters, false, rejected);
        resolution.key = AgcDriver::DriverDetail::RegisterStateKey(state, false, true, &resolution.shape);
        resolution.resolved = true;
    } catch (const std::exception&) {
    }
    return resolution;
}

bool SameResolution(const Resolution& a, const Resolution& b) {
    const auto& x = a.parameters;
    const auto& y = b.parameters;
    return a.resolved == b.resolved && a.precheck == b.precheck && a.key == b.key && a.shape == b.shape && x.indexAddress == y.indexAddress && x.indexCount == y.indexCount && x.indexSize == y.indexSize && x.instanceCount == y.instanceCount && x.flags == y.flags && x.indexed == y.indexed && x.firstVertex == y.firstVertex && x.firstInstance == y.firstInstance;
}

struct Scanned {
    PublishedDraw record;
    QueueState state;
    Resolution resolution;
};

std::map<std::size_t, Scanned> Scan(const Commands& commands, const QueueState& initial) {
    std::map<std::size_t, Scanned> draws;
    QueueState state = initial;
    DrawScan scan(commands.words, commands.registerLists, commands.conditionalEnds, state);
    scan.Next([&](const DrawScan::Draw& draw) {
        Scanned scanned{{}, draw.state, Resolve(draw.packet, draw.state)};
        scanned.record.offset = draw.offset;
        scanned.record.certain = draw.certain;
        scanned.record.indexBase = draw.state.indexBase;
        scanned.record.indexType = draw.state.indexType;
        scanned.record.instanceCount = draw.state.instanceCount;
        if (draw.certain) scan.Capture(scanned.record.checks);
        draws.emplace(draw.offset, std::move(scanned));
        return true;
    });
    return draws;
}

bool Same(const QueueState& a, const QueueState& b) {
    return a.context == b.context && a.shader == b.shader && a.userConfig == b.userConfig && a.indexBase == b.indexBase && a.indexType == b.indexType && a.instanceCount == b.instanceCount;
}

QueueState Initial() {
    QueueState state;
    state.userConfig.insert_or_assign(0x24a, 0u);
    return state;
}

std::map<std::size_t, bool> Verify(const Commands& commands, const Outcome& outcome, const std::string& name) {
    const auto initial = Initial();
    const auto scanned = Scan(commands, initial);
    const auto executed = Execute(commands, initial, outcome);
    std::map<std::size_t, bool> reused;
    for (const auto& [offset, state] : executed) {
        const auto found = scanned.find(offset);
        Check(found != scanned.end(), name + ": the scan missed an executed draw");
        const auto& record = found->second.record;
        const bool matches = record.certain && DrawScan::Matches(record, state);
        if (matches) {
            Check(Same(found->second.state, state), name + ": a published draw matched an executed state that differs from the scanned one at DWORD " + std::to_string(offset));
            const auto packet = std::span(commands.words).subspan(offset, AgcDriver::Pm4::PacketWords(commands.words[offset]));
            Check(SameResolution(found->second.resolution, Resolve(packet, state)), name + ": the parameters, key, shape or precheck published for the draw at DWORD " + std::to_string(offset) + " differ from the queue's own resolution");
            ++Compared;
        }
        reused[offset] = matches;
    }
    return reused;
}

void UnconditionalWrites() {
    Commands commands;
    commands.Context(0x2d5, 7);
    commands.Shader(0x8c, 3);
    commands.Packet(0x2f, {4});
    const auto first = commands.Draw();
    commands.Context(0x2d5, 8);
    commands.Packet(0x12, {0});
    const auto second = commands.Draw();
    const auto reused = Verify(commands, {}, "unconditional writes");
    Check(reused.at(first) && reused.at(second), "unconditional writes: a draw after unconditional packets was not reusable");
}

void ConditionalRange(bool executes) {
    Commands commands;
    commands.Context(0x2d5, 1);
    const auto condition = commands.CondExec();
    commands.Context(0x2d5, 2);
    commands.Packet(0x2f, {9});
    const auto inside = commands.Draw();
    commands.EndCondition(condition);
    const auto after = commands.Draw();
    commands.Context(0x2d5, 3);
    commands.Packet(0x2f, {1});
    const auto rewritten = commands.Draw();
    Outcome outcome;
    outcome.condition = [&](std::size_t) { return executes; };
    const auto name = std::string("COND_EXEC ") + (executes ? "taken" : "skipped");
    const auto reused = Verify(commands, outcome, name);
    Check(reused.at(after) == executes, name + ": the draw after a conditional register write was " + (executes ? "not reused" : "reused"));
    Check(reused.at(rewritten), name + ": an unconditional rewrite did not end the divergence");
    if (executes) Check(reused.at(inside), name + ": the draw inside the range was not reused");
}

void CopiedRegister() {
    Commands commands;
    commands.Context(0x2d5, 5);
    commands.CopyToContext(0x2d5);
    const auto copied = commands.Draw();
    Outcome same;
    same.copied = [](std::uint32_t) { return 5u; };
    Check(Verify(commands, same, "COPY_DATA same value").at(copied), "COPY_DATA: a register copied with the scanned value was not reused");
    Outcome different;
    different.copied = [](std::uint32_t) { return 6u; };
    Check(!Verify(commands, different, "COPY_DATA other value").at(copied), "COPY_DATA: a register copied from memory with another value was reused");
}

void ContextStack() {
    Commands commands;
    const auto condition = commands.CondExec();
    commands.Context(0x2d5, 4);
    commands.EndCondition(condition);
    commands.Custom(0x1a, {1, 0});
    commands.Context(0x2d5, 9);
    const auto pushed = commands.Draw();
    commands.Custom(0x1a, {2, 0});
    const auto popped = commands.Draw();
    for (const bool executes : {true, false}) {
        Outcome outcome;
        outcome.condition = [&](std::size_t) { return executes; };
        const auto reused = Verify(commands, outcome, "CONTEXT_STATE");
        Check(reused.at(pushed), "CONTEXT_STATE: an unconditional write inside a pushed context did not end the divergence");
        Check(reused.at(popped) == executes, "CONTEXT_STATE: popping did not restore the divergence of the saved context");
    }
    Commands cleared;
    const auto clearedCondition = cleared.CondExec();
    cleared.Packet(0x12, {0});
    cleared.EndCondition(clearedCondition);
    const auto lost = cleared.Draw();
    cleared.Custom(0x09, {0});
    const auto reset = cleared.Draw();
    Outcome skipped;
    skipped.condition = [](std::size_t) { return false; };
    const auto scanned = Scan(cleared, QueueState{});
    Check(!scanned.at(lost).record.certain, "CLEAR_STATE: a conditional CLEAR_STATE left the scan certain");
    Check(scanned.at(reset).record.certain, "RESET: an unconditional reset did not make the scan certain again");
    Verify(cleared, skipped, "conditional CLEAR_STATE");
}

void Predication(bool passes) {
    Commands commands;
    commands.Packet(0x20, {3u << 16u, 0x1000, 0});
    commands.Context(0x2d5, 6, 1);
    const auto predicated = commands.Draw();
    commands.Packet(0x20, {0, 0, 0});
    commands.Context(0x2d5, 2);
    const auto cleared = commands.Draw();
    Outcome outcome;
    outcome.predicate = [&](std::size_t) { return passes; };
    const auto reused = Verify(commands, outcome, "predication");
    Check(reused.at(predicated) == passes, "predication: a predicated register write was not compared");
    Check(reused.at(cleared), "predication: an unconditional write after predication was not reusable");
    Commands lost;
    const auto condition = lost.CondExec();
    lost.Packet(0x20, {3u << 16u, 0x1000, 0});
    lost.EndCondition(condition);
    const auto after = lost.Draw();
    Check(!Scan(lost, QueueState{}).at(after).record.certain, "predication: a conditional SET_PREDICATION left the scan certain");
}

void IndirectRegisters() {
    Commands commands;
    const auto condition = commands.CondExec();
    commands.IndirectContext({0x2d5, 3, 0x2d6, 4});
    commands.EndCondition(condition);
    const auto after = commands.Draw();
    for (const bool executes : {true, false}) {
        Outcome outcome;
        outcome.condition = [&](std::size_t) { return executes; };
        Check(Verify(commands, outcome, "indirect registers").at(after) == executes, "indirect registers: a conditional register list was not compared");
    }
}

void DrawFields() {
    Commands commands;
    const auto condition = commands.CondExec();
    commands.Packet(0x26, {0x2000, 0});
    commands.Packet(0x2a, {1});
    commands.EndCondition(condition);
    const auto after = commands.Draw();
    for (const bool executes : {true, false}) {
        Outcome outcome;
        outcome.condition = [&](std::size_t) { return executes; };
        Check(Verify(commands, outcome, "index fields").at(after) == executes, "index fields: a conditional INDEX_BASE or INDEX_TYPE was not compared");
    }
}

void IndexedDrawAfterConditionalBase() {
    FirstIndices.fill(1);
    SecondIndices.fill(2);
    const auto low = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    const auto high = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); };
    Commands commands;
    commands.Packet(0x2a, {0});
    commands.Packet(0x26, {low(FirstIndices.data()), high(FirstIndices.data())});
    const auto before = commands.Packet(0x35, {8, 2, 4, 0});
    const auto condition = commands.CondExec();
    commands.Packet(0x26, {low(SecondIndices.data()), high(SecondIndices.data())});
    commands.EndCondition(condition);
    const auto after = commands.Draw();
    const auto offset = commands.Packet(0x35, {8, 2, 4, 0});
    for (const bool executes : {true, false}) {
        Outcome outcome;
        outcome.condition = [&](std::size_t) { return executes; };
        const auto compared = Compared;
        const auto reused = Verify(commands, outcome, "indexed draw after a conditional INDEX_BASE");
        Check(reused.at(before), "indexed draw after a conditional INDEX_BASE: the draw before the conditional write was not reused");
        Check(reused.at(after) == executes, "indexed draw after a conditional INDEX_BASE: the auto draw was not compared against the conditional base");
        Check(reused.at(offset) == executes, "indexed draw after a conditional INDEX_BASE: the indexed draw was " + std::string(executes ? "not reused although the base was written" : "reused although the base was not written"));
        Check(Compared - compared == (executes ? 3u : 1u), "indexed draw after a conditional INDEX_BASE: " + std::to_string(Compared - compared) + " published resolutions were compared with the queue's");
    }
    const auto scanned = Scan(commands, Initial());
    const auto& resolution = scanned.at(offset).resolution;
    Check(resolution.resolved && resolution.parameters.indexed && resolution.parameters.indexAddress == reinterpret_cast<std::uintptr_t>(SecondIndices.data()) + 2 * sizeof(std::uint16_t) && resolution.parameters.indexCount == 4, "indexed draw after a conditional INDEX_BASE: the scan resolved the draw to another index range than the executed base and offset");
}

void TooManyDivergent() {
    Commands commands;
    const auto condition = commands.CondExec();
    for (std::uint32_t i = 0; i <= DrawScan::MaxRegisterChecks; ++i) commands.Context(0x191 + (i % 32) + (i / 32) * 0x100, i);
    commands.EndCondition(condition);
    const auto after = commands.Draw();
    Check(!Scan(commands, QueueState{}).at(after).record.certain, "a scan with more divergent registers than it compares was certain");
}

void SkippedPackets() {
    Commands commands;
    commands.Custom(0x0b, {0x41});
    commands.Words(AgcDriver::FlipPacketHeader, 5);
    const auto after = commands.Draw();
    const auto reused = Verify(commands, {}, "skipped packets");
    Check(reused.at(after), "a flip or marker made a later draw unpublishable");
}

}

int main() {
    try {
        UnconditionalWrites();
        ConditionalRange(true);
        ConditionalRange(false);
        CopiedRegister();
        ContextStack();
        Predication(true);
        Predication(false);
        IndirectRegisters();
        DrawFields();
        IndexedDrawAfterConditionalBase();
        TooManyDivergent();
        SkippedPackets();
        std::cout << "Draw-ahead scan tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
