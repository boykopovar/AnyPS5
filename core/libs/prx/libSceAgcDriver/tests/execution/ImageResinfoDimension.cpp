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
constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t Type2DArray = 13;
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) std::array<std::uint8_t, 65536> Texels{};

constexpr std::array<std::uint32_t, 8> Code(std::uint32_t dim) {
    return {0x34060084u, 0x7e020280u, 0xf0380f00u | (dim << 3u), 0x00010a01u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u};
}

alignas(256) constexpr auto Query1D = Code(0u);
alignas(256) constexpr auto Query3D = Code(2u);
alignas(256) constexpr auto QueryCube = Code(3u);
alignas(256) constexpr auto Query2DArray = Code(5u);

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t type, std::uint32_t depth) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (type << 28u),
        depth - 1u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> Query(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
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
    for (std::uint32_t tid = 1; tid < Threads; ++tid) {
        Require(std::equal(Output.begin(), Output.begin() + 4, Output.begin() + tid * 4u), "image_get_resinfo: lanes disagree");
    }
    return {Output[0], Output[1], Output[2], Output[3]};
}

std::string Text(const std::array<std::uint32_t, 4>& value) {
    return std::to_string(value[0]) + "x" + std::to_string(value[1]) + "x" + std::to_string(value[2]) + " levels " + std::to_string(value[3]);
}

using Result = std::array<std::uint32_t, 4>;

void Check(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> native, const std::array<std::uint32_t, 8>& texture, const char* what, const Result& own, const Result& oneDimensional) {
    const auto nativeResult = Query(device, native, texture);
    Require(nativeResult == own, std::string("image_get_resinfo: the ") + what + " query returned " + Text(nativeResult) + ", RDNA 2 returns " + Text(own));
    const auto lineResult = Query(device, Query1D, texture);
    Require(lineResult == oneDimensional, std::string("image_get_resinfo: DIM 1D on a ") + what + " T# returned " + Text(lineResult) + ", RDNA 2 returns " + Text(oneDimensional));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, QueryCube, Descriptor(TypeCube, 6u), "cube", {Size, Size, 6u, 1u}, {Size, Size, 0u, 1u});
        Check(*device, Query3D, Descriptor(Type3D, 4u), "3D", {Size, Size, 4u, 1u}, {Size, Size, 4u, 1u});
        Check(*device, Query2DArray, Descriptor(Type2DArray, 3u), "2D array", {Size, Size, 3u, 1u}, {Size, Size, 0u, 1u});
        std::puts("image resinfo dimension tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
