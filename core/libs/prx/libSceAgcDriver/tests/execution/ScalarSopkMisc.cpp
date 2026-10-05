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

alignas(256) constexpr std::array<std::uint32_t, 31> Code{
    0x34060086, 0xbe9403ff, 0x11111111, 0xbe9503ff, 0x22222222, 0xbe9703ff, 0x33333333, 0xbf068080,
    0xb1148001, 0xbf068180, 0xb1157fff, 0xbf068080, 0xb1171234, 0xb0801234, 0xba801801, 0x00000000,
    0xbe9603ff, 0x44444444, 0x7e140214, 0x7e160215, 0x7e180216, 0x7e1a0217, 0xe0701000, 0x80010a03,
    0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> RoundTowardZeroCode{0xba801801, 0x0000000f, 0xbf810000};

constexpr std::uint32_t Expected[4] = {0xffff8001u, 0x22222222u, 0x44444444u, 0x00001234u};
constexpr const char* Names[4] = {"cmovk_taken", "cmovk_not_taken", "after_setreg_imm32", "cmovk_positive"};

void Fill(std::uint32_t, std::uint32_t*) {}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("scalar sopk misc: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

ShaderRecompiler::RecompileRequest Request(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return request;
}

void ExpectRefused(AgcDriver::VulkanDevice& device) {
    const std::span<const std::uint32_t> code(RoundTowardZeroCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const std::vector<std::uint32_t> userData(8, 0u);
    bool refused = false;
    try {
        static_cast<void>(ShaderRecompiler::Recompile(Request(device, code, userData, memory)));
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused, "scalar sopk misc: s_setreg_imm32_b32 writing round toward zero to MODE was translated");
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto result = ShaderRecompiler::Recompile(Request(device, code, userData, memory));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < 4; ++index) Expect(tid, out[index], Expected[index], Names[index]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        ExpectRefused(*device);
        std::puts("scalar sopk misc tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
