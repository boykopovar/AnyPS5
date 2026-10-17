#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "SceShaders.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 80;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Rgba8UnormInfo = 0x8028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
std::array<std::uint32_t, 16> HullUserData{0x1000u};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalCode{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullCode{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr auto MergedHullCode = [] {
    std::array<std::uint32_t, 32 + HullCode.size()> code{};
    code[0] = 0x8f6a9003u;
    code[1] = 0x94fe6ac1u;
    code[2] = 0xbf88001cu;
    std::copy(LocalCode.begin(), LocalCode.end() - 1, code.begin() + 3);
    code[31] = 0xbefe04c1u;
    std::copy(HullCode.begin(), HullCode.end(), code.begin() + 32);
    return code;
}();

alignas(256) constexpr std::array<std::uint32_t, 30> DomainCode{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02,
    0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280,
    0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 133> DomainGeometryCode{
    0x8f6a9003, 0x94fe6ac1, 0xbf880028, 0x342a0e84, 0x162a2a83, 0xe0341000, 0x04030a15, 0xe0341010,
    0x04030c15, 0xe0341020, 0x04030e15, 0xbf8c3f70, 0x082c0af2, 0x082c0d16, 0x10201505, 0x3e201906,
    0x3e201d16, 0x10221705, 0x3e221b06, 0x3e221f16, 0xd5510012, 0x045a0d05, 0x102424ff, 0x40400000,
    0x7e260280, 0x7e2802f2, 0xd7650009, 0x000100c1, 0xd7660009, 0x000212c1, 0x93ebff03, 0x00040018,
    0xd7460009, 0x04250c6b, 0x34121285, 0xd8380100, 0x00111009, 0xd8380302, 0x00141309, 0xd8380504,
    0x00121409, 0xd8380706, 0x00141309, 0xbf8cc07f, 0xbefe04c1, 0xbf8a0000, 0x938dff02, 0x00090016,
    0x9382ff03, 0x00040018, 0x938cff03, 0x00080008, 0xd7650009, 0x000100c1, 0xd7660009, 0x000212c1,
    0xd746000a, 0x04250c02, 0x7da8120c, 0xbf88002e, 0x361600ff, 0x0000ffff, 0x34161682, 0x2c180090,
    0x34181882, 0x361a02ff, 0x0000ffff, 0x341a1a82, 0xdbfc0000, 0x1000000b, 0xdbfc0010, 0x1400000b,
    0xdbfc0000, 0x1800000c, 0xdbfc0010, 0x1c00000c, 0xdbfc0000, 0x2000000d, 0xdbfc0010, 0x2400000d,
    0xbf8cc07f, 0xbf800000, 0xbf800000, 0xbf800000, 0x161c1483, 0x161e14ff, 0x00000060, 0xbf8c3f70,
    0xdb7c0400, 0x0000100f, 0xdb7c0410, 0x0000140f, 0xdb7c0420, 0x0000180f, 0xdb7c0430, 0x00001c0f,
    0xdb7c0440, 0x0000200f, 0xdb7c0450, 0x0000240f, 0x4a501c81, 0x4a521c82, 0x3450508a, 0x34525294,
    0xd772002a, 0x04a6510e, 0xbf8cc07f, 0xbefe04c1, 0xbf8a0000, 0x930e830d, 0xbf078002, 0xbf850003,
    0x8f0f8c0d, 0x887c0f0e, 0xbf900009, 0x7da8140d, 0xbf880002, 0xf8000941, 0x0000002a, 0xbefe04c1,
    0x7da8140e, 0xbf88000a, 0x34561485, 0xdbfc0400, 0x2c00002b, 0xdbfc0410, 0x3000002b, 0xbf8cc07f,
    0xf80008cf, 0x2f2e2d2c, 0xf800020f, 0x33323130, 0xbf810000,
};

alignas(256) constexpr auto Wave32DomainGeometryCode = [] {
    auto code = DomainGeometryCode;
    code[33] = 0x04250a6bu;
    code[57] = 0x04250a02u;
    return code;
}();

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

using ShaderRecompiler::TessellationConfiguration;

constexpr TessellationConfiguration Patches{3u, 3u, 1u, 2u, 2u};
constexpr ShaderRecompiler::MeshConfiguration Subgroup{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 8u};
constexpr std::array<std::uint32_t, 3> PatchLevels{1u, 0u, 5u};

constexpr std::uint16_t Corner(std::uint32_t patch, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint16_t>((PatchLevels[patch] << 8u) | (patch << 4u) | (x << 2u) | y);
}

alignas(256) constexpr std::array<std::uint16_t, 9> Indices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

bool ProvidesSubgroup(const ShaderRecompiler::SpirvTarget& target) {
    return target.tessellation.has_value() && target.mesh.has_value();
}

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::uint32_t& pushCursor, bool geometry, std::uint32_t wave) {
    static const std::array<std::uint32_t, 8> domainUserData{};
    static const std::array<std::uint32_t, 8> mergedUserData{};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const bool hull = stage == ShaderStage::TessellationControl;
    const bool merged = geometry && (stage == ShaderStage::TessellationEvaluation || stage == ShaderStage::Mesh);
    const auto firstUser = hull || merged ? 0u : 8u;
    const auto userData = hull ? std::span<const std::uint32_t>(HullUserData) : merged ? std::span<const std::uint32_t>(mergedUserData) : stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(domainUserData) : std::span<const std::uint32_t>();
    std::optional<ShaderRecompiler::ShaderPixelStageInfo> pixel;
    std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
    std::optional<ShaderRecompiler::GraphicsCompileContext> graphics;
    if (stage == ShaderStage::Fragment) {
        ShaderRecompiler::ShaderPixelStageInfo info{};
        info.interpolatorCount = 1;
        info.interpolatorSettings[0] = 0x400u;
        info.targetOutputMode[0] = 9;
        info.targetExportMapping.fill(0xe4u);
        pixel = info;
    } else {
        vertex = ShaderRecompiler::ShaderVertexStageInfo{};
        graphics = ShaderRecompiler::GraphicsCompileContext{firstUser, {}, merged ? std::optional(Subgroup) : std::nullopt, Patches, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1}};
    }
    auto layout = stage == ShaderStage::Fragment ? PixelPushLayout(pushCursor, target) : ShaderRecompiler::BindingLayout{0, 0, pushCursor, AgcDriver::Graphics::PipelinePushSlotBytes - pushCursor};
    if (layout.pushConstantOffsetBytes < AgcDriver::Graphics::PipelinePushSlotBytes) layout.pushConstantSizeBytes = (geometry ? ShaderRecompiler::MeshDrawPushOffsetBytes : ShaderRecompiler::TessellationLevelPushOffsetBytes) - layout.pushConstantOffsetBytes;
    const ShaderRecompiler::RecompileRequest request{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {stage == ShaderStage::Fragment ? 64u : wave, firstUser, userData, std::nullopt, pixel, vertex, memory},
        target,
        layout,
        graphics
    };
    auto result = ShaderRecompiler::Recompile(request);
    pushCursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return result;
}

std::uint32_t Offset(const ShaderRecompiler::RecompileResult& result, std::uint32_t& cursor) {
    const auto at = result.pushConstants.empty() ? 0u : cursor;
    cursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return at;
}

AgcDriver::Graphics::ColorTarget LinearTarget() {
    return {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
}

AgcDriver::Graphics::State PatchState(AgcDriver::Graphics::ShaderPath path, VkCullModeFlags cull, std::uint32_t wave, const AgcDriver::Graphics::ColorTarget& color) {
    AgcDriver::Graphics::State state{};
    const auto wave32 = wave == 32u ? 0x00600000u : 0u;
    state.stages = {path, (path == AgcDriver::Graphics::ShaderPath::Tessellation ? 0x0200210du : 0x0000212du) | wave32, wave, 64, std::nullopt, Patches};
    state.stages.hullWaveSize = wave;
    if (path == AgcDriver::Graphics::ShaderPath::TessellationGeometry) state.stages.tessellationGeometry = Subgroup;
    state.color = color;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = cull;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    return state;
}

void ClearPixels() {
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) Pixels[i + c] = std::byte{Background[c]};
    }
}

void DrawReference(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, VkCullModeFlags cull, std::uint32_t wave, const AgcDriver::Graphics::ColorTarget& color) {
    std::uint32_t cursor = 0;
    const auto local = Compile(target, ShaderStage::Local, LocalCode, cursor, false, wave);
    const auto hull = Compile(target, ShaderStage::TessellationControl, HullCode, cursor, false, wave);
    const auto domain = Compile(target, ShaderStage::TessellationEvaluation, DomainCode, cursor, false, wave);
    const auto fragment = Compile(target, ShaderStage::Fragment, PixelCode, cursor, false, wave);
    std::uint32_t offsets = 0;
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Local, &local, Offset(local, offsets)},
        {ShaderStage::TessellationControl, &hull, Offset(hull, offsets)},
        {ShaderStage::TessellationEvaluation, &domain, Offset(domain, offsets)},
        {ShaderStage::Fragment, &fragment, PixelPushOffset(offsets, target)},
    }};
    device.Draw(PatchState(AgcDriver::Graphics::ShaderPath::Tessellation, cull, wave, color), {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1, 0, true, 0, 0}, shaders);
    device.WaitIdle();
}

void DrawThroughGeometry(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, VkCullModeFlags cull, std::uint32_t wave, const AgcDriver::Graphics::ColorTarget& color) {
    std::uint32_t cursor = 0;
    const std::span<const std::uint32_t> merged = wave == 32u ? std::span<const std::uint32_t>(Wave32DomainGeometryCode) : std::span<const std::uint32_t>(DomainGeometryCode);
    const auto local = Compile(target, ShaderStage::Local, LocalCode, cursor, true, wave);
    const auto hull = Compile(target, ShaderStage::TessellationControl, HullCode, cursor, true, wave);
    const auto domain = Compile(target, ShaderStage::TessellationEvaluation, merged, cursor, true, wave);
    const auto mesh = Compile(target, ShaderStage::Mesh, merged, cursor, true, wave);
    const auto fragment = Compile(target, ShaderStage::Fragment, PixelCode, cursor, true, wave);
    const auto capture = ShaderRecompiler::BuildTessellationCaptureGeometryShader(Subgroup, target, device.GeometryLimits());
    std::uint32_t offsets = 0;
    const std::array<AgcDriver::Graphics::CompiledShader, 6> shaders{{
        {ShaderStage::Local, &local, Offset(local, offsets)},
        {ShaderStage::TessellationControl, &hull, Offset(hull, offsets)},
        {ShaderStage::TessellationEvaluation, &domain, Offset(domain, offsets)},
        {ShaderStage::Geometry, &capture, 0},
        {ShaderStage::Mesh, &mesh, Offset(mesh, offsets)},
        {ShaderStage::Fragment, &fragment, PixelPushOffset(offsets, target)},
    }};
    device.Draw(PatchState(AgcDriver::Graphics::ShaderPath::TessellationGeometry, cull, wave, color), {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1, 0, true, 0, 0}, shaders);
    device.WaitIdle();
}

void RequireSameImage(std::span<const std::byte> reference, std::span<const std::byte> drawn, const std::string& what) {
    std::uint32_t covered = 0;
    for (std::size_t offset = 0; offset < reference.size(); offset += 4) {
        const bool background = std::equal(Background.begin(), Background.end(), reference.begin() + static_cast<std::ptrdiff_t>(offset), [](std::uint8_t channel, std::byte value) { return std::to_integer<std::uint8_t>(value) == channel; });
        if (!background) ++covered;
        if (std::memcmp(reference.data() + offset, drawn.data() + offset, 4) == 0) continue;
        char text[192];
        std::snprintf(text, sizeof(text), "%s: the element at byte %zu is (%u, %u, %u, %u), the tessellation path drew (%u, %u, %u, %u)", what.c_str(), offset, std::to_integer<unsigned>(drawn[offset]), std::to_integer<unsigned>(drawn[offset + 1]), std::to_integer<unsigned>(drawn[offset + 2]), std::to_integer<unsigned>(drawn[offset + 3]), std::to_integer<unsigned>(reference[offset]), std::to_integer<unsigned>(reference[offset + 1]), std::to_integer<unsigned>(reference[offset + 2]), std::to_integer<unsigned>(reference[offset + 3]));
        Require(false, text);
    }
    Require(covered > 200, what + ": the tessellation path drew only " + std::to_string(covered) + " pixels");
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget TiledTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba8UnormInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3 | (static_cast<std::uint32_t>(AgcDriver::Graphics::ColorTileMode::RenderTarget) << 14u);
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R8G8B8A8_UNORM && color.tileMode == AgcDriver::Graphics::ColorTileMode::RenderTarget && color.bytes <= block.bytes, "the tiled target decoded differently");
    return color;
}

void FillBackground(Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test clear");
    for (std::size_t offset = 0; offset + 4 <= block.bytes; offset += 4) std::memcpy(block.data + offset, Background.data(), 4);
}

std::vector<std::byte> ReadBack(AgcDriver::VulkanDevice& device, const Block& block, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::byte> stored(bytes);
    AgcDriver::GuestMemory::Read(block.Address(), stored, 1);
    return stored;
}

void RequireRecordedTarget(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    Require(AgcDriver::Graphics::Recorder::Active() != nullptr, "the device has no active recorder for draws into resident targets");
    const AgcDriver::Graphics::ColorTargetLayout layout(Width, Height, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    Block block((layout.Bytes() + BlockBytes - 1) / BlockBytes * BlockBytes);
    const auto color = TiledTarget(block);
    for (const auto wave : {64u, 32u}) {
        FillBackground(block);
        DrawReference(device, target, VK_CULL_MODE_NONE, wave, color);
        const auto reference = ReadBack(device, block, color.bytes);
        FillBackground(block);
        DrawThroughGeometry(device, target, VK_CULL_MODE_NONE, wave, color);
        RequireSameImage(reference, ReadBack(device, block, color.bytes), "recorded wave" + std::to_string(wave) + " patches in a resident tiled target");
    }
}

struct RegisteredStage {
    Shader shader{};
    std::array<ShaderRegister, 6> registers{};
    std::array<ShaderRegister, 4> context{};
    ShaderSpecialRegs specials{};
    ShaderUserData users{};
};

template<std::size_t TCount>
void Describe(RegisteredStage& stage, const std::array<std::uint32_t, TCount>& code, std::uint8_t type, std::initializer_list<ShaderRegister> registers, std::initializer_list<ShaderRegister> context, std::uint32_t routing) {
    stage.shader.file_header = 0x34333231u;
    stage.shader.version = 0x18u;
    stage.shader.header_size = sizeof(RegisteredStage);
    stage.shader.shader_size = sizeof(code);
    stage.shader.code = code.data();
    stage.shader.type = type;
    stage.shader.user_data = &stage.users;
    std::copy(registers.begin(), registers.end(), stage.registers.begin());
    stage.shader.sh_registers = stage.registers.data();
    stage.shader.num_sh_registers = static_cast<std::uint8_t>(registers.size());
    std::copy(context.begin(), context.end(), stage.context.begin());
    stage.shader.cx_registers = stage.context.data();
    stage.shader.num_cx_registers = static_cast<std::uint8_t>(context.size());
    stage.specials.ge_cntl = {0x25bu, 0u};
    stage.specials.vgt_shader_stages_en = {0x2d5u, routing};
    stage.specials.vgt_gs_out_prim_type = {0x29bu, 2u};
    stage.specials.ge_user_vgpr_en = {0x262u, 0u};
    stage.shader.specials = &stage.specials;
}

void RequireRegisteredStages() {
    const auto low = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 8u); };
    const auto high = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 40u); };
    for (const std::uint32_t hullWave32 : {0u, 0x00200000u}) {
        RegisteredStage hull;
        RegisteredStage domain;
        Describe(hull, MergedHullCode, 3u, {{0x148u, low(MergedHullCode)}, {0x149u, high(MergedHullCode)}, {0x10au, 0u}, {0x10bu, 16u}}, {{0x2dbu, 0x40049u}, {0x286u, 0x42800000u}, {0x287u, 0x3f800000u}, {0x2d6u, 0xc355u}}, 0x105u | hullWave32);
        Describe(domain, DomainGeometryCode, 2u, {{0xc8u, low(DomainGeometryCode)}, {0xc9u, high(DomainGeometryCode)}, {0x8au, 0u}, {0x8bu, 0x400010u}}, {{0x1ffu, 12u}, {0x2ceu, 3u}, {0x2abu, 8u}}, 0x2028u);
        for (const auto* stage : {&hull, &domain}) AgcDriverRegisterShader_nid_postfix(&stage->shader);
        const std::array<const Shader*, 2> stages{&hull.shader, &domain.shader};
        const std::array<ShaderRegister, 2> context{{{0x2d5u, 0x0000212du | hullWave32}, {0x29bu, 2u}}};
        const std::array<ShaderRegister, 3> primitive{{{0x25bu, 0x1804u}, {0x262u, 0u}, {0x242u, 9u}}};
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
    }
    AgcDriverShutdown_nid_postfix();
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!ProvidesSubgroup(target) || !device->GeometryLimits().has_value()) {
            std::puts("tessellation geometry: skipped, the device has no tessellation, mesh or geometry shaders");
            return VulkanTestSkipped;
        }
        if ((device->SubgroupStages() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) == 0) {
            std::puts("tessellation geometry: skipped, the device has no subgroup operations in tessellation-evaluation shaders, which the captured domain half uses");
            return VulkanTestSkipped;
        }
        for (const auto wave : {64u, 32u}) {
            for (const auto cull : {VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT}) {
                ClearPixels();
                DrawReference(*device, target, cull, wave, LinearTarget());
                const std::vector<std::byte> reference(Pixels.begin(), Pixels.end());
                ClearPixels();
                DrawThroughGeometry(*device, target, cull, wave, LinearTarget());
                RequireSameImage(reference, Pixels, std::string(cull == VK_CULL_MODE_NONE ? "unculled" : "back-face culled") + " wave" + std::to_string(wave) + " patches");
            }
        }
        RequireRecordedTarget(*device, target);
        device.reset();
        RequireRegisteredStages();
        std::puts("Tessellation geometry tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
