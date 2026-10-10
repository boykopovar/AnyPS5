#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t MaxMip = 2;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 0x60000;
constexpr std::size_t TailBlockBytes = 0x10000;
constexpr std::uint32_t StoredPast = 0x12345678u;
constexpr std::uint32_t StoredLast = 0x9abcdef0u;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::array<std::size_t, Threads> PastOffsets{0x4800, 0x4804, 0x4808, 0x480c, 0x4880, 0x4884, 0x4888, 0x488c};
constexpr std::array<std::size_t, Threads> LastOffsets{0x8400, 0x8404, 0x8408, 0x840c, 0x8480, 0x8484, 0x8488, 0x848c};

alignas(256) constexpr std::array<std::uint32_t, 7> StorePastCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, StoredPast, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreLastCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, StoredLast, 0xf0200108, 0x00020102, 0xbf810000,
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t level) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (level << 12u) | (level << 16u) | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        MaxMip << 4u,
        0u,
        0u,
    };
}

void Store(AgcDriver::VulkanDevice& device, const std::uint32_t* texels, std::uint32_t level, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, level);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
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
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::FlushCachedTextures(device.Device());
    }
    device.WaitIdle();
}

void Check(const std::uint32_t* texels, bool last, const std::string& step) {
    for (const auto offset : PastOffsets) Testing::Require(texels[offset / 4u] == StoredPast, step + ": byte offset " + std::to_string(offset) + " does not hold the store through level 3, past MAX_MIP 2, at its addrlib tail slot");
    for (const auto offset : LastOffsets) Testing::Require(texels[offset / 4u] == (last ? StoredLast : Untouched), step + ": byte offset " + std::to_string(offset) + " of level 2 holds an unexpected value");
    for (auto index = TailBlockBytes / 4u; index < SurfaceBytes / 4u; ++index) Testing::Require(texels[index] == Untouched, step + ": byte offset " + std::to_string(index * 4u) + " of levels 0 and 1 changed");
}


class MippedSurface {
public:
    explicit MippedSurface(AgcDriver::VulkanDevice& device) : device(device), storage((SurfaceBytes + 0x10000u) / 4u) {
        texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
        GuestAllocations::Mutation mutation;
        mutation.Add(texels, SurfaceBytes, true, true);
    }

    MippedSurface(const MippedSurface&) = delete;
    MippedSurface& operator=(const MippedSurface&) = delete;

    ~MippedSurface() {
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
        GuestAllocations::Mutation mutation;
        mutation.Remove(texels);
    }

    std::uint32_t* Texels() const {
        return texels;
    }

private:
    AgcDriver::VulkanDevice& device;
    std::vector<std::uint32_t> storage;
    std::uint32_t* texels = nullptr;
};

const Testing::Case storePastLastMip{"ImageStore_ThroughLevelPastMaxMip_LandsInTheAddrlibTailSlot", [] {
    auto& device = SharedVulkanTestDevice();
    MippedSurface surface(device);
    Store(device, surface.Texels(), 3u, StorePastCode);
    Check(surface.Texels(), false, "store through level 3");
}};

const Testing::Case storeLastMipAfterPast{"ImageStore_ThroughLastLevelAfterPastLevel_KeepsBothStores", [] {
    auto& device = SharedVulkanTestDevice();
    MippedSurface surface(device);
    Store(device, surface.Texels(), 3u, StorePastCode);
    Store(device, surface.Texels(), 2u, StoreLastCode);
    Check(surface.Texels(), true, "store through level 2 after level 3");
}};

} // namespace
