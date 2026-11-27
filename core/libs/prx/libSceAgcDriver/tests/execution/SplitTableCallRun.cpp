#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "SplitStateAbi.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxLanes = 64;
std::uint32_t Lanes = 32;
constexpr std::uint32_t Groups = 7;
constexpr std::uint32_t SliceGroups = 3;
constexpr std::uint32_t Vectors = 56;
constexpr std::uint32_t UserData = 14;
constexpr std::uint32_t TableOffset = 0x270;

alignas(256) std::array<std::uint32_t, MaxLanes * Groups> Input{};
alignas(256) std::array<std::uint32_t, MaxLanes * Groups> Output{};
alignas(256) std::array<std::uint32_t, MaxLanes * 4> Table{};
alignas(256) std::array<std::uint32_t, TableOffset / 4 + 4> Block{};
alignas(256) std::array<std::uint32_t, 4> Arguments{Groups, 1, 1, 0};
alignas(256) std::array<std::uint32_t, 4> ThreadArguments{};

const std::vector<ShaderRecompiler::TableCallee>& Callees() {
    static const std::vector<ShaderRecompiler::TableCallee> callees{
        {0x10000u, {0xbf068000u, 0x4a020285u, 0xbe80200eu}},
        {0x20000u, {0x4a0202c0u, 0xbe80200eu}},
        {0x30000u, {0x34020281u, 0xbe80200eu}},
    };
    return callees;
}

const std::vector<ShaderRecompiler::TableCallee>& StuckCallees() {
    static const std::vector<ShaderRecompiler::TableCallee> callees{
        Callees()[0],
        Callees()[1],
        {0x30000u, {0x7e600281u, 0xbe80200eu}},
    };
    return callees;
}

std::vector<std::uint32_t> Caller() {
    std::vector<std::uint32_t> code{Lanes == 64u ? 0x8f15860eu : 0x8f15850eu, 0x4a060015u, 0x34040682u, 0xe0301000u, 0x80000102u, 0xbf8c3f70u, 0xbeaa047eu, 0x7e600281u};
    const auto loop = static_cast<std::int32_t>(code.size());
    code.insert(code.end(), {0x7e280500u, 0xbea8047eu, 0x7da40014u, 0x7e600280u});
    code.insert(code.end(), {0x8f148414u, 0xf4080406u, 0xfa000270u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu});
    code.insert(code.end(), {0xbefe0428u, 0x7daa6080u});
    code.push_back(0xbf890000u | (static_cast<std::uint32_t>(loop - static_cast<std::int32_t>(code.size() + 1u)) & 0xffffu));
    code.insert(code.end(), {0xbefe042au, 0xe0701000u, 0x80010102u, 0xbf810000u});
    return code;
}

std::vector<std::uint32_t> FullExecCaller() {
    auto code = Caller();
    code.insert(code.begin(), Lanes == 64u ? 0xbefe04c1u : 0xbefe03c1u);
    return code;
}

std::vector<std::uint32_t> LoopCaller() {
    std::vector<std::uint32_t> code{Lanes == 64u ? 0x8f15860eu : 0x8f15850eu, 0x4a060015u, 0x34040682u, 0xe0301000u, 0x80000102u, 0xbf8c3f70u, 0xbeaa047eu, 0xbe960380u, 0x36080083u, 0x4a080881u};
    const auto loop = static_cast<std::int32_t>(code.size());
    code.insert(code.end(), {0x8f148416u, 0xf4080406u, 0xfa000270u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu, 0x80168116u, 0x7da20816u});
    code.push_back(0xbf890000u | (static_cast<std::uint32_t>(loop - static_cast<std::int32_t>(code.size() + 1u)) & 0xffffu));
    code.insert(code.end(), {0xbefe042au, 0xe0701000u, 0x80010102u, 0xbf810000u});
    return code;
}

std::uint32_t Served(std::uint32_t value, std::uint32_t callee) {
    switch (callee % 3u) {
        case 0: return value + 5u;
        case 1: return value + 64u;
        default: return value << 1u;
    }
}

std::uint32_t Expected(std::uint32_t index) {
    return Served(Input[index], index % Lanes);
}

std::uint32_t ExpectedLoop(std::uint32_t index) {
    auto value = Input[index];
    const auto calls = (index % Lanes & 3u) + 1u;
    for (std::uint32_t call = 0; call < calls; ++call) value = Served(value, call);
    return value;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::vector<std::uint32_t> UserDataWords() {
    std::vector<std::uint32_t> words(UserData, 0u);
    const auto input = BufferDescriptor(Input.data(), Lanes * Groups * 4u);
    const auto output = BufferDescriptor(Output.data(), Lanes * Groups * 4u);
    std::copy(input.begin(), input.end(), words.begin());
    std::copy(output.begin(), output.end(), words.begin() + 4);
    const auto block = reinterpret_cast<std::uintptr_t>(Block.data());
    words[12] = static_cast<std::uint32_t>(block);
    words[13] = static_cast<std::uint32_t>(block >> 32u);
    return words;
}

void Prepare() {
    for (std::uint32_t index = 0; index < Lanes * Groups; ++index) Input[index] = 0x9e3779b9u * (index + 1u);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto target = Callees()[lane % 3u].target;
        Table[lane * 4u] = static_cast<std::uint32_t>(target);
        Table[lane * 4u + 1u] = static_cast<std::uint32_t>(target >> 32u);
    }
    const auto table = BufferDescriptor(Table.data(), static_cast<std::uint32_t>(Table.size() * 4u));
    std::copy(table.begin(), table.end(), Block.begin() + TableOffset / 4u);
    Output.fill(0xdeadbeefu);
}

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::span<const std::uint32_t> fallbacks, std::span<const std::uint32_t> plan, std::uint64_t address, const ShaderRecompiler::SpirvTarget& target, bool threadDimensions = false) {
    const auto userData = UserDataWords();
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {address, std::as_bytes(code)},
        {reinterpret_cast<std::uintptr_t>(Block.data()), std::as_bytes(std::span(Block))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
    }};
    ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    if (threadDimensions) compute.partialThreads = {~0u, ~0u, ~0u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, address, code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, threadDimensions ? 0u : 128u}
    };
    request.context.linkedCallFallbacks = fallbacks;
    request.context.splitState = plan;
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Check(const char* name, std::uint32_t (*expectedOf)(std::uint32_t) = Expected) {
    for (std::uint32_t index = 0; index < Lanes * Groups; ++index) {
        const auto actual = Output[index];
        const auto expected = expectedOf(index);
        if (actual != expected) {
            char text[160];
            std::snprintf(text, sizeof(text), "%s: invocation %u holds 0x%08x, expected 0x%08x", name, index, actual, expected);
            throw std::runtime_error(text);
        }
    }
}

void RunLinked(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const ShaderRecompiler::RdnaProgram& program, const ShaderRecompiler::TableCall& call, std::uint32_t (*expectedOf)(std::uint32_t) = Expected) {
    Prepare();
    const std::vector<ShaderRecompiler::TableCall> calls{call};
    const std::vector<std::vector<ShaderRecompiler::TableCallee>> callees{Callees()};
    const auto linked = ShaderRecompiler::LinkCalls(program, {}, {}, calls, callees);
    const auto result = Compile(linked.code, linked.fallbacks, {}, 0x700000u, target);
    device.Dispatch(result, Groups, 1, 1, {}, 0x700000u);
    device.WaitIdle();
    Check("linked", expectedOf);
}

void RunContinued(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const ShaderRecompiler::SplitTableCallPrograms& programs, bool gpuRounds = false) {
    Prepare();
    Require(programs.continuation, "the loop caller's programs do not continue across dispatches");
    std::vector<const ShaderRecompiler::SplitTableCallProgram*> parts{&programs.entry};
    for (const auto& callee : programs.callees) parts.push_back(&callee);
    parts.push_back(&programs.resume);
    std::vector<ShaderRecompiler::RecompileResult> results;
    for (std::size_t part = 0; part < parts.size(); ++part) {
        results.push_back(Compile(parts[part]->code, {}, parts[part]->plan, 0x740000u + part * 0x1000u, target));
        Require(results.back().splitStateLanes == Lanes, "continued part " + std::to_string(part) + " reports " + std::to_string(results.back().splitStateLanes) + " lanes per workgroup");
    }
    const auto capacity = SliceGroups * Lanes;
    for (std::uint32_t slice = 0; slice * SliceGroups < Groups; ++slice) {
        const auto run = [&](std::size_t part, bool readTargets, std::int32_t listed = -1, const std::vector<std::uint64_t>* select = nullptr, bool selectBegin = false) {
            AgcDriver::VulkanDevice::SplitSlice current{capacity, slice * SliceGroups, SliceGroups, ShaderRecompiler::SplitStateAbi::TotalSlots(programs.stateVectors), readTargets, ShaderRecompiler::SplitStateAbi::TargetSlot(programs.stateVectors)};
            if (gpuRounds) {
                current.listTargets = AgcDriver::VulkanDevice::SplitSelectMaxTargets;
                current.lanesPerGroup = Lanes;
                current.listed = listed;
                if (select != nullptr) current.selectTargets = *select;
                current.selectBegin = selectBegin;
            }
            device.SetSplitSlice(current);
            device.Dispatch(results[part], Groups, 1, 1, {}, 0x740000u + part * 0x1000u);
            device.SetSplitSlice(std::nullopt);
            std::vector<std::size_t> called;
            if (!readTargets) return called;
            device.WaitIdle();
            const auto words = device.SplitTargets();
            const auto lanes = std::min(capacity, (Groups - slice * SliceGroups) * Lanes);
            for (std::uint32_t lane = 0; lane < lanes; ++lane) {
                const auto address = static_cast<std::uint64_t>(words[ShaderRecompiler::SplitStateAbi::HeaderDwords + lane]) | static_cast<std::uint64_t>(words[ShaderRecompiler::SplitStateAbi::HeaderDwords + capacity + lane]) << 32u;
                if (address == 0) continue;
                const auto found = std::ranges::find_if(Callees(), [&](const auto& callee) { return callee.target == address; });
                Require(found != Callees().end(), "a continued lane calls an address outside its call table");
                const auto part = static_cast<std::size_t>(found - Callees().begin()) + 1u;
                if (std::ranges::find(called, part) == called.end()) called.push_back(part);
            }
            return called;
        };
        auto called = run(0, true);
        for (std::uint32_t round = 0; !called.empty(); ++round) {
            Require(round < 8u, "continued lanes still call after 8 rounds");
            if (!gpuRounds || round == 0u) {
                for (const auto part : called) static_cast<void>(run(part, false));
                called = run(parts.size() - 1u, true);
                continue;
            }
            std::vector<std::uint64_t> selected;
            for (const auto part : called) selected.push_back(Callees()[part - 1u].target);
            constexpr std::uint32_t Batch = 4u;
            for (std::uint32_t gpuRound = 0; gpuRound < Batch; ++gpuRound) {
                for (std::size_t index = 0; index < called.size(); ++index) static_cast<void>(run(called[index], false, static_cast<std::int32_t>(index), index == 0u ? &selected : nullptr, index == 0u && gpuRound == 0u));
                const auto read = run(parts.size() - 1u, gpuRound + 1u == Batch, static_cast<std::int32_t>(called.size()));
                if (gpuRound + 1u == Batch) {
                    Require(device.SplitTargets()[ShaderRecompiler::SplitStateAbi::RoundDword] != 0u, "a GPU-selected batch of continued rounds ran no round");
                    called = read;
                }
            }
        }
    }
    device.WaitIdle();
    Check("continued", ExpectedLoop);
}

void RunSplit(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const ShaderRecompiler::SplitTableCallPrograms& programs, bool indirect, bool stuck = false, std::uint32_t threads = 0, bool fullExec = false) {
    Prepare();
    std::vector<const ShaderRecompiler::SplitTableCallProgram*> parts{&programs.entry};
    for (const auto& callee : programs.callees) parts.push_back(&callee);
    parts.push_back(&programs.resume);
    std::vector<ShaderRecompiler::RecompileResult> results;
    for (std::size_t part = 0; part < parts.size(); ++part) {
        results.push_back(Compile(parts[part]->code, {}, parts[part]->plan, 0x710000u + part * 0x1000u, target, threads != 0));
        Require(results.back().splitStateLanes == Lanes, "split part " + std::to_string(part) + " reports " + std::to_string(results.back().splitStateLanes) + " lanes per workgroup");
    }
    const auto slices = indirect ? (Groups + SliceGroups - 1u) / SliceGroups + 1u : (Groups + SliceGroups - 1u) / SliceGroups;
    for (std::uint32_t slice = 0; slice < slices; ++slice) {
        for (std::size_t part = 0; part < results.size(); ++part) {
            device.SetSplitSlice(AgcDriver::VulkanDevice::SplitSlice{SliceGroups * Lanes, slice * SliceGroups, SliceGroups, ShaderRecompiler::SplitStateAbi::TotalSlots(programs.stateVectors)});
            if (threads != 0) device.DispatchIndirect(results[part], reinterpret_cast<std::uintptr_t>(ThreadArguments.data()), {}, 0x710000u + part * 0x1000u, nullptr, nullptr, {Lanes, 1, 1});
            else if (indirect) device.DispatchIndirect(results[part], reinterpret_cast<std::uintptr_t>(Arguments.data()), {}, 0x710000u + part * 0x1000u);
            else device.Dispatch(results[part], Groups, 1, 1, {}, 0x710000u + part * 0x1000u);
            device.SetSplitSlice(std::nullopt);
        }
    }
    device.WaitIdle();
    if (stuck) {
        Require(AgcDriver::Graphics::LoopGuardTripped(), "a split callee that keeps its lanes in the waterfall did not trip the loop guard");
        return;
    }
    if (threads == 0) {
        Check(indirect ? "split indirect" : "split");
        return;
    }
    for (std::uint32_t index = 0; index < Lanes * Groups; ++index) {
        const auto expected = index < threads || fullExec ? Expected(index) : 0xdeadbeefu;
        if (Output[index] != expected) throw std::runtime_error("split thread-dimension indirect: invocation " + std::to_string(index) + " holds " + std::to_string(Output[index]) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto [lanes, hostWave] : {std::pair{32u, 32u}, std::pair{64u, 32u}, std::pair{64u, 64u}}) {
            Lanes = lanes;
            const auto target = device->ComputeTarget(hostWave);
            const auto caller = Caller();
            const auto program = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(caller);
            const ShaderRecompiler::SwappcInfo swappc{false, 0u, UserData, Lanes};
            const auto calls = ShaderRecompiler::FindTableCalls(program, swappc);
            Require(calls.size() == 1u, "the waterfall caller holds " + std::to_string(calls.size()) + " table calls");
            std::string refusal;
            const auto split = ShaderRecompiler::SplitTableCall(program, calls[0], Callees(), Lanes, refusal);
            Require(split.has_value(), "the waterfall caller cannot split: " + refusal);
            RunLinked(*device, target, program, calls[0]);
            const auto programs = ShaderRecompiler::BuildSplitTableCallPrograms(program, calls[0], Callees(), *split, Vectors);
            RunSplit(*device, target, programs, false);
            RunSplit(*device, target, programs, true);
            ThreadArguments = {Lanes * Groups - 5u, 1, 1, 0};
            RunSplit(*device, target, programs, true, false, ThreadArguments[0]);
            const auto fullExecCaller = FullExecCaller();
            const auto fullExecProgram = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(fullExecCaller);
            const auto fullExecCalls = ShaderRecompiler::FindTableCalls(fullExecProgram, swappc);
            Require(fullExecCalls.size() == 1u, "the full exec caller holds " + std::to_string(fullExecCalls.size()) + " table calls");
            std::string fullExecRefusal;
            const auto fullExecSplit = ShaderRecompiler::SplitTableCall(fullExecProgram, fullExecCalls[0], Callees(), Lanes, fullExecRefusal);
            Require(fullExecSplit.has_value(), "the full exec caller cannot split: " + fullExecRefusal);
            RunLinked(*device, target, fullExecProgram, fullExecCalls[0]);
            RunSplit(*device, target, ShaderRecompiler::BuildSplitTableCallPrograms(fullExecProgram, fullExecCalls[0], Callees(), *fullExecSplit, Vectors), true, false, ThreadArguments[0], true);
            std::string stuckRefusal;
            const auto stuck = ShaderRecompiler::SplitTableCall(program, calls[0], StuckCallees(), Lanes, stuckRefusal);
            Require(stuck.has_value(), "the waterfall caller cannot split with a stuck callee: " + stuckRefusal);
            RunSplit(*device, target, ShaderRecompiler::BuildSplitTableCallPrograms(program, calls[0], StuckCallees(), *stuck, Vectors), false, true);
            const auto loopCaller = LoopCaller();
            const auto loopProgram = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(loopCaller);
            const auto loopCalls = ShaderRecompiler::FindTableCalls(loopProgram, swappc);
            Require(loopCalls.size() == 1u, "the loop caller holds " + std::to_string(loopCalls.size()) + " table calls");
            RunLinked(*device, target, loopProgram, loopCalls[0], ExpectedLoop);
            std::string loopRefusal;
            const auto continuation = ShaderRecompiler::ContinueTableCall(loopProgram, loopCalls[0], Callees(), Vectors, loopRefusal);
            Require(continuation.has_value(), "the loop caller cannot continue: " + loopRefusal);
            const auto continued = ShaderRecompiler::BuildTableCallContinuation(loopProgram, loopCalls[0], Callees(), *continuation);
            RunContinued(*device, target, continued);
            RunContinued(*device, target, continued, true);
            std::printf("wave%u on host subgroup %u passed\n", lanes, target.subgroupSize);
        }
        std::puts("split table call run tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
