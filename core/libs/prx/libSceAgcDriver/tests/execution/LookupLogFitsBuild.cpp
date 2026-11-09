#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>

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
constexpr std::uint32_t Textures = 1500;
constexpr std::size_t LogBound = 1024;
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
        Require(block != nullptr, "lookup log: cannot allocate a guest block");
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
        device.Target(),
        {0, 0, 0, 128}
    };
    return ShaderRecompiler::Recompile(request);
}

ShaderResources::ProofReport Prove(ShaderResources& resources, const CompiledShader& shader, bool& current) {
    ShaderResources::ProofReport report;
    current = resources.Revalidate(shader, &report);
    return report;
}

std::string Path(const ShaderResources::ProofReport& report) {
    switch (report.path) {
        case ShaderResources::ProofPath::Fast: return "fast";
        case ShaderResources::ProofPath::OwnRefreshed: return "own refresh";
        default: return "full walk";
    }
}

void RunTests(const AgcDriver::VulkanDevice& device) {
    GuestBlock block(BlockBytes, true);
    GuestBlock surfaces(SurfaceBlockBytes, false);
    Prepare(block, surfaces);
    const auto result = Compile(device, block);
    const auto table = std::find_if(result.bindings.begin(), result.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    Require(table != result.bindings.end() && table->count == Textures, "lookup log: the program does not bind an image table of " + std::to_string(Textures) + " textures");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const CompiledShader shader{ShaderStage::Compute, &result, 0};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto resources = std::make_shared<ShaderResources>(context, shader);
    Require(resources->Reusable() && resources->SampledTextures().size() == Textures, "lookup log: the set over " + std::to_string(Textures) + " textures is not reusable");
    const auto capacity = ShaderResources::LookupLogCapacity();
    Require(capacity <= LogBound, "lookup log: the thread's log kept a capacity of " + std::to_string(capacity) + " records after a build of " + std::to_string(Textures) + " textures, over the bound of " + std::to_string(LogBound));
    std::printf("the thread's log capacity is %zu records after the build\n", capacity);
    bool current = false;
    auto report = Prove(*resources, shader, current);
    Require(current && report.path == ShaderResources::ProofPath::Fast, "lookup log: a set over " + std::to_string(Textures) + " textures took the " + Path(report) + " with its memory unchanged after the build");
    std::puts("unchanged memory proved on the fast path after the build");
    const auto target = static_cast<std::size_t>(Textures / 3u) * SurfaceBytes;
    auto* first = reinterpret_cast<volatile std::uint8_t*>(surfaces.Bytes(target));
    const std::uint8_t saved = *first;
    *first = saved;
    report = Prove(*resources, shader, current);
    Require(report.path == ShaderResources::ProofPath::Full, "lookup log: a store over texture " + std::to_string(Textures / 3u) + " took the " + Path(report) + ", not the full walk");
    Require(ShaderResources::LookupLogCapacity() <= LogBound, "lookup log: the thread's log kept a capacity of " + std::to_string(ShaderResources::LookupLogCapacity()) + " records after a full walk over " + std::to_string(Textures) + " textures");
    if (current) {
        report = Prove(*resources, shader, current);
        Require(current && report.path == ShaderResources::ProofPath::Fast, "lookup log: a set over " + std::to_string(Textures) + " textures took the " + Path(report) + " with its memory unchanged after a full walk");
        std::puts("unchanged memory proved on the fast path after a full walk");
    }
    *first = static_cast<std::uint8_t>(saved ^ 0xffu);
    report = Prove(*resources, shader, current);
    Require(report.path == ShaderResources::ProofPath::Full && !current, "lookup log: a set whose texture " + std::to_string(Textures / 3u) + " changed was proved current on the " + Path(report));
    std::puts("a changed texture fails the proof");
}

}

int main() {
#ifdef _WIN32
    if (std::getenv("APS5_TEMPLATE_AUDIT_MS") == nullptr) _putenv_s("APS5_TEMPLATE_AUDIT_MS", "0");
#else
    setenv("APS5_TEMPLATE_AUDIT_MS", "0", 0);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunTests(*device);
        std::puts("lookup log fits build tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
