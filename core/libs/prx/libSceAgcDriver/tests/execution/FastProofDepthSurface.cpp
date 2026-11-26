#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::CompiledShader;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::ShaderResources;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Swizzle64KbZX = 24;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t Side = 128;
constexpr std::size_t BlockBytes = 2u << 20u;

alignas(256) constexpr std::array<std::uint32_t, 12> SampleCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf09c0f08, 0x00820402, 0xbf8c3f70,
    0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "fast proof depth surface: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(AddressOf(block), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (Swizzle64KbZX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::vector<std::uint32_t> SampleUserData(const std::uint8_t* texels, std::uint8_t* buffers) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(buffers, Threads * 12u);
    const auto output = BufferDescriptor(buffers + 4096u, Threads * 16u);
    const auto texture = TextureDescriptor(texels);
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    return userData;
}

void Run(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    GuestBlock buffers;
    auto* texels = block.Data();
    const auto surface = AddressOf(texels);
    const auto words = TextureDescriptor(texels);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto bytes = static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(resource).guestBytes);
    const float quarter = 0.25f;
    for (std::size_t offset = 0; offset + sizeof(quarter) <= bytes; offset += sizeof(quarter)) std::memcpy(texels + offset, &quarter, sizeof(quarter));
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    AgcDriver::GuestMemory::CollectWritesUncached(surface, bytes);
    const auto userData = SampleUserData(texels, buffers.Data());
    const auto program = Compile(device, SampleCode, userData);
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    auto resources = std::make_shared<ShaderResources>(context, shader);
    Require(resources->Reusable() && resources->SampledTextures().size() == 1, "the set is not reusable over one texture");
    const auto held = resources->SampledTextures().front();
    Require(!AgcDriver::Graphics::DepthSurfaceAt(surface), "a depth surface holds the surface before the test registers one");
    for (int proof = 0; proof < 2; ++proof) {
        Require(resources->Revalidate(shader), "the set was not proved current before a depth surface held its texture");
        Require(resources->SampledTextures().front() == held, "the proof replaced the texture before a depth surface held it");
    }
    AgcDriver::Graphics::DepthTarget target{surface, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    target.depthSwizzle = AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX);
    Require(AgcDriver::Graphics::DepthSurfaceView(context, target, false, false) != VK_NULL_HANDLE, "the depth target got no surface");
    Require(AgcDriver::Graphics::DepthSurfaceAt(surface), "no depth surface holds the texture's address after its target was bound");
    Require(AgcDriver::Graphics::FlushDepthPlanes(surface, bytes).empty(), "a depth surface no draw wrote has a stale plane");
    const auto lookup = AgcDriver::Graphics::DepthSurfaceTexture(context, words, resource, {});
    Require(lookup != nullptr && lookup != held, "the lookup does not answer with the depth surface");
    ShaderResources::ProofReport report;
    const bool current = resources->Revalidate(shader, &report);
    Require(!current, "a set holding a texture of memory a depth surface now holds was proved current");
    Require(report.path == ShaderResources::ProofPath::Full && report.failure == ShaderResources::ProofFailure::Other, "the proof did not fail on the depth surface");
    resources.reset();
    AgcDriver::Graphics::RetireDepthSurfaces(device.Device(), surface, bytes);
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        std::puts("fast proof depth surface tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
