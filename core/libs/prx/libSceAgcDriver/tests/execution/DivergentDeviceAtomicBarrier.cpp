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

constexpr std::uint32_t Workgroups = 16;
constexpr std::uint32_t ThreadsPerWorkgroup = 256;
constexpr std::uint32_t Categories = 4;
constexpr std::uint32_t LdsDwords = 16;

alignas(256) std::array<std::uint32_t, Categories + Workgroups * ThreadsPerWorkgroup> Storage{};

alignas(256) constexpr std::array<std::uint32_t, 55> ShaderCode{
    0x36020083u, 0x34040282u, 0x34060082u, 0xbe86047eu, 0x7d880084u, 0xbe88246au, 0xbf880003u, 0x7e080280u,
    0xd8340000u, 0x00000403u, 0xbefe0406u, 0xbf8cc07fu, 0xbf8a0000u, 0x7e080281u, 0xd8000000u, 0x00000402u,
    0xbf8cc07fu, 0xbf8a0000u, 0x7d880084u, 0xbe86246au, 0xbf880014u, 0xd8d80000u, 0x04000003u, 0xbf8cc07fu,
    0x7d880082u, 0xbe88246au, 0xbf880004u, 0xe0c85000u, 0x80000403u, 0xbf8c3f70u, 0x7e0a0304u, 0x8afe7e08u,
    0xbf880005u, 0x7e080280u, 0xe0c85000u, 0x80000403u, 0xbf8c3f70u, 0x7e0a0304u, 0xbefe0408u, 0xd8340000u,
    0x00000503u, 0xbefe0406u, 0xbf8cc07fu, 0xbf8a0000u, 0xd8d80000u, 0x04000002u, 0xbf8cc07fu, 0x7e0c0204u,
    0x340c0c88u, 0x4a0c0d00u, 0x4a0c0c84u, 0x340c0c82u, 0xe0701000u, 0x80000406u, 0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Storage.fill(0u);
    for (std::uint32_t category = 0; category < Categories; ++category) Storage[category] = (category + 1u) * 1000u;
    const auto storage = BufferDescriptor(Storage.data(), static_cast<std::uint32_t>(Storage.size() * 4u));
    const std::vector<std::uint32_t> userData(storage.begin(), storage.end());
    const std::span<const std::uint32_t> code(ShaderCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{ThreadsPerWorkgroup, 1u, 1u}, LdsDwords, {true, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {64u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0u, 0u, 0u, 128u}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, Workgroups, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    std::array<std::array<bool, Workgroups>, 2> seen{};
    for (std::uint32_t wg = 0; wg < Workgroups; ++wg) {
        std::array<std::uint32_t, Categories> groupValue{};
        for (std::uint32_t lidx = 0; lidx < ThreadsPerWorkgroup; ++lidx) {
            const std::uint32_t observed = Storage[Categories + wg * ThreadsPerWorkgroup + lidx];
            const std::uint32_t category = lidx & 3u;
            if (lidx < Categories) groupValue[category] = observed;
            Require(observed == groupValue[category], "wg " + std::to_string(wg) + " lidx " + std::to_string(lidx) + ": observed " + std::to_string(observed) + " differed from wave 0 value " + std::to_string(groupValue[category]));
            const std::uint32_t base = (category + 1u) * 1000u;
            if (category < 2u) {
                Require(observed >= base && (observed - base) % 64u == 0u, "wg " + std::to_string(wg) + " category " + std::to_string(category) + ": observed " + std::to_string(observed) + ", expected " + std::to_string(base) + " + 64*k");
                const std::uint32_t slot = (observed - base) / 64u;
                Require(slot < Workgroups, "wg " + std::to_string(wg) + " category " + std::to_string(category) + ": out-of-range slot " + std::to_string(slot));
                if (lidx < Categories) {
                    Require(!seen[category][slot], "duplicate slot " + std::to_string(slot) + " in category " + std::to_string(category));
                    seen[category][slot] = true;
                }
            } else {
                Require(observed == base, "wg " + std::to_string(wg) + " category " + std::to_string(category) + ": observed " + std::to_string(observed) + ", expected " + std::to_string(base));
            }
        }
    }
    Require(Storage[0] == 1000u + Workgroups * 64u, "final counter 0 mismatch");
    Require(Storage[1] == 2000u + Workgroups * 64u, "final counter 1 mismatch");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("divergent device atomic barrier test passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
