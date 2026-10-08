#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 512;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint64_t SurfaceBytes = std::uint64_t{Side} * Side * 4u;
constexpr std::uint64_t Stride = 0x10000;
constexpr std::uint64_t FixedBudget = 16ull << 20u;
constexpr std::uint64_t TestBudget = 32ull << 20u;
constexpr std::uint32_t MaxSurfaces = 80;

alignas(256) constexpr std::array<std::uint32_t, 10> LoadStoreCode{
    0x7e040280, 0x7e060280, 0xf0001108, 0x00010402, 0xbf8c3f70, 0xf0200108, 0x00010402, 0xe0700000, 0x80000400, 0xbf810000,
};

alignas(256) std::array<std::uint32_t, 64> Output{};

std::uint32_t Marker(std::uint32_t surface) {
    return 0x5a000000u | surface;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::shared_ptr<StorageTexture> Load(AgcDriver::VulkanDevice& device, std::uint64_t base, std::uint32_t surface) {
    const auto address = base + surface * Stride;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor(address);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(LoadStoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0u, {false, false, false}, false, 1};
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
    StorageTexture::FlushPending(address, static_cast<std::size_t>(SurfaceBytes), nullptr, "test");
    device.WaitIdle();
    Require(Output[0] == Marker(surface), "surface " + std::to_string(surface) + ": the shader read " + std::to_string(Output[0]) + " instead of its first texel");
    return StorageTexture::FindLive(address, SurfaceBytes);
}

}

int main() {
    try {
#ifdef _WIN32
        Require(_putenv_s("APS5_TEXTURE_CACHE_MIB", "32") == 0, "cannot set the test cache budget");
#else
        Require(setenv("APS5_TEXTURE_CACHE_MIB", "32", 1) == 0, "cannot set the test cache budget");
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto budget = TestBudget;
        std::vector<std::uint32_t> storage((SurfaceBytes + MaxSurfaces * Stride + Stride) / 4u, 0u);
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + Stride - 1u) & ~std::uintptr_t{Stride - 1u});
        for (std::uint32_t surface = 0; surface < MaxSurfaces; ++surface) texels[surface * Stride / 4u] = Marker(surface);
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
        const auto blockBytes = static_cast<std::size_t>(SurfaceBytes + (MaxSurfaces - 1u) * Stride);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, blockBytes, true, true, true);
        }
        const auto release = [&] {
            AgcDriver::Graphics::ClearCachedTextures(device->Device());
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
        };
        try {
            const auto first = Load(*device, base, 0);
            Require(first != nullptr, "surface 0 has no cached storage image");
            const auto held = std::max<std::uint64_t>(first->AllocationBytes(), first->GuestBytes());
            const auto reused = static_cast<std::uint32_t>(FixedBudget / held) + 1u;
            std::printf("texture cache budget %llu MiB; each 512x512 32_UINT surface holds %llu MiB; cyclic set of %u surfaces (%llu MiB)\n", static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(held >> 20u), reused, static_cast<unsigned long long>((reused * held) >> 20u));
            Require(budget >= reused * held, "the cyclic set exceeds the test cache budget");
            std::vector<std::weak_ptr<StorageTexture>> images(reused);
            images[0] = first;
            for (std::uint32_t surface = 1; surface < reused; ++surface) images[surface] = Load(*device, base, surface);
            std::uint32_t remade = 0;
            for (std::uint32_t surface = 0; surface < reused; ++surface) {
                const auto again = Load(*device, base, surface);
                if (again == nullptr || again != images[surface].lock()) ++remade;
            }
            Require(remade == 0, std::to_string(remade) + " of " + std::to_string(reused) + " storage images were made again on the second pass over a " + std::to_string((reused * held) >> 20u) + " MiB cyclic set under a " + std::to_string(budget >> 20u) + " MiB budget");
            const auto capacity = static_cast<std::uint32_t>(budget / held);
            Require(capacity + 2u <= MaxSurfaces, "the eviction set exceeds the test storage");
            for (std::uint32_t surface = reused; surface < capacity + 2u; ++surface) Load(*device, base, surface);
            for (std::uint32_t surface = 0; surface < capacity + 2u; ++surface) {
                const bool cached = StorageTexture::FindLive(base + surface * Stride, SurfaceBytes) != nullptr;
                Require(cached == (surface >= 2u), "surface " + std::to_string(surface) + (cached ? " is still cached" : " was evicted") + " after " + std::to_string(capacity + 2u) + " surfaces under a budget of " + std::to_string(capacity) + " of them");
            }
        } catch (...) {
            release();
            throw;
        }
        release();
        std::puts("storage cache budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
