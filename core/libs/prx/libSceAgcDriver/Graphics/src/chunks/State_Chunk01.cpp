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

void NormalizeUnusedViewportDepth(State& state, const ShaderRecompiler::ShaderPixelStageInfo& pixel) {
    if (state.depth || pixel.posZ) return;
    for (auto& extra : state.extraViewports) {
        if (!std::isfinite(extra.minDepth) || !std::isfinite(extra.maxDepth)) continue;
        extra.minDepth = std::clamp(extra.minDepth, 0.0f, 1.0f);
        extra.maxDepth = std::clamp(extra.maxDepth, 0.0f, 1.0f);
    }
    if (!std::isfinite(state.viewport.minDepth) || !std::isfinite(state.viewport.maxDepth)) return;
    state.viewport.minDepth = std::clamp(state.viewport.minDepth, 0.0f, 1.0f);
    state.viewport.maxDepth = std::clamp(state.viewport.maxDepth, 0.0f, 1.0f);
}

void ApplyViewportIndex(const Context& context, State& state) {
    if (state.viewportExports == 0u) return;
    const bool vertex = state.stages.path == ShaderPath::Vertex && !state.stages.tessellation && !state.rectList && !state.stages.mesh.has_value();
    const bool direct = (vertex && context.viewportIndexLayer) || state.stages.mesh.has_value();
    if (context.multiViewport && direct) {
        Require(ViewportCount(state) == MaxViewports && state.viewportScissors.size() == MaxViewports, "a viewport index draw must declare all 16 viewports, since the 4-bit index can name any of them");
        return;
    }
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "[gpu] viewport index vertex exports are ignored: %s\n", !context.multiViewport ? "the device has no multiViewport with 16 viewports" : vertex ? "the device has no VK_EXT_shader_viewport_index_layer for vertex draws" : "only vertex and mesh draws select a viewport");
    }
    if (!state.viewportScissors.empty()) state.scissor = state.viewportScissors.front();
    state.viewportExports = 0u;
    state.extraViewports.clear();
    state.viewportScissors.clear();
}

namespace {

std::uint32_t read(const Registers& registers, std::uint32_t offset, RegisterBank bank = RegisterBank::Context) {
    NoteRegisterRead(bank, offset);
    const auto it = registers.find(offset);
    if (it == registers.end()) {
        std::ostringstream message;
        message << "missing register in " << RegisterBankName(bank) << " bank at DWORD 0x" << std::hex << offset << " (" << std::dec << offset << ')';
        throw std::runtime_error("AGC graphics: " + message.str());
    }
    return it->second;
}

// A register that may be absent (the decoders' find/contains), through the facade.
Registers::const_iterator find(const Registers& registers, std::uint32_t offset, RegisterBank bank = RegisterBank::Context) {
    NoteRegisterRead(bank, offset);
    return registers.find(offset);
}

float readFloat(const Registers& registers, std::uint32_t offset) {
    const auto value = std::bit_cast<float>(read(registers, offset));
    if (!std::isfinite(value)) Require(false, "non-finite register at DWORD " + std::to_string(offset));
    return value;
}

float readDepthBound(const Registers& registers, std::uint32_t offset) {
    const auto value = std::bit_cast<float>(read(registers, offset));
    if (std::isnan(value)) Require(false, "NaN depth bound at DWORD " + std::to_string(offset));
    return value;
}

std::string zeroMessage(std::uint32_t offset, std::uint32_t value, const char* name) {
    char detail[64];
    std::snprintf(detail, sizeof(detail), " (register 0x%x = 0x%08x)", offset, value);
    return std::string(name) + " is unsupported" + detail;
}

VkPolygonMode decodePolygonMode(const Registers& cx, std::uint32_t raster, const State& state, bool pointsOrLines) {
    const auto mode = (raster >> 3u) & 3u;
    if (mode == 0) return VK_POLYGON_MODE_FILL;
    if (mode != 1) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "a reserved POLY_MODE"));
    const auto front = (raster >> 5u) & 7u;
    const auto back = (raster >> 8u) & 7u;
    if (front > 2 || back > 2) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "a reserved polygon type"));
    if (pointsOrLines && (state.stages.path == ShaderPath::Vertex || state.stages.mesh.has_value())) return VK_POLYGON_MODE_FILL;
    const bool frontDrawn = (state.cullMode & VK_CULL_MODE_FRONT_BIT) == 0;
    const bool backDrawn = (state.cullMode & VK_CULL_MODE_BACK_BIT) == 0;
    if (!frontDrawn && !backDrawn) return VK_POLYGON_MODE_FILL;
    if (frontDrawn && backDrawn && front != back) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "different front and back polygon modes"));
    const auto type = frontDrawn ? front : back;
    if (type == 2) return VK_POLYGON_MODE_FILL;
    if (state.rectList) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "a point or line polygon mode on a rect list"));
    if ((raster & 0x3800u) != 0) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "depth bias with a point or line polygon mode"));
    if (type == 1) {
        const auto line = find(cx, 0x282);
        if (line == cx.end()) throw std::runtime_error("AGC graphics: a line polygon mode needs PA_SU_LINE_CNTL");
        if ((line->second & 0xffffu) == 0u) throw std::runtime_error("AGC graphics: " + zeroMessage(0x282, line->second, "a line polygon mode with a zero line width"));
        return VK_POLYGON_MODE_LINE;
    }
    const auto size = find(cx, 0x280);
    const auto limits = find(cx, 0x281);
    if (size == cx.end() || limits == cx.end()) throw std::runtime_error("AGC graphics: a point polygon mode needs PA_SU_POINT_SIZE and PA_SU_POINT_MINMAX");
    if (size->second != 0x80008u) throw std::runtime_error("AGC graphics: " + zeroMessage(0x280, size->second, "a point polygon mode with a point size other than one pixel"));
    if ((limits->second & 0xffffu) > 8u || (limits->second >> 16u) < 8u) throw std::runtime_error("AGC graphics: " + zeroMessage(0x281, limits->second, "a point polygon mode whose size limits exclude one pixel"));
    return VK_POLYGON_MODE_POINT;
}

std::string tessellationMessage(std::uint32_t config, std::uint32_t parameters, const ShaderRecompiler::TessellationConfiguration& tess) {
    static constexpr std::array<std::string_view, 3> Types{"TESS_ISOLINE", "TESS_TRIANGLE", "TESS_QUAD"};
    static constexpr std::array<std::string_view, 4> Partitionings{"PART_INTEGER", "PART_POW2", "PART_FRAC_ODD", "PART_FRAC_EVEN"};
    static constexpr std::array<std::string_view, 4> Topologies{"OUTPUT_POINT", "OUTPUT_LINE", "OUTPUT_TRIANGLE_CW", "OUTPUT_TRIANGLE_CCW"};
    std::ostringstream message;
    const auto field = [&](std::string_view name, std::uint32_t value, std::span<const std::string_view> names) {
        message << name << '=' << value;
        if (value < names.size()) message << ' ' << names[value];
    };
    message << "VGT_TF_PARAM=0x" << std::hex << parameters << std::dec << " (";
    field("TYPE", tess.domain, Types);
    message << ", ";
    field("PARTITIONING", tess.partitioning, Partitionings);
    message << ", ";
    field("TOPOLOGY", tess.outputTopology, Topologies);
    message << "), VGT_LS_HS_CONFIG=0x" << std::hex << config << std::dec << " (NUM_PATCHES=" << tess.patchesPerGroup << ", HS_NUM_INPUT_CP=" << tess.inputControlPoints << ", HS_NUM_OUTPUT_CP=" << tess.outputControlPoints << "): only triangle and quad domains with integer, fractional-even or fractional-odd partitioning and triangle output are supported by the reference path";
    return message.str();
}

void zero(const Registers& registers, std::uint32_t offset, std::uint32_t mask, const char* name, RegisterBank bank = RegisterBank::Context) {
    const auto value = read(registers, offset, bank);
    if ((value & mask) != 0) throw std::runtime_error(zeroMessage(offset, value, name));
}

bool viewportProgrammed(const Registers& registers) {
    for (std::uint32_t word = 0; word < 6; ++word) {
        if (read(registers, 0x10f + word) != (word % 2 == 0 ? 0x3f800000u : 0u)) return true;
    }
    return false;
}

std::string vteMessage(std::uint32_t viewportControl) {
    std::ostringstream message;
    message << "AGC graphics: PA_CL_VTE_CNTL=0x" << std::hex << viewportControl << ": expected 0x43f for homogeneous positions and all viewport transforms, or 0 for positions without a viewport transform; other combinations are unsupported";
    return message.str();
}

std::string conservativeMessage(std::uint32_t control) {
    std::ostringstream message;
    message << "AGC graphics: PA_SC_CONSERVATIVE_RASTERIZATION_CNTL=0x" << std::hex << control << ": only off (over- and underestimation disabled, with at most the inner-to-normal overrides, NULL_SQUAD_AA_MASK_ENABLE and the uncertainty-region mode and edge rules set) and 0x6001 (overestimation) are supported";
    return message.str();
}

void requireConservativeTriangles(bool triangles, const char* primitive, const char* source, std::uint32_t value) {
    if (triangles) return;
    std::ostringstream message;
    message << "AGC graphics: conservative rasterization of " << primitive << " is unsupported (" << source << "=0x" << std::hex << value << "): only triangles are overestimated";
    throw std::runtime_error(message.str());
}

VkConservativeRasterizationModeEXT decodeConservativeRasterization(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = read(cx, 0x313);
    if ((control & ~0x001f6000u) == 0) return VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    if (control != 0x6001u) throw std::runtime_error(conservativeMessage(control));
    const auto stages = read(cx, 0x2d5);
    if ((stages & 0x20u) != 0) {
        const auto output = read(cx, 0x29b);
        const auto type = output & 0x3fu;
        const bool perStream = (output & 0x80000000u) != 0;
        requireConservativeTriangles(!perStream && type == 2u, perStream ? "per-stream primitive types" : type == 0u ? "points" : type == 1u ? "lines" : "other primitive types", "VGT_GS_OUT_PRIM_TYPE", output);
    } else if ((stages & 4u) != 0) {
        const auto parameters = read(cx, 0x2db);
        const auto domain = parameters & 3u;
        const auto topology = (parameters >> 5u) & 7u;
        requireConservativeTriangles((domain == 1u || domain == 2u) && (topology == 2u || topology == 3u), topology == 0u ? "points" : topology == 1u || domain == 0u ? "lines" : "other primitive types", "VGT_TF_PARAM", parameters);
    } else {
        const auto primitive = read(queue.userConfig, 0x242, RegisterBank::UserConfig);
        requireConservativeTriangles(primitive == 4u || primitive == 5u || primitive == 6u || primitive == 21u, primitive == 1u ? "points" : primitive == 2u || primitive == 3u ? "lines" : primitive == 7u || primitive == 17u ? "rectangles" : "other primitive types", "VGT_PRIMITIVE_TYPE", primitive);
    }
    Require((read(cx, 0x1b3) & 0x44u) == 0 || (read(cx, 0x2f8) & 7u) == 0, "conservative rasterization with centroid interpolation of more than one rasterized sample is unsupported");
    return VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
}

// The register rules DecodeState and DrawRejection share (the precheck must reject exactly what
// DecodeState would): the masks whose set bits are unsupported, and the depth-control verdict.
constexpr std::uint32_t LayerExports = (1u << 18u) | (1u << 19u) | (1u << 21u) | (1u << 24u);
constexpr std::uint32_t ClipCullExports = 0xffffu | (1u << 22u) | (1u << 23u);

std::uint32_t inactiveVertexPointSize(const QueueState& queue) {
    const auto stages = find(queue.context, 0x2d5);
    const auto primitive = find(queue.userConfig, 0x242, RegisterBank::UserConfig);
    const auto raster = find(queue.context, 0x205);
    if (stages == queue.context.end() || primitive == queue.userConfig.end() || raster == queue.context.end()) return 0;
    if ((stages->second & 0x24u) != 0) return 0;
    if ((raster->second & 8u) != 0 && (raster->second & 0x7e0u) != 0x240u) return 0;
    switch (primitive->second) {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 17:
        case 21:
        case 19:
            return 1u << 16u;
        default:
            return 0;
    }
}

bool pointOutput(const QueueState& queue) {
    const auto stages = find(queue.context, 0x2d5);
    const auto primitive = find(queue.userConfig, 0x242, RegisterBank::UserConfig);
    if (stages == queue.context.end()) return false;
    if ((stages->second & 0x24u) == 0) return primitive != queue.userConfig.end() && primitive->second == 1u;
    const auto output = find(queue.context, 0x29b);
    return (stages->second & 0x24u) == 0x20u && output != queue.context.end() && output->second == 0u;
}

void decodePointSize(const QueueState& queue, State& result) {
    const auto& cx = queue.context;
    const auto size = read(cx, 0x280);
    const auto limits = read(cx, 0x281);
    const bool fromVertex = (result.paClVsOutCntl & (1u << 16u)) != 0;
    Require(!fromVertex || (result.paClVsOutCntl & (1u << 21u)) != 0, "USE_VTX_POINT_SIZE without VS_OUT_MISC_VEC_ENA is unsupported");
    Require(fromVertex || (size & 0xffffu) == size >> 16u, "points whose PA_SU_POINT_SIZE width and height differ are unsupported");
    Require(fromVertex || (size & 0xffffu) != 0, "points of size 0 (PA_SU_POINT_SIZE 0) are unsupported");
    Require((limits & 0xffffu) <= limits >> 16u, "PA_SU_POINT_MINMAX with a minimum above its maximum is unsupported");
    Require(!fromVertex || (limits >> 16u) != 0, "a vertex point size clamped to 0 (PA_SU_POINT_MINMAX maximum 0) is unsupported");
    result.pointSize = static_cast<float>(size & 0xffffu) / 8.0f;
    result.pointSizeMin = static_cast<float>(limits & 0xffffu) / 8.0f;
    result.pointSizeMax = static_cast<float>(limits >> 16u) / 8.0f;
    const auto interpolation = find(cx, 0x1b5);
    const auto inputs = find(cx, 0x1b6);
    if (interpolation == cx.end() || (interpolation->second & 2u) == 0 || inputs == cx.end()) return;
    for (std::uint32_t input = 0; input < (inputs->second & 0x3fu); ++input) {
        const auto control = find(cx, 0x191 + input);
        Require(control == cx.end() || (control->second & 0x820000u) == 0, "point sprite texture coordinates (SPI_PS_INPUT_CNTL PT_SPRITE_TEX) are not implemented");
    }
}

constexpr std::uint32_t DepthControlMask = ~0x007007f0u;
// EXEC_ON_HIER_FAIL / EXEC_ON_NOOP / EXEC_IF_OVERLAPPED (bits 9, 10, 17) only force the pixel shader
// to run, which it always does here.
constexpr std::uint32_t ShaderControlMask = ~(0x0000f870u | 0x00020600u | 0x00010000u);
constexpr std::uint32_t PixelStageRunsMask = 0x000207c7u;
constexpr std::uint32_t AlphaToCoverageMask = ~0x0001ff00u;
constexpr std::uint32_t AlphaToCoverageUndithered = 0x0000aa01u;
constexpr std::uint32_t AlphaToCoverageDithered = 0x00018701u;
constexpr std::uint32_t AlphaToCoverageTwoSampleDithered = 0x0000e701u;

bool ditheredAlphaToCoverageWord(std::uint32_t word) {
    return word == AlphaToCoverageDithered || word == AlphaToCoverageTwoSampleDithered;
}

bool alphaToCoverageDisabled(const Registers& cx) {
    const auto control = find(cx, 0x203);
    return control != cx.end() && (control->second & 0x800u) != 0;
}

bool ditheredAlphaSamplesSupported(const Registers& cx) {
    const auto config = find(cx, 0x2f8);
    const auto eqaa = find(cx, 0x201);
    return config != cx.end() && (config->second & 0x700007u) == 0x300003u && eqaa != cx.end() && (eqaa->second & 0x207777u) == 0x3303u;
}

bool twoSampleDitheredAlphaSamplesSupported(const Registers& cx) {
    const auto config = find(cx, 0x2f8);
    const auto eqaa = find(cx, 0x201);
    return config != cx.end() && (config->second & 0x700007u) == 0x100001u && eqaa != cx.end() && (eqaa->second & 0x207077u) == 0x1001u && ((eqaa->second >> 8u) & 7u) <= 1u;
}

bool twoSampleDitheredAlphaLocationsSupported(const Registers& cx) {
    const auto first = find(cx, 0x2fe);
    if (first == cx.end() || (first->second != 0xcc44u && first->second != 0xc44cu)) return false;
    for (std::uint32_t pixel = 1; pixel < 4; ++pixel) {
        const auto locations = find(cx, 0x2fe + pixel * 4u);
        if (locations == cx.end() || locations->second != first->second) return false;
    }
    return true;
}

bool ditheredAlphaLocationsSupported(const Registers& cx) {
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        const auto first = find(cx, 0x2fe + pixel * 4u);
        const auto second = find(cx, 0x2ff + pixel * 4u);
        if (first == cx.end() || second == cx.end() || first->second != 0xbd153fd1u || second->second != 0x9773f95bu) return false;
    }
    return true;
}
constexpr std::uint32_t ScanModeMask = ~0x22u;
constexpr std::uint32_t ScanControlMask = ~0x76033fffu;
constexpr std::uint32_t PixelShaderIteratesSamples = 0x10000u;
constexpr std::uint32_t PixelSampleCoverageInput = 0x4000u;
constexpr std::uint32_t PixelSampleRateInputs = 0x2011u;
constexpr std::uint32_t ScreenOffsetMask = ~0x01ff01ffu;
// Bits 26/27 (ZCLIP_NEAR/FAR_DISABLE) become depth clamping; bit 19 selects the [0, 1] clip space;
// bit 16 disables clipping.
constexpr std::uint32_t ClipControlMask = ~(0x10000u | 0x80000u | 0x01000000u | 0x0c000000u);

bool zFormatSupported(std::uint32_t format) {
    return format == 0u || format == 1u || format == 2u || format == 3u || format == 9u;
}

std::uint32_t shaderControlMask(std::uint32_t zFormat) {
    constexpr std::uint32_t zExportEnable = 0x1u;
    constexpr std::uint32_t stencilTestValExportEnable = 0x2u;
    constexpr std::uint32_t stencilOpValExportEnable = 0x4u;
    constexpr std::uint32_t maskExportEnable = 0x100u;
    auto mask = ShaderControlMask;
    if (zFormat != 0u && zFormatSupported(zFormat)) mask &= ~zExportEnable;
    if (zFormat == 2u || zFormat == 9u) mask &= ~(stencilTestValExportEnable | stencilOpValExportEnable);
    if (zFormat == 9u) mask &= ~maskExportEnable;
    return mask;
}

// Debug aid: APS5_IGNORE_DEPTH_TEST=1 renders depth- and stencil-tested draws without a depth
// target as if their tests always passed (wrong occlusion, but the draws run), so stages that
// depth-test everything can exercise the draw paths before depth targets exist.
bool IgnoreDepthTest() {
    static const bool ignore = std::getenv("APS5_IGNORE_DEPTH_TEST") != nullptr;
    return ignore;
}

bool depthSurfaceBound(const Registers& cx) {
    const auto z = find(cx, 0x010);
    const auto stencil = find(cx, 0x011);
    return (z != cx.end() && (z->second & 3u) != 0) || (stencil != cx.end() && (stencil->second & 1u) != 0);
}

std::uint32_t planeTests(const Registers& cx, std::uint32_t depthControl) {
    const auto z = find(cx, 0x010);
    const auto stencil = find(cx, 0x011);
    if (z != cx.end() && (z->second & 3u) == 0) depthControl &= ~2u;
    if (stencil != cx.end() && (stencil->second & 1u) == 0) depthControl &= ~1u;
    return depthControl & 0xbu;
}

bool depthPlanesAbsent(const Registers& cx) {
    const auto z = find(cx, 0x010);
    const auto stencil = find(cx, 0x011);
    return z != cx.end() && stencil != cx.end() && (z->second & 3u) == 0 && (stencil->second & 1u) == 0;
}

VkStencilOpState stencilFace(std::uint32_t compare, std::uint32_t ops, std::uint32_t refMask, bool readOnly, bool testValExport, bool opValExport, bool opValReference) {
    VkStencilOpState face{};
    face.compareOp = static_cast<VkCompareOp>(compare);
    face.compareMask = (refMask >> 8u) & 0xffu;
    face.writeMask = readOnly ? 0u : (refMask >> 16u) & 0xffu;
    const auto test = refMask & 0xffu;
    const auto opValue = refMask >> 24u;
    auto reference = test;
    auto fixed = compare == 0 || compare == 7 ? 0u : face.compareMask;
    std::uint32_t invertMask = 0;
    std::uint32_t inverted = 0;
    std::uint32_t logicMask = 0;
    const auto logic = [](std::uint32_t op) { return op == 10 || op == 11 || op == 12 || op == 15; };
    const auto convert = [&](std::uint32_t op, bool testFailed) {
        if (face.writeMask == 0) return VK_STENCIL_OP_KEEP;
        char detail[128];
        if (opValExport && ((op == 4 && !opValReference) || (op >= 5 && op != 7))) {
            std::snprintf(detail, sizeof(detail), "AGC graphics: stencil operation %u with a pixel shader that exports the stencil operation value is unsupported", op);
            throw std::runtime_error(detail);
        }
        if (logic(op)) {
            const auto bits = (op == 11 || op == 12 ? opValue : ~opValue) & face.writeMask;
            if (bits == 0) return VK_STENCIL_OP_KEEP;
            if (logicMask != 0 && logicMask != bits) {
                std::snprintf(detail, sizeof(detail), "AGC graphics: stencil logic operations on write masks 0x%02x and 0x%02x are unsupported", logicMask, bits);
                throw std::runtime_error(detail);
            }
            logicMask = bits;
            if (op != 11) return op == 10 ? VK_STENCIL_OP_ZERO : VK_STENCIL_OP_INVERT;
            if (testValExport) throw std::runtime_error("AGC graphics: stencil OR is unsupported with a pixel shader that exports the stencil test value");
            if ((~reference & fixed & bits) != 0) {
                std::snprintf(detail, sizeof(detail), "AGC graphics: stencil OR of 0x%02x against test value 0x%02x (masks 0x%02x/0x%02x) is unsupported", opValue, test, face.compareMask, face.writeMask);
                throw std::runtime_error(detail);
            }
            reference |= bits;
            fixed |= bits;
            return VK_STENCIL_OP_REPLACE;
        }
        switch (op) {
            case 0: return VK_STENCIL_OP_KEEP;
            case 1: return VK_STENCIL_OP_ZERO;
            case 2:
            case 3:
            case 4: {
                if (testValExport) {
                    if (op != 3) throw std::runtime_error("AGC graphics: stencil replacement by a value other than the test value is unsupported with a pixel shader that exports the stencil test value");
                    return VK_STENCIL_OP_REPLACE;
                }
                if (opValReference) {
                    if (op != 4) throw std::runtime_error("AGC graphics: stencil replacement by a value other than the operation value is unsupported with a pixel shader that exports the stencil operation value");
                    return VK_STENCIL_OP_REPLACE;
                }
                const auto value = op == 2 ? 0xffu : op == 3 ? test : opValue;
                if (((reference ^ value) & fixed & face.writeMask) != 0) {
                    const auto compared = test & face.compareMask;
                    const bool passKnown = compare == 2 || (compare == 3 && compared == face.compareMask) || (compare == 6 && compared == 0u);
                    const bool failKnown = compare == 5 || (compare == 1 && compared == 0u) || (compare == 4 && compared == face.compareMask);
                    const bool testValueKnown = testFailed ? failKnown : passKnown;
                    const auto flipped = (test ^ value) & face.writeMask;
                    if (testValueKnown && (face.writeMask & ~face.compareMask) == 0 && (invertMask == 0 || invertMask == flipped)) {
                        invertMask = flipped;
                        ++inverted;
                        return VK_STENCIL_OP_INVERT;
                    }
                    std::snprintf(detail, sizeof(detail), "AGC graphics: stencil replacement 0x%02x against test value 0x%02x (masks 0x%02x/0x%02x) is unsupported", value, test, face.compareMask, face.writeMask);
                    throw std::runtime_error(detail);
                }
                reference = (reference & ~face.writeMask) | (value & face.writeMask);
                fixed |= face.writeMask;
                return VK_STENCIL_OP_REPLACE;
            }
            case 5:
            case 6:
            case 8:
            case 9:
                if (opValue != 1) {
                    std::snprintf(detail, sizeof(detail), "AGC graphics: stencil add/subtract of 0x%02x is unsupported", opValue);
                    throw std::runtime_error(detail);
                }
                return op == 5 ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : op == 6 ? VK_STENCIL_OP_DECREMENT_AND_CLAMP : op == 8 ? VK_STENCIL_OP_INCREMENT_AND_WRAP : VK_STENCIL_OP_DECREMENT_AND_WRAP;
            case 7: return VK_STENCIL_OP_INVERT;
            default:
                std::snprintf(detail, sizeof(detail), "AGC graphics: stencil operation %u is unsupported", op);
                throw std::runtime_error(detail);
        }
    };
    face.failOp = convert(ops & 0xfu, true);
    face.passOp = convert((ops >> 4u) & 0xfu, false);
    face.depthFailOp = convert((ops >> 8u) & 0xfu, false);
    if (invertMask != 0) {
        const auto active = ((ops & 0xfu) != 0u) + (((ops >> 4u) & 0xfu) != 0u) + (((ops >> 8u) & 0xfu) != 0u);
        if (inverted != active) throw std::runtime_error("AGC graphics: a stencil replacement drawn as an inversion beside another stencil operation is unsupported");
        face.writeMask = invertMask;
    }
    if (logicMask != 0) {
        const auto other = [&](std::uint32_t op) { return op != 0 && !logic(op); };
        if ((other(ops & 0xfu) || other((ops >> 4u) & 0xfu) || other((ops >> 8u) & 0xfu)) && logicMask != face.writeMask) {
            char detail[128];
            std::snprintf(detail, sizeof(detail), "AGC graphics: stencil operations 0x%03x combine a logic operation on write mask 0x%02x with write mask 0x%02x", ops & 0xfffu, logicMask, face.writeMask);
            throw std::runtime_error(detail);
        }
        face.writeMask = logicMask;
    }
    face.reference = reference;
    Require(!opValReference || compare == 0 || compare == 7 || face.compareMask == 0, "a stencil compare against the test value is unsupported with a pixel shader that exports the stencil operation value");
    return face;
}

}
}
