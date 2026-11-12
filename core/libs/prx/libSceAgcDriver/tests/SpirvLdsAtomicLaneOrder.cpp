#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

constexpr std::uint32_t AddressZero = 0x7e020280u;
constexpr std::uint32_t DataOne = 0x7e040281u;
constexpr std::uint32_t WaitLds = 0xbf8cc07fu;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t DsAddRtnU32 = 0xd8800000u;
constexpr std::uint32_t DsAddU32 = 0xd8000000u;
constexpr std::uint32_t DsMinRtnU32 = 0xd89c0000u;
constexpr std::uint32_t DsIncRtnU32 = 0xd88c0000u;
constexpr std::uint32_t ReturnOperands = 0x03000201u;
constexpr std::uint32_t PlainOperands = 0x00000201u;
constexpr std::uint32_t DsWriteB32 = 0xd8340000u;
constexpr std::uint32_t WriteReturn = 0x00000301u;
constexpr std::uint32_t DsAddRtnU64 = 0xd9800000u;
constexpr std::uint32_t WideOperands = 0x04000201u;
constexpr std::uint32_t DsWriteB64 = 0xd9340000u;
constexpr std::uint32_t WriteWideReturn = 0x00000401u;
constexpr std::uint32_t DsWrxchgRtnB32 = 0xd8b40000u;
constexpr std::uint32_t DsCmpstRtnB32 = 0xd8c00000u;
constexpr std::uint32_t CompareOperands = 0x03020201u;
constexpr std::uint32_t MoveExecLo = 0xbefe03ffu;
constexpr std::uint32_t MoveExecHi = 0xbeff03ffu;
constexpr std::uint32_t EvenLanes = 0x55555555u;
constexpr std::uint32_t HighPattern = 0x0f0f0f0fu;
constexpr std::uint32_t DeviceMemoryLdsDwords = 16384u;

struct Options {
    std::uint32_t waveSize = 64u;
    bool ordered = true;
    bool arithmetic = true;
    std::uint32_t subgroupSize = 32u;
    std::uint32_t threads = 256u;
    std::uint32_t ldsDwords = 64u;
    bool spanned = false;
    bool moltenVk = false;
};

struct Usage {
    std::map<spv::Op, std::size_t> opcodes;
    std::map<std::pair<spv::Op, std::uint32_t>, std::size_t> groupOperations;
    bool arithmeticCapability = false;
    std::vector<std::uint32_t> words;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Usage Compile(std::initializer_list<std::uint32_t> body, const Options& options) {
    std::vector<std::uint32_t> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    if (options.arithmetic) capabilities.push_back(spv::CapabilityGroupNonUniformArithmetic);
    const std::array<std::uint32_t, 4> userData{};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    std::vector<std::uint32_t> code{AddressZero, DataOne};
    code.insert(code.end(), body);
    code.insert(code.end(), {WaitLds, EndProgram});
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = options.waveSize;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{options.threads, 1u, 1u}, options.ldsDwords, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = options.subgroupSize;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.target.laneOrderedLdsAtomics = options.ordered;
    request.target.spannedWaves = options.spanned;
    request.target.waveLdsMemoryBarriers = options.moltenVk;
    request.target.ldsLockFences = options.moltenVk;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto result = Recompile(request);
    Require(!result.spirv.empty(), "the LDS atomic program did not recompile");
    Usage usage;
    usage.words.assign(result.spirv.Words().begin(), result.spirv.Words().end());
    std::map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t cursor = 5; cursor < usage.words.size();) {
        const auto count = usage.words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(usage.words[cursor] & spv::OpCodeMask);
        Require(count != 0 && cursor + count <= usage.words.size(), "truncated SPIR-V instruction");
        ++usage.opcodes[op];
        if (op == spv::OpCapability && usage.words[cursor + 1u] == spv::CapabilityGroupNonUniformArithmetic) usage.arithmeticCapability = true;
        if (op >= spv::OpGroupNonUniformIAdd && op <= spv::OpGroupNonUniformLogicalXor && count >= 6u) ++usage.groupOperations[{op, usage.words[cursor + 4u]}];
        cursor += count;
    }
    return usage;
}

std::size_t Count(const Usage& usage, spv::Op op) {
    const auto found = usage.opcodes.find(op);
    return found == usage.opcodes.end() ? 0u : found->second;
}

std::size_t Count(const Usage& usage, spv::Op op, spv::GroupOperation operation) {
    const auto found = usage.groupOperations.find({op, static_cast<std::uint32_t>(operation)});
    return found == usage.groupOperations.end() ? 0u : found->second;
}

std::size_t GroupArithmetic(const Usage& usage) {
    std::size_t total = 0;
    for (const auto& [key, count] : usage.groupOperations) total += count;
    return total;
}

struct CfgBlock {
    std::uint32_t label = 0;
    std::vector<std::vector<std::uint32_t>> body;
    std::vector<std::uint32_t> successors;
    std::uint32_t loopMerge = 0;
    std::uint32_t loopContinue = 0;
};

struct CfgFunction {
    std::vector<CfgBlock> blocks;
    std::map<std::uint32_t, std::size_t> index;
};

struct CfgModule {
    std::map<std::uint32_t, std::uint32_t> constants;
    std::vector<CfgFunction> functions;
};

bool Terminator(spv::Op op) {
    return op == spv::OpBranch || op == spv::OpBranchConditional || op == spv::OpSwitch || op == spv::OpReturn || op == spv::OpReturnValue || op == spv::OpKill || op == spv::OpUnreachable || op == spv::OpTerminateInvocation;
}

CfgModule ParseCfg(std::span<const std::uint32_t> words) {
    CfgModule module;
    bool inBlock = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        Require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        const std::vector<std::uint32_t> instruction(words.begin() + static_cast<std::ptrdiff_t>(cursor), words.begin() + static_cast<std::ptrdiff_t>(cursor + count));
        if (op == spv::OpConstant && count == 4u) module.constants[instruction[2]] = instruction[3];
        if (op == spv::OpFunction) module.functions.emplace_back();
        if (op == spv::OpLabel) {
            Require(!module.functions.empty(), "a SPIR-V label outside a function");
            auto& function = module.functions.back();
            function.index[instruction[1]] = function.blocks.size();
            function.blocks.push_back(CfgBlock{instruction[1]});
            inBlock = true;
        } else if (inBlock) {
            auto& block = module.functions.back().blocks.back();
            block.body.push_back(instruction);
            if (op == spv::OpBranch) block.successors = {instruction[1]};
            if (op == spv::OpBranchConditional) block.successors = {instruction[2], instruction[3]};
            if (op == spv::OpSwitch) {
                block.successors = {instruction[2]};
                for (std::size_t operand = 4; operand < instruction.size(); operand += 2) block.successors.push_back(instruction[operand]);
            }
            if (op == spv::OpLoopMerge) {
                block.loopMerge = instruction[1];
                block.loopContinue = instruction[2];
            }
            inBlock = !Terminator(op);
        }
        cursor += count;
    }
    return module;
}

std::map<std::uint32_t, std::set<std::uint32_t>> Dominators(const CfgFunction& function) {
    std::map<std::uint32_t, std::vector<std::uint32_t>> predecessors;
    std::set<std::uint32_t> reachable;
    std::vector<std::uint32_t> work{function.blocks.front().label};
    while (!work.empty()) {
        const auto label = work.back();
        work.pop_back();
        if (!reachable.insert(label).second) continue;
        for (const auto successor : function.blocks.at(function.index.at(label)).successors) work.push_back(successor);
    }
    for (const auto& block : function.blocks) {
        if (!reachable.contains(block.label)) continue;
        for (const auto successor : block.successors) predecessors[successor].push_back(block.label);
    }
    std::map<std::uint32_t, std::set<std::uint32_t>> dominators;
    for (const auto label : reachable) dominators[label] = reachable;
    dominators[function.blocks.front().label] = {function.blocks.front().label};
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& block : function.blocks) {
            if (!reachable.contains(block.label) || block.label == function.blocks.front().label) continue;
            std::set<std::uint32_t> next = reachable;
            for (const auto predecessor : predecessors[block.label]) {
                std::set<std::uint32_t> kept;
                std::set_intersection(next.begin(), next.end(), dominators[predecessor].begin(), dominators[predecessor].end(), std::inserter(kept, kept.begin()));
                next = std::move(kept);
            }
            next.insert(block.label);
            if (next != dominators[block.label]) {
                dominators[block.label] = std::move(next);
                changed = true;
            }
        }
    }
    return dominators;
}

std::set<std::uint32_t> LoopBody(const CfgFunction& function, std::uint32_t header) {
    const auto& head = function.blocks.at(function.index.at(header));
    std::set<std::uint32_t> body;
    std::vector<std::uint32_t> work{header};
    while (!work.empty()) {
        const auto label = work.back();
        work.pop_back();
        if (label == head.loopMerge || !body.insert(label).second) continue;
        for (const auto successor : function.blocks.at(function.index.at(label)).successors) work.push_back(successor);
    }
    return body;
}

bool Contains(const CfgBlock& block, spv::Op op) {
    return std::any_of(block.body.begin(), block.body.end(), [&](const auto& instruction) { return static_cast<spv::Op>(instruction[0] & spv::OpCodeMask) == op; });
}

struct RoundBarrier {
    std::uint32_t scope = 0;
    bool deviceLds = false;
};

std::uint32_t ConstantOf(const CfgModule& module, std::uint32_t id) {
    const auto found = module.constants.find(id);
    Require(found != module.constants.end(), "an OpControlBarrier operand is not a 32-bit constant");
    return found->second;
}

bool OrdersRounds(const CfgModule& module, const CfgBlock& block, const RoundBarrier& expected) {
    const std::uint32_t required = spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask | (expected.deviceLds ? spv::MemorySemanticsUniformMemoryMask : 0u);
    return std::any_of(block.body.begin(), block.body.end(), [&](const auto& instruction) {
        if (static_cast<spv::Op>(instruction[0] & spv::OpCodeMask) != spv::OpControlBarrier) return false;
        const auto execution = ConstantOf(module, instruction[1]);
        const auto memory = ConstantOf(module, instruction[2]);
        const auto semantics = ConstantOf(module, instruction[3]);
        return execution == expected.scope && memory == expected.scope && (semantics & required) == required;
    });
}

std::string Name(std::uint32_t label) {
    return "%" + std::to_string(label);
}

void CheckRoundBarrier(const Usage& usage, spv::Op atomic, std::size_t atomics, const RoundBarrier& expected, const std::string& label) {
    const auto module = ParseCfg(usage.words);
    std::size_t found = 0;
    for (const auto& function : module.functions) {
        if (function.blocks.empty()) continue;
        const auto dominators = Dominators(function);
        const auto dominates = [&](std::uint32_t over, std::uint32_t block) {
            const auto entry = dominators.find(block);
            return entry != dominators.end() && entry->second.contains(over);
        };
        for (const auto& block : function.blocks) {
            if (!Contains(block, atomic) || !dominators.contains(block.label)) continue;
            ++found;
            const auto latchesOf = [&](const CfgBlock& head, const std::set<std::uint32_t>& body) {
                std::vector<std::uint32_t> latches;
                for (const auto member : body) {
                    const auto& successors = function.blocks.at(function.index.at(member)).successors;
                    if (member != head.loopContinue && std::find(successors.begin(), successors.end(), head.loopContinue) != successors.end()) latches.push_back(member);
                }
                return latches;
            };
            std::uint32_t header = 0;
            std::set<std::uint32_t> body;
            for (const auto& candidate : function.blocks) {
                if (candidate.loopMerge == 0u) continue;
                auto loop = LoopBody(function, candidate.label);
                if (!loop.contains(block.label) || !dominates(candidate.label, block.label) || (header != 0u && loop.size() >= body.size())) continue;
                const auto latches = latchesOf(candidate, loop);
                if (latches.empty() || std::any_of(latches.begin(), latches.end(), [&](std::uint32_t latch) { return dominates(block.label, latch); })) continue;
                header = candidate.label;
                body = std::move(loop);
            }
            Require(header != 0u, label + ": the atomic in block " + Name(block.label) + " is not inside a lane loop that runs it only on some rounds");
            const auto& head = function.blocks.at(function.index.at(header));
            const auto latches = latchesOf(head, body);
            std::set<std::uint32_t> seen;
            std::vector<std::uint32_t> work(block.successors.begin(), block.successors.end());
            std::size_t barriers = 0;
            while (!work.empty()) {
                const auto current = work.back();
                work.pop_back();
                if (!seen.insert(current).second) continue;
                Require(current != header && current != head.loopContinue && current != head.loopMerge && body.contains(current), label + ": a path from the atomic in block " + Name(block.label) + " reaches " + Name(current) + " without a round barrier");
                const auto& next = function.blocks.at(function.index.at(current));
                if (OrdersRounds(module, next, expected)) {
                    Require(!dominates(block.label, current), label + ": the round barrier in block " + Name(current) + " sits inside the leader branch");
                    for (const auto latch : latches) Require(dominates(current, latch), label + ": the round barrier in block " + Name(current) + " does not dominate the loop latch " + Name(latch) + ", so some invocations skip it");
                    ++barriers;
                    continue;
                }
                Require(std::find(latches.begin(), latches.end(), current) == latches.end(), label + ": the latch " + Name(current) + " is reached from the atomic without a round barrier with execution scope == memory scope == " + std::to_string(expected.scope));
                work.insert(work.end(), next.successors.begin(), next.successors.end());
            }
            Require(barriers != 0u, label + ": no round barrier follows the atomic in block " + Name(block.label));
        }
    }
    Require(found == atomics, label + ": " + std::to_string(found) + " serialized atomics, expected " + std::to_string(atomics));
}

std::size_t LoopControlBarriers(const Usage& usage, spv::Op atomic) {
    const auto module = ParseCfg(usage.words);
    std::size_t barriers = 0;
    for (const auto& function : module.functions) {
        for (const auto& candidate : function.blocks) {
            if (candidate.loopMerge == 0u) continue;
            const auto loop = LoopBody(function, candidate.label);
            const bool lane = std::any_of(loop.begin(), loop.end(), [&](std::uint32_t member) { return Contains(function.blocks.at(function.index.at(member)), atomic); });
            if (!lane) continue;
            for (const auto member : loop) {
                for (const auto& instruction : function.blocks.at(function.index.at(member)).body) barriers += static_cast<spv::Op>(instruction[0] & spv::OpCodeMask) == spv::OpControlBarrier ? 1u : 0u;
            }
        }
    }
    return barriers;
}

Usage WithoutControlBarriers(Usage usage) {
    for (std::size_t cursor = 5; cursor < usage.words.size();) {
        const auto count = usage.words[cursor] >> spv::WordCountShift;
        if (static_cast<spv::Op>(usage.words[cursor] & spv::OpCodeMask) == spv::OpControlBarrier) {
            for (std::size_t word = 0; word < count; ++word) usage.words[cursor + word] = (1u << spv::WordCountShift) | spv::OpNop;
        }
        cursor += count;
    }
    return usage;
}

void CheckAddAggregates() {
    const auto unordered = Compile({DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, false});
    Require(Count(unordered, spv::OpAtomicIAdd) == 2u && GroupArithmetic(unordered) == 0u, "without lane order, a wave64 ds_add_rtn_u32 on a 32-lane host does not emit one OpAtomicIAdd per half: " + std::to_string(Count(unordered, spv::OpAtomicIAdd)));
    const auto wave64 = Compile({DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, true});
    Require(Count(wave64, spv::OpAtomicIAdd) == 1u, "a lane-ordered wave64 ds_add_rtn_u32 emits " + std::to_string(Count(wave64, spv::OpAtomicIAdd)) + " OpAtomicIAdd, expected one for both halves");
    Require(Count(wave64, spv::OpGroupNonUniformIAdd, spv::GroupOperationExclusiveScan) == 2u && Count(wave64, spv::OpGroupNonUniformIAdd, spv::GroupOperationReduce) == 2u, "a lane-ordered wave64 ds_add_rtn_u32 does not scan and reduce each half");
    Require(Count(wave64, spv::OpLoopMerge) == Count(unordered, spv::OpLoopMerge) + 1u && wave64.arithmeticCapability, "a lane-ordered wave64 ds_add_rtn_u32 has no address loop or no subgroup arithmetic capability");
    const auto wave32 = Compile({DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, true});
    Require(Count(wave32, spv::OpAtomicIAdd) == 1u && Count(wave32, spv::OpGroupNonUniformIAdd, spv::GroupOperationExclusiveScan) == 1u, "a lane-ordered wave32 ds_add_rtn_u32 does not scan once and add once");
}

void CheckUnusedReturnIsUnchanged() {
    const auto unordered = Compile({DsAddU32, PlainOperands}, {64u, false});
    const auto ordered = Compile({DsAddU32, PlainOperands}, {64u, true});
    Require(unordered.words == ordered.words, "lane order changes a ds_add_u32 whose result is unused");
}

void CheckMinScansWithShuffles() {
    const auto ordered = Compile({DsMinRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, true});
    Require(Count(ordered, spv::OpAtomicUMin) == 1u, "a lane-ordered wave64 ds_min_rtn_u32 emits " + std::to_string(Count(ordered, spv::OpAtomicUMin)) + " OpAtomicUMin, expected one");
    Require(Count(ordered, spv::OpGroupNonUniformUMin, spv::GroupOperationReduce) == 2u && Count(ordered, spv::OpGroupNonUniformUMin, spv::GroupOperationExclusiveScan) == 0u && Count(ordered, spv::OpGroupNonUniformUMin, spv::GroupOperationInclusiveScan) == 0u, "a lane-ordered ds_min_rtn_u32 scans with OpGroupNonUniformUMin, which SPIRV-Cross cannot emit for Metal");
}

void CheckUpdatesSerialize() {
    const auto unordered = Compile({DsIncRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, false});
    const auto ordered = Compile({DsIncRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, true});
    Require(Count(ordered, spv::OpAtomicCompareExchange) == Count(unordered, spv::OpAtomicCompareExchange), "a lane-ordered ds_inc_rtn_u32 changes its compare-exchange count");
    Require(Count(ordered, spv::OpLoopMerge) == Count(unordered, spv::OpLoopMerge) + 1u && GroupArithmetic(ordered) == 0u, "a lane-ordered ds_inc_rtn_u32 is not serialized by one lane loop");
}

void CheckLockedProgramsAreUnchanged() {
    const auto unordered = Compile({DsAddRtnU64, WideOperands, WaitLds, DsWriteB64, WriteWideReturn, DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, false});
    const auto ordered = Compile({DsAddRtnU64, WideOperands, WaitLds, DsWriteB64, WriteWideReturn, DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, true});
    Require(unordered.words == ordered.words, "lane order changes a program whose LDS atomics already take the 64-bit workgroup lock");
}

void CheckSplitSubgroups() {
    const auto add = Compile({DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, true, true, 64u});
    Require(Count(add, spv::OpAtomicIAdd) == 1u && Count(add, spv::OpGroupNonUniformIAdd, spv::GroupOperationExclusiveScan) == 1u, "a lane-ordered wave32 ds_add_rtn_u32 on a 64-lane host does not scan once and add once");
    const auto min = Compile({DsMinRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, true, true, 64u, 32u});
    Require(Count(min, spv::OpAtomicUMin) == 1u && GroupArithmetic(min) == 0u && LoopControlBarriers(min, spv::OpAtomicUMin) == 1u, "a lane-ordered wave32 ds_min_rtn_u32 on a 64-lane host, where a shuffle scan would read the other wave's invocations, is not serialized by one lane loop with a round barrier");
}

void CheckSwapRoundsAreOrdered() {
    const std::initializer_list<std::uint32_t> wave32 = {MoveExecLo, EvenLanes, DsWrxchgRtnB32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn};
    const std::initializer_list<std::uint32_t> wave64 = {MoveExecLo, EvenLanes, MoveExecHi, HighPattern, DsWrxchgRtnB32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn};
    Options options;
    options.moltenVk = true;
    options.waveSize = 32u;
    const auto masked32 = Compile(wave32, options);
    CheckRoundBarrier(masked32, spv::OpAtomicExchange, 1u, {spv::ScopeSubgroup}, "masked wave32 ds_wrxchg_rtn_b32, 256 threads");
    bool caught = false;
    try {
        CheckRoundBarrier(WithoutControlBarriers(masked32), spv::OpAtomicExchange, 1u, {spv::ScopeSubgroup}, "stripped");
    } catch (const std::exception&) {
        caught = true;
    }
    Require(caught, "the round barrier check passes a lane loop whose control barriers were removed");
    options.threads = 32u;
    CheckRoundBarrier(Compile(wave32, options), spv::OpAtomicExchange, 1u, {spv::ScopeWorkgroup}, "masked wave32 ds_wrxchg_rtn_b32, one-wave workgroup");
    options.waveSize = 64u;
    options.threads = 256u;
    CheckRoundBarrier(Compile(wave64, options), spv::OpAtomicExchange, 2u, {spv::ScopeSubgroup}, "masked wave64 ds_wrxchg_rtn_b32 on a 32-lane host, 256 threads");
    options.ldsDwords = DeviceMemoryLdsDwords;
    CheckRoundBarrier(Compile(wave64, options), spv::OpAtomicExchange, 2u, {spv::ScopeSubgroup, true}, "masked wave64 ds_wrxchg_rtn_b32, LDS in device memory");
    options.ldsDwords = 64u;
    options.threads = 64u;
    options.spanned = true;
    CheckRoundBarrier(Compile(wave64, options), spv::OpAtomicExchange, 1u, {spv::ScopeWorkgroup}, "masked spanned wave64 ds_wrxchg_rtn_b32");
    options.ldsDwords = DeviceMemoryLdsDwords;
    CheckRoundBarrier(Compile(wave64, options), spv::OpAtomicExchange, 1u, {spv::ScopeWorkgroup, true}, "masked spanned wave64 ds_wrxchg_rtn_b32, LDS in device memory");
}

void CheckOtherRoundsAreOrdered() {
    Options options;
    options.moltenVk = true;
    for (const std::uint32_t waveSize : {32u, 64u}) {
        options.waveSize = waveSize;
        const auto halves = waveSize == 64u ? 2u : 1u;
        const auto suffix = ", wave" + std::to_string(waveSize);
        options.arithmetic = true;
        CheckRoundBarrier(Compile({MoveExecLo, EvenLanes, DsCmpstRtnB32, CompareOperands, WaitLds, DsWriteB32, WriteReturn}, options), spv::OpAtomicCompareExchange, halves, {spv::ScopeSubgroup}, "masked ds_cmpst_rtn_b32" + suffix);
        const auto aggregated = Compile({MoveExecLo, EvenLanes, DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, options);
        Require(Count(aggregated, spv::OpAtomicIAdd) == 1u && LoopControlBarriers(aggregated, spv::OpAtomicIAdd) == 0u, "the aggregated ds_add_rtn_u32" + suffix + " gained a control barrier in its address loop");
        options.arithmetic = false;
        CheckRoundBarrier(Compile({MoveExecLo, EvenLanes, DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, options), spv::OpAtomicIAdd, halves, {spv::ScopeSubgroup}, "masked serialized ds_add_rtn_u32 without subgroup arithmetic" + suffix);
    }
}

void CheckUnscopedWavesTakeThePlainPath() {
    for (const std::uint32_t opcode : {DsWrxchgRtnB32, DsMinRtnU32}) {
        const auto unordered = Compile({opcode, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, false, true, 64u});
        const auto ordered = Compile({opcode, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, true, true, 64u});
        const auto atomic = opcode == DsWrxchgRtnB32 ? spv::OpAtomicExchange : spv::OpAtomicUMin;
        Require(Count(ordered, atomic) == Count(unordered, atomic) && Count(ordered, spv::OpLoopMerge) == Count(unordered, spv::OpLoopMerge) && Count(ordered, spv::OpControlBarrier) == Count(unordered, spv::OpControlBarrier), "a serialized LDS atomic of a wave32 program on a 64-lane host with several waves per workgroup, where no barrier scope covers exactly one wave, does not take the plain path");
    }
}

void CheckWithoutArithmetic() {
    const auto ordered = Compile({DsAddRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {64u, true, false});
    Require(Count(ordered, spv::OpAtomicIAdd) == 2u && GroupArithmetic(ordered) == 0u && !ordered.arithmeticCapability, "without subgroup arithmetic, a lane-ordered ds_add_rtn_u32 does not take the serialized path");
}

}

int main() {
    try {
        CheckAddAggregates();
        CheckUnusedReturnIsUnchanged();
        CheckMinScansWithShuffles();
        CheckUpdatesSerialize();
        CheckLockedProgramsAreUnchanged();
        CheckSplitSubgroups();
        CheckWithoutArithmetic();
        CheckSwapRoundsAreOrdered();
        CheckOtherRoundsAreOrdered();
        CheckUnscopedWavesTakeThePlainPath();
        std::puts("SPIR-V LDS atomic lane order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvLdsAtomicLaneOrder: %s\n", error.what());
        return 1;
    }
}
