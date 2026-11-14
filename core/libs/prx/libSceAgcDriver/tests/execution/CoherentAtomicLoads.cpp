#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Groups = 64;

alignas(256) constexpr std::array<std::uint32_t, 29> Code{
    0xbefe0481u, 0x7e020281u, 0xe0c84000u, 0x80000100u, 0xbf8c3f70u, 0xd7600004u, 0x00010101u, 0xbefe04c1u,
    0x4a3a0004u, 0x4e3a3ac0u, 0xd4830008u, 0x00023a80u, 0x7e3c0281u, 0xbefe0408u, 0xbf880002u, 0xe030e004u,
    0x80001e1du, 0xbefe04c1u, 0xbf8c3f70u, 0x7d843c80u, 0xbf13806au, 0xbf85fff7u, 0x7e060281u, 0xbefe0481u,
    0x7e040204u, 0x4a040481u, 0xe0706000u, 0x80000302u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, Groups + 1> Flags{};

std::array<std::uint32_t, 4> FlagsDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Flags.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Flags.size()), 0x10014204u};
}

void CheckLoads(AgcDriver::VulkanDevice& device, bool atomic) {
    const auto descriptor = FlagsDescriptor();
    const std::vector<std::uint32_t> userData(descriptor.begin(), descriptor.end());
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
    std::size_t volatileLoads = 0;
    for (std::size_t cursor = 5; cursor < spirv.size();) {
        const auto length = spirv[cursor] >> 16u;
        Require(length != 0 && length <= spirv.size() - cursor, "coherent atomic loads: truncated SPIR-V instruction");
        const auto op = spirv[cursor] & 0xffffu;
        if (op == spv::OpAtomicLoad) ++atomicLoads;
        if (op == spv::OpLoad && length > 4u && (spirv[cursor + 4] & spv::MemoryAccessVolatileMask) != 0u) ++volatileLoads;
        cursor += length;
    }
    if (atomic) {
        Require(volatileLoads == 0u, "coherent atomic loads: a glc buffer load is still a volatile load");
        Require(atomicLoads >= 2u, "coherent atomic loads: the glc flag load in the waiting loop is not an atomic load");
    } else {
        Require(volatileLoads >= 2u, "coherent atomic loads: the glc flag load in the waiting loop is not a volatile load without APS5_COHERENT_ATOMIC_LOADS");
    }
}

void Dispatch() {
    const auto data = FlagsDescriptor();
    const auto program = reinterpret_cast<std::uintptr_t>(Code.data());
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207, 64, 1, 1,
        0xc0027600u, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600u, 0x213, (4u << 1u) | (1u << 7u),
        0xc0047600u, 0x240, data[0], data[1], data[2], data[3],
        0xc0031500u, Groups, 1, 1, 0x41u,
    };
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Flags.data()), sizeof(Flags));
}

}

int main() {
    try {
        const bool atomic = std::getenv("APS5_COHERENT_ATOMIC_LOADS") != nullptr;
        std::uint32_t subgroupSize = 0;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            CheckLoads(*device, atomic);
            subgroupSize = device->Target().subgroupSize;
        }
        if (!atomic) {
            AgcDriverShutdown_nid_postfix();
            std::puts("coherent atomic loads tests passed (volatile loads; the waiting dispatch runs only with APS5_COHERENT_ATOMIC_LOADS)");
            return 0;
        }
        if (subgroupSize < 32u) {
            AgcDriverShutdown_nid_postfix();
            std::printf("skipped, subgroup size %u cannot hold a wave64 in two lanes\n", subgroupSize);
            return VulkanTestSkipped;
        }
        Flags.fill(0u);
        Dispatch();
        Require(Flags[0] == Groups, "coherent atomic loads: " + std::to_string(Flags[0]) + " tickets taken");
        for (std::uint32_t ticket = 1; ticket <= Groups; ++ticket) {
            Require(Flags[ticket] == 1u, "coherent atomic loads: ticket " + std::to_string(ticket - 1) + " published " + std::to_string(Flags[ticket]));
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("coherent atomic loads tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
