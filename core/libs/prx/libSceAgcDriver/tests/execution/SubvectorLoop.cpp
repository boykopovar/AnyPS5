#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "../SubvectorLoopFixture.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using SubvectorLoopFixture::ExecKeptCode;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Columns = 9;
constexpr std::uint32_t Ones = 0xffffffffu;
constexpr std::uint32_t Marker = 0x5a5a5a5au;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads * Columns> Output{};

alignas(256) constexpr std::array<std::uint32_t, 50> ExecResetCode{
    0x34020082, 0xe0301000, 0x80000401, 0xbf8c3f70, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280,
    0x7e1c0280, 0xbe940380, 0xbea603ff, 0x5a5a5a5a, 0x7daa0880, 0xbda6000b, 0x80148114, 0x4a141481,
    0x7e160214, 0x7e180226, 0xbe96047e, 0x7e1a0216, 0x7e1c0217, 0xbf098414, 0xbf850002, 0xbefe04c1,
    0xbe26fff5, 0xbe98047e, 0xbefe04c1, 0x7e1e0226, 0x7e200218, 0x7e220219, 0x7e240214, 0xe0701000,
    0x80010a01, 0xe0701100, 0x80010b01, 0xe0701200, 0x80010c01, 0xe0701300, 0x80010d01, 0xe0701400,
    0x80010e01, 0xe0701500, 0x80010f01, 0xe0701600, 0x80011001, 0xe0701700, 0x80011101, 0xe0701800,
    0x80011201, 0xbf810000,
};

constexpr std::array<const char*, Columns> Names{"passes", "last pass", "s38 in the pass", "exec lo in the pass", "exec hi in the pass", "s38 after the loop", "exec lo after the loop", "exec hi after the loop", "total passes"};

struct Group {
    std::uint32_t first;
    std::uint32_t last;
    std::array<std::uint32_t, Columns> values;
};

struct Case {
    const char* name;
    std::uint64_t mask;
    std::vector<Group> groups;
};

const std::vector<Case> ExecKeptCases{
    {"all lanes", ~0ull, {
        {0, 31, {1, 1, Ones, Ones, 0, Ones, Ones, Ones, 2}},
        {32, 63, {1, 2, Ones, 0, Ones, Ones, Ones, Ones, 2}},
    }},
    {"low half", 0xffffffffull, {
        {0, 31, {1, 1, 0, Ones, 0, 0, Ones, 0, 1}},
        {32, 63, {0, 0, 0, 0, 0, 0, Ones, 0, 1}},
    }},
    {"high half", 0xffffffffull << 32, {
        {0, 31, {0, 0, 0, 0, 0, 0, 0, Ones, 1}},
        {32, 63, {1, 1, 0, 0, Ones, 0, 0, Ones, 1}},
    }},
    {"no lane", 0ull, {
        {0, 63, {0, 0, 0, 0, 0, Marker, 0, 0, 0}},
    }},
    {"lanes 3 and 40", (1ull << 3) | (1ull << 40), {
        {0, 63, {0, 0, 0, 0, 0, 8, 8, 0x100u, 2}},
        {3, 3, {1, 1, 0x100u, 8, 0, 8, 8, 0x100u, 2}},
        {40, 40, {1, 2, 8, 0, 0x100u, 8, 8, 0x100u, 2}},
    }},
    {"lane 3", 1ull << 3, {
        {0, 63, {0, 0, 0, 0, 0, 0, 8, 0, 1}},
        {3, 3, {1, 1, 0, 8, 0, 0, 8, 0, 1}},
    }},
    {"lane 40", 1ull << 40, {
        {0, 63, {0, 0, 0, 0, 0, 0, 0, 0x100u, 1}},
        {40, 40, {1, 1, 0, 0, 0x100u, 0, 0, 0x100u, 1}},
    }},
};

const std::vector<Case> ExecResetCases{
    {"all lanes", ~0ull, {
        {0, 63, {0, 0, 0, 0, 0, Ones, Ones, Ones, 1}},
        {0, 31, {1, 1, Ones, Ones, 0, Ones, Ones, Ones, 1}},
    }},
    {"low half", 0xffffffffull, {
        {0, 63, {0, 0, 0, 0, 0, 0, 0, Ones, 1}},
        {0, 31, {1, 1, 0, Ones, 0, 0, 0, Ones, 1}},
    }},
    {"high half", 0xffffffffull << 32, {
        {0, 63, {0, 0, 0, 0, 0, 0, 0, Ones, 1}},
        {32, 63, {1, 1, 0, 0, Ones, 0, 0, Ones, 1}},
    }},
    {"no lane", 0ull, {
        {0, 63, {0, 0, 0, 0, 0, Marker, 0, 0, 0}},
    }},
    {"lanes 3 and 40", (1ull << 3) | (1ull << 40), {
        {0, 63, {0, 0, 0, 0, 0, 0x100u, 0x100u, Ones, 1}},
        {3, 3, {1, 1, 0x100u, 8, 0, 0x100u, 0x100u, Ones, 1}},
    }},
    {"lane 3", 1ull << 3, {
        {0, 63, {0, 0, 0, 0, 0, 0, 0, Ones, 1}},
        {3, 3, {1, 1, 0, 8, 0, 0, 0, Ones, 1}},
    }},
    {"lane 40", 1ull << 40, {
        {0, 63, {0, 0, 0, 0, 0, 0, 0, Ones, 1}},
        {40, 40, {1, 1, 0, 0, 0x100u, 0, 0, Ones, 1}},
    }},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint64_t mask) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) Input[lane] = ((mask >> lane) & 1u) != 0u ? 1u : 0u;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* variant, const Case& testCase) {
    std::array<std::array<std::uint32_t, Columns>, Threads> expected{};
    for (const auto& group : testCase.groups) {
        for (std::uint32_t lane = group.first; lane <= group.last; ++lane) expected[lane] = group.values;
    }
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            const auto actual = Output[column * Threads + lane];
            Require(actual == expected[lane][column], std::string("subvector loop ") + variant + ", initial exec " + testCase.name + ": lane " + std::to_string(lane) + " " + Names[column] + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[lane][column]));
        }
    }
}

void CheckWave32Throws(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    try {
        static_cast<void>(ShaderRecompiler::Recompile(request));
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("s_subvector_loop_begin in a wave32 shader is not measured") != std::string::npos, std::string("wave32 subvector loop threw an unexpected error: ") + error.what());
        return;
    }
    Require(false, "a wave32 subvector loop was recompiled");
}

void RunCases(AgcDriver::VulkanDevice& device, const char* variant, std::span<const std::uint32_t> code, const std::vector<Case>& cases) {
    for (const auto& testCase : cases) {
        Run(device, code, testCase.mask);
        Check(variant, testCase);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckWave32Throws(*device, ExecKeptCode);
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        RunCases(*device, "exec kept by the body", ExecKeptCode, ExecKeptCases);
        RunCases(*device, "body resets exec", ExecResetCode, ExecResetCases);
        std::puts("subvector loop tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
