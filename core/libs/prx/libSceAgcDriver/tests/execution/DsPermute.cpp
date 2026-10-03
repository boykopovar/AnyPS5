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

alignas(256) constexpr std::array<std::uint32_t, 37> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xbf8c3f70, 0x7e1402ff,
    0xdeaddead, 0x7e1602ff, 0xdeaddead, 0x7e1802ff, 0xdeaddead, 0x7e1a02ff, 0xdeaddead, 0xdac80000,
    0x0a000504, 0xdacc0000, 0x0b000504, 0xdac80008, 0x0c000504, 0xbe94037e, 0xbefe03ff, 0x0000ffff,
    0xdac80000, 0x0d000504, 0xbefe0314, 0xbf8cc07f, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03,
    0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xbf810000,
};

constexpr std::uint32_t Rows[32][2] = {
    {0x000000f4u, 0x00001000u},
    {0x000000dcu, 0x00001001u},
    {0x000000f2u, 0x00001002u},
    {0x000000d9u, 0x00001003u},
    {0x0000000eu, 0x00001004u},
    {0x00000017u, 0x00001005u},
    {0x00000015u, 0x00001006u},
    {0x0000005cu, 0x00001007u},
    {0x000000d5u, 0x00001008u},
    {0x0000002bu, 0x00001009u},
    {0x000000bcu, 0x0000100au},
    {0x000000cfu, 0x0000100bu},
    {0x000000abu, 0x0000100cu},
    {0x000000dau, 0x0000100du},
    {0x0000004eu, 0x0000100eu},
    {0x00000040u, 0x0000100fu},
    {0x0000009bu, 0x00001010u},
    {0x00000036u, 0x00001011u},
    {0x0000009bu, 0x00001012u},
    {0x00000009u, 0x00001013u},
    {0x00000094u, 0x00001014u},
    {0x000000aeu, 0x00001015u},
    {0x00000028u, 0x00001016u},
    {0x000000ffu, 0x00001017u},
    {0x0000006eu, 0x00001018u},
    {0x000000a3u, 0x00001019u},
    {0x00000064u, 0x0000101au},
    {0x000000cdu, 0x0000101bu},
    {0x000000b9u, 0x0000101cu},
    {0x000000dcu, 0x0000101du},
    {0x000000feu, 0x0000101eu},
    {0x00000082u, 0x0000101fu},
};
constexpr std::uint32_t Expected[32][4] = {
    {0x0000101fu, 0x0000101du, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00001017u, 0x0000101eu, 0x00000000u},
    {0x00001013u, 0x0000101cu, 0x0000101fu, 0x00000000u},
    {0x00001004u, 0x00001016u, 0x00000000u, 0x00001004u},
    {0x00000000u, 0x00001003u, 0x00001013u, 0x00000000u},
    {0x00001014u, 0x00001005u, 0x00001004u, 0x00001006u},
    {0x00001012u, 0x00001005u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00001017u, 0x00001014u, 0x00000000u},
    {0x00001019u, 0x00001015u, 0x00001012u, 0x00000000u},
    {0x00000000u, 0x0000100au, 0x00000000u, 0x00000000u},
    {0x00001016u, 0x0000100fu, 0x00001019u, 0x0000100cu},
    {0x00001015u, 0x00001013u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x0000100au, 0x00001016u, 0x00000000u},
    {0x00001011u, 0x00001016u, 0x00001015u, 0x00000000u},
    {0x0000101cu, 0x00001013u, 0x00000000u, 0x00000000u},
    {0x0000100au, 0x00001010u, 0x00001011u, 0x0000100au},
    {0x0000100fu, 0x00001006u, 0x0000101cu, 0xdeaddeadu},
    {0x00000000u, 0x0000100du, 0x0000100au, 0xdeaddeadu},
    {0x00000000u, 0x00001006u, 0x0000100fu, 0xdeaddeadu},
    {0x0000101bu, 0x00001002u, 0x00000000u, 0xdeaddeadu},
    {0x00000000u, 0x00001005u, 0x00000000u, 0xdeaddeadu},
    {0x00001008u, 0x0000100bu, 0x0000101bu, 0xdeaddeadu},
    {0x0000100du, 0x0000100au, 0x00000000u, 0xdeaddeadu},
    {0x0000101du, 0x0000101fu, 0x00001008u, 0xdeaddeadu},
    {0x00000000u, 0x0000101bu, 0x0000100du, 0xdeaddeadu},
    {0x0000101au, 0x00001008u, 0x0000101du, 0xdeaddeadu},
    {0x00000000u, 0x00001019u, 0x00000000u, 0xdeaddeadu},
    {0x00001018u, 0x00001013u, 0x0000101au, 0xdeaddeadu},
    {0x00001002u, 0x0000100eu, 0x00000000u, 0xdeaddeadu},
    {0x00001000u, 0x00001017u, 0x00001018u, 0xdeaddeadu},
    {0x00000000u, 0x0000101fu, 0x00001002u, 0xdeaddeadu},
    {0x0000101eu, 0x00001000u, 0x00001000u, 0xdeaddeadu},
};
constexpr const char* Names[4] = {"permute", "bpermute", "permute_offset8", "permute_exec_low16"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    words[0] = Rows[tid][0];
    words[1] = Rows[tid][1];
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
    Require(actual == expected, std::string("ds permute: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < 4; ++index) Expect(tid, out[index], Expected[tid][index], Names[index]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < Threads) {
            std::printf("skipped, the device's subgroups are narrower than a wave (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device);
        Check();
        std::puts("ds permute tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
