#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 11;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 22> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1c0280u, 0x7e1e0280u, 0x7e200280u,
    0x7e220280u, 0xd56e6a0au, 0x041a0d04u, 0xd501000eu, 0x01a90280u, 0xd56e6a0cu, 0x041a0904u, 0xd501000fu,
    0x01a90280u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xbf810000u,
};

constexpr std::uint64_t Pairs[Threads][2] = {
    {0x0000000000000001ull, 0x3ff0000000000000ull},
    {0x80000000e33e474aull, 0x3ff0000000000000ull},
    {0x3ff0000000000000ull, 0x0000000000000001ull},
    {0x3ff0000000000000ull, 0x800fffffffffffffull},
    {0x000fffffffffffffull, 0x0010000000000000ull},
    {0x0010000000000000ull, 0x0000000000000001ull},
    {0x3fe0000000000000ull, 0x3ff0000000000000ull},
    {0x0000000000000000ull, 0x3ff0000000000000ull},
    {0x8000000000000001ull, 0x0010000000000000ull},
    {0x000fffffffffffffull, 0x3ff0000000000000ull},
    {0x3ff0000000000000ull, 0x7ff8000000000000ull},
};

constexpr std::uint64_t Expected[4][Threads][2] = {
    {
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x8000000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x3fe0000000000000ull, 0x3fe0000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x8000000000000000ull, 0xfff8000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x37f0000000000000ull, 0x47f0000000000000ull},
    },
    {
        {0x0000000000000000ull, 0x04d0000000000000ull},
        {0x8000000000000000ull, 0x86cc67c8e9400000ull},
        {0x47f0000000000000ull, 0x3ff0000000000000ull},
        {0x47f0000000000000ull, 0x3ff0000000000000ull},
        {0x080ffffffffffffeull, 0x080ffffffffffffeull},
        {0x0810000000000000ull, 0x0810000000000000ull},
        {0x3fe0000000000000ull, 0x3fe0000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x84d0000000000000ull, 0x84d0000000000000ull},
        {0x0000000000000000ull, 0x080ffffffffffffeull},
        {0x37f0000000000000ull, 0x47f0000000000000ull},
    },
    {
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x8000000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x3fe0000000000000ull, 0x3fe0000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x8000000000000000ull, 0xfff8000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x37f0000000000000ull, 0x47f0000000000000ull},
    },
    {
        {0x0000000000000001ull, 0x04d0000000000000ull},
        {0x80000000e33e474aull, 0x86cc67c8e9400000ull},
        {0x47f0000000000000ull, 0x3ff0000000000000ull},
        {0x47f0000000000000ull, 0x3ff0000000000000ull},
        {0x080ffffffffffffeull, 0x080ffffffffffffeull},
        {0x0810000000000000ull, 0x0810000000000000ull},
        {0x3fe0000000000000ull, 0x3fe0000000000000ull},
        {0x0000000000000000ull, 0xfff8000000000000ull},
        {0x84d0000000000000ull, 0x84d0000000000000ull},
        {0x000fffffffffffffull, 0x080ffffffffffffeull},
        {0x37f0000000000000ull, 0x47f0000000000000ull},
    },
};

constexpr std::uint32_t ExpectedVcc[4][Threads][2] = {
    {
        {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 0u}, {0u, 0u},
        {0u, 0u}, {0u, 1u}, {0u, 0u}, {0u, 1u}, {0u, 1u},
    },
    {
        {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 0u}, {0u, 0u},
        {0u, 0u}, {0u, 1u}, {0u, 0u}, {0u, 1u}, {0u, 1u},
    },
    {
        {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 0u}, {0u, 0u},
        {0u, 0u}, {0u, 1u}, {0u, 0u}, {0u, 1u}, {0u, 1u},
    },
    {
        {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 1u}, {0u, 0u}, {0u, 0u},
        {0u, 0u}, {0u, 1u}, {0u, 0u}, {0u, 1u}, {0u, 1u},
    },
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
    std::uint32_t field;
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

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = static_cast<std::uint32_t>(Pairs[tid][0]);
        Input[tid * Inputs + 1u] = static_cast<std::uint32_t>(Pairs[tid][0] >> 32u);
        Input[tid * Inputs + 2u] = static_cast<std::uint32_t>(Pairs[tid][1]);
        Input[tid * Inputs + 3u] = static_cast<std::uint32_t>(Pairs[tid][1] >> 32u);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
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
    request.context.floatMode = mode.value;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Check(const Mode& mode, bool keepsDenormals) {
    constexpr const char* Forms[2] = {"div_scale(value, denom, num)", "div_scale(value, value, num)"};
    const std::array<std::uint32_t, 3> deviceFlushes{mode.field & 2u, mode.field & 1u, 0u};
    std::uint32_t skipped = 0;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t form = 0; form < 2u; ++form) {
            const std::uint64_t actual = Output[tid * Results + form * 2u] | (static_cast<std::uint64_t>(Output[tid * Results + form * 2u + 1u]) << 32u);
            const std::uint32_t actualVcc = Output[tid * Results + 4u + form];
            const std::uint64_t expected = Expected[mode.field][tid][form];
            const std::uint32_t expectedVcc = ExpectedVcc[mode.field][tid][form];
            Require(actualVcc == expectedVcc, std::string("v_div_scale_f64 VCC ") + mode.name + " of " + Forms[form] + " with " + Hex(Pairs[tid][0]) + " and " + Hex(Pairs[tid][1]) + " is " + std::to_string(actualVcc) + ", expected " + std::to_string(expectedVcc));
            if (actual == expected) continue;
            const std::string message = std::string("v_div_scale_f64 ") + mode.name + " of " + Forms[form] + " with " + Hex(Pairs[tid][0]) + " and " + Hex(Pairs[tid][1]) + " is " + Hex(actual) + ", expected " + Hex(expected);
            const bool deviceDependent = std::any_of(deviceFlushes.begin(), deviceFlushes.end(), [&](std::uint32_t flushed) { return Expected[flushed][tid][form] != expected; });
            Require(deviceDependent && !keepsDenormals, message);
            std::printf("skipped %s, the device flushes f64 denormals without DenormPreserve support\n", message.c_str());
            ++skipped;
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
        const Mode modes[] = {
            {"without a known float mode", std::nullopt, 3u},
            {"f64 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}, 0u},
            {"f64 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, 0u},
            {"f64 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}, 1u},
            {"f64 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}, 1u},
            {"f64 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}, 2u},
            {"f64 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}, 2u},
            {"f64 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, 3u},
            {"f64 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, 3u},
        };
        bool keepsDenormals = device->Target().float64DenormPreserve;
        std::uint32_t skipped = 0;
        for (const Mode& mode : modes) {
            Run(*device, mode);
            const std::uint32_t modeSkipped = Check(mode, keepsDenormals);
            keepsDenormals = keepsDenormals || (!mode.value.has_value() && modeSkipped == 0u);
            skipped += modeSkipped;
        }
        std::printf("v_div_scale_f64 denormal mode tests passed, %u device-dependent results skipped\n", skipped);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
