#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::DescriptorKind;
using ShaderRecompiler::DescriptorRole;

constexpr std::uint32_t Fill = 0xdeadbeefu;
constexpr std::uint32_t InputWord3 = 0x2004a3acu;
constexpr std::array<std::uint32_t, 3> Expected{0x3fa00000u, 0xc0200000u, 0x41980000u};
alignas(256) const std::array<std::uint32_t, 4> Input{Expected[0], Expected[1], Expected[2], 0x42c60000u};
alignas(256) std::array<std::uint32_t, 4> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0xe0082000u, 0x80000200u, 0xbf8c3f70u,
    0xe0701000u, 0x80010200u,
    0xe0701004u, 0x80010300u,
    0xe0701008u, 0x80010400u, 0xbf810000u,
};

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t records, std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    Require(address != 0 && address <= 0xffffffffffffull, "fixture address does not fit Base48");
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), records, word3};
}

void RequireSsboInput(const ShaderRecompiler::RecompileResult& result, const std::array<std::uint32_t, 4>& input) {
    Require(result.vertexInputs.empty() && result.vertexAttributes.empty(), "fixture unexpectedly uses native vertex attributes");
    bool found = false;
    for (const auto& binding : result.bindings) {
        Require(binding.role != DescriptorRole::BdaPagetable, "fixture unexpectedly uses a BDA pagetable");
        if (binding.role != DescriptorRole::GuestBuffers) continue;
        Require(binding.kind == DescriptorKind::StorageBuffer, "fixture guest buffers are not SSBOs");
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4, "malformed fixture SSBO descriptors");
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto* words = binding.guestDescriptor.data() + 4 * element;
            if (std::equal(input.begin(), input.end(), words)) found = true;
        }
    }
    Require(found, "the exact stride0/format74/OOB2 input V# is absent from the SSBO binding");
}

std::array<std::uint32_t, 3> Run(AgcDriver::VulkanDevice& device, std::uint32_t records) {
    Output.fill(Fill);
    const auto input = Descriptor(Input.data(), records, InputWord3);
    const auto output = Descriptor(Output.data(), 12u, 0x31016facu);
    std::array<std::uint32_t, 8> userData{};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0u, {false, false, false}, false, 1};
    auto target = device.ComputeTarget(32);
    target.registrationModules = false;
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    RequireSsboInput(result, input);
    Require(!result.registrationModule, "fixture did not use the specialized compute module");
    std::printf("case records=%u input=%08x,%08x,%08x,%08x path=compute_guestbuffers_ssbo native_vertex_attributes=0 robust2=%u exact_robust=%u\n",
        records, input[0], input[1], input[2], input[3], target.robustBufferAccess2 ? 1u : 0u, target.exactRobustLoads ? 1u : 0u);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    std::printf("result records=%u xyz=%08x,%08x,%08x guard=%08x expected=%08x,%08x,%08x\n",
        records, Output[0], Output[1], Output[2], Output[3], Expected[0], Expected[1], Expected[2]);
    Require(Output[3] == Fill, "fixture wrote beyond its three output DWORDs");
    return {Output[0], Output[1], Output[2]};
}

}

int main() {
    try {
        static_assert(((InputWord3 >> 12u) & 127u) == 74u);
        static_assert(((InputWord3 >> 28u) & 3u) == 2u);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto records12 = Run(*device, 12u);
        const auto records1 = Run(*device, 1u);
        Require(records12 == Expected, "NumRecords=12 control did not preserve RGB32_FLOAT values");
        Require(records1 == records12 && records1 == Expected, "OOB_SELECT=2 NumRecords=1 differs from NumRecords=12; nonempty OOB2 must preserve the float3");
        std::puts("stride0 format74 OOB2 SSBO NumRecords=1/12 regression passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
