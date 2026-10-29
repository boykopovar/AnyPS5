#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Words = std::vector<std::uint32_t>;

constexpr std::uint32_t PrivateHigh = 0x70000000u;
constexpr std::uint32_t WaitCnt0 = 0xbf8c0070u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t ShiftLaneBy5 = 0x34020085u;
constexpr std::uint32_t Barrier = 0xbf8a0000u;
constexpr std::uint32_t StoreDwordOp = 0x1cu;
constexpr std::uint32_t LoadDwordOp = 0x0cu;
constexpr std::uint32_t AtomicAddOp = 0x32u;
constexpr std::uint32_t ScratchSegment = 1u << 14u;
constexpr std::uint32_t AliasScratchDwords = 4u;
constexpr std::uint32_t ContentionScratchDwords = 1900u;
constexpr std::uint32_t ContentionGroups = 64u;
constexpr std::uint32_t DelayIterations = 100000u;

enum class Form : std::uint8_t { Table, Literal, Computed };
constexpr std::array<const char*, 3> FormNames{"table load", "literal", "computed"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t FlatWord0(std::uint32_t op, bool glc, std::uint32_t offset = 0u) {
    return 0xdc000000u | (op << 18u) | (glc ? 0x10000u : 0u) | offset;
}

std::uint32_t FlatWord1(std::uint32_t vdst, std::uint32_t data, std::uint32_t address) {
    return (vdst << 24u) | (0x7du << 16u) | (data << 8u) | address;
}

void EmitFlat(Words& code, std::uint32_t op, bool glc, std::uint32_t vdst, std::uint32_t data, std::uint32_t address, std::uint32_t offset = 0u) {
    code.push_back(FlatWord0(op, glc, offset));
    code.push_back(FlatWord1(vdst, data, address));
}

void EmitScratch(Words& code, std::uint32_t op, std::uint32_t vdst, std::uint32_t data, std::uint32_t address, std::uint32_t offset = 0u) {
    code.push_back(FlatWord0(op, false, offset) | ScratchSegment);
    code.push_back(FlatWord1(vdst, data, address));
}

void EmitTableLoad(Words& code, std::uint32_t vdst, std::uint32_t offset) {
    code.push_back(0xe0381000u | offset);
    code.push_back(0x80010001u | (vdst << 8u));
}

void EmitOutputStore(Words& code, std::uint32_t vdata, std::uint32_t offset) {
    code.push_back(0xe0701000u | offset);
    code.push_back(0x80000001u | (vdata << 8u));
}

std::uint32_t VMov(std::uint32_t vdst, std::uint32_t source) {
    return 0x7e000000u | (vdst << 17u) | (1u << 9u) | source;
}

std::uint32_t VShiftLeft(std::uint32_t vdst, std::uint32_t amount, std::uint32_t vsrc) {
    return 0x34000000u | (vdst << 17u) | (vsrc << 9u) | (128u + amount);
}

std::uint32_t VAdd(std::uint32_t vdst, std::uint32_t source0, std::uint32_t vsrc1) {
    return 0x4a000000u | (vdst << 17u) | (vsrc1 << 9u) | source0;
}

constexpr std::uint32_t Vgpr(std::uint32_t index) { return 256u + index; }
constexpr std::uint32_t Inline(std::uint32_t value) { return 128u + value; }
constexpr std::uint32_t Literal = 255u;

void EmitDelay(Words& code) {
    code.push_back(0xbe9403ffu);
    code.push_back(DelayIterations);
    code.push_back(0x80948114u);
    code.push_back(0xbf078014u);
    code.push_back(0xbf85fffdu);
}

std::uint32_t OperandA(std::uint32_t lane) {
    return 0x9e3779b1u * (lane + 1u) ^ 0x5a5a0f0fu;
}

std::uint32_t OperandB(std::uint32_t lane) {
    return OperandA(lane) ^ 0xa5a5a5a5u;
}

std::uint32_t InitValue(std::uint32_t lane) {
    return 0x3c6ef372u * (lane + 3u) + 11u;
}

Words AliasKernel(Form form, bool atomic) {
    Words code{ShiftLaneBy5};
    EmitTableLoad(code, 2u, 0u);
    EmitTableLoad(code, 6u, 16u);
    code.push_back(WaitCnt0);
    if (form == Form::Literal) {
        code.push_back(VMov(3u, Literal));
        code.push_back(PrivateHigh);
    } else if (form == Form::Computed) {
        code.push_back(VMov(7u, Literal));
        code.push_back(0x30000000u);
        code.push_back(VAdd(3u, Vgpr(7u), 3u));
    }
    if (atomic) {
        EmitScratch(code, StoreDwordOp, 0u, 6u, 2u);
        code.push_back(WaitCnt0);
        EmitFlat(code, AtomicAddOp, true, 10u, 4u, 2u);
        code.push_back(WaitCnt0);
        EmitScratch(code, LoadDwordOp, 11u, 0u, 2u);
        code.push_back(WaitCnt0);
    } else {
        EmitFlat(code, StoreDwordOp, false, 0u, 4u, 2u);
        code.push_back(WaitCnt0);
        EmitScratch(code, LoadDwordOp, 10u, 0u, 2u);
        EmitScratch(code, StoreDwordOp, 0u, 5u, 2u, 8u);
        code.push_back(WaitCnt0);
        EmitFlat(code, LoadDwordOp, false, 11u, 0u, 2u, 8u);
        code.push_back(WaitCnt0);
    }
    EmitOutputStore(code, 10u, 0u);
    EmitOutputStore(code, 11u, 4u);
    code.push_back(EndProgram);
    return code;
}

void EmitGlobalLaneSetup(Words& code, std::uint32_t threadShift) {
    code.push_back(VMov(7u, 4u));
    code.push_back(VShiftLeft(8u, threadShift, 7u));
    code.push_back(VAdd(9u, Vgpr(8u), 0u));
    code.push_back(VShiftLeft(13u, 2u, 9u));
    code.push_back(VShiftLeft(1u, 4u, 9u));
    code.push_back(VMov(14u, Inline(0u)));
    for (std::uint32_t k = 0; k < 4u; ++k) code.push_back(VAdd(15u + k, Inline(k), 13u));
}

void EmitScratchFill(Words& code) {
    for (std::uint32_t k = 0; k < 4u; ++k) EmitScratch(code, StoreDwordOp, 0u, 15u + k, 14u, 4u * k);
    code.push_back(WaitCnt0);
}

void EmitScratchReadback(Words& code) {
    for (std::uint32_t k = 0; k < 4u; ++k) EmitScratch(code, LoadDwordOp, 20u + k, 0u, 14u, 4u * k);
    code.push_back(WaitCnt0);
    for (std::uint32_t k = 0; k < 4u; ++k) EmitOutputStore(code, 20u + k, 4u * k);
    code.push_back(EndProgram);
}

Words ContentionKernel(std::uint32_t waveSize) {
    Words code;
    EmitGlobalLaneSetup(code, waveSize == 64u ? 7u : 6u);
    EmitScratchFill(code);
    code.push_back(Barrier);
    EmitDelay(code);
    EmitScratchReadback(code);
    return code;
}

Words DivergentExitKernel(std::uint32_t waveSize) {
    Words code;
    EmitGlobalLaneSetup(code, waveSize == 64u ? 7u : 6u);
    code.push_back(0x7c000000u | (0xc1u << 17u) | Inline(waveSize - 1u));
    code.push_back(0xbf870001u);
    code.push_back(EndProgram);
    EmitScratchFill(code);
    EmitDelay(code);
    EmitScratchReadback(code);
    return code;
}

Words CalleeKernel() {
    Words code{ShiftLaneBy5};
    EmitTableLoad(code, 2u, 0u);
    EmitTableLoad(code, 6u, 16u);
    code.push_back(WaitCnt0);
    const auto counterAfterGetpc = code.size() + 1u;
    code.push_back(0xbe941f00u);
    code.push_back(0x8014ff14u);
    const auto literalAt = code.size();
    code.push_back(0u);
    code.push_back(0x82158015u);
    code.push_back(0xbe9e2114u);
    const auto branchAt = code.size();
    code.push_back(0xbf820000u);
    code.at(literalAt) = static_cast<std::uint32_t>((code.size() - counterAfterGetpc) * 4u);
    EmitFlat(code, StoreDwordOp, false, 0u, 4u, 2u);
    code.push_back(WaitCnt0);
    code.push_back(0xbe80201eu);
    code.at(branchAt) |= static_cast<std::uint32_t>(code.size() - branchAt - 1u);
    EmitScratch(code, LoadDwordOp, 10u, 0u, 2u);
    code.push_back(WaitCnt0);
    EmitOutputStore(code, 10u, 0u);
    code.push_back(EndProgram);
    return code;
}

Words PlainKernel() {
    Words code{ShiftLaneBy5};
    EmitTableLoad(code, 2u, 0u);
    code.push_back(WaitCnt0);
    EmitOutputStore(code, 2u, 0u);
    code.push_back(EndProgram);
    return code;
}

struct Facts {
    bool poolBinding = false;
    bool functionArrayAccess = false;
};

Facts Inspect(std::span<const std::uint32_t> words) {
    Facts facts;
    std::set<std::uint32_t> arrays, pointers, variables;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = words[cursor] & 0xffffu;
        Require(count != 0u, "scratch pool: malformed SPIR-V");
        if (op == spv::OpDecorate && words[cursor + 2u] == spv::DecorationDescriptorSet && words[cursor + 3u] == ShaderRecompiler::ScratchPoolDescriptorSet) facts.poolBinding = true;
        if (op == spv::OpTypeArray) arrays.insert(words[cursor + 1u]);
        if (op == spv::OpTypePointer && words[cursor + 2u] == spv::StorageClassFunction && arrays.contains(words[cursor + 3u])) pointers.insert(words[cursor + 1u]);
        if (op == spv::OpVariable && pointers.contains(words[cursor + 1u])) variables.insert(words[cursor + 2u]);
        if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) && variables.contains(words[cursor + 3u])) facts.functionArrayAccess = true;
        cursor += count;
    }
    return facts;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const Words& code, std::vector<std::uint32_t>& userData, std::uint32_t waveSize, std::uint32_t threads, bool groupId, std::uint32_t scratchDwords) {
    const std::span<const std::uint32_t> codeSpan(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(codeSpan.data()), std::as_bytes(codeSpan)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0, {groupId, false, false}, false, 1};
    compute.scratchDwords = scratchDwords;
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(codeSpan.data()), codeSpan, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void RunAlias(AgcDriver::VulkanDevice& device, Form form, bool atomic, std::uint32_t waveSize) {
    const std::string name = std::string("scratch pool alias ") + FormNames[static_cast<std::size_t>(form)] + (atomic ? " atomic" : " store") + " wave" + std::to_string(waveSize);
    std::vector<std::uint32_t> table(waveSize * 8u, 0u);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        table[lane * 8u] = 4u;
        table[lane * 8u + 1u] = form == Form::Table ? PrivateHigh : (form == Form::Computed ? PrivateHigh - 0x30000000u : 0x1234u);
        table[lane * 8u + 2u] = OperandA(lane);
        table[lane * 8u + 3u] = OperandB(lane);
        table[lane * 8u + 4u] = InitValue(lane);
    }
    std::vector<std::uint32_t> output(waveSize * 8u, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    const auto tableDescriptor = BufferDescriptor(table.data(), static_cast<std::uint32_t>(table.size() * 4u));
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin());
    std::copy(tableDescriptor.begin(), tableDescriptor.end(), userData.begin() + 4);
    const auto code = AliasKernel(form, atomic);
    const auto result = Compile(device, code, userData, waveSize, waveSize, false, AliasScratchDwords);
    const auto facts = Inspect(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()));
    Require(facts.poolBinding && result.scratchPoolSlotDwords != 0u, name + ": the scratch program has no scratch pool binding");
    Require(!facts.functionArrayAccess, name + ": the scratch program indexes a Function array");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto first = output[lane * 8u];
        const auto second = output[lane * 8u + 1u];
        const auto expectedFirst = atomic ? InitValue(lane) : OperandA(lane);
        const auto expectedSecond = atomic ? InitValue(lane) + OperandA(lane) : OperandB(lane);
        Require(first == expectedFirst, name + ": lane " + std::to_string(lane) + " first value " + Hex(first) + ", expected " + Hex(expectedFirst));
        Require(second == expectedSecond, name + ": lane " + std::to_string(lane) + " second value " + Hex(second) + ", expected " + Hex(expectedSecond));
    }
}

void RunCallee(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    const std::string name = "scratch pool callee wave" + std::to_string(waveSize);
    std::vector<std::uint32_t> table(waveSize * 8u, 0u);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        table[lane * 8u] = 4u;
        table[lane * 8u + 1u] = PrivateHigh;
        table[lane * 8u + 2u] = OperandA(lane);
    }
    std::vector<std::uint32_t> output(waveSize * 8u, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    const auto tableDescriptor = BufferDescriptor(table.data(), static_cast<std::uint32_t>(table.size() * 4u));
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin());
    std::copy(tableDescriptor.begin(), tableDescriptor.end(), userData.begin() + 4);
    const auto code = CalleeKernel();
    const auto result = Compile(device, code, userData, waveSize, waveSize, false, AliasScratchDwords);
    Require(result.scratchPoolSlotDwords != 0u, name + ": the scratch program has no scratch pool binding");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        Require(output[lane * 8u] == OperandA(lane), name + ": lane " + std::to_string(lane) + " read " + Hex(output[lane * 8u]) + " back, expected " + Hex(OperandA(lane)));
    }
}

void RunPlain(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> table(32u * 8u, 7u);
    std::vector<std::uint32_t> output(32u * 8u, 0u);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    const auto tableDescriptor = BufferDescriptor(table.data(), static_cast<std::uint32_t>(table.size() * 4u));
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin());
    std::copy(tableDescriptor.begin(), tableDescriptor.end(), userData.begin() + 4);
    const auto code = PlainKernel();
    const auto result = Compile(device, code, userData, 32u, 32u, false, 0u);
    const auto facts = Inspect(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()));
    Require(!facts.poolBinding && result.scratchPoolSlotDwords == 0u, "scratch pool: a program without scratch got a scratch pool binding");
}

void RunContention(AgcDriver::VulkanDevice& device, const char* label, const Words& code, std::uint32_t waveSize, bool exitsEarly) {
    const auto threads = waveSize * 2u;
    const auto lanes = ContentionGroups * threads;
    std::vector<std::uint32_t> output(static_cast<std::size_t>(lanes) * 4u, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin());
    const auto result = Compile(device, code, userData, waveSize, threads, true, ContentionScratchDwords);
    const std::string name = std::string("scratch pool ") + label + " wave" + std::to_string(waveSize);
    Require(result.scratchPoolSlotDwords == threads * ContentionScratchDwords, name + ": unexpected slot size " + std::to_string(result.scratchPoolSlotDwords));
    for (std::uint32_t round = 0; round < 2u; ++round) {
        std::fill(output.begin(), output.end(), 0xdeadbeefu);
        device.Dispatch(result, ContentionGroups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            const bool exited = exitsEarly && (lane % threads) < waveSize;
            for (std::uint32_t k = 0; k < 4u; ++k) {
                const auto value = output[static_cast<std::size_t>(lane) * 4u + k];
                const auto expected = exited ? 0xdeadbeefu : lane * 4u + k;
                Require(value == expected, name + ": round " + std::to_string(round) + " lane " + std::to_string(lane) + " dword " + std::to_string(k) + " read " + Hex(value) + ", expected " + Hex(expected));
            }
        }
    }
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_SCRATCH_POOL_MIB", "1");
#else
        setenv("APS5_SCRATCH_POOL_MIB", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool wave64 = device->Target().subgroupSize >= 32u;
        RunPlain(*device);
        for (const std::uint32_t waveSize : {32u, 64u}) {
            if (waveSize == 64u && !wave64) continue;
            for (const Form form : {Form::Table, Form::Literal, Form::Computed}) {
                RunAlias(*device, form, false, waveSize);
                RunAlias(*device, form, true, waveSize);
            }
            RunCallee(*device, waveSize);
            RunContention(*device, "isolation", ContentionKernel(waveSize), waveSize, false);
            RunContention(*device, "divergent exit", DivergentExitKernel(waveSize), waveSize, true);
        }
        std::puts("scratch pool tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
