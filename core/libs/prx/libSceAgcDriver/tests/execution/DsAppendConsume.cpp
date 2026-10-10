#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t ActiveLanes = 16;
constexpr std::uint32_t LdsDwords = 256;
constexpr std::uint32_t AppendStart = 1000;
constexpr std::uint32_t ConsumeStart = 2000;
constexpr std::uint32_t MaskedAppendStart = 3000;
constexpr std::uint32_t MaskedConsumeStart = 4000;
constexpr std::uint32_t BaseShiftedStart = 5000;
constexpr std::uint32_t UntouchedAppend = 0x5eed0001u;
constexpr std::uint32_t UntouchedConsume = 0x5eed0002u;
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr auto Code = std::to_array<std::uint32_t>({
    0x34060084, 0x7e100280,
    0x7e0802ff, AppendStart, 0x7e0a02ff, ConsumeStart, 0x7e0c02ff, MaskedAppendStart, 0x7e0e02ff, MaskedConsumeStart,
    0x7e1c02ff, BaseShiftedStart, 0x7e1e02ff, BaseShiftedStart + 1u, 0x7e2002ff, BaseShiftedStart + 2u, 0x7e2202ff, BaseShiftedStart + 3u,
    0x7e1802ff, UntouchedAppend, 0x7e1a02ff, UntouchedConsume,
    0x7da40080,
    0xd8340010, 0x00000408, 0xd8340014, 0x00000508, 0xd8340018, 0x00000608, 0xd834001c, 0x00000708,
    0xd8340050, 0x00000e08, 0xd8340054, 0x00000f08, 0xd8340058, 0x00001008, 0xd834005c, 0x00001108,
    0xbefe03c1,
    0xbf8cc07f,
    0xbefc03ff, 0x0040ffff,
    0xd8f80010, 0x0a000000,
    0xd8f40014, 0x0b000000,
    0xbefc03ff, 0x00400000,
    0x7da80090,
    0xd8f80018, 0x0c000000,
    0xd8f4001c, 0x0d000000,
    0xbefe03c1,
    0xbf8cc07f,
    0xd8d80010, 0x14000008, 0xd8d80014, 0x15000008, 0xd8d80018, 0x16000008, 0xd8d8001c, 0x17000008,
    0xd8d80050, 0x18000008, 0xd8d80054, 0x19000008, 0xd8d80058, 0x1a000008, 0xd8d8005c, 0x1b000008,
    0xbf8cc07f,
    0xe0702000, 0x80000a03, 0xe0702004, 0x80000b03, 0xe0702008, 0x80000c03, 0xe070200c, 0x80000d03,
    0xe0702010, 0x80001403, 0xe0702014, 0x80001503, 0xe0702018, 0x80001603, 0xe070201c, 0x80001703,
    0xe0702020, 0x80001803, 0xe0702024, 0x80001903, 0xe0702028, 0x80001a03, 0xe070202c, 0x80001b03,
    0xbf810000,
});

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin());
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
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

void Check() {
    constexpr std::array<const char*, 12> names{
        "ds_append result", "ds_consume result", "masked ds_append result", "masked ds_consume result",
        "ds_append counter", "ds_consume counter", "masked ds_append counter", "masked ds_consume counter",
        "dword at M0.base + ds_append offset", "dword at M0.base + ds_consume offset",
        "dword at M0.base + masked ds_append offset", "dword at M0.base + masked ds_consume offset",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const bool active = tid < ActiveLanes;
        const std::array<std::uint32_t, 12> expected{
            AppendStart, ConsumeStart, active ? MaskedAppendStart : UntouchedAppend, active ? MaskedConsumeStart : UntouchedConsume,
            AppendStart + Threads, ConsumeStart - Threads, MaskedAppendStart + ActiveLanes, MaskedConsumeStart - ActiveLanes,
            BaseShiftedStart, BaseShiftedStart + 1u, BaseShiftedStart + 2u, BaseShiftedStart + 3u,
        };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const auto actual = Output[tid * Results + j];
            Require(actual == expected[j], std::string("ds append consume: lane ") + std::to_string(tid) + " " + names[j] + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case appendConsume{"DsAppendConsume_FullAndMaskedWaves_UpdateCountersAndReturnPreviousValues", [] {
    auto& device = SharedVulkanTestDevice();
    if (device.Target().subgroupSize < Threads) Testing::Skip("the device's subgroups are narrower than a wave (" + std::to_string(device.Target().subgroupSize) + " lanes)");
    Run(device);
    Check();
}};

} // namespace
