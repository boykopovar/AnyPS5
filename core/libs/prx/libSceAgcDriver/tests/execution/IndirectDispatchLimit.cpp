#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t FoldedColumns = 2;
constexpr std::uint32_t FoldedCapacity = FoldedColumns * 65536u;
alignas(256) std::array<std::uint32_t, Threads> Output{};
alignas(256) std::array<std::uint32_t, FoldedCapacity> GroupOutput{};

alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{0x34020082u, 0x7e040287u, 0xe0701000u, 0x80010201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 10> GroupCode{0x8f0a8109u, 0x800a080au, 0x8f0b9009u, 0x880b080bu, 0x7e02020bu, 0x7e04020au, 0x34040482u, 0xe0701000u, 0x80010102u, 0xbf810000u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), count * 4u, 0x31016facu};
}

int Run(AgcDriver::VulkanDevice& device, std::uint32_t* block, std::array<std::uint32_t, 3> groups, std::span<const std::uint32_t> code, std::array<bool, 3> groupIds, std::span<std::uint32_t> output, bool threadDimensions = false) {
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    std::memcpy(block, groups.data(), sizeof(groups));
    std::vector<std::uint32_t> userData(8, 0u);
    const auto descriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size()));
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, groupIds, false, 1};
    compute.groupFold = true;
    if (threadDimensions) compute.partialThreads = {~0u, ~0u, ~0u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, threadDimensions ? 0u : 128u}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const std::array<std::uint32_t, 3> threadGroup = threadDimensions ? std::array<std::uint32_t, 3>{Threads, 1u, 1u} : std::array<std::uint32_t, 3>{};
    const auto outcome = device.DispatchIndirect(result, reinterpret_cast<std::uintptr_t>(block), {}, reinterpret_cast<std::uintptr_t>(code.data()), nullptr, nullptr, threadGroup);
    device.WaitIdle();
    return outcome.cpuReason;
}

std::string Text(const std::array<std::uint32_t, 3>& groups) {
    return std::to_string(groups[0]) + "x" + std::to_string(groups[1]) + "x" + std::to_string(groups[2]);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto* block = static_cast<std::uint32_t*>(::operator new(BlockBytes, std::align_val_t{BlockBytes}));
        std::memset(block, 0, BlockBytes);
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, BlockBytes, true, true, true);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        int failures = 0;
        try {
            const auto* limit = device->WorkGroupCountLimit();
            const std::array<std::uint32_t, 3> invalid{1u, 0xffffffffu, 1u};
            const std::array<std::uint32_t, 3> folded{FoldedColumns, limit[1] + 1u, 1u};
            const std::array<bool, 3> noGroupIds{false, false, false};
            if (limit[1] == 0xffffffffu) {
                std::puts("skipped, the device reports no Y workgroup count limit below 2^32");
            } else if (const int path = Run(*device, block, invalid, StoreCode, noGroupIds, Output); path != 0) {
                std::printf("skipped, the group counts were read on the CPU (reason %d): the device cannot import the block\n", path);
            } else {
                for (std::uint32_t tid = 0; tid < Threads; ++tid) Require(Output[tid] == 0xdeadbeefu, "indirect dispatch limit: thread " + std::to_string(tid) + " ran with " + Text(invalid) + " groups, over 2^32 invocations");
                if (static_cast<std::uint64_t>(folded[0]) * folded[1] > FoldedCapacity || static_cast<std::uint64_t>(folded[0]) * folded[1] > limit[0]) {
                    std::printf("indirect dispatch fold: skipped, %s groups do not fit the test buffer or the device's X limit %u\n", Text(folded).c_str(), limit[0]);
                } else {
                    const auto groups = folded[0] * folded[1];
                    const auto check = [&](const std::string& what) {
                        for (std::uint32_t group = 0; group < groups; ++group) {
                            const auto expected = ((group / FoldedColumns) << 16u) | (group % FoldedColumns);
                            Require(GroupOutput[group] == expected, what + ": group " + std::to_string(group) + " of " + Text(folded) + " stored " + std::to_string(GroupOutput[group]) + ", expected " + std::to_string(expected));
                        }
                    };
                    Require(Run(*device, block, folded, GroupCode, {true, true, false}, std::span(GroupOutput).first(groups)) == 0, "indirect dispatch fold: the counts left the GPU path");
                    check("indirect dispatch fold");
                    const std::array<std::uint32_t, 3> threads{FoldedColumns * Threads - 1u, folded[1], 1u};
                    Require(Run(*device, block, threads, GroupCode, {true, true, false}, std::span(GroupOutput).first(groups), true) == 0, "indirect dispatch fold: the thread counts left the GPU path");
                    check("indirect dispatch fold of " + Text(threads) + " threads");
                }
                Require(Run(*device, block, {1u, 1u, 1u}, StoreCode, noGroupIds, Output) == 0, "indirect dispatch limit: valid counts left the GPU path");
                for (std::uint32_t tid = 0; tid < Threads; ++tid) Require(Output[tid] == 7u, "indirect dispatch limit: thread " + std::to_string(tid) + " stored " + std::to_string(Output[tid]) + " with one group, expected 7");
            }
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            failures = 1;
        }
        mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        ::operator delete(block, std::align_val_t{BlockBytes});
        if (failures != 0) return 1;
        std::puts("indirect dispatch limit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
