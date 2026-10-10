#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Garbage = 0xabcd1234u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 30> SdwaCode{
    0x34020082, 0x34060082, 0xe0302000, 0x80000401,
    0x7e1402ff, 0xabcd1234, 0x7e1602ff, 0xabcd1234, 0x7e1802ff, 0xabcd1234, 0x7e1a02ff, 0xabcd1234,
    0xbf8c3f70, 0x7e14a0f9, 0x00031404, 0x7e16a0f9, 0x00000604, 0x7e18a0f9, 0x00011504, 0x7e1aa0f9, 0x00020604,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03,
    0xbf810000,
};

std::uint32_t HalfBits(std::uint32_t value) {
    if (value == 0u) return 0u;
    int exponent = 0;
    const float mantissa = std::frexp(static_cast<float>(value), &exponent);
    const auto fraction = static_cast<std::uint32_t>(std::ldexp(mantissa, 11));
    return (static_cast<std::uint32_t>(exponent + 14) << 10u) | (fraction & 0x3ffu);
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = tid | ((tid + 64u) << 8u) | ((255u - tid) << 16u) | (((tid * 3u + 128u) & 0xffu) << 24u);
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(SdwaCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto source = Input[tid * Inputs];
        const auto* out = &Output[tid * Results];
        const auto half = [&](std::uint32_t byte) { return HalfBits((source >> (byte * 8u)) & 0xffu); };
        const auto where = [&](std::uint32_t result) { return "sdwa cvt f16 u16: thread " + std::to_string(tid) + " result " + std::to_string(result) + " is " + Hex(out[result]); };
        const auto preserved = (Garbage & 0xffff0000u) | half(3u);
        Require(out[0] == preserved, where(0) + ", expected " + Hex(preserved) + ": src0_sel:BYTE_3 dst_sel:WORD_0 dst_unused:UNUSED_PRESERVE must convert the top byte and keep the high half");
        Require(out[1] == half(0u), where(1) + ", expected " + Hex(half(0u)) + ": src0_sel:BYTE_0 dst_sel:DWORD must convert the low byte");
        const auto high = (half(1u) << 16u) | (Garbage & 0xffffu);
        Require(out[2] == high, where(2) + ", expected " + Hex(high) + ": src0_sel:BYTE_1 dst_sel:WORD_1 dst_unused:UNUSED_PRESERVE must keep the low half");
        Require(out[3] == half(2u), where(3) + ", expected " + Hex(half(2u)) + ": src0_sel:BYTE_2 dst_sel:DWORD must convert the third byte");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        std::puts("sdwa cvt f16 u16 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
