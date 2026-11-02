#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Slots = 16;
constexpr std::uint32_t Stride = 4;
constexpr std::uint32_t Guard = 4096;
alignas(256) std::array<std::uint32_t, Lanes * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 58> WaterfallCode{
    0x34240083, 0x7e040280, 0x7e060280, 0xd9340000, 0x00000212, 0xbf8cc07f, 0x16080085, 0x3608088f,
    0x34080883, 0xd6ff0002, 0x00010300, 0xd9280000, 0x00000204, 0xbf8cc07f, 0xd9d80000, 0x06000012,
    0xbf8cc07f, 0xd4e50002, 0x00020c80, 0x7e140280, 0x7e160280, 0xbe8a0380, 0xbefe0400, 0xbf138002,
    0xbf840016, 0xbe8b1402, 0xbe821c0b, 0x8f0c830b, 0x7e18020c, 0xd9d80000, 0x0e00000c, 0xbf8cc07f,
    0x7dca1c80, 0xbf86fff5, 0x7e1c050e, 0x7e1e050f, 0xbe90140e, 0x8f921081, 0x3a1c1c12, 0x3a1e1e13,
    0x4a141481, 0x4a161610, 0x800a810a, 0xbf08ff0a, 0x00001000, 0xbf850001, 0xbf82fff1, 0xbefe04c1,
    0x7e1a020a, 0xe0702000, 0x80010a00, 0xe0702004, 0x80010b00, 0xe0702008, 0x80010d00, 0xe070200c,
    0x80010600, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 23> MaskedOrCode{
    0x34240083, 0x7e040280, 0x7e060280, 0xd9340000, 0x00000212, 0xbf8cc07f, 0x3608008f, 0x34080883,
    0xd6ff0002, 0x00010300, 0xbefe0400, 0xd9280000, 0x00000204, 0xbf8cc07f, 0xbefe04c1, 0xd9d80000,
    0x06000012, 0xbf8cc07f, 0xe0702000, 0x80010600, 0xe0702004, 0x80010700, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | ((Stride * 4u) << 16u), records, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint64_t exec, bool spanned) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    userData[0] = static_cast<std::uint32_t>(exec);
    userData[1] = static_cast<std::uint32_t>(exec >> 32u);
    const auto output = BufferDescriptor(Output.data(), Lanes);
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address, std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, Lanes * 2u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, address, code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.target.spannedWaves = spanned;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(result.fullSubgroups == spanned, spanned ? "the program did not span two host subgroups" : "the program spans host subgroups without APS5_SPANNED_WAVE");
    device.Dispatch(result, 1, 1, 1, {}, address);
    device.WaitIdle();
}

void Check(std::uint64_t exec, bool spanned) {
    std::array<std::uint64_t, Lanes> slots{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) slots[(lane * 5u) & (Slots - 1u)] |= std::uint64_t{1} << lane;
    std::uint32_t iterations = 0;
    std::uint32_t indexSum = 0;
    for (std::uint32_t slot = 0; slot < Slots; ++slot) {
        for (auto mask = slots[slot]; mask != 0; mask &= mask - 1) {
            ++iterations;
            indexSum += static_cast<std::uint32_t>(std::countr_zero(mask));
        }
    }
    char name[64];
    std::snprintf(name, sizeof(name), "%s, exec 0x%016llx", spanned ? "spanned" : "two-lane", static_cast<unsigned long long>(exec));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const bool active = ((exec >> lane) & 1u) != 0;
        const std::array<std::uint32_t, Stride> expected{active ? iterations : 0u, active ? indexSum : 0u, iterations, static_cast<std::uint32_t>(slots[lane])};
        const std::array<const char*, Stride> fields{"inner iterations", "bit index sum", "wave iterations", "own slot low word"};
        for (std::uint32_t field = 0; field < Stride; ++field) {
            const auto actual = Output[lane * Stride + field];
            Require(actual != Guard + 1u || field != 2u, std::string(name) + ": the waterfall ran past " + std::to_string(Guard) + " iterations");
            Require(actual == expected[field], std::string(name) + ": lane " + std::to_string(lane) + " " + fields[field] + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[field]));
        }
    }
}

void CheckMaskedOr(std::uint64_t exec, bool spanned) {
    std::array<std::uint64_t, Lanes> slots{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        if (((exec >> lane) & 1u) != 0) slots[lane & (Slots - 1u)] |= std::uint64_t{1} << lane;
    }
    char name[64];
    std::snprintf(name, sizeof(name), "%s masked ds_or_b64, exec 0x%016llx", spanned ? "spanned" : "two-lane", static_cast<unsigned long long>(exec));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::array<std::uint32_t, 2> expected{static_cast<std::uint32_t>(slots[lane]), static_cast<std::uint32_t>(slots[lane] >> 32u)};
        for (std::uint32_t word = 0; word < expected.size(); ++word) {
            const auto actual = Output[lane * Stride + word];
            Require(actual == expected[word], std::string(name) + ": lane " + std::to_string(lane) + " slot word " + std::to_string(word) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[word]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        const bool spannable = device->Target().subgroupSize == 32u && device->ComputeFullSubgroups();
        if (!spannable) std::puts("spanned mode skipped: the device has no full 32-lane compute subgroups");
        for (const bool spanned : {false, true}) {
            if (spanned && !spannable) continue;
            for (const std::uint64_t exec : {~std::uint64_t{0}, std::uint64_t{0xaaaaaaaaaaaaaaaa}, std::uint64_t{0xffff000000000000}, std::uint64_t{0x000000000000ffff}, std::uint64_t{1} << 63u, std::uint64_t{1}, std::uint64_t{0x0000000100000000}}) {
                Run(*device, WaterfallCode, exec, spanned);
                Check(exec, spanned);
            }
            for (const std::uint64_t exec : {~std::uint64_t{0}, std::uint64_t{0x00000000ffffffff}, std::uint64_t{0xffffffff00000000}, std::uint64_t{1} << 40u, std::uint64_t{0x8000000000000001}}) {
                Run(*device, MaskedOrCode, exec, spanned);
                CheckMaskedOr(exec, spanned);
            }
        }
        std::printf("LDS mask waterfall tests passed (two-lane%s)\n", spannable ? " and spanned" : "");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
