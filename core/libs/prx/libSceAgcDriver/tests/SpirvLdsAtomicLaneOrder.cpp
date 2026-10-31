#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <map>
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

struct Options {
    std::uint32_t waveSize = 64u;
    bool ordered = true;
    bool arithmetic = true;
    std::uint32_t subgroupSize = 32u;
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
    request.context.compute = ShaderComputeStageInfo{{256u, 1u, 1u}, 64u, {false, false, false}, false, 1u};
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
    const auto unordered = Compile({DsMinRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, false, true, 64u});
    const auto min = Compile({DsMinRtnU32, ReturnOperands, WaitLds, DsWriteB32, WriteReturn}, {32u, true, true, 64u});
    Require(Count(min, spv::OpAtomicUMin) == 1u && GroupArithmetic(min) == 0u && Count(min, spv::OpLoopMerge) == Count(unordered, spv::OpLoopMerge) + 1u, "a lane-ordered wave32 ds_min_rtn_u32 on a 64-lane host, where a shuffle scan would read the other wave's invocations, is not serialized by one lane loop");
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
        std::puts("SPIR-V LDS atomic lane order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvLdsAtomicLaneOrder: %s\n", error.what());
        return 1;
    }
}
