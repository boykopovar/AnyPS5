#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x00010004, 0xd7610003, 0x00010205, 0xd7610003, 0x00010406,
    0xd7610003, 0x00010607, 0xbe840380, 0xbe850380, 0xbe860380, 0xbe870380, 0xd7600008, 0x00010103, 0xd7600009,
    0x00010303, 0xd760000a, 0x00010503, 0xd760000b, 0x00010703, 0xe0702000, 0x80020001, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 27> IndexedStoreCode{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x00010004, 0xd7610003, 0x00010205, 0xd7610003, 0x00010406,
    0xd7610003, 0x00010607, 0x7e048500, 0xbe840380, 0xbe850380, 0xbe860380, 0xbe870380, 0xd7600008, 0x00010103,
    0xd7600009, 0x00010303, 0xd760000a, 0x00010503, 0xd760000b, 0x00010703, 0xe0702000, 0x80020001, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::string& name) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == tid, name + ": lane " + std::to_string(tid) + " stored " + std::to_string(Output[tid]) + " through the V# read back from a window register");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < Threads) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device, Code);
        Check("window lane spill");
        Run(*device, IndexedStoreCode);
        Check("window lane spill past an indexed store");
        std::puts("window lane spill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
