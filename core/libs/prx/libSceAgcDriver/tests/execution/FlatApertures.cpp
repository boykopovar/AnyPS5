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

constexpr std::uint32_t Threads = 32;
alignas(256) std::array<std::uint32_t, Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020082, 0x7e0402ff, 0x80000000, 0x4a0600ff, 0x00001000, 0x7e080288, 0x7e0a02ff, 0x70000000,
    0x4a0c00ff, 0x00002000, 0xdc700000, 0x007d0301, 0xdc700000, 0x007d0604, 0xbf8c0070, 0xdc300000,
    0x077d0001, 0xdc300000, 0x087d0004, 0xbf8c0070, 0xe0701000, 0x80000701, 0xe0701080, 0x80000801,
    0xbf810000, 0xbf810000, 0xbf810000, 0xbf810000, 0xbf810000, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 256u, {false, false, false}, false, 1};
    compute.scratchDwords = 4u;
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
        Require(Output[tid] == tid + 0x1000u, "flat apertures: lane " + std::to_string(tid) + " read " + Hex(Output[tid]) + " back through the shared aperture, expected " + Hex(tid + 0x1000u));
        Require(Output[Threads + tid] == tid + 0x2000u, "flat apertures: lane " + std::to_string(tid) + " read " + Hex(Output[Threads + tid]) + " back through the private aperture, expected " + Hex(tid + 0x2000u));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("flat aperture tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
