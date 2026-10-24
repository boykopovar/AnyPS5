#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t CaseDwords = 8;
constexpr std::uint32_t CaseBytes = CaseDwords * 4;

constexpr std::uint32_t Sgpr8 = 8;
constexpr std::uint32_t VccLo = 106;
constexpr std::uint32_t VccHi = 107;
constexpr std::uint32_t ExecLo = 126;
constexpr std::uint32_t Literal = 255;
constexpr std::uint32_t Vgpr = 256;
constexpr std::uint32_t Four = 246;
constexpr std::uint32_t MinusOne = 193;
constexpr std::uint32_t Inline(std::uint32_t value) { return 128u + value; }

constexpr std::uint32_t SMovB32 = 0x03;
constexpr std::uint32_t SMovB64 = 0x04;
constexpr std::uint32_t SNop = 0x00;
constexpr std::uint32_t SEndpgm = 0x01;
constexpr std::uint32_t SWaitcnt = 0x0c;
constexpr std::uint32_t VMovB32 = 0x01;
constexpr std::uint32_t VCvtF32U32 = 0x06;
constexpr std::uint32_t VCmpxEqF32 = 0x12;
constexpr std::uint32_t VCmpLtU32 = 0xc1;
constexpr std::uint32_t VCmpxLtU32 = 0xd1;
constexpr std::uint32_t BufferStoreDword = 0x1c;

constexpr std::uint32_t Sop1(std::uint32_t opcode, std::uint32_t sdst, std::uint32_t ssrc0) {
    return 0xbe800000u | (sdst << 16u) | (opcode << 8u) | ssrc0;
}

constexpr std::uint32_t Sopp(std::uint32_t opcode, std::uint32_t immediate) {
    return 0xbf800000u | (opcode << 16u) | immediate;
}

constexpr std::uint32_t Vop1(std::uint32_t opcode, std::uint32_t vdst, std::uint32_t src0) {
    return 0x7e000000u | (vdst << 17u) | (opcode << 9u) | src0;
}

constexpr std::uint32_t Vopc(std::uint32_t opcode, std::uint32_t src0, std::uint32_t vsrc1) {
    return 0x7c000000u | (opcode << 17u) | (vsrc1 << 9u) | src0;
}

std::vector<std::uint32_t> Vop3Compare(std::uint32_t opcode, std::uint32_t sdst, std::uint32_t src0, std::uint32_t src1) {
    return {0xd0000000u | (opcode << 17u) | sdst, src0 | (src1 << 9u)};
}

constexpr std::array<std::uint32_t, 2> Mubuf(std::uint32_t opcode, std::uint32_t vdata, std::uint32_t vaddr, std::uint32_t offset) {
    return {0xe0000000u | (opcode << 18u) | (1u << 12u) | offset, vaddr | (vdata << 8u) | (Inline(0) << 24u)};
}

constexpr std::uint64_t AllLanes = ~0ull;
constexpr std::uint64_t LowHalf = 0x00000000ffffffffull;
constexpr std::uint64_t Quarters = 0x0000ffff0000ffffull;
constexpr std::uint64_t Lane4 = 0x0000000000000010ull;
constexpr std::uint64_t GcnBufferBaseLimit = 1ull << 44u;
constexpr std::uintptr_t LowAddressHint = std::uintptr_t{1} << 40u;
constexpr std::uint64_t VccPreset = 0x2222222211111111ull;
constexpr std::uint64_t SdstPreset = 0x4444444433333333ull;

struct Case {
    const char* name;
    std::uint64_t exec;
    std::vector<std::uint32_t> compare;
    std::uint64_t expectedExec;
    std::uint64_t expectedVcc;
    std::uint64_t expectedSdst;
};

const std::array<Case, 7> Cases{{
    {"v_cmpx_lt_u32_e32 vcc, v0, 32", AllLanes, {Vopc(VCmpxLtU32, Vgpr + 0, 1)}, LowHalf, LowHalf, SdstPreset},
    {"v_cmpx_lt_u32_e64 s[8:9], v0, 32", AllLanes, Vop3Compare(VCmpxLtU32, Sgpr8, Vgpr + 0, Vgpr + 1), LowHalf, VccPreset, LowHalf},
    {"v_cmpx_eq_f32_e32 vcc, 4.0, f32(v0)", AllLanes, {Vopc(VCmpxEqF32, Four, 2)}, Lane4, Lane4, SdstPreset},
    {"v_cmpx_lt_u32_e32 vcc, v0, 64 under exec 0000ffff0000ffff", Quarters, {Vopc(VCmpxLtU32, Vgpr + 0, 4)}, Quarters, Quarters, SdstPreset},
    {"v_cmpx_lt_u32_e64 s[8:9], v0, 64 under exec 0000ffff0000ffff", Quarters, Vop3Compare(VCmpxLtU32, Sgpr8, Vgpr + 0, Vgpr + 4), Quarters, VccPreset, Quarters},
    {"v_cmpx_lt_u32_e64 vcc, v0, 32", AllLanes, Vop3Compare(VCmpxLtU32, VccLo, Vgpr + 0, Vgpr + 1), LowHalf, LowHalf, SdstPreset},
    {"v_cmp_lt_u32_e64 s[8:9], v0, 32", AllLanes, Vop3Compare(VCmpLtU32, Sgpr8, Vgpr + 0, Vgpr + 1), AllLanes, VccPreset, LowHalf},
}};

constexpr std::size_t OutputDwords = Cases.size() * CaseDwords;

std::span<std::uint32_t> LowOutput() {
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(reinterpret_cast<void*>(LowAddressHint), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (block == nullptr) block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = mmap(reinterpret_cast<void*>(LowAddressHint), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) block = nullptr;
#endif
    Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) + bytes <= GcnBufferBaseLimit, "GCN compare EXEC writes: cannot allocate the output below the 44-bit GCN buffer base limit");
    return {static_cast<std::uint32_t*>(block), OutputDwords};
}

std::vector<std::uint32_t> Program() {
    std::vector<std::uint32_t> code;
    const auto emit = [&](auto... words) { (code.push_back(words), ...); };
    const auto movConstant = [&](std::uint32_t sdst, std::uint32_t value) { emit(Sop1(SMovB32, sdst, Literal), value); };
    emit(Vop1(VMovB32, 1, Inline(32)));
    emit(Vop1(VMovB32, 4, Inline(64)));
    emit(Vop1(VCvtF32U32, 2, Vgpr + 0));
    movConstant(16, static_cast<std::uint32_t>(Quarters));
    movConstant(17, static_cast<std::uint32_t>(Quarters >> 32u));
    for (std::uint32_t index = 0; index < Cases.size(); ++index) {
        const auto& item = Cases[index];
        movConstant(VccLo, static_cast<std::uint32_t>(VccPreset));
        movConstant(VccHi, static_cast<std::uint32_t>(VccPreset >> 32u));
        movConstant(Sgpr8, static_cast<std::uint32_t>(SdstPreset));
        movConstant(Sgpr8 + 1, static_cast<std::uint32_t>(SdstPreset >> 32u));
        if (item.exec != AllLanes) emit(Sop1(SMovB64, ExecLo, 16));
        for (const auto word : item.compare) emit(word);
        emit(Sopp(SNop, 4));
        emit(Sop1(SMovB64, 10, ExecLo));
        emit(Sop1(SMovB64, 12, VccLo));
        emit(Sop1(SMovB64, 14, Sgpr8));
        emit(Sop1(SMovB64, ExecLo, MinusOne));
        const auto base = index * CaseBytes;
        if (base <= 64u) emit(Vop1(VMovB32, 11, Inline(base)));
        else emit(Vop1(VMovB32, 11, Literal), base);
        for (std::uint32_t dword = 0; dword < 6; ++dword) emit(Vop1(VMovB32, 12 + dword, 10 + dword));
        for (std::uint32_t dword = 0; dword < 6; ++dword) {
            const auto store = Mubuf(BufferStoreDword, 12 + dword, 11, dword * 4u);
            emit(store[0], store[1]);
        }
    }
    emit(Sopp(SWaitcnt, 0));
    emit(Sopp(SEndpgm, 0));
    return code;
}

std::array<std::uint32_t, 4> GcnBufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x27facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<std::uint32_t> output, const ShaderRecompiler::SpirvTarget& target) {
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    const auto descriptor = GcnBufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
    const std::vector<std::uint32_t> userData(descriptor.begin(), descriptor.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Hex(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Check(std::span<const std::uint32_t> output, const char* run) {
    for (std::uint32_t index = 0; index < Cases.size(); ++index) {
        const auto& item = Cases[index];
        const auto pair = [&](std::uint32_t dword) {
            return static_cast<std::uint64_t>(output[index * CaseDwords + dword]) | (static_cast<std::uint64_t>(output[index * CaseDwords + dword + 1]) << 32u);
        };
        const auto expect = [&](const char* what, std::uint64_t actual, std::uint64_t expected) {
            Require(actual == expected, std::string("GCN compare EXEC writes ") + run + ": " + item.name + " left " + what + " " + Hex(actual) + ", expected " + Hex(expected));
        };
        expect("exec", pair(0), item.expectedExec);
        expect("vcc", pair(2), item.expectedVcc);
        expect("s[8:9]", pair(4), item.expectedSdst);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, the device's subgroups are narrower than the 32 lanes a wave64 run needs (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        ShaderRecompiler::SetGcnGuestShaders(true);
        const auto code = Program();
        const auto output = LowOutput();
        Run(*device, code, output, device->Target());
        Check(output, "wave64");
        Run(*device, code, output, device->ComputeTarget(32));
        Check(output, "wave64 split");
        std::puts("GCN compare EXEC writes tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
