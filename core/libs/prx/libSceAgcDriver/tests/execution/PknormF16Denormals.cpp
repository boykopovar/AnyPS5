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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

struct alignas(256) Shader {
    std::array<std::uint32_t, 11> words;
};

struct Vector {
    std::uint32_t low;
    std::uint32_t high;
    std::uint32_t expected[4];
};

struct Case {
    const char* name;
    Shader shader;
    std::span<const Vector> rows;
};

constexpr Vector PknormI16Rows[] = {
    {0x0001u, 0x8001u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x0155u, 0x8155u, {0x00000000u, 0xffff0001u, 0x00000000u, 0xffff0001u}},
    {0x0200u, 0x8200u, {0x00000000u, 0xffff0001u, 0x00000000u, 0xffff0001u}},
    {0x03ffu, 0x83ffu, {0x00000000u, 0xfffe0002u, 0x00000000u, 0xfffe0002u}},
    {0x8001u, 0x0001u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x8200u, 0x0200u, {0x00000000u, 0x0001ffffu, 0x00000000u, 0x0001ffffu}},
    {0x83ffu, 0x03ffu, {0x00000000u, 0x0002fffeu, 0x00000000u, 0x0002fffeu}},
    {0x0400u, 0x8400u, {0xfffe0002u, 0xfffe0002u, 0xfffe0002u, 0xfffe0002u}},
    {0x8400u, 0x0400u, {0x0002fffeu, 0x0002fffeu, 0x0002fffeu, 0x0002fffeu}},
    {0x0000u, 0x8000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x8000u, 0x0000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x3c00u, 0xbc00u, {0x80017fffu, 0x80017fffu, 0x80017fffu, 0x80017fffu}},
    {0xbc00u, 0x3c00u, {0x7fff8001u, 0x7fff8001u, 0x7fff8001u, 0x7fff8001u}},
    {0x3800u, 0xb800u, {0xc0004000u, 0xc0004000u, 0xc0004000u, 0xc0004000u}},
    {0x7c00u, 0xfc00u, {0x80017fffu, 0x80017fffu, 0x80017fffu, 0x80017fffu}},
    {0x7e00u, 0xfe00u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
};

constexpr Vector PknormU16Rows[] = {
    {0x0001u, 0x8001u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x0155u, 0x8155u, {0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u}},
    {0x0200u, 0x8200u, {0x00000000u, 0x00000002u, 0x00000000u, 0x00000002u}},
    {0x03ffu, 0x83ffu, {0x00000000u, 0x00000004u, 0x00000000u, 0x00000004u}},
    {0x8001u, 0x0001u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x8200u, 0x0200u, {0x00000000u, 0x00020000u, 0x00000000u, 0x00020000u}},
    {0x83ffu, 0x03ffu, {0x00000000u, 0x00040000u, 0x00000000u, 0x00040000u}},
    {0x0400u, 0x8400u, {0x00000004u, 0x00000004u, 0x00000004u, 0x00000004u}},
    {0x8400u, 0x0400u, {0x00040000u, 0x00040000u, 0x00040000u, 0x00040000u}},
    {0x0000u, 0x8000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x8000u, 0x0000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x3c00u, 0xbc00u, {0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu}},
    {0xbc00u, 0x3c00u, {0xffff0000u, 0xffff0000u, 0xffff0000u, 0xffff0000u}},
    {0x3800u, 0xb800u, {0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u}},
    {0x7c00u, 0xfc00u, {0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu}},
    {0x7e00u, 0xfe00u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
};

const Case Cases[] = {
    {"v_cvt_pknorm_i16_f16", {{0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd712000au, 0x00020b04u, 0xe0701000u, 0x80010a02u, 0xbf810000u}}, PknormI16Rows},
    {"v_cvt_pknorm_u16_f16", {{0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd713000au, 0x00020b04u, 0xe0701000u, 0x80010a02u, 0xbf810000u}}, PknormU16Rows},
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

void Run(AgcDriver::VulkanDevice& device, const Case& item, std::uint32_t denorm16, bool ieee) {
    Input.fill(0u);
    for (std::uint32_t row = 0; row < item.rows.size(); ++row) {
        Input[row * Inputs] = item.rows[row].low;
        Input[row * Inputs + 1] = item.rows[row].high;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(item.shader.words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{denorm16 << 6u, true, ieee, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Case& item, std::uint32_t denorm16, bool ieee) {
    for (std::uint32_t row = 0; row < item.rows.size(); ++row) {
        const auto actual = Output[row];
        const auto expected = item.rows[row].expected[denorm16];
        Require(actual == expected, std::string(item.name) + " denorm16 " + std::to_string(denorm16) + (ieee ? " ieee" : " no ieee") + " input " +
                                        Hex(item.rows[row].low) + " " + Hex(item.rows[row].high) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto& item : Cases) {
            for (std::uint32_t denorm16 = 0u; denorm16 < 4u; ++denorm16) {
                for (const bool ieee : {false, true}) {
                    Run(*device, item, denorm16, ieee);
                    Check(item, denorm16, ieee);
                }
            }
        }
        std::puts("pknorm f16 denormal tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
