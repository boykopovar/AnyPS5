#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <mutex>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Layers = 4;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t LayerBytes = Side * Side * 4u;
constexpr std::size_t SurfaceBytes = LayerBytes * Layers;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0xf0201128, 0x00011714, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, SurfaceBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(LayerBytes, SurfaceBytes));
#endif
        Require(block != nullptr, "storage array layers: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Value(std::uint32_t round, std::uint32_t layer, std::uint32_t tid) {
    return (tid + 1u) * 0x9e3779b1u ^ (layer + 1u) * 0x85ebca6bu ^ (round + 1u) * 0xc2b2ae35u;
}

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> ArrayDescriptor(std::uint64_t address, std::uint32_t baseLayer, std::uint32_t lastLayer) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        IdentitySwizzle | (Type2DArray << 28u),
        lastLayer | (baseLayer << 16u),
        0u,
        0u,
        0u,
    };
}

void StoreLayer(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t round, std::uint32_t layer) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> words{tid, layer, 0u, Value(round, layer, tid)};
        std::copy(words.begin(), words.end(), Input.begin() + tid * 4u);
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = ArrayDescriptor(surface, layer, layer);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
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

std::uint32_t Initial(std::size_t word) {
    return static_cast<std::uint32_t>(word) * 0x01000193u + 0x5au;
}

void Check(const std::uint8_t* texels, std::uint32_t rounds) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(ArrayDescriptor(AddressOf(texels), 0u, Layers - 1u)));
    Require(geometry.guestBytes == SurfaceBytes, "storage array layers: the array does not span its four layers");
    const auto& mip = geometry.mips.at(0);
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t y = 0; y < Layers; ++y) {
            for (std::uint32_t x = 0; x < Threads; ++x) {
                const auto offset = geometry.GuestLayerOffset(layer) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u;
                std::uint32_t actual = 0;
                std::memcpy(&actual, texels + offset, 4u);
                const auto expected = y == layer ? Value(rounds - 1u, layer, x) : Initial(offset / 4u);
                Require(actual == expected, "storage array layers: layer " + std::to_string(layer) + " texel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + Hex(actual) + ", expected " + Hex(expected));
            }
        }
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    const auto surface = AddressOf(texels);
    for (std::size_t word = 0; word < SurfaceBytes / 4u; ++word) {
        const auto value = Initial(word);
        std::memcpy(texels + word * 4u, &value, 4u);
    }
    std::shared_ptr<StorageTexture> image;
    constexpr std::uint32_t Rounds = 3;
    for (std::uint32_t round = 0; round < Rounds; ++round) {
        for (std::uint32_t layer = 0; layer < Layers; ++layer) {
            StoreLayer(device, surface, round, layer);
            if (round == 0) continue;
            Require(StorageTexture::FindPending(surface, SurfaceBytes) == image, "storage array layers: a store to layer " + std::to_string(layer) + " in round " + std::to_string(round) + " did not reuse the array image the first round built");
        }
        if (round != 0) continue;
        image = StorageTexture::FindPending(surface, SurfaceBytes);
        Require(image != nullptr, "storage array layers: per-layer stores left no pending image of the whole array");
        Require(image->Descriptor().baseArray == 0 && image->Descriptor().depthOrLastArray == Layers - 1u, "storage array layers: the pending image does not hold every layer of the array");
    }
    image.reset();
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    Check(texels, Rounds);
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

void ExtendMipChain(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    AgcDriver::Graphics::GuestTextureResource chain{};
    chain.baseAddress = AddressOf(texels);
    chain.width = Side;
    chain.height = Side;
    chain.mipCount = 2;
    chain.allocatedMipCount = 1;
    chain.tileMode = AgcDriver::Graphics::TextureTileMode::kLinear;
    chain.dimension = AgcDriver::Graphics::TextureDimension::k2D;
    chain.format = Format32UInt;
    chain.dstSelX = 4;
    chain.dstSelY = 5;
    chain.dstSelZ = 6;
    chain.dstSelW = 7;
    Require(AgcDriver::Graphics::DescribeSurface(chain).guestBytes <= SurfaceBytes, "storage array layers: the two-level chain does not fit its guest block");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(AgcDriver::Graphics::CachedStorageLayers(context, chain, 0, 1) != nullptr, "storage array layers: the two-level chain has no storage image");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        Run(*device, block.Data());
        GuestBlock chain;
        ExtendMipChain(*device, chain.Data());
        GuestBlock extended;
        Run(*device, extended.Data());
        std::puts("storage array layers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
