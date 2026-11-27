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

constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Offsets = 16;
constexpr std::uint32_t DataBytes = 64;
alignas(256) std::array<std::uint32_t, 64 * 4> Input{};
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
alignas(256) std::array<std::uint8_t, DataBytes> Data{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0x7e1c0280u,
    0xe0201000u, 0x80020a04u,
    0xe0281000u, 0x80020b04u,
    0xe0301000u, 0x80020c04u,
    0xe0341000u, 0x80020d04u,
    0xbf8c3f70u,
    0xe0781000u, 0x80010a02u,
    0xe0781010u, 0x80010e02u,
    0xbf810000u,
};

constexpr std::uint32_t Records[] = {1u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Bytes(std::uint32_t offset, std::uint32_t size, std::uint32_t records) {
    if (offset + size > records) return 0u;
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0; byte < size; ++byte) value |= static_cast<std::uint32_t>(Data[offset + byte]) << (8u * byte);
    return value;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t records, const std::string& run) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) Input[lane * 4u] = lane % Offsets;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    const auto input = BufferDescriptor(Input.data(), waveSize * 16u);
    const auto output = BufferDescriptor(Output.data(), waveSize * Results * 4u);
    const auto data = BufferDescriptor(Data.data(), records);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(data.begin(), data.end(), userData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span{Code})}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    constexpr const char* names[5] = {"buffer_load_ubyte", "buffer_load_ushort", "buffer_load_dword", "buffer_load_dwordx2.x", "buffer_load_dwordx2.y"};
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const std::uint32_t offset = lane % Offsets;
        const std::uint32_t expected[5] = {Bytes(offset, 1u, records), Bytes(offset, 2u, records), Bytes(offset, 4u, records), Bytes(offset, 4u, records), Bytes(offset + 4u, 4u, records)};
        constexpr std::uint32_t sizes[5] = {1u, 2u, 4u, 4u, 4u};
        for (std::uint32_t index = 0; index < 5u; ++index) {
            const std::uint32_t start = index == 4u ? offset + 4u : offset;
            if (start % sizes[index] != 0u) continue;
            if (start + sizes[index] <= records && (start & ~3u) + 4u > records) continue;
            const std::uint32_t actual = Output[lane * Results + index];
            if (actual != expected[index]) {
                Mismatches.push_back(run + " NUM_RECORDS " + std::to_string(records) + " lane " + std::to_string(lane) + " " + names[index] + " at offset " + std::to_string(offset) + " is " + Hex(actual) + ", expected " + Hex(expected[index]));
            }
        }
    }
}

}

int main() {
    try {
        for (std::uint32_t byte = 0; byte < DataBytes; ++byte) Data[byte] = static_cast<std::uint8_t>(0xa0u + byte);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto records : Records) {
            Run(*device, 32, device->Target(), records, "wave32");
            Run(*device, 64, device->Target(), records, "wave64");
            Run(*device, 64, device->ComputeTarget(32), records, "wave64 split");
        }
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " raw buffer record tail mismatches");
        std::puts("raw buffer record tail tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
