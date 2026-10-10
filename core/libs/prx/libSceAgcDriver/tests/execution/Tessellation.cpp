#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 80;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

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

alignas(256) constexpr std::array<std::uint32_t, 51> HullRestoredExecSaved{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xbe94047e, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000,
    0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 30> DomainCode{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02,
    0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280,
    0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalIndexAsData{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000030a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 46> HullUnguardedFactors{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xe0781000, 0x0403180a, 0x4a162481,
    0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 47> HullSquaredAddress{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x16002914, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013,
    0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullAndSaveexec{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0x7d8a2887, 0xbe94246a,
    0xe0781000, 0x02020216, 0x7da82881, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 36> DomainPatchRead{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0x4a060e81, 0x16060690, 0x4c0606ff, 0x00008000, 0xe0341000, 0x04030c03,
    0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06,
    0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf,
    0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 32> DomainCulledPrimitive{
    0x380000ff, 0x80000000, 0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01,
    0xe0341010, 0x04030c01, 0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505,
    0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff,
    0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

constexpr ShaderRecompiler::TessellationConfiguration Patches{3u, 3u, 1u, 2u, 2u};
constexpr std::array<std::uint32_t, 3> PatchLevels{1u, 0u, 5u};

constexpr std::uint16_t Corner(std::uint32_t patch, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint16_t>((PatchLevels[patch] << 8u) | (patch << 4u) | (x << 2u) | y);
}

alignas(256) constexpr std::array<std::uint16_t, 9> Indices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

struct Stages {
    ShaderRecompiler::RecompileResult local;
    ShaderRecompiler::RecompileResult hull;
    ShaderRecompiler::RecompileResult domain;
    ShaderRecompiler::RecompileResult fragment;
};

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::uint32_t& pushCursor) {
    static const std::array<std::uint32_t, 16> hullUserData{0x1000u};
    static const std::array<std::uint32_t, 8> domainUserData{};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const bool hull = stage == ShaderStage::TessellationControl;
    const auto userData = hull ? std::span<const std::uint32_t>(hullUserData) : stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(domainUserData) : std::span<const std::uint32_t>();
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
        graphics = ShaderRecompiler::GraphicsCompileContext{hull ? 0u : 8u, {}, std::nullopt, Patches, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1}};
    }
    const ShaderRecompiler::RecompileRequest request{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, hull ? 0u : 8u, userData, std::nullopt, pixel, vertex, memory},
        target,
        {0, 0, pushCursor, AgcDriver::Graphics::PipelinePushConstantBytes - pushCursor},
        graphics
    };
    auto result = ShaderRecompiler::Recompile(request);
    pushCursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return result;
}

void ExpectRefusal(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, const std::string& reason) {
    std::uint32_t cursor = 0;
    try {
        static_cast<void>(Compile(target, stage, code, cursor));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, "the refusal named another reason: " + std::string(error.what()));
        return;
    }
    Require(false, "a tessellation program was accepted although " + reason);
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

void Draw(AgcDriver::VulkanDevice& device, const Stages& stages, VkCullModeFlags cull, VkFrontFace front) {
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
        {ShaderStage::Fragment, &stages.fragment, offset(stages.fragment)},
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Tessellation, 0x0200210du, 64, 64, std::nullopt, Patches};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = cull;
    state.frontFace = front;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1, 0, true}, shaders);
    device.WaitIdle();
}

std::uint32_t CoveredPixels(std::uint32_t patch, bool interior) {
    std::uint32_t count = 0;
    for (std::uint32_t y = Row(0.5f); y <= Row(-0.5f) && y < Height; ++y) {
        for (std::uint32_t x = Column(patch, 0.0f); x <= Column(patch, 0.5f) && x < Width; ++x) {
            const auto pixel = PixelAt(x, y);
            if (pixel == Background) continue;
            Require(pixel[0] == 255 && pixel[2] == 0 && pixel[3] == 255, "patch " + std::to_string(patch) + " pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + PixelText(pixel));
            if (!interior || pixel[1] > 32) ++count;
        }
    }
    return count;
}

void RequirePatches(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> hull, const std::string& program) {
    std::uint32_t cursor = 0;
    const Stages stages{
        Compile(target, ShaderStage::Local, LocalCode, cursor),
        Compile(target, ShaderStage::TessellationControl, hull, cursor),
        Compile(target, ShaderStage::TessellationEvaluation, DomainCode, cursor),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor),
    };
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    for (const auto cull : {VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT}) {
        Draw(device, stages, cull, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        const std::string name = program + (cull == VK_CULL_MODE_NONE ? ", unculled" : ", back-face culled");
        Require(CoveredPixels(0, false) > 100 && CoveredPixels(0, true) == 0, name + ": the level-1 patch was not drawn as one flat triangle, centre " + PixelText(PixelAt(Column(0, 0.25f), center)));
        Require(CoveredPixels(1, false) == 0, name + ": the patch with level 0 was not culled");
        Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), center)[1] > 32, name + ": the level-5 patch has no interior domain points at its centre, " + PixelText(PixelAt(Column(2, 0.25f), center)));
        Require(PixelAt(0, 0) == Background && PixelAt(Width - 1, Height - 1) == Background, name + ": the corners changed");
    }
    Draw(device, stages, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    Require(CoveredPixels(0, false) == 0 && CoveredPixels(2, false) == 0, program + ": tessellated triangles did not keep the control points' winding");
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
    stage.specials.vgt_gs_out_prim_type = {0x29bu, 0u};
    stage.specials.ge_user_vgpr_en = {0x262u, 0u};
    stage.shader.specials = &stage.specials;
}

void RequireRegisteredStages() {
    const auto low = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 8u); };
    const auto high = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 40u); };
    RegisteredStage local;
    RegisteredStage hull;
    RegisteredStage domain;
    Describe(local, LocalCode, 5u, {{0x10au, 0u}, {0x10bu, 0u}}, {}, 0u);
    Describe(hull, HullCode, 7u, {{0x148u, 0u}, {0x149u, 0u}, {0x10au, 0u}, {0x10bu, 16u}, {0x108u, low(HullCode)}, {0x109u, high(HullCode)}}, {{0x2dbu, 0x40049u}, {0x286u, 0x42800000u}, {0x287u, 0x3f800000u}, {0x2d6u, 0xc355u}}, 0x105u);
    Describe(domain, DomainCode, 2u, {{0xc8u, low(DomainCode)}, {0xc9u, high(DomainCode)}, {0x8au, 0u}, {0x8bu, 16u}}, {}, 0x02002008u);
    for (const auto* stage : {&local, &hull, &domain}) AgcDriverRegisterShader_nid_postfix(&stage->shader);
    const std::array<const Shader*, 2> stages{&hull.shader, &domain.shader};
    const std::array<ShaderRegister, 2> context{{{0x2d5u, 0x0200210du}, {0x29bu, 0u}}};
    const std::array<ShaderRegister, 3> primitive{{{0x25bu, 0u}, {0x262u, 0u}, {0x242u, 9u}}};
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
    AgcDriverShutdown_nid_postfix();
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!target.tessellation.has_value()) {
            std::puts("Tessellation tests skipped: the device has no tessellation shaders");
            return VulkanTestSkipped;
        }
        ExpectRefusal(target, ShaderStage::TessellationControl, HullUnguardedFactors, "not written by control point 0 alone");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullSquaredAddress, "not affine");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullAndSaveexec, "lane mask");
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainPatchRead, "patch constants");
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainCulledPrimitive, "passthrough primitive");
        ExpectRefusal(target, ShaderStage::Local, LocalIndexAsData, "outside a tessellation ring or LDS address");

        RequirePatches(*device, target, HullCode, "hull program");
        RequirePatches(*device, target, HullRestoredExecSaved, "hull program saving its restored exec");
        device.reset();
        RequireRegisteredStages();
        std::puts("Tessellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
