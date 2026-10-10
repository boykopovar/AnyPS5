#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 40;
constexpr std::uint32_t Height = 24;
constexpr std::uint32_t Depth = 20;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t TileZ64KBX = 0x18;
constexpr std::uint32_t TileS64KBX = 0x19;
constexpr std::uint32_t TileD64KBX = 0x1a;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
constexpr std::size_t TexelBytes = 2u << 20u;
alignas(4096) std::array<std::uint8_t, TexelBytes + 65536u> TexelStorage{};
const std::span<std::uint8_t, TexelBytes> Texels(TexelStorage.data() + ((0u - reinterpret_cast<std::uintptr_t>(TexelStorage.data())) & 0xffffu), TexelBytes);

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf0001f10, 0x00020602, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t tileMode) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (tileMode << 20u) | (Type3D << 28u),
        Depth - 1u, 0u, 0u, 0u,
    };
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return {static_cast<std::uint8_t>(x * 6u + 1u), static_cast<std::uint8_t>(y * 10u + 2u), static_cast<std::uint8_t>(z * 12u + 3u), 0xff};
}

std::uint64_t EquationOffset(const TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12) & (mask & 0xfff000u)) ^ ((z << 24) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

void FillTexels(std::uint32_t tileMode) {
    std::ranges::fill(Texels, std::uint8_t{0});
    const auto descriptor = DecodeTextureResource(TextureDescriptor(tileMode));
    const auto geometry = DescribeSurface(descriptor);
    Require(geometry.guestBytes <= Texels.size(), "volume does not fit the test allocation");
    const auto* equation = FindTextureSwizzleEquation(geometry.thick ? 0x100u | XorSwizzleMode(descriptor.tileMode) : XorSwizzleMode(descriptor.tileMode), 4u);
    Require(equation != nullptr, "volume swizzle equation is missing");
    const auto& mip = geometry.mips.at(0);
    const auto block = geometry.thick ? ThickBlockExtent(descriptor.tileMode, 4u) : std::array<std::uint32_t, 3>{ThinBlockLayout(descriptor.tileMode, 4u)[1], ThinBlockLayout(descriptor.tileMode, 4u)[2], 1u};
    for (std::uint32_t z = 0; z < Depth; ++z) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                auto offset = geometry.GuestLayerOffset(z) + mip.tiledOffset;
                if (mip.tail) offset += EquationOffset(*equation, x + mip.tailX, y + mip.tailY, z);
                else offset += (static_cast<std::uint64_t>(y / block[1]) * mip.blocksPerRow + x / block[0]) * 65536u + EquationOffset(*equation, x, y, z);
                const auto texel = Texel(x, y, z);
                std::copy(texel.begin(), texel.end(), Texels.begin() + static_cast<std::ptrdiff_t>(offset));
            }
        }
    }
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = (tid * 13u + 7u) % Width;
        Input[tid * 4u + 1u] = (tid * 5u + 3u) % Height;
        Input[tid * 4u + 2u] = (tid * 7u + 1u) % Depth;
        Input[tid * 4u + 3u] = 0u;
    }
    Input[0] = 0u; Input[1] = 0u; Input[2] = 0u;
    Input[4] = Width - 1u; Input[5] = Height - 1u; Input[6] = Depth - 1u;
    Input[8] = 33u; Input[9] = 17u; Input[10] = 16u;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t tileMode, const char* name) {
    FillTexels(tileMode);
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(tileMode);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
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
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Texel(Input[tid * 4u], Input[tid * 4u + 1u], Input[tid * 4u + 2u]);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float scaled = Output[tid * 4u + component] * 255.0f;
            Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && std::lround(scaled) == expected[component], std::string(name) + ": texel (" + std::to_string(Input[tid * 4u]) + ", " + std::to_string(Input[tid * 4u + 1u]) + ", " + std::to_string(Input[tid * 4u + 2u]) + ") component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device, TileS64KBX, "thick SW_64KB_S_X volume");
        Run(*device, TileD64KBX, "thick SW_64KB_D_X volume");
        Run(*device, TileZ64KBX, "thin SW_64KB_Z_X volume");
        std::puts("image XOR swizzle volume tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
