#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
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
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Wide = 64;
constexpr std::uint32_t Narrow = 32;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t TextureBytes = Wide * Height * 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 48;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TexturesOffset = 65536;
constexpr std::size_t BlockBytes = TexturesOffset + 2 * 65536;
constexpr std::uint32_t Largest = 60;
constexpr std::uint32_t Written = 61;
constexpr std::uint32_t Depth = 62;

alignas(256) std::array<std::uint32_t, Records> Keys{};
alignas(256) std::array<std::uint32_t, Lanes * 4> Output{};
alignas(256) std::array<std::uint32_t, 32> Srt{};

alignas(256) constexpr std::array<std::uint32_t, 28> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e020d00u, 0x082802f7u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201108u, 0x0001141eu, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t width, std::uint32_t format = Format32Float) {
    const auto address = AddressOf(texels);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = AddressOf(base);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

class Unreachable {
public:
    Unreachable() {
#ifdef _WIN32
        memory = VirtualAlloc(nullptr, TextureBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
        void* mapped = mmap(nullptr, TextureBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memory = mapped == MAP_FAILED ? nullptr : mapped;
#endif
        Require(memory != nullptr, "image table variants: cannot reserve the unreachable surface");
    }

    ~Unreachable() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, TextureBytes);
#endif
    }

    const float* Texels() const { return static_cast<const float*>(memory); }

private:
    void* memory = nullptr;
};

class GuestBlock {
public:
    explicit GuestBlock(std::size_t registered = BlockBytes) {
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
        Require(block != nullptr, "image table variants: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, registered, true, true, true);
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

    std::uint32_t* Table() const { return reinterpret_cast<std::uint32_t*>(block); }
    float* Texels(std::uint32_t texture) const { return reinterpret_cast<float*>(block + TexturesOffset + static_cast<std::size_t>(texture) * TextureBytes); }

private:
    std::uint8_t* block = nullptr;
};

void SetRecord(const GuestBlock& table, std::uint32_t record, const float* texels, std::uint32_t width, std::uint32_t format = Format32Float) {
    auto* words = table.Table() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(PointClamp.begin(), PointClamp.end(), words);
    const auto image = Image(texels, width, format);
    std::copy(image.begin(), image.end(), words + 4);
}

void Fill(float* texels, float bias) {
    for (std::uint32_t texel = 0; texel < Wide * Height; ++texel) texels[texel] = bias + static_cast<float>(texel);
}

void FillTable(const GuestBlock& block) {
    for (std::uint32_t record = 0; record < Records; ++record) {
        Fill(block.Texels(record), 0.25f + static_cast<float>(record) * 100.0f);
        SetRecord(block, record, block.Texels(record), Wide);
    }
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const GuestBlock& table) {
    const auto srtAddress = AddressOf(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::span<const std::uint32_t> code(TableCode);
    const std::span<const std::uint32_t> words(table.Table(), Records * Stride / 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {AddressOf(code.data()), std::as_bytes(code)},
        {srtAddress, std::as_bytes(std::span(Srt))},
        {AddressOf(words.data()), std::as_bytes(words)},
        {AddressOf(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    const ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    auto result = ShaderRecompiler::Recompile(request);
    const auto binding = std::find_if(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& candidate) { return candidate.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    Require(binding != result.bindings.end() && binding->guestDescriptor.size() == static_cast<std::size_t>(Records) * 8u, "image table variants: the shader does not bind the whole table");
    return result;
}

void Store(AgcDriver::VulkanDevice& device, const float* texels, std::uint32_t width) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto image = Image(texels, width);
    std::copy(image.begin(), image.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{AddressOf(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(code.data()));
    device.WaitIdle();
}

std::vector<std::uint32_t> Dispatch(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::uint32_t record) {
    Keys[0] = record;
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(TableCode.data()));
    device.WaitIdle();
    std::vector<std::uint32_t> red(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) red[lane] = Output[lane * 4u];
    return red;
}

std::vector<std::uint32_t> Row(const float* texels, std::uint32_t width) {
    std::vector<std::uint32_t> row(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) row[lane] = std::bit_cast<std::uint32_t>(texels[lane * width / Lanes]);
    return row;
}

struct Variants {
    std::uint64_t surfaces;
    std::uint64_t substituted;
    std::uint64_t rejected;
};

Variants Since(const AgcDriver::Graphics::ImageTableReuseCounts& before) {
    const auto now = AgcDriver::Graphics::ImageTableReuses();
    return {now.variantSurfaces - before.variantSurfaces, now.variantSubstituted - before.variantSubstituted, now.variantRejected - before.variantRejected};
}

void Expect(const AgcDriver::Graphics::ImageTableReuseCounts& before, std::uint64_t surfaces, std::uint64_t substituted, std::uint64_t rejected, const std::string& what) {
    const auto step = Since(before);
    Require(step.surfaces == surfaces && step.substituted == substituted && step.rejected == rejected, "image table variants: " + what + ": " + std::to_string(step.surfaces) + " surfaces, " + std::to_string(step.substituted) + " substituted and " + std::to_string(step.rejected) + " rejected entries, expected " + std::to_string(surfaces) + ", " + std::to_string(substituted) + " and " + std::to_string(rejected));
}

void SetOutput() {
    const auto view = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(view.begin(), view.end(), Srt.begin() + 8);
}

void Prepare(const GuestBlock& table) {
    FillTable(table);
    for (std::uint32_t key = 0; key < Records; ++key) Keys[key] = key;
    const auto srt = Buffer(table.Table(), Records * Stride);
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    std::copy(srt.begin(), srt.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    SetOutput();
}

void RunLargest(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Largest), Wide);
    SetRecord(block, 1, block.Texels(Largest), Narrow);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1) == Row(block.Texels(Largest), Wide), "image table variants: with no write and no depth surface, the narrow variant did not borrow the largest one");
    Require(Dispatch(device, result, 0) == Row(block.Texels(Largest), Wide), "image table variants: the largest variant sampled the wrong texels");
    Require(Dispatch(device, result, 5) == Row(block.Texels(5), Wide), "image table variants: an entry outside the family sampled the wrong texels");
    Expect(before, 1, 3, 0, "three dispatches of two variants with no write and no depth surface");
}

void RunWritten(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Written), Wide);
    SetRecord(block, 1, block.Texels(Written), Narrow);
    const auto result = Compile(device, block);
    Store(device, block.Texels(Written), Narrow);
    std::vector<float> stored(Narrow);
    for (std::uint32_t x = 0; x < Narrow; ++x) stored[x] = -4.0f - static_cast<float>(x);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 0) == Row(stored.data(), Narrow), "image table variants: the wide variant did not borrow the narrow one written last");
    Require(Dispatch(device, result, 1) == Row(stored.data(), Narrow), "image table variants: the narrow variant written last sampled the wrong texels");
    Expect(before, 1, 2, 0, "two dispatches of two variants, the narrow one written last");
}

void RunDepth(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Depth), Wide);
    SetRecord(block, 1, block.Texels(Depth), Narrow);
    const auto result = Compile(device, block);
    const auto context = device.graphicsContext();
    AgcDriver::Graphics::DepthSurfaceView(context, AgcDriver::Graphics::DepthTarget{AddressOf(block.Texels(Depth)), 0, {Narrow, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    Require(AgcDriver::Graphics::ActiveDepthExtent(device.Device(), AddressOf(block.Texels(Depth))).has_value(), "image table variants: the depth surface is not active");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto wide = Dispatch(device, result, 0);
    const auto narrow = Dispatch(device, result, 1);
    Require(wide == narrow, "image table variants: the two variants of a surface under a depth surface sampled different texels");
    Require(wide != Row(block.Texels(Depth), Wide), "image table variants: the wide variant sampled guest memory instead of the depth surface");
    for (std::uint32_t lane = Narrow; lane < Lanes; ++lane) Require(wide[lane] == wide[Narrow - 1u], "image table variants: the wide variant under a narrow depth surface did not clamp to the depth surface extent");
    Expect(before, 1, 2, 0, "two dispatches of two variants under an active depth surface");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

void RunRejected(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const Unreachable surface;
    Prepare(block);
    SetRecord(block, 0, surface.Texels(), Wide);
    SetRecord(block, 1, surface.Texels(), Narrow);
    const auto result = Compile(device, block);
    AgcDriver::Graphics::DepthSurfaceView(device.graphicsContext(), AgcDriver::Graphics::DepthTarget{AddressOf(surface.Texels()), 0, {Wide, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    static_cast<void>(Dispatch(device, result, 1));
    Expect(before, 1, 0, 1, "a narrow variant whose representative is not mapped");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

void RunTests(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    RunLargest(device, block);
    RunWritten(device, block);
    RunDepth(device, block);
    RunRejected(device, block);
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunTests(*device, block);
        std::puts("image table variants tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
