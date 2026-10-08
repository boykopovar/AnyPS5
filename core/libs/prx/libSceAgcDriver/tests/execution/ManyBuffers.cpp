#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::DescriptorRole;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t PushBuffers = 52;
constexpr std::uint32_t DataBuffers = 124;
constexpr std::uint32_t MaxBuffers = DataBuffers;
constexpr std::uint32_t RegionWords = 2u * Threads + 4u;
constexpr std::uint32_t RangeBytes = (RegionWords + 4u) * 4u;
constexpr std::uint32_t TableRegister = 0;
constexpr std::uint32_t DescriptorRegister = 4;
constexpr std::uint32_t NullRegister = 0x7du;
constexpr std::uint32_t ThreadVector = 0;
constexpr std::uint32_t OffsetVector = 1;
constexpr std::uint32_t DataVector = 2;
constexpr std::uint32_t ConstantZero = 0x80u;
constexpr std::uint32_t ConstantTwo = 0x82u;
constexpr std::uint32_t Literal = 0xffu;
constexpr std::uint32_t WaitScalarLoads = 0xbf8cc07fu;
constexpr std::uint32_t WaitVectorLoads = 0xbf8c3f70u;
constexpr std::uint32_t Endpgm = 0xbf810000u;

alignas(4096) std::array<std::uint32_t, (MaxBuffers + 1u) * RegionWords> Data{};
alignas(256) std::array<std::uint32_t, MaxBuffers * 4u> Table{};

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::uint32_t Increment(std::uint32_t buffer) {
    return (buffer + 1u) * 0x01000193u;
}

constexpr std::size_t CodeWords(std::uint32_t buffers) {
    return 2u + buffers * 10u;
}

template <std::uint32_t Buffers>
constexpr std::array<std::uint32_t, CodeWords(Buffers)> BuildCode() {
    constexpr std::uint32_t vLshlrevB32 = 0x1au;
    constexpr std::uint32_t vAddNcU32 = 0x25u;
    constexpr std::uint32_t sLoadDwordx4 = 0xf4080000u | (DescriptorRegister << 6u) | (TableRegister / 2u);
    constexpr std::uint32_t bufferLoadDword = 0xe0301000u;
    constexpr std::uint32_t bufferStoreDword = 0xe0701000u | (Threads * 4u);
    constexpr std::uint32_t operands = (ConstantZero << 24u) | ((DescriptorRegister / 4u) << 16u) | (DataVector << 8u) | OffsetVector;
    std::array<std::uint32_t, CodeWords(Buffers)> code{};
    std::size_t count = 0;
    code[count++] = Vop2(vLshlrevB32, OffsetVector, ConstantTwo, ThreadVector);
    for (std::uint32_t buffer = 0; buffer < Buffers; ++buffer) {
        code[count++] = sLoadDwordx4;
        code[count++] = (NullRegister << 25u) | (buffer * 16u);
        code[count++] = WaitScalarLoads;
        code[count++] = bufferLoadDword;
        code[count++] = operands;
        code[count++] = WaitVectorLoads;
        code[count++] = Vop2(vAddNcU32, DataVector, Literal, DataVector);
        code[count++] = Increment(buffer);
        code[count++] = bufferStoreDword;
        code[count++] = operands;
    }
    code[count++] = Endpgm;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, CodeWords(PushBuffers)> PushCode = BuildCode<PushBuffers>();
alignas(256) constexpr std::array<std::uint32_t, CodeWords(DataBuffers)> DataCode = BuildCode<DataBuffers>();

std::uint32_t BufferBase(std::uint32_t buffer) {
    return buffer * RegionWords + buffer % 4u;
}

std::uint32_t InputValue(std::uint32_t buffer, std::uint32_t tid) {
    return 0x40000000u | (buffer << 8u) | tid;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t buffers, bool shaderData) {
    const std::string label = "many buffers (" + std::to_string(buffers) + "): ";
    Data.fill(0xdeadbeefu);
    for (std::uint32_t buffer = 0; buffer < buffers; ++buffer) {
        const auto base = BufferBase(buffer);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) Data[base + tid] = InputValue(buffer, tid);
        const auto address = reinterpret_cast<std::uintptr_t>(&Data[base]);
        Table[buffer * 4u + 0u] = static_cast<std::uint32_t>(address);
        Table[buffer * 4u + 1u] = static_cast<std::uint32_t>((address >> 32u) & 0xffffu);
        Table[buffer * 4u + 2u] = RangeBytes;
        Table[buffer * 4u + 3u] = 0x31016facu;
    }
    const auto table = reinterpret_cast<std::uintptr_t>(Table.data());
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)};
    const std::span<const std::uint32_t> tableWords(Table.data(), buffers * 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}, {table, std::as_bytes(tableWords)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto guest = std::find_if(result.bindings.begin(), result.bindings.end(), [](const auto& binding) { return binding.role == DescriptorRole::GuestBuffers; });
    Require(guest != result.bindings.end() && guest->count == buffers, label + "the program does not bind every buffer");
    const bool hasShaderData = std::any_of(result.bindings.begin(), result.bindings.end(), [](const auto& binding) { return binding.role == DescriptorRole::ShaderData; });
    Require(hasShaderData == shaderData, label + (shaderData ? "the buffer offsets are not in shader data" : "the buffer offsets are not in push constants"));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t buffer = 0; buffer < buffers; ++buffer) {
        const auto base = BufferBase(buffer);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto input = Data[base + tid];
            const auto actual = Data[base + Threads + tid];
            const auto expected = InputValue(buffer, tid) + Increment(buffer);
            Require(input == InputValue(buffer, tid), label + "buffer " + std::to_string(buffer) + " thread " + std::to_string(tid) + " input changed to " + Hex(input));
            Require(actual == expected, label + "buffer " + std::to_string(buffer) + " thread " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, PushCode, PushBuffers, false);
        Run(*device, DataCode, DataBuffers, true);
        std::puts("many buffers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
