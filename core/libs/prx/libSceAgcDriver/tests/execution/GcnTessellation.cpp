#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
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

TessellationConfiguration Patches(bool onChip) {
    TessellationConfiguration configuration{3u, 3u, 1u, 0u, 2u, 4u};
    configuration.onChip = onChip;
    return configuration;
}

struct Stages {
    ShaderRecompiler::RecompileResult local;
    ShaderRecompiler::RecompileResult hull;
    ShaderRecompiler::RecompileResult domain;
    ShaderRecompiler::RecompileResult fragment;
};

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::uint32_t& pushCursor, const TessellationConfiguration& configuration) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto userData = stage == ShaderStage::TessellationControl || stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(RingUserData) : std::span<const std::uint32_t>();
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
    auto result = ShaderRecompiler::Recompile(request);
    pushCursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return result;
}

void ExpectRefusal(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, const std::string& reason) {
    std::uint32_t cursor = 0;
    try {
        static_cast<void>(Compile(target, stage, code, cursor, Patches(true)));
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

void RequirePatches(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> hull, std::span<const std::uint32_t> domain, bool onChip, const std::string& program) {
    const auto target = device.Target();
    const auto configuration = Patches(onChip);
    std::uint32_t cursor = 0;
    Stages stages{
        Compile(target, ShaderStage::Local, GcnLocal, cursor, configuration),
        Compile(target, ShaderStage::TessellationControl, hull, cursor, configuration),
        Compile(target, ShaderStage::TessellationEvaluation, domain, cursor, configuration),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, configuration),
    };
    Draw(device, stages, configuration);
    constexpr std::array<std::pair<float, float>, 4> inside{{{0.25f, -0.267f}, {0.06f, -0.72f}, {0.44f, -0.72f}, {0.25f, 0.4f}}};
    for (std::uint32_t patch = 0; patch < 3u; ++patch) {
        const auto expected = patch == 1u ? Background : White;
        for (const auto [x, y] : inside) {
            const auto pixel = PixelAt(Column(patch, x), Row(y));
            Require(pixel == expected, program + ": patch " + std::to_string(patch) + " at (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + PixelText(pixel) + ", expected " + PixelText(expected) + (patch == 1u ? " for a patch whose tessellation factors are 0" : " inside its tessellated triangle"));
        }
        const auto outside = PixelAt(Column(patch, 0.45f), Row(0.5f));
        Require(outside == Background, program + ": patch " + std::to_string(patch) + " covers a pixel outside its triangle: " + PixelText(outside));
    }
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
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, GcnDomainOnChipLds, "reads them from LDS");
        ShaderRecompiler::SetGcnGuestShaders(false);
        device.reset();
        std::puts("GCN tessellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
