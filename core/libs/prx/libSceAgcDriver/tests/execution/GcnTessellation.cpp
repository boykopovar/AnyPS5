#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Optimization/TessellationLowering.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using ShaderRecompiler::TessellationConfiguration;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 80;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
constexpr std::array<std::uint8_t, 4> White{255, 255, 255, 255};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 26> GcnLocal{
    0xbefc03c1, 0xd2900002, 0x02110900, 0xd2900003, 0x02090500, 0xd2900004, 0x02090100, 0x7e040d02,
    0x7e060d03, 0x7e080d04, 0x100a04ff, 0x3f19999a, 0x060a0aff, 0xbf666666, 0x3e0a06ff, 0x3e800000,
    0x100c08ff, 0x3f4ccccd, 0x060c0cff, 0xbf4ccccd, 0x340e0284, 0xd8340000, 0x00000507, 0xd8340004,
    0x00000607, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 32> GcnHullOffChip{
    0xbefc03c1, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0x34080484, 0xd2860005, 0x04116103,
    0xd8d80000, 0x06000005, 0xd8d80004, 0x07000005, 0xbf8c007f, 0xe0701000, 0x04000605, 0xe0701004,
    0x04000705, 0x7d840480, 0xbe86246a, 0x36100081, 0x7e100d08, 0x081010f2, 0x34120684, 0xe0701000,
    0x05000809, 0xe0701004, 0x05000809, 0xe0701008, 0x05000809, 0xe070100c, 0x05000809, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> GcnHullOnChip{
    0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0x7d840480, 0xbe86246a, 0x36100081, 0x7e100d08,
    0x081010f2, 0x34120684, 0xe0701000, 0x04000809, 0xe0701004, 0x04000809, 0xe0701008, 0x04000809,
    0xe070100c, 0x04000809, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 30> GcnHullOnChipLds{
    0xbefc03c1, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0x34080484, 0xd2860005, 0x04116103,
    0xd8d80000, 0x06000005, 0xbf8c007f, 0x4a140aff, 0x00000900, 0xd8340000, 0x0000060a, 0x7d840480,
    0xbe86246a, 0x36100081, 0x7e100d08, 0x081010f2, 0x34120684, 0xe0701000, 0x04000809, 0xe0701004,
    0x04000809, 0xe0701008, 0x04000809, 0xe070100c, 0x04000809, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 29> GcnDomainOffChip{
    0x160804b0, 0xe0301000, 0x04000504, 0xe0301004, 0x04000604, 0xe0301010, 0x04000704, 0xe0301014,
    0x04000804, 0xe0301020, 0x04000904, 0xe0301024, 0x04000a04, 0xbf8c0070, 0x081600f2, 0x0816030b,
    0x10180b00, 0x3e180f01, 0x3e18130b, 0x101a0d00, 0x3e1a1101, 0x3e1a150b, 0x7e1c0280, 0x7e1e02f2,
    0xf80008cf, 0x0f0e0d0c, 0xf800020f, 0x0f0f0f0f, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 21> GcnDomainOnChip{
    0x081600f2, 0x0816030b, 0x7e080d03, 0x100808ff, 0x3f19999a, 0x060808ff, 0xbf666666, 0x3e0802f0,
    0x3e0816ff, 0x3e800000, 0x100a16ff, 0x3fcccccd, 0x060a0aff, 0xbf4ccccd, 0x7e1c0280, 0x7e1e02f2,
    0xf80008cf, 0x0f0e0504, 0xf800020f, 0x0f0f0f0f, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> GcnDomainOnChipLds{
    0xbefc03c1, 0x160804b0, 0xd8d80000, 0x05000004, 0xbf8c007f, 0x7e1e02f2, 0xf80008cf, 0x0f0f0505,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 28> GcnHullOnChipPassthrough{
    0xbefc03c1, 0xc2c60500, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0xbf8c007f, 0x7d840480,
    0xbe86246a, 0x36100081, 0x7e100d08, 0x081010f2, 0x34120684, 0xe0701000, 0x08000809, 0xe0701004,
    0x08000809, 0xe0701008, 0x08000809, 0xe070100c, 0x08000809, 0x7e140211, 0xd286000a, 0x04282103,
    0x7e1602f2, 0xd8340000, 0x00000b0a, 0xbf810000,
};

constexpr std::size_t PatchConstantWord = 24;
static_assert(GcnHullOnChipPassthrough[PatchConstantWord] == 0x7e1602f2u, "the patched word is v_mov_b32 v11, 1.0, the patch constant the pass-through hull writes");

alignas(256) constexpr std::array<std::uint32_t, 28> GcnHullOnChipBufferScale = [] {
    auto code = GcnHullOnChipPassthrough;
    code[PatchConstantWord] = 0x7e160c13u;
    return code;
}();

alignas(256) constexpr std::array<std::uint32_t, 45> GcnHullOnChipOutputs{
    0xbefc03c1, 0xc2c60500, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0xbf8c007f, 0x34080484,
    0xd2860005, 0x04116103, 0xd8d80000, 0x06000005, 0xd8d80004, 0x07000005, 0x7e28020f, 0xd2860014,
    0x04502503, 0xd2860014, 0x04501b02, 0xbf8c007f, 0xd8340000, 0x00000614, 0xd8340004, 0x00000714,
    0x7d840480, 0xbe86246a, 0x36100081, 0x7e100d08, 0x081010f2, 0x34120684, 0xe0701000, 0x08000809,
    0xe0701004, 0x08000809, 0xe0701008, 0x08000809, 0xe070100c, 0x08000809, 0x7e140211, 0xd286000a,
    0x04282103, 0x7e1602f2, 0xd8340000, 0x00000b0a, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 41> GcnDomainOnChipInputs{
    0xbefc03c1, 0xc2c20100, 0xbf8c007f, 0x7e080207, 0xd2860004, 0x04101502, 0xd8d80000, 0x05000004,
    0xd8d80004, 0x06000004, 0xd8d80010, 0x07000004, 0xd8d80014, 0x08000004, 0xd8d80020, 0x09000004,
    0xd8d80024, 0x0a000004, 0x7e180209, 0xd286000c, 0x04301102, 0xd8d80000, 0x0d00000c, 0xbf8c007f,
    0x081600f2, 0x0816030b, 0x101c0b00, 0x3e1c0f01, 0x3e1c130b, 0x101c1b0e, 0x101e0d00, 0x3e1e1101,
    0x3e1e150b, 0x101e1b0f, 0x7e200280, 0x7e2202f2, 0xf80008cf, 0x11100f0e, 0xf800020f, 0x11111111,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 40> GcnDomainOnChipOffsetInputs{
    0xbefc03c1, 0xc2c20100, 0xbf8c007f, 0xd2860004, 0x02001502, 0xd8d800c0, 0x05000004, 0xd8d800c4,
    0x06000004, 0xd8d800d0, 0x07000004, 0xd8d800d4, 0x08000004, 0xd8d800e0, 0x09000004, 0xd8d800e4,
    0x0a000004, 0x7e180209, 0xd286000c, 0x04301102, 0xd8d80000, 0x0d00000c, 0xbf8c007f, 0x081600f2,
    0x0816030b, 0x101c0b00, 0x3e1c0f01, 0x3e1c130b, 0x101c1b0e, 0x101e0d00, 0x3e1e1101, 0x3e1e150b,
    0x101e1b0f, 0x7e200280, 0x7e2202f2, 0xf80008cf, 0x11100f0e, 0xf800020f, 0x11111111, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 57> GcnHullOnChipComputedOutputRead{
    0xbefc03c1, 0xc2c60500, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0xbf8c007f, 0x34080484,
    0xd2860005, 0x04116103, 0xd8d80000, 0x06000005, 0xd8d80004, 0x07000005, 0x7e28020f, 0xd2860014,
    0x04502503, 0xd2860014, 0x04501b02, 0xbf8c007f, 0xd8340000, 0x00000614, 0xd8340004, 0x00000714,
    0x7d840480, 0x7e1a02f2, 0x00181a80, 0xd8340008, 0x00000c14, 0xbe86246a, 0x36100081, 0x7e100d08,
    0x081010f2, 0x34120684, 0xe0701000, 0x08000809, 0xe0701004, 0x08000809, 0xe0701008, 0x08000809,
    0xe070100c, 0x08000809, 0x261c0080, 0x341c1c84, 0x7e1e020f, 0xd286000f, 0x043c2503, 0x4a1e1d0f,
    0xd8d80008, 0x0b00000f, 0xbf8c007f, 0x7e140211, 0xd286000a, 0x04282103, 0xd8340000, 0x00000b0a,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 57> GcnHullOnChipComputedOutputReadPatchData{
    0xbefc03c1, 0xc2c60500, 0xd2900002, 0x02151101, 0xd2900003, 0x02210101, 0xbf8c007f, 0x34080484,
    0xd2860005, 0x04116103, 0xd8d80000, 0x06000005, 0xd8d80004, 0x07000005, 0x7e28020f, 0xd2860014,
    0x04502503, 0xd2860014, 0x04501b02, 0xbf8c007f, 0xd8340000, 0x00000614, 0xd8340004, 0x00000714,
    0x7d840480, 0x7e1a02f2, 0x00181a80, 0xd8340008, 0x00000c14, 0xbe86246a, 0x36100681, 0x7e100d08,
    0x081010f2, 0x34120684, 0xe0701000, 0x08000809, 0xe0701004, 0x08000809, 0xe0701008, 0x08000809,
    0xe070100c, 0x08000809, 0x261c0080, 0x341c1c84, 0x7e1e020f, 0xd286000f, 0x043c2503, 0x4a1e1d0f,
    0xd8d80008, 0x0b00000f, 0xbf8c007f, 0x7e140211, 0xd286000a, 0x04282103, 0xd8340000, 0x00000b0a,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 14> GcnDomainOnChipBelowOutputs{
    0xbefc03c1, 0xc2c20100, 0xbf8c007f, 0xd2860004, 0x02001502, 0x261c0680, 0x341c1c84, 0x4c081d04,
    0xd8d800c0, 0x05000004, 0xbf8c007f, 0xf80008cf, 0x05050505, 0xbf810000,
};

using Constants = std::array<std::uint32_t, ShaderRecompiler::TessellationConstantWords>;

alignas(16) constexpr Constants PassthroughConstants{16u, 16u, 4u, 0u, 16u, 192u, 48u, 0u, 0u};
alignas(16) constexpr Constants OutputConstants{16u, 16u, 4u, 192u, 16u, 384u, 48u, 0u, 0u};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

constexpr std::uint16_t Corner(std::uint32_t patch, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint16_t>((patch << 4u) | (x << 2u) | y);
}

alignas(256) constexpr std::array<std::uint16_t, 9> Indices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

constexpr std::array<std::uint32_t, 4> RingUserData{0x1000u, 0u, 0u, 0u};

std::array<std::uint32_t, 4> ConstantBuffer(const Constants& constants) {
    const auto address = reinterpret_cast<std::uintptr_t>(constants.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, static_cast<std::uint32_t>(sizeof(Constants)), 0x00027facu};
}

std::array<std::uint32_t, 8> HullUserData(const Constants& constants) {
    const auto buffer = ConstantBuffer(constants);
    return {RingUserData[0], RingUserData[1], RingUserData[2], RingUserData[3], buffer[0], buffer[1], buffer[2], buffer[3]};
}

TessellationConfiguration Patches(bool onChip, std::uint32_t primitiveGroupSize = 0u) {
    TessellationConfiguration configuration{3u, 3u, 1u, 0u, 2u, 4u};
    configuration.primitiveGroupSize = primitiveGroupSize;
    configuration.onChip = onChip;
    return configuration;
}

struct Stages {
    ShaderRecompiler::RecompileResult local;
    ShaderRecompiler::RecompileResult hull;
    ShaderRecompiler::RecompileResult domain;
    ShaderRecompiler::RecompileResult fragment;
};

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::uint32_t& pushCursor, const TessellationConfiguration& configuration, std::span<const std::uint32_t> stageUserData = {}, const std::optional<Constants>& constants = std::nullopt) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto userData = !stageUserData.empty() ? stageUserData : stage == ShaderStage::TessellationControl || stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(RingUserData) : std::span<const std::uint32_t>();
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
        graphics = ShaderRecompiler::GraphicsCompileContext{0u, {}, std::nullopt, configuration, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1}};
    }
    auto layout = stage == ShaderStage::Fragment ? PixelPushLayout(pushCursor, target) : ShaderRecompiler::BindingLayout{0, 0, pushCursor, AgcDriver::Graphics::PipelinePushSlotBytes - pushCursor};
    if (layout.pushConstantOffsetBytes < AgcDriver::Graphics::PipelinePushSlotBytes) layout.pushConstantSizeBytes = ShaderRecompiler::TessellationLevelPushOffsetBytes - layout.pushConstantOffsetBytes;
    ShaderRecompiler::RecompileRequest request{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0u, userData, std::nullopt, pixel, vertex, memory},
        target,
        layout,
        graphics
    };
    request.useCache = false;
    if (request.graphics) request.graphics->tessellationConstants = constants;
    auto result = ShaderRecompiler::Recompile(request);
    pushCursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return result;
}

void ExpectConstantsRequired(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::uint32_t word) {
    std::uint32_t cursor = 0;
    try {
        static_cast<void>(Compile(target, stage, code, cursor, Patches(true), userData));
    } catch (const ShaderRecompiler::TessellationConstantsRequired& required) {
        Require(required.UserDataWord() == word, "the tessellation constant buffer was asked for at user data word " + std::to_string(required.UserDataWord()) + ", expected " + std::to_string(word));
        return;
    }
    Require(false, "an on-chip GCN tessellation program that addresses LDS through the tessellation constant buffer compiled without its constants");
}

void ExpectRefusal(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, const std::string& reason, std::span<const std::uint32_t> userData = {}, const std::optional<Constants>& constants = std::nullopt) {
    std::uint32_t cursor = 0;
    try {
        static_cast<void>(Compile(target, stage, code, cursor, Patches(true), userData, constants));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, "the refusal named another reason: " + std::string(error.what()));
        return;
    }
    Require(false, "a GCN tessellation program was accepted although " + reason);
}

std::array<std::uint8_t, 4> PixelAt(std::uint32_t x, std::uint32_t y) {
    const auto offset = (static_cast<std::size_t>(y) * Width + x) * 4u;
    return {std::to_integer<std::uint8_t>(Pixels[offset]), std::to_integer<std::uint8_t>(Pixels[offset + 1]), std::to_integer<std::uint8_t>(Pixels[offset + 2]), std::to_integer<std::uint8_t>(Pixels[offset + 3])};
}

std::string PixelText(const std::array<std::uint8_t, 4>& pixel) {
    char text[64];
    std::snprintf(text, sizeof(text), "(%u, %u, %u, %u)", pixel[0], pixel[1], pixel[2], pixel[3]);
    return text;
}

std::uint32_t Column(std::uint32_t patch, float offset) {
    return static_cast<std::uint32_t>((-0.9f + 0.6f * static_cast<float>(patch) + offset + 1.0f) * 0.5f * Width);
}

std::uint32_t Row(float y) {
    return static_cast<std::uint32_t>((1.0f - y) * 0.5f * Height);
}

template<typename TPixel>
void RequireCoverage(const std::string& program, const TPixel& pixelAt, bool drawn) {
    constexpr std::array<std::pair<float, float>, 4> inside{{{0.25f, -0.267f}, {0.06f, -0.72f}, {0.44f, -0.72f}, {0.25f, 0.4f}}};
    for (std::uint32_t patch = 0; patch < 3u; ++patch) {
        const bool covered = drawn && patch != 1u;
        const auto expected = covered ? White : Background;
        for (const auto [x, y] : inside) {
            const auto pixel = pixelAt(Column(patch, x), Row(y));
            Require(pixel == expected, program + ": patch " + std::to_string(patch) + " at (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + PixelText(pixel) + ", expected " + PixelText(expected) + (covered ? " inside its tessellated triangle" : patch == 1u ? " for a patch whose tessellation factors are 0" : " for a patch whose positions are scaled to 0"));
        }
        const auto outside = pixelAt(Column(patch, 0.45f), Row(0.5f));
        Require(outside == Background, program + ": patch " + std::to_string(patch) + " covers a pixel outside its triangle: " + PixelText(outside));
    }
}

void Draw(AgcDriver::VulkanDevice& device, const Stages& stages, const TessellationConfiguration& configuration) {
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) Pixels[i + c] = std::byte{Background[c]};
    }
    std::uint32_t cursor = 0;
    const auto offset = [&](const ShaderRecompiler::RecompileResult& result) {
        const auto at = result.pushConstants.empty() ? 0u : cursor;
        cursor += static_cast<std::uint32_t>(result.pushConstants.size());
        return at;
    };
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Local, &stages.local, offset(stages.local)},
        {ShaderStage::TessellationControl, &stages.hull, offset(stages.hull)},
        {ShaderStage::TessellationEvaluation, &stages.domain, offset(stages.domain)},
        {ShaderStage::Fragment, &stages.fragment, PixelPushOffset(cursor, device.Target())},
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Tessellation, 0x45u, 64, 64, std::nullopt, configuration, 1.0f, 64.0f};
    state.stages.legacyTessellation = true;
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1, 0, true, 0, 0}, shaders);
    device.WaitIdle();
}

void RequirePatches(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> hull, std::span<const std::uint32_t> domain, bool onChip, const std::string& program, const std::optional<Constants>& constants = std::nullopt, std::uint32_t primitiveGroupSize = 0u) {
    const auto target = device.Target();
    const auto configuration = Patches(onChip, primitiveGroupSize);
    const auto hullUsers = constants ? HullUserData(*constants) : std::array<std::uint32_t, 8>{};
    const auto domainUsers = constants ? ConstantBuffer(*constants) : std::array<std::uint32_t, 4>{};
    const auto hullData = constants ? std::span<const std::uint32_t>(hullUsers) : std::span<const std::uint32_t>();
    const auto domainData = constants ? std::span<const std::uint32_t>(domainUsers) : std::span<const std::uint32_t>();
    std::uint32_t cursor = 0;
    Stages stages{
        Compile(target, ShaderStage::Local, GcnLocal, cursor, configuration),
        Compile(target, ShaderStage::TessellationControl, hull, cursor, configuration, hullData, constants),
        Compile(target, ShaderStage::TessellationEvaluation, domain, cursor, configuration, domainData, constants),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, configuration),
    };
    Draw(device, stages, configuration);
    RequireCoverage(program, PixelAt, true);
}

void Bind(AgcDriver::QueueState& queue, const void* code, std::uint32_t program, std::uint32_t resources, std::span<const std::uint32_t> users) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    queue.shader[program] = static_cast<std::uint32_t>(address >> 8u);
    queue.shader[program + 1u] = static_cast<std::uint32_t>(address >> 40u);
    queue.shader[resources] = static_cast<std::uint32_t>(users.size()) << 1u;
    for (std::size_t i = 0; i < users.size(); ++i) queue.shader[resources + 1u + static_cast<std::uint32_t>(i)] = users[i];
}

void RequireProgramDecode() {
    using namespace AgcDriver::DriverDetail;
    using ShaderRecompiler::ProgramRole;
    constexpr std::array<std::uint32_t, 2> localUsers{0x11110001u, 0x11110002u};
    constexpr std::array<std::uint32_t, 4> hullUsers{0x22220001u, 0x22220002u, 0x22220003u, 0x22220004u};
    constexpr std::array<std::uint32_t, 3> domainUsers{0x33330001u, 0x33330002u, 0x33330003u};
    AgcDriver::QueueState queue{};
    queue.context[0x31c] = 10u << 2u;
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    Bind(queue, GcnLocal.data(), 0x148u, 0x14bu, localUsers);
    Bind(queue, GcnHullOffChip.data(), 0x108u, 0x10bu, hullUsers);
    Bind(queue, GcnDomainOffChip.data(), 0x048u, 0x04bu, domainUsers);
    Bind(queue, PixelCode.data(), 0x008u, 0x00bu, {});
    DrawDecode draw{};
    draw.state.stages = {AgcDriver::Graphics::ShaderPath::Tessellation, 0x45u, 64u, 64u, std::nullopt, Patches(false), 1.0f, 64.0f};
    draw.state.stages.legacyTessellation = true;
    draw.pixel.interpolatorCount = 1;
    draw.pixel.interpolatorSettings[0] = 0x400u;
    draw.pixel.targetOutputMode[0] = 9;
    draw.pixel.targetExportMapping.fill(0xe4u);
    const ShaderRegistry registry;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    const std::vector<ProgramRole> roles{ProgramRole::Local, ProgramRole::Hull, ProgramRole::Domain, ProgramRole::Fragment};
    Require(draw.roles == roles, "the GCN LS, HS and VS-stage domain programs did not decode as the local, hull and domain roles");
    const std::array<ShaderStage, 4> stages{ShaderStage::Local, ShaderStage::TessellationControl, ShaderStage::TessellationEvaluation, ShaderStage::Fragment};
    const std::array<const void*, 4> codes{GcnLocal.data(), GcnHullOffChip.data(), GcnDomainOffChip.data(), PixelCode.data()};
    const std::array<std::span<const std::uint32_t>, 4> users{localUsers, hullUsers, domainUsers, {}};
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        const auto name = "GCN program " + std::to_string(index);
        Require(program.binary.stage == stages[index] && program.binary.codeAddress == reinterpret_cast<std::uintptr_t>(codes[index]), name + " was read from another program register");
        Require(program.firstUserSgpr == 0u && program.hiddenUserWords == 0u, name + " places its user data after hidden words");
        Require(std::ranges::equal(program.userData, users[index]), name + " did not read its own stage's user data");
    }
    Require(!draw.state.stages.tessellation->mergedHull, "a GCN hull program decoded as a merged hull");
}

constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::size_t ColorBytes = Width * Height * 4u;
constexpr std::size_t ConstantsOffset = 0x14000;
constexpr std::size_t IndicesOffset = 0x14100;
constexpr std::size_t BlockBytes = 0x20000;
static_assert(ColorBytes <= ConstantsOffset, "the color target overlaps the tessellation constant buffer");

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
    }

    ~LowBlock() {
        if (data == nullptr) return;
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

    bool Usable() const { return data != nullptr && At(BlockBytes) <= GcnAddressLimit; }
    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)) + offset; }
    std::uint8_t* Bytes(std::size_t offset) const { return data + offset; }

private:
    std::uint8_t* data = nullptr;
};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::span<const std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1u) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload.begin(), payload.end());
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    Append(commands, opcode, std::span<const std::uint32_t>(payload.begin(), payload.size()));
}

void SetProgram(std::vector<std::uint32_t>& commands, std::uint32_t program, const void* code, std::span<const std::uint32_t> users) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    Append(commands, 0x76, {program, static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)});
    Append(commands, 0x76, {program + 2u, 0u, static_cast<std::uint32_t>(users.size()) << 1u});
    if (users.empty()) return;
    std::vector<std::uint32_t> payload{program + 4u};
    payload.insert(payload.end(), users.begin(), users.end());
    Append(commands, 0x76, payload);
}

void SubmitOnChipDraw(const LowBlock& block) {
    for (std::size_t i = 0; i < ColorBytes; i += 4) std::memcpy(block.Bytes(i), Background.data(), Background.size());
    const auto color = block.At(0);
    const auto constants = block.At(ConstantsOffset);
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(constants), static_cast<std::uint32_t>(constants >> 32u) & 0xffffu, static_cast<std::uint32_t>(sizeof(Constants)), 0x00027facu};
    const std::array<std::uint32_t, 8> hullUsers{RingUserData[0], RingUserData[1], RingUserData[2], RingUserData[3], buffer[0], buffer[1], buffer[2], buffer[3]};
    const auto extent = (Height << 16u) | Width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x45}, {0x2d6, (3u << 14u) | (3u << 8u) | 4u}, {0x2db, (2u << 5u) | 1u},
        {0x286, std::bit_cast<std::uint32_t>(64.0f)}, {0x287, std::bit_cast<std::uint32_t>(1.0f)},
        {0x1b6, 1}, {0x191, 0x400}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x319, Width / 8u - 1u}, {0x31a, Width * Height / 64u - 1u},
        {0x31b, 0}, {0x31c, (1u << 15u) | (10u << 2u)}, {0x31d, 8}, {0x1e0, 0},
        {0xc, 0}, {0xd, extent}, {0x81, 0x80000000}, {0x82, extent},
        {0x90, 0x80000000}, {0x91, extent}, {0x94, 0x80000000}, {0x95, extent},
        {0x10f, std::bit_cast<std::uint32_t>(Width * 0.5f)}, {0x110, std::bit_cast<std::uint32_t>(Width * 0.5f)},
        {0x111, std::bit_cast<std::uint32_t>(Height * -0.5f)}, {0x112, std::bit_cast<std::uint32_t>(Height * 0.5f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 9});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    SetProgram(commands, 0x148u, GcnLocal.data(), {});
    SetProgram(commands, 0x108u, GcnHullOnChipBufferScale.data(), hullUsers);
    SetProgram(commands, 0x048u, GcnDomainOnChipInputs.data(), buffer);
    SetProgram(commands, 0x008u, PixelCode.data(), {});
    const auto indices = block.At(IndicesOffset);
    Append(commands, 0x2a, {0});
    Append(commands, 0x27, {static_cast<std::uint32_t>(Indices.size()), static_cast<std::uint32_t>(indices), static_cast<std::uint32_t>(indices >> 32u), static_cast<std::uint32_t>(Indices.size()), 0});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the on-chip GCN tessellation draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, ColorBytes);
}

void RequireRewrittenConstants(const LowBlock& block) {
    std::memcpy(block.Bytes(IndicesOffset), Indices.data(), sizeof(Indices));
    const auto pixelAt = [&](std::uint32_t x, std::uint32_t y) {
        const auto* pixel = block.Bytes((static_cast<std::size_t>(y) * Width + x) * 4u);
        return std::array<std::uint8_t, 4>{pixel[0], pixel[1], pixel[2], pixel[3]};
    };
    auto constants = PassthroughConstants;
    for (const std::uint32_t scale : {1u, 0u, 1u}) {
        constants[7] = scale;
        std::memcpy(block.Bytes(ConstantsOffset), constants.data(), sizeof(constants));
        SubmitOnChipDraw(block);
        RequireCoverage("GCN on-chip draw through the driver after word 7 of its tessellation constant buffer, which the hull writes as the patch constant the domain shader scales positions by, was rewritten to " + std::to_string(scale), pixelAt, scale != 0u);
    }
}

}

int main() {
    try {
        ShaderRecompiler::SetGcnGuestShaders(true);
        RequireProgramDecode();
        ShaderRecompiler::SetGcnGuestShaders(false);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!target.tessellation.has_value()) {
            std::puts("GCN tessellation: skipped, the device has no tessellation shaders");
            return VulkanTestSkipped;
        }
        ShaderRecompiler::SetGcnGuestShaders(true);
        RequirePatches(*device, GcnHullOffChip, GcnDomainOffChip, false, "GCN off-chip LS-HS-VS draw");
        RequirePatches(*device, GcnHullOnChip, GcnDomainOnChip, true, "GCN on-chip LS-HS-VS draw");
        ExpectRefusal(target, ShaderStage::TessellationControl, GcnHullOnChipLds, "keeps its outputs on chip");
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, GcnDomainOnChipLds, "reads LDS at an address that does not come from the tessellation constant buffer");
        RequirePatches(*device, GcnHullOnChipPassthrough, GcnDomainOnChipInputs, true, "GCN on-chip pass-through LS-HS-VS draw through LDS", PassthroughConstants);
        RequirePatches(*device, GcnHullOnChipOutputs, GcnDomainOnChipInputs, true, "GCN on-chip LS-HS-VS draw with hull output control points in LDS", OutputConstants);
        RequirePatches(*device, GcnHullOnChipOutputs, GcnDomainOnChipOffsetInputs, true, "GCN on-chip LS-HS-VS draw whose domain shader reaches the hull output control points through its LDS instruction offset", OutputConstants);
        RequirePatches(*device, GcnHullOnChipComputedOutputRead, GcnDomainOnChipInputs, true, "GCN on-chip LS-HS-VS draw whose hull reads its patch's output control point 0 at a computed offset", OutputConstants);
        RequirePatches(*device, GcnHullOnChipComputedOutputReadPatchData, GcnDomainOnChipInputs, true, "GCN on-chip LS-HS-VS draw, with GE_CNTL.PRIM_GRP_SIZE 4, whose hull takes its factors from its relative patch index and reads its patch's output control point 0 at a computed offset", OutputConstants, 4u);
        const auto hullUsers = HullUserData(PassthroughConstants);
        const auto domainUsers = ConstantBuffer(PassthroughConstants);
        ExpectConstantsRequired(target, ShaderStage::TessellationControl, GcnHullOnChipPassthrough, hullUsers, 4u);
        ExpectConstantsRequired(target, ShaderStage::TessellationEvaluation, GcnDomainOnChipInputs, domainUsers, 0u);
        const auto outputDomainUsers = ConstantBuffer(OutputConstants);
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, GcnDomainOnChipBelowOutputs, "at a computed offset that can fall below the hull shader's on-chip output control points", outputDomainUsers, OutputConstants);
        auto narrowOutputs = target;
        narrowOutputs.tessellation->maxControlPerVertexOutputComponents = 2u;
        ExpectRefusal(narrowOutputs, ShaderStage::TessellationControl, GcnHullOnChipPassthrough, "the pass-through hull shader's control points exceed the device limits (4 input and 4 output components per control point", hullUsers, PassthroughConstants);
        auto narrowInputs = target;
        narrowInputs.tessellation->maxControlPerVertexInputComponents = 2u;
        ExpectRefusal(narrowInputs, ShaderStage::TessellationControl, GcnHullOnChipPassthrough, "the pass-through hull shader's control points exceed the device limits (4 input and 4 output components per control point", hullUsers, PassthroughConstants);
        ShaderRecompiler::SetGcnGuestShaders(false);
        device.reset();
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("GCN tessellation: skipped, no guest block below the 40-bit GCN color base range (0x%llx) for the constant buffer rewrites\n", static_cast<unsigned long long>(block.At(0)));
            return VulkanTestSkipped;
        }
        AgcDriver::DriverDetail::Driver::Get().WaitForPipelineJournal();
        ShaderRecompiler::SetGcnGuestShaders(true);
        try {
            RequireRewrittenConstants(block);
        } catch (...) {
            ShaderRecompiler::SetGcnGuestShaders(false);
            AgcDriverShutdown_nid_postfix();
            throw;
        }
        ShaderRecompiler::SetGcnGuestShaders(false);
        AgcDriverShutdown_nid_postfix();
        std::puts("GCN tessellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
