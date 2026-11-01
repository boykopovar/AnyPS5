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

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> FmaCode{
    0x34020085u, 0x34060083u, 0xe0381000u, 0x80000401u, 0xe0381010u, 0x80000801u, 0xbf8c3f70u, 0xd54c000cu, 0x04220d04u, 0xe0741000u, 0x80010c03u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 13> DivFmasClearCode{
    0x34020085u, 0x34060083u, 0xe0381000u, 0x80000401u, 0xe0381010u, 0x80000801u, 0xbf8c3f70u, 0xbeea0380u, 0xd570000cu, 0x04220d04u, 0xe0741000u, 0x80010c03u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 13> DivFmasSetCode{
    0x34020085u, 0x34060083u, 0xe0381000u, 0x80000401u, 0xe0381010u, 0x80000801u, 0xbf8c3f70u, 0xbeea03c1u, 0xd570000cu, 0x04220d04u, 0xe0741000u, 0x80010c03u, 0xbf810000u,
};

struct Case {
    const char* name;
    std::span<const std::uint32_t> code;
    std::array<std::array<std::uint64_t, 3>, Threads> rows;
    std::array<std::array<std::uint64_t, Threads>, 4> expected;
};

constexpr std::array<std::array<std::uint64_t, 3>, Threads> FmaRows{{
    {0x3ff0000000000001ull, 0x000fffffffffffffull, 0x0000000000000000ull},
    {0x2000000000000001ull, 0x1ffffffffffffffeull, 0x0000000000000000ull},
    {0x1fffffffffffffffull, 0x2000000000000000ull, 0x0000000000000000ull},
    {0x2000000000000000ull, 0x1ff0000000000000ull, 0x0000000000000000ull},
    {0x000fffffffffffffull, 0x4000000000000000ull, 0x8000000000000000ull},
    {0x3ff0000000000000ull, 0x0008000000000000ull, 0x0008000000000001ull},
    {0x4000000000000000ull, 0x0008000000000000ull, 0x8008000000000000ull},
    {0x3ff8000000000000ull, 0x0007555555555556ull, 0x0000000000000000ull}
}};
constexpr std::array<std::array<std::uint64_t, 3>, Threads> DivFmasClearRows{{
    {0x3ff0000000000001ull, 0x000fffffffffffffull, 0x0000000000000000ull},
    {0x1fffffffffffffffull, 0x2000000000000000ull, 0x0000000000000000ull},
    {0x2000000000000000ull, 0x1ff0000000000000ull, 0x0000000000000000ull},
    {0x3ff0000000000000ull, 0x0000000000000000ull, 0x000fffffffffffffull},
    {0x23d0000000000000ull, 0x23d0000000000000ull, 0x0000000000000000ull},
    {0x4000000000000000ull, 0x0008000000000000ull, 0x8008000000000000ull},
    {0x3ff0000000000000ull, 0x0008000000000000ull, 0x0008000000000001ull},
    {0x3ff8000000000000ull, 0x0007555555555556ull, 0x0000000000000000ull}
}};
constexpr std::array<std::array<std::uint64_t, 3>, Threads> DivFmasSetRows{{
    {0x19d8000000000000ull, 0x2b58000000000000ull, 0x0000000000000000ull},
    {0x1ae8000000000000ull, 0x2b58000000000000ull, 0x0000000000000000ull},
    {0x3ff0000000000001ull, 0x000fffffffffffffull, 0x0000000000000000ull},
    {0x23d0000000000000ull, 0x23d0000000000000ull, 0x0000000000000000ull},
    {0x4000000000000000ull, 0x4000000000000000ull, 0x7fe0000000000000ull},
    {0x3ff0000000000000ull, 0x0000000000000000ull, 0x000fffffffffffffull},
    {0x4000000000000000ull, 0x3ff0000000000000ull, 0x0000000000000000ull},
    {0x7fe0000000000000ull, 0x7fe0000000000000ull, 0x7fe0000000000000ull}
}};

constexpr std::array<std::array<std::uint64_t, Threads>, 4> FmaExpected{{
    {
        0x0000000000000000ull,
        0x0010000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
    },
    {
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x001ffffffffffffeull,
        0x0010000000000001ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
    },
    {
        0x0000000000000000ull,
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0008000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
    },
    {
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0008000000000000ull,
        0x001ffffffffffffeull,
        0x0010000000000001ull,
        0x0008000000000000ull,
        0x000b000000000001ull,
    },
}};
constexpr std::array<std::array<std::uint64_t, Threads>, 4> DivFmasClearExpected{{
    {
        0x0010000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x07b0000000000000ull,
        0x0000000000000000ull,
        0x0010000000000001ull,
        0x0000000000000000ull,
    },
    {
        0x0010000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x07b0000000000000ull,
        0x0000000000000000ull,
        0x0010000000000001ull,
        0x0000000000000000ull,
    },
    {
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0008000000000000ull,
        0x000fffffffffffffull,
        0x07b0000000000000ull,
        0x0008000000000000ull,
        0x0010000000000001ull,
        0x000b000000000001ull,
    },
    {
        0x0010000000000000ull,
        0x0010000000000000ull,
        0x0008000000000000ull,
        0x000fffffffffffffull,
        0x07b0000000000000ull,
        0x0008000000000000ull,
        0x0010000000000001ull,
        0x000b000000000001ull,
    },
}};
constexpr std::array<std::array<std::uint64_t, Threads>, 4> DivFmasSetExpected{{
    {
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x7ff0000000000000ull,
        0x0000000000000000ull,
        0x3800000000000000ull,
        0x7ff0000000000000ull,
    },
    {
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x0000000000000000ull,
        0x7ff0000000000000ull,
        0x0000000000000000ull,
        0x3800000000000000ull,
        0x7ff0000000000000ull,
    },
    {
        0x0000000000000090ull,
        0x0000000001200000ull,
        0x0000000000000000ull,
        0x0000400000000000ull,
        0x7ff0000000000000ull,
        0x0000000000000000ull,
        0x3800000000000000ull,
        0x7ff0000000000000ull,
    },
    {
        0x0000000000000090ull,
        0x0000000001200000ull,
        0x0000000000000000ull,
        0x0000400000000000ull,
        0x7ff0000000000000ull,
        0x0000000000000000ull,
        0x3800000000000000ull,
        0x7ff0000000000000ull,
    },
}};

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

void Run(AgcDriver::VulkanDevice& device, const Case& item, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t operand = 0; operand < 3u; ++operand) {
            Input[tid * Inputs + operand * 2u] = static_cast<std::uint32_t>(item.rows[tid][operand]);
            Input[tid * Inputs + operand * 2u + 1u] = static_cast<std::uint32_t>(item.rows[tid][operand] >> 32u);
        }
        Input[tid * Inputs + 6u] = 0u;
        Input[tid * Inputs + 7u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = item.code;
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

bool DeviceFlushed(const Case& item, const Mode& mode, std::uint32_t tid, std::uint64_t actual) {
    const std::array<std::uint32_t, 3> deviceFlushes{mode.field & 2u, mode.field & 1u, 0u};
    return std::any_of(deviceFlushes.begin(), deviceFlushes.end(), [&](std::uint32_t flushed) { return item.expected[flushed][tid] == actual; });
}

std::uint32_t Check(const Case& item, const Mode& mode, bool keepsDenormals) {
    std::uint32_t skipped = 0;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t actual = Output[tid * Results] | (static_cast<std::uint64_t>(Output[tid * Results + 1u]) << 32u);
        const std::uint64_t expected = item.expected[mode.field][tid];
        if (actual == expected) continue;
        const std::string message = std::string(item.name) + " " + mode.name + " of " + Hex(item.rows[tid][0]) + ", " + Hex(item.rows[tid][1]) + " and " + Hex(item.rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected);
        Require(DeviceFlushed(item, mode, tid, actual) && !keepsDenormals, message);
        std::printf("skipped %s, the device flushes f64 denormals without DenormPreserve support\n", message.c_str());
        ++skipped;
    }
    return skipped;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped f64 fma denormal mode tests, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        const Case cases[] = {
            {"v_fma_f64", std::span<const std::uint32_t>(FmaCode), FmaRows, FmaExpected},
            {"v_div_fmas_f64 (VCC clear)", std::span<const std::uint32_t>(DivFmasClearCode), DivFmasClearRows, DivFmasClearExpected},
            {"v_div_fmas_f64 (VCC set)", std::span<const std::uint32_t>(DivFmasSetCode), DivFmasSetRows, DivFmasSetExpected},
        };
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
        Require(DeviceFlushed(cases[0], modes[7], 7u, 0x0000000000000000ull), "a flushed denormal result is not accepted as a device flush");
        Require(!DeviceFlushed(cases[0], modes[7], 7u, 0x12345678deadbeefull), "an unrelated result is accepted as a device flush");
        bool keepsDenormals = device->Target().float64DenormPreserve;
        std::uint32_t skipped = 0;
        for (const Case& item : cases) {
            bool caseKeeps = keepsDenormals;
            for (const Mode& mode : modes) {
                Run(*device, item, mode);
                const std::uint32_t modeSkipped = Check(item, mode, caseKeeps);
                caseKeeps = caseKeeps || (!mode.value.has_value() && modeSkipped == 0u);
                skipped += modeSkipped;
            }
        }
        if (skipped != 0u) {
            std::printf("skipped, the device flushes f64 denormals without DenormPreserve support, %u results not checked\n", skipped);
            return VulkanTestSkipped;
        }
        std::puts("f64 fma and div_fmas denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
