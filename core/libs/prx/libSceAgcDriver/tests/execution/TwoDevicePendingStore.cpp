#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 0x60000;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300u, 0x7e060280u, 0x7e0202ffu, Stored, 0xf0200108u, 0x00020102u, 0xbf810000u,
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
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

void Store(AgcDriver::VulkanDevice& device, const std::uint32_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

void FlushAll(const char* reason) {
    std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::StorageTexture::FlushAllPending(reason);
}

void Label(AgcDriver::VulkanDevice& device, const std::uint32_t* label) {
    {
        std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        const std::uint32_t value = 1;
        static_cast<void>(device.WriteLabelOnGpu(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(label)), std::as_bytes(std::span(&value, 1)), 1, 0, false));
        device.SubmitRecorded(false);
    }
    device.WaitIdle();
}

}

int main(int argc, char** argv) {
    try {
        const bool label = argc > 1 && std::strcmp(argv[1], "--label") == 0;
        Require(!label || AgcDriver::Graphics::StorageTexture::StoreAtLabelRequested(std::getenv("APS5_STORE_AT_LABEL")), "two devices: --label needs APS5_STORE_AT_LABEL=1");
        const auto first = OpenVulkanTestDevice();
        if (!first) return VulkanTestSkipped;
        std::vector<std::uint32_t> storage((BlockBytes + 0x10000u) / 4u);
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        std::fill(texels, texels + BlockBytes / 4u, Untouched);
        std::array<std::uint32_t, 4> labelWords{};
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, BlockBytes, true, true, true);
        }
        const auto bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes;
        const volatile std::uint32_t* cpu = texels;
        const auto stored = [&] {
            for (std::size_t index = 0; index < bytes / 4u; ++index) {
                if (cpu[index] == Stored) return true;
            }
            return false;
        };

        Store(*first, texels);
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "two devices: the first device's image store left no pending results");
        {
            const auto second = OpenVulkanTestDevice();
            Require(second != nullptr, "two devices: the second device did not open");
            Require(AgcDriver::Graphics::Recorder::Active() == AgcDriver::VulkanDeviceTestAccess::ContextOf(*second).recorder, "two devices: the second device did not activate its recorder");
            second->StoreIdleResults(false);
            second->WaitIdle();
            Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "two devices: the second device's idle store took the first device's pending results");
            Require(!stored(), "two devices: the first device's results reached guest memory through the second device's idle store");
            if (label) {
                Label(*second, labelWords.data());
                Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "two devices: the second device's label store took the first device's pending results");
                Require(!stored(), "two devices: the first device's results reached guest memory through the second device's label store");
                Label(*first, labelWords.data());
                Require(!AgcDriver::Graphics::StorageTexture::AnyPending(), "two devices: the first device's label store left its results pending");
                Require(stored(), "two devices: the first device's label store did not bring its results to guest memory");
            } else {
                FlushAll("memory access");
                first->WaitIdle();
                Require(!AgcDriver::Graphics::StorageTexture::AnyPending(), "two devices: a flush with the second device active left the first device's results pending");
                Require(stored(), "two devices: a flush with the second device active did not bring the first device's results to guest memory");
            }
            second->WaitIdle();
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
        }
        std::puts("two device pending store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
