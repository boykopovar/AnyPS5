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
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::GeometryConfiguration;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 64;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 9> GcnExport{
    0x7e020d00, 0x060402f1, 0x100602f1, 0x060606f2, 0xe0704000, 0x04400200, 0xe0704004, 0x04400300,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 63> GcnGeometry{
    0x34100082, 0xe0305000, 0x80400908, 0xb0140100, 0xe0305000, 0x14400a08, 0xbf8c0000, 0x061612ff,
    0xbe800000, 0x061812ff, 0x3e800000, 0x7e1a02ff, 0xbe800000, 0x7e1c02ff, 0x3e800000, 0x7e1e0280,
    0x7e2002f2, 0xe0704000, 0x08410b00, 0xe0704010, 0x08410d00, 0xe0704020, 0x08410f00, 0xe0704030,
    0x08411000, 0xe0704040, 0x08410a00, 0xbf900022, 0xe0704004, 0x08410c00, 0xe0704014, 0x08410d00,
    0xe0704024, 0x08410f00, 0xe0704034, 0x08411000, 0xe0704044, 0x08410a00, 0xbf900022, 0xe0704008,
    0x08410b00, 0xe0704018, 0x08410e00, 0xe0704028, 0x08410f00, 0xe0704038, 0x08411000, 0xe0704048,
    0x08410a00, 0xbf900022, 0xe070400c, 0x08410c00, 0xe070401c, 0x08410e00, 0xe070402c, 0x08410f00,
    0xe070403c, 0x08411000, 0xe070404c, 0x08410a00, 0xbf900022, 0xbf900012, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 20> GcnCopy{
    0x34020082, 0xe0305000, 0x80400201, 0xe0305100, 0x80400301, 0xe0305200, 0x80400401, 0xb0040300,
    0xe0305000, 0x04400501, 0xe0305400, 0x80400601, 0xbf8c0000, 0x7e0e0280, 0x7e1002f2, 0xf80008cf,
    0x05040302, 0xf800020f, 0x08070706, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 15> GcnCopyFolded{
    0x34020082, 0xb0040200, 0xb7840100, 0x8f058881, 0x92068a81, 0xe0305000, 0x04400201, 0xe0305000,
    0x05400301, 0xe0305000, 0x06400401, 0xbf8c0000, 0xf80008cf, 0x02040302, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> GcnCopyBranch{
    0x34020082, 0xe0305000, 0x80400201, 0xbf880001, 0xbf8c0000, 0xf80008cf, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> GcnCopyExec{
    0x34020082, 0xe0305000, 0x80400201, 0xbefe04c1, 0xbf8c0000, 0xf80008cf, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> GcnCopyMovedIndex{
    0x4a000081, 0x34020082, 0xe0305000, 0x80400201, 0xbf8c0000, 0xf80008cf, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> GcnCopyStreams{
    0x34000082, 0x900a9802, 0xb48a0000, 0xbf840005, 0xe0305000, 0x80400200, 0xbf8c0000, 0xf80008cf, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> GcnCopyComputed{
    0x34020082, 0xe0305000, 0x80400201, 0xbf8c0000, 0x060604f2, 0xf80008cf, 0x02020302, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint16_t, 2> Indices{0u, 1u};

constexpr std::array<std::uint32_t, 4> RingUserData{0x1000u, 0u, 0u, 0u};
constexpr std::array<std::uint32_t, 8> GeometryUserData{0x1000u, 0u, 0u, 0u, 0x2000u, 0u, 0u, 0u};

GeometryConfiguration Points() {
    GeometryConfiguration geometry;
    geometry.inputVertices = 1u;
    geometry.outputTopology = 2u;
    geometry.maxVertices = 4u;
    geometry.instances = 1u;
    geometry.esgsItemDwords = 2u;
    geometry.vertexDwords = 5u;
    geometry.exports = ShaderRecompiler::ParseGeometryCopyProgram(GcnCopy, geometry.maxVertices, geometry.vertexDwords, std::nullopt);
    return geometry;
}

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::uint32_t& pushCursor, const GeometryConfiguration& geometry) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
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
        graphics = ShaderRecompiler::GraphicsCompileContext{0u, {}, std::nullopt, std::nullopt, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1}, std::nullopt, std::nullopt, geometry};
    }
    const auto layout = stage == ShaderStage::Fragment ? PixelPushLayout(pushCursor, target) : ShaderRecompiler::BindingLayout{0, 0, pushCursor, AgcDriver::Graphics::PipelinePushSlotBytes - pushCursor};
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

std::array<std::uint8_t, 4> PixelAt(float x, float y) {
    const auto column = static_cast<std::uint32_t>((x + 1.0f) * 0.5f * Width);
    const auto row = static_cast<std::uint32_t>((1.0f - y) * 0.5f * Height);
    const auto offset = (static_cast<std::size_t>(row) * Width + column) * 4u;
    return {std::to_integer<std::uint8_t>(Pixels[offset]), std::to_integer<std::uint8_t>(Pixels[offset + 1]), std::to_integer<std::uint8_t>(Pixels[offset + 2]), std::to_integer<std::uint8_t>(Pixels[offset + 3])};
}

std::string PixelText(const std::array<std::uint8_t, 4>& pixel) {
    char text[64];
    std::snprintf(text, sizeof(text), "(%u, %u, %u, %u)", pixel[0], pixel[1], pixel[2], pixel[3]);
    return text;
}

void RequireQuads(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    const auto geometry = Points();
    std::uint32_t cursor = 0;
    auto exporter = Compile(target, ShaderStage::Vertex, GcnExport, RingUserData, cursor, geometry);
    auto program = Compile(target, ShaderStage::Geometry, GcnGeometry, GeometryUserData, cursor, geometry);
    auto fragment = Compile(target, ShaderStage::Fragment, PixelCode, {}, cursor, geometry);
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) Pixels[i + c] = std::byte{Background[c]};
    }
    std::uint32_t offset = 0;
    const auto push = [&](const ShaderRecompiler::RecompileResult& result) {
        const auto at = result.pushConstants.empty() ? 0u : offset;
        offset += static_cast<std::uint32_t>(result.pushConstants.size());
        return at;
    };
    const std::array<AgcDriver::Graphics::CompiledShader, 3> shaders{{
        {ShaderStage::Vertex, &exporter, push(exporter)},
        {ShaderStage::Geometry, &program, push(program)},
        {ShaderStage::Fragment, &fragment, PixelPushOffset(offset, target)},
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0xb0u, 64, 64, std::nullopt, std::nullopt, 1.0f, 64.0f};
    state.stages.legacyGeometry = geometry;
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
    const auto first = PixelAt(-0.5f, 0.0f);
    Require(first == std::array<std::uint8_t, 4>{255u, 0u, 0u, 255u}, "the first point's quad is " + PixelText(first) + ", expected (255, 0, 0, 255)");
    const auto second = PixelAt(0.5f, 0.0f);
    Require(second[0] >= 127u && second[0] <= 128u && second[1] == 0u && second[2] == 0u && second[3] == 255u, "the second point's quad is " + PixelText(second) + ", expected half red");
    for (const auto [x, y] : {std::pair{0.0f, 0.0f}, std::pair{-0.5f, 0.4f}, std::pair{0.5f, -0.4f}, std::pair{-0.9f, 0.0f}}) {
        const auto outside = PixelAt(x, y);
        Require(outside == Background, "a pixel outside both quads at (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + PixelText(outside));
    }
}

void RequireCopyLayout() {
    const auto exports = ShaderRecompiler::ParseGeometryCopyProgram(GcnCopy, 4u, 5u, std::nullopt);
    Require(exports.size() == 2u, "the copy program did not yield a position and a parameter export");
    Require(exports[0].target == 0x0cu && exports[0].enable == 0xfu && exports[0].done && exports[0].constants == 0u && exports[0].values == std::array<std::uint32_t, 4>{0u, 1u, 2u, 3u}, "the position export did not read GSVS dwords 0-3");
    Require(exports[1].target == 0x20u && exports[1].constants == 0xeu && exports[1].values == std::array<std::uint32_t, 4>{4u, 0u, 0u, 0x3f800000u}, "the parameter export did not read GSVS dword 4 beside the constants 0, 0 and 1.0");
    const auto folded = ShaderRecompiler::ParseGeometryCopyProgram(GcnCopyFolded, 4u, 5u, std::nullopt);
    Require(folded.size() == 1u && folded[0].constants == 0u && folded[0].values == std::array<std::uint32_t, 4>{3u, 1u, 4u, 3u}, "scalar offsets built by s_addk_i32, s_lshl_b32 and s_bfm_b32 did not select GSVS dwords 3, 1 and 4");
    bool refused = false;
    try {
        static_cast<void>(ShaderRecompiler::ParseGeometryCopyProgram(GcnCopyComputed, 4u, 5u, std::nullopt));
    } catch (const std::exception& error) {
        refused = std::string(error.what()).find("an exported value other than a GSVS ring dword or a constant") != std::string::npos;
    }
    Require(refused, "a copy program that exports a computed value was accepted");
    refused = false;
    try {
        static_cast<void>(ShaderRecompiler::ParseGeometryCopyProgram(GcnCopy, 3u, 5u, std::nullopt));
    } catch (const std::exception& error) {
        refused = std::string(error.what()).find("which is not one of the 5 vertex dwords") != std::string::npos;
    }
    Require(refused, "a copy program read at a slot stride other than VGT_GS_MAX_VERT_OUT times 64 bytes was accepted");
    const auto streams = ShaderRecompiler::ParseGeometryCopyProgram(GcnCopyStreams, 4u, 5u, 2u);
    Require(streams.size() == 1u && streams[0].constants == 0u && streams[0].values == std::array<std::uint32_t, 4>{0u, 0u, 0u, 0u}, "the stream 0 block of a copy program that selects its stream from the stream-out configuration SGPR was not read");
    refused = false;
    try {
        static_cast<void>(ShaderRecompiler::ParseGeometryCopyProgram(GcnCopyStreams, 4u, 5u, std::nullopt));
    } catch (const std::exception& error) {
        refused = std::string(error.what()).find("a copy program branch on a condition not known at draw time") != std::string::npos;
    }
    Require(refused, "a copy program branch on the stream ID was followed without the stream-out configuration SGPR");
    for (const auto& [code, reason] : {std::pair{std::span<const std::uint32_t>(GcnCopyBranch), "control flow other than a scalar condition in a copy program"}, std::pair{std::span<const std::uint32_t>(GcnCopyExec), "a copy program that writes EXEC"}, std::pair{std::span<const std::uint32_t>(GcnCopyMovedIndex), "not addressed by the vertex index times 4"}}) {
        refused = false;
        try {
            static_cast<void>(ShaderRecompiler::ParseGeometryCopyProgram(code, 4u, 5u, std::nullopt));
        } catch (const std::exception& error) {
            refused = std::string(error.what()).find(reason) != std::string::npos;
        }
        Require(refused, std::string("a copy program was accepted that should fail with: ") + reason);
    }
}

void Bind(AgcDriver::QueueState& queue, const void* code, std::uint32_t program, std::uint32_t resources, std::span<const std::uint32_t> users) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    queue.shader[program] = static_cast<std::uint32_t>(address >> 8u);
    queue.shader[program + 1u] = static_cast<std::uint32_t>(address >> 40u);
    queue.shader[resources] = static_cast<std::uint32_t>(users.size()) << 1u;
    for (std::size_t i = 0; i < users.size(); ++i) queue.shader[resources + 1u + static_cast<std::uint32_t>(i)] = users[i];
}

AgcDriver::QueueState GeometryQueue() {
    AgcDriver::QueueState queue{};
    queue.context[0x2d5] = 0xb0u;
    queue.context[0x1b6] = 0u;
    queue.context[0x290] = 0x00180033u;
    queue.context[0x29b] = 2u;
    queue.context[0x2ab] = 2u;
    queue.context[0x2ac] = 20u;
    queue.context[0x2ce] = 4u;
    queue.context[0x2d7] = 5u;
    queue.context[0x2e4] = 0u;
    queue.userConfig[0x242] = 1u;
    queue.shader[0xca] = 0u;
    return queue;
}

void RequireStageDecode() {
    auto queue = GeometryQueue();
    const auto stages = AgcDriver::Graphics::DecodeShaderStages(queue);
    Require(stages.path == AgcDriver::Graphics::ShaderPath::Vertex && stages.legacyGeometry.has_value() && !stages.legacyVertex && !stages.mesh, "VGT_SHADER_STAGES_EN 0xb0 did not decode as GCN ES-GS-VS routing");
    const auto& geometry = *stages.legacyGeometry;
    Require(geometry.inputVertices == 1u && geometry.outputTopology == 2u && geometry.maxVertices == 4u && geometry.instances == 1u && geometry.esgsItemDwords == 2u && geometry.vertexDwords == 5u, "the GCN geometry registers decoded to another configuration");
    const auto refuses = [&](std::uint32_t offset, std::uint32_t value, const std::string& reason) {
        auto refused = GeometryQueue();
        refused.context[offset] = value;
        try {
            static_cast<void>(AgcDriver::Graphics::DecodeShaderStages(refused));
        } catch (const std::exception& error) {
            Require(std::string(error.what()).find(reason) != std::string::npos, "the refusal named another reason: " + std::string(error.what()));
            return;
        }
        Require(false, "a GCN geometry draw was accepted with register 0x" + std::to_string(offset) + " = " + std::to_string(value));
    };
    refuses(0x2e5, 1u, "stream-out");
    refuses(0x290, 0x00200033u, "VGT_GS_MODE");
    refuses(0x2d8, 4u, "vertex streams 1-3");
    refuses(0x29b, 0u, "VGT_GS_OUT_PRIM_TYPE");
    refuses(0x2ac, 19u, "VGT_GSVS_RING_ITEMSIZE");
}

void RequireProgramDecode() {
    using namespace AgcDriver::DriverDetail;
    using ShaderRecompiler::ProgramRole;
    constexpr std::array<std::uint32_t, 4> exportUsers{0x11110001u, 0x11110002u, 0x11110003u, 0x11110004u};
    constexpr std::array<std::uint32_t, 8> geometryUsers{0x22220001u, 0x22220002u, 0x22220003u, 0x22220004u, 0x22220005u, 0x22220006u, 0x22220007u, 0x22220008u};
    constexpr std::array<std::uint32_t, 4> copyUsers{0x33330001u, 0x33330002u, 0x33330003u, 0x33330004u};
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    Bind(queue, GcnExport.data(), 0x0c8u, 0x0cbu, exportUsers);
    Bind(queue, GcnGeometry.data(), 0x088u, 0x08bu, geometryUsers);
    Bind(queue, GcnCopy.data(), 0x048u, 0x04bu, copyUsers);
    Bind(queue, PixelCode.data(), 0x008u, 0x00bu, {});
    DrawDecode draw{};
    draw.state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0xb0u, 64u, 64u, std::nullopt, std::nullopt, 1.0f, 64.0f};
    auto geometry = Points();
    geometry.exports.clear();
    draw.state.stages.legacyGeometry = geometry;
    draw.pixel.interpolatorCount = 1;
    draw.pixel.interpolatorSettings[0] = 0x400u;
    draw.pixel.targetOutputMode[0] = 9;
    draw.pixel.targetExportMapping.fill(0xe4u);
    const ShaderRegistry registry;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    const std::vector<ProgramRole> roles{ProgramRole::Main, ProgramRole::Geometry, ProgramRole::Copy, ProgramRole::Fragment};
    Require(draw.roles == roles, "the GCN ES, GS, copy VS and PS programs did not decode as the export, geometry, copy and fragment roles");
    const std::array<ShaderStage, 4> stages{ShaderStage::Vertex, ShaderStage::Geometry, ShaderStage::Vertex, ShaderStage::Fragment};
    const std::array<const void*, 4> codes{GcnExport.data(), GcnGeometry.data(), GcnCopy.data(), PixelCode.data()};
    const std::array<std::span<const std::uint32_t>, 4> users{exportUsers, geometryUsers, copyUsers, {}};
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        const auto name = "GCN program " + std::to_string(index);
        Require(program.binary.stage == stages[index] && program.binary.codeAddress == reinterpret_cast<std::uintptr_t>(codes[index]), name + " was read from another program register");
        Require(program.firstUserSgpr == 0u && program.hiddenUserWords == 0u, name + " places its user data after hidden words");
        Require(std::ranges::equal(program.userData, users[index]), name + " did not read its own stage's user data");
    }
    Require(draw.state.stages.legacyGeometry->exports == Points().exports, "the copy program's exports were not read into the geometry configuration");
}

}

int main() {
    try {
        ShaderRecompiler::SetGcnGuestShaders(true);
        RequireCopyLayout();
        RequireStageDecode();
        RequireProgramDecode();
        ShaderRecompiler::SetGcnGuestShaders(false);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->GeometryLimits()) {
            std::puts("GCN geometry: skipped, the device has no geometry shaders");
            return VulkanTestSkipped;
        }
        ShaderRecompiler::SetGcnGuestShaders(true);
        RequireQuads(*device);
        ShaderRecompiler::SetGcnGuestShaders(false);
        device.reset();
        std::puts("GCN geometry tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
