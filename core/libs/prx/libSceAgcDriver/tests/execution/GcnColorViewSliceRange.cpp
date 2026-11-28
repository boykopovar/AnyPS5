#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Slices = 3;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::size_t PlaneBytes = Width * Height * 4u;
constexpr std::size_t Slot = 0x10000;
constexpr std::size_t BlockBytes = 2u * Slot;
constexpr std::size_t ArrayColor = 0u;
constexpr std::size_t Vertices = Slot;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::uint8_t Canary = 0x5au;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000000u, 0xbf8c0f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr Triangle Screen{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

alignas(256) std::array<std::uint32_t, 64> RedProgram{};

std::span<const std::uint32_t> Program(const Color& color) {
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        RedProgram[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        RedProgram[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    RedProgram[at++] = 0xf800180fu;
    RedProgram[at++] = 0x07060504u;
    RedProgram[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(RedProgram.data(), at);
}

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t offset) const { return Address() + offset; }
    std::uint8_t* Data(std::size_t offset) const { return data + offset; }

private:
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeView(std::uint64_t color, std::uint32_t firstSlice, std::uint32_t lastSlice) {
    AgcDriver::Registers cx;
    cx[0x207] = 0u;
    cx[0x318] = static_cast<std::uint32_t>(color >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = firstSlice | (lastSlice << 13u);
    cx[0x31c] = (Format8888 << 2u) | BlendClamp;
    cx[0x31d] = Thin1D;
    const auto target = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(target.format == VK_FORMAT_R8G8B8A8_UNORM && target.tileMode == AgcDriver::Graphics::ColorTileMode::GcnThin1D && target.layers == 1u, "the GCN array view did not decode as one thin 1D RGBA8 slice");
    return target;
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    return pixel;
}

void Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, const AgcDriver::Graphics::ColorTarget& color, const Triangle& triangle, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    std::memcpy(block.Data(Vertices), triangle.data(), sizeof(triangle));
    const auto address = block.At(Vertices);
    const std::vector<std::uint32_t> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(sizeof(triangle)), 0xfacu | (7u << 12u) | (14u << 15u)};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, PixelInfo(), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = color.samples;
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::vector<std::uint8_t> ReadSlices(const LowBlock& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.At(ArrayColor), Slices * PlaneBytes, nullptr, "test read-back");
    std::vector<std::uint8_t> stored(Slices * PlaneBytes);
    AgcDriver::GuestMemory::Read(block.At(ArrayColor), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("skipped, no guest block below the 40-bit GCN color base range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        std::memset(block.Data(ArrayColor), Canary, Slices * PlaneBytes);
        const auto view = DecodeView(block.At(ArrayColor), 1u, 2u);
        Require(view.address == block.At(ArrayColor) + PlaneBytes && view.bytes == PlaneBytes, "the GCN view of slices 1 to 2 does not start at slice 1");
        Draw(*device, block, view, Screen, Program({1.0f, 0.0f, 0.0f, 1.0f}));
        const auto stored = ReadSlices(block);
        for (std::uint32_t slice = 0; slice < Slices; ++slice) {
            const auto first = stored.begin() + static_cast<std::ptrdiff_t>(slice * PlaneBytes);
            for (std::size_t texel = 0; texel < Width * Height; ++texel) {
                const auto* bytes = &*(first + static_cast<std::ptrdiff_t>(texel * 4u));
                const bool red = bytes[0] == 255u && bytes[1] == 0u && bytes[2] == 0u && bytes[3] == 255u;
                const bool untouched = bytes[0] == Canary && bytes[1] == Canary && bytes[2] == Canary && bytes[3] == Canary;
                if (slice == 1u) Require(red, "texel " + std::to_string(texel) + " of slice 1, the view's first slice, is not red");
                else Require(untouched, "texel " + std::to_string(texel) + " of slice " + std::to_string(slice) + ", outside the view's first slice, was written");
            }
        }
        std::printf("GCN color view slice range tests passed: a view of slices 1 to 2 without a vertex render target index drew into slice 1 only (%zu bytes per slice)\n", PlaneBytes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
