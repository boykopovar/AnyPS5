#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
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
#include <string>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::size_t Block = 65536;
alignas(256) std::array<std::uint32_t, HtileBytes / 4u> Htile{};
alignas(256) std::array<std::uint32_t, HtileBytes / 4u> ViewedHtile{};
alignas(256) constexpr std::array<std::uint32_t, 11> LoadCode{
    0x7e02020du, 0x7e04020cu, 0xf0001108u, 0x00010300u, 0xbf8c3f70u, 0x34080082u, 0xe0701000u, 0x80000304u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
constexpr std::uint32_t LoadThreads = 32;
constexpr std::size_t ColorBytes = static_cast<std::size_t>(Width) * Height * 4u;

void* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map the depth plane block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

std::array<std::uint32_t, LoadThreads> LoadRow(AgcDriver::VulkanDevice& device, std::uint64_t address, std::uint32_t width, std::uint32_t height, std::uint32_t swizzle) {
    alignas(256) static std::array<std::uint32_t, LoadThreads> output{};
    std::vector<std::uint32_t> userData(16, 0u);
    const auto out = reinterpret_cast<std::uintptr_t>(output.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(out), static_cast<std::uint32_t>((out >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(output)), 0x31016facu};
    const std::array<std::uint32_t, 8> texture{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((height - 1u) << 14u), 0xfacu | (swizzle << 20u) | (9u << 28u), 0u, 0u, 0u, 0u};
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(LoadCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{LoadThreads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    output.fill(0xdeadbeefu);
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return output;
}

bool RowHolds(const std::array<std::uint32_t, LoadThreads>& row, float depth) {
    return std::all_of(row.begin(), row.end(), [&](std::uint32_t bits) { return bits == std::bit_cast<std::uint32_t>(depth); });
}

std::size_t Holding(std::span<const float> plane, float depth) {
    return static_cast<std::size_t>(std::count(plane.begin(), plane.end(), depth));
}

void DrawBlendKeepingMemory(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& compiled, std::uint64_t address) {
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {address, {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, ColorBytes, 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0.0f, 1.0f};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend = {VK_TRUE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 15};
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(DepthFastClearHarness::Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto planeBytes = static_cast<std::size_t>(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 4u));
        Require(planeBytes <= Block, "the depth plane does not fit one block");
        auto* block = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> plane(block, planeBytes / sizeof(float));
        const auto address = reinterpret_cast<std::uintptr_t>(block);
        const auto shaders = Compile(*device);
        const auto reader = Compile(*device, block);
        const Surface depth{address, 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX)};

        Require(Draw(*device, shaders, depth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 did not cover the target");
        Draw(*device, reader, depth, {.depthTest = false});
        Require(Holding(plane, 0.5f) == static_cast<std::size_t>(Width) * Height, "a vertex fetch from the depth plane did not see the depth 0.5 a draw wrote into it");

        if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(address, Block)) {
            std::puts("guest memory is not write-watched here: a buffer write into the depth plane is not exercised");
            std::puts("depth plane buffer memory tests passed");
            return 0;
        }
        std::fill(plane.begin(), plane.end(), 0.75f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(Draw(*device, shaders, depth, {}) == Covered, "a draw at depth 0.5 failed LESS against the 0.75 written into the depth plane's memory");
        Require(Draw(*device, shaders, depth, {.depthWrite = true}) == Covered, "a draw at depth 0.5 writing depth failed LESS against 0.75");
        Draw(*device, reader, depth, {.depthTest = false});
        Require(Holding(plane, 0.5f) == static_cast<std::size_t>(Width) * Height, "a vertex fetch from the depth plane did not see the depth 0.5 written after the plane was reloaded");

        auto* fresh = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> freshPlane(fresh, planeBytes / sizeof(float));
        const auto freshReader = Compile(*device, fresh);
        const Surface unstored{reinterpret_cast<std::uintptr_t>(fresh), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, depth.swizzle};
        Require(Draw(*device, shaders, unstored, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 into a new plane did not cover the target");
        std::fill(freshPlane.begin(), freshPlane.end(), 0.75f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Draw(*device, freshReader, depth, {.depthTest = false});
        Require(Holding(freshPlane, 0.75f) == freshPlane.size(), "the depth 0.5 a draw wrote before a guest write into the plane was stored over that write");
        Require(Draw(*device, shaders, unstored, {}) == Covered, "a draw at depth 0.5 failed LESS against the 0.75 written over depth the surface had not stored");
        std::fill(freshPlane.begin(), freshPlane.end(), 0.25f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(Draw(*device, shaders, unstored, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.25 written into the plane before it");
        Require(Draw(*device, shaders, unstored, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 over the reloaded plane did not cover the target");
        Draw(*device, freshReader, depth, {.depthTest = false});
        Require(Holding(freshPlane, 0.5f) == static_cast<std::size_t>(Width) * Height, "the depth 0.5 a draw wrote after a guest write into the plane did not reach memory");

        Htile.fill(0xffffffffu);
        auto* cleared = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> clearedPlane(cleared, planeBytes / sizeof(float));
        const auto clearedReader = Compile(*device, cleared);
        const Surface compressed{reinterpret_cast<std::uintptr_t>(cleared), 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false, VK_FORMAT_D32_SFLOAT, 0, 0, depth.swizzle};
        Require(Draw(*device, shaders, compressed, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 into a plane with HTILE did not cover the target");
        Draw(*device, clearedReader, depth, {.depthTest = false});
        Require(Holding(clearedPlane, 0.5f) == static_cast<std::size_t>(Width) * Height, "a vertex fetch from a plane with HTILE did not see the depth 0.5 a draw wrote into it");
        std::fill(clearedPlane.begin(), clearedPlane.end(), 0.25f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        AgcDriver::Graphics::NoteDepthMetadataFill(compressed.htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, compressed, {}) == Covered, "a guest write into the depth plane won over a later HTILE fast clear of it");

        auto* viewed = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> viewedBlock(viewed, Block / sizeof(float));
        std::fill(viewedBlock.begin(), viewedBlock.end(), 0.25f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        ViewedHtile.fill(0xffffffffu);
        const Surface sampled{reinterpret_cast<std::uintptr_t>(viewed), 0, reinterpret_cast<std::uintptr_t>(ViewedHtile.data()), false, VK_FORMAT_D32_SFLOAT, 0, 0, depth.swizzle};
        const auto viewedReader = Compile(*device, viewed);
        constexpr std::size_t ViewOffset = 0x100;
        const auto view = sampled.depth + ViewOffset;
        const auto matchesMemory = [&](const std::array<std::uint32_t, LoadThreads>& row) {
            Draw(*device, viewedReader, depth, {.depthTest = false});
            return std::memcmp(row.data(), reinterpret_cast<const std::byte*>(viewed) + ViewOffset, sizeof(row)) == 0;
        };
        Require(Draw(*device, shaders, sampled, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 into a viewed plane did not cover the target");
        Require(matchesMemory(LoadRow(*device, view, Width, Height / 2u, 0u)), "a linear texture at an offset into a depth plane did not read the depth drawn into it");
        AgcDriver::Graphics::NoteDepthMetadataFill(sampled.htile, HtileBytes, 0u);
        Draw(*device, shaders, sampled, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_NEVER});
        Require(matchesMemory(LoadRow(*device, view, Width, Height / 2u, 0u)), "a cached linear texture at an offset into a depth plane did not read the depth cleared after it was cached");
        Require(Draw(*device, shaders, sampled, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 over the cleared viewed plane did not cover the target");
        Require(matchesMemory(LoadRow(*device, view, Width, Height / 2u, 0u)), "a linear texture at an offset into a depth plane, viewed through its cached storage image, did not read the depth drawn after the image was made");

        auto* blended = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> blendedBlock(blended, Block / sizeof(float));
        std::fill(blendedBlock.begin(), blendedBlock.end(), 0.25f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const Surface underBlend{reinterpret_cast<std::uintptr_t>(blended), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, depth.swizzle};
        Require(Draw(*device, shaders, underBlend, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 under a later blended color target did not cover the target");
        DrawBlendKeepingMemory(*device, shaders, underBlend.depth);
        Draw(*device, Compile(*device, blended), depth, {.depthTest = false});
        Require(Holding(blendedBlock, 0.5f) == static_cast<std::size_t>(Width) * Height, "a color target blending over a depth plane did not load the depth drawn into the plane");
        Htile.fill(0xffffffffu);
        auto* reused = static_cast<float*>(AllocateWatched(Block));
        const std::span<float> reusedPlane(reused, planeBytes / sizeof(float));
        const auto reusedReader = Compile(*device, reused);
        const Surface recleared{reinterpret_cast<std::uintptr_t>(reused), 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false, VK_FORMAT_D32_SFLOAT, 0, 0, depth.swizzle};
        Require(Draw(*device, shaders, recleared, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the draw writing depth 0.5 into a plane that is cleared each frame did not cover the target");
        Draw(*device, reusedReader, depth, {.depthTest = false});
        Require(Holding(reusedPlane, 0.5f) == static_cast<std::size_t>(Width) * Height, "a vertex fetch did not see the depth 0.5 drawn into a plane that is cleared each frame");
        std::fill(reusedPlane.begin(), reusedPlane.end(), 0.25f);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        for (int frame = 0; frame < 2; ++frame) {
            const float value = 0.75f - 0.125f * static_cast<float>(frame);
            AgcDriver::Graphics::NoteDepthMetadataFill(recleared.htile, HtileBytes, 0u);
            Require(Draw(*device, shaders, recleared, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = value, .maxDepth = value}) == Covered, "the draw after the HTILE clear did not cover the target");
            Draw(*device, reusedReader, depth, {.depthTest = false});
            Require(Holding(reusedPlane, value) == static_cast<std::size_t>(Width) * Height, "after a guest write into a stored depth plane, the depth drawn behind an HTILE clear did not reach memory (frame " + std::to_string(frame) + ")");
        }
        std::puts("depth plane buffer memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
