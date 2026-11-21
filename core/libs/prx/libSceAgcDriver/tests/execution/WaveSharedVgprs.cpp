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

constexpr std::uint32_t Threads = 64;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads> Index{};
alignas(256) std::array<std::uint32_t, Threads * 3> Output{};

alignas(256) constexpr std::array<std::uint32_t, 23> SwapCode{
    0x34020082, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e0602ff, 0xdeadbeef, 0x7e1002fa, 0x3f04e402,
    0x7e1202fa, 0xcf04e402, 0x7e0602fa, 0xcf04e408, 0x7e0602fa, 0x3f04e409, 0xbeff0380, 0x7e140302,
    0xbefe04c1, 0x7e08030a, 0xe0701000, 0x80010301, 0xe0701100, 0x80010401, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 28> BpermuteCode{
    0x34020082, 0xe0301000, 0x80000201, 0xe0301000, 0x80020501, 0xbf8c3f70, 0xdacc0000, 0x03000205,
    0x7e1202fa, 0xcf04e402, 0xbe94047e, 0x92fe80a0, 0x7e100302, 0xdacc0000, 0x09000905, 0x92fea0a0,
    0xdacc0000, 0x08000805, 0xbf8cc07f, 0x8afe0c14, 0x7e0602fa, 0x3f04e409, 0x7e0602fa, 0xcf04e408,
    0xbefe0414, 0xe0701200, 0x80010301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> BothHalvesCode{
    0x34020082, 0xe0301000, 0x80000201, 0xe0301000, 0x80020501, 0xbf8c3f70, 0xdacc0000, 0x08000205,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> ReadWriteCode{
    0x34020082, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x4a100508, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    compute.granulatedVgprCount = 1u;
    compute.sharedVgprBlocks = 1u;
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold half of a wave64\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        std::uint64_t sameHalf = 0;
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Input[lane] = 0x1000u + lane * 0x10001u;
            const std::uint32_t target = (lane * 5u + 3u) & 63u;
            Index[lane] = target * 4u;
            if ((target >> 5u) == (lane >> 5u)) sameHalf |= 1ull << lane;
        }
        Output.fill(0xdeadbeefu);
        std::vector<std::uint32_t> userData(14, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const auto index = BufferDescriptor(Index.data(), static_cast<std::uint32_t>(Index.size() * 4u));
        std::copy(index.begin(), index.end(), userData.begin() + 8);
        userData[12] = static_cast<std::uint32_t>(sameHalf);
        userData[13] = static_cast<std::uint32_t>(sameHalf >> 32u);
        const std::span<const std::uint32_t> code(SwapCode);
        const auto result = Compile(*device, code, userData);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto swapped = Input[lane ^ 32u];
            Require(Output[lane] == swapped, "wave-shared VGPR half swap: lane " + std::to_string(lane) + " read " + Hex(Output[lane]) + ", expected lane " + std::to_string(lane ^ 32u) + "'s " + Hex(swapped));
            const auto low = Input[lane & 31u];
            Require(Output[Threads + lane] == low, "wave-shared VGPR written by the low half: lane " + std::to_string(lane) + " read " + Hex(Output[Threads + lane]) + ", expected " + Hex(low));
        }
        const std::span<const std::uint32_t> bpermute(BpermuteCode);
        const auto permuted = Compile(*device, bpermute, userData);
        device->Dispatch(permuted, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(bpermute.data()));
        device->WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const std::uint32_t target = Index[lane] / 4u;
            Require(Output[2u * Threads + lane] == Input[target], "wave64 ds_bpermute through wave-shared VGPRs: lane " + std::to_string(lane) + " read " + Hex(Output[2u * Threads + lane]) + ", expected lane " + std::to_string(target) + "'s " + Hex(Input[target]));
        }
        bool bothRejected = false;
        try {
            static_cast<void>(Compile(*device, std::span<const std::uint32_t>(BothHalvesCode), userData));
        } catch (const std::exception& error) {
            bothRejected = std::string(error.what()).find("non-VALU write to wave-shared VGPR v8") != std::string::npos;
        }
        Require(bothRejected, "a ds_bpermute_b32 into a wave-shared VGPR with both halves of EXEC possibly active was not rejected");
        bool rejected = false;
        try {
            static_cast<void>(Compile(*device, std::span<const std::uint32_t>(ReadWriteCode), userData));
        } catch (const std::exception& error) {
            rejected = std::string(error.what()).find("reads and writes wave-shared VGPR v8") != std::string::npos;
        }
        Require(rejected, "a VALU instruction reading and writing the same wave-shared VGPR was not rejected");
        std::puts("wave shared VGPR tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
