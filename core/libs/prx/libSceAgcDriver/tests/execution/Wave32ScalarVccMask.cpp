#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>

namespace {

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t VccMask = 0x96a596a5u;
alignas(256) std::array<std::uint32_t, Threads> Output{};
alignas(256) constexpr std::array<std::uint32_t, 8> Code{
    0x340c0082u,
    0x7e0e0281u,
    0xbeea03ffu, VccMask,
    0x02020e80u,
    0xe0701000u, 0x80000106u,
    0xbf810000u,
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;

        Output.fill(0xdeadbeefu);
        const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
        const std::array<std::uint32_t, 4> userData{
            static_cast<std::uint32_t>(address),
            static_cast<std::uint32_t>((address >> 32u) & 0xffffu),
            static_cast<std::uint32_t>(Output.size() * sizeof(Output[0])),
            0x31016facu,
        };
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        }};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128},
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();

        std::printf("wave32 scalar VCC mask expected=0x%08x:", VccMask);
        std::uint32_t actual = 0;
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            std::printf(" %u", Output[lane]);
            if (Output[lane] == 1u) actual |= 1u << lane;
        }
        std::printf(" observed=0x%08x\n", actual);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto expected = (VccMask >> lane) & 1u;
            AgcDriver::Graphics::Require(Output[lane] == expected,
                "wave32 scalar VCC: lane " + std::to_string(lane) + " selected " +
                std::to_string(Output[lane]) + ", expected " + std::to_string(expected));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}