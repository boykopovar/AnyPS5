#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "ExecutionHelpers.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t InputWords = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Rows = 12;
constexpr std::uint32_t Ops = 10;
constexpr std::uint32_t HighHalf = 0xabcd0000u;
alignas(256) std::array<std::uint32_t, Threads * InputWords> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 29> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x64140b04u, 0xd532000bu, 0x00020b04u,
    0x66180b04u, 0xd533000du, 0x00020b04u, 0x681c0b04u, 0xd534000fu, 0x00020b04u, 0x6a200b04u, 0xd5350011u,
    0x00020b04u, 0xd5330012u, 0x20020b04u, 0x6a2608f2u, 0x7e280280u, 0x7e2a0280u, 0xe0781000u, 0x80010a03u,
    0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xbf810000u,
};

constexpr std::uint16_t Inputs[Rows][2] = {
    {0x7dbeu, 0x3c00u},
    {0x3c00u, 0x7dbeu},
    {0x7dbeu, 0xfc01u},
    {0xfc01u, 0x7dbeu},
    {0x7e55u, 0x7dbeu},
    {0x7dbeu, 0x7e55u},
    {0x7e55u, 0x3c00u},
    {0x3c00u, 0x7e55u},
    {0x7e55u, 0xffaau},
    {0xffaau, 0x7e55u},
    {0x7c00u, 0xfc00u},
    {0x0000u, 0x7c00u}
};
constexpr std::uint16_t ExpectedIeee[Rows][Ops] = {
    {0x7fbeu, 0x7fbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0xffbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0x7fbeu},
    {0x7fbeu, 0x7fbeu, 0xffbeu, 0xffbeu, 0x7fbeu, 0x7fbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0x3c00u},
    {0x7fbeu, 0x7fbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0xffbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0x7fbeu},
    {0xfe01u, 0xfe01u, 0xfe01u, 0xfe01u, 0x7e01u, 0x7e01u, 0xfe01u, 0xfe01u, 0x7e01u, 0xfe01u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0x7fbeu, 0x7fbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0xffbeu, 0x7fbeu, 0x7fbeu, 0xffbeu, 0x7fbeu},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x3c00u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0xffaau, 0xffaau, 0xffaau, 0xffaau, 0x7faau, 0x7faau, 0xffaau, 0xffaau, 0x7faau, 0xffaau},
    {0xfe00u, 0xfe00u, 0x7c00u, 0x7c00u, 0xfc00u, 0xfc00u, 0xfc00u, 0xfc00u, 0xfe00u, 0x7c00u},
    {0x7c00u, 0x7c00u, 0xfc00u, 0xfc00u, 0x7c00u, 0x7c00u, 0xfe00u, 0xfe00u, 0xfc00u, 0x0000u}
};
constexpr std::uint16_t ExpectedNoIeee[Rows][Ops] = {
    {0x7dbeu, 0x7dbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0xfdbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0x7dbeu},
    {0x7dbeu, 0x7dbeu, 0xfdbeu, 0xfdbeu, 0x7dbeu, 0x7dbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0x3c00u},
    {0x7dbeu, 0x7dbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0xfdbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0x7dbeu},
    {0xfc01u, 0xfc01u, 0xfc01u, 0xfc01u, 0x7c01u, 0x7c01u, 0xfc01u, 0xfc01u, 0x7c01u, 0xfc01u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0x7dbeu, 0x7dbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0xfdbeu, 0x7dbeu, 0x7dbeu, 0xfdbeu, 0x7dbeu},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x3c00u},
    {0x7e55u, 0x7e55u, 0x7e55u, 0x7e55u, 0xfe55u, 0xfe55u, 0x7e55u, 0x7e55u, 0xfe55u, 0x7e55u},
    {0xffaau, 0xffaau, 0xffaau, 0xffaau, 0x7faau, 0x7faau, 0xffaau, 0xffaau, 0x7faau, 0xffaau},
    {0xfe00u, 0xfe00u, 0x7c00u, 0x7c00u, 0xfc00u, 0xfc00u, 0xfc00u, 0xfc00u, 0xfe00u, 0x7c00u},
    {0x7c00u, 0x7c00u, 0xfc00u, 0xfc00u, 0x7c00u, 0x7c00u, 0xfe00u, 0xfe00u, 0xfc00u, 0x0000u}
};
constexpr const char* Names[Ops] = {
    "v_add_f16",
    "v_add_f16_e64",
    "v_sub_f16",
    "v_sub_f16_e64",
    "v_subrev_f16",
    "v_subrev_f16_e64",
    "v_mul_f16",
    "v_mul_f16_e64",
    "v_sub_f16_e64 -v4 v5",
    "v_mul_f16 1.0 v4"
};

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, bool nativeFloat16) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < 2; ++i) Input[tid * InputWords + i] = HighHalf | Inputs[tid % Rows][i];
        Input[tid * InputWords + 2] = 0u;
        Input[tid * InputWords + 3] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    std::vector<std::uint32_t> capabilities(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
    if (!nativeFloat16) {
        std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityFloat16));
        request.target.supportedCapabilities = capabilities;
    }
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check(const std::uint16_t (&expected)[Rows][Ops], const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& row = Inputs[tid % Rows];
        for (std::uint32_t op = 0; op < Ops; ++op) {
            const std::uint32_t want = expected[tid % Rows][op];
            const std::uint32_t actual = Output[tid * Results + op];
            Require(actual == want, "f16 arithmetic nan: " + mode + " " + Names[op] + " of " + Hex(row[0]) + ", " + Hex(row[1]) + " is " + Hex(actual) + ", expected " + Hex(want));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool native : {true, false}) {
            const std::string path = native ? "native f16" : "f16 through f32";
            Run(*device, std::nullopt, native);
            Check(ExpectedNoIeee, path + " no float mode");
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, native);
            Check(ExpectedNoIeee, path + " IEEE=0");
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}, native);
            Check(ExpectedIeee, path + " IEEE=1");
        }
        std::puts("f16 arithmetic nan tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
