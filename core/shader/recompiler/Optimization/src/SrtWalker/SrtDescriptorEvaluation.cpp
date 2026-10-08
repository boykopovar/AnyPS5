#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
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

template<typename TNode>
bool evaluateRuntimeSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources) {
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
    BasicEvaluator<TNode> cleanEvaluator(program, cleanRuntime);
    BasicEvaluator<TNode> evaluator(program, runtime, cleanFlatSlots, &cleanEvaluator);
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
            TNode conditionValue{};
            const bool cleanEvaluable = TNode::Condition(program, index, conditionValue) && runtime.readSpecializationMemory != nullptr && cleanEvaluator.Evaluate(conditionValue, condition);
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
                if (!evaluator.Evaluate(TNode::SourceDword(program, sourceIndex, index), value.dwords[index])) {
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
        for (std::uint32_t slot = 0; slot < program.srtReads.size(); ++slot) {
            const auto& read = program.srtReads[slot];
            const bool clean = read.flatOffset < cleanFlatSlots.size() && cleanFlatSlots[read.flatOffset] != 0u;
            auto& selected = clean ? cleanEvaluator : evaluator;
            // A pure slot's raw read is reachable from no root, so it was not evaluated (nor
            // cached) before this loop: its dereference happens here, once, and is recorded as
            // the slot's leaf; reads nested in its address cone land among the other reads.
            auto* trace = runtime.readTrace;
            const bool pure = trace != nullptr && read.flatOffset < program.pureFlatSlots.size() && program.pureFlatSlots[read.flatOffset] != 0u;
            if (pure) {
                trace->leaf = TNode::SrtRead(program, slot).Value();
                trace->leafSlot = read.flatOffset;
            }
            const bool evaluated = read.flatOffset < flattened.size() && selected.Evaluate(TNode::SrtRead(program, slot), flattened[read.flatOffset]);
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

void prefetchCompactPlan(const CompactResourcePlan& compact) {
    const auto prefetch = [](const void* data, std::size_t bytes) {
        const auto* bytesData = static_cast<const char*>(data);
        for (std::size_t offset = 0; offset < std::min<std::size_t>(bytes, 16384u); offset += 64) __builtin_prefetch(bytesData + offset);
    };
    prefetch(compact.values.data(), compact.values.size() * sizeof(CompactPlanValue));
    prefetch(compact.arguments.data(), compact.arguments.size() * sizeof(std::uint32_t));
    prefetch(compact.sourceDwords.data(), compact.sourceDwords.size() * sizeof(std::uint32_t));
    prefetch(compact.srtReads.data(), compact.srtReads.size() * sizeof(std::uint32_t));
}

std::string describeDifference(const std::vector<DescriptorValue>& compactResults, const std::vector<DescriptorValue>& irResults, const std::vector<std::uint32_t>& compactFlat, const std::vector<std::uint32_t>& irFlat) {
    for (std::size_t index = 0; index < std::min(compactResults.size(), irResults.size()); ++index) {
        for (std::size_t dword = 0; dword < compactResults[index].dwords.size(); ++dword) {
            if (compactResults[index].dwords[dword] != irResults[index].dwords[dword]) return "source " + std::to_string(index) + " dword " + std::to_string(dword) + ": compact " + std::to_string(compactResults[index].dwords[dword]) + ", IR " + std::to_string(irResults[index].dwords[dword]);
        }
    }
    for (std::size_t index = 0; index < std::min(compactFlat.size(), irFlat.size()); ++index) {
        if (compactFlat[index] != irFlat[index]) return "flat offset " + std::to_string(index) + ": compact " + std::to_string(compactFlat[index]) + ", IR " + std::to_string(irFlat[index]);
    }
    return "sizes " + std::to_string(compactResults.size()) + "/" + std::to_string(irResults.size()) + " sources, " + std::to_string(compactFlat.size()) + "/" + std::to_string(irFlat.size()) + " flat words";
}

}

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources) {
    if (program.compact.values.empty()) return evaluateRuntimeSources<IrNode>(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources);
    prefetchCompactPlan(program.compact);
    const bool evaluated = evaluateRuntimeSources<CompactNode>(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources);
    static const bool verify = std::getenv("APS5_VERIFY_SRT_COMPACT") != nullptr;
    if (verify) {
        const auto compactReason = RuntimeSourceFailureReason();
        SrtRuntime untraced = runtime;
        untraced.readTrace = nullptr;
        std::vector<DescriptorValue> irResults;
        std::vector<std::uint32_t> irFlat = flat;
        std::vector<std::uint8_t> irActive;
        const bool irEvaluated = evaluateRuntimeSources<IrNode>(program, sources, untraced, irResults, irFlat, evaluateFlat, cleanFlatSlots, irActive);
        if (evaluated != irEvaluated) throw std::runtime_error("SRT compact walk " + std::string(evaluated ? "succeeded" : "failed (" + compactReason + ")") + " where the IR walk " + (irEvaluated ? "succeeded" : "failed (" + RuntimeSourceFailureReason() + ")"));
        if (evaluated && (results != irResults || (evaluateFlat && flat != irFlat) || activeSources != irActive)) throw std::runtime_error("SRT compact walk differs from the IR walk at " + describeDifference(results, irResults, flat, irFlat));
        if (!evaluated) failureReason() = compactReason;
    }
    return evaluated;
}

const std::string& RuntimeSourceFailureReason() {
    return failureReason();
}

}
