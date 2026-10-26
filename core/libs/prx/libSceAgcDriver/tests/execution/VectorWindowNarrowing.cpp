#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
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

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 6;
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 54> Code{
    0x34020086, 0x34060088, 0x4a140680, 0x4a160681, 0x4a180682, 0x4a1a0683, 0x4a1c0684, 0x4a1e0685,
    0x4a200686, 0x4a220687, 0x4a240688, 0x4a260689, 0x4a28068a, 0x4a2a068b, 0x4a2c068c, 0x4a2e068d,
    0x4a30068e, 0x4a32068f, 0x4a340690, 0x4a360691, 0x4a380692, 0x4a3a0693, 0x4a3c0694, 0x4a3e0695,
    0x4a400696, 0x4a420697, 0x4a440698, 0x4a460699, 0x4a48069a, 0x4a4a069b, 0x4a4c069c, 0x4a4e069d,
    0x4a50069e, 0x4a52069f, 0x877c8308, 0x7e08870a, 0xbefc0309, 0x7e0a870a, 0x7e148504, 0x877c8308,
    0x7e148505, 0xe0701000, 0x80010401, 0xe0701004, 0x80010501, 0xe0701008, 0x80010c01, 0xe070100c,
    0x80010d01, 0xe0701010, 0x80011e01, 0xe0701014, 0x80012901, 0xbf810000,
};

constexpr const char* Names[Checked] = {"v4", "v5", "v12", "v13", "v30", "v41"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t FunctionVariables(std::span<const std::uint32_t> words) {
    std::uint32_t count = 0;
    for (std::size_t at = 5; at < words.size();) {
        const auto length = words[at] >> 16u;
        if (length == 0u) break;
        if ((words[at] & 0xffffu) == spv::OpVariable && at + 3u < words.size() && words[at + 3u] == spv::StorageClassFunction) ++count;
        at += length;
    }
    return count;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, std::uint32_t bounded, std::uint32_t data) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(10, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = bounded;
    userData[9] = data;
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(FunctionVariables(result.spirv.Words()) > 8u, "vector window narrowing: wave" + std::to_string(waveSize) + " window was not split into separate slot variables");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t waveSize, std::uint32_t threads, std::uint32_t bounded, std::uint32_t data) {
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        std::array<std::uint32_t, 32> window{};
        for (std::uint32_t k = 0; k < window.size(); ++k) window[k] = (tid << 8u) + k;
        const std::uint32_t v4 = window[bounded & 3u];
        const std::uint32_t v5 = window[data];
        window[data] = v4;
        window[bounded & 3u] = v5;
        const std::array<std::uint32_t, Checked> expected{v4, v5, window[2], window[3], window[20], window[31]};
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < Checked; ++index) {
            Require(out[index] == expected[index], "vector window narrowing: wave" + std::to_string(waveSize) + " data index " + std::to_string(data) + " lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(out[index]) + ", expected " + Hex(expected[index]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const std::uint32_t data : {20u, 2u, 31u}) {
            Run(*device, 32, 32, 7u, data);
            Check(32, 32, 7u, data);
            Run(*device, 64, 64, 7u, data);
            Check(64, 64, 7u, data);
        }
        std::puts("vector window narrowing tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
