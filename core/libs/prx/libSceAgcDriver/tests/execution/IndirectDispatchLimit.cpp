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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{0x34020082u, 0x7e040287u, 0xe0701000u, 0x80010201u, 0xbf810000u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), count * 4u, 0x31016facu};
}

std::optional<std::array<std::uint32_t, 3>> OverLimit(const std::uint32_t* limit) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (limit[axis] == 0xffffffffu) continue;
        std::array<std::uint32_t, 3> groups{1u, 1u, 1u};
        groups[axis] = limit[axis] + 1u;
        return groups;
    }
    return std::nullopt;
}

int Run(AgcDriver::VulkanDevice& device, std::uint32_t* block, std::array<std::uint32_t, 3> groups) {
    Output.fill(0xdeadbeefu);
    std::memcpy(block, groups.data(), sizeof(groups));
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), Threads);
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
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
    const auto outcome = device.DispatchIndirect(result, reinterpret_cast<std::uintptr_t>(block), {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return outcome.cpuReason;
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
            const auto over = OverLimit(device->WorkGroupCountLimit());
            const int invalidPath = over ? Run(*device, block, *over) : 0;
            const auto overText = over ? std::to_string((*over)[0]) + "x" + std::to_string((*over)[1]) + "x" + std::to_string((*over)[2]) : std::string();
            if (!over) {
                std::puts("skipped, every group count is within the device limits");
            } else if (invalidPath != 0) {
                std::printf("skipped, the group counts were read on the CPU (reason %d): the device cannot import the block\n", invalidPath);
            } else {
                for (std::uint32_t tid = 0; tid < Threads; ++tid) Require(Output[tid] == 0xdeadbeefu, "indirect dispatch limit: thread " + std::to_string(tid) + " ran with " + overText + " groups");
                Require(Run(*device, block, {1u, 1u, 1u}) == 0, "indirect dispatch limit: valid counts left the GPU path");
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
