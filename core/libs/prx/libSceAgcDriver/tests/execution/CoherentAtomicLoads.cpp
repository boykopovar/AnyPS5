#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

alignas(256) constexpr std::array<std::uint32_t, 29> Code{
    0xbefe0481u, 0x7e020281u, 0xe0c84000u, 0x80000100u, 0xbf8c3f70u, 0xd7600004u, 0x00010101u, 0xbefe04c1u,
    0x4a3a0004u, 0x4e3a3ac0u, 0xd4830008u, 0x00023a80u, 0x7e3c0281u, 0xbefe0408u, 0xbf880002u, 0xe030e004u,
    0x80001e1du, 0xbefe04c1u, 0xbf8c3f70u, 0x7d843c80u, 0xbf13806au, 0xbf85fff7u, 0x7e060281u, 0xbefe0481u,
    0x7e040204u, 0x4a040481u, 0xe0706000u, 0x80000302u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 1025> Flags{};

void CheckLoads(AgcDriver::VulkanDevice& device) {
    const auto address = reinterpret_cast<std::uintptr_t>(Flags.data());
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Flags.size()), 0x00014204u};
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto spirv = result.spirv.Words();
    std::size_t atomicLoads = 0;
    for (std::size_t cursor = 5; cursor < spirv.size();) {
        const auto length = spirv[cursor] >> 16u;
        Require(length != 0 && length <= spirv.size() - cursor, "coherent atomic loads: truncated SPIR-V instruction");
        const auto op = spirv[cursor] & 0xffffu;
        if (op == spv::OpAtomicLoad) ++atomicLoads;
        Require(op != spv::OpLoad || length <= 4u || (spirv[cursor + 4] & spv::MemoryAccessVolatileMask) == 0u, "coherent atomic loads: a glc buffer load is still a volatile load");
        cursor += length;
    }
    Require(atomicLoads >= 2u, "coherent atomic loads: the glc flag load in the waiting loop is not an atomic load");
}

}

int main() {
    try {
#ifdef _WIN32
        Require(_putenv_s("APS5_COHERENT_ATOMIC_LOADS", "1") == 0, "cannot set APS5_COHERENT_ATOMIC_LOADS");
#else
        Require(setenv("APS5_COHERENT_ATOMIC_LOADS", "1", 1) == 0, "cannot set APS5_COHERENT_ATOMIC_LOADS");
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckLoads(*device);
        std::puts("coherent atomic loads tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
