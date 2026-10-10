#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
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

}

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources, std::vector<SrtReadPoison>* poison, std::uint32_t* nullRootReads) {
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
                if (!evaluator.Evaluate(source->dwords[index], value.dwords[index])) {
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
            inaccessible = {};
            const bool evaluated = read.flatOffset < flattened.size() && selected.Evaluate(read.value, flattened[read.flatOffset]);
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
            const auto pc = read.read->Flags<MemoryFlags>().pc;
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

const std::string& RuntimeSourceFailureReason() {
    return failureReason();
}

}
