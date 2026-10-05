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
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Written = 5;
constexpr std::uint32_t SharedWords = 2;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t MaskedExecLo = 0x55555555u;
constexpr std::uint32_t SharedPartial = 0x800186a0u;
constexpr std::uint32_t SharedExhausted = 1000u;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results + SharedWords> Output{};

alignas(256) constexpr std::array<std::uint32_t, 35> Code{
    0x34020084, 0x34040085, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xbf8c3f70, 0x7e140280, 0x461414c1, 0x481414c1, 0x7e0e0304, 0xe0d05000, 0x80010702, 0x7e100304,
    0xbefe03ff, 0x55555555, 0xe0d05004, 0x80010802, 0xbefe03c1, 0x7e120305, 0xe0d04800, 0x80010900,
    0x7e160306, 0xe0d04804, 0x80010b00, 0xbf8c3f70, 0xe0701008, 0x80010702, 0xe070100c, 0x80010802,
    0xe0701010, 0x80010a02, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> NoReturnCode{
    0xe0d01000, 0x80010702, 0xbf810000,
};

struct Workgroup {
    std::uint32_t threads;
    std::uint32_t waveSize;

    [[nodiscard]] std::string Name() const {
        return "wave" + std::to_string(waveSize) + " " + std::to_string(threads) + " threads";
    }
};

constexpr std::array<Workgroup, 3> Workgroups{{{32, 32}, {64, 32}, {64, 64}}};

struct Row {
    std::uint32_t memory;
    std::uint32_t subtrahend;
};

constexpr std::array<Row, 16> Edges{{
    {0u, 0u}, {0u, 1u}, {1u, 0u}, {1u, 1u},
    {5u, 7u}, {7u, 5u}, {0xffffffffu, 1u}, {1u, 0xffffffffu},
    {0x80000000u, 0x7fffffffu}, {0x7fffffffu, 0x80000000u}, {0xffffffffu, 0xffffffffu}, {0x80000000u, 0x80000000u},
    {0x80000001u, 0x80000000u}, {0u, 0xffffffffu}, {0xffffffffu, 0u}, {0x12345678u, 0x12345679u},
}};

Row RowOf(std::uint32_t tid) {
    if (tid < Edges.size()) return Edges[tid];
    const std::uint32_t memory = (tid + 1u) * 0x9e3779b9u;
    const std::uint32_t subtrahend = (tid + 1u) * 0x85ebca6bu;
    return {memory, (tid & 3u) == 0u ? subtrahend >> 8u : subtrahend};
}

std::uint32_t SharedPartialStep(std::uint32_t tid) { return tid + 1u; }
std::uint32_t SharedExhaustedStep(std::uint32_t tid) { return tid + 100u; }

std::uint32_t SubtractClamped(std::uint32_t memory, std::uint32_t subtrahend) {
    return memory >= subtrahend ? memory - subtrahend : 0u;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

ShaderRecompiler::RecompileResult Recompile(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const Workgroup& workgroup, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{workgroup.threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {workgroup.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup) {
    Input.fill(0u);
    Output.fill(Untouched);
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        const Row row = RowOf(tid);
        Input[tid * Inputs] = row.subtrahend;
        Input[tid * Inputs + 1u] = SharedPartialStep(tid);
        Input[tid * Inputs + 2u] = SharedExhaustedStep(tid);
        Output[tid * Results] = row.memory;
        Output[tid * Results + 1u] = row.memory;
    }
    Output[MaxThreads * Results] = SharedPartial;
    Output[MaxThreads * Results + 1u] = SharedExhausted;
    const auto userData = UserData();
    const std::span<const std::uint32_t> code(Code);
    const auto result = Recompile(device, code, workgroup, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Workgroup& workgroup) {
    constexpr std::array<const char*, 4> names{"memory", "memory under a partial exec mask", "returned value", "data register under a partial exec mask"};
    std::uint32_t partialTotal = 0;
    std::uint32_t exhaustedTotal = 0;
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        const Row row = RowOf(tid);
        const auto* out = &Output[tid * Results];
        const auto where = "buffer atomic csub: " + workgroup.Name() + " thread " + std::to_string(tid);
        if (tid >= workgroup.threads) {
            Require(out[0] == row.memory && out[1] == row.memory, where + " is outside the workgroup but its memory changed");
            for (std::uint32_t j = 2; j < Results; ++j) {
                Require(out[j] == Untouched, where + " is outside the workgroup but result " + std::to_string(j) + " was written");
            }
            continue;
        }
        partialTotal += SharedPartialStep(tid);
        exhaustedTotal += SharedExhaustedStep(tid);
        const auto lane = out[4];
        Require(lane < workgroup.waveSize, where + " reports lane " + std::to_string(lane));
        const bool active = lane >= 32u || ((MaskedExecLo >> lane) & 1u) != 0u;
        const std::uint32_t clamped = SubtractClamped(row.memory, row.subtrahend);
        const std::array<std::uint32_t, 4> expected{clamped, active ? clamped : row.memory, row.memory, active ? row.memory : row.subtrahend};
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            Require(out[j] == expected[j], where + " lane " + std::to_string(lane) + " " + names[j] + " is " + Hex(out[j]) + ", expected " + Hex(expected[j]));
        }
        for (std::uint32_t j = Written; j < Results; ++j) {
            Require(out[j] == Untouched, where + " result " + std::to_string(j) + " was written");
        }
    }
    const std::array<std::uint32_t, SharedWords> shared{SubtractClamped(SharedPartial, partialTotal), SubtractClamped(SharedExhausted, exhaustedTotal)};
    constexpr std::array<const char*, SharedWords> sharedNames{"dword shared by all lanes", "dword exhausted by all lanes"};
    Require(shared[0] != 0u && shared[1] == 0u, "buffer atomic csub: " + workgroup.Name() + " does not cover both shared outcomes");
    for (std::uint32_t j = 0; j < SharedWords; ++j) {
        const auto actual = Output[MaxThreads * Results + j];
        Require(actual == shared[j], "buffer atomic csub: " + workgroup.Name() + " " + sharedNames[j] + " is " + Hex(actual) + ", expected " + Hex(shared[j]));
    }
}

void CheckNoReturnRejected(const AgcDriver::VulkanDevice& device) {
    const auto userData = UserData();
    const std::string reason = "buffer_atomic_csub without glc is not supported";
    std::string failure;
    try {
        static_cast<void>(Recompile(device, NoReturnCode, Workgroups[0], userData));
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find(reason) != std::string::npos, "buffer atomic csub: expected '" + reason + "', got '" + failure + "'");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto& workgroup : Workgroups) {
            Run(*device, workgroup);
            Check(workgroup);
        }
        CheckNoReturnRejected(*device);
        std::puts("buffer atomic csub tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
