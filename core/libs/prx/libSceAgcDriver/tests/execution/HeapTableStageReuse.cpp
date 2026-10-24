#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
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

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Textures = 4;
constexpr std::uint32_t Stride = 32;
constexpr std::size_t SurfaceBytes = 256;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SrtOffset = 4096;
constexpr std::size_t InputOffset = 8192;
constexpr std::size_t OutputOffset = 12288;
constexpr std::size_t HeapOffset = 65536;
constexpr std::size_t BlockBytes = (HeapOffset + std::size_t{Textures} * Stride + 65535u) & ~std::size_t{65535};
constexpr std::size_t SurfaceBlockBytes = (std::size_t{Textures} * SurfaceBytes + 65535u) & ~std::size_t{65535};

constexpr std::array<std::uint32_t, 25> Code{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u,
    0x34080082u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xbf8cc07fu,
    0xe0301000u, 0x80030504u, 0xf09c0f08u, 0x00450000u, 0xbf8c3f70u, 0x4a000105u,
    0xe0701000u, 0x80070004u, 0xbf810000u, 0u, 0u};

class GuestBlock {
public:
    GuestBlock(std::size_t bytes, bool gpu) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
        }
#endif
        Require(block != nullptr, "heap table stage reuse: cannot allocate a guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, gpu);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), bytes);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return reinterpret_cast<std::uint64_t>(block + offset); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }
    std::uint32_t* Words(std::size_t offset) const { return reinterpret_cast<std::uint32_t*>(block + offset); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t stride, std::uint32_t records) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

void Prepare(const GuestBlock& block, const GuestBlock& surfaces) {
    std::memcpy(block.Words(0), Code.data(), sizeof(Code));
    for (std::uint32_t texture = 0; texture < Textures; ++texture) {
        const auto offset = static_cast<std::size_t>(texture) * SurfaceBytes;
        std::memset(surfaces.Bytes(offset), static_cast<int>(texture & 0xffu), SurfaceBytes);
        const auto surface = surfaces.At(offset);
        const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(surface >> 8u), static_cast<std::uint32_t>((surface >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 1u | (7u << 14u), 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
        std::memcpy(block.Words(HeapOffset + static_cast<std::size_t>(texture) * Stride), words.data(), sizeof(words));
    }
    auto* srt = block.Words(SrtOffset);
    const auto heap = BufferDescriptor(block.At(HeapOffset), Stride, Textures);
    const auto input = BufferDescriptor(block.At(InputOffset), 0u, Lanes * 4u);
    const auto output = BufferDescriptor(block.At(OutputOffset), 0u, Lanes * 4u);
    std::copy(heap.begin(), heap.end(), srt);
    std::copy(input.begin(), input.end(), srt + 8);
    std::copy(output.begin(), output.end(), srt + 12);
    AgcDriver::GuestMemory::CollectWritesUncached(block.At(0), BlockBytes);
    AgcDriver::GuestMemory::CollectWritesUncached(surfaces.At(0), SurfaceBlockBytes);
}

ShaderRecompiler::RecompileResult Compile(const AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const auto srt = block.At(SrtOffset);
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srt), static_cast<std::uint32_t>(srt >> 32u)};
    const std::span<const std::uint32_t> code(block.Words(0), Code.size());
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {block.At(0), std::as_bytes(code)},
        {srt, std::as_bytes(std::span<const std::uint32_t>(block.Words(SrtOffset), 64))},
        {block.At(HeapOffset), std::as_bytes(std::span<const std::uint32_t>(block.Words(HeapOffset), static_cast<std::size_t>(Textures) * Stride / 4u))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, block.At(0), code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(Lanes),
        {0, 0, 0, 128}
    };
    return ShaderRecompiler::Recompile(request);
}

void RequireHeapTable(const ShaderRecompiler::RecompileResult& result) {
    const auto has = [&](ShaderRecompiler::DescriptorRole role) {
        return std::any_of(result.bindings.begin(), result.bindings.end(), [&](const auto& binding) { return binding.role == role; });
    };
    Require(result.gpuResolvedImages && result.dataAddressPush, "heap table stage reuse: the program is not in the heap's GPU-resolved form");
    Require(has(ShaderRecompiler::DescriptorRole::ImageTable) && has(ShaderRecompiler::DescriptorRole::ImageTableMap), "heap table stage reuse: the program does not bind an image table and its map");
}

std::uint32_t TableMapRoot(const ShaderResources& resources, const CompiledShader& shader) {
    namespace Abi = ShaderRecompiler::RuntimeAbi;
    std::array<std::byte, AgcDriver::Graphics::PipelinePushConstantBytes> bytes{};
    resources.PatchDataAddresses(bytes, std::span<const CompiledShader>(&shader, 1), nullptr);
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + (Abi::RootDword + Abi::RootStageDword(Abi::Stage::Main, Abi::RootStageField::TableMap)) * sizeof(std::uint32_t), sizeof(value));
    return value;
}

void RefreshAfterOriginalDestroyed(const AgcDriver::VulkanDevice& device, const GuestBlock& block, bool sameAddress) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const char* what = sameAddress ? "an equal-content program at the destroyed program's address" : "an equal-content program at another address";
    std::optional<ShaderRecompiler::RecompileResult> slot;
    slot.emplace(Compile(device, block));
    RequireHeapTable(*slot);
    const auto* original = &*slot;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::BindlessHeap::SpanMark lead;
    static_cast<void>(AgcDriver::Graphics::BindlessHeap::For(context).Allocate(64, lead));
    const CompiledShader built{ShaderStage::Compute, original, 0};
    auto resources = std::make_shared<ShaderResources>(context, built);
    Require(resources->HeapStage(), "heap table stage reuse: the set was not built on the heap");
    const auto builtMap = TableMapRoot(*resources, built);
    Require(builtMap != 0u, "heap table stage reuse: the build left the table map unset");
    const auto bindingBytes = slot->bindings.capacity() * sizeof(ShaderRecompiler::DescriptorBinding);
    std::optional<ShaderRecompiler::RecompileResult> other;
    std::vector<std::unique_ptr<std::byte[]>> squatters;
    if (sameAddress) {
        slot.reset();
        for (int i = 0; i < 16; ++i) {
            squatters.push_back(std::make_unique<std::byte[]>(bindingBytes));
            std::memset(squatters.back().get(), 0xff, bindingBytes);
        }
        slot.emplace(Compile(device, block));
        Require(&*slot == original, "heap table stage reuse: the reused slot moved");
    } else {
        other.emplace(Compile(device, block));
        slot.reset();
    }
    const auto& current = sameAddress ? *slot : *other;
    RequireHeapTable(current);
    const CompiledShader shader{ShaderStage::Compute, &current, 0};
    resources->RefreshHeapStage(std::span<const CompiledShader>(&shader, 1));
    const auto refreshed = TableMapRoot(*resources, shader);
    Require(refreshed != 0u, std::string("heap table stage reuse: refreshing for ") + what + " left the table map unset");
    Require(refreshed == builtMap, std::string("heap table stage reuse: refreshing for ") + what + " wrote a table map that differs from the build's");
    std::printf("the heap table map refreshed for %s\n", what);
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_GPU_HEAP", "1");
    _putenv_s("APS5_VERIFY_GPU_HEAP", "1");
#else
    setenv("APS5_GPU_HEAP", "1", 1);
    setenv("APS5_VERIFY_GPU_HEAP", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::Graphics::DescriptorHeapEnabled()) {
            std::cout << "skipped, the device lacks VK_EXT_descriptor_heap, or the maintenance5, slot room and push data the GPU heap needs with it\n";
            return VulkanTestSkipped;
        }
        GuestBlock block(BlockBytes, true);
        GuestBlock surfaces(SurfaceBlockBytes, false);
        Prepare(block, surfaces);
        RefreshAfterOriginalDestroyed(*device, block, false);
        RefreshAfterOriginalDestroyed(*device, block, true);
        std::puts("heap table stage reuse tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
