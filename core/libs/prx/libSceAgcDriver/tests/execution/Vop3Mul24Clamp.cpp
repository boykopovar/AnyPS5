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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 28> Code{
    0x34020084u, 0x34060085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd509800au, 0x00020b04u, 0xd50b800bu,
    0x00020b04u, 0xd509000cu, 0x00020b04u, 0xd50b000du, 0x00020b04u, 0xd509800eu, 0x00020affu, 0xff800000u,
    0xd50b800fu, 0x0001ff04u, 0x00ffffffu, 0xd50a0010u, 0x00020b04u, 0xd50c0011u, 0x00020b04u, 0xe0781000u,
    0x80010a03u, 0xe0781010u, 0x80010e03u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][Inputs] = {
    {0x007fffffu, 0x007fffffu, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00800000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x007fffffu, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x00ffffffu, 0x00000000u, 0x00000000u},
    {0x007fffffu, 0x00000100u, 0x00000000u, 0x00000000u},
    {0x007fffffu, 0x00000101u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000100u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000101u, 0x00000000u, 0x00000000u},
    {0x0000b505u, 0x0000b505u, 0x00000000u, 0x00000000u},
    {0x0000b504u, 0x0000b504u, 0x00000000u, 0x00000000u},
    {0x00010000u, 0x00010000u, 0x00000000u, 0x00000000u},
    {0x00010000u, 0x0000ffffu, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x00000101u, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x00000100u, 0x00000000u, 0x00000000u},
    {0xab7fffffu, 0x12000002u, 0x00000000u, 0x00000000u},
    {0xff800000u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x00400000u, 0x00000200u, 0x00000000u, 0x00000000u},
    {0x00400000u, 0x000001ffu, 0x00000000u, 0x00000000u},
    {0x00c00000u, 0x00000200u, 0x00000000u, 0x00000000u},
    {0x00c00000u, 0x00000201u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00ffffffu, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00ffffffu, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x00000003u, 0x00555556u, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x00000100u, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x000000ffu, 0x00000000u, 0x00000000u},
    {0x12345678u, 0x9abcdef0u, 0x00000000u, 0x00000000u},
    {0xfedcba98u, 0x01234567u, 0x00000000u, 0x00000000u},
    {0x00002d41u, 0x00002d41u, 0x00000000u, 0x00000000u},
    {0x00123456u, 0x00000070u, 0x00000000u, 0x00000000u},
    {0x00fffffeu, 0x00800000u, 0x00000000u, 0x00000000u},
    {0x0000ffffu, 0x00010001u, 0x00000000u, 0x00000000u},
};

constexpr std::uint32_t Expected[Threads][Results] = {
    {0x7fffffffu, 0xffffffffu, 0xff000001u, 0xff000001u, 0x80000000u, 0xffffffffu, 0x00003fffu, 0x00003fffu},
    {0x7fffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00004000u, 0x00004000u},
    {0x80000000u, 0xffffffffu, 0x00800000u, 0xff800000u, 0x80000000u, 0xffffffffu, 0xffffc000u, 0x00003fffu},
    {0x00000001u, 0xffffffffu, 0x00000001u, 0xfe000001u, 0x00800000u, 0xffffffffu, 0x00000000u, 0x0000ffffu},
    {0x7fffff00u, 0x7fffff00u, 0x7fffff00u, 0x7fffff00u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0x807ffeffu, 0x807ffeffu, 0x807ffeffu, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x80000000u, 0x80800000u, 0x7f800000u, 0x80800000u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x7fffffffu, 0x80001219u, 0x80001219u, 0x80001219u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7ffea810u, 0x7ffea810u, 0x7ffea810u, 0x7ffea810u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x80000000u, 0xffffffffu, 0x00000001u, 0x00000001u},
    {0x7fffffffu, 0xffff0000u, 0xffff0000u, 0xffff0000u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0xfffffeffu, 0xffffffffu, 0xfffffeffu, 0x00fffeffu, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffff00u, 0xffffff00u, 0xffffff00u, 0xffffff00u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x00fffffeu, 0x00fffffeu, 0x00fffffeu, 0x00fffffeu, 0xff000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00004000u, 0x00004000u},
    {0x7fffffffu, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x80000000u, 0xffffffffu, 0x80000000u, 0x80000000u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x80000000u, 0xffffffffu, 0x7fc00000u, 0x80c00000u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0x00ffffffu, 0xffffffffu, 0x00ffffffu, 0x00800000u, 0x00ffffffu, 0xffffffffu, 0x00000000u},
    {0xffffffffu, 0x00ffffffu, 0xffffffffu, 0x00ffffffu, 0xff800000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x01000002u, 0x01000002u, 0x01000002u, 0x01000002u, 0x80000000u, 0x02fffffdu, 0x00000000u, 0x00000000u},
    {0x80000100u, 0x80000100u, 0x80000100u, 0x80000100u, 0x80000000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x808000ffu, 0x7f8000ffu, 0x808000ffu, 0x7f8000ffu, 0x80800000u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x80000000u, 0xffffffffu, 0x9c2d2080u, 0x142d2080u, 0x7fffffffu, 0xffffffffu, 0xfffff246u, 0x0000269du},
    {0x80000000u, 0xffffffffu, 0xf2e20b28u, 0x59e20b28u, 0x80000000u, 0xffffffffu, 0xfffffb23u, 0x00001e69u},
    {0x07ffea81u, 0x07ffea81u, 0x07ffea81u, 0x07ffea81u, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x07f6e5a0u, 0x07f6e5a0u, 0x07f6e5a0u, 0x07f6e5a0u, 0xc8000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x01000000u, 0xffffffffu, 0x01000000u, 0xff000000u, 0x7fffffffu, 0xffffffffu, 0x00000000u, 0x00007fffu},
    {0x7fffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0x80000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
};

constexpr const char* Names[Results] = {
    "v_mul_i32_i24 v10, v4, v5 clamp",
    "v_mul_u32_u24 v11, v4, v5 clamp",
    "v_mul_i32_i24 v12, v4, v5",
    "v_mul_u32_u24 v13, v4, v5",
    "v_mul_i32_i24 v14, 0xff800000, v5 clamp",
    "v_mul_u32_u24 v15, v4, 0x00ffffff clamp",
    "v_mul_hi_i32_i24 v16, v4, v5",
    "v_mul_hi_u32_u24 v17, v4, v5",
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

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t index = 0; index < Results; ++index) {
                const std::uint32_t actual = Output[tid * Results + index];
                Require(actual == Expected[tid][index], std::string("VOP3 24-bit multiply clamp: lane ") + std::to_string(tid) + " " + Names[index] + " of " + Hex(Rows[tid][0]) + " and " +
                                                            Hex(Rows[tid][1]) + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][index]));
            }
        }
        std::puts("VOP3 24-bit multiply clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
