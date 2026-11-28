#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <thread>
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

std::shared_ptr<StorageTexture> Store(AgcDriver::VulkanDevice& device, std::uint64_t base, std::uint32_t surface, std::uint64_t stride, bool flush) {
    const auto address = base + surface * stride;
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
    if (flush) {
        StorageTexture::FlushPending(address, static_cast<std::size_t>(SurfaceBytes), nullptr, "test");
        device.WaitIdle();
    }
    return StorageTexture::FindLive(address, SurfaceBytes);
}

void SetVariable(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("cannot set ") + name);
#endif
}

void InUse(AgcDriver::VulkanDevice& device, std::uint64_t base, bool budget) {
    using AgcDriver::Graphics::Recorder;
    const auto cached = [&](std::uint32_t surface) { return StorageTexture::FindLive(base + surface * Stride, SurfaceBytes) != nullptr; };
    Recorder::CountPresent();
    std::vector<std::weak_ptr<StorageTexture>> images(Surfaces + 1u);
    for (std::uint32_t surface = 0; surface < Surfaces; ++surface) images[surface] = Store(device, base, surface, Stride, true);
    if (budget) {
        Require(!cached(0) && images[0].expired(), "the least recently used of " + std::to_string(Surfaces) + " surfaces of 1 MiB stayed cached under a 4 MiB budget because it was used in the current present");
        Require(cached(Surfaces - 1u), "the newest surface is not cached under a 4 MiB budget");
        return;
    }
    for (std::uint32_t surface = 0; surface < Surfaces; ++surface) {
        Require(cached(surface) && !images[surface].expired(), "surface " + std::to_string(surface) + " was evicted by a limit of " + std::to_string(Entries) + " entries in the present that used it");
    }
    Recorder::CountPresent();
    for (std::uint32_t surface = 0; surface < Surfaces; ++surface) {
        const auto again = Store(device, base, surface, Stride, true);
        Require(again != nullptr && again == images[surface].lock(), "surface " + std::to_string(surface) + " was made again one present after its last use, with " + std::to_string(Surfaces) + " surfaces in use under a limit of " + std::to_string(Entries));
    }
    for (std::uint64_t present = 0; present <= StorageTexture::CapIdlePresents; ++present) Recorder::CountPresent();
    for (std::uint32_t surface = 2; surface < Surfaces; ++surface) {
        const auto again = Store(device, base, surface, Stride, true);
        Require(again != nullptr && again == images[surface].lock(), "surface " + std::to_string(surface) + " was made again after idle presents although no new surface arrived");
    }
    images[Surfaces] = Store(device, base, Surfaces, Stride, true);
    for (std::uint32_t surface = 0; surface <= Surfaces; ++surface) {
        const bool idle = surface < 2u;
        Require(cached(surface) != idle, "surface " + std::to_string(surface) + (idle ? " stayed cached over the limit although it was not used for more than the idle presents" : " was evicted by the entry limit although it was used in the current present"));
        Require(images[surface].expired() == idle, "surface " + std::to_string(surface) + (idle ? " was evicted but its image is still alive" : " is cached but its weak image expired"));
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        const bool unlimited = mode == "--unlimited";
        const bool profile = mode == "--profile";
        const bool inUseBudget = mode == "--in-use-budget";
        const bool inUse = inUseBudget || mode == "--in-use";
        const std::string limit = unlimited ? "no entry limit" : "a limit of " + std::to_string(Entries) + " entries";
        const auto stride = profile ? SurfaceBytes : Stride;
        SetVariable("APS5_STORAGE_CACHE_ENTRIES", unlimited ? "0" : "4");
        SetVariable("APS5_TEXTURE_CACHE_MIB", inUseBudget ? "4" : "1024");
        if (profile) SetVariable("APS5_PROFILE_DRAW", "1");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> storage((SurfaceBytes + (Surfaces + 1u) * stride + Stride) / 4u, 0u);
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + Stride - 1u) & ~std::uintptr_t{Stride - 1u});
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
        const auto blockBytes = static_cast<std::size_t>(SurfaceBytes + Surfaces * stride);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, blockBytes, true, true, true);
        }
        const auto release = [&] {
            AgcDriver::Graphics::ClearCachedTextures(device->Device());
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
        };
        if (inUse) {
            try {
                InUse(*device, base, inUseBudget);
            } catch (...) {
                release();
                throw;
            }
            release();
            std::puts(inUseBudget ? "storage cache entry limit in-use budget tests passed" : "storage cache entry limit in-use tests passed");
            return 0;
        }
        try {
            std::vector<std::weak_ptr<StorageTexture>> images(Surfaces);
            for (std::uint32_t surface = 0; surface < Entries; ++surface) {
                images[surface] = Store(*device, base, surface, stride, !profile);
                Require(!images[surface].expired(), "surface " + std::to_string(surface) + " has no cached storage image");
            }
            for (std::uint32_t surface = 0; surface < Entries; ++surface) {
                const auto again = Store(*device, base, surface, stride, !profile);
                Require(again != nullptr && again == images[surface].lock(), "surface " + std::to_string(surface) + " was made again while the cache held " + std::to_string(Entries) + " of its " + std::to_string(Entries) + " entries");
            }
            for (std::uint32_t surface = Entries; surface < Surfaces; ++surface) images[surface] = Store(*device, base, surface, stride, !profile);
            for (std::uint32_t surface = 0; surface < Surfaces; ++surface) {
                const bool cached = StorageTexture::FindLive(base + surface * stride, SurfaceBytes) != nullptr;
                Require(cached == (unlimited || surface >= Surfaces - Entries), "surface " + std::to_string(surface) + (cached ? " is still cached" : " was evicted") + " after " + std::to_string(Surfaces) + " small surfaces under " + limit + " and a 1024 MiB budget");
                Require(cached == !images[surface].expired(), "surface " + std::to_string(surface) + (cached ? " is cached but its weak image expired" : " was evicted but its image is still alive"));
            }
            if (profile) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10500));
                StorageTexture::FlushPending(base + (Surfaces - 1u) * stride, static_cast<std::size_t>(SurfaceBytes), nullptr, "test");
                device->WaitIdle();
            }
        } catch (...) {
            release();
            throw;
        }
        release();
        std::puts(unlimited ? "storage cache without an entry limit tests passed" : profile ? "storage cache entry limit counter tests passed" : "storage cache entry limit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "storage cache entry limit test failed: " << error.what() << '\n';
        return 1;
    }
}
