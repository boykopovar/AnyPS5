#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
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
#include <cmath>
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
using AgcDriver::Graphics::RuntimeSwitch;
using AgcDriver::Graphics::ShaderResources;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;
namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::size_t BlockBytes = 2u << 20u;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{
    0x7e020280, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

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
        Require(block != nullptr, "own refresh view alias: cannot allocate a guest block");
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

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, std::uint32_t format) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
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

void Store(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::uint32_t value) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, Format32Float);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = value;
    const auto result = Compile(device, StoreCode, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
    device.WaitIdle();
}

std::vector<std::uint32_t> SampleUserData(const std::uint8_t* texels, std::uint8_t* buffers) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(buffers, Threads * 12u);
    const auto output = BufferDescriptor(buffers + 4096u, Threads * 16u);
    const auto texture = TextureDescriptor(texels, Format8888UNorm);
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    return userData;
}

std::string Path(const ShaderResources::ProofReport& report) {
    switch (report.path) {
        case ShaderResources::ProofPath::Fast: return "fast path";
        case ShaderResources::ProofPath::OwnRefreshed: return "own-object refresh";
        default: return "full walk";
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void RequireSampled(const std::uint8_t* buffers, std::uint32_t stored, const std::string& what) {
    std::array<float, 4> sampled{};
    std::memcpy(sampled.data(), buffers + 4096u, sizeof(sampled));
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        const auto expected = static_cast<float>((stored >> (channel * 8u)) & 0xffu) / 255.0f;
        Require(std::fabs(sampled[channel] - expected) < 1e-4f, what + ": channel " + std::to_string(channel) + " reads " + std::to_string(sampled[channel]) + ", expected " + std::to_string(expected) + " from the R32 float store of " + Hex(stored));
    }
}

bool Run(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    GuestBlock buffers;
    auto* texels = block.Data();
    const auto surface = AddressOf(texels);
    const auto bytes = static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels, Format8888UNorm))).guestBytes);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Store(device, texels, 0x11223344u);
    StorageTexture::FlushPending(surface, bytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::GuestMemory::CollectWritesUncached(surface, bytes);
    if (!AgcDriver::Graphics::HostImportCovers(context, surface, bytes)) {
        std::puts("skipped, the device does not import guest memory");
        return false;
    }
    const bool compared = !AgcDriver::GuestMemory::Watched(surface, bytes);
    std::printf("the surface is %s\n", compared ? "compared, not write-watched" : "write-watched");
    Require(StorageTexture::FindPending(surface, bytes) == nullptr, "the first R32 float store stayed pending after its flush");
    const auto userData = SampleUserData(texels, buffers.Data());
    const auto program = Compile(device, SampleCode, userData);
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    auto resources = std::make_shared<ShaderResources>(context, shader);
    Require(resources->Reusable() && resources->SampledTextures().size() == 1, "the set is not reusable over one texture");
    const auto view = resources->SampledTextures().front();
    const auto source = view->SharedStorageSource();
    Require(source != nullptr && source->Descriptor().format == Format8888UNorm, "the 8888 unorm view is not a view of its own storage image");
    for (const std::uint32_t value : {0x3c003c00u, 0x41804180u}) {
        Store(device, texels, value);
        const auto alias = StorageTexture::FindPending(surface, bytes);
        Require(alias != nullptr && alias != source && alias->Descriptor().format == Format32Float, "the R32 float store left no pending image of its own over the surface");
        Require(source->IsPendingAlias(*alias), "the pending R32 float image is not the 8888 unorm image's alias");
        ShaderResources::ProofReport report;
        Require(resources->Revalidate(shader, &report), "the set over the view was not proved current over the pending alias");
        if (!compared) Require(report.path == ShaderResources::ProofPath::OwnRefreshed, "a set whose view refreshed its own image through a pending R32 float alias took the " + Path(report) + ", not the own-object refresh");
        Require(resources->SampledTextures().front() == view, "the proof replaced the view");
        const auto rebuilt = std::make_shared<ShaderResources>(context, shader);
        Require(rebuilt->SampledTextures().size() == 1 && rebuilt->SampledTextures().front() == view, "a build over the pending alias replaced the cached view");
        std::memset(buffers.Data() + 4096u, 0xff, Threads * 16u);
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
        device.WaitIdle();
        RequireSampled(buffers.Data(), value, "the dispatch through the kept view");
        if (!compared) Require(StorageTexture::FindPending(surface, bytes) == alias, "the view's refresh wrote the alias's results back instead of taking them on the device");
    }
    RuntimeSwitches::Assign(RuntimeSwitch::NoAliasViews, true);
    Store(device, texels, 0x7f107f10u);
    const auto replaced = std::make_shared<ShaderResources>(context, shader);
    RuntimeSwitches::Assign(RuntimeSwitch::NoAliasViews, false);
    Require(replaced->SampledTextures().size() == 1 && replaced->SampledTextures().front() != view, "APS5_NO_ALIAS_VIEWS kept the view over the pending alias");
    StorageTexture::FlushPending(surface, bytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    return true;
}

}

int main() {
#ifdef _WIN32
    if (std::getenv("APS5_TEMPLATE_AUDIT_MS") == nullptr) _putenv_s("APS5_TEMPLATE_AUDIT_MS", "0");
    if (std::getenv("APS5_VERIFY_PROOFS") == nullptr) _putenv_s("APS5_VERIFY_PROOFS", "1");
#else
    setenv("APS5_TEMPLATE_AUDIT_MS", "0", 0);
    setenv("APS5_VERIFY_PROOFS", "1", 0);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Run(*device)) return VulkanTestSkipped;
        std::puts("own refresh view alias tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
