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

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> Code{
    0x34020084u, 0x34040083u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0xcc20600au, 0x1c120b04u, 0xcc20600bu,
    0x0c120d04u, 0xe0741000u, 0x80010a02u, 0xbf810000u,
};
constexpr std::uint32_t Rows[Threads][3] = {
    {0x00010000u, 0x00000000u, 0x3f800000u},
    {0x80010000u, 0x00000000u, 0x00800000u},
    {0x00003c00u, 0x00000001u, 0x4b000000u},
    {0x80000001u, 0x00003c00u, 0xbf800000u},
    {0x000003ffu, 0x00003c00u, 0x33800000u},
    {0x000183ffu, 0x00004000u, 0x00000001u},
    {0x00000200u, 0x00000200u, 0x3e800000u},
    {0x00013c00u, 0x00003c00u, 0x7f7fffffu},
    {0x00010400u, 0x00003800u, 0x3f800000u},
    {0x00007c00u, 0x00000001u, 0x3f800000u},
    {0x00010001u, 0x00000001u, 0x40000000u},
    {0x81554400u, 0x00000155u, 0xc0800000u},
    {0x83ff3c00u, 0x00000400u, 0x3f000000u},
    {0x00007bffu, 0x00000001u, 0x2f800000u},
    {0x80000000u, 0x00008000u, 0x80000000u},
    {0x00008001u, 0x00008001u, 0x3f800000u},
};
constexpr std::uint32_t Expected[2][2][Threads][2] = {
    {
        {
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x4b000000u},
            {0x00000000u, 0x80000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x3f800000u, 0x7f7fffffu},
            {0x38000000u, 0x38800000u},
            {0xffc00000u, 0x7f800000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0xc1800000u},
            {0x38800000u, 0x3f000000u},
            {0x00000000u, 0x377fe000u},
            {0x80000000u, 0x80000000u},
            {0x00000000u, 0x00000000u},
        },
        {
            {0x33800000u, 0x33800000u},
            {0xb3800000u, 0xb3800000u},
            {0x33800000u, 0x4b000000u},
            {0x33800000u, 0xb3800000u},
            {0x387fc000u, 0x2c7fc000u},
            {0xb8ffa000u, 0x33800000u},
            {0x30800000u, 0x37000000u},
            {0x3f800000u, 0x7f7fffffu},
            {0x38004000u, 0x38802000u},
            {0x7f800000u, 0x7f800000u},
            {0x33800000u, 0x34400000u},
            {0x387fc000u, 0xc180000bu},
            {0x33800000u, 0x3efff802u},
            {0x3b7fe000u, 0x377fe000u},
            {0x80000000u, 0x80000000u},
            {0x27800000u, 0xb3800000u},
        },
    },
    {
        {
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x4b000000u},
            {0x00000000u, 0x80000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0x00000000u},
            {0x3f800000u, 0x7f7fffffu},
            {0x38000000u, 0x38800000u},
            {0xffc00000u, 0x7f800000u},
            {0x00000000u, 0x00000000u},
            {0x00000000u, 0xc1800000u},
            {0x38800000u, 0x3f000000u},
            {0x00000000u, 0x377fe000u},
            {0x80000000u, 0x80000000u},
            {0x00000000u, 0x00000000u},
        },
        {
            {0x33800000u, 0x33800000u},
            {0xb3800000u, 0xb3800000u},
            {0x33800000u, 0x4b000000u},
            {0x33800000u, 0xb3800000u},
            {0x387fc000u, 0x2c7fc000u},
            {0xb8ffa000u, 0x33800000u},
            {0x30800000u, 0x37000000u},
            {0x3f800000u, 0x7f7fffffu},
            {0x38004000u, 0x38802000u},
            {0x7f800000u, 0x7f800000u},
            {0x33800000u, 0x34400000u},
            {0x387fc000u, 0xc180000bu},
            {0x33800000u, 0x3efff802u},
            {0x3b7fe000u, 0x377fe000u},
            {0x80000000u, 0x80000000u},
            {0x27800000u, 0xb3800000u},
        },
    },
};

struct Mode {
    const char* name;
    ShaderRecompiler::ShaderFloatMode value;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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

bool Matches(std::uint32_t actual, std::uint32_t expected) {
    const auto nan = [](std::uint32_t single) { return (single & 0x7fffffffu) > 0x7f800000u; };
    return actual == expected || (nan(expected) && nan(actual));
}

void Check(const Mode& mode) {
    constexpr const char* Names[Results] = {"v_fma_mix_f32 of three f16 sources", "v_fma_mix_f32 of an f16, an f32 and an f16 source"};
    const std::uint32_t f32Kept = (mode.value.floatMode >> 4u) & 1u;
    const std::uint32_t f16InputsKept = (mode.value.floatMode >> 6u) & 1u;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t form = 0; form < Results; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            const std::uint32_t expected = Expected[f32Kept][f16InputsKept][tid][form];
            Require(Matches(actual, expected), std::string(Names[form]) + " " + mode.name + " of " + Hex(Rows[tid][0]) + ", " + Hex(Rows[tid][1]) + ", " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<Mode> modes;
        static const char* const Names[2][4][2] = {
            {{"f32 denormals flushed, f16 denormal mode 0, IEEE mode off", "f32 denormals flushed, f16 denormal mode 0, IEEE mode on"},
             {"f32 denormals flushed, f16 denormal mode 1, IEEE mode off", "f32 denormals flushed, f16 denormal mode 1, IEEE mode on"},
             {"f32 denormals flushed, f16 denormal mode 2, IEEE mode off", "f32 denormals flushed, f16 denormal mode 2, IEEE mode on"},
             {"f32 denormals flushed, f16 denormal mode 3, IEEE mode off", "f32 denormals flushed, f16 denormal mode 3, IEEE mode on"}},
            {{"f32 denormals kept, f16 denormal mode 0, IEEE mode off", "f32 denormals kept, f16 denormal mode 0, IEEE mode on"},
             {"f32 denormals kept, f16 denormal mode 1, IEEE mode off", "f32 denormals kept, f16 denormal mode 1, IEEE mode on"},
             {"f32 denormals kept, f16 denormal mode 2, IEEE mode off", "f32 denormals kept, f16 denormal mode 2, IEEE mode on"},
             {"f32 denormals kept, f16 denormal mode 3, IEEE mode off", "f32 denormals kept, f16 denormal mode 3, IEEE mode on"}},
        };
        for (std::uint32_t f32 = 0; f32 < 2u; ++f32) {
            for (std::uint32_t f16 = 0; f16 < 4u; ++f16) {
                for (std::uint32_t ieee = 0; ieee < 2u; ++ieee) {
                    modes.push_back({Names[f32][f16][ieee], ShaderRecompiler::ShaderFloatMode{(f16 << 6u) | (f32 != 0u ? 0x30u : 0u), true, ieee != 0u, false}});
                }
            }
        }
        for (const Mode& mode : modes) {
            Run(*device, mode);
            Check(mode);
        }
        std::puts("v_fma_mix_f32 f16 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
