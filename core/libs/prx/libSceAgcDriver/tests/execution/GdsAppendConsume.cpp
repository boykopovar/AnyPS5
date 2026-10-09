#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
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

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Counters = 4;
constexpr std::uint32_t CounterOffset = 0x10;
constexpr std::array<std::uint32_t, Counters> Starts{1000, 2000, 3000, 4000};
constexpr std::uint32_t UntouchedAppend = 0x5eed0001u;
constexpr std::uint32_t UntouchedConsume = 0x5eed0002u;
constexpr std::size_t ActiveLanesLiteral = 12;
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};
alignas(16) std::array<std::uint32_t, Counters> CounterReadback{};

alignas(256) auto Code = std::to_array<std::uint32_t>({
    0x34060082,
    0x7e0c02ff, UntouchedAppend, 0x7e0e02ff, UntouchedConsume,
    0xbefc03ff, 0x0000ffff,
    0xd8fa0010, 0x04000000,
    0xd8f60014, 0x05000000,
    0x7da800ff, 0,
    0xd8fa0018, 0x06000000,
    0xd8f6001c, 0x07000000,
    0xbefe04c1,
    0xbf8cc07f,
    0xe0702000, 0x80000403, 0xe0702004, 0x80000503, 0xe0702008, 0x80000603, 0xe070200c, 0x80000703,
    0xbf810000,
});

struct Workgroup {
    std::uint32_t threads;
    std::uint32_t waveSize;
    std::uint32_t activeLanes;

    std::string Name() const {
        return "wave" + std::to_string(waveSize) + " " + std::to_string(threads) + " threads, " + std::to_string(activeLanes) + " active";
    }
};

constexpr std::array<Workgroup, 4> Workgroups{{{32, 32, 32}, {32, 32, 20}, {64, 32, 40}, {64, 64, 40}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::vector<std::uint32_t> Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    std::vector<std::uint32_t> result{0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u)};
    result.insert(result.end(), payload);
    return result;
}

void Execute(AgcDriver::QueueState& queue, const std::vector<std::uint32_t>& packet) {
    AgcDriver::Pm4::Validate(packet, 0);
    AgcDriver::Pm4::Execute(packet, queue);
}

void SetCounters(AgcDriver::QueueState& queue) {
    for (std::uint32_t counter = 0; counter < Counters; ++counter) Execute(queue, Packet(0x50, {0x40100000u, Starts[counter], 0, CounterOffset + counter * 4u, 0, 4}));
}

std::array<std::uint32_t, Counters> ReadCounters(AgcDriver::QueueState& queue) {
    CounterReadback.fill(0xdeadbeefu);
    const auto address = reinterpret_cast<std::uintptr_t>(CounterReadback.data());
    Execute(queue, Packet(0x50, {0x20000000u, CounterOffset, 0, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), Counters * 4u}));
    return CounterReadback;
}

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup) {
    Output.fill(0xdeadbeefu);
    Code[ActiveLanesLiteral] = workgroup.activeLanes;
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin());
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{workgroup.threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {workgroup.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckWaves(const Workgroup& workgroup, std::uint32_t result, std::uint32_t start, bool append, bool masked) {
    const auto waves = workgroup.threads / workgroup.waveSize;
    std::vector<std::uint32_t> values(waves);
    std::vector<std::uint32_t> counts(waves);
    const auto what = workgroup.Name() + (masked ? " masked" : "") + (append ? " ds_append" : " ds_consume");
    for (std::uint32_t wave = 0; wave < waves; ++wave) {
        const auto first = wave * workgroup.waveSize;
        for (std::uint32_t lane = first; lane < first + workgroup.waveSize; ++lane) {
            const bool active = !masked || lane < workgroup.activeLanes;
            const auto actual = Output[lane * Results + result];
            if (!active) {
                const auto untouched = append ? UntouchedAppend : UntouchedConsume;
                Require(actual == untouched, what + ": inactive lane " + std::to_string(lane) + " was written " + std::to_string(actual));
                continue;
            }
            if (counts[wave]++ == 0) values[wave] = actual;
            Require(actual == values[wave], what + ": lane " + std::to_string(lane) + " returned " + std::to_string(actual) + ", its wave's first active lane " + std::to_string(values[wave]));
        }
    }
    std::vector<std::uint32_t> order(waves);
    for (std::uint32_t wave = 0; wave < waves; ++wave) order[wave] = wave;
    std::sort(order.begin(), order.end(), [&](std::uint32_t left, std::uint32_t right) { return append ? values[left] < values[right] : values[left] > values[right]; });
    auto expected = start;
    for (const auto wave : order) {
        if (counts[wave] == 0) continue;
        Require(values[wave] == expected, what + ": wave " + std::to_string(wave) + " returned " + std::to_string(values[wave]) + ", expected " + std::to_string(expected));
        expected = append ? expected + counts[wave] : expected - counts[wave];
    }
}

void Check(const Workgroup& workgroup, std::uint32_t pass, const std::array<std::uint32_t, Counters>& counters) {
    const auto active = std::min(workgroup.activeLanes, workgroup.threads);
    const std::array<std::uint32_t, Counters> steps{workgroup.threads, workgroup.threads, active, active};
    std::array<std::uint32_t, Counters> before{};
    for (std::uint32_t counter = 0; counter < Counters; ++counter) {
        const bool append = counter % 2u == 0u;
        before[counter] = append ? Starts[counter] + pass * steps[counter] : Starts[counter] - pass * steps[counter];
        const auto after = append ? before[counter] + steps[counter] : before[counter] - steps[counter];
        Require(counters[counter] == after, workgroup.Name() + ": GDS counter " + std::to_string(counter) + " after dispatch " + std::to_string(pass + 1u) + " is " + std::to_string(counters[counter]) + ", expected " + std::to_string(after));
    }
    CheckWaves(workgroup, 0, before[0], true, false);
    CheckWaves(workgroup, 1, before[1], false, false);
    CheckWaves(workgroup, 2, before[2], true, true);
    CheckWaves(workgroup, 3, before[3], false, true);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, the device's subgroups are narrower than a wave32 (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        AgcDriver::QueueState queue;
        for (const auto& workgroup : Workgroups) {
            SetCounters(queue);
            Require(ReadCounters(queue) == Starts, workgroup.Name() + ": the GDS counters did not read back as set");
            for (std::uint32_t pass = 0; pass < 2; ++pass) {
                Run(*device, workgroup);
                Check(workgroup, pass, ReadCounters(queue));
            }
        }
        std::puts("gds append consume tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
