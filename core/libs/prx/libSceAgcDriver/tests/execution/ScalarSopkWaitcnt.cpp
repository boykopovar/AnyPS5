#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

struct Wait {
    std::uint32_t opcode;
    const char* name;
};

constexpr std::array<Wait, 4> Waits{{
    {0x17u, "s_waitcnt_vscnt"},
    {0x18u, "s_waitcnt_vmcnt"},
    {0x19u, "s_waitcnt_expcnt"},
    {0x1au, "s_waitcnt_lgkmcnt"},
}};

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t OutputRegister = 4;
constexpr std::uint32_t ValueRegister = 8;
constexpr std::uint32_t MaskRegister = 30;
constexpr std::uint32_t BitRegister = 31;
constexpr std::uint32_t NullRegister = 0x7du;
constexpr std::uint32_t ThreadVector = 0;
constexpr std::uint32_t IndexVector = 3;
constexpr std::uint32_t DataVector = 10;
constexpr std::uint32_t ConstantZero = 0x80u;
constexpr std::uint32_t ConstantOne = 0x81u;
constexpr std::uint32_t ConstantTwo = 0x82u;
constexpr std::uint32_t ConstantSix = 0x86u;
constexpr std::uint32_t Endpgm = 0xbf810000u;
constexpr std::uint32_t Value = 0x12345678u;
constexpr std::uint32_t SccKept = 1u;
constexpr std::uint32_t SccRaised = 2u;

constexpr std::array<std::uint32_t, 2> Registers{NullRegister, ValueRegister};
constexpr std::array<std::uint16_t, 3> Levels{0x0000u, 0x003fu, 0xffffu};
constexpr std::size_t Cases = Waits.size() * Registers.size() * Levels.size();
constexpr std::size_t CodeWords = 2u + Cases * 15u;

alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::uint32_t Sop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0xbe800000u | (destination << 16u) | (opcode << 8u) | source;
}

constexpr std::uint32_t Sop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return 0x80000000u | (opcode << 23u) | (destination << 16u) | (source1 << 8u) | source0;
}

constexpr std::uint32_t Sopc(std::uint32_t opcode, std::uint32_t source0, std::uint32_t source1) {
    return 0xbf000000u | (opcode << 16u) | (source1 << 8u) | source0;
}

constexpr std::uint32_t Sopk(std::uint32_t opcode, std::uint32_t reg, std::uint16_t immediate) {
    return 0xb0000000u | (opcode << 23u) | (reg << 16u) | immediate;
}

constexpr std::uint32_t Vop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0x7e000000u | (destination << 17u) | (opcode << 9u) | source;
}

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::array<std::uint32_t, CodeWords> BuildCode() {
    constexpr std::uint32_t sMovB32 = 0x03u;
    constexpr std::uint32_t sCselectB32 = 0x0au;
    constexpr std::uint32_t sOrB32 = 0x10u;
    constexpr std::uint32_t sCmpEqI32 = 0x00u;
    constexpr std::uint32_t sCmpLgI32 = 0x01u;
    constexpr std::uint32_t vMovB32 = 0x01u;
    constexpr std::uint32_t vLshlrevB32 = 0x1au;
    constexpr std::uint32_t bufferStoreDwordIndexed = 0xe0702000u;
    constexpr std::uint32_t storeOperands = (ConstantZero << 24u) | ((OutputRegister / 4u) << 16u) | (DataVector << 8u) | IndexVector;
    std::array<std::uint32_t, CodeWords> code{};
    std::size_t count = 0;
    std::uint32_t result = 0;
    code[count++] = Vop2(vLshlrevB32, IndexVector, ConstantSix, ThreadVector);
    for (const Wait& wait : Waits) {
        for (const std::uint32_t reg : Registers) {
            for (const std::uint16_t level : Levels) {
                code[count++] = Sop1(sMovB32, MaskRegister, ConstantZero);
                code[count++] = Sopc(sCmpEqI32, ValueRegister, ValueRegister);
                code[count++] = Sopk(wait.opcode, reg, level);
                code[count++] = Sop2(sCselectB32, BitRegister, ConstantOne, ConstantZero);
                code[count++] = Sop2(sOrB32, MaskRegister, MaskRegister, BitRegister);
                code[count++] = Sopc(sCmpLgI32, ValueRegister, ValueRegister);
                code[count++] = Sopk(wait.opcode, reg, level);
                code[count++] = Sop2(sCselectB32, BitRegister, ConstantTwo, ConstantZero);
                code[count++] = Sop2(sOrB32, MaskRegister, MaskRegister, BitRegister);
                code[count++] = Vop1(vMovB32, DataVector, MaskRegister);
                code[count++] = bufferStoreDwordIndexed | (result++ * 4u);
                code[count++] = storeOperands;
                code[count++] = Vop1(vMovB32, DataVector, ValueRegister);
                code[count++] = bufferStoreDwordIndexed | (result++ * 4u);
                code[count++] = storeOperands;
            }
        }
    }
    code[count++] = Endpgm;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, CodeWords> WaitCode = BuildCode();

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(ValueRegister + 1u, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + OutputRegister);
    userData[ValueRegister] = Value;
    const std::span<const std::uint32_t> code(WaitCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::uint32_t result = 0;
        for (const Wait& wait : Waits) {
            for (const std::uint32_t reg : Registers) {
                for (const std::uint16_t level : Levels) {
                    const std::string label = "scalar sopk waitcnt: thread " + std::to_string(tid) + " " + wait.name + " register " + Hex(reg) + ", level " + Hex(level);
                    const std::uint32_t mask = Output[tid * Results + result++];
                    const std::uint32_t value = Output[tid * Results + result++];
                    Testing::Require((mask & SccKept) != 0u, label + " cleared SCC");
                    Testing::Require((mask & SccRaised) == 0u, label + " set SCC");
                    Testing::Require(mask == SccKept, label + " left the flags at " + Hex(mask));
                    Testing::Require(value == Value, label + " left s" + std::to_string(ValueRegister) + " at " + Hex(value) + ", expected " + Hex(Value));
                }
            }
        }
    }
}

const Testing::Case scalarSopkWaitcnt{"ScalarSopkWaitcnt_AllWaitVariants_KeepSccAndRegister", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
