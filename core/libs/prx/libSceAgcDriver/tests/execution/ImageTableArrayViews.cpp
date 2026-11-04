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
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t TextureBytes = Wide * Height * 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 48;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TexturesOffset = 65536;
constexpr std::size_t BlockBytes = TexturesOffset + 2 * 65536;
constexpr std::uint32_t Type2DArray = 13;

alignas(256) std::array<std::uint32_t, Records> Keys{};
alignas(256) std::array<std::uint32_t, Records> ArrayKeys{};
alignas(256) std::array<std::uint32_t, Lanes * 8> Output{};
alignas(256) std::array<std::uint32_t, 32> Srt{};

alignas(256) constexpr std::array<std::uint32_t, 40> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080300u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4300802u, 0x24000000u, 0xf4200506u, 0x20000000u,
    0x9315b014u, 0xf4300c02u, 0x2a000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u,
    0x7e0402ffu, 0x3e000000u, 0x7e060280u, 0xf09c0f08u, 0x01090401u, 0xf09c0f28u, 0x018d0a01u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xe0781400u, 0x80070a08u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t type, std::uint32_t format = Format32Float) {
    const auto address = AddressOf(texels);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Wide - 1u) & 3u) << 30u), ((Wide - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (type << 28u)};
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
        Require(memory != nullptr, "image table array views: cannot reserve the unreachable surface");
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
        Require(block != nullptr, "image table array views: cannot allocate the guest block");
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

void SetRecord(const GuestBlock& table, std::uint32_t record, const float* texels, std::uint32_t type, std::uint32_t lastArray = 0, std::uint32_t swizzle = 0xfacu) {
    auto* words = table.Table() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(PointClamp.begin(), PointClamp.end(), words);
    auto image = Image(texels, type);
    image[3] = (image[3] & ~0xfffu) | swizzle;
    std::copy(image.begin(), image.end(), words + 4);
    words[8] = lastArray;
}

void Fill(float* texels, float bias) {
    for (std::uint32_t texel = 0; texel < Wide * Height; ++texel) texels[texel] = bias + static_cast<float>(texel);
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const GuestBlock& table) {
    const std::span<const std::uint32_t> code(TableCode);
    const auto srtAddress = AddressOf(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::span<const std::uint32_t> words(table.Table(), Records * Stride / 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 5> memory{{
        {AddressOf(code.data()), std::as_bytes(code)},
        {srtAddress, std::as_bytes(std::span(Srt))},
        {AddressOf(words.data()), std::as_bytes(words)},
        {AddressOf(Keys.data()), std::as_bytes(std::span(Keys))},
        {AddressOf(ArrayKeys.data()), std::as_bytes(std::span(ArrayKeys))},
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
    Require(binding != result.bindings.end(), "image table array views: the shader binds no image table");
    return result;
}

struct Rows {
    std::vector<std::uint32_t> flat;
    std::vector<std::uint32_t> array;
};

Rows Dispatch(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::uint32_t flat, std::uint32_t array) {
    Keys.fill(flat);
    ArrayKeys.fill(array);
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(TableCode.data()));
    device.WaitIdle();
    Rows rows{std::vector<std::uint32_t>(Lanes), std::vector<std::uint32_t>(Lanes)};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        rows.flat[lane] = Output[lane * 4u];
        rows.array[lane] = Output[Lanes * 4u + lane * 4u];
    }
    return rows;
}

std::vector<std::uint32_t> Row(const float* texels) {
    std::vector<std::uint32_t> row(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) row[lane] = std::bit_cast<std::uint32_t>(texels[lane * Wide / Lanes]);
    return row;
}

std::uint64_t Shared(const AgcDriver::Graphics::ImageTableReuseCounts& before) {
    return AgcDriver::Graphics::ImageTableReuses().arrayViewsShared - before.arrayViewsShared;
}

void Prepare(const GuestBlock& block, std::uint32_t flat, std::uint32_t array) {
    for (std::uint32_t record = 0; record < 8; ++record) Fill(block.Texels(record), 0.25f + static_cast<float>(record) * 100.0f);
    Keys.fill(flat);
    ArrayKeys.fill(array);
    const auto srt = Buffer(block.Table(), Records * Stride);
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto arrayKeys = Buffer(ArrayKeys.data(), static_cast<std::uint32_t>(sizeof(ArrayKeys)));
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(srt.begin(), srt.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    std::copy(arrayKeys.begin(), arrayKeys.end(), Srt.begin() + 12);
}

void RunShared(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block, 0, 1);
    SetRecord(block, 0, block.Texels(0), Type2D);
    SetRecord(block, 1, block.Texels(0), Type2DArray);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto rows = Dispatch(device, result, 0, 1);
    Require(rows.flat == Row(block.Texels(0)), "image table array views: the 2D entry sampled the wrong texels");
    Require(rows.array == Row(block.Texels(0)), "image table array views: the one-layer 2D array entry of the same surface sampled the wrong texels");
    Require(Shared(before) == 1, "image table array views: the one-layer 2D array entry did not bind the 2D entry's texture (" + std::to_string(Shared(before)) + " shared)");
    Fill(block.Texels(0), 9000.0f);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto again = Dispatch(device, result, 0, 1);
    Require(again.flat == Row(block.Texels(0)) && again.array == Row(block.Texels(0)), "image table array views: after the guest rewrote the surface, an entry sampled the old texels");
}

void RunOtherSurface(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block, 0, 1);
    SetRecord(block, 0, block.Texels(3), Type2D);
    SetRecord(block, 1, block.Texels(4), Type2DArray);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto rows = Dispatch(device, result, 0, 1);
    Require(rows.flat == Row(block.Texels(3)), "image table array views: the 2D entry beside another surface's array entry sampled the wrong texels");
    Require(rows.array == Row(block.Texels(4)), "image table array views: an array entry over another surface did not sample its own surface");
    Require(Shared(before) == 0, "image table array views: an array entry over another surface borrowed the 2D entry's texture");
}

void RunOtherSwizzle(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block, 0, 1);
    SetRecord(block, 0, block.Texels(2), Type2D);
    SetRecord(block, 1, block.Texels(2), Type2DArray, 0, 0xfa9u);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto rows = Dispatch(device, result, 0, 1);
    Require(rows.flat == Row(block.Texels(2)), "image table array views: the 2D entry beside an array entry of another swizzle sampled the wrong texels");
    Require(rows.array == std::vector<std::uint32_t>(Lanes, std::bit_cast<std::uint32_t>(1.0f)), "image table array views: an array entry whose red reads the constant 1 did not read it");
    Require(Shared(before) == 0, "image table array views: an array entry of another swizzle borrowed the 2D entry's texture");
}

void RunLayers(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block, 0, 1);
    SetRecord(block, 0, block.Texels(5), Type2D);
    SetRecord(block, 1, block.Texels(5), Type2DArray, 1);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto rows = Dispatch(device, result, 0, 1);
    Require(rows.flat == Row(block.Texels(5)), "image table array views: the 2D entry beside a two-layer array entry sampled the wrong texels");
    Require(rows.array == Row(block.Texels(5)), "image table array views: slice 0 of a two-layer array entry sampled the wrong texels");
    Require(Shared(before) == 0, "image table array views: a two-layer array entry borrowed the one-layer 2D entry's texture");
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunShared(*device, block);
        RunOtherSurface(*device, block);
        RunOtherSwizzle(*device, block);
        RunLayers(*device, block);
        std::puts("image table array view tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
