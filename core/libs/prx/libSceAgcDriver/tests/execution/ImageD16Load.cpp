#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 16;
constexpr std::size_t Bytes = 16384;
constexpr std::uint32_t Sentinel = 0xcafe0000u;
alignas(256) std::array<std::uint8_t, Bytes> Texels{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};

struct Case {
    const char* name;
    std::uint32_t format;
    std::uint32_t bytes;
    std::array<std::uint32_t, Width> texels;
    std::array<std::uint32_t, Width> expected;
};

constexpr std::array<Case, 6> Cases{{
    {"32_UINT", 20u, 4u,
        {0x0u, 0x1u, 0x7fffu, 0x8000u, 0xffffu, 0x10000u, 0x12345u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0x2u, 0x3u, 0xfffeu, 0x1ffffu, 0x40000000u, 0x5u},
        {0x0u, 0x1u, 0x7fffu, 0x8000u, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0x2u, 0x3u, 0xfffeu, 0xffffu, 0xffffu, 0x5u}},
    {"32_SINT", 21u, 4u,
        {0x0u, 0x1u, 0xffffffffu, 0x7fffu, 0x8000u, 0xffff8000u, 0xffff7fffu, 0xffffu, 0x7fffffffu, 0x80000000u, 0x186a0u, 0xfffe7960u, 0x2u, 0xfffffffeu, 0x7ffeu, 0xffff8001u},
        {0x0u, 0x1u, 0xffffu, 0x7fffu, 0x7fffu, 0x8000u, 0x8000u, 0x7fffu, 0x7fffu, 0x8000u, 0x7fffu, 0x8000u, 0x2u, 0xfffeu, 0x7ffeu, 0x8001u}},
    {"32_FLOAT", 22u, 4u,
        {0x3f801000u, 0x3f803000u, 0x3f801001u, 0x3f80ffffu, 0xbf801001u, 0x33400000u, 0x337fffffu, 0x33c00000u, 0x387ff000u, 0x477ff000u, 0xc77ff001u, 0x7f800000u, 0xff800000u, 0x7f800001u, 0x7f804001u, 0x7fc00001u},
        {0x3c00u, 0x3c01u, 0x3c00u, 0x3c07u, 0xbc00u, 0x0u, 0x0u, 0x1u, 0x3ffu, 0x7bffu, 0xfbffu, 0x7c00u, 0xfc00u, 0x7c01u, 0x7c02u, 0x7e00u}},
    {"16_FLOAT", 13u, 2u,
        {0x7c01u, 0xfc30u, 0x7e00u, 0xfe00u, 0x7fffu, 0x7c00u, 0xfc00u, 0x1u, 0x8001u, 0x3c00u, 0x7bffu, 0xd400u, 0x7d23u, 0x0u, 0x8000u, 0x3555u},
        {0x7c01u, 0xfc30u, 0x7e00u, 0xfe00u, 0x7fffu, 0x7c00u, 0xfc00u, 0x1u, 0x8001u, 0x3c00u, 0x7bffu, 0xd400u, 0x7d23u, 0x0u, 0x8000u, 0x3555u}},
    {"8_UNORM", 1u, 1u,
        {0x0u, 0x1u, 0x2u, 0x3u, 0x9u, 0x11u, 0x40u, 0x64u, 0x7fu, 0x80u, 0x81u, 0xc8u, 0xfau, 0xfdu, 0xfeu, 0xffu},
        {0x0u, 0x1c04u, 0x2004u, 0x2206u, 0x2885u, 0x2c44u, 0x3404u, 0x3646u, 0x37f8u, 0x3804u, 0x380cu, 0x3a46u, 0x3bd8u, 0x3bf0u, 0x3bf8u, 0x3c00u}},
    {"8_SNORM", 2u, 1u,
        {0x0u, 0x1u, 0x7fu, 0x80u, 0x81u, 0xffu, 0x40u, 0xc0u, 0x9u, 0x10u, 0x30u, 0x50u, 0xf0u, 0x7eu, 0x82u, 0x55u},
        {0x0u, 0x2008u, 0x3c00u, 0xbc00u, 0xbc00u, 0xa008u, 0x3808u, 0xb808u, 0x2c89u, 0x3008u, 0x360cu, 0x390au, 0xb008u, 0x3bf0u, 0xbbf0u, 0x395bu}},
}};

std::vector<std::uint32_t> Code() {
    std::vector<std::uint32_t> code{0x34060084u, 0xe0381000u, 0x80031e03u, 0xbf8c3f70u};
    for (std::uint32_t index = 0; index < 4u; ++index) {
        code.push_back(0x7e0002ffu | ((10u + index) << 17u));
        code.push_back(Sentinel + index);
    }
    code.push_back(0xf0000008u | (0x1u << 8u));
    code.push_back(0x80010a1eu);
    code.push_back(0xbf8c3f70u);
    code.push_back(0xe0781000u);
    code.push_back(0x80000a03u);
    code.push_back(0xbf810000u);
    return code;
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels.data());
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u, 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Fill(const Case& test) {
    Texels.fill(0xeeu);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(Descriptor(test.format)));
    Require(!surface.mips.empty(), "image D16 load has no mip");
    for (std::uint32_t x = 0; x < Width; ++x) {
        const auto offset = surface.mips[0].tiledOffset + x * test.bytes;
        Require(offset + test.bytes <= Texels.size(), "image D16 load texture size");
        std::memcpy(Texels.data() + offset, &test.texels[x], test.bytes);
    }
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Input[thread * 4u + 0u] = thread % Width;
        Input[thread * 4u + 1u] = 0u;
        Input[thread * 4u + 2u] = 0u;
        Input[thread * 4u + 3u] = 0u;
    }
}

void Run(AgcDriver::VulkanDevice& device, const Case& test) {
    Fill(test);
    Output.fill(0xdeadbeefu);
    const auto code = Code();
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto descriptor = Descriptor(test.format);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    std::copy(input.begin(), input.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto x = thread % Width;
        for (std::uint32_t index = 0; index < 4u; ++index) {
            const auto expected = index == 0u ? test.expected[x] : Sentinel + index;
            const auto actual = Output[thread * 4u + index];
            Require(actual == expected, std::string("image_load d16 of ") + test.name + " texel " + Hex(test.texels[x]) + ": v" + std::to_string(10u + index) + " is " + Hex(actual) + ", RDNA 2 returns " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(Texels.data(), Bytes, true, true, true);
        for (const auto& test : Cases) Run(*device, test);
        GuestAllocations::Mutation().Remove(Texels.data());
        std::puts("image D16 load tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
