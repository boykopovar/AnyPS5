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
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Results = 32;
constexpr std::uint32_t Width = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
alignas(256) std::array<std::uint32_t, 64> Texels{};
alignas(256) std::array<std::uint32_t, 64> StoredTexels{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 98> Code{
    0x34060087u, 0x36640087u, 0xd5480033u, 0x02050700u, 0xd5480034u, 0x02050900u, 0x7e6c0d32u, 0x066c6cf0u,
    0x106c6cffu, 0x3e800000u, 0x7e6e02f0u, 0x7e700280u, 0xbeac03ffu, 0xdeadbeefu, 0x7e14022cu, 0x7e16022cu,
    0x7e18022cu, 0x7e1a022cu, 0x7e1c022cu, 0x7e1e022cu, 0x7e20022cu, 0x7e22022cu, 0x7e24022cu, 0x7e26022cu,
    0x7e28022cu, 0x7e2a022cu, 0x7e2c022cu, 0x7e2e022cu, 0x7e30022cu, 0x7e32022cu, 0x7e34022cu, 0x7e36022cu,
    0x7e38022cu, 0x7e3a022cu, 0x7e3c022cu, 0x7e3e022cu, 0x7e40022cu, 0x7e42022cu, 0x7e44022cu, 0x7e46022cu,
    0x7e48022cu, 0x7e4a022cu, 0x7e4c022cu, 0x7e4e022cu, 0x7e50022cu, 0x7e52022cu, 0x7e54022cu, 0x7e56022cu,
    0xf0011108u, 0x00010a32u, 0xf0021108u, 0x00010c32u, 0xf0031108u, 0x00010e32u, 0xf09d0108u, 0x00611036u,
    0xf09e0108u, 0x00611236u, 0xf0011308u, 0x00011432u, 0xf0011308u, 0x80011732u, 0xf0051108u, 0x00011932u,
    0xf0391108u, 0x00011b38u, 0xf11d0108u, 0x00611d36u, 0xf09f0108u, 0x00612236u, 0xf0920108u, 0x00612436u,
    0xf0011908u, 0x00012632u, 0xbf8c3f70u, 0x385464ffu, 0x11220000u, 0xf0211108u, 0x00042a32u, 0x7e52032bu,
    0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xe0781020u, 0x80001203u, 0xe0781030u,
    0x80001603u, 0xe0781040u, 0x80001a03u, 0xe0781050u, 0x80001e03u, 0xe0781060u, 0x80002203u, 0xe0781070u,
    0x80002603u, 0xbf810000u,
};

constexpr const char* Names[Results] = {
    "image_load tfe", "image_load tfe status",
    "image_load lwe", "image_load lwe status",
    "image_load tfe lwe", "image_load tfe lwe status",
    "image_sample_lz tfe", "image_sample_lz tfe status",
    "image_sample_lz lwe", "image_sample_lz lwe status",
    "image_load dmask 3 tfe.x", "image_load dmask 3 tfe.y", "image_load dmask 3 tfe status",
    "image_load d16 dmask 3 tfe", "image_load d16 dmask 3 tfe status",
    "image_load_mip tfe", "image_load_mip tfe status",
    "image_get_resinfo tfe", "image_get_resinfo tfe status",
    "image_gather4_lz tfe.x", "image_gather4_lz tfe.y", "image_gather4_lz tfe.z", "image_gather4_lz tfe.w", "image_gather4_lz tfe status",
    "image_sample_lz tfe lwe", "image_sample_lz tfe lwe status",
    "image_sample_l lwe", "image_sample_l lwe status",
    "image_load dmask 9 tfe.x", "image_load dmask 9 tfe.w", "image_load dmask 9 tfe status",
    "image_store tfe status",
};

std::uint32_t Texel(std::uint32_t index) {
    return 0x41000000u + (index << 16u) + index;
}

std::array<std::uint32_t, Results> Expected(std::uint32_t lane) {
    const auto x = lane & 7u;
    const auto y = (lane >> 3u) & 1u;
    const auto mip = (lane >> 4u) & 1u;
    const bool inside = x < Width && y == 0u;
    const auto load = inside ? Texel(x) : 0u;
    const auto sample = Texel(x & 3u);
    const auto next = Texel((x + 1u) & 3u);
    const auto half = inside ? 0x4800u + 8u * x : 0u;
    const auto mipLoad = inside && mip == 0u ? Texel(x) : 0u;
    return {
        load, Sentinel, load, Sentinel, load, Sentinel,
        sample, Sentinel, sample, Sentinel,
        load, 0u, Sentinel,
        half, Sentinel,
        mipLoad, Sentinel,
        Width, Sentinel,
        sample, next, next, sample, Sentinel,
        sample, Sentinel,
        sample, Sentinel,
        load, 0x3f800000u, Sentinel,
        Sentinel,
    };
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t minLodWarn) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | (1u << 31u),
        0x204u | (Type2D << 28u),
        0u,
        minLodWarn << 8u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t minLodWarn, const std::string& run) {
    for (std::uint32_t index = 0; index < Texels.size(); ++index) Texels[index] = Texel(index);
    StoredTexels.fill(0u);
    Output.fill(0u);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), waveSize * Results * 4u, 0x31016facu};
    const auto texture = TextureDescriptor(Texels.data(), minLodWarn);
    const auto stored = TextureDescriptor(StoredTexels.data(), 0u);
    const std::array<std::uint32_t, 4> sampler{0u, 0x00fff000u, 0u, 0u};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    std::copy(stored.begin(), stored.end(), userData.begin() + 16);
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(StoredTexels.data()), sizeof(StoredTexels), nullptr, "test");
    device.WaitIdle();
    const auto prefix = run + " MIN_LOD_WARN " + Hex(minLodWarn);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto expected = Expected(lane);
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[lane * Results + index];
            if (actual != expected[index]) {
                Mismatches.push_back(prefix + " lane " + std::to_string(lane) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected[index]));
            }
        }
    }
    for (std::uint32_t index = 0; index < StoredTexels.size(); ++index) {
        const auto expected = index < Width ? 0x11220000u | index : 0u;
        if (StoredTexels[index] != expected) {
            Mismatches.push_back(prefix + " image_store tfe texel " + std::to_string(index) + " is " + Hex(StoredTexels[index]) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(StoredTexels.data(), sizeof(StoredTexels), true, true, true);
        for (const auto minLodWarn : {0u, 0xfffu}) {
            Run(*device, 32, device->Target(), minLodWarn, "wave32");
            Run(*device, 64, device->Target(), minLodWarn, "wave64");
            Run(*device, 64, device->ComputeTarget(32), minLodWarn, "wave64 split");
        }
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " image texel status bounds mismatches");
        std::puts("image texel status bounds tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
