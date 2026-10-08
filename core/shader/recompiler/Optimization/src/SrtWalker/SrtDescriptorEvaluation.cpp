#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtExecutionPlan.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>

namespace ShaderRecompiler::Detail {

namespace {

std::string& failureReason() {
    struct FailureReasonStorage {};
    return HostThreadLocal<std::string, FailureReasonStorage>();
}

std::string DescribeValue(const IrValue* value, std::uint32_t depth) {
    if (value == nullptr) return "null";
    value = value->Resolve();
    std::string text(IrOpcodeName(value->Opcode()));
    if (value->HasImmediate() && value->Type() == IrType::U32) return text + "(" + std::to_string(value->ImmediateU32()) + ")";
    if (depth == 0 || value->ArgumentCount() == 0) return text;
    text += "(";
    for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
        if (index != 0) text += ", ";
        text += DescribeValue(value->Argument(index), depth - 1);
    }
    return text + ")";
}

bool Fail(std::string reason) {
    failureReason() = std::move(reason);
    return false;
}

const DescriptorSource* Source(const IrResourcePlan& program, std::uint32_t source) {
    if (source >= program.descriptorSources.size()) {
        return nullptr;
    }
    return &program.descriptorSources[source];
}

}

static bool EvaluateRuntimeSourcesGeneric(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources) {
    failureReason().clear();
    static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
    if (debug) {
        for (std::size_t slot = 0; slot < program.srtReads.size(); ++slot) std::fprintf(stderr, "[srt] slot %zu = %s"  "\n", slot, DescribeValue(program.srtReads[slot].value, 6).c_str());
    }
    if (!program.srtPlanComplete) {
        return Fail("SRT plan is incomplete");
    }
    if (std::any_of(cleanFlatSlots.begin(), cleanFlatSlots.end(), [](std::uint8_t clean) { return clean != 0u; }) && runtime.readSpecializationMemory == nullptr) {
        return Fail("clean flat slots need specialization memory");
    }
    SrtRuntime cleanRuntime = runtime;
    cleanRuntime.readMemory = runtime.readSpecializationMemory;
    Evaluator cleanEvaluator(program, cleanRuntime);
    Evaluator evaluator(program, runtime, cleanFlatSlots, &cleanEvaluator);
    std::vector<std::uint8_t> active;
    if (evaluateFlat) {
        active.assign(program.descriptorSources.size(), 1u);
    }
    if (evaluateFlat && !program.controlFlow.empty()) {
        for (const auto& block : program.controlFlow) {
            for (const auto source : block.sources) {
                active.at(source) = 0u;
            }
        }
        std::vector<std::uint8_t> visited(program.controlFlow.size());
        std::vector<std::uint32_t> pending {0};
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            if (visited.at(index)) {
                continue;
            }
            visited[index] = 1u;
            const auto& block = program.controlFlow[index];
            for (const auto source : block.sources) {
                active[source] = 1u;
            }
            std::uint32_t condition = 0;
            const bool cleanEvaluable = block.condition != nullptr && runtime.readSpecializationMemory != nullptr && cleanEvaluator.Evaluate(block.condition, condition);
            if (cleanEvaluable) {
                pending.push_back(block.successors[condition != 0u ? 0u : 1u]);
            } else {
                pending.insert(pending.end(), block.successors.begin(), block.successors.end());
            }
        }
    }
    std::vector<DescriptorValue> evaluated;
    evaluated.reserve(sources.size());
    for (const auto sourceIndex : sources) {
        const auto* source = Source(program, sourceIndex);
        if (source == nullptr) {
            return Fail("descriptor source " + std::to_string(sourceIndex) + " does not exist");
        }
        DescriptorValue value;
        value.dwordCount = source->dwordCount;
        if (!evaluateFlat || active[sourceIndex]) {
            for (std::uint32_t index = 0; index < source->dwordCount; index++) {
                if (!evaluator.Evaluate(source->dwords[index], value.dwords[index])) {
                    std::string detail = DescribeValue(source->dwords[index], 4);
                    const IrValue* dword = source->dwords[index]->Resolve();
                    if (dword->Opcode() == IrOpcode::ReadConst && dword->ArgumentCount() == 2 && dword->Argument(1)->Resolve()->HasImmediate()) {
                        const auto slot = dword->Argument(1)->Resolve()->ImmediateU32();
                        if (slot < program.srtReads.size()) detail += " where slot " + std::to_string(slot) + " = " + DescribeValue(program.srtReads[slot].value, 8);
                    }
                    return Fail("descriptor source " + std::to_string(sourceIndex) + " dword " + std::to_string(index) + ": " + detail);
                }
            }
        }
        evaluated.push_back(value);
    }
    std::vector<std::uint32_t> flattened;
    if (evaluateFlat) {
        flattened.resize(program.srtReads.size());
        for (const auto& read : program.srtReads) {
            const bool clean = read.flatOffset < cleanFlatSlots.size() && cleanFlatSlots[read.flatOffset] != 0u;
            auto& selected = clean ? cleanEvaluator : evaluator;
            // A pure slot's raw read is reachable from no root, so it was not evaluated (nor
            // cached) before this loop: its dereference happens here, once, and is recorded as
            // the slot's leaf; reads nested in its address cone land among the other reads.
            auto* trace = runtime.readTrace;
            const bool pure = trace != nullptr && read.flatOffset < program.pureFlatSlots.size() && program.pureFlatSlots[read.flatOffset] != 0u;
            if (pure) {
                trace->leaf = read.value->Resolve();
                trace->leafSlot = read.flatOffset;
            }
            const bool evaluated = read.flatOffset < flattened.size() && selected.Evaluate(read.value, flattened[read.flatOffset]);
            if (pure) trace->leaf = nullptr;
            if (!evaluated) {
                return Fail(std::string(clean ? "clean " : "") + "SRT read at flat offset " + std::to_string(read.flatOffset) + ": " + DescribeValue(read.value, 4));
            }
        }
    }
    results = std::move(evaluated);
    activeSources = std::move(active);
    if (evaluateFlat) {
        flat = std::move(flattened);
    }
    return true;
}

namespace {

struct RecordedReads {
    const SrtRuntime& runtime;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> words;
    std::size_t cursor = 0;
    bool replay = false;

    static bool Read(void* context, std::uint64_t address, std::uint32_t* value) {
        auto& state = *static_cast<RecordedReads*>(context);
        if (state.replay) {
            if (state.cursor >= state.words.size() || state.words[state.cursor].first != address) return false;
            *value = state.words[state.cursor++].second;
            return true;
        }
        if (state.runtime.readMemory != nullptr) {
            if (!state.runtime.readMemory(state.runtime.userContext, address, value)) return false;
        } else std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
        state.words.emplace_back(address, *value);
        return true;
    }
};

bool verifyExecution(const IrResourcePlan& program, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, std::vector<std::uint8_t>& activeSources) {
    RecordedReads reads{runtime};
    auto captured = runtime;
    captured.userContext = &reads;
    captured.readMemory = RecordedReads::Read;
    SrtReadTrace actualTrace, expectedTrace;
    captured.readTrace = &actualTrace;
    const bool evaluated = program.executionPlan->Evaluate(captured, results, flat, activeSources);
    if (runtime.readTrace != nullptr) {
        runtime.readTrace->leaves.insert(runtime.readTrace->leaves.end(), actualTrace.leaves.begin(), actualTrace.leaves.end());
        runtime.readTrace->otherReads.insert(runtime.readTrace->otherReads.end(), actualTrace.otherReads.begin(), actualTrace.otherReads.end());
    }
    if (!evaluated) return Fail("compiled SRT inputs could not be read");
    reads.replay = true;
    captured.readTrace = &expectedTrace;
    std::vector<DescriptorValue> expected;
    std::vector<std::uint32_t> expectedFlat;
    std::vector<std::uint8_t> expectedActive;
    const bool matches = EvaluateRuntimeSourcesGeneric(program, program.materializationSources, captured, expected, expectedFlat, true, {}, expectedActive)
        && reads.cursor == reads.words.size() && expected == results && expectedFlat == flat && expectedActive == activeSources
        && expectedTrace.leaves == actualTrace.leaves && expectedTrace.otherReads == actualTrace.otherReads;
    if (!matches) return Fail("APS5_VERIFY_COMPILED_SRT: compiled evaluation differs from the generic walk");
    return true;
}

}

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources) {
    static const bool disablePlan = std::getenv("APS5_NO_COMPILED_SRT") != nullptr || std::getenv("APS5_SRT_DEBUG") != nullptr;
    static const bool verify = std::getenv("APS5_VERIFY_COMPILED_SRT") != nullptr;
    const bool compiled = !disablePlan && evaluateFlat && program.executionPlan != nullptr && sources.data() == program.materializationSources.data() && sources.size() == program.materializationSources.size() && std::ranges::none_of(cleanFlatSlots, [](auto clean) { return clean != 0; });
    if (verify) {
        struct Counts { std::uint64_t total = 0, compiled = 0; };
        auto& counts = HostThreadLocal<Counts, Counts>();
        counts.compiled += compiled;
        if ((++counts.total & 0x3fffu) == 0) std::fprintf(stderr, "[srt-plan] %llu/%llu evaluations use compiled plans with generic verification\n", static_cast<unsigned long long>(counts.compiled), static_cast<unsigned long long>(counts.total));
    }
    if (!compiled) return EvaluateRuntimeSourcesGeneric(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources);
    failureReason().clear();
    if (verify) return verifyExecution(program, runtime, results, flat, activeSources);
    return program.executionPlan->Evaluate(runtime, results, flat, activeSources) || Fail("compiled SRT inputs could not be read");
}

const std::string& RuntimeSourceFailureReason() {
    return failureReason();
}

}
