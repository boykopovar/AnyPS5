#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Selectors = 7;
constexpr std::uint32_t UnusedModes = 3;
constexpr std::uint32_t Combinations = Selectors * 2u * Selectors * UnusedModes;
constexpr std::uint32_t BroadcastResult = Combinations;
constexpr std::uint32_t MaxLanes = 64;
constexpr std::uint32_t Inputs = 2;
constexpr std::uint32_t Results = Combinations + 1u;
constexpr std::uint32_t InputIndexVector = 1;
constexpr std::uint32_t OutputIndexVector = 2;
constexpr std::uint32_t BroadcastVector = 3;
constexpr std::uint32_t SourceVector = 4;
constexpr std::uint32_t PreviousVector = 5;
constexpr std::uint32_t DataVector = 6;
constexpr std::uint32_t ThreadVector = 0;
constexpr std::uint32_t ExecLoRegister = 0x7eu;
constexpr std::uint32_t ExecHiRegister = 0x7fu;
constexpr std::uint32_t InputRegister = 0;
constexpr std::uint32_t OutputRegister = 4;
constexpr std::uint32_t ConstantZero = 0x80u;
constexpr std::uint32_t ConstantOne = 0x81u;
constexpr std::uint32_t ConstantAllOnes = 0xc1u;
constexpr std::uint32_t Literal = 0xffu;
constexpr std::uint32_t Sdwa = 0xf9u;
constexpr std::uint32_t VectorSource = 0x100u;
constexpr std::uint32_t Endpgm = 0xbf810000u;
constexpr std::array<std::uint32_t, 2> BroadcastExec{0xa5a5a5a5u, 0x3c3c3c3cu};

template <std::uint32_t WaveSize>
constexpr std::size_t CodeWords = 15u + (WaveSize == 64u ? 5u : 3u) + Combinations * 5u + 3u;

constexpr std::array<std::uint32_t, 32> Sources{
    0x00000000u, 0xffffffffu, 0x80808080u, 0x7f7f7f7fu, 0x000000ffu, 0x0000ff00u, 0x00ff0000u, 0xff000000u,
    0x00000080u, 0x00008000u, 0x00800000u, 0x80000000u, 0x0000007fu, 0x00007fffu, 0x007fffffu, 0x7fffffffu,
    0x01020384u, 0x8504f306u, 0x12345678u, 0x9abcdef0u, 0xfedcba98u, 0x76543210u, 0x00ff00ffu, 0xff00ff00u,
    0x80007fffu, 0x7fff8000u, 0xc3a5815au, 0x5a81a5c3u, 0x0f1e2d3cu, 0xf0e1d2c3u, 0x40c08001u, 0xdeadbeefu,
};

alignas(256) std::array<std::uint32_t, MaxLanes * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxLanes * Results> Output{};

constexpr std::uint32_t Sop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0xbe800000u | (destination << 16u) | (opcode << 8u) | source;
}

constexpr std::uint32_t Vop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0x7e000000u | (destination << 17u) | (opcode << 9u) | source;
}

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::uint32_t SdwaModifier(std::uint32_t source, std::uint32_t destinationSelector, std::uint32_t unused, std::uint32_t sourceSelector, std::uint32_t signExtend) {
    return source | (destinationSelector << 8u) | (unused << 11u) | (sourceSelector << 16u) | (signExtend << 19u);
}

constexpr std::uint32_t BufferAccess(std::uint32_t data, std::uint32_t index, std::uint32_t descriptor) {
    return (ConstantZero << 24u) | ((descriptor / 4u) << 16u) | (data << 8u) | index;
}

constexpr std::uint32_t CombinationIndex(std::uint32_t sourceSelector, std::uint32_t signExtend, std::uint32_t destinationSelector, std::uint32_t unused) {
    return ((sourceSelector * 2u + signExtend) * Selectors + destinationSelector) * UnusedModes + unused;
}

template <std::uint32_t WaveSize>
constexpr std::array<std::uint32_t, CodeWords<WaveSize>> BuildCode() {
    constexpr std::uint32_t sMovB32 = 0x03u;
    constexpr std::uint32_t sMovB64 = 0x04u;
    constexpr std::uint32_t vMovB32 = 0x01u;
    constexpr std::uint32_t vMulU32U24 = 0x0bu;
    constexpr std::uint32_t vLshlrevB32 = 0x1au;
    constexpr std::uint32_t bufferLoadDwordIndexed = 0xe0302000u;
    constexpr std::uint32_t bufferStoreDwordIndexed = 0xe0702000u;
    constexpr std::uint32_t waitVmcnt = 0xbf8c3f70u;
    std::array<std::uint32_t, CodeWords<WaveSize>> code{};
    std::size_t count = 0;
    code[count++] = Vop2(vLshlrevB32, InputIndexVector, ConstantOne, ThreadVector);
    code[count++] = Vop2(vMulU32U24, OutputIndexVector, Literal, ThreadVector);
    code[count++] = Results;
    code[count++] = bufferLoadDwordIndexed;
    code[count++] = BufferAccess(SourceVector, InputIndexVector, InputRegister);
    code[count++] = bufferLoadDwordIndexed | 4u;
    code[count++] = BufferAccess(PreviousVector, InputIndexVector, InputRegister);
    code[count++] = waitVmcnt;
    code[count++] = Vop1(vMovB32, BroadcastVector, VectorSource | SourceVector);
    code[count++] = Sop1(sMovB32, ExecLoRegister, Literal);
    code[count++] = BroadcastExec[0];
    if constexpr (WaveSize == 64u) {
        code[count++] = Sop1(sMovB32, ExecHiRegister, Literal);
        code[count++] = BroadcastExec[1];
    }
    for (std::uint32_t byte = 1; byte < 4u; ++byte) {
        code[count++] = Vop1(vMovB32, BroadcastVector, Sdwa);
        code[count++] = SdwaModifier(BroadcastVector, byte, 2u, 0u, 0u);
    }
    code[count++] = WaveSize == 64u ? Sop1(sMovB64, ExecLoRegister, ConstantAllOnes) : Sop1(sMovB32, ExecLoRegister, ConstantAllOnes);
    for (std::uint32_t sourceSelector = 0; sourceSelector < Selectors; ++sourceSelector) {
        for (std::uint32_t signExtend = 0; signExtend < 2u; ++signExtend) {
            for (std::uint32_t destinationSelector = 0; destinationSelector < Selectors; ++destinationSelector) {
                for (std::uint32_t unused = 0; unused < UnusedModes; ++unused) {
                    code[count++] = Vop1(vMovB32, DataVector, VectorSource | PreviousVector);
                    code[count++] = Vop1(vMovB32, DataVector, Sdwa);
                    code[count++] = SdwaModifier(SourceVector, destinationSelector, unused, sourceSelector, signExtend);
                    code[count++] = bufferStoreDwordIndexed | (CombinationIndex(sourceSelector, signExtend, destinationSelector, unused) * 4u);
                    code[count++] = BufferAccess(DataVector, OutputIndexVector, OutputRegister);
                }
            }
        }
    }
    code[count++] = bufferStoreDwordIndexed | (BroadcastResult * 4u);
    code[count++] = BufferAccess(BroadcastVector, OutputIndexVector, OutputRegister);
    code[count++] = Endpgm;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, CodeWords<32>> Wave32Code = BuildCode<32>();
alignas(256) constexpr std::array<std::uint32_t, CodeWords<64>> Wave64Code = BuildCode<64>();

std::uint32_t Source(std::uint32_t tid) {
    return std::rotl(Sources[tid % 32u], static_cast<int>(tid / 32u) * 8);
}

std::uint32_t Previous(std::uint32_t tid) {
    return 0xabcd1234u ^ (tid * 0x01030507u);
}

bool BroadcastActive(std::uint32_t tid) {
    return ((BroadcastExec[tid / 32u] >> (tid % 32u)) & 1u) != 0u;
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < MaxLanes; ++tid) {
        Input[tid * Inputs] = Source(tid);
        Input[tid * Inputs + 1u] = Previous(tid);
    }
}

std::uint32_t FieldOffset(std::uint32_t selector) {
    return selector < 4u ? selector * 8u : (selector - 4u) * 16u;
}

std::uint32_t FieldMask(std::uint32_t selector) {
    return selector < 4u ? 0xffu : 0xffffu;
}

std::uint32_t SignExtendField(std::uint32_t field, std::uint32_t mask) {
    return (field & ((mask >> 1u) + 1u)) != 0u ? field | ~mask : field;
}

std::uint32_t SelectSource(std::uint32_t value, std::uint32_t selector, bool signExtend) {
    if (selector == 6u) {
        return value;
    }
    const std::uint32_t field = (value >> FieldOffset(selector)) & FieldMask(selector);
    return signExtend ? SignExtendField(field, FieldMask(selector)) : field;
}

std::uint32_t PlaceDestination(std::uint32_t result, std::uint32_t selector, std::uint32_t unused, std::uint32_t previous) {
    if (selector == 6u) {
        return result;
    }
    const std::uint32_t offset = FieldOffset(selector);
    const std::uint32_t mask = FieldMask(selector);
    const std::uint32_t field = result & mask;
    switch (unused) {
        case 0u: return field << offset;
        case 1u: return SignExtendField(field, mask) << offset;
        default: return (previous & ~(mask << offset)) | (field << offset);
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Describe(std::uint32_t sourceSelector, bool signExtend, std::uint32_t destinationSelector, std::uint32_t unused) {
    constexpr const char* selectorNames[Selectors] = {"BYTE_0", "BYTE_1", "BYTE_2", "BYTE_3", "WORD_0", "WORD_1", "DWORD"};
    constexpr const char* unusedNames[UnusedModes] = {"UNUSED_PAD", "UNUSED_SEXT", "UNUSED_PRESERVE"};
    return std::string("v_mov_b32_sdwa dst_sel:") + selectorNames[destinationSelector] + " dst_unused:" + unusedNames[unused] +
        " src0_sel:" + selectorNames[sourceSelector] + (signExtend ? " sext" : "");
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin() + InputRegister);
    std::copy(output.begin(), output.end(), userData.begin() + OutputRegister);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t waveSize, const char* run, bool masked) {
    for (std::uint32_t tid = 0; tid < waveSize; ++tid) {
        const std::uint32_t source = Source(tid);
        const std::uint32_t previous = Previous(tid);
        const auto* out = &Output[tid * Results];
        const auto where = [&](std::uint32_t result) {
            return std::string("sdwa move selectors ") + run + ": lane " + std::to_string(tid) + " source " + Hex(source) + " previous " + Hex(previous) + " result " + Hex(out[result]);
        };
        for (std::uint32_t sourceSelector = 0; sourceSelector < Selectors; ++sourceSelector) {
            for (std::uint32_t signExtend = 0; signExtend < 2u; ++signExtend) {
                const std::uint32_t selected = SelectSource(source, sourceSelector, signExtend != 0u);
                for (std::uint32_t destinationSelector = 0; destinationSelector < Selectors; ++destinationSelector) {
                    for (std::uint32_t unused = 0; unused < UnusedModes; ++unused) {
                        const std::uint32_t index = CombinationIndex(sourceSelector, signExtend, destinationSelector, unused);
                        const std::uint32_t expected = PlaceDestination(selected, destinationSelector, unused, previous);
                        Require(out[index] == expected, where(index) + ", expected " + Hex(expected) + ": " + Describe(sourceSelector, signExtend != 0u, destinationSelector, unused));
                    }
                }
            }
        }
        if (!masked) continue;
        if (BroadcastActive(tid)) {
            const std::uint32_t broadcast = (source & 0xffu) * 0x01010101u;
            Require(out[BroadcastResult] == broadcast, where(BroadcastResult) + ", expected " + Hex(broadcast) + ": in-place v_mov_b32_sdwa v3, v3 dst_sel:BYTE_1/2/3 dst_unused:UNUSED_PRESERVE src0_sel:BYTE_0 must copy byte 0 into the other bytes");
        } else {
            Require(out[BroadcastResult] == source, where(BroadcastResult) + ", expected " + Hex(source) + ": the in-place v_mov_b32_sdwa must not write an inactive lane");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool masked = device->Target().subgroupSize >= 32u;
        if (!masked) std::printf("EXEC-masked broadcast skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
        FillInput();
        Run(*device, Wave32Code, 32, device->Target());
        Check(32, "wave32", masked);
        Run(*device, Wave64Code, 64, device->Target());
        Check(64, "wave64", masked);
        Run(*device, Wave64Code, 64, device->ComputeTarget(32));
        Check(64, "wave64 split", masked);
        std::puts("sdwa move selector tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
