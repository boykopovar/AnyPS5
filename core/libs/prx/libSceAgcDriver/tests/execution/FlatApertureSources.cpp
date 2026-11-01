#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Words = std::vector<std::uint32_t>;

constexpr std::uint32_t SharedBase = 235u;
constexpr std::uint32_t SharedLimit = 236u;
constexpr std::uint32_t PrivateBase = 237u;
constexpr std::uint32_t PrivateLimit = 238u;
constexpr std::array<std::uint32_t, 4> Sources{SharedBase, SharedLimit, PrivateBase, PrivateLimit};
constexpr std::array<const char*, 4> SourceNames{"src_shared_base", "src_shared_limit", "src_private_base", "src_private_limit"};
constexpr std::array<std::uint64_t, 4> SourceValues{0x8000000000000000ull, 0x80000000ffffffffull, 0x7000000000000000ull, 0x70000000ffffffffull};
constexpr std::uint32_t LaneDwords = 32u;
constexpr std::uint32_t LdsDwords = 256u;
constexpr std::uint32_t ScratchDwords = 4u;
constexpr std::uint32_t ScratchOffset = 4u;
constexpr std::uint32_t SharedData = 0x5eed0000u;
constexpr std::uint32_t PrivateData = 0x51de0000u;
constexpr std::uint32_t WaitCnt0 = 0xbf8c0070u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t InlineZero = 128u;
constexpr std::uint32_t InlineOne = 129u;
constexpr std::uint32_t Literal = 255u;
constexpr std::uint32_t MovB32 = 0x03u;
constexpr std::uint32_t MovB64 = 0x04u;
constexpr std::uint32_t CndmaskOp = 0x01u;
constexpr std::uint32_t LshlrevOp = 0x1au;
constexpr std::uint32_t AddNcOp = 0x25u;
constexpr std::uint32_t FlatStoreDword = 0x1cu;
constexpr std::uint32_t FlatLoadDword = 0x0cu;
constexpr std::uint32_t FlatSegment = 0u;
constexpr std::uint32_t ScratchSegment = 1u;
constexpr std::uint32_t CmpLeU64 = 0xe3u;
constexpr std::uint32_t CmpGeU64 = 0xe6u;
constexpr std::uint32_t LaneOffset = 1u;
constexpr std::uint32_t SharedPointer = 3u;
constexpr std::uint32_t SharedValue = 5u;
constexpr std::uint32_t SharedFlatRead = 6u;
constexpr std::uint32_t PrivatePointer = 7u;
constexpr std::uint32_t PrivateValue = 9u;
constexpr std::uint32_t LdsRead = 10u;
constexpr std::uint32_t ScratchRead = 11u;
constexpr std::uint32_t EdgePointer = 12u;
constexpr std::uint32_t PastPointer = 14u;
constexpr std::uint32_t Result = 16u;
constexpr std::uint32_t PrivateFlatRead = 17u;
constexpr std::uint32_t One = 20u;
constexpr std::uint32_t NarrowReads = 8u;
constexpr std::uint32_t WideReads = 12u;
constexpr std::uint32_t PastHigh = 20u;

struct Compare {
    std::uint32_t op;
    std::uint32_t source;
    std::uint32_t pointer;
    std::uint32_t expected;
    const char* name;
};

constexpr std::array<Compare, 8> Compares{{
    {CmpLeU64, SharedBase, SharedPointer, 1u, "src_shared_base <= shared pointer"},
    {CmpGeU64, SharedLimit, SharedPointer, 1u, "src_shared_limit >= shared pointer"},
    {CmpLeU64, SharedBase, PrivatePointer, 0u, "src_shared_base <= private pointer"},
    {CmpLeU64, PrivateBase, PrivatePointer, 1u, "src_private_base <= private pointer"},
    {CmpGeU64, PrivateLimit, PrivatePointer, 1u, "src_private_limit >= private pointer"},
    {CmpGeU64, PrivateLimit, SharedPointer, 0u, "src_private_limit >= shared pointer"},
    {CmpGeU64, SharedLimit, EdgePointer, 1u, "src_shared_limit >= src_shared_limit"},
    {CmpGeU64, SharedLimit, PastPointer, 0u, "src_shared_limit >= the first address past it"},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t ScalarMove(std::uint32_t opcode, std::uint32_t sdst, std::uint32_t source) {
    return 0xbe800000u | (sdst << 16u) | (opcode << 8u) | source;
}

std::uint32_t VectorMove(std::uint32_t vdst, std::uint32_t source) {
    return 0x7e000200u | (vdst << 17u) | source;
}

std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t vdst, std::uint32_t src0, std::uint32_t vsrc1) {
    return (opcode << 25u) | (vdst << 17u) | (vsrc1 << 9u) | src0;
}

void EmitFlat(Words& code, std::uint32_t segment, std::uint32_t op, std::uint32_t vdst, std::uint32_t data, std::uint32_t address) {
    code.push_back(0xdc000000u | (op << 18u) | (segment << 14u));
    code.push_back((vdst << 24u) | (0x7du << 16u) | (data << 8u) | address);
}

void EmitOutputStore(Words& code, std::uint32_t vdata, std::uint32_t slot) {
    code.push_back(0xe0701000u | (slot * 4u));
    code.push_back(0x80000000u | LaneOffset | (vdata << 8u));
}

Words Kernel() {
    Words code{Vop2(LshlrevOp, LaneOffset, 135u, 0u)};
    for (std::uint32_t index = 0; index < Sources.size(); ++index) {
        code.push_back(ScalarMove(MovB32, NarrowReads + index, Sources[index]));
        code.push_back(ScalarMove(MovB64, WideReads + index * 2u, Sources[index]));
    }
    for (std::uint32_t index = 0; index < 12u; ++index) {
        code.push_back(VectorMove(Result, NarrowReads + index));
        EmitOutputStore(code, Result, index);
    }
    code.push_back(Vop2(LshlrevOp, SharedPointer, 130u, 0u));
    code.push_back(VectorMove(SharedPointer + 1u, WideReads + 1u));
    code.push_back(Vop2(AddNcOp, SharedValue, Literal, 0u));
    code.push_back(SharedData);
    code.push_back(VectorMove(PrivatePointer, InlineZero + ScratchOffset));
    code.push_back(VectorMove(PrivatePointer + 1u, WideReads + 5u));
    code.push_back(Vop2(AddNcOp, PrivateValue, Literal, 0u));
    code.push_back(PrivateData);
    EmitFlat(code, FlatSegment, FlatStoreDword, 0u, SharedValue, SharedPointer);
    EmitFlat(code, FlatSegment, FlatStoreDword, 0u, PrivateValue, PrivatePointer);
    code.push_back(WaitCnt0);
    code.push_back(0xd8d80000u);
    code.push_back((LdsRead << 24u) | SharedPointer);
    EmitFlat(code, ScratchSegment, FlatLoadDword, ScratchRead, 0u, PrivatePointer);
    EmitFlat(code, FlatSegment, FlatLoadDword, SharedFlatRead, 0u, SharedPointer);
    EmitFlat(code, FlatSegment, FlatLoadDword, PrivateFlatRead, 0u, PrivatePointer);
    code.push_back(WaitCnt0);
    EmitOutputStore(code, LdsRead, 12u);
    EmitOutputStore(code, ScratchRead, 13u);
    EmitOutputStore(code, SharedFlatRead, 14u);
    EmitOutputStore(code, PrivateFlatRead, 15u);
    code.push_back(VectorMove(EdgePointer, WideReads + 2u));
    code.push_back(VectorMove(EdgePointer + 1u, WideReads + 3u));
    code.push_back(0x80000000u | (PastHigh << 16u) | (InlineOne << 8u) | (WideReads + 3u));
    code.push_back(VectorMove(PastPointer, InlineZero));
    code.push_back(VectorMove(PastPointer + 1u, PastHigh));
    code.push_back(VectorMove(One, InlineOne));
    for (std::uint32_t index = 0; index < Compares.size(); ++index) {
        const auto& compare = Compares[index];
        code.push_back(0x7c000000u | (compare.op << 17u) | (compare.pointer << 9u) | compare.source);
        code.push_back(Vop2(CndmaskOp, Result, InlineZero, One));
        EmitOutputStore(code, Result, 16u + index);
    }
    for (std::uint32_t index = 0; index < Sources.size(); ++index) {
        code.push_back(0xd5010000u | Result);
        code.push_back(InlineZero | (InlineOne << 9u) | (Sources[index] << 18u));
        EmitOutputStore(code, Result, 24u + index);
    }
    code.push_back(EndProgram);
    return code;
}

void Expect(const std::vector<std::uint32_t>& output, std::uint32_t waveSize, std::uint32_t lane, std::uint32_t slot, std::uint32_t expected, const std::string& what) {
    const auto read = output[lane * LaneDwords + slot];
    Require(read == expected, "flat aperture sources wave" + std::to_string(waveSize) + ": lane " + std::to_string(lane) + " " + what + " read " + Hex(read) + ", expected " + Hex(expected));
}

void Check(const std::vector<std::uint32_t>& output, std::uint32_t waveSize) {
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t index = 0; index < Sources.size(); ++index) {
            const auto value = SourceValues[index];
            const std::string name = SourceNames[index];
            Expect(output, waveSize, lane, index, static_cast<std::uint32_t>(value), "s_mov_b32 of " + name);
            Expect(output, waveSize, lane, 4u + index * 2u, static_cast<std::uint32_t>(value), "s_mov_b64 of " + name + " low");
            Expect(output, waveSize, lane, 5u + index * 2u, static_cast<std::uint32_t>(value >> 32u), "s_mov_b64 of " + name + " high");
            const auto maskBit = waveSize == 32u ? (static_cast<std::uint32_t>(value) >> lane) & 1u : static_cast<std::uint32_t>(value >> lane) & 1u;
            Expect(output, waveSize, lane, 24u + index, maskBit, "v_cndmask_b32 with " + name + " as the lane mask");
        }
        Expect(output, waveSize, lane, 12u, SharedData + lane, "ds_read_b32 after a flat store through src_shared_base");
        Expect(output, waveSize, lane, 13u, PrivateData + lane, "scratch_load_dword after a flat store through src_private_base");
        Expect(output, waveSize, lane, 14u, SharedData + lane, "flat_load_dword through src_shared_base");
        Expect(output, waveSize, lane, 15u, PrivateData + lane, "flat_load_dword through src_private_base");
        for (std::uint32_t index = 0; index < Compares.size(); ++index) {
            Expect(output, waveSize, lane, 16u + index, Compares[index].expected, Compares[index].name);
        }
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    std::vector<std::uint32_t> output(waveSize * LaneDwords, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto descriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    const auto code = Kernel();
    const std::span<const std::uint32_t> codeSpan(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(codeSpan.data()), std::as_bytes(codeSpan)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    compute.scratchDwords = ScratchDwords;
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(codeSpan.data()), codeSpan, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(codeSpan.data()));
    device.WaitIdle();
    Check(output, waveSize);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32u);
        if (device->Target().subgroupSize >= 32u) {
            Run(*device, 64u);
        } else {
            std::printf("skipped wave64, subgroup size %u cannot hold a wave64 in two lanes\n", device->Target().subgroupSize);
        }
        std::puts("flat aperture sources tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
