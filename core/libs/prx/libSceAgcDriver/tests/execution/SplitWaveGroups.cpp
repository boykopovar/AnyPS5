#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::RuntimeSwitch;

alignas(256) constexpr std::array<std::uint32_t, 12> PlaneCode{0xd7460002u, 0x04010901u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0xe0702000u, 0x80000503u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> LineCode{0x7e040300u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0xe0702000u, 0x80000503u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 19> LdsCode{0xd7460002u, 0x04010901u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0x340c0482u, 0xd8340000u, 0x00000506u, 0xbf8cc07fu, 0xd8d80000u, 0x05000006u, 0xbf8cc07fu, 0xe0702000u, 0x80000503u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 13> BarrierCode{0xd7460002u, 0x04010901u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0xbf8a0000u, 0xe0702000u, 0x80000503u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 18> ScratchCode{0x7e040300u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0x7e0e0280u, 0xdc704000u, 0x007d0507u, 0xbf8c3f70u, 0xdc304000u, 0x057d0007u, 0xbf8c3f70u, 0xe0702000u, 0x80000503u, 0xbf810000u};
constexpr std::uint32_t StoredFormat = 0x00014204u;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t OutputWords = 1024;
constexpr std::size_t BlockBytes = 65536;

struct Case {
    const char* name;
    std::span<const std::uint32_t> code;
    std::array<std::uint32_t, 3> threads;
    std::uint32_t ldsDwords;
    std::uint32_t threadIds;
    std::uint32_t scratchDwords;
    std::array<std::uint32_t, 3> partialThreads;
    std::uint32_t wave;
    std::uint32_t splitGroups;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases{
        {"line64", LineCode, {64, 1, 1}, 0, 1, 0, {}, 64, 1},
        {"line64-wave32", LineCode, {64, 1, 1}, 0, 1, 0, {}, 32, 1},
        {"plane", PlaneCode, {16, 16, 1}, 0, 2, 0, {}, 64, 4},
        {"plane-wave32", PlaneCode, {16, 16, 1}, 0, 2, 0, {}, 32, 1},
        {"line", LineCode, {256, 1, 1}, 0, 1, 0, {}, 64, 4},
        {"line192", LineCode, {192, 1, 1}, 0, 1, 0, {}, 64, 3},
        {"line96", LineCode, {96, 1, 1}, 0, 1, 0, {}, 64, 1},
        {"plane-lds", LdsCode, {16, 16, 1}, 256, 2, 0, {}, 64, 1},
        {"plane-barrier", BarrierCode, {16, 16, 1}, 0, 2, 0, {}, 64, 1},
        {"line-scratch", ScratchCode, {256, 1, 1}, 0, 1, 1, {}, 64, 1},
        {"line-partial", LineCode, {256, 1, 1}, 0, 1, 0, {320, 1, 1}, 64, 1},
    };
    return cases;
}

std::uint64_t Fnv(std::span<const std::uint32_t> words) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto word : words) {
        for (std::uint32_t byte = 0; byte < 4u; ++byte) {
            hash ^= (word >> (8u * byte)) & 0xffu;
            hash *= 0x100000001b3ull;
        }
    }
    return hash;
}

std::uint64_t Compile(AgcDriver::VulkanDevice& device, const Case& item, bool split, std::uint32_t* groups) {
    constexpr std::array<std::uint32_t, 4> userData{0x200000u, 4u << 16u, 1024u, StoredFormat};
    const ShaderRecompiler::ShaderComputeStageInfo compute{item.threads, item.ldsDwords, {true, false, false}, false, item.threadIds, item.partialThreads, item.scratchDwords};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(item.code.data()), item.code, 0, {}},
        {item.wave, 0, userData, compute, std::nullopt, std::nullopt, {}, std::nullopt},
        device.ComputeTarget(item.wave),
        {0, 0, 0, 128}};
    request.target.splitWaveGroups = split;
    const auto result = ShaderRecompiler::Recompile(request);
    *groups = result.groupMultiplier;
    return Fnv(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()));
}

void Forms(AgcDriver::VulkanDevice& device) {
    for (const auto& item : Cases()) {
        std::uint32_t groups = 0;
        const auto plain = Compile(device, item, false, &groups);
        Require(groups == 1u, std::string(item.name) + ": the plain request multiplied the workgroups");
        const auto split = Compile(device, item, true, &groups);
        Require(groups == item.splitGroups, std::string(item.name) + ": the split request multiplies the workgroups by " + std::to_string(groups) + ", expected " + std::to_string(item.splitGroups));
        Require(item.splitGroups != 1u || split == plain, std::string(item.name) + ": a program the split does not apply to compiled differently with the split requested");
        Require(item.splitGroups == 1u || split != plain, std::string(item.name) + ": the split form compiled to the plain module");
        std::printf("form %s split %u %016llx\n", item.name, item.splitGroups, static_cast<unsigned long long>(split));
    }
}

std::span<std::uint32_t> GuestOutput() {
    std::uint8_t* block = nullptr;
#ifdef _WIN32
    block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
    constexpr std::uintptr_t alignment = 65536;
    void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped != MAP_FAILED) {
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
        if (aligned != begin) munmap(mapped, aligned - begin);
        if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
    }
#endif
    Require(block != nullptr, "cannot allocate the guest block for the output");
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return {reinterpret_cast<std::uint32_t*>(block), OutputWords};
}

struct Launch {
    std::span<const std::uint32_t> code;
    std::array<std::uint32_t, 3> threads;
    std::uint32_t rsrc2;
    std::uint32_t x;
    std::uint32_t initiator;
};

std::vector<std::uint32_t> Words(std::span<std::uint32_t> output, const Launch& launch) {
    const auto address = reinterpret_cast<std::uintptr_t>(output.data());
    const auto program = reinterpret_cast<std::uintptr_t>(launch.code.data());
    return {
        0xc0037600u, 0x207, launch.threads[0], launch.threads[1], launch.threads[2],
        0xc0027600u, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600u, 0x213, launch.rsrc2,
        0xc0047600u, 0x240, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(output.size()), StoredFormat,
    };
}

std::vector<std::uint32_t> Submit(std::span<std::uint32_t> output, std::vector<std::uint32_t> words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(output.data()), output.size_bytes());
    return {output.begin(), output.end()};
}

std::vector<std::uint32_t> Run(std::span<std::uint32_t> output, const Launch& launch) {
    std::fill(output.begin(), output.end(), Untouched);
    auto words = Words(output, launch);
    words.insert(words.end(), {0xc0031500u, launch.x, 1, 1, launch.initiator});
    return Submit(output, std::move(words));
}

std::vector<std::uint32_t> RunIndirect(std::span<std::uint32_t> output, const Launch& launch) {
    std::fill(output.begin(), output.end(), Untouched);
    auto* arguments = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(output.data()) + BlockBytes / 2u);
    arguments[0] = launch.x;
    arguments[1] = 1;
    arguments[2] = 1;
    arguments[3] = 0;
    const auto table = reinterpret_cast<std::uintptr_t>(arguments);
    auto words = Words(output, launch);
    words.insert(words.end(), {0xc0021102u, 1u, static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u), 0xc0011600u, 0u, launch.initiator});
    return Submit(output, std::move(words));
}

std::vector<std::uint32_t> ExpectedWaves(std::uint32_t groupThreads, std::uint32_t totalThreads) {
    std::vector<std::uint32_t> expected(OutputWords, Untouched);
    for (std::uint32_t thread = 0; thread < totalThreads; ++thread) {
        const auto group = thread / groupThreads;
        const auto linear = thread % groupThreads;
        const auto first = linear / 64u * 64u;
        expected.at(group * 256u + linear) = (group << 24u) | (first << 16u) | linear;
    }
    return expected;
}

void Same(const std::vector<std::uint32_t>& actual, const std::vector<std::uint32_t>& expected, const std::string& what) {
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (actual[index] != expected[index]) {
            char text[160];
            std::snprintf(text, sizeof(text), ": word %zu holds 0x%08x, expected 0x%08x", index, actual[index], expected[index]);
            throw std::runtime_error(what + text);
        }
    }
}

std::uint32_t Rsrc2(std::uint32_t threadIds, std::uint32_t ldsDwords) {
    return (4u << 1u) | (1u << 7u) | ((threadIds - 1u) << 11u) | ((ldsDwords / 128u) << 15u);
}

void Staged(std::span<std::uint32_t> output, const Launch& launch, const std::vector<std::uint32_t>& expected, bool split, const std::string& name) {
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::SplitWaveGroups, false);
    Same(Run(output, launch), expected, name + " with the switch unset");
    const auto before = AgcDriver::VulkanDevice::SplitWaveDispatches();
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::SplitWaveGroups, true);
    Same(Run(output, launch), expected, name + " at the first dispatch with the switch set");
    Require(AgcDriver::VulkanDevice::SplitWaveDispatches() == before, name + ": the first dispatch with the switch set ran the split form before its pipeline was built in the background");
    AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
    Same(Run(output, launch), expected, name + " after the background build with the switch set");
    Require(AgcDriver::VulkanDevice::SplitWaveDispatches() == before + (split ? 1u : 0u), name + (split ? ": the dispatch after the background build did not run the split form" : ": a program the split does not apply to ran it"));
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::SplitWaveGroups, false);
    Same(Run(output, launch), expected, name + " after the switch was unset again");
    Require(AgcDriver::VulkanDevice::SplitWaveDispatches() == before + (split ? 1u : 0u), name + ": the split form ran after the switch was unset");
}

void Dispatches() {
    auto output = GuestOutput();
    Staged(output, {PlaneCode, {16, 16, 1}, Rsrc2(2, 0), 4, 0x41u}, ExpectedWaves(256, 1024), true, "a 16x16 workgroup");
    Staged(output, {LineCode, {256, 1, 1}, Rsrc2(1, 0), 4, 0x41u}, ExpectedWaves(256, 1024), true, "a 256x1 workgroup");
    Staged(output, {LineCode, {192, 1, 1}, Rsrc2(1, 0), 4, 0x41u}, ExpectedWaves(192, 768), true, "a 192x1 workgroup");
    Staged(output, {LdsCode, {16, 16, 1}, Rsrc2(2, 256), 4, 0x41u}, ExpectedWaves(256, 1024), false, "a 16x16 workgroup using LDS");
    Staged(output, {BarrierCode, {16, 16, 1}, Rsrc2(2, 0), 4, 0x41u}, ExpectedWaves(256, 1024), false, "a 16x16 workgroup with a barrier");
    Staged(output, {LineCode, {96, 1, 1}, Rsrc2(1, 0), 4, 0x41u}, ExpectedWaves(96, 384), false, "a 96x1 workgroup");
    Staged(output, {LineCode, {256, 1, 1}, Rsrc2(1, 0), 320, 0x61u}, ExpectedWaves(256, 320), false, "a partial 256x1 workgroup");
    const Launch indirect{PlaneCode, {16, 16, 1}, Rsrc2(2, 0), 4, 0x41u};
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::SplitWaveGroups, true);
    const auto before = AgcDriver::VulkanDevice::SplitWaveDispatches();
    Same(RunIndirect(output, indirect), ExpectedWaves(256, 1024), "an indirect 16x16 dispatch with the switch set");
    AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
    Same(RunIndirect(output, indirect), ExpectedWaves(256, 1024), "an indirect 16x16 dispatch after the background builds");
    Require(AgcDriver::VulkanDevice::SplitWaveDispatches() == before, "an indirect dispatch ran the split form");
    Same(Run(output, indirect), ExpectedWaves(256, 1024), "a direct 16x16 dispatch after the indirect ones");
    Require(AgcDriver::VulkanDevice::SplitWaveDispatches() == before + 1u, "the direct dispatch beside the indirect ones did not run the split form");
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::SplitWaveGroups, false);
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        Require(argc == 1 || mode == "--forms" || mode == "--dispatch", "usage: agc_driver_split_wave_groups_tests [--forms|--dispatch]");
        {
            auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (argc == 1 || mode == "--forms") Forms(*device);
        }
        if (argc == 1 || mode == "--dispatch") Dispatches();
        AgcDriverShutdown_nid_postfix();
        std::puts("split wave group tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
