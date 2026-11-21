#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <span>
#include <array>
#include <sstream>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
std::array<std::uint16_t, 4> QuadSampleMasks(const State& state, const SampleOrder& order) {
    if (!SampleMaskFollowsOrder(state, order)) return QuadSampleMasks(state);
    const auto mask = static_cast<std::uint16_t>(state.quadSampleMask[0] & ((1u << state.samples) - 1u));
    return {mask, mask, mask, mask};
}

VkLogicOp LogicOpFromRop(std::uint8_t rop) {
    if (!documentedRop(rop)) {
        char text[96];
        std::snprintf(text, sizeof(text), "AGC graphics: ROP3 code 0x%02x combines source and brush, which is not implemented", rop);
        throw std::runtime_error(text);
    }
    static constexpr std::array<VkLogicOp, 16> operations{
        VK_LOGIC_OP_CLEAR, VK_LOGIC_OP_NOR, VK_LOGIC_OP_AND_INVERTED, VK_LOGIC_OP_COPY_INVERTED,
        VK_LOGIC_OP_AND_REVERSE, VK_LOGIC_OP_INVERT, VK_LOGIC_OP_XOR, VK_LOGIC_OP_NAND,
        VK_LOGIC_OP_AND, VK_LOGIC_OP_EQUIVALENT, VK_LOGIC_OP_NO_OP, VK_LOGIC_OP_OR_INVERTED,
        VK_LOGIC_OP_COPY, VK_LOGIC_OP_OR_REVERSE, VK_LOGIC_OP_OR, VK_LOGIC_OP_SET};
    const std::uint32_t index = (rop & 3u) | ((rop >> 4u) & 4u) | ((rop >> 4u) & 8u);
    return operations[index];
}

std::array<VkSampleLocationEXT, 32> SampleLocations(const State& state) {
    Require(state.samples == 1 || state.samples == 2 || state.samples == 4 || state.samples == 8, "invalid sample count");
    std::array<VkSampleLocationEXT, 32> locations{};
    const auto grid = SampleLocationGrid(state);
    for (std::uint32_t pixel = 0; pixel < grid.width * grid.height; ++pixel) {
        for (std::uint32_t sample = 0; sample < state.samples; ++sample) {
            std::array<int, 2> position{};
            if (state.customSampleLocations) {
                const auto bits = state.sampleLocations[pixel * 8u + sample];
                position = {static_cast<int>(bits & 7u) - static_cast<int>(bits & 8u), static_cast<int>((bits >> 4u) & 7u) - static_cast<int>((bits >> 4u) & 8u)};
            } else if (state.samples != 1) {
                const auto& standard = state.samples == 2 ? StandardLocations2[sample] : state.samples == 4 ? StandardLocations4[sample] : StandardLocations8[sample];
                position = {standard[0], standard[1]};
            }
            locations[pixel * state.samples + sample] = {0.5f + position[0] / 16.0f, 0.5f + position[1] / 16.0f};
        }
    }
    return locations;
}

std::array<std::uint8_t, 32> PixelSampleLocations(const State& state, const ShaderRecompiler::ShaderPixelStageInfo& pixel) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    std::array<std::uint8_t, 32> locations{};
    const bool sampleBarycentrics = (pixel.inputAddr & (PixelInputBit(PixelInput::PerspectiveSample) | PixelInputBit(PixelInput::LinearSample))) != 0u;
    const bool samplePosition = !state.perSampleShading && (pixel.posX || pixel.posY) && pixel.positionLocation == ShaderRecompiler::PixelPositionLocation::Sample;
    const bool mergedCentres = !pixel.targetMerges.empty() && pixel.targetMerges.front().samples > 1u && (pixel.inputAddr & (PixelInputBit(PixelInput::PerspectiveCenter) | PixelInputBit(PixelInput::LinearCenter))) != 0u;
    if (!sampleBarycentrics && !samplePosition && !mergedCentres) return locations;
    const auto samples = state.perSampleShading ? state.samples : 1u;
    for (std::uint32_t pixelIndex = 0; pixelIndex < 4u; ++pixelIndex) {
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            if (state.customSampleLocations) {
                locations[pixelIndex * 8u + sample] = state.sampleLocations[pixelIndex * 8u + sample];
            } else if (state.samples != 1) {
                const auto& standard = state.samples == 2 ? StandardLocations2[sample] : state.samples == 4 ? StandardLocations4[sample] : StandardLocations8[sample];
                locations[pixelIndex * 8u + sample] = static_cast<std::uint8_t>((standard[0] & 0xf) | ((standard[1] & 0xf) << 4u));
            }
        }
    }
    return locations;
}

void ApplyPointSize(const State& state, ShaderRecompiler::ShaderVertexStageInfo& vertex) {
    if (!state.vertexPoints && !(state.stages.mesh.has_value() && state.stages.mesh->outputPrimitiveSize == 1u)) return;
    vertex.paClVsOutCntl |= state.paClVsOutCntl & (1u << 16u);
    vertex.pointList = true;
    vertex.pointSize = state.pointSize;
    vertex.pointSizeMin = state.pointSizeMin;
    vertex.pointSizeMax = state.pointSizeMax;
}

ShaderStages DecodeShaderStages(const QueueState& queue, bool vertexWorkgroup) {
    const auto value = read(queue.context, 0x2d5);
    const auto validate = [&](bool condition, const char* reason) {
        if (condition) return;
        std::ostringstream prefix;
        prefix << "VGT_SHADER_STAGES_EN=0x" << std::hex << value << ": " << reason;
        Require(false, prefix.str());
    };
    validate((value & 0xfc000000u) == 0, "reserved stage bits are set");
    validate((value & 3u) != 3u && ((value >> 3u) & 3u) != 3u && ((value >> 6u) & 3u) != 3u, "reserved LS_EN, ES_EN or VS_EN encoding");
    const auto primitive = read(queue.userConfig, 0x242, RegisterBank::UserConfig);
    const bool tessellation = primitive == 9;
    validate(!vertexWorkgroup || ((value & 0x02002020u) == 0x2000u && !tessellation), "a vertex program that needs a workgroup requires PRIMGEN_EN without GS_EN, PRIMGEN_PASSTHRU_EN or tessellation");
    const bool geometry = (value & 0x20u) != 0 || vertexWorkgroup;
    validate(tessellation == ((value & 4u) != 0), "Patch topology and HS_EN disagree");
    const auto path = tessellation ? geometry ? ShaderPath::TessellationGeometry : ShaderPath::Tessellation : geometry ? ShaderPath::Geometry : ShaderPath::Vertex;
    ShaderStages result{path, value, (value & 0x00400000u) != 0 ? 32u : 64u, (read(queue.context, 0x1b6) & 0x8000u) != 0 ? 32u : 64u, {}, {}};
    result.hullWaveSize = (value & 0x00200000u) != 0 ? 32u : 64u;
    APS5_LOG_OUT_DEBUG("DecodeShaderStages value=0x%x primitive=%u path=%u vertexWave=%u", value, primitive, static_cast<unsigned>(result.path), result.vertexWaveSize);
    const auto meshConfiguration = [&](std::uint32_t inputPrimitive, bool fastLaunch) {
        const auto group = read(queue.userConfig, 0x25b, RegisterBank::UserConfig);
        const auto vertices = (group >> 9u) & 0x1ffu;
        const auto primitives = group & 0x1ffu;
        validate(!fastLaunch || (!vertexWorkgroup && vertices == 1 && primitives == 1), "fast launch other than one vertex and one primitive per subgroup is unsupported");
        const auto maxVertices = read(queue.context, 0x1ff);
        const auto verticesPerPrimitive = vertexWorkgroup ? 3u : read(queue.context, 0x2ce);
        const auto outputType = read(queue.context, 0x29b);
        validate(outputType == 2 || ((outputType == 0 || outputType == 1) && !vertexWorkgroup), "VGT_GS_OUT_PRIM_TYPE other than a triangle strip, or a point list or line strip from a geometry program, is unsupported");
        const auto outputSize = outputType == 0 ? 1u : outputType == 1 ? 2u : 3u;
        validate((inputPrimitive == 1 || inputPrimitive == 2 || inputPrimitive == 4 || inputPrimitive == 5 || inputPrimitive == 6) && verticesPerPrimitive >= outputSize, "unsupported geometry input or output assembly");
        const auto inputSize = inputPrimitive == 1 ? 1u : inputPrimitive == 2 ? 2u : 3u;
        validate(vertices >= inputSize && maxVertices != 0 && (!vertexWorkgroup || maxVertices >= inputSize) && maxVertices <= 256 && verticesPerPrimitive <= 256, "invalid geometry subgroup output");
        const auto inputStep = inputPrimitive == 5 || inputPrimitive == 6 ? 1u : inputSize;
        const auto outputLimit = vertexWorkgroup ? (maxVertices - inputSize) / inputStep + 1u : maxVertices / verticesPerPrimitive;
        const auto groupPrimitives = std::min({primitives, (vertices - inputSize) / inputStep + 1u, outputLimit});
        validate(groupPrimitives != 0, "geometry subgroup contains no primitives");
        const auto resources = read(queue.shader, 0x8b, RegisterBank::Shader);
        const auto esgsItemSize = read(queue.context, 0x2ab);
        validate(esgsItemSize != 0 && esgsItemSize * vertices <= 0xffffu, "invalid VGT_ESGS_RING_ITEMSIZE");
        const auto amplification = fastLaunch ? read(queue.context, 0x2d3) & 0x1ffu : verticesPerPrimitive - outputSize + 1u;
        validate(amplification != 0, "fast launch subgroup amplifies to no primitives");
        const auto maxPrimitives = outputSize <= 2u && !fastLaunch ? std::max(primitives * amplification, maxVertices) : primitives * amplification;
        const auto threads = std::max({(groupPrimitives - 1u) * inputStep + inputSize, primitives, maxVertices, maxPrimitives});
        const auto raster = find(queue.context, 0x205);
        const auto provokingVertex = raster != queue.context.end() && (raster->second & (1u << 19u)) != 0 ? outputSize - 1u : 0u;
        return ShaderRecompiler::MeshConfiguration{inputPrimitive, groupPrimitives, (groupPrimitives - 1u) * inputStep + inputSize, maxVertices, maxPrimitives, ((threads + result.vertexWaveSize - 1u) / result.vertexWaveSize) * result.vertexWaveSize, ((resources >> 19u) & 0xffu) * 128u, provokingVertex, esgsItemSize, outputSize};
    };
    if (path == ShaderPath::Vertex && ShaderRecompiler::GcnGuestShaders() && (value & 0x2000u) == 0) {
        validate(value == 0, "GCN vertex routing other than a plain VS is unsupported");
        const auto unitStepRate = [&](std::uint32_t offset) {
            const auto rate = find(queue.context, offset);
            return rate == queue.context.end() || rate->second == 1u;
        };
        const auto components = (read(queue.shader, 0x4a, RegisterBank::Shader) >> 24u) & 3u;
        validate(components == 0 || (unitStepRate(0x2a8) && (components == 1 || unitStepRate(0x2a9))), "GCN vertex instance step rates other than 1 are not implemented");
        result.legacyVertex = true;
    } else if (path == ShaderPath::Vertex) {
        validate((value & 0x2000u) != 0, "legacy vertex routing without PRIMGEN_EN is unsupported");
        validate((value & ~0x0247a010u) == 0 || value == 0x02002008u, "unsupported vertex routing, scheduling or wave-ID state");
    } else if (path == ShaderPath::Tessellation || path == ShaderPath::TessellationGeometry) {
        if (path == ShaderPath::Tessellation) validate((value & ~0x0267ed0du) == 0 && (value & 0x02002000u) == 0x02002000u && (value & 3u) == 1u && ((value >> 3u) & 3u) == 1u, "unsupported tessellation routing");
        else validate((value & ~0x0067ed2du) == 0 && (value & 0x02002000u) == 0x2000u && (value & 3u) == 1u && ((value >> 3u) & 3u) == 1u, "unsupported tessellation and geometry routing");
        const auto config = read(queue.context, 0x2d6);
        const auto parameters = read(queue.context, 0x2db);
        const auto primitiveGroup = find(queue.userConfig, 0x25b, RegisterBank::UserConfig);
        ShaderRecompiler::TessellationConfiguration tess{(config >> 8u) & 0x3fu, (config >> 14u) & 0x3fu, parameters & 3u, (parameters >> 2u) & 7u, (parameters >> 5u) & 7u, config & 0xffu, primitiveGroup == queue.userConfig.end() ? 0u : primitiveGroup->second & 0x1ffu, primitiveGroup != queue.userConfig.end() && (primitiveGroup->second & 0x40000u) != 0u};
        validate(tess.inputControlPoints != 0 && tess.inputControlPoints <= 32 && tess.outputControlPoints != 0 && tess.outputControlPoints <= 32, "invalid tessellation control-point counts");
        validate(tess.patchesPerGroup != 0, "VGT_LS_HS_CONFIG.NUM_PATCHES is zero");
        if ((parameters & ~0x000600ffu) != 0) Require(false, zeroMessage(0x2db, parameters, "VGT_TF_PARAM donut, factor detection, memory type or reserved state"));
        if (!((tess.domain == 1 || tess.domain == 2) && (tess.partitioning == 0 || tess.partitioning == 2 || tess.partitioning == 3) && (tess.outputTopology == 2 || tess.outputTopology == 3))) validate(false, tessellationMessage(config, parameters, tess).c_str());
        result.minTessellationLevel = readFloat(queue.context, 0x287);
        result.maxTessellationLevel = readFloat(queue.context, 0x286);
        Require(result.minTessellationLevel >= 0.0f && result.minTessellationLevel <= 63.0f && result.minTessellationLevel <= result.maxTessellationLevel && result.maxTessellationLevel >= 1.0f, "tessellation level clamps need 0 <= VGT_HOS_MIN_TESS_LEVEL <= 63, VGT_HOS_MAX_TESS_LEVEL >= 1 and the minimum at most the maximum");
        result.tessellation = tess;
        if (path == ShaderPath::TessellationGeometry) result.tessellationGeometry = meshConfiguration(4u, false);
    } else {
        const bool fastLaunch = ((value >> 19u) & 3u) == 1u;
        validate((value & ~(fastLaunch ? 0x004fec30u : 0x0047ec30u)) == 0, "unsupported geometry routing, fast launch or wave-ID state");
        result.mesh = meshConfiguration(fastLaunch ? 1u : primitive, fastLaunch);
    }
    return result;
}

std::uint32_t StageWaveSize(const ShaderStages& stages, ShaderRecompiler::ShaderStage stage) {
    if (stage == ShaderRecompiler::ShaderStage::Fragment) return stages.fragmentWaveSize;
    if (stage == ShaderRecompiler::ShaderStage::Local || stage == ShaderRecompiler::ShaderStage::TessellationControl) return stages.hullWaveSize;
    return stages.vertexWaveSize;
}

std::optional<ShaderRecompiler::MeshConfiguration> StageMeshConfiguration(const ShaderStages& stages, ShaderRecompiler::ShaderStage stage) {
    if (stages.mesh) return stages.mesh;
    if (stage == ShaderRecompiler::ShaderStage::TessellationEvaluation || stage == ShaderRecompiler::ShaderStage::Mesh) return stages.tessellationGeometry;
    return std::nullopt;
}

std::uint32_t StagePushLimit(const ShaderStages& stages, std::uint32_t pushOffset, bool depthRemap) {
    const bool preRasterization = pushOffset < PipelinePushSlotBytes;
    auto limit = PipelinePushSlotBytes;
    if (stages.mesh || (stages.tessellationGeometry && preRasterization)) limit = ShaderRecompiler::MeshDrawPushOffsetBytes;
    else if (stages.tessellation && preRasterization) limit = ShaderRecompiler::TessellationLevelPushOffsetBytes;
    return depthRemap ? std::min(limit, ShaderRecompiler::DepthRemapPushOffsetBytes) : limit;
}

bool DepthResummarizeDrawsNothing(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x000);
    const auto depthControl = find(cx, 0x200);
    const auto zInfo = find(cx, 0x010);
    const auto stencilInfo = find(cx, 0x011);
    const auto targetMask = find(cx, 0x8e);
    const auto shaderMask = find(cx, 0x8f);
    if (control == cx.end() || control->second != 0x10u || depthControl == cx.end() || zInfo == cx.end() || stencilInfo == cx.end() || targetMask == cx.end() || shaderMask == cx.end()) return false;
    const auto depth = depthControl->second;
    const auto format = zInfo->second & 3u;
    return (depth & 2u) != 0 && (depth & 0xc000000cu) == 0 && depthPassThrough(depth) && (format == 1u || format == 3u) && (stencilInfo->second & 1u) == 0 && ColorWriteMask(cx) == 0 && PixelProgramSkipped(queue);
}

std::string DepthMaintenanceRejection(const QueueState& queue) {
    if (DepthResummarizeDrawsNothing(queue)) return {};
    const auto control = find(queue.context, 0x000);
    if (control == queue.context.end()) return {};
    if (const auto planes = outOfPlacePlanes(queue.context); planes.depth || planes.stencil) {
        char text[192];
        std::snprintf(text, sizeof(text), "AGC graphics: an out-of-place DB expand (DB_RENDER_CONTROL 0x%08x) is run by the draw precheck, in order, and is not drawn", control->second);
        return text;
    }
    constexpr std::uint32_t maintenance = 0x0000101cu;
    if ((control->second & maintenance) == 0) return {};
    const auto depthControl = find(queue.context, 0x200);
    const auto colorControl = find(queue.context, 0x202);
    const bool colorDisabled = colorControl != queue.context.end() && ((colorControl->second >> 4u) & 7u) == 0u;
    const bool colorMasks = find(queue.context, 0x8e) != queue.context.end() && find(queue.context, 0x8f) != queue.context.end();
    if ((control->second & (maintenance & ~0x10u)) == 0 && depthControl != queue.context.end() && (depthControl->second & 3u) == 0 && (colorDisabled || (colorMasks && ColorWriteMask(queue.context) == 0))) return {};
    return zeroMessage(0x000, control->second, "DB_RENDER_CONTROL depth copy, resummarize or decompress");
}

std::optional<DepthRenderControl> DecodeDepthRenderControl(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x000);
    const auto zInfo = find(cx, 0x010);
    if (control == cx.end() || (control->second & 0x63u) == 0u || zInfo == cx.end() || (zInfo->second & 3u) == 0u) return std::nullopt;
    const auto high = find(cx, 0x01a);
    const auto address = (high == cx.end() ? 0ull : static_cast<std::uint64_t>(high->second & 0xffu) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x012)) << 8u);
    return DepthRenderControl{address & ~std::uint64_t{65535u}, 1u << ((zInfo->second >> 2u) & 3u), control->second};
}

ReversedBlend TranslateReversedBlend(std::uint32_t blendControl, std::uint8_t componentMapping, VkFormat format, bool forceDestinationAlphaOne, const std::array<float, 4>& constants) {
    const auto refuse = [&](const char* reason) {
        char text[320];
        std::snprintf(text, sizeof(text), "AGC graphics: blending into a color target with a reversed component order %s (CB_BLEND_CONTROL 0x%08x, component mapping 0x%02x, VkFormat %d)", reason, blendControl, componentMapping, static_cast<int>(format));
        throw std::runtime_error(text);
    };
    Require(componentMapping == 0x1bu || componentMapping == 0x93u, "a reversed-order blend needs the component mapping 0x1b or 0x93");
    if (format == VK_FORMAT_R8G8B8A8_SRGB) refuse("is not implemented for sRGB targets");
    if (format == VK_FORMAT_R8G8B8A8_SNORM || format == VK_FORMAT_R16G16B16A16_SNORM) refuse("is not implemented for signed normalized targets");
    if (format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R16G16B16A16_UNORM && format != VK_FORMAT_R16G16B16A16_SFLOAT) refuse("is not implemented for integer targets");
    const auto alphaControl = (blendControl & 0x20000000u) != 0 ? blendControl >> 16u : blendControl;
    std::array<VkBlendFactor, 2> color{blendFactor(blendControl & 0x1fu), blendFactor((blendControl >> 8u) & 0x1fu)};
    std::array<VkBlendFactor, 2> alpha{blendFactor(alphaControl & 0x1fu), blendFactor((alphaControl >> 8u) & 0x1fu)};
    const auto operation = blendOp((blendControl >> 5u) & 7u);
    const auto alphaOperation = blendOp((alphaControl >> 5u) & 7u);
    const auto any = [&](auto predicate) { return predicate(color[0]) || predicate(color[1]) || predicate(alpha[0]) || predicate(alpha[1]); };
    const auto constant = [](VkBlendFactor factor) { return factor == VK_BLEND_FACTOR_CONSTANT_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR || factor == VK_BLEND_FACTOR_CONSTANT_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA; };
    if (any(secondSource)) refuse("with a second-source factor is not implemented, as dual-source blending into a component-swapped color target is unsupported");
    if (operation != alphaOperation) refuse("with different color and alpha operations is not implemented");
    if (forceDestinationAlphaOne) {
        for (auto& factor : color) factor = colorFactorWithOpaqueDestination(factor);
        for (auto& factor : alpha) factor = alphaFactorWithOpaqueDestination(factor);
    }
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.colorBlendOp = operation;
    blend.alphaBlendOp = operation;
    if (operation == VK_BLEND_OP_MIN || operation == VK_BLEND_OP_MAX) {
        if (any([](VkBlendFactor factor) { return factor != VK_BLEND_FACTOR_ONE; })) refuse("with a MIN or MAX operation whose factors are not ONE is not implemented");
        blend.srcColorBlendFactor = blend.dstColorBlendFactor = blend.srcAlphaBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        return {blend, 0u};
    }
    for (const auto factor : color) {
        if (factor == VK_BLEND_FACTOR_DST_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA || factor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE) refuse("with a factor of the destination alpha in the color equation, which the target keeps in its first channel, is not implemented");
    }
    const auto guestConstant = [&](std::uint32_t component) { return format == VK_FORMAT_R16G16B16A16_SFLOAT ? constants[component] : std::clamp(constants[component], 0.0f, 1.0f); };
    const auto fixedValue = [&](VkBlendFactor factor, std::uint32_t component) -> std::optional<float> {
        switch (factor) {
            case VK_BLEND_FACTOR_ZERO: return 0.0f;
            case VK_BLEND_FACTOR_ONE: return 1.0f;
            case VK_BLEND_FACTOR_CONSTANT_COLOR: return guestConstant(component);
            case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return 1.0f - guestConstant(component);
            case VK_BLEND_FACTOR_CONSTANT_ALPHA: return guestConstant(3u);
            case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return 1.0f - guestConstant(3u);
            default: return std::nullopt;
        }
    };
    std::array<std::optional<std::array<float, 4>>, 2> fixedValues;
    auto routedColor = color;
    auto routedAlpha = alpha;
    for (std::size_t position = 0; position < 2; ++position) {
        if (!constant(color[position]) && !constant(alpha[position])) continue;
        std::array<float, 4> values{};
        for (std::uint32_t component = 0; component < 4; ++component) {
            const auto value = fixedValue(component == 0u ? alpha[position] : color[position], (componentMapping >> (2u * component)) & 3u);
            if (!value) refuse("with a constant blend factor beside a factor of the export or the destination in the same operand is not implemented");
            values[component] = *value;
        }
        fixedValues[position] = values;
        routedColor[position] = VK_BLEND_FACTOR_ZERO;
        routedAlpha[position] = VK_BLEND_FACTOR_ZERO;
    }
    const auto routing = routeReversedBlend(routedColor, routedAlpha);
    if (!routing) refuse("with factors the host blend cannot form from copies of the export is not implemented");
    auto factors = routing->factors;
    ReversedBlend result{blend, routing->code};
    const auto fixedFactor = [&](const std::array<float, 4>& values, std::uint32_t first, std::uint32_t last, VkBlendFactor same, VkBlendFactor complement) {
        const auto all = [&](auto predicate) {
            bool matches = true;
            for (std::uint32_t component = first; component <= last; ++component) matches = matches && predicate(component);
            return matches;
        };
        if (all([&](std::uint32_t component) { return values[component] == 0.0f; })) return VK_BLEND_FACTOR_ZERO;
        if (all([&](std::uint32_t component) { return values[component] == 1.0f; })) return VK_BLEND_FACTOR_ONE;
        const std::uint32_t mask = ((2u << last) - 1u) & ~((1u << first) - 1u);
        if ((result.constantMask & mask) == 0u) {
            for (std::uint32_t component = first; component <= last; ++component) result.constants[component] = values[component];
            result.constantMask |= mask;
            return same;
        }
        if (all([&](std::uint32_t component) { return values[component] == result.constants[component]; })) return same;
        if (all([&](std::uint32_t component) { return values[component] == 1.0f - result.constants[component]; })) return complement;
        refuse("with two constant blend factors that one set of host blend constants cannot form is not implemented");
        return VK_BLEND_FACTOR_ZERO;
    };
    for (std::size_t position = 0; position < 2; ++position) {
        if (!fixedValues[position]) continue;
        if (routing->code != 0u) refuse("with a constant blend factor and factors the host blend forms through a changed export is not implemented");
        factors[position] = fixedFactor(*fixedValues[position], 0u, 2u, VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        factors[2u + position] = fixedFactor(*fixedValues[position], 3u, 3u, VK_BLEND_FACTOR_CONSTANT_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);
    }
    result.blend.srcColorBlendFactor = factors[0];
    result.blend.dstColorBlendFactor = factors[1];
    result.blend.srcAlphaBlendFactor = factors[2];
    result.blend.dstAlphaBlendFactor = factors[3];
    return result;
}

void ApplyReadOnlyDepthBoundsComparison(State& state, const ShaderRecompiler::ShaderPixelStageInfo& pixel, bool enabled) {
    if (!enabled || !state.depthBoundsTest || !state.depth || state.depth->format != VK_FORMAT_D32_SFLOAT_S8_UINT) return;
    if (state.depthTest || state.depthWrite || state.stencilTest || state.depthBias || state.depthBiasPerFace || state.depthRemap.mode != 0) return;
    if (state.samples != 1 || state.depth->samples != 1 || state.customSampleLocations || state.sampleLocationGrid) return;
    if (pixel.posZ || pixel.depthExportEnable) return;
    if (!std::isfinite(state.minDepthBounds) || !std::isfinite(state.maxDepthBounds) || state.minDepthBounds < 0.0f || state.maxDepthBounds > 1.0f || state.minDepthBounds > state.maxDepthBounds) return;
    if (!std::isfinite(state.viewport.minDepth) || !std::isfinite(state.viewport.maxDepth) || state.viewport.minDepth < 0.0f || state.viewport.minDepth > 1.0f || state.viewport.maxDepth < 0.0f || state.viewport.maxDepth > 1.0f) return;
    if (state.depthClamp && (!std::isfinite(state.depthClampMin) || !std::isfinite(state.depthClampMax) || state.depthClampMin > 1.0f || state.depthClampMax < 1.0f)) return;
    state.depthTest = true;
    state.depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;
    state.viewport.minDepth = 1.0f;
    state.viewport.maxDepth = 1.0f;
}

void ApplyDepthBoundsEmulation(const Context& context, State& state, const ShaderRecompiler::ShaderPixelStageInfo& pixel) {
    if (!state.depthBoundsTest || !state.depth || context.depthBounds) return;
    ValidateDepthBounds(context, state);
    Require(!context.depthRangeUnrestricted, "the depth bounds test without the depthBounds feature is emulated only where stored depth stays in [0, 1]");
    Require(state.depth->format == VK_FORMAT_D32_SFLOAT || state.depth->format == VK_FORMAT_D32_SFLOAT_S8_UINT, "the depth bounds test on a normalized depth target needs the depthBounds feature");
    Require(!state.depthTest && !state.depthWrite, "the depth bounds test with depth testing needs the depthBounds feature");
    Require(!pixel.posZ && !pixel.depthExportEnable, "the depth bounds test from a pixel program that reads or exports depth needs the depthBounds feature");
    const auto keepsOnFail = [](const VkStencilOpState& face) { return (face.writeMask & 0xffu) == 0 || (face.failOp == VK_STENCIL_OP_KEEP && face.depthFailOp == VK_STENCIL_OP_KEEP); };
    Require(!state.stencilTest || (keepsOnFail(state.stencilFront) && keepsOnFail(state.stencilBack)), "the depth bounds test with a stencil test that writes on a failed test needs the depthBounds feature");
    const bool upper = state.minDepthBounds <= 0.0f;
    Require(upper || state.maxDepthBounds >= 1.0f, "the depth bounds test with both bounds inside (0, 1) needs the depthBounds feature");
    const float bound = upper ? state.maxDepthBounds : state.minDepthBounds;
    state.depthBoundsTest = false;
    state.depthTest = true;
    state.depthCompare = upper ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
    state.depthBias = false;
    state.depthBiasPerFace = false;
    state.viewport.minDepth = bound;
    state.viewport.maxDepth = bound;
    for (auto& extra : state.extraViewports) {
        extra.minDepth = bound;
        extra.maxDepth = bound;
    }
}

void ApplyDepthRemap(const Context& context, State& state) {
    const auto withinUnitRange = [](const VkViewport& viewport) { return viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1; };
    const bool inRange = withinUnitRange(state.viewport) && std::ranges::all_of(state.extraViewports, withinUnitRange);
    if (context.depthRangeUnrestricted || inRange) return;
    Require(state.extraViewports.empty(), "viewport depth outside [0, 1] is remapped for a single viewport only");
    const bool vertex = state.stages.path == ShaderPath::Vertex && !state.stages.tessellation;
    const bool tessellation = state.stages.path == ShaderPath::Tessellation && state.stages.tessellation.has_value();
    Require((vertex || tessellation) && !state.stages.mesh, "viewport depth outside [0, 1] is remapped on the vertex and tessellation paths only");
    Require(context.depthClamp, "viewport depth outside [0, 1] needs depth clamping to stand in for the console's clamp");
    const float clampMin = std::clamp(state.depthClampMin, 0.0f, 1.0f);
    const float clampMax = std::clamp(state.depthClampMax, 0.0f, 1.0f);
    const bool floatDepth = state.depth.has_value() && (state.depth->format == VK_FORMAT_D32_SFLOAT || state.depth->format == VK_FORMAT_D32_SFLOAT_S8_UINT);
    if ((clampMin != state.depthClampMin || clampMax != state.depthClampMax) && floatDepth && (state.depthWrite || state.depthTest)) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] viewport depth clamp bounds [%g, %g] on a float depth target are clipped to [0, 1]: the device has no VK_EXT_depth_range_unrestricted\n", state.depthClampMin, state.depthClampMax);
        }
    }
    const float range = clampMax - clampMin;
    const float scale = range > 0.0f ? state.depthScale / range : state.depthScale;
    const float offset = range > 0.0f ? (state.depthOffset - clampMin) / range : state.depthOffset;
    state.depthRemap = {state.negativeOneToOne ? 2u : 1u, scale, offset};
    state.viewport.minDepth = clampMin;
    state.viewport.maxDepth = clampMax;
    state.negativeOneToOne = false;
    state.depthClamp = true;
}

}
}
