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
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
alignas(4096) std::array<std::uint32_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LoadCode{
    0x7e040300, 0x7e060280, 0xf0000108, 0x00020102, 0xbf8c3f70, 0xf0200108, 0x00020102, 0xbf810000,
};

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseLevel, std::uint32_t lastLevel, std::uint32_t maxMip) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (baseLevel << 12u) | (lastLevel << 16u) | (Type2D << 28u),
        0u,
        maxMip << 4u,
        0u,
        0u,
    };
}

bool RunStore(AgcDriver::VulkanDevice& device, std::uint32_t baseLevel) {
    Texels.fill(Untouched);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(baseLevel, baseLevel, 1u);
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
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::FlushCachedTextures(device.Device());
    }
    device.WaitIdle();
    return std::any_of(Texels.begin(), Texels.end(), [](std::uint32_t texel) { return texel != Untouched; });
}

void RunLoad(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(2u, 2u, 1u);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(LoadCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    try {
        static_cast<void>(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find("past its surface's last mip level") != std::string_view::npos, std::string("load from an empty view failed for another reason: ") + error.what());
        return;
    }
    Require(false, "load from an image view past the surface's last mip level compiled");
}

void RegisterTexels(bool add) {
    auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
    if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, Texels.data(), sizeof(Texels), true, true);
    else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, Texels.data());
    GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RegisterTexels(true);
        Require(RunStore(*device, 1u), "store through the surface's last mip level did not reach the texels");
        Require(!RunStore(*device, 2u), "store through a view past the surface's last mip level changed the texels");
        RunLoad(*device);
        RegisterTexels(false);
        std::puts("empty mip view tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
