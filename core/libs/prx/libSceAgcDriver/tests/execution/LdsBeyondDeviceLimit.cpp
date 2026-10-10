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
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 12;
constexpr std::uint32_t MaxGroups = 16;
constexpr std::uint32_t AboveLimitBytes = 4096;
constexpr std::uint32_t BelowLimitBytes = 512;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t OpTypeInt = 21;
constexpr std::uint32_t OpTypeFloat = 22;
constexpr std::uint32_t OpTypeVector = 23;
constexpr std::uint32_t OpTypeArray = 28;
constexpr std::uint32_t OpTypeStruct = 30;
constexpr std::uint32_t OpTypePointer = 32;
constexpr std::uint32_t OpConstant = 43;
constexpr std::uint32_t OpVariable = 59;
constexpr std::uint32_t StorageClassWorkgroup = 4;
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, MaxGroups * Threads * Results> Output{};
alignas(256) std::array<std::uint32_t, 4> IndirectGroups{};

alignas(256) constexpr std::array<std::uint32_t, 69> Code{
    0x34020084, 0xe0381000, 0x80000401, 0x7e100208, 0x7e3e0209, 0x343e3e82, 0x4a103f08, 0x7e40020a,
    0x34404083, 0x4a104108, 0x34121090, 0x4a121300, 0x4a1412ff, 0x10000000, 0x7e160280, 0x7e180280,
    0x7e1a0281, 0x7e1e0281, 0x7e200280, 0xbf8c0000, 0xd9340000, 0x00000b06, 0xd834000c, 0x00000b06,
    0xd8340000, 0x00000904, 0xd8340000, 0x00000a05, 0xd8340000, 0x00000907, 0xbf8c0000, 0xbf8a0000,
    0xd880000c, 0x28000d06, 0xd9800000, 0x29000f06, 0x3a260884, 0x3a2a0aff, 0x00000080, 0x363608ff,
    0xfffffff8, 0xbf8c0000, 0xbf8a0000, 0xd8d80000, 0x2b000013, 0xd8d80000, 0x2c000015, 0xd8d80000,
    0x2d000007, 0xd8d8000c, 0x2e000006, 0xd9d80000, 0x2f000006, 0xd9d80000, 0x3100001b, 0xd8d80000,
    0x33000005, 0x343c1086, 0x4a3c3d00, 0x343c3c86, 0xbf8c0000, 0xe0781000, 0x8001281e, 0xe0781010,
    0x80012c1e, 0xe0781020, 0x8001301e, 0xbf8c0000, 0xbf810000,
};

constexpr const char* Names[Checked] = {
    "ds_add_rtn_u32 result", "ds_add_rtn_u64 result low", "ds_add_rtn_u64 result high", "neighbour's word above the limit",
    "other wave's word below the limit", "out-of-range read", "final 32-bit counter", "final 64-bit counter low",
    "final 64-bit counter high", "ds_read_b64 above the limit, low", "ds_read_b64 above the limit, high", "own word below the limit",
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

std::uint64_t WorkgroupBytes(std::span<const std::uint32_t> words) {
    std::unordered_map<std::uint32_t, std::span<const std::uint32_t>> types;
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    std::vector<std::uint32_t> pointers;
    for (std::size_t at = 5; at < words.size();) {
        const auto count = words[at] >> 16u;
        Require(count != 0u && at + count <= words.size(), "lds beyond the device limit: malformed SPIR-V at word " + std::to_string(at));
        const auto instruction = words.subspan(at, count);
        const auto opcode = instruction[0] & 0xffffu;
        if (opcode == OpTypeInt || opcode == OpTypeFloat || opcode == OpTypeVector || opcode == OpTypeArray || opcode == OpTypeStruct || opcode == OpTypePointer) types[instruction[1]] = instruction;
        if (opcode == OpConstant) constants[instruction[2]] = instruction[3];
        if (opcode == OpVariable && instruction[3] == StorageClassWorkgroup) pointers.push_back(instruction[1]);
        at += count;
    }
    const std::function<std::uint64_t(std::uint32_t)> size = [&](std::uint32_t id) -> std::uint64_t {
        const auto type = types.find(id);
        Require(type != types.end(), "lds beyond the device limit: a Workgroup variable has a type the test does not size");
        const auto& definition = type->second;
        switch (definition[0] & 0xffffu) {
        case OpTypeInt:
        case OpTypeFloat:
            return definition[2] / 8u;
        case OpTypeVector:
            return size(definition[2]) * definition[3];
        case OpTypeArray: {
            const auto length = constants.find(definition[3]);
            Require(length != constants.end(), "lds beyond the device limit: a Workgroup array has a length that is not a constant");
            return size(definition[2]) * length->second;
        }
        case OpTypeStruct: {
            std::uint64_t bytes = 0;
            for (const auto member : definition.subspan(2)) bytes += size(member);
            return bytes;
        }
        default:
            throw std::runtime_error("lds beyond the device limit: a Workgroup variable has a type the test does not size");
        }
    };
    std::uint64_t total = 0;
    for (const auto pointer : pointers) {
        const auto type = types.find(pointer);
        Require(type != types.end() && (type->second[0] & 0xffffu) == OpTypePointer, "lds beyond the device limit: a Workgroup variable is not a pointer");
        total += size(type->second[3]);
    }
    return total;
}

ShaderRecompiler::RecompileResult Compile(std::uint32_t waveSize, std::uint32_t ldsBytes, const ShaderRecompiler::SpirvTarget& target) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, ldsBytes / 4u, {true, true, true}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::uint32_t Group(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (z << 3u) | (y << 2u) | x;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t groupsX, std::uint32_t groupsY, std::uint32_t groupsZ, bool indirect, const std::string& run) {
    const auto limit = target.maxWorkgroupSharedMemoryBytes;
    Require(limit % 8u == 0u && limit >= 2048u, "lds beyond the device limit: unexpected maxComputeSharedMemorySize " + std::to_string(limit));
    const auto fits = [&](const ShaderRecompiler::RecompileResult& compiled, const std::string& what) {
        const auto bytes = WorkgroupBytes(compiled.spirv.Words());
        Require(bytes <= limit, "lds beyond the device limit " + run + ": " + what + " declares " + std::to_string(bytes) + " bytes of Workgroup memory, over the device's " + std::to_string(limit));
    };
    const auto within = Compile(waveSize, limit - BelowLimitBytes, target);
    Require(within.workgroupMemoryDwords == 0u, "lds beyond the device limit " + run + ": LDS below the device limit left workgroup memory");
    fits(within, "LDS below the device limit");
    const auto atLimit = Compile(waveSize, limit, target);
    Require(atLimit.workgroupMemoryDwords == limit / 4u + 1u, "lds beyond the device limit " + run + ": LDS at the device limit gave " + std::to_string(atLimit.workgroupMemoryDwords) + " device memory dwords per workgroup");
    fits(atLimit, "LDS at the device limit");
    const auto bytes = limit + AboveLimitBytes;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = limit + tid * 4u;
        Input[tid * 4u + 1u] = 1024u + tid * 4u;
        Input[tid * 4u + 2u] = bytes - 16u;
        Input[tid * 4u + 3u] = bytes + tid * 4u;
    }
    Output.fill(Untouched);
    const auto result = Compile(waveSize, bytes, target);
    Require(result.workgroupMemoryDwords == bytes / 4u + 1u, "lds beyond the device limit " + run + ": " + std::to_string(bytes) + " bytes of LDS gave " + std::to_string(result.workgroupMemoryDwords) + " device memory dwords per workgroup");
    fits(result, "LDS above the device limit");
    if (indirect) {
        IndirectGroups = {groupsX, groupsY, groupsZ, 0u};
        const auto outcome = device.DispatchIndirect(result, reinterpret_cast<std::uintptr_t>(IndirectGroups.data()), {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        Require(outcome.cpuReason == 8, "lds beyond the device limit " + run + ": the indirect group counts were not read on the CPU (reason " + std::to_string(outcome.cpuReason) + ")");
    } else {
        device.Dispatch(result, groupsX, groupsY, groupsZ, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    }
    device.WaitIdle();
    std::vector<bool> dispatched(MaxGroups, false);
    for (std::uint32_t z = 0; z < groupsZ; ++z) {
        for (std::uint32_t y = 0; y < groupsY; ++y) {
            for (std::uint32_t x = 0; x < groupsX; ++x) dispatched[Group(x, y, z)] = true;
        }
    }
    for (std::uint32_t group = 0; group < MaxGroups; ++group) {
        const std::string where = "lds beyond the device limit " + run + ", group " + std::to_string(group);
        std::vector<std::uint32_t> counter32;
        std::vector<std::uint32_t> counter64;
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto* lane = &Output[(group * Threads + tid) * Results];
            if (!dispatched[group]) {
                Require(std::all_of(lane, lane + Results, [](std::uint32_t word) { return word == Untouched; }), where + ": lane " + std::to_string(tid) + " of a group that was not dispatched was written");
                continue;
            }
            const auto value = [&](std::uint32_t laneId) { return (group << 16u) | laneId; };
            const std::array<std::uint32_t, Checked> expected{
                lane[0], lane[1], 0u, value(tid ^ 1u),
                value(tid ^ 32u) + 0x10000000u, 0u, Threads, Threads,
                0u, value(tid & ~1u), value(tid | 1u), value(tid) + 0x10000000u,
            };
            for (std::uint32_t index = 0; index < Checked; ++index) {
                Require(lane[index] == expected[index], where + ": lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(lane[index]) + ", expected " + Hex(expected[index]));
            }
            counter32.push_back(lane[0]);
            counter64.push_back(lane[1]);
        }
        if (!dispatched[group]) continue;
        std::sort(counter32.begin(), counter32.end());
        std::sort(counter64.begin(), counter64.end());
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Require(counter32[tid] == tid && counter64[tid] == tid, where + ": the LDS atomics returned " + Hex(counter32[tid]) + " / " + Hex(counter64[tid]) + " as their " + std::to_string(tid) + "th smallest old value");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32, device->Target(), 3, 2, 1, false, "wave32 3x2");
        Run(*device, 32, device->Target(), 4, 2, 1, false, "wave32 4x2");
        Run(*device, 64, device->Target(), 3, 2, 1, false, "wave64 3x2");
        Run(*device, 64, device->ComputeTarget(32), 4, 2, 1, false, "wave64 split 4x2");
        Run(*device, 64, device->Target(), 2, 2, 2, false, "wave64 2x2x2");
        Run(*device, 32, device->Target(), 2, 2, 2, true, "wave32 indirect 2x2x2");
        std::puts("lds beyond the device limit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
