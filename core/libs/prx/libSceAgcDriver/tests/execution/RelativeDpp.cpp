#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 5;
constexpr std::uint32_t Stride = 8;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 39> Code{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0x7e1402b0,
    0x7e1602c0, 0xbefc0381, 0x7e0886e9, 0xfac68808, 0x7e1084e9, 0xfac68808, 0x7e0a0309, 0x7e1202a0,
    0x7e1488e9, 0xfac68808, 0x7e0c030b, 0xbefc03ff, 0x00010001, 0x7e1202a0, 0x7e1490e9, 0xfac68808,
    0x7e0e030b, 0xbefc0381, 0x7e1886fa, 0xff00e408, 0xe0701000, 0x80010403, 0xe0701004, 0x80010503,
    0xe0701008, 0x80010603, 0xe070100c, 0x80010703, 0xe0701010, 0x80010c03, 0xbf810000,
};

constexpr std::array<std::uint32_t, Results> Expected{0x20u, 0x10u, 0x20u, 0x20u, 0x20u};

alignas(256) constexpr std::array<std::uint32_t, 17> MaskedNoFiCode{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0xbefc0381,
    0x7e0802ff, 0xdeadbeef, 0xbefe048f, 0x7e0886e9, 0xfacfac08, 0xbefe04c1, 0xe0701000, 0x80010403,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> MaskedFiCode{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0xbefc0381,
    0x7e0802ff, 0xdeadbeef, 0xbefe048f, 0x7e0886ea, 0xfacfac08, 0xbefe04c1, 0xe0701000, 0x80010403,
    0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, ShaderRecompiler::SpirvTarget target) {
    Input.fill(0xdeadbeefu);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
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
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        for (std::uint32_t component = 0; component < Results; ++component) {
            const std::uint32_t actual = Output[tid * Stride + component];
            Require(actual == Expected[component], std::string("relative dpp: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " register " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(Expected[component]));
        }
    }
}

void RunMasked(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, std::span<const std::uint32_t> code, std::uint32_t activeValue, ShaderRecompiler::SpirvTarget target) {
    Input.fill(0xdeadbeefu);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
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
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        const std::uint32_t expected = tid < 4u ? activeValue : 0xdeadbeefu;
        const std::uint32_t actual = Output[tid * Stride];
        Require(actual == expected, std::string("relative dpp masked: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

struct LaneCase {
    bool dpp8;
    std::array<std::uint32_t, 8> selects;
    std::uint32_t control;
    std::uint32_t boundCtrl;
    std::uint32_t bankMask;
    std::uint32_t rowMask;
};

struct RelativeOp {
    std::uint32_t opcode;
    std::uint32_t m0;
    std::uint32_t landing;
    std::uint32_t read;
    const char* name;
};

constexpr std::array<RelativeOp, 4> RelativeOps{{
    {0x42u, 2u, 12u, 4u, "v_movreld_b32"},
    {0x43u, 2u, 10u, 6u, "v_movrels_b32"},
    {0x44u, 2u, 12u, 6u, "v_movrelsd_b32"},
    {0x48u, (3u << 16u) | 2u, 13u, 6u, "v_movrelsd_2_b32"},
}};
constexpr std::uint32_t OldValue = 0xdead0000u;

std::vector<LaneCase> LaneCases() {
    constexpr std::array<std::array<std::uint32_t, 8>, 5> selects{{
        {7, 6, 5, 4, 3, 2, 1, 0}, {1, 0, 3, 2, 5, 4, 7, 6}, {0, 0, 0, 0, 7, 7, 7, 7}, {3, 1, 4, 1, 5, 2, 6, 5}, {0, 1, 2, 3, 4, 5, 6, 7},
    }};
    constexpr std::array<std::uint32_t, 12> controls{0x1bu, 0xe4u, 0x39u, 0x101u, 0x103u, 0x10fu, 0x111u, 0x118u, 0x121u, 0x12fu, 0x140u, 0x141u};
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 3> masks{{{0xfu, 0xfu}, {0x5u, 0xfu}, {0xfu, 0x6u}}};
    std::vector<LaneCase> cases;
    for (const auto& select : selects) cases.push_back({true, select, 0u, 0u, 0xfu, 0xfu});
    for (const auto control : controls) {
        for (std::uint32_t boundCtrl = 0u; boundCtrl < 2u; ++boundCtrl) {
            for (const auto& [bank, row] : masks) cases.push_back({false, {}, control, boundCtrl, bank, row});
        }
    }
    return cases;
}

std::uint32_t LaneValue(std::uint32_t reg, std::uint32_t lane) {
    return (reg << 24u) | ((lane * 0x111u) & 0xffffffu);
}

std::int32_t Dpp16Source(std::uint32_t control, std::uint32_t lane) {
    const auto row = static_cast<std::int32_t>(lane / 16u) * 16;
    const auto index = static_cast<std::int32_t>(lane % 16u);
    const auto amount = static_cast<std::int32_t>(control & 15u);
    if (control < 0x100u) return row + (index & ~3) + static_cast<std::int32_t>((control >> (2u * (lane & 3u))) & 3u);
    if (control >= 0x101u && control <= 0x10fu) return index + amount > 15 ? -1 : row + index + amount;
    if (control >= 0x111u && control <= 0x11fu) return index - amount < 0 ? -1 : row + index - amount;
    if (control >= 0x121u && control <= 0x12fu) return row + (index - amount + 16) % 16;
    if (control == 0x140u) return row + 15 - index;
    return row + (index & 8) + 7 - (index & 7);
}

std::uint32_t LaneExpected(const LaneCase& test, const RelativeOp& op, std::uint32_t lane) {
    if (test.dpp8) return LaneValue(op.read, (lane & ~7u) | test.selects[lane & 7u]);
    const bool enabled = ((test.rowMask >> (lane / 16u)) & 1u) != 0u && ((test.bankMask >> ((lane % 16u) / 4u)) & 1u) != 0u;
    if (!enabled) return OldValue | lane;
    const auto source = Dpp16Source(test.control, lane);
    if (source < 0) return test.boundCtrl != 0u ? 0u : OldValue | lane;
    return LaneValue(op.read, static_cast<std::uint32_t>(source));
}

alignas(256) std::array<std::uint32_t, MaxThreads * 4u> LaneInput{};
alignas(256) std::array<std::uint32_t, MaxThreads * 80u> LaneOutput{};

void RunLaneSelection(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const RelativeOp& op, ShaderRecompiler::SpirvTarget target) {
    const auto cases = LaneCases();
    const auto count = static_cast<std::uint32_t>(cases.size());
    Require(count * MaxThreads <= LaneOutput.size(), "relative dpp lanes: output too small");
    for (std::uint32_t lane = 0u; lane < MaxThreads; ++lane) {
        for (std::uint32_t reg = 0u; reg < 4u; ++reg) LaneInput[lane * 4u + reg] = LaneValue(4u + reg, lane);
    }
    LaneOutput.fill(0xbaadf00du);
    std::vector<std::uint32_t> words{0x34020084u, 0x160600ffu, count * 4u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u};
    for (std::uint32_t index = 0u; index < count; ++index) {
        const auto& test = cases[index];
        std::uint32_t dpp = 4u;
        if (test.dpp8) {
            for (std::uint32_t lane = 0u; lane < 8u; ++lane) dpp |= test.selects[lane] << (8u + 3u * lane);
        } else {
            dpp |= (test.control << 8u) | (test.boundCtrl << 19u) | (test.bankMask << 24u) | (test.rowMask << 28u);
        }
        words.insert(words.end(), {
            0xbefc03ffu, op.m0,
            0x7e0002ffu | (op.landing << 17u), OldValue,
            0x38000100u | (op.landing << 17u) | (op.landing << 9u),
            0x7e140000u | (op.opcode << 9u) | (test.dpp8 ? 0xe9u : 0xfau), dpp,
            0xe0701000u | (index * 4u), 0x80010003u | (op.landing << 8u),
        });
    }
    words.push_back(0xbf810000u);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(LaneInput.data(), static_cast<std::uint32_t>(LaneInput.size() * 4u));
    const auto output = BufferDescriptor(LaneOutput.data(), static_cast<std::uint32_t>(LaneOutput.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
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
    for (std::uint32_t index = 0u; index < count; ++index) {
        const auto& test = cases[index];
        for (std::uint32_t lane = 0u; lane < waveSize; ++lane) {
            const auto expected = LaneExpected(test, op, lane);
            const auto actual = LaneOutput[lane * count + index];
            Require(actual == expected, std::string("relative dpp lanes: ") + op.name + " wave" + std::to_string(waveSize) + " case " + std::to_string(index) +
                (test.dpp8 ? " dpp8" : " dpp16 control " + Hex(test.control) + " bound_ctrl " + std::to_string(test.boundCtrl) + " bank " + Hex(test.bankMask) +
                " row " + Hex(test.rowMask)) + " lane " + std::to_string(lane) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        Run(*device, 32, 32, device->ComputeTarget(32));
        Run(*device, 64, 64, device->ComputeTarget(64));
        if (target.subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", target.subgroupSize);
            return VulkanTestSkipped;
        }
        RunMasked(*device, 32, 32, MaskedNoFiCode, 0u, device->ComputeTarget(32));
        RunMasked(*device, 32, 32, MaskedFiCode, 0x20u, device->ComputeTarget(32));
        for (const auto& op : RelativeOps) RunLaneSelection(*device, 32, op, device->ComputeTarget(32));
        if (target.subgroupSize < 64u) {
            std::printf("skipped, subgroup size %u cannot hold a wave64\n", target.subgroupSize);
        } else {
            RunMasked(*device, 64, 64, MaskedNoFiCode, 0u, device->ComputeTarget(64));
            RunMasked(*device, 64, 64, MaskedFiCode, 0x20u, device->ComputeTarget(64));
            for (const auto& op : RelativeOps) RunLaneSelection(*device, 64, op, device->ComputeTarget(64));
        }
        std::puts("relative dpp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
