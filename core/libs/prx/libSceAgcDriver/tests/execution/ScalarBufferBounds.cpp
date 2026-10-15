#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t DataBytes = 64;
constexpr std::uint32_t ResultBytes = Lanes * 4;
constexpr std::uint32_t Fill = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, DataBytes / 4> Data{};
alignas(256) std::array<std::uint32_t, Lanes> Output{};

struct Case {
    std::uint32_t records;
    std::uint32_t stride;
    std::uint32_t soffset;
    std::uint32_t immediate;
};

std::array<std::uint32_t, 8> Code(std::uint32_t immediate) {
    return {0x34020082, 0xf4200400, 0x10000000u | immediate, 0xbf8c0000, 0x7e040210, 0xe0701000, 0x80010201, 0xbf810000};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t records, std::uint32_t stride) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Expected(const Case& test) {
    const auto address = test.soffset + test.immediate;
    if (address < test.soffset) return 0;
    const auto byte = static_cast<std::uint64_t>(address & ~3u);
    const auto size = test.stride == 0 ? static_cast<std::uint64_t>(test.records) : static_cast<std::uint64_t>(test.stride) * test.records;
    if (byte + 4u > size) return 0;
    Require(byte + 4u <= DataBytes, "scalar buffer bounds: an in-bounds case reads past the test buffer");
    return Data[byte / 4u];
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, bool robust) {
    for (std::uint32_t index = 0; index < Data.size(); ++index) Data[index] = 0x10203040u + index * 0x01010101u;
    Output.fill(Fill);
    const auto code = Code(test.immediate);
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(Data.data(), test.records, test.stride), BufferDescriptor(Output.data(), ResultBytes, 0)}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    userData.push_back(test.soffset);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    auto target = device.Target();
    target.robustBufferAccess2 = robust;
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto expected = Expected(test);
    const auto run = "scalar buffer bounds records " + std::to_string(test.records) + " stride " + std::to_string(test.stride) + " soffset " + Hex(test.soffset) + " offset " + std::to_string(test.immediate) + (robust ? " robustBufferAccess2" : "");
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        Require(Output[lane] == expected, run + ": lane " + std::to_string(lane) + " is " + Hex(Output[lane]) + ", expected " + Hex(expected));
    }
}

void Run(AgcDriver::VulkanDevice& device, const Case& test) {
    Run(device, test, false);
    if (device.Target().robustBufferAccess2) Run(device, test, true);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t records = 0; records <= 9; ++records) {
            for (const auto soffset : {0u, 4u, 5u}) Run(*device, {records, 0, soffset, 0});
        }
        for (const auto soffset : {56u, 59u, 60u, 61u, 63u, 64u, 68u}) {
            Run(*device, {64, 0, soffset, 0});
            Run(*device, {63, 0, soffset, 0});
            Run(*device, {61, 0, soffset, 0});
        }
        Run(*device, {64, 0, 0, 60});
        Run(*device, {64, 0, 56, 4});
        Run(*device, {64, 0, 0xfffffffcu, 8});
        Run(*device, {64, 0, 0xfffffff8u, 4});
        for (const auto soffset : {0u, 8u, 12u, 16u}) {
            Run(*device, {4, 4, soffset, 0});
            Run(*device, {3, 4, soffset, 0});
            Run(*device, {1, 16, soffset, 0});
        }
        std::puts("scalar buffer bounds tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
