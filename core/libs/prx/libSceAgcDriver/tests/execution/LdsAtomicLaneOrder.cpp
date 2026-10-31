#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Groups = 128;
constexpr std::uint32_t MaxThreads = 256;
constexpr std::uint32_t Planes = 4;
constexpr std::uint32_t PlaneDwords = MaxThreads;
constexpr std::uint32_t GroupDwords = Planes * PlaneDwords;
constexpr std::uint32_t Inactive = 0x80000000u;
constexpr std::uint32_t Untouched = 0xdeadbeefu;

alignas(256) std::array<std::uint32_t, Groups * GroupDwords> Input{};
alignas(256) std::array<std::uint32_t, Groups * GroupDwords> Output{};

struct alignas(256) Program {
    std::array<std::uint32_t, 64> words{};
};

struct Operation {
    const char* name;
    std::uint32_t opcode;
    bool wide;
};

constexpr std::array<Operation, 17> Operations{{
    {"ds_add_rtn_u32", 0x20u, false},
    {"ds_sub_rtn_u32", 0x21u, false},
    {"ds_rsub_rtn_u32", 0x22u, false},
    {"ds_inc_rtn_u32", 0x23u, false},
    {"ds_dec_rtn_u32", 0x24u, false},
    {"ds_min_rtn_i32", 0x25u, false},
    {"ds_max_rtn_i32", 0x26u, false},
    {"ds_min_rtn_u32", 0x27u, false},
    {"ds_max_rtn_u32", 0x28u, false},
    {"ds_and_rtn_b32", 0x29u, false},
    {"ds_or_rtn_b32", 0x2au, false},
    {"ds_xor_rtn_b32", 0x2bu, false},
    {"ds_mskor_rtn_b32", 0x2cu, false},
    {"ds_wrxchg_rtn_b32", 0x2du, false},
    {"ds_cmpst_rtn_b32", 0x30u, false},
    {"ds_wrap_rtn_b32", 0x34u, false},
    {"ds_add_rtn_u64", 0x60u, true},
}};

constexpr Program AtomicProgram(std::uint32_t opcode, std::uint32_t waveSize, bool masked) {
    const bool wave64 = waveSize == 64u;
    constexpr std::uint32_t Nop = 0xbf800000u;
    const std::array<std::uint32_t, 37> body{
        0x340a0082u,
        0x8f098c08u,
        0xe0301000u, 0x09000105u,
        0xe0301400u, 0x09000205u,
        0xe0301800u, 0x09000305u,
        0xe0301c00u, 0x09000405u,
        0x7e0c02ffu, Untouched,
        0x7e0e0280u,
        0xbf8c3f70u,
        0xd8340000u, 0x00000405u,
        0xbf8cc07fu,
        0xbf8a0000u,
        masked ? 0x7d060280u : Nop,
        !masked ? Nop : wave64 ? 0xbe8a246au : 0xbe8a3c6au,
        0x34020282u,
        0xd8000000u | (opcode << 18u), 0x06030201u,
        !masked ? Nop : wave64 ? 0xbefe040au : 0xbefe030au,
        0xbf8cc07fu,
        0xbf8a0000u,
        0xd8d80000u, 0x08000005u,
        0xbf8cc07fu,
        0xe0701000u, 0x09010605u,
        0xe0701400u, 0x09010805u,
        0xe0701800u, 0x09010705u,
        0xbf810000u,
    };
    Program program;
    for (std::size_t index = 0; index < body.size(); ++index) program.words[index] = body[index];
    for (std::size_t index = body.size(); index < program.words.size(); ++index) program.words[index] = 0xbf810000u;
    return program;
}

constexpr std::array<std::uint32_t, 2> WaveSizes{64u, 32u};

constexpr std::size_t ProgramsPerWave = Operations.size() + 1u;

constexpr std::array<Program, ProgramsPerWave * WaveSizes.size()> BuildPrograms() {
    std::array<Program, ProgramsPerWave * WaveSizes.size()> programs{};
    for (std::size_t wave = 0; wave < WaveSizes.size(); ++wave) {
        for (std::size_t operation = 0; operation < Operations.size(); ++operation) programs[wave * ProgramsPerWave + operation] = AtomicProgram(Operations[operation].opcode, WaveSizes[wave], true);
        programs[wave * ProgramsPerWave + Operations.size()] = AtomicProgram(Operations[0].opcode, WaveSizes[wave], false);
    }
    return programs;
}

alignas(256) constexpr std::array<Program, ProgramsPerWave * WaveSizes.size()> Programs = BuildPrograms();

const Program& ProgramFor(std::size_t operation, std::uint32_t waveSize) {
    return Programs[(waveSize == 64u ? 0u : 1u) * ProgramsPerWave + operation];
}

const Program& UnmaskedAdd(std::uint32_t waveSize) {
    return ProgramFor(Operations.size(), waveSize);
}

std::uint32_t Mix(std::uint32_t value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

std::uint32_t Random(std::uint32_t group, std::uint32_t thread, std::uint32_t salt) {
    return Mix(group * 0x9e3779b9u + thread * 0x85ebca6bu + salt * 0xc2b2ae35u + 0x27d4eb2fu);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t& At(std::array<std::uint32_t, Groups * GroupDwords>& data, std::uint32_t group, std::uint32_t plane, std::uint32_t thread) {
    return data[group * GroupDwords + plane * PlaneDwords + thread];
}

struct Lane {
    std::uint32_t address = 0;
    std::uint32_t data0 = 0;
    std::uint32_t data1 = 0;
    std::uint32_t init = 0;
    bool active = true;
};

void Dispatch(AgcDriver::VulkanDevice& device, const Program& program, std::uint32_t waveSize, std::uint32_t threads, const ShaderRecompiler::SpirvTarget& target, const std::function<Lane(std::uint32_t, std::uint32_t)>& lane) {
    Input.fill(0u);
    Output.fill(0xfeedfaceu);
    for (std::uint32_t group = 0; group < Groups; ++group) {
        for (std::uint32_t thread = 0; thread < threads; ++thread) {
            const auto values = lane(group, thread);
            At(Input, group, 0, thread) = values.address | (values.active ? 0u : Inactive);
            At(Input, group, 1, thread) = values.data0;
            At(Input, group, 2, thread) = values.data1;
            At(Input, group, 3, thread) = values.init;
        }
    }
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(program.words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, MaxThreads, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, Groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Join(std::span<const std::uint32_t> values) {
    std::string text;
    for (std::size_t index = 0; index < values.size(); ++index) text += std::to_string(values[index]) + (index + 1 < values.size() ? "," : "");
    return text;
}

std::uint32_t MeasureOrder(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, std::uint32_t waveSize, std::uint32_t threads, bool masked, const char* label) {
    Dispatch(device, masked ? ProgramFor(0, waveSize) : UnmaskedAdd(waveSize), waveSize, threads, target, [&](std::uint32_t, std::uint32_t thread) {
        return Lane{(thread / waveSize) * waveSize, 1u, 0u, 0u, true};
    });
    std::uint32_t waves = 0;
    std::uint32_t ordered = 0;
    std::uint32_t halves = 0;
    std::map<std::vector<std::uint32_t>, std::uint32_t> maps;
    for (std::uint32_t group = 0; group < Groups; ++group) {
        for (std::uint32_t wave = 0; wave < threads / waveSize; ++wave) {
            std::vector<std::uint32_t> ranks(waveSize);
            for (std::uint32_t lane = 0; lane < waveSize; ++lane) ranks[lane] = At(Output, group, 0, wave * waveSize + lane);
            ++waves;
            bool inOrder = true;
            bool halfOrder = true;
            for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
                inOrder = inOrder && ranks[lane] == lane;
                halfOrder = halfOrder && (ranks[lane] < 32u) == (lane < 32u);
            }
            ordered += inOrder ? 1u : 0u;
            halves += halfOrder ? 1u : 0u;
            ++maps[ranks];
        }
    }
    std::printf("%s: same-address ds_add_rtn_u32%s, wave%u, %u threads: %u waves, %u in lane order", label, masked ? " under an EXEC mask" : "", waveSize, threads, waves, ordered);
    if (waveSize == 64u) std::printf(", %u with lanes 0-31 ranked before lanes 32-63", halves);
    std::printf(", %zu distinct lane-to-rank maps\n", maps.size());
    std::vector<std::pair<std::uint32_t, const std::vector<std::uint32_t>*>> common;
    for (const auto& [ranks, count] : maps) common.emplace_back(count, &ranks);
    std::sort(common.begin(), common.end(), [](const auto& left, const auto& right) { return left.first > right.first; });
    for (std::size_t index = 0; index < std::min<std::size_t>(common.size(), 3u); ++index) std::printf("  %u waves: %s\n", common[index].first, Join(*common[index].second).c_str());
    return waves - ordered;
}

std::uint32_t CheckRanks(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const char* label) {
    constexpr std::uint32_t Digits = 16;
    const auto digit = [](std::uint32_t group, std::uint32_t thread) { return Random(group, thread, 1u) % Digits; };
    Dispatch(device, UnmaskedAdd(64u), 64u, MaxThreads, target, [&](std::uint32_t group, std::uint32_t thread) {
        return Lane{digit(group, thread), 1u << (8u * (thread / 64u)), 0u, 0u, true};
    });
    std::uint32_t wrong = 0;
    std::uint32_t wrongTotals = 0;
    for (std::uint32_t group = 0; group < Groups; ++group) {
        std::array<std::array<std::uint32_t, Digits>, 4> counts{};
        for (std::uint32_t thread = 0; thread < MaxThreads; ++thread) {
            const auto wave = thread / 64u;
            const auto key = digit(group, thread);
            const auto rank = (At(Output, group, 0, thread) >> (8u * wave)) & 0xffu;
            wrong += rank == counts[wave][key] ? 0u : 1u;
            ++counts[wave][key];
        }
        for (std::uint32_t key = 0; key < Digits; ++key) {
            std::uint32_t total = 0;
            for (std::uint32_t wave = 0; wave < 4u; ++wave) total |= counts[wave][key] << (8u * wave);
            wrongTotals += At(Output, group, 1, key) == total ? 0u : 1u;
        }
    }
    std::printf("%s: byte-packed per-wave ranks, wave64, %u threads, %u digits: %u of %u lanes ranked out of lane order, %u of %u digit totals wrong\n", label, MaxThreads, Digits, wrong, Groups * MaxThreads, wrongTotals, Groups * Digits);
    return wrong + wrongTotals;
}

std::uint64_t Apply(std::uint32_t opcode, std::uint64_t old, std::uint32_t data0, std::uint32_t data1) {
    const auto low = static_cast<std::uint32_t>(old);
    const auto signedLow = static_cast<std::int32_t>(low);
    const auto signedData = static_cast<std::int32_t>(data0);
    switch (opcode) {
    case 0x20u: return low + data0;
    case 0x21u: return low - data0;
    case 0x22u: return data0 - low;
    case 0x23u: return low >= data0 ? 0u : low + 1u;
    case 0x24u: return low == 0u || low > data0 ? data0 : low - 1u;
    case 0x25u: return static_cast<std::uint32_t>(std::min(signedLow, signedData));
    case 0x26u: return static_cast<std::uint32_t>(std::max(signedLow, signedData));
    case 0x27u: return std::min(low, data0);
    case 0x28u: return std::max(low, data0);
    case 0x29u: return low & data0;
    case 0x2au: return low | data0;
    case 0x2bu: return low ^ data0;
    case 0x2cu: return (low & ~data0) | data1;
    case 0x2du: return data0;
    case 0x30u: return low == data0 ? data1 : low;
    case 0x34u: return low >= data0 ? low - data0 : low + data1;
    case 0x60u: return old + ((static_cast<std::uint64_t>(data1) << 32u) | data0);
    default: throw std::runtime_error("lds atomic lane order: no reference for opcode " + std::to_string(opcode));
    }
}

Lane OperationLane(const Operation& operation, std::uint32_t waveSize, std::uint32_t group, std::uint32_t thread) {
    const auto wave = thread / waveSize;
    const auto pick = Random(group, thread, 2u);
    Lane lane;
    lane.address = wave * waveSize + (operation.wide ? 2u * (pick % 4u) : pick % 8u);
    lane.active = Random(group, thread, 3u) % 8u != 0u;
    lane.data0 = Random(group, thread, 4u);
    lane.data1 = Random(group, thread, 5u);
    lane.init = Random(group, thread, 6u);
    switch (operation.opcode) {
    case 0x23u:
    case 0x24u:
        lane.init %= 24u;
        lane.data0 %= 28u;
        break;
    case 0x29u:
        lane.init = 0xffffffffu;
        lane.data0 = ~(1u << (lane.data0 % 32u));
        break;
    case 0x2au:
        lane.init = 0u;
        lane.data0 = 1u << (lane.data0 % 32u);
        break;
    case 0x30u:
        lane.init = 0u;
        lane.data0 %= 4u;
        lane.data1 = lane.data0 + 1u;
        break;
    case 0x34u:
        lane.init %= 32u;
        lane.data0 = 1u + lane.data0 % 40u;
        lane.data1 %= 8u;
        break;
    default:
        break;
    }
    return lane;
}

std::uint32_t CheckOperation(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, std::size_t index, std::uint32_t waveSize, const char* label) {
    const auto& operation = Operations[index];
    Dispatch(device, ProgramFor(index, waveSize), waveSize, MaxThreads, target, [&](std::uint32_t group, std::uint32_t thread) { return OperationLane(operation, waveSize, group, thread); });
    std::uint32_t wrongReturns = 0;
    std::uint32_t wrongMemory = 0;
    std::string example;
    for (std::uint32_t group = 0; group < Groups; ++group) {
        std::array<std::uint32_t, MaxThreads> memory{};
        for (std::uint32_t thread = 0; thread < MaxThreads; ++thread) memory[thread] = OperationLane(operation, waveSize, group, thread).init;
        for (std::uint32_t thread = 0; thread < MaxThreads; ++thread) {
            const auto lane = OperationLane(operation, waveSize, group, thread);
            std::uint32_t expectedLow = Untouched;
            std::uint32_t expectedHigh = 0u;
            if (lane.active) {
                const std::uint64_t old = memory[lane.address] | (operation.wide ? static_cast<std::uint64_t>(memory[lane.address + 1u]) << 32u : 0u);
                const auto next = Apply(operation.opcode, old, lane.data0, lane.data1);
                memory[lane.address] = static_cast<std::uint32_t>(next);
                if (operation.wide) memory[lane.address + 1u] = static_cast<std::uint32_t>(next >> 32u);
                expectedLow = static_cast<std::uint32_t>(old);
                expectedHigh = static_cast<std::uint32_t>(old >> 32u);
            }
            const bool same = At(Output, group, 0, thread) == expectedLow && (!operation.wide || !lane.active || At(Output, group, 2, thread) == expectedHigh);
            if (!same && example.empty()) {
                char text[160];
                std::snprintf(text, sizeof(text), " (group %u lane %u returned 0x%08x, expected 0x%08x)", group, thread, At(Output, group, 0, thread), expectedLow);
                example = text;
            }
            wrongReturns += same ? 0u : 1u;
        }
        for (std::uint32_t thread = 0; thread < MaxThreads; ++thread) wrongMemory += At(Output, group, 1, thread) == memory[thread] ? 0u : 1u;
    }
    std::printf("%s: %s, wave%u, %u threads: %u of %u returns and %u of %u final LDS dwords differ from lane order%s\n", label, operation.name, waveSize, MaxThreads, wrongReturns, Groups * MaxThreads, wrongMemory, Groups * MaxThreads, example.c_str());
    return wrongReturns + wrongMemory;
}

std::uint32_t RunAll(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& wave64, const ShaderRecompiler::SpirvTarget& wave32, const char* label) {
    std::uint32_t failures = 0;
    for (const bool masked : {false, true}) {
        for (const std::uint32_t threads : {64u, 128u, 256u}) failures += MeasureOrder(device, wave64, 64u, threads, masked, label) != 0u ? 1u : 0u;
        for (const std::uint32_t threads : {32u, 64u, 128u, 256u}) failures += MeasureOrder(device, wave32, 32u, threads, masked, label) != 0u ? 1u : 0u;
    }
    failures += CheckRanks(device, wave64, label) != 0u ? 1u : 0u;
    for (const std::uint32_t waveSize : WaveSizes) {
        for (std::size_t index = 0; index < Operations.size(); ++index) failures += CheckOperation(device, waveSize == 64u ? wave64 : wave32, index, waveSize, label) != 0u ? 1u : 0u;
    }
    return failures;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::printf("device: %s, driver %s, subgroup %u\n", device->DeviceName().c_str(), device->DriverInfo().c_str(), device->ComputeTarget(64).subgroupSize);
        auto failures = RunAll(*device, device->ComputeTarget(64), device->ComputeTarget(32), "device target");
        auto ordered64 = device->ComputeTarget(64);
        auto ordered32 = device->ComputeTarget(32);
        ordered64.laneOrderedLdsAtomics = true;
        ordered32.laneOrderedLdsAtomics = true;
        failures += RunAll(*device, ordered64, ordered32, "lane-ordered target");
        std::vector<std::uint32_t> capabilities(ordered64.supportedCapabilities.begin(), ordered64.supportedCapabilities.end());
        std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityGroupNonUniformArithmetic));
        ordered64.supportedCapabilities = capabilities;
        ordered32.supportedCapabilities = capabilities;
        failures += RunAll(*device, ordered64, ordered32, "lane-ordered target without subgroup arithmetic");
        Require(failures == 0, "lds atomic lane order: " + std::to_string(failures) + " cases returned values out of lane order");
        std::puts("lds atomic lane order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
