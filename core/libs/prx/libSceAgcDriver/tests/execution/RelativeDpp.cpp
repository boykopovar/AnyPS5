#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 5;
constexpr std::uint32_t Stride = 8;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 39> Code{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0x7e1402b0,
    0x7e1602c0, 0xbefc0381, 0x7e0886e9, 0xfac68808, 0x7e1084e9, 0xfac68808, 0x7e0a0309, 0x7e1202a0,
    0x7e1488e9, 0xfac68808, 0x7e0c030b, 0xbefc03ff, 0x00010001, 0x7e1202a0, 0x7e1490e9, 0xfac68808,
    0x7e0e030b, 0xbefc0381, 0x7e1886fa, 0xff00e408, 0xe0701000, 0x80010403, 0xe0701004, 0x80010503,
    0xe0701008, 0x80010603, 0xe070100c, 0x80010703, 0xe0701010, 0x80010c03, 0xbf810000,
};

constexpr std::array<std::uint32_t, Results> Expected{0x20u, 0x10u, 0x20u, 0x20u, 0x20u};

alignas(256) constexpr std::array<std::uint32_t, 17> MaskedNoFiCode{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0xbefc0381,
    0x7e0802ff, 0xdeadbeef, 0xbefe048f, 0x7e0886e9, 0xfacfac08, 0xbefe04c1, 0xe0701000, 0x80010403,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> MaskedFiCode{
    0x34020084, 0x34060085, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e100290, 0x7e1202a0, 0xbefc0381,
    0x7e0802ff, 0xdeadbeef, 0xbefe048f, 0x7e0886ea, 0xfacfac08, 0xbefe04c1, 0xe0701000, 0x80010403,
    0xbf810000,
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

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads) {
    Input.fill(0xdeadbeefu);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        for (std::uint32_t component = 0; component < Results; ++component) {
            const std::uint32_t actual = Output[tid * Stride + component];
            Require(actual == Expected[component], std::string("relative dpp: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " register " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(Expected[component]));
        }
    }
}

void RunMasked(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, std::span<const std::uint32_t> code, std::uint32_t activeValue) {
    Input.fill(0xdeadbeefu);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        const std::uint32_t expected = tid < 4u ? activeValue : 0xdeadbeefu;
        const std::uint32_t actual = Output[tid * Stride];
        Require(actual == expected, std::string("relative dpp masked: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32, 32);
        Run(*device, 64, 64);
        RunMasked(*device, 32, 32, MaskedNoFiCode, 0u);
        RunMasked(*device, 64, 64, MaskedNoFiCode, 0u);
        RunMasked(*device, 32, 32, MaskedFiCode, 0x20u);
        RunMasked(*device, 64, 64, MaskedFiCode, 0x20u);
        std::puts("relative dpp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
