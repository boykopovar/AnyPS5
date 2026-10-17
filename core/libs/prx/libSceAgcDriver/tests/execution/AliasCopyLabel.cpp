#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 0x40000;
constexpr std::size_t LabelOffset = 2 * SurfaceBytes;
constexpr std::size_t BlockBytes = LabelOffset + 0x10000;
constexpr std::uint32_t LabelValue = 0x600dcafeu;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, 0x12345678u, 0xf0200108, 0x00020102, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels) {
    const auto address = AddressOf(texels);
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

void Store(AgcDriver::VulkanDevice& device, const std::uint8_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(StoreCode);
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
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint8_t> storage(BlockBytes + 0x10000u);
        auto* block = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        auto* source = block;
        auto* destination = block + SurfaceBytes;
        auto* label = reinterpret_cast<volatile std::uint32_t*>(block + LabelOffset);
        for (std::size_t at = 0; at < SurfaceBytes; ++at) source[at] = static_cast<std::uint8_t>(0x31u + at % 67u);
        std::memset(destination, 0xa5, SurfaceBytes);
        *label = 0;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, BlockBytes, true, true, true);
        }
        const auto surfaceBytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(source))).guestBytes;
        Require(surfaceBytes == SurfaceBytes, "the 256x256 32-bit surface must cover exactly its 256 KiB block");

        Store(*device, source);
        Require(StorageTexture::FindPending(AddressOf(source), SurfaceBytes) != nullptr, "the compute store left no pending image over the source");

        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto outcome = device->CopyBuffer(AddressOf(destination), AddressOf(source), SurfaceBytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
            if (outcome.path != 3) {
                std::printf("skipped, the copy took path %d instead of the image copy\n", outcome.path);
                return VulkanTestSkipped;
            }
            const std::uint32_t value = LabelValue;
            const int reason = device->WriteLabelOnGpu(AddressOf(const_cast<std::uint32_t*>(label)), std::as_bytes(std::span(&value, 1)), 1, 0);
            if (reason != 0) {
                std::printf("skipped, the label was not recorded on the GPU (reason %d)\n", reason);
                return VulkanTestSkipped;
            }
            device->SubmitRecorded(true);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (*label != LabelValue) {
            Require(std::chrono::steady_clock::now() < deadline, "the label after the image copy never landed");
            std::this_thread::yield();
        }
        std::vector<std::uint8_t> seen(destination, destination + SurfaceBytes);

        std::vector<std::byte> expected(SurfaceBytes);
        AgcDriver::GuestMemory::Read(AddressOf(source), expected);
        std::size_t differences = 0;
        for (std::size_t at = 0; at < SurfaceBytes; ++at) differences += static_cast<std::byte>(seen[at]) != expected[at];
        Require(differences == 0, "a CPU read after the label that followed the image copy found " + std::to_string(differences) + " of " + std::to_string(SurfaceBytes) + " destination bytes still unwritten");

        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::FlushCachedTextures(device->Device());
        }
        device->WaitIdle();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
        std::puts("alias copy label tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
