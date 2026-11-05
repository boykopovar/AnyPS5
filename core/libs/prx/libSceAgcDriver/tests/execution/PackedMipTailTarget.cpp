#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::Graphics::TextureTileMode;
using AgcDriver::Graphics::TileMipLayout;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Levels = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t BlockSide = 128;
constexpr std::size_t SurfaceBytes = BlockBytes + Side * Side * 4u + BlockBytes;
constexpr std::uint32_t Color32FloatInfo = (4u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Initial = 0x5a5a5a5au;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, Levels> PixelCodes{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, BlockBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + BlockBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "packed mip tail target: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t address, std::uint32_t level, const TileMipLayout& mip) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = level << 26u;
    cx[0x31c] = Color32FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Levels - 1u) << 28u) | ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R32_SFLOAT && color.tileMode == ColorTileMode::RenderTarget, "the COLOR_32 float view did not decode as an SW_64KB_R_X R32 target");
    Require(color.mip == level && color.mipCount == Levels && color.mipTail == mip.tail, "the color view did not keep its mip of the surface");
    Require(color.extent.width == mip.width && color.extent.height == mip.height && color.address == address + mip.tiledOffset, "the color view did not decode the extent and address of mip " + std::to_string(level));
    return color;
}

void DrawLevel(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t level, const TileMipLayout& mip, std::uint32_t value) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    auto& pixelWords = PixelCodes[level];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        pixelWords[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        pixelWords[at++] = value;
    }
    pixelWords[at++] = 0xf800180fu;
    pixelWords[at++] = 0x07060504u;
    pixelWords[at++] = 0xbf810000u;
    const std::span<const std::uint32_t> pixelCode(pixelWords.data(), at);
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    const auto color = DecodeTarget(surface, level, mip);
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = color.extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(mip.height), static_cast<float>(mip.width), -static_cast<float>(mip.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, color.extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state)), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Expect(std::vector<std::uint32_t>& expected, const TileMipLayout& mip, std::uint32_t value) {
    const ColorTargetLayout block(BlockSide, BlockSide, ColorTileMode::RenderTarget, 4);
    const ColorTargetLayout own(mip.width, mip.height, ColorTileMode::RenderTarget, 4);
    for (std::uint32_t y = 0; y < mip.height; ++y) {
        for (std::uint32_t x = 0; x < mip.width; ++x) {
            const auto offset = mip.tail ? block.Offset(x + mip.tailX, y + mip.tailY) : mip.tiledOffset + own.Offset(x, y);
            Require(expected.at(offset / 4u) == Initial, "the expected texels of two mips overlap at byte " + Hex(offset));
            expected[offset / 4u] = value;
        }
    }
}

void Check(const std::uint8_t* texels, const std::vector<std::uint32_t>& expected, const std::string& what) {
    const auto* words = reinterpret_cast<const std::uint32_t*>(texels);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(words[index] == expected[index], what + ": surface byte " + Hex(index * 4u) + " holds " + Hex(words[index]) + ", expected " + Hex(expected[index]));
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    const auto mips = AgcDriver::Graphics::ComputeElementMipLayout(TextureTileMode::kR64KBX, 4, Side, Side, Levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) == SurfaceBytes, "the 256x256 R32 surface with 9 mips is not one tail block, mip 0 and mip 1");
    Require(!mips[1].tail && mips[2].tail && mips[3].tail && mips[2].width == 64 && mips[3].width == 32, "mips 2 and 3 of the 256x256 R32 surface are not its first packed tail mips");
    const auto surface = AddressOf(texels);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    std::vector<std::uint32_t> expected(SurfaceBytes / 4u, Initial);
    const std::array<std::pair<std::uint32_t, float>, 3> draws{{{2u, 0.25f}, {3u, 0.75f}, {1u, 0.5f}}};
    for (const auto& [level, drawn] : draws) {
        const auto value = std::bit_cast<std::uint32_t>(drawn);
        DrawLevel(device, surface, level, mips[level], value);
        Require(StorageTexture::FindPending(surface, SurfaceBytes) == nullptr, "the draw into mip " + std::to_string(level) + " rendered into a resident image instead of the guest-memory fallback");
        Expect(expected, mips[level], value);
        Check(texels, expected, "after the draw into mip " + std::to_string(level));
    }
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_RESIDENT_TARGETS", "1");
#else
        setenv("APS5_NO_RESIDENT_TARGETS", "1", 1);
#endif
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, block.Data());
        std::puts("packed mip tail target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
