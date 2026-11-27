#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "Recompiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
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

constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 11;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32Float = 22;
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
alignas(256) std::array<std::uint32_t, Width> Texels{};
alignas(256) std::array<std::uint32_t, Width> NullStored{};
alignas(256) std::array<std::uint32_t, Width> Stored{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 34> Code{
    0x7e500300u, 0x7e520280u, 0x7e540280u, 0x7e580d00u, 0x065858f0u, 0x105858ffu, 0x3c800000u, 0x7e5a02f0u,
    0x385c00ffu, 0xc0de0000u, 0x385e00ffu, 0x7e000000u, 0xf0001f08u, 0x00020a28u, 0xf09c0108u, 0x01020e2cu,
    0xf0381f08u, 0x00020f2au, 0xf0041308u, 0x00021328u, 0xf0201108u, 0x00042e28u, 0xf0201108u, 0x00062f28u,
    0xbf8c3f70u, 0xbbfd0000u, 0x34060086u, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xe0781020u,
    0x80001203u, 0xbf810000u,
};

constexpr const char* Names[Checked] = {
    "image_load.x", "image_load.y", "image_load.z", "image_load.w",
    "image_sample_lz",
    "image_get_resinfo.x", "image_get_resinfo.y", "image_get_resinfo.z", "image_get_resinfo.w",
    "image_load_mip.x", "image_load_mip.y",
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t type, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (type << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t type, std::uint32_t format, const std::string& run) {
    for (std::uint32_t index = 0; index < Width; ++index) {
        Texels[index] = 0x41000000u + (index << 16u) + index;
        NullStored[index] = 0x5a5a0000u | index;
        Stored[index] = 0x5a5a0000u | index;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(36, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), waveSize * Results * 4u, 0x31016facu};
    const auto texture = TextureDescriptor(Texels.data(), type, format);
    const auto nullStored = TextureDescriptor(NullStored.data(), type, format);
    const auto stored = TextureDescriptor(Stored.data(), Type2D, format);
    const std::array<std::uint32_t, 4> sampler{0u, 0x00fff000u, 0u, 0u};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(nullStored.begin(), nullStored.end(), userData.begin() + 16);
    std::copy(stored.begin(), stored.end(), userData.begin() + 24);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 32);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span{Code})}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {true, false, false}, false, 1};
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(Stored.data()), sizeof(Stored), nullptr, "test");
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(NullStored.data()), sizeof(NullStored), nullptr, "test");
    device.WaitIdle();
    const auto prefix = run + " TYPE " + std::to_string(type) + " FORMAT " + std::to_string(format);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t index = 0; index < Checked; ++index) {
            const auto actual = Output[lane * Results + index];
            if (actual != 0u) Mismatches.push_back(prefix + " lane " + std::to_string(lane) + " " + Names[index] + " is " + Hex(actual) + ", expected 0");
        }
    }
    for (std::uint32_t index = 0; index < Width; ++index) {
        const auto unchanged = 0x5a5a0000u | index;
        if (NullStored[index] != unchanged) Mismatches.push_back(prefix + " image_store through the null T# changed texel " + std::to_string(index) + " to " + Hex(NullStored[index]));
        const auto expected = index < waveSize ? 0x7e000000u | index : unchanged;
        if (Stored[index] != expected) Mismatches.push_back(prefix + " image_store through the TYPE 9 T# wrote " + Hex(Stored[index]) + " to texel " + std::to_string(index) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(NullStored.data(), sizeof(NullStored), true, true, true);
        GuestAllocations::Mutation().Add(Stored.data(), sizeof(Stored), true, true, true);
        for (const auto type : {0u, 1u}) {
            for (const auto format : {Format32Float, Format32UInt}) {
                Run(*device, 32, device->Target(), type, format, "wave32");
                Run(*device, 64, device->Target(), type, format, "wave64");
                Run(*device, 64, device->ComputeTarget(32), type, format, "wave64 split");
            }
        }
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " null image type mismatches");
        std::puts("image null type tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
