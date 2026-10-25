#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtTape.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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
bool evaluateRuntimeSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison, std::uint32_t* nullRootReads) {
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
    std::vector<std::uint8_t> activeFlat;
    if (evaluateFlat) {
        active.assign(program.descriptorSources.size() + program.info.images.size(), 1u);
        activeFlat.assign(program.srtReads.size(), 1u);
    }
    if (evaluateFlat && !program.controlFlow.empty()) {
        for (const auto& block : program.controlFlow) {
            for (const auto source : block.sources) {
                active.at(source) = 0u;
            }
            for (const auto image : block.images) {
                active.at(program.descriptorSources.size() + image) = 0u;
            }
            for (const auto slot : block.flatSlots) activeFlat.at(slot) = 0u;
        }
        std::vector<std::uint8_t> visited(program.controlFlow.size());
        std::vector<std::uint32_t> pending {0};
        InaccessibleRead conditionRead;
        std::uint32_t conditionNullRoots = 0;
        cleanEvaluator.ReportInaccessibleReads(&conditionRead);
        cleanEvaluator.ReportNullRootReads(&conditionNullRoots);
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
            for (const auto image : block.images) {
                active[program.descriptorSources.size() + image] = 1u;
            }
            for (const auto slot : block.flatSlots) activeFlat.at(slot) = 1u;
            std::uint32_t condition = 0;
            TNode conditionValue{};
            const auto nullRootsBefore = conditionNullRoots;
            const bool cleanEvaluable = TNode::Condition(program, index, conditionValue) && runtime.readSpecializationMemory != nullptr && cleanEvaluator.Evaluate(conditionValue, condition) && conditionNullRoots == nullRootsBefore;
            if (cleanEvaluable) {
                pending.push_back(block.successors[condition != 0u ? 0u : 1u]);
            } else {
                pending.insert(pending.end(), block.successors.begin(), block.successors.end());
            }
        }
        cleanEvaluator.ReportInaccessibleReads(nullptr);
        cleanEvaluator.ReportNullRootReads(nullptr);
    }
    InaccessibleRead inaccessible;
    std::uint32_t nullRoots = 0;
    if (evaluateFlat && poison != nullptr) {
        evaluator.ReportInaccessibleReads(&inaccessible);
        cleanEvaluator.ReportInaccessibleReads(&inaccessible);
        if (nullRootReads != nullptr) {
            evaluator.ReportNullRootReads(&nullRoots);
            cleanEvaluator.ReportNullRootReads(&nullRoots);
        }
    }
    std::vector<std::pair<std::uint32_t, InaccessibleRead>> zeroedSources;
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
                inaccessible = {};
                if (!evaluator.Evaluate(TNode::SourceDword(program, sourceIndex, index), value.dwords[index])) {
                    if (inaccessible.read != nullptr) {
                        if (debug) std::fprintf(stderr, "[srt] descriptor source %u reads inaccessible 0x%llx at pc 0x%x: zero words\n", sourceIndex, static_cast<unsigned long long>(inaccessible.address), inaccessible.read->Flags<MemoryFlags>().pc);
                        value.dwords.fill(0u);
                        zeroedSources.emplace_back(sourceIndex, inaccessible);
                        break;
                    }
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
    std::vector<SrtReadPoison> poisoned;
    if (evaluateFlat) {
        flattened.resize(program.srtReads.size());
        for (std::uint32_t slot = 0; slot < program.srtReads.size(); ++slot) {
            const auto& read = program.srtReads[slot];
            if (read.flatOffset < activeFlat.size() && activeFlat[read.flatOffset] == 0u) continue;
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
            inaccessible = {};
            const bool evaluated = read.flatOffset < flattened.size() && selected.Evaluate(TNode::SrtRead(program, slot), flattened[read.flatOffset]);
            if (pure) trace->leaf = nullptr;
            if (!evaluated && inaccessible.read != nullptr) {
                if (debug) std::fprintf(stderr, "[srt] flat offset %u reads inaccessible 0x%llx at pc 0x%x: poisoned\n", read.flatOffset, static_cast<unsigned long long>(inaccessible.address), inaccessible.read->Flags<MemoryFlags>().pc);
                flattened[read.flatOffset] = 0u;
                poisoned.push_back({read.flatOffset, inaccessible.read->Flags<MemoryFlags>().pc, inaccessible.address});
                continue;
            }
            if (!evaluated) {
                return Fail(std::string(clean ? "clean " : "") + "SRT read at flat offset " + std::to_string(read.flatOffset) + ": " + DescribeValue(read.value, 4));
            }
        }
        for (const auto& [sourceIndex, read] : zeroedSources) {
            const auto pc = read.read->template Flags<MemoryFlags>().pc;
            if (std::none_of(poisoned.begin(), poisoned.end(), [&](const SrtReadPoison& entry) { return entry.pc == pc && entry.address == read.address; })) {
                char address[64];
                std::snprintf(address, sizeof(address), "0x%llx at pc 0x%x", static_cast<unsigned long long>(read.address), pc);
                return Fail("descriptor source " + std::to_string(sourceIndex) + " reads inaccessible " + address + ", which no flat SRT slot records");
            }
        }
    }
    results = std::move(evaluated);
    activeSources = std::move(active);
    if (evaluateFlat) {
        flat = std::move(flattened);
    }
    if (poison != nullptr) {
        *poison = std::move(poisoned);
    }
    if (nullRootReads != nullptr) {
        if (debug && nullRoots != 0u) std::fprintf(stderr, "[srt] %u reads through a null user-data pointer: zero words\n", nullRoots);
        *nullRootReads = nullRoots;
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

bool interpretRuntimeSources(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison, std::uint32_t* nullRootReads) {
    if (program.compact.values.empty()) return evaluateRuntimeSources<IrNode>(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources, poison, nullRootReads);
    prefetchCompactPlan(program.compact);
    const bool evaluated = evaluateRuntimeSources<CompactNode>(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources, poison, nullRootReads);
    static const bool verify = std::getenv("APS5_VERIFY_SRT_COMPACT") != nullptr;
    if (verify) {
        const auto compactReason = RuntimeSourceFailureReason();
        SrtRuntime untraced = runtime;
        untraced.readTrace = nullptr;
        std::vector<DescriptorValue> irResults;
        std::vector<std::uint32_t> irFlat = flat;
        std::vector<std::uint8_t> irActive;
        std::vector<SrtReadPoison> irPoison;
        std::uint32_t irNullRoots = 0;
        const bool irEvaluated = evaluateRuntimeSources<IrNode>(program, sources, untraced, irResults, irFlat, evaluateFlat, cleanFlatSlots, irActive, poison != nullptr ? &irPoison : nullptr, nullRootReads != nullptr ? &irNullRoots : nullptr);
        if (evaluated != irEvaluated) throw std::runtime_error("SRT compact walk " + std::string(evaluated ? "succeeded" : "failed (" + compactReason + ")") + " where the IR walk " + (irEvaluated ? "succeeded" : "failed (" + RuntimeSourceFailureReason() + ")"));
        if (evaluated && (results != irResults || (evaluateFlat && flat != irFlat) || activeSources != irActive)) throw std::runtime_error("SRT compact walk differs from the IR walk at " + describeDifference(results, irResults, flat, irFlat));
        if (!evaluated) failureReason() = compactReason;
    }
    return evaluated;
}

bool sameUserDataUse(const SrtUserDataUse& tape, const SrtUserDataUse& interpreted) {
    if (tape.asBase != interpreted.asBase || tape.asValue != interpreted.asValue) return false;
    auto tapeReads = tape.reads;
    auto interpretedReads = interpreted.reads;
    for (auto* reads : {&tapeReads, &interpretedReads}) {
        std::sort(reads->begin(), reads->end());
        reads->erase(std::unique(reads->begin(), reads->end()), reads->end());
    }
    return tapeReads == interpretedReads;
}

void verifyTape(const IrResourcePlan& program, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison, std::uint32_t* nullRootReads, std::size_t leavesBefore, std::size_t readsBefore, const SrtUserDataUse* useBefore) {
    SrtReadTrace expectedTrace;
    SrtUserDataUse expectedUse = useBefore != nullptr ? *useBefore : SrtUserDataUse {};
    SrtRuntime interpreted = runtime;
    interpreted.readTrace = runtime.readTrace != nullptr ? &expectedTrace : nullptr;
    interpreted.userDataUse = runtime.userDataUse != nullptr ? &expectedUse : nullptr;
    std::vector<DescriptorValue> expectedResults;
    std::vector<std::uint32_t> expectedFlat;
    std::vector<std::uint8_t> expectedActive;
    std::vector<SrtReadPoison> expectedPoison;
    std::uint32_t expectedNullRoots = 0;
    const bool ok = interpretRuntimeSources(program, program.materializationSources, interpreted, expectedResults, expectedFlat, true, program.cleanFlatSlots, expectedActive, &expectedPoison, &expectedNullRoots);
    std::string difference;
    if (!ok) difference = "the Evaluator failed: " + failureReason();
    else if (expectedResults != results) difference = "descriptors";
    else if (expectedFlat != flat) difference = "flat slots";
    else if (expectedActive != activeSources) difference = "active sources";
    else if (!expectedPoison.empty() || expectedNullRoots != 0u) difference = "poisoned reads";
    else if (auto* trace = runtime.readTrace; trace != nullptr) {
        auto leaves = std::vector(trace->leaves.begin() + static_cast<std::ptrdiff_t>(leavesBefore), trace->leaves.end());
        auto reads = std::vector(trace->otherReads.begin() + static_cast<std::ptrdiff_t>(readsBefore), trace->otherReads.end());
        auto expectedLeaves = expectedTrace.leaves;
        auto expectedReads = expectedTrace.otherReads;
        std::sort(leaves.begin(), leaves.end());
        std::sort(expectedLeaves.begin(), expectedLeaves.end());
        std::sort(reads.begin(), reads.end());
        reads.erase(std::unique(reads.begin(), reads.end()), reads.end());
        std::sort(expectedReads.begin(), expectedReads.end());
        expectedReads.erase(std::unique(expectedReads.begin(), expectedReads.end()), expectedReads.end());
        if (leaves != expectedLeaves) difference = "traced leaf reads";
        else if (reads != expectedReads) difference = "traced reads";
        if (!difference.empty()) {
            trace->leaves.resize(leavesBefore);
            trace->otherReads.resize(readsBefore);
            trace->leaves.insert(trace->leaves.end(), expectedTrace.leaves.begin(), expectedTrace.leaves.end());
            trace->otherReads.insert(trace->otherReads.end(), expectedTrace.otherReads.begin(), expectedTrace.otherReads.end());
        }
    }
    if (difference.empty() && runtime.userDataUse != nullptr && !sameUserDataUse(*runtime.userDataUse, expectedUse)) difference = "user-data use";
    static std::atomic<std::uint64_t> walks {0};
    static std::atomic<std::uint64_t> differences {0};
    const auto walked = walks.fetch_add(1, std::memory_order_relaxed) + 1;
    if (difference.empty()) {
        bool decade = walked >= 10;
        for (auto rest = walked; decade && rest >= 10; rest /= 10) decade = rest % 10 == 0;
        if (decade) std::fprintf(stderr, "[srt-tape] verify: %llu walks, %llu differed\n", static_cast<unsigned long long>(walked), static_cast<unsigned long long>(differences.load(std::memory_order_relaxed)));
        return;
    }
    if (differences.fetch_add(1, std::memory_order_relaxed) < 20) std::fprintf(stderr, "[srt-tape] verify: shader %016llx: the tape's walk differs from the Evaluator's in its %s\n", static_cast<unsigned long long>(program.shaderHash), difference.c_str());
    if (!ok) return;
    if (runtime.userDataUse != nullptr) *runtime.userDataUse = std::move(expectedUse);
    results = std::move(expectedResults);
    flat = std::move(expectedFlat);
    activeSources = std::move(expectedActive);
    if (poison != nullptr) *poison = std::move(expectedPoison);
    if (nullRootReads != nullptr) *nullRootReads = expectedNullRoots;
}

}

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison, std::uint32_t* nullRootReads) {
    const bool planInputs = evaluateFlat && sources.data() == program.materializationSources.data() && sources.size() == program.materializationSources.size() && cleanFlatSlots.data() == program.cleanFlatSlots.data() && cleanFlatSlots.size() == program.cleanFlatSlots.size();
    if (planInputs) {
        if (const auto* tape = SrtTape::For(program); tape != nullptr) {
            const auto leavesBefore = runtime.readTrace != nullptr ? runtime.readTrace->leaves.size() : 0;
            const auto readsBefore = runtime.readTrace != nullptr ? runtime.readTrace->otherReads.size() : 0;
            std::optional<SrtUserDataUse> useBefore;
            if (SrtTape::Verified() && runtime.userDataUse != nullptr) useBefore = *runtime.userDataUse;
            if (tape->Run(program, runtime, results, flat, activeSources)) {
                if (poison != nullptr) poison->clear();
                if (nullRootReads != nullptr) *nullRootReads = 0;
                if (SrtTape::Verified()) verifyTape(program, runtime, results, flat, activeSources, poison, nullRootReads, leavesBefore, readsBefore, useBefore ? &*useBefore : nullptr);
                return true;
            }
        }
    }
    return interpretRuntimeSources(program, sources, runtime, results, flat, evaluateFlat, cleanFlatSlots, activeSources, poison, nullRootReads);
}

const std::string& RuntimeSourceFailureReason() {
    return failureReason();
}

}
