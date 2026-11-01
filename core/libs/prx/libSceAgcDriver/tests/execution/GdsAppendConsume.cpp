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
constexpr std::uint32_t UntouchedOrdered = 0x5eed0003u;
constexpr std::size_t ActiveLanesLiteral = 12;
constexpr std::size_t OrderedWaveMaskLiteral = 5;
constexpr std::size_t OrderedM0Literal = 7;
constexpr std::size_t OrderedFirstLaneLiteral = 9;
constexpr std::size_t OrderedOperationLiteral = 10;
constexpr std::uint32_t OrderedAdd = 0xd8fe0310u;
constexpr std::uint32_t OrderedSwap = 0xd8fe1310u;
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

alignas(256) auto OrderedCode = std::to_array<std::uint32_t>({
    0x34060082,
    0x4a020083,
    0x7e0a02ff, UntouchedOrdered,
    0x360400ff, 0x0000003f,
    0xbefc03ff, 0x0000ffff,
    0x7da604ff, 0x00000000,
    OrderedAdd, 0x05000001,
    0xbefe04c1,
    0xbf8cc07f,
    0xe0702000, 0x80000503,
    0xbf810000,
});

constexpr std::uint32_t OrderedGroups = 64;
constexpr std::uint32_t OrderedGroupLanes = 32;
constexpr std::uint32_t OrderedGroupPasses = 8;
constexpr std::uint32_t MultiWaveGroups = 16;
constexpr std::uint32_t MultiWaveGroupLanes = 128;
constexpr std::uint32_t TimeoutGroups = 4;
constexpr std::uint32_t SkippedGroup = 1;
constexpr std::size_t GroupShiftLiteral = 2;
constexpr std::uint32_t SingleWaveShift = 0x8f058704u;
constexpr std::uint32_t MultiWaveShift = 0x8f058904u;
alignas(256) std::array<std::uint32_t, OrderedGroups * OrderedGroupLanes> GroupOutput{};

alignas(256) auto OrderedGroupCode = std::to_array<std::uint32_t>({
    0x7e0a02ff, UntouchedOrdered,
    0x8f058704,
    0xbefc03ff, 0x0000ffff,
    0x7e0202ff, 0x00000001,
    OrderedAdd, 0x05000001,
    0xbf8cc07f,
    0xe0702000, 0x05000500,
    0xbf810000,
});

alignas(256) auto DelayedFirstGroupCode = std::to_array<std::uint32_t>({
    0x7e0a02ff, UntouchedOrdered,
    SingleWaveShift,
    0xbefc03ff, 0x0000ffff,
    0xbe860380,
    0xbf078004,
    0xbf850005,
    0xbe8603ff, 0x00040000,
    0x80868106,
    0xbf078006,
    0xbf85fffd,
    0x7e0202ff, 0x00000001,
    0x4a020206,
    OrderedAdd, 0x05000001,
    0xbf8cc07f,
    0xe0702000, 0x05000500,
    0xbf810000,
});

alignas(256) auto SkippedGroupCode = std::to_array<std::uint32_t>({
    0x7e0a02ff, UntouchedOrdered,
    SingleWaveShift,
    0xbefc03ff, 0x0000ffff,
    0x7e0202ff, 0x00000001,
    0xbf060004u | (SkippedGroup + 0x80u) << 8u,
    0xbf850002,
    OrderedAdd, 0x05000001,
    0xbf8cc07f,
    0xe0702000, 0x05000500,
    0xbf810000,
});

struct OrderedRun {
    std::uint32_t m0;
    std::uint32_t firstLaneEighths;
};

constexpr std::array<OrderedRun, 4> OrderedRuns{{{0, 0}, {7, 0}, {7, 2}, {7, 5}}};

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

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin());
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

std::uint32_t OrderedTimeouts() {
    return *reinterpret_cast<const volatile std::uint32_t*>(AgcDriver::Pm4::GdsAddress() + AgcDriver::Pm4::GdsOrderedTimeoutsOffset);
}

void RunGroups(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t groups = OrderedGroups, std::uint32_t lanes = OrderedGroupLanes) {
    GroupOutput.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(GroupOutput.data(), static_cast<std::uint32_t>(GroupOutput.size()));
    std::copy(output.begin(), output.end(), userData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{lanes, 1, 1}, 0, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckOrderedGroups(const std::string& what, std::uint32_t start, const std::array<std::uint32_t, Counters>& counters, std::uint32_t timeoutsBefore, std::uint32_t groups = OrderedGroups, std::uint32_t lanes = OrderedGroupLanes) {
    const auto waves = lanes / OrderedGroupLanes;
    for (std::uint32_t group = 0; group < groups; ++group) {
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            const auto actual = GroupOutput[group * lanes + lane];
            const auto expected = start + group * waves + lane / OrderedGroupLanes;
            Require(actual == expected, what + ": group " + std::to_string(group) + " lane " + std::to_string(lane) + " returned " + std::to_string(actual) + ", expected " + std::to_string(expected) + " (the counter in launch order)");
        }
    }
    Require(counters[0] == start + groups * waves, what + ": GDS counter 0 is " + std::to_string(counters[0]) + ", expected " + std::to_string(start + groups * waves));
    Require(OrderedTimeouts() == timeoutsBefore, what + ": " + std::to_string(OrderedTimeouts() - timeoutsBefore) + " waves gave up waiting for their turn");
}

void CheckSkippedGroup(std::uint32_t start, const std::array<std::uint32_t, Counters>& counters, std::uint32_t timeoutsBefore) {
    const auto what = std::to_string(TimeoutGroups) + " single-wave groups, group " + std::to_string(SkippedGroup) + " skips ds_ordered_count";
    std::vector<std::uint32_t> seen;
    std::vector<std::uint32_t> expected;
    for (std::uint32_t group = 0; group < TimeoutGroups; ++group) {
        const auto value = GroupOutput[group * OrderedGroupLanes];
        for (std::uint32_t lane = 0; lane < OrderedGroupLanes; ++lane) {
            const auto actual = GroupOutput[group * OrderedGroupLanes + lane];
            Require(actual == value, what + ": group " + std::to_string(group) + " lane " + std::to_string(lane) + " returned " + std::to_string(actual) + ", its lane 0 " + std::to_string(value));
        }
        if (group < SkippedGroup) {
            Require(value == start + group, what + ": group " + std::to_string(group) + " before the skipped one returned " + std::to_string(value) + ", expected " + std::to_string(start + group));
        } else if (group == SkippedGroup) {
            Require(value == UntouchedOrdered, what + ": the skipped group was written " + std::to_string(value));
        } else {
            seen.push_back(value);
            expected.push_back(start + group - 1u);
        }
    }
    std::sort(seen.begin(), seen.end());
    Require(seen == expected, what + ": the groups after the skipped one did not each get one of the remaining counter values");
    Require(counters[0] == start + TimeoutGroups - 1u, what + ": GDS counter 0 is " + std::to_string(counters[0]) + ", expected " + std::to_string(start + TimeoutGroups - 1u));
    const auto timeouts = OrderedTimeouts() - timeoutsBefore;
    Require(timeouts == TimeoutGroups - SkippedGroup - 1u, what + ": " + std::to_string(timeouts) + " waves gave up waiting for their turn, expected " + std::to_string(TimeoutGroups - SkippedGroup - 1u) + " (every wave after the skipped one, none of them passing the turn on)");
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

void CheckOrdered(const Workgroup& workgroup, std::uint32_t m0, std::uint32_t firstLane, const std::array<std::uint32_t, Counters>& counters) {
    const auto waves = workgroup.threads / workgroup.waveSize;
    const auto what = workgroup.Name() + " ds_ordered_count with M0 " + std::to_string(m0) + " from lane " + std::to_string(firstLane) + " of each wave";
    std::vector<std::uint32_t> values(waves);
    std::vector<std::uint32_t> order(waves);
    for (std::uint32_t wave = 0; wave < waves; ++wave) {
        order[wave] = wave;
        const auto first = wave * workgroup.waveSize + firstLane;
        values[wave] = Output[first * Results];
        for (std::uint32_t lane = wave * workgroup.waveSize; lane < (wave + 1u) * workgroup.waveSize; ++lane) {
            const auto actual = Output[lane * Results];
            if (lane < first) {
                Require(actual == UntouchedOrdered, what + ": inactive lane " + std::to_string(lane) + " was written " + std::to_string(actual));
                continue;
            }
            Require(actual == values[wave], what + ": lane " + std::to_string(lane) + " returned " + std::to_string(actual) + ", its wave's first active lane " + std::to_string(values[wave]));
        }
    }
    std::sort(order.begin(), order.end(), [&](std::uint32_t left, std::uint32_t right) { return values[left] < values[right]; });
    auto expected = Starts[0];
    for (const auto wave : order) {
        Require(values[wave] == expected, what + ": wave " + std::to_string(wave) + " returned " + std::to_string(values[wave]) + ", expected " + std::to_string(expected));
        expected += wave * workgroup.waveSize + firstLane + 3u;
    }
    Require(counters[0] == expected, what + ": GDS counter 0 is " + std::to_string(counters[0]) + ", expected " + std::to_string(expected));
}

void CheckOrderedSwap(const Workgroup& workgroup, std::uint32_t m0, std::uint32_t firstLane, const std::array<std::uint32_t, Counters>& counters) {
    const auto waves = workgroup.threads / workgroup.waveSize;
    const auto what = workgroup.Name() + " ds_ordered_count swap with M0 " + std::to_string(m0) + " from lane " + std::to_string(firstLane) + " of each wave";
    std::vector<std::uint32_t> seen{counters[0]};
    std::vector<std::uint32_t> expected{Starts[0]};
    for (std::uint32_t wave = 0; wave < waves; ++wave) {
        const auto first = wave * workgroup.waveSize + firstLane;
        for (std::uint32_t lane = wave * workgroup.waveSize; lane < (wave + 1u) * workgroup.waveSize; ++lane) {
            const auto actual = Output[lane * Results];
            if (lane < first) {
                Require(actual == UntouchedOrdered, what + ": inactive lane " + std::to_string(lane) + " was written " + std::to_string(actual));
                continue;
            }
            Require(actual == Output[first * Results], what + ": lane " + std::to_string(lane) + " returned " + std::to_string(actual) + ", its wave's first active lane " + std::to_string(Output[first * Results]));
        }
        seen.push_back(Output[first * Results]);
        expected.push_back(first + 3u);
    }
    std::sort(seen.begin(), seen.end());
    std::sort(expected.begin(), expected.end());
    Require(seen == expected, what + ": the returned values and GDS counter 0 (" + std::to_string(counters[0]) + ") are not the start value and each wave's first active lane value");
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
            Code[ActiveLanesLiteral] = workgroup.activeLanes;
            for (std::uint32_t pass = 0; pass < 2; ++pass) {
                Run(*device, workgroup, Code);
                Check(workgroup, pass, ReadCounters(queue));
            }
            for (const auto& run : OrderedRuns) {
                const auto firstLane = workgroup.waveSize * run.firstLaneEighths / 8u;
                SetCounters(queue);
                OrderedCode[OrderedWaveMaskLiteral] = workgroup.waveSize - 1u;
                OrderedCode[OrderedM0Literal] = run.m0;
                OrderedCode[OrderedFirstLaneLiteral] = firstLane;
                OrderedCode[OrderedOperationLiteral] = OrderedAdd;
                Run(*device, workgroup, OrderedCode);
                CheckOrdered(workgroup, run.m0, firstLane, ReadCounters(queue));
                SetCounters(queue);
                OrderedCode[OrderedOperationLiteral] = OrderedSwap;
                Run(*device, workgroup, OrderedCode);
                CheckOrderedSwap(workgroup, run.m0, firstLane, ReadCounters(queue));
            }
        }
        SetCounters(queue);
        for (std::uint32_t pass = 0; pass < OrderedGroupPasses; ++pass) {
            const auto timeouts = OrderedTimeouts();
            RunGroups(*device, OrderedGroupCode);
            CheckOrderedGroups(std::to_string(OrderedGroups) + " single-wave groups, dispatch " + std::to_string(pass + 1u) + ", ds_ordered_count", Starts[0] + pass * OrderedGroups, ReadCounters(queue), timeouts);
        }
        SetCounters(queue);
        auto timeouts = OrderedTimeouts();
        DelayedFirstGroupCode[GroupShiftLiteral] = SingleWaveShift;
        RunGroups(*device, DelayedFirstGroupCode);
        CheckOrderedGroups(std::to_string(OrderedGroups) + " single-wave groups, group 0 delayed, ds_ordered_count", Starts[0], ReadCounters(queue), timeouts);
        SetCounters(queue);
        timeouts = OrderedTimeouts();
        DelayedFirstGroupCode[GroupShiftLiteral] = MultiWaveShift;
        RunGroups(*device, DelayedFirstGroupCode, MultiWaveGroups, MultiWaveGroupLanes);
        CheckOrderedGroups(std::to_string(MultiWaveGroups) + " groups of " + std::to_string(MultiWaveGroupLanes / OrderedGroupLanes) + " waves, group 0 delayed, ds_ordered_count", Starts[0], ReadCounters(queue), timeouts, MultiWaveGroups, MultiWaveGroupLanes);
        SetCounters(queue);
        timeouts = OrderedTimeouts();
        RunGroups(*device, SkippedGroupCode, TimeoutGroups);
        CheckSkippedGroup(Starts[0], ReadCounters(queue), timeouts);
        SetCounters(queue);
        timeouts = OrderedTimeouts();
        RunGroups(*device, OrderedGroupCode);
        CheckOrderedGroups(std::to_string(OrderedGroups) + " single-wave groups after a dispatch whose waves timed out, ds_ordered_count", Starts[0], ReadCounters(queue), timeouts);
        std::puts("gds append consume tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
