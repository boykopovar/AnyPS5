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
constexpr std::uint32_t Entries = 4;
constexpr std::uint32_t Surfaces = Entries + 2;

alignas(256) constexpr std::array<std::uint32_t, 8> StoreCode{
    0x7e020280, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

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

std::shared_ptr<StorageTexture> Store(AgcDriver::VulkanDevice& device, std::uint64_t base, std::uint32_t surface) {
    const auto address = base + surface * Stride;
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(address);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = true;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    StorageTexture::FlushPending(address, static_cast<std::size_t>(SurfaceBytes), nullptr, "test");
    device.WaitIdle();
    return StorageTexture::FindLive(address, SurfaceBytes);
}

void SetVariable(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("cannot set ") + name);
#endif
}

}

int main() {
    try {
        SetVariable("APS5_STORAGE_CACHE_ENTRIES", "4");
        SetVariable("APS5_TEXTURE_CACHE_MIB", "1024");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> storage((SurfaceBytes + Surfaces * Stride + Stride) / 4u, 0u);
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + Stride - 1u) & ~std::uintptr_t{Stride - 1u});
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
        const auto blockBytes = static_cast<std::size_t>(SurfaceBytes + (Surfaces - 1u) * Stride);
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
            std::vector<std::weak_ptr<StorageTexture>> images(Surfaces);
            for (std::uint32_t surface = 0; surface < Entries; ++surface) {
                images[surface] = Store(*device, base, surface);
                Require(!images[surface].expired(), "surface " + std::to_string(surface) + " has no cached storage image");
            }
            for (std::uint32_t surface = 0; surface < Entries; ++surface) {
                const auto again = Store(*device, base, surface);
                Require(again != nullptr && again == images[surface].lock(), "surface " + std::to_string(surface) + " was made again while the cache held " + std::to_string(Entries) + " of its " + std::to_string(Entries) + " entries");
            }
            for (std::uint32_t surface = Entries; surface < Surfaces; ++surface) images[surface] = Store(*device, base, surface);
            for (std::uint32_t surface = 0; surface < Surfaces; ++surface) {
                const bool cached = StorageTexture::FindLive(base + surface * Stride, SurfaceBytes) != nullptr;
                Require(cached == (surface >= Surfaces - Entries), "surface " + std::to_string(surface) + (cached ? " is still cached" : " was evicted") + " after " + std::to_string(Surfaces) + " small surfaces under a limit of " + std::to_string(Entries) + " entries and a 1024 MiB budget");
                Require(cached == !images[surface].expired(), "surface " + std::to_string(surface) + (cached ? " is cached but its weak image expired" : " was evicted but its image is still alive"));
            }
        } catch (...) {
            release();
            throw;
        }
        release();
        std::puts("storage cache entry limit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
