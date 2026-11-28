#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Waves = 8;
constexpr std::uint32_t Rounds = 16;
constexpr std::uint32_t Groups = 1;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Default = 0x11111111u;
alignas(4096) std::array<std::uint32_t, 1024> Input{};
alignas(256) std::array<std::uint32_t, Waves * 64u> Output{};
alignas(256) std::array<std::uint32_t, 16> Root{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

bool KeepsEndedWaves(const ShaderRecompiler::SharedSpirv& spirv) {
    const std::string_view text(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
    return text.find("ended_wave") != std::string_view::npos;
}

ShaderRecompiler::RecompileRequest Request(std::span<const std::uint32_t> program, std::span<const std::uint32_t> userData, std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const std::uint32_t threads = Waves * wave;
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, threads, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(program.data()), program, 0, {}},
        {wave, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return request;
}

template <typename Result>
void Dispatch(AgcDriver::VulkanDevice& device, const Result& result, std::span<const std::uint32_t> program, std::uint32_t wave, std::uint32_t bias, const std::string& name) {
    const std::uint32_t threads = Waves * wave;
    const std::uint32_t live = threads / 2u;
    for (std::uint32_t round = 0; round < Rounds; ++round) {
        Output.fill(Untouched);
        device.Dispatch(result, Groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(program.data()));
        device.WaitIdle();
        for (std::uint32_t thread = 0; thread < threads; ++thread) {
            const std::uint32_t expected = thread < live ? thread + 1u + bias : Untouched;
            Require(Output[thread] == expected, name + ", wave" + std::to_string(wave) + " round " + std::to_string(round) + ": thread " + std::to_string(thread) + " (wave " + std::to_string(thread / wave) + ") holds " + std::to_string(Output[thread]) + ", expected " + std::to_string(expected));
        }
    }
}

void RunActiveLanes(AgcDriver::VulkanDevice& device, std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target) {
    const std::uint32_t live = Waves * wave / 2u;
    const auto input = reinterpret_cast<std::uintptr_t>(Input.data());
    alignas(256) const std::array<std::uint32_t, 25> code{
        0x34040082u, 0x7d8800ffu, live, 0x4a1004ffu, static_cast<std::uint32_t>(input), 0x02101080u, 0x7e1202ffu, static_cast<std::uint32_t>(input >> 32u),
        0x02121280u, 0xbf8a0000u, 0xbf86000du, 0xdc308000u, 0x067d0008u, 0xbf8c3f70u, 0xd8340000u, 0x00000602u,
        0xbf8cc07fu, 0xbf8a0000u, 0xd8d80000u, 0x03000002u, 0xbf8cc07fu, 0xbf8a0000u, 0xe0701000u, 0x80000302u,
        0xbf810000u,
    };
    const auto userData = BufferDescriptor(Output.data(), Waves * wave * 4u);
    const std::span<const std::uint32_t> program(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.data()), std::as_bytes(program)}}};
    const auto result = ShaderRecompiler::Recompile(Request(program, userData, wave, target, memory));
    Require(KeepsEndedWaves(result.spirv), "ended wave barrier, wave" + std::to_string(wave) + ": the ended waves were not kept for the later barriers");
    for (std::uint32_t thread = 0; thread < Input.size(); ++thread) Input[thread] = thread + 1u;
    Dispatch(device, result, program, wave, 0u, "ended wave barrier");
    std::printf("ended wave barrier, wave%u: %u live waves passed two barriers after %u waves with active lanes ended\n", wave, Waves / 2u, Waves / 2u);
}

void RunGuardedRead(AgcDriver::VulkanDevice& device, std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target) {
    const std::uint32_t threads = Waves * wave;
    const std::uint32_t live = threads / 2u;
    alignas(256) const std::array<std::uint32_t, 29> code{
        0xf4080100u, 0xfa000000u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, Default, 0xbf8cc07fu, 0x34040082u,
        0x4a020081u, 0x7d8800ffu, live, 0xbf8a0000u, 0xbf86000fu, 0xbf870003u, 0xf4000305u, 0xfa000000u,
        0xbf8cc07fu, 0x4a02020cu, 0xd8340000u, 0x00000102u, 0xbf8cc07fu, 0xbf8a0000u, 0xd8d80000u, 0x03000002u,
        0xbf8cc07fu, 0xbf8a0000u, 0xe0701000u, 0x80010302u, 0xbf810000u,
    };
    Root = {};
    const auto output = BufferDescriptor(Output.data(), threads * 4u);
    std::copy(output.begin(), output.end(), Root.begin());
    const auto root = reinterpret_cast<std::uintptr_t>(Root.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> program(code);
    auto request = Request(program, userData, wave, target, {});
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto result = ShaderRecompiler::Recompile(request, *capture);
    Require(result->poisonedSrtReads == 1u, "ended wave guarded read, wave" + std::to_string(wave) + ": the read through the null pointer was not guarded");
    Require(KeepsEndedWaves(result->spirv), "ended wave guarded read, wave" + std::to_string(wave) + ": the ended waves were not kept for the later barriers");
    Dispatch(device, *result, program, wave, Default, "ended wave guarded read");
    std::printf("ended wave guarded read, wave%u: only the ended waves reached a read through a null pointer, and it reported no fault\n", wave);
}

void RequireBypassLeaves(std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target) {
    const std::uint32_t live = Waves * wave / 2u;
    alignas(256) const std::array<std::uint32_t, 11> code{
        0x34040082u, 0x7d8800ffu, live, 0xbf8a0000u, 0xbf860005u, 0xbf068000u, 0xbf850001u, 0xbf8a0000u,
        0xe0701000u, 0x80000202u, 0xbf810000u,
    };
    const auto userData = BufferDescriptor(Output.data(), Waves * wave * 4u);
    const std::span<const std::uint32_t> program(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.data()), std::as_bytes(program)}}};
    const auto result = ShaderRecompiler::Recompile(Request(program, userData, wave, target, memory));
    Require(!KeepsEndedWaves(result.spirv), "ended wave bypass, wave" + std::to_string(wave) + ": ended waves were kept although a branch after the exit skips the barrier");
    std::printf("ended wave bypass, wave%u: a barrier that a later branch can skip leaves ended waves to exit\n", wave);
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target) {
    RequireBypassLeaves(wave, target);
    RunActiveLanes(device, wave, target);
    RunGuardedRead(device, wave, target);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(Input.data(), sizeof(Input), true, false, true);
        const auto wave32 = device->ComputeTarget(32);
        if (wave32.subgroupSize == 32u) Run(*device, 32, wave32);
        else std::printf("wave32 skipped, wave32 programs do not run on 32-lane subgroups (%u lanes)\n", wave32.subgroupSize);
        Run(*device, 64, device->ComputeTarget(64));
        std::puts("ended wave barrier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
