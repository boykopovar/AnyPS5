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
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t LdsDwords = 3072;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

// v_lshlrev_b32 v8/v9/v10 lane byte offsets; load init+data pairs from input; seed LDS with the init pair;
// ds_condxchg32_rtn_b64 twice (first from data1, then from data2), reading LDS back after each; store all.
alignas(256) constexpr auto Code = std::to_array<std::uint32_t>({
    0x34100084u, 0x34120085u, 0x34140086u,
    0xe0381000u, 0x80000409u, 0xe0381000u, 0x90000c09u,
    0xbf8c3f70u,
    0xd8340000u, 0x00000408u, 0xd8340004u, 0x00000508u,
    0xbf8cc07fu,
    0xd9f80000u, 0x10000608u,
    0xbf8cc07fu,
    0xd8d80000u, 0x12000008u, 0xd8d80004u, 0x13000008u,
    0xbf8cc07fu,
    0xd9f80000u, 0x14000c08u,
    0xbf8cc07fu,
    0xd8d80000u, 0x16000008u, 0xd8d80004u, 0x17000008u,
    0xbf8cc07fu,
    0xe0781000u, 0x8001100au, 0xe0781000u, 0x9001140au,
    0xbf8c3f70u,
    0xbf810000u,
});

// ds_condxchg32_rtn_b64 with the GDS bit set: a 64-bit GDS atomic is not supported.
alignas(256) constexpr auto GdsVariant = std::to_array<std::uint32_t>({0xd9fa0000u, 0x10000608u, 0xbf810000u});

std::uint32_t ConditionalWrite(std::uint32_t data, std::uint32_t old) {
    return (data & 0x80000000u) != 0u ? (data & 0x7fffffffu) : old;
}

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    for (std::uint32_t i = 0; i < Inputs; ++i) {
        seed = seed * 0xbf58476d1ce4e5b9ull + 0x94d049bb133111ebull;
        auto value = static_cast<std::uint32_t>(seed >> 32u);
        if (i >= 2u) {
            // drive the bit31 write-enable through every combination across lanes
            constexpr std::array flags{0x00000000u, 0x80000000u};
            value = (value & 0x7fffffffu) | flags[(tid / 2u + i) % 2u];
        }
        words[i] = value;
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

template<typename TUse>
void Translate(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, TUse&& use) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    use(ShaderRecompiler::Recompile(request));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    Translate(device, Code, [&](const auto& result) {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device.WaitIdle();
    });
}

void Reject(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    std::string error;
    try {
        Translate(device, code, [](const auto&) {});
    } catch (const std::exception& exception) {
        error = exception.what();
    }
    Require(error.find(reason) != std::string::npos, "ds condxchg32 b64: expected a rejection for \"" + reason + "\", got \"" + error + "\"");
}

void CheckRejections(AgcDriver::VulkanDevice& device) {
    Reject(device, GdsVariant, "64-bit GDS atomics are not supported");
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        const std::uint32_t seedLo = in[0], seedHi = in[1];
        const std::uint32_t firstLo = ConditionalWrite(in[2], seedLo);
        const std::uint32_t firstHi = ConditionalWrite(in[3], seedHi);
        const std::uint32_t secondLo = ConditionalWrite(in[4], firstLo);
        const std::uint32_t secondHi = ConditionalWrite(in[5], firstHi);
        const std::array expected{seedLo, seedHi, firstLo, firstHi, firstLo, firstHi, secondLo, secondHi};
        for (std::uint32_t i = 0; i < expected.size(); ++i) {
            Require(out[i] == expected[i], "ds condxchg32 b64: lane " + std::to_string(tid) + " output dword " + std::to_string(i) + " is " + Hex(out[i]) + ", expected " + Hex(expected[i]));
        }
    }
}

}  // namespace

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        CheckRejections(*device);
        std::puts("ds condxchg32 b64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
