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
constexpr std::uint32_t ShaderClockCapability = 5055;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 49> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xf4900200, 0x00000000, 0xf4940280, 0x00000000,
    0xbf8c0070, 0x7e0a0280, 0xbe9003ff, 0x000007d0, 0xd5430005, 0x04110705, 0x80908110, 0xbf078010,
    0xbf85fffb, 0xf4900300, 0x00000000, 0xf4940380, 0x00000000, 0xbf8cc07f, 0x7e140208, 0x7e160209,
    0x7e18020a, 0x7e1a020b, 0x7e1c020c, 0x7e1e020d, 0x7e20020e, 0x7e22020f, 0xe0701000, 0x80010a03,
    0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03,
    0xe0701014, 0x80010f03, 0xe0701018, 0x80011003, 0xe070101c, 0x80011103, 0xe0701020, 0x80010503,
    0xbf810000,
};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    words[0] = tid + 1u;
}

std::uint64_t Pair(const std::uint32_t* words) {
    return static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32u);
}

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
    Require(actual == expected, std::string("shader clock: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        const std::uint32_t* first = &Output[(tid & ~7u) * Results];
        for (std::uint32_t index = 0; index < 8; ++index) Expect(tid, out[index], first[index], "clock read uniform over the wave");
        Require(Pair(out + 4) >= Pair(out), "memtime: lane " + std::to_string(tid) + " went back from " + std::to_string(Pair(out)) + " to " + std::to_string(Pair(out + 4)));
        Require(Pair(out + 6) >= Pair(out + 2), "memrealtime: lane " + std::to_string(tid) + " went back from " + std::to_string(Pair(out + 2)) + " to " + std::to_string(Pair(out + 6)));
        Require(Pair(out + 4) != 0u && Pair(out + 6) != 0u, "clocks read zero");
        std::uint32_t sum = 0u;
        for (std::uint32_t step = 0; step < 2000u; ++step) sum = (sum & 0xffffffu) * 3u + in[0];
        Expect(tid, out[8], sum, "loop result");
    }
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
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
    constexpr std::uint32_t Passes = 128;
    for (std::uint32_t pass = 0; pass < Passes; ++pass) {
        Output.fill(0xdeadbeefu);
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
        Check();
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto capabilities = device->Target().supportedCapabilities;
        if (std::find(capabilities.begin(), capabilities.end(), ShaderClockCapability) == capabilities.end()) {
            std::puts("skipped, the device lacks shaderSubgroupClock or shaderDeviceClock");
            return VulkanTestSkipped;
        }
        Run(*device);
        Check();
        std::puts("shader clock tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
