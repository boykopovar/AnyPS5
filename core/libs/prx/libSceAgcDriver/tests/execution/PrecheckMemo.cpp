#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRegisterKey.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static std::optional<DrawVerdict> Precheck(const QueueState& queue, const Pm4::DrawParameters& parameters, std::string& rejected, bool& traceIndirect) {
        static const Submission submission{};
        static const std::array<std::uint32_t, 4> packet{0xc0001000u, 0u, 0u, 0u};
        return Driver::Get().precheckDraw(queue, submission, 0, packet, parameters, rejected, traceIndirect);
    }
    static PrecheckMemoCounts Counts() { return Driver::precheckMemoCounts(); }
    static void Reset() { Driver::resetPrecheckMemo(); }
};

}

namespace {

using AgcDriver::DriverDetail::DriverTestAccess;
using AgcDriver::DriverDetail::DrawKeyFingerprints;
using AgcDriver::DriverDetail::DrawVerdict;
using AgcDriver::QueueState;
using AgcDriver::Registers;
namespace Graphics = AgcDriver::Graphics;
namespace Pm4 = AgcDriver::Pm4;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

struct Outcome {
    int verdict = -1;
    std::string rejected;
    bool traceIndirect = false;
    std::string error;
    bool operator==(const Outcome&) const = default;
};

Outcome run(const QueueState& queue, const Pm4::DrawParameters& parameters) {
    Outcome outcome;
    std::string rejected = "stale";
    bool traceIndirect = true;
    try {
        const auto verdict = DriverTestAccess::Precheck(queue, parameters, rejected, traceIndirect);
        outcome.verdict = verdict ? static_cast<int>(*verdict) : -1;
    } catch (const std::exception& error) {
        outcome.error = error.what();
    }
    outcome.rejected = rejected;
    outcome.traceIndirect = traceIndirect;
    return outcome;
}

std::string describe(const Outcome& outcome) {
    return "verdict " + std::to_string(outcome.verdict) + " rejected '" + outcome.rejected + "' trace " + std::to_string(outcome.traceIndirect) + " error '" + outcome.error + "'";
}

bool allowed(const Outcome& outcome) {
    return outcome.verdict == -1 && outcome.error.empty() && outcome.rejected.empty() && !outcome.traceIndirect;
}

bool runsPass(const QueueState& queue) {
    try {
        if (Graphics::DecodeColorMetadataPass(queue)) return true;
        if (Graphics::DecodeDepthExpandPass(queue)) return true;
        return Graphics::DecodeDepthCopyPass(queue).has_value();
    } catch (const std::exception&) {
        return false;
    }
}

Pm4::DrawParameters plainDraw() {
    Pm4::DrawParameters parameters{};
    parameters.indexed = false;
    parameters.indexCount = 3;
    parameters.instanceCount = 1;
    return parameters;
}

Pm4::DrawParameters indexedDraw(std::uint32_t indexSize) {
    Pm4::DrawParameters parameters{};
    parameters.indexed = true;
    parameters.indexCount = 6;
    parameters.indexSize = indexSize;
    parameters.instanceCount = 1;
    return parameters;
}

Pm4::DrawParameters indirectDraw() {
    Pm4::DrawParameters parameters{};
    parameters.indexed = false;
    Pm4::DrawParameters::IndirectDraw indirect{};
    indirect.count = 1;
    parameters.indirect = indirect;
    return parameters;
}

QueueState baseState() {
    QueueState queue;
    queue.shader.insert_or_assign(0x008u, 0x1000u);
    queue.shader.insert_or_assign(0x009u, 0u);
    return queue;
}

QueueState restartState() {
    auto queue = baseState();
    queue.userConfig.insert_or_assign(0x242u, 4u);
    queue.userConfig.insert_or_assign(0x24bu, 1u);
    queue.context.insert_or_assign(0x103u, 0xffffffffu);
    return queue;
}

QueueState renderControlState() {
    auto queue = baseState();
    queue.context.insert_or_assign(0x000u, 1u);
    queue.context.insert_or_assign(0x010u, 1u);
    queue.context.insert_or_assign(0x012u, 0u);
    return queue;
}

QueueState depthState() {
    auto queue = baseState();
    queue.context.insert_or_assign(0x010u, 1u);
    queue.context.insert_or_assign(0x200u, 0x76u);
    return queue;
}

QueueState colorOffState() {
    auto queue = baseState();
    queue.context.insert_or_assign(0x202u, 0xcc0000u);
    queue.context.insert_or_assign(0x8eu, 0u);
    return queue;
}

bool memoExpected() {
    return std::getenv("APS5_NO_PRECHECK_MEMO") == nullptr;
}

struct Delta {
    AgcDriver::DriverDetail::PrecheckMemoCounts before = DriverTestAccess::Counts();
    std::uint64_t hits() const { return DriverTestAccess::Counts().hits - before.hits; }
    std::uint64_t records() const { return DriverTestAccess::Counts().records - before.records; }
};

void expectAllowRecorded(const QueueState& queue, const Pm4::DrawParameters& parameters, const std::string& name) {
    Delta delta;
    check(allowed(run(queue, parameters)), name + ": the draw was not let through");
    check(delta.hits() == 0, name + ": a first draw hit");
    check(delta.records() == (memoExpected() ? 1u : 0u), name + ": a draw let through was not recorded exactly when the memo is on");
}

void expectHit(const QueueState& queue, const Pm4::DrawParameters& parameters, const std::string& name) {
    Delta delta;
    check(allowed(run(queue, parameters)), name + ": the repeat was not let through");
    check(delta.hits() == (memoExpected() ? 1u : 0u), name + ": a repeat did not hit exactly when the memo is on");
    check(delta.records() == 0, name + ": a hit recorded again");
}

void expectMiss(const QueueState& queue, const Pm4::DrawParameters& parameters, const std::string& name, const Outcome& expected) {
    Delta delta;
    const auto outcome = run(queue, parameters);
    check(outcome == expected, name + ": the outcome differs from the expected one: " + outcome.rejected + outcome.error);
    check(delta.hits() == 0, name + ": a state the memo does not hold hit");
}

void testRepeats() {
    DriverTestAccess::Reset();
    const auto queue = baseState();
    expectAllowRecorded(queue, plainDraw(), "plain draw");
    expectHit(queue, plainDraw(), "plain repeat");
    expectHit(queue, indexedDraw(2), "indexed 16-bit repeat");
    expectHit(queue, indexedDraw(4), "indexed 32-bit repeat");
    expectHit(queue, indirectDraw(), "indirect repeat");
    auto empty = plainDraw();
    empty.indexCount = 0;
    Delta delta;
    const auto nothing = run(queue, empty);
    check(nothing.verdict == static_cast<int>(DrawVerdict::Nothing) && delta.hits() == 0, "an empty draw reached the memo");
    empty.indexCount = 3;
    empty.instanceCount = 0;
    check(run(queue, empty).verdict == static_cast<int>(DrawVerdict::Nothing), "a draw without instances was not dropped");
    check(delta.hits() == 0 && delta.records() == 0, "an empty draw changed the memo");
    expectHit(queue, plainDraw(), "repeat after empty draws");
}

void testRegisterWrites() {
    DriverTestAccess::Reset();
    auto queue = baseState();
    expectAllowRecorded(queue, plainDraw(), "register writes");
    queue.shader.insert_or_assign(0x00cu, 0x1234u);
    queue.shader.insert_or_assign(0x10fu, 0x4321u);
    queue.shader.insert_or_assign(0x083u, 7u);
    queue.userConfig.insert_or_assign(0x24au, 9u);
    queue.context.insert_or_assign(0x001u, 5u);
    expectHit(queue, plainDraw(), "writes outside the draw-key registers");
    auto rejected = queue;
    rejected.context.insert_or_assign(0x207u, 0x40000000u);
    DriverTestAccess::Reset();
    const auto expected = run(rejected, plainDraw());
    check(expected.verdict == static_cast<int>(DrawVerdict::Rejected) && !expected.rejected.empty(), "the rejecting state is not rejected");
    DriverTestAccess::Reset();
    check(allowed(run(queue, plainDraw())), "the state before the masked write is not let through");
    expectMiss(rejected, plainDraw(), "a masked write", expected);
    DriverTestAccess::Reset();
    queue.context.insert_or_assign(0x207u, 0x40000000u);
    check(run(queue, plainDraw()).verdict == static_cast<int>(DrawVerdict::Rejected), "a rejected draw was let through");
    Delta none;
    check(run(queue, plainDraw()).verdict == static_cast<int>(DrawVerdict::Rejected) && none.hits() == 0 && none.records() == 0, "a rejected draw touched the memo");
    queue.context.insert_or_assign(0x207u, 0u);
    expectAllowRecorded(queue, plainDraw(), "after the rejection is undone");
    queue.context.insert_or_assign(0x207u, 0x40000000u);
    check(run(queue, plainDraw()).verdict == static_cast<int>(DrawVerdict::Rejected), "the memo let a rejected state through");
    queue.context.insert_or_assign(0x207u, 0u);
    expectHit(queue, plainDraw(), "a restored value after a rejected draw");
    auto erased = queue;
    erased.shader.erase(0x008u);
    erased.shader.erase(0x009u);
    const auto fresh = [&] {
        DriverTestAccess::Reset();
        return run(erased, plainDraw());
    }();
    DriverTestAccess::Reset();
    run(queue, plainDraw());
    check(run(erased, plainDraw()) == fresh, "an erased pixel program was answered from the memo");
}

void testRestart() {
    DriverTestAccess::Reset();
    auto queue = baseState();
    expectAllowRecorded(queue, indexedDraw(2), "restart off");
    queue.context.insert_or_assign(0x103u, 0x12345u);
    expectHit(queue, indexedDraw(2), "reset index while restart is off");
    expectHit(queue, indexedDraw(4), "other index size while restart is off");
    expectHit(queue, plainDraw(), "non-indexed draw while restart is off");

    DriverTestAccess::Reset();
    auto restart = restartState();
    expectAllowRecorded(restart, indexedDraw(2), "restart with all ones");
    expectHit(restart, indexedDraw(2), "restart repeat");
    expectHit(restart, plainDraw(), "a non-indexed draw under restart");
    {
        Delta delta;
        check(allowed(run(restart, indexedDraw(4))), "restart with 32-bit indices was not let through");
        check(delta.hits() == 0 && delta.records() == (memoExpected() ? 1u : 0u), "another index size hit an entry recorded for 16-bit indices");
    }
    expectHit(restart, indexedDraw(4), "restart repeat with 32-bit indices");
    expectHit(restart, plainDraw(), "a non-indexed draw after 32-bit indices");
    restart.context.insert_or_assign(0x103u, 0x12345u);
    const auto rejected = run(restart, indexedDraw(2));
    check(rejected.verdict == static_cast<int>(DrawVerdict::Rejected) && !rejected.rejected.empty(), "a changed reset index was let through");
    {
        Delta delta;
        check(run(restart, indexedDraw(2)) == rejected, "a repeat of the changed reset index differs");
        check(run(restart, indexedDraw(4)).verdict == static_cast<int>(DrawVerdict::Rejected), "a changed reset index was let through for 32-bit indices");
        check(delta.hits() == 0 && delta.records() == 0, "a changed reset index hit an entry recorded for another one, or a rejected draw was recorded");
    }
    expectHit(restart, plainDraw(), "a non-indexed draw under a changed reset index");
    auto erased = restart;
    erased.context.erase(0x103u);
    DriverTestAccess::Reset();
    const auto freshErased = run(erased, indexedDraw(2));
    DriverTestAccess::Reset();
    check(allowed(run(restartState(), indexedDraw(2))), "restart with all ones was not let through again");
    check(run(erased, indexedDraw(2)) == freshErased, "an erased reset index was answered from the memo");

    DriverTestAccess::Reset();
    const auto restartAgain = restartState();
    expectAllowRecorded(restartAgain, plainDraw(), "a non-indexed draw first");
    Delta delta;
    check(allowed(run(restartAgain, indexedDraw(2))), "an indexed draw after a non-indexed one was not let through");
    check(delta.hits() == 0 && delta.records() == (memoExpected() ? 1u : 0u), "an indexed draw hit an entry that never checked the restart");
    expectHit(restartAgain, indexedDraw(2), "an indexed repeat after the entry was upgraded");

    DriverTestAccess::Reset();
    auto missing = restartState();
    missing.context.erase(0x103u);
    expectAllowRecorded(missing, plainDraw(), "a non-indexed draw without a reset index");
    Delta zeroDelta;
    const auto answer = run(missing, indexedDraw(0));
    check(answer.verdict == static_cast<int>(DrawVerdict::Rejected) && !answer.rejected.empty(), "an indexed draw without a reset index was let through");
    check(zeroDelta.hits() == 0, "an indexed draw hit an entry recorded without restart inputs");
}

void testRenderControl() {
    DriverTestAccess::Reset();
    const auto queue = renderControlState();
    Delta delta;
    const auto first = run(queue, plainDraw());
    check(allowed(first), "a draw with a depth render control was not let through: " + describe(first));
    check(allowed(run(queue, plainDraw())), "a repeat with a depth render control was not let through");
    check(delta.hits() == 0 && delta.records() == 0, "a draw with a depth render control was memoized");
    auto cleared = queue;
    cleared.context.insert_or_assign(0x000u, 0u);
    check(allowed(run(cleared, plainDraw())), "the draw without the render control was not let through");
    check(delta.records() == (memoExpected() ? 1u : 0u), "the draw without the render control was not recorded");
    check(allowed(run(queue, plainDraw())) && delta.hits() == 0, "a render control hit an entry recorded without it");
}

void testThreads() {
    DriverTestAccess::Reset();
    const auto queue = baseState();
    expectAllowRecorded(queue, plainDraw(), "main thread");
    std::string failure;
    std::thread other([&] {
        try {
            expectAllowRecorded(queue, plainDraw(), "other thread");
            expectHit(queue, plainDraw(), "other thread repeat");
        } catch (const std::exception& error) {
            failure = error.what();
        }
    });
    other.join();
    check(failure.empty(), failure);
    expectHit(queue, plainDraw(), "main thread after the other thread");
}

struct SweepStats {
    std::uint64_t mutations = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t reads = 0;
};

Registers& bankOf(QueueState& queue, int bank) {
    return bank == 0 ? queue.context : bank == 1 ? queue.shader : queue.userConfig;
}

const Registers& bankOf(const QueueState& queue, int bank) {
    return bank == 0 ? queue.context : bank == 1 ? queue.shader : queue.userConfig;
}

bool restartOn(const QueueState& queue) {
    return queue.userConfig.contains(0x24bu) && queue.userConfig.at(0x24bu) != 0;
}

std::optional<std::uint32_t> resetIndexOf(const QueueState& queue) {
    if (!queue.context.contains(0x103u)) return std::nullopt;
    return queue.context.at(0x103u);
}

const char* bankName(int bank) {
    return bank == 0 ? "context" : bank == 1 ? "shader" : "user-config";
}

void sweep(const std::string& name, const QueueState& base, const Pm4::DrawParameters& parameters, bool recorded, SweepStats& stats) {
    DriverTestAccess::Reset();
    check(allowed(run(base, parameters)), name + ": the base state is not let through");
    for (int bank = 0; bank < 3; ++bank) {
        for (std::uint32_t offset = 0; offset < 0x400u; ++offset) {
            std::vector<std::optional<std::uint32_t>> values;
            values.push_back(std::nullopt);
            for (const auto value : {0u, 1u, 2u, 0xffffffffu, 0x80000000u, 0xaaaaaaaau}) values.push_back(value);
            const auto& baseBank = bankOf(base, bank);
            const std::uint32_t current = baseBank.contains(offset) ? baseBank.at(offset) : 0u;
            for (std::uint32_t bit = 0; bit < 32; ++bit) values.push_back(current ^ (1u << bit));
            for (const auto& value : values) {
                QueueState mutated = base;
                if (value) bankOf(mutated, bank).insert_or_assign(offset, *value);
                else bankOf(mutated, bank).erase(offset);
                if (runsPass(mutated)) continue;
                DriverTestAccess::Reset();
                run(base, parameters);
                const auto before = DriverTestAccess::Counts();
                const auto got = run(mutated, parameters);
                const bool hit = DriverTestAccess::Counts().hits > before.hits;
                DriverTestAccess::Reset();
                const auto fresh = run(mutated, parameters);
                const auto where = name + ": " + bankName(bank) + " " + hex(offset) + " = " + (value ? hex(*value) : std::string("erased"));
                check(got == fresh, where + " gave another answer from the memo than from a full precheck: " + std::to_string(got.verdict) + " '" + got.rejected + "' '" + got.error + "' against " + std::to_string(fresh.verdict) + " '" + fresh.rejected + "' '" + fresh.error + "'");
                const bool sameKey = DrawKeyFingerprints(mutated) == DrawKeyFingerprints(base);
                const bool baseDecides = parameters.indexed && restartOn(base);
                const bool mutatedDecides = parameters.indexed && restartOn(mutated);
                const bool restartMatches = !mutatedDecides || (baseDecides && resetIndexOf(mutated) == resetIndexOf(base));
                check(hit == (memoExpected() && recorded && sameKey && restartMatches), where + " hit " + std::to_string(hit) + " against fingerprints equal " + std::to_string(sameKey) + " and restart inputs equal " + std::to_string(restartMatches));
                ++stats.mutations;
                (hit ? stats.hits : stats.misses) += 1;
                if (!allowed(fresh)) continue;
                std::vector<Graphics::RegisterRead> log;
                {
                    const Graphics::RegisterReadLogScope scope(&log);
                    DriverTestAccess::Reset();
                    run(mutated, parameters);
                }
                for (const auto& read : log) {
                    const bool resetIndex = read.bank == Graphics::RegisterBank::Context && read.offset == 0x103u;
                    const bool mutatedRestart = parameters.indexed && restartOn(mutated);
                    if (resetIndex) {
                        check(mutatedRestart, where + " read the reset index while restart is off");
                        continue;
                    }
                    check(Graphics::DrawKeyCovers(read), where + " read " + bankName(static_cast<int>(read.bank)) + " " + hex(read.offset) + ", which the draw key does not cover");
                    ++stats.reads;
                }
            }
        }
    }
}

void testSweeps() {
    SweepStats stats;
    sweep("plain", baseState(), plainDraw(), true, stats);
    sweep("indexed", baseState(), indexedDraw(4), true, stats);
    sweep("indirect", baseState(), indirectDraw(), true, stats);
    sweep("depth", depthState(), plainDraw(), true, stats);
    sweep("color off", colorOffState(), plainDraw(), true, stats);
    sweep("restart", restartState(), indexedDraw(2), true, stats);
    sweep("restart 32-bit", restartState(), indexedDraw(4), true, stats);
    sweep("restart plain", restartState(), plainDraw(), true, stats);
    sweep("render control", renderControlState(), plainDraw(), false, stats);
    check(stats.mutations > 100000u, "the sweeps covered too few mutations");
    check(stats.misses > 0, "no mutation reached a full precheck");
    check(memoExpected() ? stats.hits > 1000u : stats.hits == 0, "the sweeps made no memo hits, or made some with the memo off");
    check(stats.reads > 1000u, "the sweeps logged too few register reads");
    std::printf("precheck memo sweeps: %llu mutations, %llu hits, %llu misses, %llu covered reads\n", static_cast<unsigned long long>(stats.mutations), static_cast<unsigned long long>(stats.hits), static_cast<unsigned long long>(stats.misses), static_cast<unsigned long long>(stats.reads));
}

}

int main() {
    try {
        testRepeats();
        testRegisterWrites();
        testRestart();
        testRenderControl();
        testThreads();
        testSweeps();
        std::puts(memoExpected() ? "Draw precheck memo passed" : "Draw precheck without the memo passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
