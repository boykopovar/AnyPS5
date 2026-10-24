#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 17> Code{
    0x34020084u, 0x34040086u, 0xe0341000u, 0x80000401u, 0xbf8c3f70u, 0x7e143504u, 0x7e183104u, 0x7e1c7d04u,
    0x7e207b04u, 0x7e247904u, 0xe0781000u, 0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0701020u, 0x80011202u,
    0xbf810000u,
};

constexpr std::array<std::uint64_t, 18> Values{
    0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000001ull, 0x8000000000000001ull, 0x000fffffffffffffull, 0x800fffffffffffffull,
    0x0008000000000000ull, 0x8008000000000000ull, 0x0010000000000000ull, 0x8010000000000000ull, 0x3fe0000000000000ull, 0xbff8000000000000ull,
    0x4006000000000000ull, 0x3fefffffffffffffull, 0xbfefffffffffffffull, 0x7fefffffffffffffull, 0xffefffffffffffffull, 0x4330000000000001ull,
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
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

std::uint64_t ValueAt(std::uint32_t tid) {
    return tid < Values.size() ? Values[tid] : 0u;
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = static_cast<std::uint32_t>(ValueAt(tid));
        Input[tid * Inputs + 1u] = static_cast<std::uint32_t>(ValueAt(tid) >> 32u);
        Input[tid * Inputs + 2u] = 0u;
        Input[tid * Inputs + 3u] = 0u;
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

std::uint64_t Flush(std::uint64_t bits, bool flushed) {
    return flushed && (bits & 0x7ff0000000000000ull) == 0u ? bits & 0x8000000000000000ull : bits;
}

std::uint64_t Bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

std::array<std::uint64_t, 5> Expected(std::uint64_t source, std::uint32_t field) {
    const double value = std::bit_cast<double>(Flush(source, (field & 1u) == 0u));
    const double below = std::bit_cast<double>(0x3fefffffffffffffull);
    int exponent = 0;
    const double mantissa = std::frexp(value, &exponent);
    return {
        Bits(std::floor(value)),
        Bits(std::ceil(value)),
        Flush(Bits(std::min(value - std::floor(value), below)), (field & 2u) == 0u),
        Bits(mantissa),
        static_cast<std::uint32_t>(exponent),
    };
}

std::uint32_t Check(const Mode& mode, bool keepsDenormals) {
    const std::uint32_t field = mode.value.has_value() ? mode.value->floatMode >> 6u : 3u;
    const std::array<std::uint32_t, 3> deviceFlushes{field & 2u, field & 1u, 0u};
    constexpr const char* Names[5] = {"v_floor_f64", "v_ceil_f64", "v_fract_f64", "v_frexp_mant_f64", "v_frexp_exp_i32_f64"};
    std::uint32_t skipped = 0;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t source = ValueAt(tid);
        const auto expected = Expected(source, field);
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < expected.size(); ++index) {
            const std::uint64_t actual = index < 4u ? out[index * 2u] | (static_cast<std::uint64_t>(out[index * 2u + 1u]) << 32u) : out[8];
            if (actual == expected[index]) continue;
            const auto text = [index](std::uint64_t bits) { return index < 4u ? Hex(bits) : std::to_string(static_cast<std::int32_t>(bits)); };
            const std::string message = std::string(Names[index]) + " " + mode.name + " of " + Hex(source) + " is " + text(actual) + ", expected " + text(expected[index]);
            const bool deviceDependent = std::any_of(deviceFlushes.begin(), deviceFlushes.end(), [&](std::uint32_t flushed) { return Expected(source, flushed)[index] != expected[index]; });
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
            {"without a known float mode", std::nullopt},
            {"f64 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}},
            {"f64 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}},
            {"f64 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}},
            {"f64 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}},
            {"f64 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}},
            {"f64 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}},
            {"f64 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}},
            {"f64 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}},
        };
        bool keepsDenormals = device->Target().float64DenormPreserve;
        std::uint32_t skipped = 0;
        for (const Mode& mode : modes) {
            Run(*device, mode);
            const std::uint32_t modeSkipped = Check(mode, keepsDenormals);
            keepsDenormals = keepsDenormals || (!mode.value.has_value() && modeSkipped == 0u);
            skipped += modeSkipped;
        }
        std::printf("f64 rounding denormal mode tests passed, %u device-dependent results skipped\n", skipped);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
