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

constexpr std::uint32_t Threads = 31;
constexpr std::uint32_t Ops = 3;
constexpr std::uint32_t Modes = 4;
constexpr std::uint64_t Approximate = 0xdeadbeefdeadbeefull;
constexpr std::array<std::uint32_t, Modes> FloatModes{0x30u, 0x70u, 0xb0u, 0xf0u};

constexpr std::array<std::array<std::uint64_t, 1u + Modes * Ops>, Threads> Measured{{
    {0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, Approximate, 0x3ff0000000000000ull, 0x3ff0000000000000ull, Approximate, 0x3ff0000000000000ull, 0x3ff0000000000000ull, Approximate, 0x3ff0000000000000ull, 0x3ff0000000000000ull, Approximate},
    {0x4000000000000000ull, 0x3fe0000000000000ull, Approximate, Approximate, 0x3fe0000000000000ull, Approximate, Approximate, 0x3fe0000000000000ull, Approximate, Approximate, 0x3fe0000000000000ull, Approximate, Approximate},
    {0x4010000000000000ull, 0x3fd0000000000000ull, 0x3fe0000000000000ull, Approximate, 0x3fd0000000000000ull, 0x3fe0000000000000ull, Approximate, 0x3fd0000000000000ull, 0x3fe0000000000000ull, Approximate, 0x3fd0000000000000ull, 0x3fe0000000000000ull, Approximate},
    {0x3fd0000000000000ull, 0x4010000000000000ull, 0x4000000000000000ull, Approximate, 0x4010000000000000ull, 0x4000000000000000ull, Approximate, 0x4010000000000000ull, 0x4000000000000000ull, Approximate, 0x4010000000000000ull, 0x4000000000000000ull, Approximate},
    {0x0000000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull},
    {0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull},
    {0x7fd0000000000000ull, 0x0010000000000000ull, 0x2000000000000000ull, Approximate, 0x0010000000000000ull, 0x2000000000000000ull, Approximate, 0x0010000000000000ull, 0x2000000000000000ull, Approximate, 0x0010000000000000ull, 0x2000000000000000ull, Approximate},
    {0x7fe0000000000000ull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, 0x0008000000000000ull, Approximate, Approximate, 0x0008000000000000ull, Approximate, Approximate},
    {0xffe0000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8008000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8008000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x0010000000000000ull, 0x7fd0000000000000ull, 0x5fe0000000000000ull, Approximate, 0x7fd0000000000000ull, 0x5fe0000000000000ull, Approximate, 0x7fd0000000000000ull, 0x5fe0000000000000ull, Approximate, 0x7fd0000000000000ull, 0x5fe0000000000000ull, Approximate},
    {0xc000000000000000ull, 0xbfe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbfe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbfe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbfe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x000fffffffffffffull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, Approximate, Approximate, Approximate, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, Approximate, Approximate, Approximate},
    {0x0000000000000001ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x6180000000000000ull, Approximate, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x6180000000000000ull, Approximate},
    {0x0008000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7fe0000000000000ull, Approximate, Approximate, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7fe0000000000000ull, Approximate, Approximate},
    {0x8008000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xffe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xffe0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x800fffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x0004000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x5ff0000000000000ull, Approximate, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x5ff0000000000000ull, Approximate},
    {0x7fefffffffffffffull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate},
    {0xffefffffffffffffull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x7fec000000000000ull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate},
    {0x7fd8000000000000ull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate},
    {0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull},
    {0xfff0000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
    {0xbff0000000000000ull, 0xbff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xbff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x7fe8000000000000ull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate},
    {0xffd8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0x8000000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull, Approximate, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x7fe0000000000001ull, 0x0000000000000000ull, Approximate, Approximate, 0x0000000000000000ull, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate, Approximate},
    {0x0000000080000000ull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, Approximate, Approximate, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x0000000000000000ull, 0x7ff0000000000000ull, Approximate, Approximate},
    {0x8000000000000001ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
    {0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull, 0x7ff4000000000000ull},
}};

constexpr const char* Names[Ops] = {"v_rcp_f64", "v_rsq_f64", "v_sqrt_f64"};

alignas(256) std::array<std::uint64_t, Threads> Input{};
alignas(256) std::array<std::uint64_t, Threads * 4u> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x34020083u, 0xe0341000u, 0x80000201u, 0xbf8c3f70u, 0x7e085f02u, 0x7e0c6302u, 0x7e106902u, 0x34140085u,
    0xe0781000u, 0x8001040au, 0xe0741010u, 0x8001080au, 0xbf8c3f70u, 0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

bool Denormal(std::uint64_t value) {
    return (value & 0x7ff0000000000000ull) == 0u && (value & 0x000fffffffffffffull) != 0u;
}

std::uint32_t Run(AgcDriver::VulkanDevice& device, std::uint32_t mode, bool keepsDenormals) {
    std::uint32_t skipped = 0;
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 8u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 8u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{FloatModes[mode], true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = Measured[tid][0];
    Output.fill(0xdeadbeefdeadbeefull);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t op = 0; op < Ops; ++op) {
            const auto wanted = Measured[tid][1u + mode * Ops + op];
            if (wanted == Approximate) continue;
            const bool keptInput = (FloatModes[mode] & 0x40u) != 0u && Denormal(Measured[tid][0]);
            if (!keepsDenormals && (keptInput || Denormal(wanted))) {
                ++skipped;
                continue;
            }
            const auto got = Output[tid * 4u + op];
            Require(got == wanted, std::string(Names[op]) + " of " + Hex(Measured[tid][0]) + " with FLOAT_MODE " + Hex(FloatModes[mode]) +
                                       " is " + Hex(got) + ", expected the RDNA 2 value " + Hex(wanted));
        }
    }
    return skipped;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        const bool keepsDenormals = device->Target().float64DenormPreserve;
        std::uint32_t skipped = 0;
        for (std::uint32_t mode = 0; mode < Modes; ++mode) skipped += Run(*device, mode, keepsDenormals);
        std::printf("f64 transcendental denormal tests passed, %u device-dependent results skipped\n", skipped);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
