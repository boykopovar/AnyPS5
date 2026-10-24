#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
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
    if (!std::isfinite(state.viewport.minDepth) || !std::isfinite(state.viewport.maxDepth)) return;
    state.viewport.minDepth = std::clamp(state.viewport.minDepth, 0.0f, 1.0f);
    state.viewport.maxDepth = std::clamp(state.viewport.maxDepth, 0.0f, 1.0f);
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
        if ((line->second & 0xffffu) != 8u) throw std::runtime_error("AGC graphics: " + zeroMessage(0x282, line->second, "a line polygon mode wider or narrower than one pixel"));
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

bool alphaToCoverageDisabled(const Registers& cx) {
    const auto control = find(cx, 0x203);
    return control != cx.end() && (control->second & 0x800u) != 0;
}

bool ditheredAlphaSamplesSupported(const Registers& cx) {
    const auto config = find(cx, 0x2f8);
    const auto eqaa = find(cx, 0x201);
    return config != cx.end() && (config->second & 0x700007u) == 0x300003u && eqaa != cx.end() && (eqaa->second & 0x207777u) == 0x3303u;
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
    const auto convert = [&](std::uint32_t op, bool testFailed) {
        if (face.writeMask == 0) return VK_STENCIL_OP_KEEP;
        char detail[112];
        if (opValExport && ((op == 4 && !opValReference) || op == 5 || op == 6 || op == 8 || op == 9)) {
            std::snprintf(detail, sizeof(detail), "AGC graphics: stencil operation %u with a pixel shader that exports the stencil operation value is unsupported", op);
            throw std::runtime_error(detail);
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
    face.reference = reference;
    Require(!opValReference || compare == 0 || compare == 7 || face.compareMask == 0, "a stencil compare against the test value is unsupported with a pixel shader that exports the stencil operation value");
    return face;
}

DepthTarget decodeDepthPlane(const Registers& cx) {
    const auto viewRegister = find(cx, 0x002);
    const auto view = viewRegister == cx.end() ? 0u : viewRegister->second;
    const auto viewMip = (view >> 26u) & 0xfu;
    const auto maxMip = (read(cx, 0x010) >> 16u) & 0xfu;
    const auto zFormat = read(cx, 0x010) & 3u;
    const auto stencilRegister = find(cx, 0x011);
    const auto stencilInfo = stencilRegister == cx.end() ? 0u : stencilRegister->second;
    const bool stencil = (stencilInfo & 1u) != 0;
    const auto base = [&](std::uint32_t low, std::uint32_t highOffset) {
        const auto high = find(cx, highOffset);
        return (high == cx.end() ? 0ull : static_cast<std::uint64_t>(high->second & 0xffu) << 40u) | (static_cast<std::uint64_t>(read(cx, low)) << 8u);
    };
    DepthTarget depth{};
    depth.samples = 1u << ((read(cx, 0x010) >> 2u) & 3u);
    depth.depthSwizzle = (read(cx, 0x010) >> 4u) & 31u;
    depth.iterate256 = (read(cx, 0x010) & (1u << 20u)) != 0;
    depth.firstSlice = view & 0x1fffu;
    depth.lastSlice = (view >> 13u) & 0x7ffu;
    depth.address = zFormat != 0 ? base(0x012, 0x01a) : 0;
    depth.stencilAddress = stencil ? base(0x013, 0x01b) : 0;
    if (depth.samples != 1u) {
        depth.pipeBankXor = static_cast<std::uint32_t>(depth.address & 65535u);
        depth.address &= ~std::uint64_t{65535u};
        depth.stencilAddress &= ~std::uint64_t{65535u};
        if (const auto surface = find(cx, 0x2af); surface != cx.end()) {
            depth.htilePipeAligned = (surface->second & (1u << 18u)) != 0;
            depth.htileVrsEncoding = (surface->second >> 19u) & 3u;
        }
    }
    const auto size = read(cx, 0x007);
    depth.extent = {(size & 0x3fffu) + 1u, ((size >> 16u) & 0x3fffu) + 1u};
    if (maxMip == 0 && viewMip != 0) Require(false, "depth view mip " + std::to_string(viewMip) + " on a surface without mips");
    if (maxMip != 0) {
        char message[320];
        std::snprintf(message, sizeof(message), "depth view mip %u of %u at %ux%u (DB_Z_INFO 0x%08x, DB_DEPTH_VIEW 0x%08x)", viewMip, maxMip + 1u, depth.extent.width, depth.extent.height, read(cx, 0x010), view);
        const std::string where(message);
        Require(viewMip <= maxMip, "the " + where + " is beyond the surface's mips");
        Require(depth.samples == 1u, "multisampled " + where + " is unsupported");
        Require(depth.depthSwizzle == XorSwizzleMode(TextureTileMode::kZ64KBX), "the " + where + " uses a swizzle other than 64 KiB Z_X");
        const auto depthMips = ComputeElementMipLayout(TextureTileMode::kZ64KBX, zFormat == 1 ? 2u : 4u, depth.extent.width, depth.extent.height, maxMip + 1u);
        const auto stencilMips = ComputeElementMipLayout(TextureTileMode::kZ64KBX, 1u, depth.extent.width, depth.extent.height, maxMip + 1u);
        Require(zFormat == 0 || !depthMips.at(viewMip).tail, "rendering the " + where + " into the packed mip tail is unsupported");
        Require(!stencil || !stencilMips.at(viewMip).tail, "rendering the stencil of the " + where + " into the packed mip tail is unsupported");
        Require((read(cx, 0x010) & (1u << 29u)) == 0, "HTILE for the " + where + " is unsupported");
        if (const auto slice = view & 0x1fffu; slice != 0) {
            if (depth.address != 0) depth.address += static_cast<std::uint64_t>(slice) * ComputeSurfaceSize(depthMips, 1u);
            if (depth.stencilAddress != 0) depth.stencilAddress += static_cast<std::uint64_t>(slice) * ComputeSurfaceSize(stencilMips, 1u);
        }
        if (depth.address != 0) depth.address += depthMips.at(viewMip).tiledOffset;
        if (depth.stencilAddress != 0) depth.stencilAddress += stencilMips.at(viewMip).tiledOffset;
        depth.extent = {depthMips.at(viewMip).width, depthMips.at(viewMip).height};
    } else if (const auto slice = view & 0x1fffu; slice != 0) {
        if (depth.address != 0) depth.address += static_cast<std::uint64_t>(slice) * DepthSliceBytes(depth.extent, zFormat == 1 ? 2u : 4u);
        if (depth.stencilAddress != 0) depth.stencilAddress += static_cast<std::uint64_t>(slice) * DepthSliceBytes(depth.extent, 1u);
    }
    depth.format = zFormat == 1 ? (stencil ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D16_UNORM) : (stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT);
    depth.htileDepthOnly = (stencilInfo & (1u << 29u)) != 0u;
    if (const auto htile = find(cx, 0x005); zFormat != 0 && htile != cx.end() && htile->second != 0 && (read(cx, 0x010) & (1u << 29u)) != 0) {
        depth.htileAddress = base(0x005, 0x01e) + static_cast<std::uint64_t>(view & 0x1fffu) * HtileMetadataBytes(depth.extent);
        depth.htileStencil = stencil && !depth.htileDepthOnly;
    }
    return depth;
}

void decodeDepth(const Registers& cx, std::uint32_t depthControl, bool testValExport, bool opValExport, State& result) {
    const bool opValReference = opValExport && StencilOperationValueReplaces(cx);
    zero(cx, 0x000, 0x0000101cu, "depth copy, resummarize or decompress draws (DB_RENDER_CONTROL)");
    const bool depthClear = (read(cx, 0x000) & 1u) != 0;
    const auto view = read(cx, 0x002);
    zero(cx, 0x010, 0x00001000u, "partially resident depth (DB_Z_INFO)");
    zero(cx, 0x011, 0x00001000u, "partially resident stencil (DB_STENCIL_INFO)");
    const auto zFormat = read(cx, 0x010) & 3u;
    const bool stencil = (read(cx, 0x011) & 1u) != 0;
    const bool stencilClear = stencil && (read(cx, 0x000) & 2u) != 0;
    Require(zFormat != 2, "Z_24 depth is unsupported");
    if (zFormat == 0) depthControl &= ~6u;
    if (!stencil) depthControl &= ~1u;
    const auto base = [&](std::uint32_t low, std::uint32_t highOffset) {
        const auto high = find(cx, highOffset);
        return (high == cx.end() ? 0ull : static_cast<std::uint64_t>(high->second & 0xffu) << 40u) | (static_cast<std::uint64_t>(read(cx, low)) << 8u);
    };
    const bool depthReadOnly = (view & 0x01000000u) != 0;
    const bool stencilReadOnly = (view & 0x02000000u) != 0;
    Require(!stencilClear || !stencilReadOnly, "stencil clear requires a writable stencil plane");
    Require(!stencilClear || !testValExport, "stencil clear with a pixel shader that exports the stencil test value is unsupported");
    Require(!depthClear || (zFormat != 0 && !depthReadOnly), "depth clear requires a writable depth plane");
    const auto readAddress = zFormat != 0 ? base(0x012, 0x01a) : 0;
    const auto stencilReadAddress = stencil ? base(0x013, 0x01b) : 0;
    const bool depthWritten = zFormat != 0 && !depthReadOnly && (depthClear || (depthControl & 6u) == 6u || (read(cx, 0x000) & 0x60u) != 0);
    Require(!depthWritten || base(0x014, 0x01c) == readAddress, "depth read and written at different addresses is unsupported");
    Require(!stencil || stencilReadOnly || base(0x015, 0x01d) == stencilReadAddress, "stencil read and written at different addresses is unsupported");
    auto depth = decodeDepthPlane(cx);
    depth.clearDepth = readFloat(cx, 0x00b);
    depth.clearStencil = static_cast<std::uint8_t>(read(cx, 0x00a) & 0xffu);
    result.depth = depth;
    result.depthTest = (depthControl & 2u) != 0;
    result.depthWrite = result.depthTest && (depthControl & 4u) != 0 && !depthReadOnly;
    result.depthCompare = static_cast<VkCompareOp>((depthControl >> 4u) & 7u);
    result.depthBoundsTest = (depthControl & 8u) != 0;
    if (result.depthBoundsTest) {
        Require(zFormat != 0, "depth bounds without a depth plane");
        result.minDepthBounds = readDepthBound(cx, 0x008);
        result.maxDepthBounds = readDepthBound(cx, 0x009);
    }
    if (depthClear) {
        result.depthTest = true;
        result.depthWrite = true;
        result.depthCompare = VK_COMPARE_OP_ALWAYS;
        result.depthBoundsTest = false;
    }
    result.stencilTest = (depthControl & 1u) != 0;
    if (stencilClear) {
        VkStencilOpState clear{};
        clear.failOp = VK_STENCIL_OP_REPLACE;
        clear.passOp = VK_STENCIL_OP_REPLACE;
        clear.depthFailOp = VK_STENCIL_OP_REPLACE;
        clear.compareOp = VK_COMPARE_OP_ALWAYS;
        clear.compareMask = 0xffu;
        clear.writeMask = 0xffu;
        clear.reference = depth.clearStencil;
        result.stencilTest = true;
        result.stencilFront = clear;
        result.stencilBack = clear;
    } else if (result.stencilTest) {
        const auto ops = read(cx, 0x10b);
        result.stencilFront = stencilFace((depthControl >> 8u) & 7u, ops, read(cx, 0x10c), stencilReadOnly, testValExport, opValExport, opValReference);
        result.stencilBack = (depthControl & 0x80u) != 0 ? stencilFace((depthControl >> 20u) & 7u, ops >> 12u, read(cx, 0x10d), stencilReadOnly, testValExport, opValExport, opValReference) : result.stencilFront;
    }
}

void decodeDepthBias(const Registers& cx, std::uint32_t raster, State& result) {
    const bool front = (result.cullMode & VK_CULL_MODE_FRONT_BIT) == 0;
    const bool back = (result.cullMode & VK_CULL_MODE_BACK_BIT) == 0;
    const bool frontBias = (raster & 0x800u) != 0;
    const bool backBias = (raster & 0x1000u) != 0;
    if (!(front && frontBias) && !(back && backBias)) return;
    const bool perFace = front && back && (frontBias != backBias || read(cx, 0x2e0) != read(cx, 0x2e2) || read(cx, 0x2e1) != read(cx, 0x2e3));
    const bool d16 = result.depth->format == VK_FORMAT_D16_UNORM || result.depth->format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = find(cx, 0x2de) == cx.end() ? (d16 ? 0xf0u : 0x1e9u) : read(cx, 0x2de);
    if (format != (d16 ? 0xf0u : 0x1e9u)) throw std::runtime_error("AGC graphics: " + zeroMessage(0x2de, format, "depth bias in units other than the depth format"));
    const auto scale = front && frontBias ? 0x2e0u : 0x2e2u;
    result.depthBias = true;
    result.depthBiasSlope = frontBias || !perFace ? readFloat(cx, scale) / 16.0f : 0.0f;
    result.depthBiasConstant = frontBias || !perFace ? readFloat(cx, scale + 1u) : 0.0f;
    result.depthBiasClamp = readFloat(cx, 0x2df);
    if (!perFace) return;
    result.depthBiasPerFace = true;
    result.backDepthBiasSlope = backBias ? readFloat(cx, 0x2e2) / 16.0f : 0.0f;
    result.backDepthBiasConstant = backBias ? readFloat(cx, 0x2e3) : 0.0f;
}

bool depthPassThrough(std::uint32_t depthControl) {
    const bool stencil = (depthControl & 1u) != 0;
    const bool depth = (depthControl & 2u) != 0;
    const bool depthWrite = (depthControl & 4u) != 0;
    const bool passThroughDepth = !depth || (!depthWrite && ((depthControl >> 4u) & 7u) == 7u);
    const bool passThroughStencil = !stencil || (((depthControl >> 8u) & 7u) == 7u && ((depthControl & 0x80u) == 0 || ((depthControl >> 20u) & 7u) == 7u));
    return (stencil || depth) && passThroughDepth && passThroughStencil;
}

std::uint32_t effectiveDepthControl(std::uint32_t depthControl) {
    return (depthControl & 3u) == 0 ? depthControl & ~4u : depthControl;
}

constexpr std::array<std::array<std::int8_t, 2>, 2> StandardLocations2{{{4, 4}, {-4, -4}}};
constexpr std::array<std::array<std::int8_t, 2>, 4> StandardLocations4{{{-2, -6}, {6, -2}, {-6, 2}, {2, 6}}};
constexpr std::array<std::array<std::int8_t, 2>, 8> StandardLocations8{{{1, -3}, {-1, 3}, {5, 1}, {-3, -5}, {-5, 5}, {-7, -1}, {3, 7}, {7, -7}}};

std::string nonTriangleRejection(const QueueState& queue, const char* message, bool rectangles = false) {
    const auto word = [](const Registers& registers, std::uint32_t offset, RegisterBank bank = RegisterBank::Context) -> std::optional<std::uint32_t> {
        const auto it = find(registers, offset, bank);
        if (it == registers.end()) return std::nullopt;
        return it->second;
    };
    const auto& cx = queue.context;
    const auto stages = word(cx, 0x2d5);
    if (!stages) return {};
    if ((*stages & 0x20u) != 0) {
        if (const auto output = word(cx, 0x29b); output && ((*output & 0x80000000u) != 0 || (*output & 0x3fu) != 2u)) return zeroMessage(0x29b, *output, message);
    } else if ((*stages & 4u) != 0) {
        if (const auto parameters = word(cx, 0x2db)) {
            const auto domain = *parameters & 3u;
            const auto topology = (*parameters >> 5u) & 7u;
            if ((domain != 1u && domain != 2u) || (topology != 2u && topology != 3u)) return zeroMessage(0x2db, *parameters, message);
        }
    } else if (const auto primitive = word(queue.userConfig, 0x242, RegisterBank::UserConfig); primitive && *primitive != 4u && *primitive != 5u && *primitive != 6u && *primitive != 21u && !(rectangles && (*primitive == 7u || *primitive == 17u))) {
        return zeroMessage(0x242, *primitive, message);
    }
    return {};
}

std::string singleSampleMsaaRejection(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto word = [](const Registers& registers, std::uint32_t offset, RegisterBank bank = RegisterBank::Context) -> std::optional<std::uint32_t> {
        const auto it = find(registers, offset, bank);
        if (it == registers.end()) return std::nullopt;
        return it->second;
    };
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        const auto offset = 0x2feu + pixel * 4u;
        if (const auto locations = word(cx, offset); locations && (*locations & 0xffu) != 0) return zeroMessage(offset, *locations, "MSAA_ENABLE on a single-sample target whose sample is off the pixel center");
    }
    if (const auto alphaToMask = word(cx, 0x2dc); alphaToMask && (*alphaToMask & 1u) != 0 && !(*alphaToMask == AlphaToCoverageDithered && alphaToCoverageDisabled(cx))) return zeroMessage(0x2dc, *alphaToMask, "MSAA_ENABLE on a single-sample target with alpha-to-coverage");
    if (const auto conservative = word(cx, 0x313); conservative && (*conservative & ~0x001f6000u) != 0) return zeroMessage(0x313, *conservative, "MSAA_ENABLE on a single-sample target with conservative rasterization");
    return nonTriangleRejection(queue, "MSAA_ENABLE on a single-sample target for primitives other than triangles and rectangles", true);
}

std::string singleSampleDepthScanRejection(const QueueState& queue) {
    if (auto reason = nonTriangleRejection(queue, "a single-sample scan into a multisampled depth target for primitives other than triangles and rectangles", true); !reason.empty()) return reason;
    const auto& cx = queue.context;
    if (const auto eqaa = find(cx, 0x201); eqaa != cx.end() && (eqaa->second & 0x0f0c0000u) != 0) return zeroMessage(0x201, eqaa->second, "a single-sample scan into a multisampled depth target with Z interpolation or overrasterization (DB_EQAA)");
    if (PixelProgramSkipped(queue)) return {};
    constexpr std::uint32_t sampleIndex = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    const auto ena = find(cx, 0x1b3);
    const auto addr = find(cx, 0x1b4);
    if (ena != cx.end() && addr != cx.end() && (ena->second & addr->second & sampleIndex) != 0) return zeroMessage(0x1b3, ena->second, "a single-sample scan into a multisampled depth target with a pixel program reading the sample index");
    if (const auto control = find(cx, 0x203); control != cx.end() && (control->second & 0x100u) != 0) return zeroMessage(0x203, control->second, "a single-sample scan into a multisampled depth target with a sample-mask export");
    return {};
}

constexpr std::uint32_t CoveredCentroidIsCenter = 1u << 29u;
constexpr std::uint32_t SampleZeroRefusedInputs = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::LinearCentroid) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage);
constexpr std::uint32_t CentroidInputs = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCentroid) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::LinearCentroid);

bool staticAnchors(const Registers& cx, std::uint32_t depthSamples) {
    const auto eqaa = find(cx, 0x201);
    return eqaa != cx.end() && (eqaa->second & 7u) == static_cast<std::uint32_t>(std::countr_zero(depthSamples)) && (eqaa->second & 0x100000u) != 0;
}

bool overrasterized(const Registers& cx, std::uint32_t samples) {
    const auto eqaa = find(cx, 0x201);
    const auto mode = find(cx, 0x292);
    return samples > 1u && eqaa != cx.end() && mode != cx.end() && (mode->second & 1u) != 0 && ((eqaa->second >> 24u) & 7u) == static_cast<std::uint32_t>(std::countr_zero(samples));
}

bool sampleCoverageSupported(const QueueState& queue, std::uint32_t samples, bool perSampleShading) {
    if (samples == 1u || PixelProgramSkipped(queue)) return true;
    const auto ena = find(queue.context, 0x1b3);
    const auto addr = find(queue.context, 0x1b4);
    if (ena == queue.context.end() || addr == queue.context.end()) return true;
    const auto inputs = ena->second & addr->second;
    return (inputs & PixelSampleCoverageInput) == 0u || (!perSampleShading && (inputs & PixelSampleRateInputs) == 0u);
}

bool pixelRateInputsKept(const QueueState& queue, const State& state) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    constexpr std::uint32_t numbered = PixelInputBit(PixelInput::Ancillary) | PixelInputBit(PixelInput::SampleCoverage);
    constexpr std::uint32_t located = PixelInputBit(PixelInput::PerspectiveSample) | PixelInputBit(PixelInput::PerspectiveCentroid) | PixelInputBit(PixelInput::PerspectivePullModel) | PixelInputBit(PixelInput::LinearSample) | PixelInputBit(PixelInput::LinearCentroid) | PixelInputBit(PixelInput::PositionZ) | PixelInputBit(PixelInput::PositionW);
    const bool centred = state.customSampleLocations && std::ranges::all_of(state.sampleLocations, [](std::uint8_t location) { return location == 0u; });
    const auto moving = numbered | (centred ? 0u : located);
    if (PixelProgramSkipped(queue)) return true;
    const auto ena = find(queue.context, 0x1b3);
    const auto addr = find(queue.context, 0x1b4);
    if (ena == queue.context.end() || addr == queue.context.end()) return true;
    const auto inputs = ena->second & addr->second;
    if ((inputs & moving) != 0u) return false;
    if (centred || (inputs & (PixelInputBit(PixelInput::PositionX) | PixelInputBit(PixelInput::PositionY))) == 0u) return true;
    const auto position = find(queue.context, 0x1b8);
    const auto control = position == queue.context.end() ? 0u : position->second;
    return (control & 0x100000u) != 0u || ((control >> 16u) & 3u) == 0u;
}

bool coveredCentroidSupported(const QueueState& queue) {
    const auto config = find(queue.context, 0x2f8);
    if (config == queue.context.end() || (config->second & CoveredCentroidIsCenter) == 0 || PixelProgramSkipped(queue)) return true;
    const auto ena = find(queue.context, 0x1b3);
    const auto addr = find(queue.context, 0x1b4);
    return ena != queue.context.end() && addr != queue.context.end() && (ena->second & addr->second & CentroidInputs) == 0;
}

std::array<std::uint32_t, 4> sampleMaskFields(std::uint32_t top, std::uint32_t bottom) {
    return {top & 0xffffu, top >> 16u, bottom & 0xffffu, bottom >> 16u};
}

std::string sampleMaskRejection(const QueueState& queue, std::uint32_t config, std::uint32_t top, std::uint32_t bottom) {
    const auto log2 = config & 7u;
    if (log2 > 3u) return {};
    const auto full = (1u << (1u << log2)) - 1u;
    auto masks = sampleMaskFields(top, bottom);
    for (auto& mask : masks) mask &= full;
    if (std::ranges::all_of(masks, [&](std::uint32_t mask) { return mask == full; })) return {};
    const bool uniform = std::ranges::all_of(masks, [&](std::uint32_t mask) { return mask == masks[0]; });
    const bool wholePixels = std::ranges::all_of(masks, [&](std::uint32_t mask) { return mask == 0u || mask == full; });
    char detail[96];
    std::snprintf(detail, sizeof(detail), " (PA_SC_AA_MASK_X0Y0_X1Y0=0x%08x, PA_SC_AA_MASK_X0Y1_X1Y1=0x%08x)", top, bottom);
    const auto conservative = find(queue.context, 0x313);
    if (conservative != queue.context.end() && (conservative->second & ~0x001f6000u) != 0) return std::string("AGC graphics: sample masks with conservative rasterization are unsupported") + detail;
    if (!wholePixels && uniform && !PixelProgramSkipped(queue)) {
        const auto ena = find(queue.context, 0x1b3);
        const auto addr = find(queue.context, 0x1b4);
        if (ena != queue.context.end() && addr != queue.context.end() && (ena->second & addr->second & CentroidInputs) != 0) return std::string("AGC graphics: sample masks that remove some samples of a pixel with centroid interpolation are unsupported") + detail;
    }
    return {};
}

std::array<std::uint16_t, 4> decodeQuadSampleMask(const Registers& cx, std::uint32_t samples) {
    const bool singleSample = (read(cx, 0x2f8) & 7u) == 0u;
    const auto full = (1u << samples) - 1u;
    const auto fields = sampleMaskFields(read(cx, 0x30e), read(cx, 0x30f));
    std::array<std::uint16_t, 4> masks{};
    for (std::size_t pixel = 0; pixel < masks.size(); ++pixel) masks[pixel] = static_cast<std::uint16_t>(singleSample ? ((fields[pixel] & 1u) != 0u ? full : 0u) : fields[pixel] & full);
    return masks;
}

std::uint32_t decodeSamples(const Registers& cx) {
    const auto config = read(cx, 0x2f8);
    zero(cx, 0x2f8, ~(0x7u | 0x1e000u | 0x700000u | CoveredCentroidIsCenter), "coverage conversion or nonstandard multisampling");
    const auto log2 = config & 7u;
    if (log2 == 0) {
        Require(((config >> 20u) & 7u) == 0, "exposed samples without multisampling are unsupported");
        return 1;
    }
    Require(log2 <= 3u, "more than eight samples are unsupported");
    Require(((config >> 20u) & 7u) == log2, "exposing fewer samples than are rasterized is unsupported");
    return 1u << log2;
}

void decodeSampleLocations(const Registers& cx, State& state) {
    const auto samples = state.samples;
    if (samples == 1) return;
    const std::span<const std::array<std::int8_t, 2>> standard = samples == 2 ? std::span<const std::array<std::int8_t, 2>>(StandardLocations2) : samples == 4 ? std::span<const std::array<std::int8_t, 2>>(StandardLocations4) : std::span<const std::array<std::int8_t, 2>>(StandardLocations8);
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) {
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            const auto word = read(cx, 0x2fe + pixel * 4u + sample / 4u);
            const auto bits = (word >> (8u * (sample % 4u))) & 0xffu;
            const auto x = static_cast<std::int8_t>(static_cast<std::uint8_t>(bits << 4u)) >> 4;
            const auto y = static_cast<std::int8_t>(static_cast<std::uint8_t>(bits & 0xf0u)) >> 4;
            state.sampleLocations[pixel * 8u + sample] = static_cast<std::uint8_t>(bits);
            state.customSampleLocations |= x != standard[sample][0] || y != standard[sample][1];
            state.sampleLocationGrid |= bits != state.sampleLocations[sample];
        }
    }
}

bool rasterizesSampleZeroOnly(const Registers& cx, const State& state) {
    if (state.samples <= 1 || state.alphaToCoverage || (read(cx, 0x292) & 1u) == 0) return false;
    const auto inputs = read(cx, 0x1b3) & read(cx, 0x1b4);
    const auto refused = state.perSampleShading ? ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage) : SampleZeroRefusedInputs;
    return (inputs & refused) == 0;
}

void rasterizeSampleZero(State& state) {
    std::array<std::uint8_t, 32> sampleZero{};
    for (std::uint32_t pixel = 0; pixel < 4; ++pixel) sampleZero[pixel * 8u] = state.sampleLocations[pixel * 8u];
    state.samples = 1;
    state.perSampleShading = false;
    state.sampleLocations = sampleZero;
    state.customSampleLocations = std::any_of(sampleZero.begin(), sampleZero.end(), [](std::uint8_t bits) { return bits != 0; });
    state.sampleLocationGrid = sampleZero[8] != sampleZero[0] || sampleZero[16] != sampleZero[0] || sampleZero[24] != sampleZero[0];
}

bool documentedRop(std::uint32_t rop) {
    return (rop >> 4u) == (rop & 0xfu) || ((rop >> 2u) & 0x33u) == (rop & 0x33u);
}

bool colorControlSupported(std::uint32_t colorControl, bool hasColorTarget) {
    colorControl &= ~1u;
    return ((colorControl & ~0xff0000u) == 0x10u && documentedRop((colorControl >> 16u) & 0xffu)) || ((colorControl >> 4u) & 7u) == 0u || (!hasColorTarget && (colorControl & ~0x70u) == 0xcc0000u);
}

std::string colorControlMessage(std::uint32_t colorControl) {
    static constexpr const char* modes[8] = {"disable", "normal", "eliminate fast clear", "resolve", "decompress", "FMASK decompress", "DCC decompress", "reserved"};
    char text[160];
    std::snprintf(text, sizeof(text), "AGC graphics: only normal color rendering with a ROP3 code of the source or brush form is supported (CB_COLOR_CONTROL 0x%08x, mode %s)", colorControl, modes[(colorControl >> 4u) & 7u]);
    return text;
}

VkBlendFactor blendFactor(std::uint32_t value) {
    switch (value) {
        case 0: return VK_BLEND_FACTOR_ZERO;
        case 1: return VK_BLEND_FACTOR_ONE;
        case 2: return VK_BLEND_FACTOR_SRC_COLOR;
        case 3: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case 4: return VK_BLEND_FACTOR_SRC_ALPHA;
        case 5: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case 6: return VK_BLEND_FACTOR_DST_ALPHA;
        case 7: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case 8: return VK_BLEND_FACTOR_DST_COLOR;
        case 9: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case 10: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case 13: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case 14: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case 15: return VK_BLEND_FACTOR_SRC1_COLOR;
        case 16: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case 17: return VK_BLEND_FACTOR_SRC1_ALPHA;
        case 18: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        case 19: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case 20: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        default: throw std::runtime_error("AGC graphics: unsupported blend factor " + std::to_string(value));
    }
}

VkBlendFactor colorFactorWithOpaqueDestination(VkBlendFactor factor) {
    switch (factor) {
        case VK_BLEND_FACTOR_DST_ALPHA: return VK_BLEND_FACTOR_ONE;
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ZERO;
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return VK_BLEND_FACTOR_ZERO;
        default: return factor;
    }
}

VkBlendFactor alphaFactorWithOpaqueDestination(VkBlendFactor factor) {
    switch (factor) {
        case VK_BLEND_FACTOR_DST_ALPHA: case VK_BLEND_FACTOR_DST_COLOR: return VK_BLEND_FACTOR_ONE;
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ZERO;
        default: return factor;
    }
}

bool secondSource(VkBlendFactor factor) {
    return factor == VK_BLEND_FACTOR_SRC1_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR || factor == VK_BLEND_FACTOR_SRC1_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
}

VkBlendOp blendOp(std::uint32_t value) {
    switch (value) {
        case 0: return VK_BLEND_OP_ADD;
        case 1: return VK_BLEND_OP_SUBTRACT;
        case 2: return VK_BLEND_OP_MIN;
        case 3: return VK_BLEND_OP_MAX;
        case 4: return VK_BLEND_OP_REVERSE_SUBTRACT;
        default: throw std::runtime_error("AGC graphics: unsupported blend operation " + std::to_string(value));
    }
}

constexpr std::uint8_t TermComplement = 0x80u;
constexpr std::uint8_t TermUnsupported = 0xffu;

std::uint8_t exportTerm(std::uint32_t component) {
    return static_cast<std::uint8_t>(1u + component);
}

std::uint8_t destinationTerm(std::uint32_t component) {
    return static_cast<std::uint8_t>(5u + component);
}

std::uint8_t guestBlendTerm(VkBlendFactor factor, std::uint32_t component) {
    switch (factor) {
        case VK_BLEND_FACTOR_ZERO: return 0u;
        case VK_BLEND_FACTOR_ONE: return TermComplement;
        case VK_BLEND_FACTOR_SRC_COLOR: return exportTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return TermComplement | exportTerm(component);
        case VK_BLEND_FACTOR_SRC_ALPHA: return exportTerm(0u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return TermComplement | exportTerm(0u);
        case VK_BLEND_FACTOR_DST_COLOR: return destinationTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return TermComplement | destinationTerm(component);
        case VK_BLEND_FACTOR_DST_ALPHA: return destinationTerm(0u);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return TermComplement | destinationTerm(0u);
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return component == 0u ? TermComplement : TermUnsupported;
        default: return TermUnsupported;
    }
}

std::uint8_t routedTerm(std::uint32_t route, std::uint32_t component) {
    namespace Code = ShaderRecompiler::ReversedBlendCode;
    switch (route) {
        case Code::Own: return exportTerm(component);
        case Code::Component0: return exportTerm(0u);
        case Code::Zero: return 0u;
        default: return TermComplement;
    }
}

std::uint8_t hostBlendTerm(VkBlendFactor factor, std::uint32_t component, const std::array<std::uint32_t, 4>& color, const std::array<std::uint32_t, 4>& second) {
    switch (factor) {
        case VK_BLEND_FACTOR_ZERO: return 0u;
        case VK_BLEND_FACTOR_ONE: return TermComplement;
        case VK_BLEND_FACTOR_SRC_COLOR: return routedTerm(color[component], component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return TermComplement ^ routedTerm(color[component], component);
        case VK_BLEND_FACTOR_SRC_ALPHA: return routedTerm(color[3], 3u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return TermComplement ^ routedTerm(color[3], 3u);
        case VK_BLEND_FACTOR_DST_COLOR: return destinationTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return TermComplement | destinationTerm(component);
        case VK_BLEND_FACTOR_DST_ALPHA: return destinationTerm(3u);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return TermComplement | destinationTerm(3u);
        case VK_BLEND_FACTOR_SRC1_COLOR: return routedTerm(second[component], component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return TermComplement ^ routedTerm(second[component], component);
        case VK_BLEND_FACTOR_SRC1_ALPHA: return routedTerm(second[3], 3u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return TermComplement ^ routedTerm(second[3], 3u);
        default: return TermUnsupported;
    }
}

struct BlendRouting {
    std::array<VkBlendFactor, 4> factors;
    std::uint32_t code;
};

std::optional<BlendRouting> routeReversedBlend(const std::array<VkBlendFactor, 2>& color, const std::array<VkBlendFactor, 2>& alpha) {
    namespace Code = ShaderRecompiler::ReversedBlendCode;
    static constexpr std::array<VkBlendFactor, 14> colorFactors{
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA};
    static constexpr std::array<VkBlendFactor, 8> alphaFactors{
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA};
    std::array<std::array<std::uint8_t, 2>, 4> wanted{};
    std::uint32_t replaceable = 0;
    for (std::uint32_t component = 0; component < 4; ++component) {
        for (std::size_t position = 0; position < 2; ++position) {
            wanted[component][position] = guestBlendTerm(component == 0u ? alpha[position] : color[position], component);
            if (wanted[component][position] == TermUnsupported) return std::nullopt;
        }
        if (wanted[component][0] == TermComplement) replaceable |= 1u << component;
    }
    const auto search = [&](bool routed) -> std::optional<BlendRouting> {
        const auto usable = [&](VkBlendFactor factor) { return routed || !secondSource(factor); };
        const std::uint32_t routeCount = routed ? 4u : 1u;
        std::uint32_t ones = 0;
        do {
            std::array<std::uint32_t, 4> colorRoutes{};
            for (std::uint32_t component = 0; component < 4; ++component) colorRoutes[component] = ((ones >> component) & 1u) != 0 ? Code::One : Code::Own;
            const auto matches = [&](VkBlendFactor factor, std::size_t position, std::uint32_t component, const std::array<std::uint32_t, 4>& second) {
                const auto term = hostBlendTerm(factor, component, colorRoutes, second);
                if (position == 1 || colorRoutes[component] == Code::Own) return term == wanted[component][position];
                return term == exportTerm(component);
            };
            for (std::uint32_t secondAlpha = 0; secondAlpha < routeCount; ++secondAlpha) {
                const std::array<std::uint32_t, 4> second{Code::Own, Code::Own, Code::Own, secondAlpha};
                std::array<std::optional<VkBlendFactor>, 2> alphaFactor{};
                for (std::size_t position = 0; position < 2; ++position) {
                    const auto found = std::find_if(alphaFactors.begin(), alphaFactors.end(), [&](VkBlendFactor factor) { return usable(factor) && matches(factor, position, 3u, second); });
                    if (found != alphaFactors.end()) alphaFactor[position] = *found;
                }
                if (!alphaFactor[0] || !alphaFactor[1]) continue;
                std::array<std::array<std::array<std::uint8_t, 3>, colorFactors.size()>, 2> masks{};
                for (std::size_t position = 0; position < 2; ++position) {
                    for (std::size_t index = 0; index < colorFactors.size(); ++index) {
                        if (!usable(colorFactors[index])) continue;
                        for (std::uint32_t component = 0; component < 3; ++component) {
                            auto routes = second;
                            for (std::uint32_t route = 0; route < routeCount; ++route) {
                                routes[component] = route;
                                if (matches(colorFactors[index], position, component, routes)) masks[position][index][component] |= static_cast<std::uint8_t>(1u << route);
                            }
                        }
                    }
                }
                for (std::size_t source = 0; source < colorFactors.size(); ++source) {
                    for (std::size_t destination = 0; destination < colorFactors.size(); ++destination) {
                        std::uint32_t code = Code::Enable;
                        for (std::uint32_t component = 0; component < 4; ++component) {
                            const auto shared = component == 3u ? 1u << secondAlpha : static_cast<std::uint32_t>(masks[0][source][component] & masks[1][destination][component]);
                            if (shared == 0u) {
                                code = 0u;
                                break;
                            }
                            const auto route = static_cast<std::uint32_t>(std::countr_zero(shared));
                            code |= (colorRoutes[component] << (Code::ColorShift + Code::RouteBits * component)) | (route << (Code::SecondShift + Code::RouteBits * component));
                        }
                        if (code != 0u) return BlendRouting{{colorFactors[source], colorFactors[destination], *alphaFactor[0], *alphaFactor[1]}, routed ? code : 0u};
                    }
                }
            }
            ones = (ones - replaceable) & replaceable;
        } while (routed && ones != 0u);
        return std::nullopt;
    };
    if (auto native = search(false)) return native;
    return search(true);
}

struct DecodedColorFormat {
    VkFormat format;
    std::uint32_t elementBytes;
    std::uint8_t componentMapping = 0xe4u;
    ShaderRecompiler::ColorExportPacking packing = ShaderRecompiler::ColorExportPacking::None;
};

const char* PackedColorTargetName(ShaderRecompiler::ColorExportPacking packing) {
    if (packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4) return "4_4_4_4 unorm";
    return packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 ? "11_11_10 float" : "10_11_11 unorm";
}

// CB_COLOR_INFO FORMAT / NUMBER_TYPE / COMP_SWAP to a Vulkan attachment format. AMD formats list
// components from the least significant bits, as Vulkan's non-packed formats do.
DecodedColorFormat DecodeColorFormat(std::uint32_t format, std::uint32_t number, std::uint32_t swap) {
    constexpr std::uint32_t unorm = 0, snorm = 1, sint = 5, srgb = 6, floating = 7;
    constexpr std::uint32_t uint = 4;
    const auto fail = [&]() -> DecodedColorFormat {
        throw std::runtime_error("AGC graphics: unsupported color format " + std::to_string(format) + " number type " + std::to_string(number) + " component swap " + std::to_string(swap));
    };
    const bool alternate = swap == 1;
    const auto single = [&](VkFormat vkFormat, std::uint32_t bytes) { return DecodedColorFormat{vkFormat, bytes, static_cast<std::uint8_t>((0xe4u & ~3u) | swap)}; };
    if (swap > 1 && format != 1 && format != 2 && format != 4 && format != 7 && format != 10 && format != 12 && format != 16 && format != 19) return fail();
    switch (format) {
        case 1:
            if (number == unorm) return single(VK_FORMAT_R8_UNORM, 1);
            if (number == snorm) return single(VK_FORMAT_R8_SNORM, 1);
            if (number == uint) return single(VK_FORMAT_R8_UINT, 1);
            if (number == srgb && swap == 0) return single(VK_FORMAT_R8_SRGB, 1);
            return fail();
        case 2:
            if (number == unorm) return single(VK_FORMAT_R16_UNORM, 2);
            if (number == snorm) return single(VK_FORMAT_R16_SNORM, 2);
            if (number == floating) return single(VK_FORMAT_R16_SFLOAT, 2);
            if (number == uint) return single(VK_FORMAT_R16_UINT, 2);
            return fail();
        case 3: {
            if (swap > 1) return fail();
            const auto mapping = static_cast<std::uint8_t>(alternate ? 0xecu : 0xe4u);
            if (number == unorm) return {VK_FORMAT_R8G8_UNORM, 2, mapping};
            if (number == snorm) return {VK_FORMAT_R8G8_SNORM, 2, mapping};
            if (number == uint) return {VK_FORMAT_R8G8_UINT, 2, mapping};
            return fail();
        }
        case 4:
            if (number == floating) return single(VK_FORMAT_R32_SFLOAT, 4);
            if (number == uint) return single(VK_FORMAT_R32_UINT, 4);
            if (number == sint) return single(VK_FORMAT_R32_SINT, 4);
            return fail();
        case 5:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R16G16_SFLOAT, 4};
            if (number == unorm) return {VK_FORMAT_R16G16_UNORM, 4};
            if (number == snorm) return {VK_FORMAT_R16G16_SNORM, 4};
            if (number == uint) return {VK_FORMAT_R16G16_UINT, 4};
            return fail();
        case 6:
            // COLOR_10_11_11: red in the low 11 bits, the Vulkan B10G11R11 packing.
            if (swap != 0 || (number != floating && number != unorm)) return fail();
            if (number == unorm) return {VK_FORMAT_R32_UINT, 4, 0xe4u, ShaderRecompiler::ColorExportPacking::Unorm10_11_11};
            return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4};
        case 7:
            if ((swap != 0 && swap != 2) || number != floating) return fail();
            return {VK_FORMAT_R32_UINT, 4, static_cast<std::uint8_t>(swap == 2 ? 0xc6u : 0xe4u), ShaderRecompiler::ColorExportPacking::Float11_11_10};
        case 9:
            // COLOR_2_10_10_10 keeps red in the low bits, the Vulkan A2B10G10R10 packing.
            if (number != unorm) return fail();
            return {alternate ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4};
        case 10: {
            const auto reversed = static_cast<std::uint8_t>(swap == 2 ? 0x1bu : swap == 3 ? 0x93u : 0xe4u);
            if (number == unorm) return {alternate ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, 4, reversed};
            if (number == snorm) return {alternate ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM, 4, reversed};
            if (number == srgb) return {alternate ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, 4, reversed};
            if (number == uint && !alternate) return {VK_FORMAT_R8G8B8A8_UINT, 4, reversed};
            return fail();
        }
        case 11:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32_SFLOAT, 8};
            if (number == uint) return {VK_FORMAT_R32G32_UINT, 8};
            return fail();
        case 12: {
            const auto reversed = static_cast<std::uint8_t>(alternate ? 0xc6u : swap == 2 ? 0x1bu : swap == 3 ? 0x93u : 0xe4u);
            if (number == floating) return {VK_FORMAT_R16G16B16A16_SFLOAT, 8, reversed};
            if (number == unorm) return {VK_FORMAT_R16G16B16A16_UNORM, 8, reversed};
            if (number == snorm) return {VK_FORMAT_R16G16B16A16_SNORM, 8, reversed};
            if (number == uint) return {VK_FORMAT_R16G16B16A16_UINT, 8, reversed};
            return fail();
        }
        case 14:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32B32A32_SFLOAT, 16};
            if (number == uint) return {VK_FORMAT_R32G32B32A32_UINT, 16};
            return fail();
        case 16:
            if ((swap != 0 && swap != 2) || number != unorm) return fail();
            return {VK_FORMAT_R5G6B5_UNORM_PACK16, 2, static_cast<std::uint8_t>(swap == 2 ? 0xe4u : 0xc6u)};
        case 17:
            if (swap != 0 || number != unorm) return fail();
            return {VK_FORMAT_A1R5G5B5_UNORM_PACK16, 2, 0xc6u};
        case 19:
            if (swap != 2 || number != unorm) return fail();
            return {VK_FORMAT_R16_UINT, 2, 0x1bu, ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4};
        default:
            return fail();
    }
}

void intersect(VkRect2D& result, const Registers& registers, std::uint32_t offset, bool screen) {
    const auto tl = read(registers, offset);
    const auto br = read(registers, offset + 1);
    if (!screen) Require((tl & 0x8000u) == 0 && (br & 0x80008000u) == 0, "scissor reserved bits are unsupported");
    const auto x = tl & 0xffffu;
    const auto y = (tl >> 16u) & (screen ? 0xffffu : 0x7fffu);
    const auto right = br & 0xffffu;
    const auto bottom = br >> 16u;
    const auto oldRight = static_cast<std::uint32_t>(result.offset.x) + result.extent.width;
    const auto oldBottom = static_cast<std::uint32_t>(result.offset.y) + result.extent.height;
    const auto left = std::max(static_cast<std::uint32_t>(result.offset.x), x);
    const auto top = std::max(static_cast<std::uint32_t>(result.offset.y), y);
    result.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    result.extent = {std::min(oldRight, right) > left ? std::min(oldRight, right) - left : 0, std::min(oldBottom, bottom) > top ? std::min(oldBottom, bottom) - top : 0};
}

}

VkExtent2D SampleLocationGrid(const State& state) {
    return state.sampleLocationGrid ? VkExtent2D{2, 2} : VkExtent2D{1, 1};
}

std::optional<SampleOrder> SameSetSampleOrder(const State& state) {
    if (!state.customSampleLocations || !state.sampleLocationGrid || state.samples < 2u || state.samples > 8u) return std::nullopt;
    SampleOrder order{};
    for (std::uint32_t pixel = 0; pixel < order.size(); ++pixel) {
        std::uint32_t word = KeptSampleOrder;
        std::uint32_t taken = 0;
        for (std::uint32_t sample = 0; sample < state.samples; ++sample) {
            std::uint32_t guest = 0;
            while (guest < state.samples && (((taken >> guest) & 1u) != 0u || state.sampleLocations[pixel * 8u + guest] != state.sampleLocations[sample])) ++guest;
            if (guest == state.samples) return std::nullopt;
            taken |= 1u << guest;
            word = (word & ~(0xfu << (sample * 4u))) | (guest << (sample * 4u));
        }
        order[pixel] = word;
    }
    return order;
}

bool ReordersSamples(const SampleOrder& order) {
    return std::ranges::any_of(order, [](std::uint32_t word) { return word != 0u && word != KeptSampleOrder; });
}

std::uint32_t InvertSampleOrder(std::uint32_t order) {
    std::uint32_t inverse = 0;
    for (std::uint32_t sample = 0; sample < 8u; ++sample) inverse |= sample << (((order >> (sample * 4u)) & 0x7u) * 4u);
    return inverse;
}

std::uint32_t HostSampleBits(std::uint32_t guestBits, std::uint32_t order) {
    std::uint32_t host = 0;
    for (std::uint32_t sample = 0; sample < 8u; ++sample) host |= ((guestBits >> ((order >> (sample * 4u)) & 0x7u)) & 1u) << sample;
    return host | (guestBits & ~0xffu);
}

bool SampleMaskFollowsOrder(const State& state, const SampleOrder& order) {
    if (!ReordersSamples(order) || !PartialSampleMask(state) || !UniformSampleMask(state)) return false;
    const auto full = (1u << state.samples) - 1u;
    const auto mask = state.quadSampleMask[0] & full;
    return std::ranges::any_of(order, [&](std::uint32_t word) { return (HostSampleBits(mask, word) & full) != mask; });
}

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
    const bool mergedCentres = pixel.targetMerge.samples > 1u && (pixel.inputAddr & (PixelInputBit(PixelInput::PerspectiveCenter) | PixelInputBit(PixelInput::LinearCenter))) != 0u;
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
    validate(!tessellation || !geometry, "combined tessellation and geometry is unsupported by the reference path");
    const auto path = tessellation ? ShaderPath::Tessellation : geometry ? ShaderPath::Geometry : ShaderPath::Vertex;
    ShaderStages result{path, value, (value & 0x00400000u) != 0 ? 32u : 64u, (read(queue.context, 0x1b6) & 0x8000u) != 0 ? 32u : 64u, {}, {}};
    APS5_LOG_OUT_DEBUG("DecodeShaderStages value=0x%x primitive=%u path=%u vertexWave=%u", value, primitive, static_cast<unsigned>(result.path), result.vertexWaveSize);
    if (path == ShaderPath::Vertex) {
        const bool legacy = (value & 0x20ffu) == 0;
        validate(legacy || (value & ~0x0247a010u) == 0 || value == 0x02002008u, "unsupported vertex routing, scheduling or wave-ID state");
    } else if (path == ShaderPath::Tessellation) {
        validate((value & 0x00600020u) == 0, "wave32 tessellation or geometry amplification is unsupported");
        validate((value & ~0x0207ed0du) == 0 && (value & 0x02002000u) == 0x02002000u && (value & 3u) == 1u && ((value >> 3u) & 3u) == 1u, "unsupported tessellation routing");
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
    } else {
        const bool fastLaunch = ((value >> 19u) & 3u) == 1u;
        validate((value & ~(fastLaunch ? 0x004fec30u : 0x0047ec30u)) == 0, "unsupported geometry routing, fast launch or wave-ID state");
        const auto group = read(queue.userConfig, 0x25b, RegisterBank::UserConfig);
        const auto vertices = (group >> 9u) & 0x1ffu;
        const auto primitives = group & 0x1ffu;
        validate(!fastLaunch || (!vertexWorkgroup && primitive == 1 && vertices == 1 && primitives == 1), "fast launch other than one point per subgroup is unsupported");
        const auto maxVertices = read(queue.context, 0x1ff);
        const auto verticesPerPrimitive = vertexWorkgroup ? 3u : read(queue.context, 0x2ce);
        const auto outputType = read(queue.context, 0x29b);
        validate(outputType == 2 || ((outputType == 0 || outputType == 1) && !vertexWorkgroup), "VGT_GS_OUT_PRIM_TYPE other than a triangle strip, or a point list or line strip from a geometry program, is unsupported");
        const auto outputSize = outputType == 0 ? 1u : outputType == 1 ? 2u : 3u;
        validate((primitive == 1 || primitive == 2 || primitive == 4 || primitive == 5 || primitive == 6) && verticesPerPrimitive >= outputSize, "unsupported geometry input or output assembly");
        const auto inputSize = primitive == 1 ? 1u : primitive == 2 ? 2u : 3u;
        validate(vertices >= inputSize && maxVertices != 0 && (!vertexWorkgroup || maxVertices >= inputSize) && maxVertices <= 256 && verticesPerPrimitive <= 256, "invalid geometry subgroup output");
        const auto inputStep = primitive == 5 || primitive == 6 ? 1u : inputSize;
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
        result.mesh = ShaderRecompiler::MeshConfiguration{primitive, groupPrimitives, (groupPrimitives - 1u) * inputStep + inputSize, maxVertices, maxPrimitives, ((threads + result.vertexWaveSize - 1u) / result.vertexWaveSize) * result.vertexWaveSize, ((resources >> 19u) & 0xffu) * 128u, provokingVertex, esgsItemSize, outputSize};
    }
    return result;
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
    if (control == queue.context.end() || (control->second & ~0x2fe3u) == 0) return {};
    const auto depthControl = find(queue.context, 0x200);
    const bool colorMasks = find(queue.context, 0x8e) != queue.context.end() && find(queue.context, 0x8f) != queue.context.end();
    if ((control->second & ~0x2fe3u) == 0x10u && depthControl != queue.context.end() && (depthControl->second & 3u) == 0 && colorMasks && ColorWriteMask(queue.context) == 0) return {};
    return zeroMessage(0x000, control->second, "DB_RENDER_CONTROL depth copy, resummarize or decompress");
}

ReversedBlend TranslateReversedBlend(std::uint32_t blendControl, std::uint8_t componentMapping, VkFormat format, bool forceDestinationAlphaOne) {
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
    if (any(constant)) refuse("with a constant blend factor is not implemented");
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
    const auto routing = routeReversedBlend(color, alpha);
    if (!routing) refuse("with factors the host blend cannot form from copies of the export is not implemented");
    blend.srcColorBlendFactor = routing->factors[0];
    blend.dstColorBlendFactor = routing->factors[1];
    blend.srcAlphaBlendFactor = routing->factors[2];
    blend.dstAlphaBlendFactor = routing->factors[3];
    return {blend, routing->code};
}

void ApplyDepthRemap(const Context& context, State& state) {
    const auto& viewport = state.viewport;
    const bool inRange = viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1;
    if (context.depthRangeUnrestricted || inRange) return;
    Require(state.stages.path == ShaderPath::Vertex && !state.stages.mesh && !state.stages.tessellation, "viewport depth outside [0, 1] is remapped on the vertex path only");
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

State DecodeState(const QueueState& queue, bool vertexWorkgroup) {
    if (auto reason = DepthMaintenanceRejection(queue); !reason.empty()) throw std::runtime_error(reason);
    const auto& cx = queue.context;
    State result{};
    result.conservativeRasterization = decodeConservativeRasterization(queue);
    result.stages = DecodeShaderStages(queue, vertexWorkgroup);
    const auto primitive = read(queue.userConfig, 0x242, RegisterBank::UserConfig);
    APS5_LOG_OUT_DEBUG("DecodeState primitive=%u path=%u vertexWave=%u", primitive, static_cast<unsigned>(result.stages.path), result.stages.vertexWaveSize);
    switch (primitive) {
        case 1:
            Require(result.stages.mesh.has_value() || result.stages.path == ShaderPath::Vertex, "point lists are only drawn through the vertex and geometry paths");
            result.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            result.vertexPoints = result.stages.path == ShaderPath::Vertex;
            break;
        case 2: result.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case 3: result.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
        case 7:
        case 17:
            Require(result.stages.path == ShaderPath::Vertex, "rect-list requires vertex routing");
            result.rectList = true;
            result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            break;
        case 9: result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
        case 4: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case 5:
        case 21:
            result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            break;
        case 6: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        case 19:
            Require(result.stages.path == ShaderPath::Vertex, "quad-list requires vertex routing");
            result.quadList = true;
            result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        default: throw std::runtime_error("AGC graphics: unsupported primitive type " + std::to_string(primitive));
    }
    APS5_LOG_OUT_DEBUG("Topology=%u", static_cast<unsigned>(result.topology));
    result.primitiveRestart = read(queue.userConfig, 0x24b, RegisterBank::UserConfig) != 0 && !result.rectList && !result.quadList && result.topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    zero(cx, 0x207, ~(LayerExports | ClipCullExports | ((result.vertexPoints || (result.stages.mesh.has_value() && result.stages.mesh->outputPrimitiveSize == 1u)) ? 1u << 16u : inactiveVertexPointSize(queue))), "layer, viewport or auxiliary vertex exports");
    Require(std::popcount(read(cx, 0x207) & 0xffffu) <= 8, "more than eight clip and cull distances are unsupported");
    result.paClVsOutCntl = read(cx, 0x207);
    if (result.vertexPoints || (result.stages.mesh.has_value() && result.stages.mesh->outputPrimitiveSize == 1u)) decodePointSize(queue, result);
    result.samples = decodeSamples(cx);
    Require(coveredCentroidSupported(queue), "COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs is not implemented");
    decodeSampleLocations(cx, result);
    {
        const auto depthControl = read(cx, 0x200);
        const auto renderControl = find(cx, 0x000);
        const bool clear = renderControl != cx.end() && (renderControl->second & 3u) != 0;
        const auto tests = planeTests(cx, depthControl);
        if ((clear || tests != 0) && depthSurfaceBound(cx)) {
            decodeDepth(cx, depthControl, (read(cx, 0x203) & 2u) != 0 && !PixelProgramSkipped(queue), (read(cx, 0x203) & 4u) != 0 && !PixelProgramSkipped(queue), result);
        } else if (depthPassThrough(depthControl) || ((depthControl & 0xbu) != 0 && (tests == 0 || depthPlanesAbsent(cx)))) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[gpu] always-pass depth/stencil state is rendered without a depth target (DB_DEPTH_CONTROL=0x%08x)\n", depthControl);
            }
        } else if (IgnoreDepthTest()) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[gpu] depth/stencil tests are ignored (APS5_IGNORE_DEPTH_TEST; DB_DEPTH_CONTROL=0x%08x)\n", depthControl);
            }
        } else {
            if ((effectiveDepthControl(depthControl) & DepthControlMask) != 0) zero(cx, 0x200, DepthControlMask, "depth, stencil or conditional color writes");
        }
        Require((depthControl & 8u) == 0 || result.depth.has_value() || depthPlanesAbsent(cx), "depth bounds without a depth surface");
        Require((depthControl & 0xc0000000u) == 0, "depth-conditional color writes are unsupported");
    }
    zero(cx, 0x203, shaderControlMask(read(cx, 0x1c4)), "depth export, shader coverage or ordered fragment execution");
    const auto alpha = read(cx, 0x2dc);
    const bool alphaDisabled = alpha == AlphaToCoverageDithered && alphaToCoverageDisabled(cx);
    result.alphaToCoverage = alpha == AlphaToCoverageUndithered || (!alphaDisabled && alpha == AlphaToCoverageDithered);
    result.ditheredAlphaToCoverage = result.alphaToCoverage && alpha == AlphaToCoverageDithered;
    if (!result.alphaToCoverage) zero(cx, 0x2dc, alphaDisabled ? AlphaToCoverageMask & ~1u : AlphaToCoverageMask, "dithered or rounded alpha-to-coverage");
    if (result.ditheredAlphaToCoverage) {
        Require(ditheredAlphaSamplesSupported(cx), "dithered alpha-to-coverage requires eight matched raster/DB_EQAA samples and one pixel iteration");
        Require(!result.customSampleLocations, "dithered alpha-to-coverage with custom sample locations is unsupported");
    }
    const bool depthKeepsSampleZero = result.depth && result.depth->samples == 1 && result.samples > 1;
    zero(cx, 0x292, ScanModeMask & ~1u, "scan conversion mode");
    if (depthKeepsSampleZero && (read(cx, 0x292) & 1u) == 0) {
        const bool filled = result.topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST && result.topology != VK_PRIMITIVE_TOPOLOGY_LINE_LIST && result.topology != VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        const auto sampleIndex = (read(cx, 0x293) & PixelShaderIteratesSamples) != 0u ? ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary) : 0u;
        const auto sampleInputs = read(cx, 0x1b3) & read(cx, 0x1b4) & (sampleIndex | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage));
        Require(filled && !result.alphaToCoverage && (PixelProgramSkipped(queue) || sampleInputs == 0) && (read(cx, 0x203) & 0x100u) == 0 && (read(cx, 0x201) & 0x0f080000u) == 0, "a one-sample depth target under a multisampled rasterizer without MSAA_ENABLE is unsupported for points or lines and with alpha-to-coverage, a sample index input under PS_ITER_SAMPLE, a coverage input, a sample-mask export, or DB_EQAA source Z interpolation or overrasterization");
        result.samples = 1;
        result.customSampleLocations = false;
        result.sampleLocations.fill(0);
        result.sampleLocationGrid = false;
    }
    if (result.samples > 1 && (read(cx, 0x292) & 1u) == 0) {
        Require(RasterPrimitiveOf(result) != ShaderRecompiler::RasterPrimitive::Point && !result.alphaToCoverage, "multisampled targets rasterized without MSAA_ENABLE are unsupported for points or alpha-to-coverage");
        if (const auto eqaa = find(cx, 0x201); result.depth && eqaa != cx.end() && (eqaa->second & 0x0f0c0000u) != 0) throw std::runtime_error(zeroMessage(0x201, eqaa->second, "a multisampled depth target rasterized without MSAA_ENABLE with Z interpolation or overrasterization (DB_EQAA)"));
        if (const auto control = find(cx, 0x203); result.depth && control != cx.end() && (control->second & 0x100u) != 0) throw std::runtime_error(zeroMessage(0x203, control->second, "a multisampled depth target rasterized without MSAA_ENABLE with a sample-mask export"));
        result.customSampleLocations = true;
        result.sampleLocations.fill(0);
        result.sampleLocationGrid = false;
    }
    if (result.samples == 1 && (read(cx, 0x292) & 1u) != 0) {
        for (std::uint32_t pixel = 0; pixel < 4; ++pixel) static_cast<void>(read(cx, 0x2feu + pixel * 4u));
        if (auto reason = singleSampleMsaaRejection(queue); !reason.empty()) throw std::runtime_error(reason);
    }
    zero(cx, 0x293, ScanControlMask, "primitive discard or out-of-order rasterization");
    result.perSampleShading = result.samples > 1 && (read(cx, 0x293) & PixelShaderIteratesSamples) != 0;
    Require(sampleCoverageSupported(queue, result.samples, result.perSampleShading), "SAMPLE_COVERAGE in a multisampled pixel shader that runs per sample, or reads sample-rate or ancillary inputs, is unsupported (the input holds the whole pixel's coverage, gl_SampleMaskIn only the invocation's samples)");
    if (depthKeepsSampleZero && result.samples > 1) {
        Require(rasterizesSampleZeroOnly(cx, result), "the depth target's sample count differs from the rasterizer's");
        rasterizeSampleZero(result);
    }
    if (result.depth && result.depth->samples > 1 && result.samples == 1 && (read(cx, 0x292) & 1u) == 0) {
        Require(!result.alphaToCoverage, "a single-sample scan into a multisampled depth target with alpha-to-coverage is unsupported");
        if (auto reason = singleSampleDepthScanRejection(queue); !reason.empty()) throw std::runtime_error(reason);
        result.samples = result.depth->samples;
        result.customSampleLocations = true;
        result.sampleLocations.fill(0);
    }
    Require(!result.depth || depthKeepsSampleZero || result.depth->samples == result.samples, "the depth target's sample count differs from the rasterizer's");
    zero(cx, 0x80, ~0u, "window offset");
    zero(cx, 0x8d, ScreenOffsetMask, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits");
    Require(read(cx, 0x83) == 0xffffu, "clip rectangles are unsupported");
    Require((read(cx, 0x8c) & 0xfu) == 0xau, "nonstandard triangle edge rules are unsupported");
    Require(read(cx, 0x2f9) == 0x2du, "nonstandard pixel center or vertex quantization is unsupported");
    if (auto reason = sampleMaskRejection(queue, read(cx, 0x2f8), read(cx, 0x30e), read(cx, 0x30f)); !reason.empty()) throw std::runtime_error(reason);
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu && viewportControl != 0u) throw std::runtime_error(vteMessage(viewportControl));
    zero(cx, 0x204, ClipControlMask, "a PA_CL_CLIP_CNTL flag");
    result.depthClamp = (read(cx, 0x204) & 0x0c000000u) != 0;
    result.negativeOneToOne = (read(cx, 0x204) & 0x80000u) == 0;
    const auto raster = read(cx, 0x205);
    // Bits 5-10 give the front/back polygon type (2 = filled triangles), which POLY_MODE (bit 3) turns on
    // explicitly; KEEP_TOGETHER_ENABLE (bit 24) only affects primitive distribution across the chip.
    const bool pointsOrLines = result.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || (result.stages.mesh.has_value() && result.stages.mesh->outputPrimitiveSize <= 2u);
    const auto lineOffset = (raster & 0x1800u) != 0 && !pointsOrLines ? 0x2000u : 0u;
    const auto rasterMode = raster & ~0x7u & ~0x7f8u & ~(1u << 24u) & ~0x1800u & ~lineOffset & ~(1u << 19u) & ~(1u << 16u);
    if (rasterMode != 0) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "depth bias or nonstandard rasterization"));
    result.cullMode = ((raster & 1u) != 0 ? VK_CULL_MODE_FRONT_BIT : 0u) | ((raster & 2u) != 0 ? VK_CULL_MODE_BACK_BIT : 0u);
    if (result.rectList) result.cullMode = VK_CULL_MODE_NONE;
    result.polygonMode = decodePolygonMode(cx, raster, result, pointsOrLines);
    if ((raster & 0x1800u) != 0 && result.depth) decodeDepthBias(cx, raster, result);
    result.frontFace = (raster & 4u) != 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    result.provokingVertexMode = (raster & (1u << 19u)) != 0 ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    APS5_LOG_OUT_DEBUG("Raster=0x%x cullMode=0x%x frontFace=%u negativeOneToOne=%u", raster, static_cast<unsigned>(result.cullMode), static_cast<unsigned>(result.frontFace), result.negativeOneToOne ? 1u : 0u);
    const auto shaderMask = read(cx, 0x8f);
    // Channels of targets the pixel shader does not export are never written, so the target mask only
    // matters where the shader exports.
    const auto targetMask = ColorWriteMask(cx);
    APS5_LOG_OUT_DEBUG("CB_TARGET_MASK=0x%x CB_SHADER_MASK=0x%x", targetMask, shaderMask);
    const auto exportFormatEntry = find(cx, 0x1c5);
    const auto exportFormat = exportFormatEntry == cx.end() ? 0u : exportFormatEntry->second;
    std::vector<std::uint32_t> exportSlots;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        if (((shaderMask >> (4u * slot)) & 0xfu) != 0) exportSlots.push_back(slot);
    }
    const auto written = [&](std::uint32_t slot) { return ((targetMask >> (4u * slot)) & 0xfu) != 0; };
    std::uint32_t exportCount = 0;
    for (std::uint32_t index = 0; index < exportSlots.size(); ++index) {
        if (!written(exportSlots[index])) continue;
        if (((exportFormat >> (4u * index)) & 0xfu) == 0) continue;
        exportCount = index + 1;
    }
    if (((read(cx, 0x202) >> 4u) & 7u) == 0u) exportCount = 0;
    result.hasColorTarget = exportCount != 0;
    APS5_LOG_OUT_DEBUG("hasColorTarget=%u exports=%u", result.hasColorTarget ? 1u : 0u, exportCount);

    // CB_COLOR_CONTROL mode 0 disables color writes, which only matters when a target is written.
    if (const auto colorControl = read(cx, 0x202); !colorControlSupported(colorControl, result.hasColorTarget)) throw std::runtime_error(colorControlMessage(colorControl));
    if (const auto colorControl = read(cx, 0x202); ((colorControl >> 4u) & 7u) == 1u) result.colorRop = static_cast<std::uint8_t>(colorControl >> 16u);
    zero(cx, 0x1c4, zFormatSupported(read(cx, 0x1c4)) ? 0u : ~0u, "depth or sample-mask export");
    APS5_LOG_OUT_DEBUG("Export format=%u", exportFormat);
    // SPI_SHADER_POS_FORMAT: POS0 must be a 4-component position; later vectors carry the misc/clip
    // exports that PA_CL_VS_OUT_CNTL validation above already limits to ignored layer/viewport data.
    const auto positionFormat = read(cx, 0x1c3) & 0xfu;
    if (positionFormat == 0u) {
        result.rasterizerDiscard = true;
    } else {
        Require(positionFormat == 4u, "position export format " + std::to_string(positionFormat) + " is unsupported");
    }
    for (std::uint32_t index = 0; index < exportCount; ++index) {
        const auto slot = exportSlots[index];
        if (!written(slot)) continue;
        // Export formats only matter for the targets the draw writes.
        const auto slotExport = (exportFormat >> (4u * index)) & 0xfu;
        if (slotExport == 0) continue;
        if (slotExport == 8 || slotExport > 9) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " is unsupported");
        auto color = DecodeColorBuffer(cx, slot);
        if (slotExport == 7 && ((read(cx, 0x31c + slot * 0xfu) >> 8u) & 7u) != 4u) throw std::runtime_error("AGC graphics: color export format 7 (UINT16_ABGR) into a target that is not unsigned integer is unsupported");
        color.exportIndex = index;
        color.uintExport = slotExport == 7;
        if (color.samples == 1 && result.samples > 1 && result.colors.empty() && !result.depth && (read(cx, 0x292) & 1u) == 0) {
            const auto sampleInputs = read(cx, 0x1b3) & read(cx, 0x1b4) & (ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage));
            Require(sampleInputs == 0 && (read(cx, 0x203) & 0x100u) == 0, "a one-sample color target under a multisampled rasterizer without MSAA_ENABLE is unsupported with a sample index or coverage input, or a sample-mask export");
            result.samples = 1;
            result.customSampleLocations = false;
            result.perSampleShading = false;
        }
        if (color.samples > 1 && result.samples == 1 && result.colors.empty() && !result.depth && !result.alphaToCoverage && (read(cx, 0x292) & 1u) == 0) {
            Require(result.conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT || (read(cx, 0x1b3) & 0x44u) == 0, "conservative rasterization with centroid interpolation of a draw raised to a multisampled target is unsupported");
            result.samples = color.samples;
            result.customSampleLocations = true;
            result.sampleLocations.fill(0);
        }
        const bool merged = color.samples == 1u && color.coverageSamples == 1u && !result.depth && !result.perSampleShading && (result.colors.empty() || result.mergedCoverage) && overrasterized(cx, result.samples);
        if (!merged && color.samples == 1 && color.coverageSamples == 1u && result.colors.empty() && !result.depth && rasterizesSampleZeroOnly(cx, result)) rasterizeSampleZero(result);
        Require(color.coverageSamples == color.samples || color.coverageSamples == result.samples || (result.samples == 1u && (read(cx, 0x292) & 1u) == 0), "a one-fragment color target's sample count differs from the rasterizer's");
        const bool truncated = color.samples == 1u && result.samples > 1u && result.depth && result.depth->samples == result.samples && (result.colors.empty() || result.truncatedCoverage) && !overrasterized(cx, result.samples);
        Require((color.samples == result.samples && !result.mergedCoverage && !result.truncatedCoverage) || merged || truncated, "a color target's sample count differs from the rasterizer's");
        Require(color.coverageSamples == color.samples || result.samples == 1u || staticAnchors(cx, result.depth->samples), "a one-fragment color target is supported only with static DB_EQAA anchors at the depth target's sample count");
        result.mergedCoverage = merged;
        result.truncatedCoverage = truncated;
        APS5_LOG_OUT_DEBUG("Color %u address=0x%llx extent=%ux%u bytes=%llu VkFormat=%u", slot, static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height, static_cast<unsigned long long>(color.bytes), static_cast<unsigned>(color.format));
        if (result.colors.empty()) {
            result.renderExtent = color.extent;
        } else {
            result.renderExtent = {std::min(result.renderExtent.width, color.extent.width), std::min(result.renderExtent.height, color.extent.height)};
        }
        result.colors.push_back(color);
    }
    if (result.hasColorTarget) {
        result.color = result.colors.front();
        result.renderLayers = result.color.layers;
        for (const auto& color : result.colors) Require(color.layers == result.renderLayers, "color targets with different layer counts are unsupported");
        if (result.renderLayers > 1) {
            Require(!result.depth, "layered rendering with a depth target is unsupported");
            result.layerExports = read(cx, 0x207) & (1u << 18u);
        }
        if (result.depth) result.renderExtent = {std::min(result.renderExtent.width, result.depth->extent.width), std::min(result.renderExtent.height, result.depth->extent.height)};
    } else if (result.depth) {
        result.renderExtent = result.depth->extent;
    } else {
        const auto screenBottomRight = read(cx, 0xd);
        APS5_LOG_OUT_DEBUG("No color target, screen BR register=0x%x", screenBottomRight);
        result.renderExtent = {std::max(screenBottomRight & 0xffffu, 1u), std::max(screenBottomRight >> 16u, 1u)};
        APS5_LOG_OUT_DEBUG("Render extent from screen=%ux%u", result.renderExtent.width, result.renderExtent.height);
    }
    result.quadSampleMask = decodeQuadSampleMask(cx, result.samples);
    result.pixelEarlyZ = !PixelProgramSkipped(queue) && PixelEarlyZ(read(cx, 0x203));
    result.pixelKill = !PixelProgramSkipped(queue) && (read(cx, 0x203) & 0x40u) != 0;
    if ((read(cx, 0x207) & LayerExports & ~result.layerExports & ~(1u << 21u)) != 0) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] layer/viewport index vertex exports are ignored (PA_CL_VS_OUT_CNTL=0x%08x)\n", read(cx, 0x207));
        }
    }
    bool collapsed = false;
    if (viewportControl == 0u) {
        result.viewport = VkViewport{0.0f, 0.0f, static_cast<float>(result.renderExtent.width), static_cast<float>(result.renderExtent.height), result.negativeOneToOne ? -1.0f : 0.0f, 1.0f};
    } else {
        const auto xs = readFloat(cx, 0x10f);
        const auto xo = readFloat(cx, 0x110);
        const auto ys = readFloat(cx, 0x111);
        const auto yo = readFloat(cx, 0x112);
        const auto zs = readFloat(cx, 0x113);
        const auto zo = readFloat(cx, 0x114);
        const auto minDepth = result.negativeOneToOne ? zo - zs : zo;
        const auto maxDepth = zo + zs;
        APS5_LOG_OUT_DEBUG("Viewport transform scale=(%f,%f,%f) offset=(%f,%f,%f) depth=(%f,%f)", xs, ys, zs, xo, yo, zo, minDepth, maxDepth);
        const bool areaFree = result.stages.path == ShaderPath::Vertex && (result.rectList || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        collapsed = (xs == 0 || ys == 0) && xs >= 0 && areaFree && std::isfinite(xo) && std::isfinite(yo) && std::isfinite(minDepth) && std::isfinite(maxDepth);
        if (!collapsed && !(xs > 0 && ys != 0 && std::isfinite(minDepth) && std::isfinite(maxDepth))) {
            std::ostringstream message;
            message << "AGC graphics: unsupported viewport transform: scale=(" << xs << ", " << ys << ", " << zs << "), offset=(" << xo << ", " << yo << ", " << zo << "), depth=(" << minDepth << ", " << maxDepth << "), negativeOneToOne=" << result.negativeOneToOne;
            throw std::runtime_error(message.str());
        }
        Require(readFloat(cx, 0xb4) <= readFloat(cx, 0xb5), "inverted viewport depth clamp bounds");
        result.depthScale = zs;
        result.depthOffset = zo;
        result.depthClampMin = readFloat(cx, 0xb4);
        result.depthClampMax = readFloat(cx, 0xb5);
        result.viewport = collapsed ? VkViewport{xo, yo, 1, 1, minDepth, maxDepth} : VkViewport{xo - xs, yo - ys, 2 * xs, 2 * ys, minDepth, maxDepth};
    }
    if (const auto renderControl = find(cx, 0x000); result.depth && renderControl != cx.end() && (renderControl->second & 1u) != 0) {
        result.viewport.minDepth = result.depth->clearDepth;
        result.viewport.maxDepth = result.depth->clearDepth;
        result.depthBias = false;
    }
    APS5_LOG_OUT_DEBUG("Viewport x=%f y=%f w=%f h=%f minDepth=%f maxDepth=%f", result.viewport.x, result.viewport.y, result.viewport.width, result.viewport.height, result.viewport.minDepth, result.viewport.maxDepth);
    result.scissor = {{0, 0}, result.renderExtent};
    intersect(result.scissor, cx, 0xc, true);
    intersect(result.scissor, cx, 0x81, false);
    intersect(result.scissor, cx, 0x90, false);
    if ((read(cx, 0x292) & 2u) != 0) intersect(result.scissor, cx, 0x94, false);
    if (collapsed) result.scissor.extent = {0, 0};
    APS5_LOG_OUT_DEBUG("Scissor offset=(%d,%d) extent=%ux%u", result.scissor.offset.x, result.scissor.offset.y, result.scissor.extent.width, result.scissor.extent.height);
    result.blends.assign(exportCount, VkPipelineColorBlendAttachmentState{});
    std::array<float, 3> requiredBlendConstants{};
    std::uint32_t requiredBlendMask = 0;
    for (auto& color : result.colors) {
        const auto slot = color.slot;
        const auto blend = read(cx, 0x1e0 + slot);
        APS5_LOG_OUT_DEBUG("Blend %u control=0x%x", slot, blend);
        Require((blend & 0x0000e000u) == 0, "reserved blend control bits");
        VkPipelineColorBlendAttachmentState state{};
        const auto mapping = color.componentMapping;
        const auto exportedMask = (targetMask >> (4u * slot)) & 0xfu;
        for (std::uint32_t component = 0; component < 4; ++component) {
            if (((exportedMask >> ((mapping >> (2u * component)) & 3u)) & 1u) != 0) state.colorWriteMask |= 1u << component;
        }
        state.blendEnable = ((blend >> 30u) & 1u) != 0 && (read(cx, 0x31c + slot * 0xfu) & 0x10000u) == 0;
        Require(!state.blendEnable || !color.uintExport, "blending into an unsigned integer target is unsupported");
        const bool reversed = mapping == 0x1bu || mapping == 0x93u;
        if (state.blendEnable) {
            const bool opaqueDestination = (read(cx, 0x31d + slot * 0xfu) & 0x20000u) != 0;
            if (reversed) {
                const auto translated = TranslateReversedBlend(blend, mapping, color.format, opaqueDestination);
                state.srcColorBlendFactor = translated.blend.srcColorBlendFactor;
                state.dstColorBlendFactor = translated.blend.dstColorBlendFactor;
                state.colorBlendOp = translated.blend.colorBlendOp;
                state.srcAlphaBlendFactor = translated.blend.srcAlphaBlendFactor;
                state.dstAlphaBlendFactor = translated.blend.dstAlphaBlendFactor;
                state.alphaBlendOp = translated.blend.alphaBlendOp;
                if (translated.shaderCode != 0u) result.reversedBlend = translated.shaderCode;
            } else {
                state.srcColorBlendFactor = blendFactor(blend & 0x1fu);
                state.dstColorBlendFactor = blendFactor((blend >> 8u) & 0x1fu);
                state.colorBlendOp = blendOp((blend >> 5u) & 7u);
                const auto alpha = (blend & 0x20000000u) != 0 ? blend >> 16u : blend;
                state.srcAlphaBlendFactor = blendFactor(alpha & 0x1fu);
                state.dstAlphaBlendFactor = blendFactor((alpha >> 8u) & 0x1fu);
                state.alphaBlendOp = blendOp((alpha >> 5u) & 7u);
                if ((mapping & 3u) == 3u) {
                    state.srcColorBlendFactor = state.srcAlphaBlendFactor;
                    state.dstColorBlendFactor = state.dstAlphaBlendFactor;
                    state.colorBlendOp = state.alphaBlendOp;
                }
                if (opaqueDestination) {
                    state.srcColorBlendFactor = colorFactorWithOpaqueDestination(state.srcColorBlendFactor);
                    state.dstColorBlendFactor = colorFactorWithOpaqueDestination(state.dstColorBlendFactor);
                    state.srcAlphaBlendFactor = alphaFactorWithOpaqueDestination(state.srcAlphaBlendFactor);
                    state.dstAlphaBlendFactor = alphaFactorWithOpaqueDestination(state.dstAlphaBlendFactor);
                }
            }
            for (std::uint32_t i = 0; i < 4; ++i) result.blendConstants[i] = readFloat(cx, 0x105 + i);
            const auto constantColor = [](VkBlendFactor factor) { return factor == VK_BLEND_FACTOR_CONSTANT_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR; };
            if ((mapping == 0xe4u || mapping == 0xc6u) && state.colorBlendOp != VK_BLEND_OP_MIN && state.colorBlendOp != VK_BLEND_OP_MAX && (constantColor(state.srcColorBlendFactor) || constantColor(state.dstColorBlendFactor))) {
                for (std::uint32_t i = 0; i < 3; ++i) {
                    if ((state.colorWriteMask & (1u << i)) == 0) continue;
                    const auto value = result.blendConstants[(mapping >> (2u * i)) & 3u];
                    Require((requiredBlendMask & (1u << i)) == 0 || requiredBlendConstants[i] == value, "color targets require conflicting blend constants after component swap");
                    requiredBlendConstants[i] = value;
                    requiredBlendMask |= 1u << i;
                }
            }
            if (mapping == 0xecu) {
                const auto destinationAlpha = [](VkBlendFactor factor) { return factor == VK_BLEND_FACTOR_DST_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA || factor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE; };
                if (destinationAlpha(state.srcColorBlendFactor) || destinationAlpha(state.dstColorBlendFactor) || destinationAlpha(state.srcAlphaBlendFactor) || destinationAlpha(state.dstAlphaBlendFactor)) throw std::runtime_error("AGC graphics: blending into an 8_8 color target with the alternate component order with a factor of the destination alpha, which the target keeps in its second channel, is not implemented");
                const auto weighted = [](VkBlendOp operation) { return operation != VK_BLEND_OP_MIN && operation != VK_BLEND_OP_MAX; };
                if ((weighted(state.colorBlendOp) && (constantColor(state.srcColorBlendFactor) || constantColor(state.dstColorBlendFactor))) || (weighted(state.alphaBlendOp) && (constantColor(state.srcAlphaBlendFactor) || constantColor(state.dstAlphaBlendFactor)))) throw std::runtime_error("AGC graphics: blending into an 8_8 color target with the alternate component order with a constant color factor, which Vulkan applies to the second channel with the green blend constant instead of the alpha one, is not implemented");
            }
        }
        if (color.packing != ShaderRecompiler::ColorExportPacking::None) {
            const std::string packed = std::string(color.packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 ? "an " : "a ") + PackedColorTargetName(color.packing);
            const auto slotExport = (exportFormat >> (4u * color.exportIndex)) & 0xfu;
            if (slotExport == 5 || slotExport == 6) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " into " + packed + " target is unsupported");
            Require(!state.blendEnable, "blending into " + packed + " color target is unsupported");
            const bool nibbles = color.packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4;
            const std::uint32_t packedMask = nibbles ? 0xfu : 7u;
            Require(nibbles || (exportedMask & packedMask) == packedMask, "partial writes of " + packed + " color target are unsupported");
            if ((exportedMask & packedMask) != packedMask) {
                Require(color.mipCount == 1u && color.depth == 1u && color.layers == 1u && color.arrayLastSlice == 0u, "partial writes of a mipmapped, volume or array " + std::string(PackedColorTargetName(color.packing)) + " color target are not implemented");
                Require(std::ranges::none_of(result.colors, [](const ColorTarget& other) { return other.partialWriteMask != 0; }), "partial writes of more than one packed color target are not implemented");
                color.partialWriteMask = PackedWriteMask(mapping, exportedMask);
                state.colorWriteMask = 0;
                if (color.samples > 1u) {
                    Require(sampleCoverageSupported(queue, result.samples, true), "SAMPLE_COVERAGE in a multisampled pixel shader that runs per sample, or reads sample-rate or ancillary inputs, is unsupported (the input holds the whole pixel's coverage, gl_SampleMaskIn only the invocation's samples)");
                    if (!result.perSampleShading && !pixelRateInputsKept(queue, result)) {
                        const auto value = [&](std::uint32_t offset) { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
                        char detail[160];
                        std::snprintf(detail, sizeof(detail), " (SPI_PS_INPUT_ENA 0x%x, SPI_PS_INPUT_ADDR 0x%x, SPI_BARYC_CNTL 0x%x, sample 1 location 0x%02x)", value(0x1b3), value(0x1b4), value(0x1b8), result.sampleLocations[1]);
                        Require(false, std::string("partial writes of a multisampled 4_4_4_4 unorm color target run the pixel shader once per sample, so a shader the guest runs once per pixel cannot read the ancillary word or, unless every sample sits at the pixel centre, centroid, sample-located or pull-model barycentrics, Z, 1/W or a position at the centroid or a sample: they would follow the invocation's sample") + detail);
                    }
                    result.perSampleShading = true;
                }
            }
        }
        if (state.blendEnable && !reversed && (secondSource(state.srcColorBlendFactor) || secondSource(state.dstColorBlendFactor) || secondSource(state.srcAlphaBlendFactor) || secondSource(state.dstAlphaBlendFactor))) {
            const auto addition = [](VkBlendOp op) { return op == VK_BLEND_OP_ADD || op == VK_BLEND_OP_SUBTRACT || op == VK_BLEND_OP_REVERSE_SUBTRACT; };
            Require(addition(state.colorBlendOp) && addition(state.alphaBlendOp), "dual-source blending with a MIN or MAX operation is unsupported");
            result.dualSourceBlend = true;
        }
        if (const auto info = read(cx, 0x31c + slot * 0xfu); state.blendEnable && (info & 0x8000u) == 0 && ((info >> 8u) & 7u) <= 1u) {
            char detail[80];
            std::snprintf(detail, sizeof(detail), " (CB_COLOR_INFO 0x%08x, CB_BLEND%u_CONTROL 0x%08x)", info, slot, blend);
            throw std::runtime_error(std::string("AGC graphics: blending unclamped normalized color is not implemented") + detail);
        }
        result.blends[color.exportIndex] = state;
    }
    for (std::uint32_t i = 0; i < 3; ++i) {
        if ((requiredBlendMask & (1u << i)) != 0) result.blendConstants[i] = requiredBlendConstants[i];
    }
    Require((!result.mergedCoverage && !result.truncatedCoverage) || std::ranges::none_of(result.colors, [](const ColorTarget& color) { return color.partialWriteMask != 0; }), "partial writes of a 4_4_4_4 unorm color target under merged or truncated coverage (a one-sample target under a multisampled rasterizer) are not implemented");
    if (!result.colors.empty()) {
        result.color = result.colors.front();
        result.blend = result.blends[result.colors.front().exportIndex];
    }
    if (LogicOpFromRop(result.colorRop) != VK_LOGIC_OP_COPY) {
        for (const auto& blend : result.blends) Require(blend.blendEnable == VK_FALSE, "a color logic operation together with blending is not implemented");
    }
    Require(result.reversedBlend == 0 || (exportSlots.size() == 1 && exportCount == 1), "blending into a color target with a reversed component order through a second source color needs that target to be the shader's only color export");
    if (result.dualSourceBlend) {
        Require(exportSlots.size() == 2 && exportSlots[0] == 0 && exportSlots[1] == 1 && exportCount == 1, "dual-source blending needs pixel exports to MRT slots 0 and 1 and color writes to slot 0 alone");
        Require(result.colors.front().componentMapping == 0xe4u, "dual-source blending into a component-swapped color target is unsupported");
        Require(((exportFormat >> 4u) & 0xfu) == (exportFormat & 0xfu), "dual-source blending needs the MRT1 export in the MRT0 export format");
        Require(((shaderMask >> 4u) & 0xfu) == (shaderMask & 0xfu), "dual-source blending needs the MRT1 export with the MRT0 shader mask");
    }
    if (result.alphaToCoverage) {
        const auto mrt0 = std::find_if(result.colors.begin(), result.colors.end(), [](const ColorTarget& color) { return color.exportIndex == 0; });
        const auto format = read(cx, 0x1c5) & 0xfu;
        Require(format == 3u || format == 4u || format == 5u || format == 6u || format == 9u, "alpha-to-coverage needs an MRT0 export format with a floating-point alpha");
        Require(!result.ditheredAlphaToCoverage || format == 4u || format == 9u, "dithered alpha-to-coverage needs an FP16_ABGR or 32_ABGR export");
        Require(mrt0 == result.colors.end() || (mrt0->componentMapping == 0xe4u && mrt0->packing == ShaderRecompiler::ColorExportPacking::None), "alpha-to-coverage into a component-swapped or packed MRT0 target is unsupported");
    }
    APS5_LOG_OUT_DEBUG("DecodeState done colorTarget=%u render=%ux%u topology=%u", result.hasColorTarget ? 1u : 0u, result.renderExtent.width, result.renderExtent.height, static_cast<unsigned>(result.topology));
    return result;
}

std::array<ShaderRecompiler::ColorExportPacking, 8> ExportPackings(const State& state) {
    std::array<ShaderRecompiler::ColorExportPacking, 8> packings{};
    for (const auto& color : state.colors) {
        if (color.exportIndex < packings.size()) packings[color.exportIndex] = color.packing;
    }
    return packings;
}

std::uint16_t PackedWriteMask(std::uint8_t componentMapping, std::uint32_t exportedMask) {
    std::uint16_t mask = 0;
    for (std::uint32_t component = 0; component < 4; ++component) {
        if (((exportedMask >> ((componentMapping >> (2u * component)) & 3u)) & 1u) != 0) mask = static_cast<std::uint16_t>(mask | (0xfu << (4u * component)));
    }
    return mask;
}

GuestTextureResource TargetMergeSurface(const ColorTarget& color) {
    auto surface = SurfaceForTarget(color);
    if (color.samples > 1u) {
        Require(surface.baseAddress == color.surfaceAddress, "a merged multisampled color target does not start at its surface");
        surface.dimension = TextureDimension::k2DMsaa;
        surface.samples = color.samples;
    }
    return surface;
}

bool DepthStencilWrites(const State& state) {
    if (!state.depth) return false;
    const auto writes = [](const VkStencilOpState& face) { return (face.writeMask & 0xffu) != 0 && (face.failOp != VK_STENCIL_OP_KEEP || face.passOp != VK_STENCIL_OP_KEEP || face.depthFailOp != VK_STENCIL_OP_KEEP); };
    return state.depthWrite || (state.stencilTest && (writes(state.stencilFront) || writes(state.stencilBack)));
}

bool DepthStencilTests(const State& state) {
    return state.depth.has_value() && (state.depthTest || state.stencilTest || state.depthBoundsTest);
}

ShaderRecompiler::TargetMerge ExportTargetMerge(const State& state) {
    ShaderRecompiler::TargetMerge merge;
    for (const auto& color : state.colors) {
        if (color.partialWriteMask == 0) continue;
        Require(merge.mask == 0 && color.exportIndex < 8u && color.samples <= 8u, "a draw merges into one packed color target at most");
        Require(color.samples == 1u || state.perSampleShading, "a merged multisampled color target needs per-sample shading");
        merge.target = static_cast<std::uint8_t>(color.exportIndex);
        merge.mask = color.partialWriteMask;
        merge.samples = static_cast<std::uint8_t>(color.samples);
        merge.descriptor = EncodeTextureResource(TargetMergeSurface(color));
        merge.depthStencilWrites = DepthStencilWrites(state);
        merge.depthStencilTests = DepthStencilTests(state);
    }
    return merge;
}

bool PartialSampleMask(const State& state) {
    const auto full = (1u << state.samples) - 1u;
    return std::ranges::any_of(state.quadSampleMask, [&](std::uint16_t mask) { return (mask & full) != full; });
}

bool UniformSampleMask(const State& state) {
    const auto full = (1u << state.samples) - 1u;
    return std::ranges::all_of(state.quadSampleMask, [&](std::uint16_t mask) { return (mask & full) == (state.quadSampleMask[0] & full); });
}

VkSampleMask HostSampleMask(const State& state) {
    const auto full = (1u << state.samples) - 1u;
    return UniformSampleMask(state) ? state.quadSampleMask[0] & full : full;
}

std::uint8_t QuadPixelMask(const State& state) {
    if (!PartialSampleMask(state) || UniformSampleMask(state)) return 0xfu;
    const auto full = (1u << state.samples) - 1u;
    std::uint8_t pixels = 0;
    for (std::uint32_t pixel = 0; pixel < state.quadSampleMask.size(); ++pixel) {
        if ((state.quadSampleMask[pixel] & full) != 0u) pixels = static_cast<std::uint8_t>(pixels | (1u << pixel));
    }
    return pixels;
}

std::array<std::uint16_t, 4> QuadSampleMasks(const State& state) {
    std::array<std::uint16_t, 4> masks{0xffffu, 0xffffu, 0xffffu, 0xffffu};
    if (!PartialSampleMask(state) || UniformSampleMask(state)) return masks;
    const auto full = (1u << state.samples) - 1u;
    if (std::ranges::all_of(state.quadSampleMask, [&](std::uint16_t mask) { return (mask & full) == 0u || (mask & full) == full; })) return masks;
    for (std::size_t pixel = 0; pixel < masks.size(); ++pixel) {
        const auto mask = state.quadSampleMask[pixel] & full;
        masks[pixel] = static_cast<std::uint16_t>(mask == full ? 0xffffu : mask);
    }
    return masks;
}

ShaderRecompiler::RasterPrimitive RasterPrimitiveOf(const State& state) {
    if (state.rectList) return ShaderRecompiler::RasterPrimitive::Rectangle;
    if (state.stages.mesh.has_value() && state.stages.mesh->outputPrimitiveSize == 1u) return ShaderRecompiler::RasterPrimitive::Point;
    if (state.stages.mesh.has_value() && state.stages.mesh->outputPrimitiveSize == 2u) return ShaderRecompiler::RasterPrimitive::Line;
    if (state.stages.mesh.has_value() || state.stages.tessellation.has_value()) return ShaderRecompiler::RasterPrimitive::Triangle;
    switch (state.topology) {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return ShaderRecompiler::RasterPrimitive::Point;
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return ShaderRecompiler::RasterPrimitive::Line;
        default: return ShaderRecompiler::RasterPrimitive::Triangle;
    }
}

std::array<std::uint8_t, 8> ExportMappings(const State& state) {
    std::array<std::uint8_t, 8> mappings{};
    mappings.fill(0xe4u);
    for (const auto& color : state.colors) {
        if (color.exportIndex < mappings.size()) mappings[color.exportIndex] = color.componentMapping;
    }
    return mappings;
}

std::size_t CmaskBytes(std::uint32_t width, std::uint32_t height) {
    return CmaskLayout(width, height).Bytes();
}

std::uint32_t ColorWriteMask(const Registers& context) {
    auto mask = read(context, 0x8e) & read(context, 0x8f);
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        const auto channels = 0xfu << (slot * 4u);
        if ((mask & channels) == 0) continue;
        const auto info = find(context, 0x31c + slot * 0xfu);
        if (info != context.end() && ((info->second >> 2u) & 0x1fu) == 0) mask &= ~channels;
    }
    return mask;
}

namespace {

std::uint32_t GuestFormatFor(VkFormat format, std::uint32_t elementBytes) {
    if (const auto guest = FindGuestColorTargetFormat(format, elementBytes)) return *guest;
    throw std::runtime_error("AGC graphics: no guest texture format matches the color buffer format " + std::to_string(static_cast<int>(format)));
}

}

GuestTextureResource SurfaceForTarget(const ColorTarget& color) {
    static const bool residentLinear = std::getenv("APS5_NO_RESIDENT_LINEAR") == nullptr;
    Require(residentLinear || color.tileMode != ColorTileMode::Linear, "linear color targets are not resident");
    const bool chain = color.mipCount > 1;
    GuestTextureResource surface{};
    surface.baseAddress = chain ? color.surfaceAddress : color.address;
    surface.pipeBankXor = color.pipeBankXor;
    surface.width = chain ? color.surfaceExtent.width : color.extent.width;
    surface.height = chain ? color.surfaceExtent.height : color.extent.height;
    surface.depthOrLastArray = color.depth - 1u;
    surface.baseArray = 0;
    surface.mipCount = color.mipCount;
    surface.baseLevel = 0;
    surface.lastLevel = color.mipCount - 1;
    surface.tileMode = ColorTextureTileMode(color.tileMode);
    surface.dimension = color.depth > 1 ? TextureDimension::k3D : TextureDimension::k2D;
    surface.format = GuestFormatFor(color.format, color.elementBytes);
    surface.dstSelX = 4;
    surface.dstSelY = 5;
    surface.dstSelZ = 6;
    surface.dstSelW = 7;
    surface.dccAddress = color.dccAddress;
    surface.dccAlphaOnMsb = color.dccAlphaOnMsb;
    surface.dccPipeAligned = color.dccPipeAligned;
    return surface;
}

ColorTarget DecodeColorBuffer(const Registers& cx, std::uint32_t slot, bool allowMultisampleDcc) {
    const auto stride = slot * 0xfu;
    ColorTarget color{};
    color.slot = slot;
    const auto info = read(cx, 0x31c + stride);
    const auto number = (info >> 8u) & 7u;
    const auto swap = (info >> 11u) & 3u;
    APS5_LOG_OUT_DEBUG("Color %u info=0x%x number=%u swap=%u", slot, info, number, swap);
    const auto format = (info >> 2u) & 0x1fu;
    const auto decoded = DecodeColorFormat(format, number, swap);
    const auto attrib = read(cx, 0x31d + stride);
    zero(cx, 0x31d + stride, ~0x3f000u, "color destination alpha override or other CB_COLOR_ATTRIB modes");
    color.coverageSamples = 1u << ((attrib >> 12u) & 7u);
    Require(color.coverageSamples <= 8u, "color targets with more than eight samples are unsupported");
    color.samples = 1u << ((attrib >> 15u) & 3u);
    Require(color.samples <= color.coverageSamples, "color targets with more fragments than samples are unsupported");
    Require(color.samples == color.coverageSamples || color.samples == 1u, "color targets with more than one but fewer fragments than samples (EQAA) are unsupported");
    const auto multisampleInfo = color.coverageSamples > 1 ? 0x0c006000u : 0u;
    // ROUND_MODE (bit 18) only affects unorm rounding and packed float truncation. With DCC_ENABLE (bit 28) the target is written
    if ((info & ~(0x00039f7cu | 0x00040000u | 0x10000000u | 0x00002000u | multisampleInfo)) != 0) throw std::runtime_error("AGC graphics: color compression, DCC, endian conversion, nonstandard rounding or color optimization is unsupported (CB_COLOR_INFO 0x" + [&] { char text[16]; std::snprintf(text, sizeof(text), "%08x", info); return std::string(text); }() + ")");
    Require((info & 0x8000u) != 0 || number <= 1u || number == 7 || number == 4 || number == 5, "unclamped normalized color is unsupported");
    const auto view = read(cx, 0x31b + stride);
    Require((view & ~0x3fffffffu) == 0, "reserved CB_COLOR_VIEW bits are set");
    const auto slice = view & 0x1fffu;
    const auto lastSlice = (view >> 13u) & 0x1fffu;
    Require(lastSlice >= slice, "the color view ends before its first slice");
    const auto viewMip = (view >> 26u) & 0xfu;
    const auto attrib2 = read(cx, 0x3b0 + slot);
    const auto maxMip = attrib2 >> 28u;
    Require(color.samples == 1 || maxMip == 0, "multisampled color mips are unsupported");
    Require(viewMip <= maxMip, "color view mip exceeds the surface");
    const auto attrib3 = read(cx, 0x3b8 + slot);
    color.tileMode = DecodeColorTileMode(attrib3);
    const bool volume = ((attrib3 >> 24u) & 3u) == 2u;
    Require(((attrib3 >> 24u) & 3u) != 0u || (attrib2 & 0x3fffu) == 0u, "1D color targets taller than one row are unsupported");
    if (volume) {
        color.depth = (attrib3 & 0x1fffu) + 1u;
        if (maxMip != 0) {
            char message[384];
            std::snprintf(message, sizeof(message), "mipmapped 3D color targets are unsupported (slot %u, %ux%ux%u, viewMip %u, maxMip %u, slices %u..%u, CB_COLOR_INFO 0x%08x, ATTRIB2 0x%08x, ATTRIB3 0x%08x, VIEW 0x%08x)", slot, ((attrib2 >> 14u) & 0x3fffu) + 1u, (attrib2 & 0x3fffu) + 1u, color.depth, viewMip, maxMip, slice, lastSlice, info, attrib2, attrib3, view);
            Require(false, message);
        }
        Require(slice < color.depth && lastSlice >= slice, "the color view slices are outside the 3D surface");
        color.depthSlice = slice;
        color.layers = std::min(lastSlice + 1u, color.depth) - slice;
    } else if ((attrib3 & 0x1fffu) != 0) {
        Require(lastSlice <= (attrib3 & 0x1fffu), "the color view slice is beyond the array surface");
        color.arrayLastSlice = attrib3 & 0x1fffu;
        color.arraySlice = slice;
    }
    if (!volume && lastSlice != slice) {
        const auto exports = find(cx, 0x207);
        if (exports != cx.end() && (exports->second & (1u << 18u)) != 0) {
            if (color.arrayLastSlice == 0) {
                color.arrayLastSlice = lastSlice;
                color.arraySlice = slice;
            }
            color.layers = lastSlice + 1u - slice;
        }
    }
    color.extent = {((attrib2 >> 14u) & 0x3fffu) + 1u, (attrib2 & 0x3fffu) + 1u};
    color.elementBytes = decoded.elementBytes;
    std::uint64_t mipOffset = 0;
    color.surfaceExtent = color.extent;
    color.mipCount = maxMip + 1u;
    color.mip = viewMip;
    std::uint32_t firstTailLevel = maxMip + 1u;
    if (maxMip != 0) {
        const auto mips = ComputeElementMipLayout(ColorTextureTileMode(color.tileMode), color.elementBytes, color.extent.width, color.extent.height, maxMip + 1u);
        const auto& mip = mips.at(viewMip);
        mipOffset = mip.tiledOffset;
        color.mipTail = mip.tail;
        color.extent = {mip.width, mip.height};
        for (std::uint32_t level = 0; level < mips.size(); ++level) {
            if (mips[level].tail) {
                firstTailLevel = level;
                break;
            }
        }
    }
    const ColorTargetLayout colorLayout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
    const auto high = read(cx, 0x390 + slot);
    Require((high & ~0xffu) == 0, "invalid color address extension");
    color.surfaceAddress = (static_cast<std::uint64_t>(high) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x318 + stride)) << 8u);
    if (ColorTileModeIsXor(color.tileMode)) {
        color.pipeBankXor = static_cast<std::uint32_t>(color.surfaceAddress & (ColorTileModeBlockBytes(color.tileMode) - 1u));
        color.surfaceAddress -= color.pipeBankXor;
    }
    Require(color.tileMode != ColorTileMode::D64KBX || (color.pipeBankXor & ~0xcf00u) == 0, "unsupported display color pipe/bank XOR column bits");
    color.arrayAddress = color.arrayLastSlice != 0 ? color.surfaceAddress : 0;
    if (slice != 0 && !volume) {
        color.surfaceAddress += slice * ComputeSurfaceSize(ComputeElementMipLayout(ColorTextureTileMode(color.tileMode), color.elementBytes, color.surfaceExtent.width, color.surfaceExtent.height, color.mipCount), 1);
        if (color.arrayLastSlice == 0 && ColorTileModeIsXor(color.tileMode)) {
            const auto* equation = FindTextureSwizzleEquation(static_cast<std::uint32_t>(color.tileMode), color.elementBytes);
            Require(equation != nullptr, "a sliced color target has no swizzle equation");
            const auto coordinates = slice << 24u;
            for (std::uint32_t bit = 0; bit < equation->bits.size(); ++bit) {
                color.pipeBankXor ^= (std::popcount(equation->bits[bit] & coordinates) & 1u) << bit;
            }
        }
    }
    color.address = color.surfaceAddress + mipOffset;
    color.bytes = colorLayout.Bytes();
    GuestMemory::CheckRange(reinterpret_cast<const void*>(color.address), color.bytes, colorLayout.Alignment(), true);
    color.format = decoded.format;
    color.componentMapping = decoded.componentMapping;
    color.packing = decoded.packing;
    if (color.packing != ShaderRecompiler::ColorExportPacking::None && (info & 0x10000000u) != 0) throw std::runtime_error("AGC graphics: DCC-compressed " + std::string(PackedColorTargetName(color.packing)) + " color targets are unsupported");
    Require(color.packing != ShaderRecompiler::ColorExportPacking::Unorm10_11_11 || (info & 0x00040000u) == 0, "truncating (ROUND_MODE) 10_11_11 unorm color targets are unsupported");
    Require(color.packing != ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4 || (info & 0x00040000u) == 0, "truncating (ROUND_MODE) 4_4_4_4 unorm color targets are unsupported");
    Require(color.packing != ShaderRecompiler::ColorExportPacking::Float11_11_10 || (info & 0x00040000u) != 0, "rounding (ROUND_MODE 0) 11_11_10 float color targets are unsupported");
    for (std::uint32_t word = 0; word < 2; ++word) {
        const auto clear = find(cx, 0x323 + word + stride);
        color.clearWords[word] = clear == cx.end() ? 0u : clear->second;
    }
    if (((info & 0x2000u) != 0 || (color.samples > 1 && (info & 0x4000u) != 0)) && (color.samples > 1 || viewMip == 0)) {
        Require(color.samples > 1 || (!volume && slice == 0 && color.arrayLastSlice == 0), "CMASK fast clears of a 3D or array color target are unsupported");
        Require(((attrib3 >> 19u) & 0x1fu) == 0x18u && (attrib3 & 0x4000000u) != 0, "CMASK fast clears need pipe-aligned SW_64KB_Z_X metadata (CB_COLOR_ATTRIB3 FMASK_SW_MODE 24, CMASK_PIPE_ALIGNED)");
        if (color.samples == 1) {
            Require(color.elementBytes <= 8, "CMASK fast clears of texels over 64 bits are unsupported");
            if (maxMip != 0) Require(color.elementBytes > 2, "CMASK fast clears of mipmapped color targets with 8 or 16 bpp are unsupported");
        }
        const auto cmaskHigh = find(cx, 0x398 + slot);
        const auto cmask = ((cmaskHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(cmaskHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x31f + stride)) << 8u);
        Require((info & 0x2000u) == 0 || cmask != 0, "CMASK fast clears without a CMASK address are unsupported");
        if (cmask != 0) {
            std::size_t precedingBlocks = 0;
            std::size_t totalBlocks = static_cast<std::size_t>((color.surfaceExtent.width + 1023u) / 1024u) * ((color.surfaceExtent.height + 511u) / 512u);
            if (maxMip != 0) {
                if (firstTailLevel > 0) {
                    precedingBlocks = firstTailLevel < color.mipCount ? 1u : 0u;
                    for (auto level = firstTailLevel; level-- > 1u;) {
                        const auto w = std::max((color.surfaceExtent.width + (1u << level) - 1u) >> level, 1u);
                        const auto h = std::max((color.surfaceExtent.height + (1u << level) - 1u) >> level, 1u);
                        precedingBlocks += static_cast<std::size_t>((w + 1023u) / 1024u) * ((h + 511u) / 512u);
                    }
                    totalBlocks += precedingBlocks;
                } else {
                    totalBlocks = 1u;
                }
            }
            const auto cmaskMip0Offset = precedingBlocks * CmaskLayout::Alignment;
            const auto totalCmaskBytes = totalBlocks * CmaskLayout::Alignment;
            color.cmaskAddress = cmask + cmaskMip0Offset;
            color.cmaskBytes = CmaskBytes(color.extent.width, color.extent.height);
            if (color.samples == 1) GuestMemory::CheckRange(reinterpret_cast<const void*>(cmask), totalCmaskBytes, CmaskLayout::Alignment, true);
        }
        color.cmaskFastClear = (info & 0x2000u) != 0;
        if (const auto fmask = find(cx, 0x321 + stride); color.samples > 1u && fmask != cx.end()) {
            const auto high = find(cx, 0x3a0 + slot);
            const auto fmaskHigh = high == cx.end() ? 0u : high->second;
            Require((fmaskHigh & ~0xffu) == 0u, "invalid FMASK address extension");
            const auto address = (static_cast<std::uint64_t>(fmaskHigh) << 40u) | (static_cast<std::uint64_t>(fmask->second) << 8u);
            color.fmaskPipeBankXor = static_cast<std::uint32_t>(address & 0xffffu);
            color.fmaskAddress = address - color.fmaskPipeBankXor;
        }
    }
    if (color.coverageSamples > color.samples && (info & 0x4000u) != 0) {
        const auto fmaskHigh = find(cx, 0x3a0 + slot);
        color.fmaskAddress = (((fmaskHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(fmaskHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x321 + stride)) << 8u)) & ~0xffffull;
        Require(color.fmaskAddress != 0, "a compressed one-fragment color target without an FMASK address is unsupported");
    }
    if ((info & 0x10000000u) != 0) {
        if (maxMip == 0) {
            const auto dccHigh = find(cx, 0x3a8 + slot);
            color.dccAddress = ((dccHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(dccHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x325 + stride)) << 8u);
            color.dccAlphaOnMsb = DccAlphaOnMsb(color.format, swap);
            color.dccPipeAligned = ((attrib3 >> 30u) & 1u) != 0;
            if (color.samples > 1) {
                if (!allowMultisampleDcc) Require(color.tileMode == ColorTileMode::RenderTarget && color.dccAddress != 0 && color.dccPipeAligned && !volume && color.arrayLastSlice == 0 && slice == 0, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
                color.dccControl = read(cx, 0x31e + stride);
                if (!allowMultisampleDcc) Require((color.dccControl & 0x3ff80u) == 0, "multisampled DCC color transforms or lossy modes are not implemented");
            }
        } else {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[gpu] DCC keys of mipmapped color targets are ignored\n");
            }
        }
    }
    if (volume && (color.tileMode == ColorTileMode::RenderTarget || color.tileMode == ColorTileMode::Standard4KB)) {
        color.bytes = DescribeSurface(SurfaceForTarget(color)).guestBytes;
        GuestMemory::CheckRange(reinterpret_cast<const void*>(color.address), color.bytes, colorLayout.Alignment(), true);
    }
    return color;
}

namespace {

bool drawCovers(const Registers& cx, VkExtent2D extent) {
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu && viewportControl != 0u) throw std::runtime_error(vteMessage(viewportControl));
    const auto xs = viewportControl == 0u ? 0.0f : std::fabs(readFloat(cx, 0x10f));
    const auto xo = viewportControl == 0u ? 0.0f : readFloat(cx, 0x110);
    const auto ys = viewportControl == 0u ? 0.0f : std::fabs(readFloat(cx, 0x111));
    const auto yo = viewportControl == 0u ? 0.0f : readFloat(cx, 0x112);
    VkRect2D covered{{0, 0}, {0x7fffu, 0x7fffu}};
    intersect(covered, cx, 0xc, true);
    intersect(covered, cx, 0x81, false);
    intersect(covered, cx, 0x90, false);
    if ((read(cx, 0x292) & 2u) != 0) intersect(covered, cx, 0x94, false);
    const auto width = static_cast<float>(extent.width);
    const auto height = static_cast<float>(extent.height);
    const bool viewportCovers = viewportControl == 0u || (xo - xs <= 0.0f && xo + xs >= width && yo - ys <= 0.0f && yo + ys >= height);
    const bool scissorCovers = covered.offset.x == 0 && covered.offset.y == 0 && covered.extent.width >= extent.width && covered.extent.height >= extent.height;
    return viewportCovers && scissorCovers;
}

}

std::optional<DepthCopyPass> DecodeDepthCopyPass(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x000);
    if (control == cx.end() || (control->second & 4u) == 0) return std::nullopt;
    Require((control->second & ~0xf84u) == 0, "DB depth copy combined with a stencil copy, clear, resummarize or decompress");
    Require((read(cx, 0x200) & 0xfu) == 0, "DB depth copy with depth or stencil tests");
    Require((read(cx, 0x010) & 3u) == 3u, "DB depth copy from a depth plane that is not Z_32_FLOAT");
    const auto viewRegister = find(cx, 0x002);
    Require((read(cx, 0x010) & 0x000f1000u) == 0 && ((viewRegister == cx.end() ? 0u : viewRegister->second) & 0x3c000000u) == 0, "DB depth copy from a mipmapped or partially resident depth plane");
    Require(ColorWriteMask(cx) == 1u, "DB depth copy into anything but the red channel of color target 0");
    Require((read(cx, 0x1e0) & (1u << 30u)) == 0, "DB depth copy with color target 0 blending is unsupported");
    DepthCopyPass pass{};
    pass.source = decodeDepthPlane(cx);
    const auto maskLow = find(cx, 0x30e);
    const auto maskHigh = find(cx, 0x30f);
    Require((maskLow == cx.end() || maskLow->second == 0xffffffffu) && (maskHigh == cx.end() || maskHigh->second == 0xffffffffu), "DB depth copy with a sample mask is unsupported");
    pass.sample = (control->second & 0x80u) != 0 ? 0u : (control->second >> 8u) & 0xfu;
    Require(pass.sample < pass.source.samples, "DB depth copy of a sample the depth plane does not have");
    pass.color = DecodeColorBuffer(cx, 0);
    Require(pass.color.format == VK_FORMAT_R32_SFLOAT && pass.color.samples == 1 && pass.color.mip == 0 && pass.color.layers == 1, "DB depth copy into a color target that is not a single-sample R32 float");
    Require(pass.color.extent.width == pass.source.extent.width && pass.color.extent.height == pass.source.extent.height, "DB depth copy into a color target of another size");
    return pass;
}

std::optional<ColorMetadataPass> DecodeColorMetadataPass(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x202);
    if (control == cx.end()) return std::nullopt;
    const auto mode = (control->second >> 4u) & 7u;
    const auto aaConfig = find(cx, 0x2f8);
    const bool rasterizerSamples = aaConfig != cx.end() && (aaConfig->second & 7u) != 0;
    const auto sourceAttrib = find(cx, 0x31d);
    const bool sourceSamples = sourceAttrib != cx.end() && ((sourceAttrib->second >> 12u) & 7u) != 0;
    const bool resolve = mode == 3u && (rasterizerSamples || sourceSamples);
    const bool fmaskDecompress = mode == 5u;
    if (mode != 2u && mode != 6u && !resolve && !fmaskDecompress) return std::nullopt;
    ColorMetadataPass pass{resolve ? ColorMetadataPass::Mode::Resolve : fmaskDecompress ? ColorMetadataPass::Mode::FmaskDecompress : mode == 2u ? ColorMetadataPass::Mode::EliminateFastClear : ColorMetadataPass::Mode::DccDecompress, {}, std::nullopt};
    Require((control->second & ~0x70u) == 0xcc0000u, "CB metadata pass with a nonstandard ROP, dual quads disabled or degamma");
    const auto value = [&](std::uint32_t offset) {
        const auto found = find(cx, offset);
        return found == cx.end() ? 0u : found->second;
    };
    const bool planes = !depthPlanesAbsent(cx);
    const auto depthControl = planes ? read(cx, 0x200) : 0u;
    const bool depthWork = (depthControl & 0xfu) != 0 && ((depthControl & 8u) != 0 || !depthPassThrough(depthControl));
    if (depthWork || (planes && (read(cx, 0x0) & 0xfu) != 0)) {
        char text[192];
        std::snprintf(text, sizeof(text), "CB metadata pass with depth or stencil work (DB_DEPTH_CONTROL 0x%08x, DB_RENDER_CONTROL 0x%08x, DB_Z_INFO 0x%08x, DB_STENCIL_INFO 0x%08x)", value(0x200), value(0x0), value(0x010), value(0x011));
        throw std::runtime_error(text);
    }
    const auto modeControl = find(cx, 0x292);
    const bool multisampleRaster = modeControl != cx.end() && (modeControl->second & 1u) != 0;
    const auto samples = resolve && rasterizerSamples && multisampleRaster ? decodeSamples(cx) : 1u;
    if (!resolve) zero(cx, 0x2f8, ~0u, "multisampling or coverage conversion");
    zero(cx, 0x80, ~0u, "window offset");
    const auto requireCovered = [&](const ColorTarget& target) { Require(drawCovers(cx, target.extent), "CB metadata pass over part of a color target"); };
    if (resolve) {
        const auto source = DecodeColorBuffer(cx, 0);
        const auto destination = DecodeColorBuffer(cx, 1);
        Require(samples == 1 || source.samples == samples, "CB resolve from a target whose sample count differs from the rasterizer's");
        Require(destination.samples == 1, "CB resolve into a multisampled target is unsupported");
        Require(destination.cmaskAddress == 0, "CB resolve into a fast-clear color target is unsupported (the resolve does not update its CMASK)");
        Require(source.format == destination.format && source.extent.width == destination.extent.width && source.extent.height == destination.extent.height, "CB resolve between different formats or extents is unsupported");
        if (source.packing != ShaderRecompiler::ColorExportPacking::None) throw std::runtime_error("AGC graphics: CB resolves of " + std::string(PackedColorTargetName(source.packing)) + " color targets are unsupported");
        requireCovered(destination);
        pass.source = source;
        pass.targets.push_back(destination);
        return pass;
    }
    const auto targetMask = read(cx, 0x8e);
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        if (((targetMask >> (4u * slot)) & 0xfu) == 0 || ((read(cx, 0x31c + slot * 0xfu) >> 2u) & 0x1fu) == 0) continue;
        const auto target = DecodeColorBuffer(cx, slot, fmaskDecompress);
        if (fmaskDecompress) {
            Require(target.samples > 1, "CB metadata pass over a single-sample target");
            Require(target.cmaskAddress != 0, "FMASK decompress of a target without a CMASK address");
        } else {
            Require(target.samples == 1, "CB metadata pass over a multisampled color target, whose texels are on the host, is unsupported (only a CB resolve is modeled)");
        }
        Require((read(cx, 0x31c + slot * 0xfu) & 0x10000000u) == 0 || target.dccAddress != 0, "CB metadata pass over a mipmapped DCC color target, whose keys are not modeled");
        requireCovered(target);
        pass.targets.push_back(target);
    }
    return pass;
}

std::string DrawRejection(const QueueState& queue, bool indexed) {
    if (auto reason = DepthMaintenanceRejection(queue); !reason.empty()) return reason;
    const auto& cx = queue.context;
    // A register a rule needs that is absent gives no verdict here: DecodeState reports it.
    const auto value = [&](const Registers& registers, std::uint32_t offset, std::uint32_t& out) {
        const auto it = find(registers, offset, &registers == &queue.userConfig ? RegisterBank::UserConfig : RegisterBank::Context);
        if (it == registers.end()) return false;
        out = it->second;
        return true;
    };
    std::uint32_t word = 0;
    const auto nonzero = [&](const Registers& registers, std::uint32_t offset, std::uint32_t mask, const char* name) {
        return value(registers, offset, word) && (word & mask) != 0 ? zeroMessage(offset, word, name) : std::string();
    };
    const auto require = [&](bool condition, const char* reason) {
        return condition ? std::string() : "AGC graphics: " + std::string(reason);
    };
    if (indexed && value(queue.userConfig, 0x24b, word) && word != 0) {
        std::uint32_t primitive = 0;
        std::uint32_t resetIndex = 0;
        if (value(queue.userConfig, 0x242, primitive) && (primitive & 0x3fu) != 1 && (primitive & 0x3fu) != 2 && (primitive & 0x3fu) != 3 && (primitive & 0x3fu) != 4 && (primitive & 0x3fu) != 5 && (primitive & 0x3fu) != 6 && (primitive & 0x3fu) != 21) return "AGC graphics: primitive restart is only supported for point, line and triangle topologies";
        if (value(cx, 0x103, resetIndex) && (resetIndex & 0xffffu) != 0xffffu) return "AGC graphics: primitive restart index other than all ones is unsupported";
    }
    if (auto reason = nonzero(cx, 0x207, ~(LayerExports | ClipCullExports | (pointOutput(queue) ? 1u << 16u : inactiveVertexPointSize(queue))), "layer, viewport or auxiliary vertex exports"); !reason.empty()) return reason;
    if (value(cx, 0x207, word) && std::popcount(word & 0xffffu) > 8) return require(false, "more than eight clip and cull distances are unsupported");
    if (value(cx, 0x200, word)) {
        const auto renderControl = find(cx, 0x000);
        const bool depthClear = renderControl != cx.end() && (renderControl->second & 1u) != 0;
        std::uint32_t stencilInfo = 0;
        const bool stencilClear = renderControl != cx.end() && (renderControl->second & 2u) != 0 && !(value(cx, 0x011, stencilInfo) && (stencilInfo & 1u) == 0);
        if (stencilClear && !depthPlanesAbsent(cx)) {
            std::uint32_t view = 0;
            if (value(cx, 0x002, view) && (view & 0x02000000u) != 0) return require(false, "stencil clear requires a writable stencil plane");
        }
        if (depthClear && !depthPlanesAbsent(cx)) {
            std::uint32_t z = 0, view = 0;
            if ((value(cx, 0x010, z) && (z & 3u) == 0) || (value(cx, 0x002, view) && (view & 0x01000000u) != 0)) return require(false, "depth clear requires a writable depth plane");
        }
        const auto tests = planeTests(cx, word);
        const bool surface = (tests != 0 || depthClear || stencilClear) && depthSurfaceBound(cx);
        if (!surface && !((word & 0xbu) != 0 && (tests == 0 || depthPlanesAbsent(cx))) && !depthPassThrough(word) && !IgnoreDepthTest() && (effectiveDepthControl(word) & DepthControlMask) != 0) return zeroMessage(0x200, word, "depth, stencil or conditional color writes");
        if (auto reason = require((word & 8u) == 0 || surface || depthPlanesAbsent(cx), "depth bounds without a depth surface"); !reason.empty()) return reason;
        if (auto reason = require((word & 0xc0000000u) == 0, "depth-conditional color writes are unsupported"); !reason.empty()) return reason;
    }
    std::uint32_t zFormat = 0;
    static_cast<void>(value(cx, 0x1c4, zFormat));
    if (auto reason = nonzero(cx, 0x203, shaderControlMask(zFormat), "depth export, shader coverage or ordered fragment execution"); !reason.empty()) return reason;
    if (value(cx, 0x2dc, word)) {
        const bool alphaDisabled = word == AlphaToCoverageDithered && alphaToCoverageDisabled(cx);
        if ((word != AlphaToCoverageUndithered && word != AlphaToCoverageDithered) || alphaDisabled) {
            if (auto reason = nonzero(cx, 0x2dc, alphaDisabled ? AlphaToCoverageMask & ~1u : AlphaToCoverageMask, "dithered or rounded alpha-to-coverage"); !reason.empty()) return reason;
        }
        if (!alphaDisabled && word == AlphaToCoverageDithered && !ditheredAlphaSamplesSupported(cx)) return "AGC graphics: dithered alpha-to-coverage requires eight matched raster/DB_EQAA samples and one pixel iteration";
        if (!alphaDisabled && word == AlphaToCoverageDithered) {
            if (!ditheredAlphaLocationsSupported(cx)) return "AGC graphics: dithered alpha-to-coverage with custom sample locations is unsupported";
            std::uint32_t format = 0;
            if (value(cx, 0x1c5, format)) {
                format &= 0xfu;
                if (format != 3u && format != 4u && format != 5u && format != 6u && format != 9u) return "AGC graphics: alpha-to-coverage needs an MRT0 export format with a floating-point alpha";
                if (format != 4u && format != 9u) return "AGC graphics: dithered alpha-to-coverage needs an FP16_ABGR or 32_ABGR export";
            }
        }
    }
    if (auto reason = nonzero(cx, 0x2f8, ~(0x7u | 0x1e000u | 0x700000u | CoveredCentroidIsCenter), "coverage conversion or nonstandard multisampling"); !reason.empty()) return reason;
    if (!coveredCentroidSupported(queue)) return "AGC graphics: COVERED_CENTROID_IS_CENTER with active or unspecified centroid inputs is not implemented";
    const bool multisampled = value(cx, 0x2f8, word) && (word & 7u) != 0;
    if (auto reason = nonzero(cx, 0x292, ScanModeMask & ~1u, "scan conversion mode"); !reason.empty()) return reason;
    if (!multisampled && value(cx, 0x292, word) && (word & 1u) != 0) {
        if (auto reason = singleSampleMsaaRejection(queue); !reason.empty()) return reason;
    }
    if (auto reason = nonzero(cx, 0x293, ScanControlMask, "primitive discard or out-of-order rasterization"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x80, ~0u, "window offset"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x8d, ScreenOffsetMask, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits"); !reason.empty()) return reason;
    if (value(cx, 0x83, word) && word != 0xffffu) return require(false, "clip rectangles are unsupported");
    if (value(cx, 0x8c, word) && (word & 0xfu) != 0xau) return require(false, "nonstandard triangle edge rules are unsupported");
    if (value(cx, 0x2f9, word) && word != 0x2du) return require(false, "nonstandard pixel center or vertex quantization is unsupported");
    if (value(cx, 0x313, word) && (word & ~0x001f6000u) != 0 && word != 0x6001u) return conservativeMessage(word);
    std::uint32_t config = 0, other = 0;
    if (value(cx, 0x2f8, config) && value(cx, 0x30e, word) && value(cx, 0x30f, other)) {
        if (auto reason = sampleMaskRejection(queue, config, word, other); !reason.empty()) return reason;
    }
    if (value(cx, 0x206, word) && word != 0x43fu && word != 0u) return vteMessage(word);
    if (auto reason = nonzero(cx, 0x204, ClipControlMask, "a PA_CL_CLIP_CNTL flag"); !reason.empty()) return reason;
    std::uint32_t targetMask = 0, shaderMask = 0;
    if (value(cx, 0x8e, targetMask) && value(cx, 0x8f, shaderMask) && value(cx, 0x202, word) && !colorControlSupported(word, ColorWriteMask(cx) != 0)) return colorControlMessage(word);
    if (auto reason = nonzero(cx, 0x1c4, zFormatSupported(zFormat) ? 0u : ~0u, "depth or sample-mask export"); !reason.empty()) return reason;
    if (PixelProgramSkipped(queue)) return NullPixelProgramRejection(queue);
    for (const auto offset : {0x1b3u, 0x1b4u, 0x1c5u}) {
        if (find(cx, offset) != cx.end()) continue;
        char text[64];
        std::snprintf(text, sizeof(text), "AGC graphics: missing register at DWORD 0x%x", offset);
        return text;
    }
    return {};
}

bool PixelProgramSkipped(const QueueState& queue) {
    const auto low = find(queue.shader, 0x008, RegisterBank::Shader);
    const auto high = find(queue.shader, 0x009, RegisterBank::Shader);
    if ((low == queue.shader.end() || low->second == 0) && (high == queue.shader.end() || high->second == 0)) return true;
    const auto& cx = queue.context;
    const auto targetMask = find(cx, 0x8e);
    const auto shaderMask = find(cx, 0x8f);
    const auto shaderControl = find(cx, 0x203);
    if (targetMask == cx.end() || shaderMask == cx.end() || shaderControl == cx.end()) return false;
    const auto alphaToMask = find(cx, 0x2dc);
    if (alphaToMask != cx.end() && (alphaToMask->second & 1u) != 0) return false;
    return (targetMask->second & shaderMask->second) == 0 && (shaderControl->second & PixelStageRunsMask) == 0;
}

std::string NullPixelProgramRejection(const QueueState& queue) {
    const auto targetMask = find(queue.context, 0x8e);
    const auto shaderMask = find(queue.context, 0x8f);
    if (targetMask == queue.context.end() || shaderMask == queue.context.end()) return "AGC graphics: a draw without a pixel program needs CB_TARGET_MASK and CB_SHADER_MASK";
    const auto colorControl = find(queue.context, 0x202);
    const bool colorDisabled = colorControl != queue.context.end() && ((colorControl->second >> 4u) & 7u) == 0u;
    if (!colorDisabled && ColorWriteMask(queue.context) != 0) return "AGC graphics: a draw without a pixel program writes color";
    return {};
}

std::vector<RegisterRead>*& RegisterReadLog() {
    static thread_local std::vector<RegisterRead>* log = nullptr;
    return log;
}

std::atomic<std::uint32_t>& RegisterReadLogsOpen() {
    static std::atomic<std::uint32_t> open{0};
    return open;
}

bool SplitsAlternateAlpha(const State& state) {
    for (std::size_t index = 0; index < state.colors.size() && index < state.blends.size(); ++index) {
        const auto& blend = state.blends[index];
        if (state.colors[index].componentMapping != 0xecu || !blend.blendEnable) continue;
        if (blend.srcColorBlendFactor != blend.srcAlphaBlendFactor || blend.dstColorBlendFactor != blend.dstAlphaBlendFactor || blend.colorBlendOp != blend.alphaBlendOp) return true;
    }
    return false;
}

bool StencilOperationValueReplaces(const Registers& context) {
    const auto value = [&](std::uint32_t offset) {
        const auto found = find(context, offset);
        return found == context.end() ? 0u : found->second;
    };
    if ((value(0x203) & 6u) != 4u) return false;
    const auto depthControl = value(0x200);
    if ((depthControl & 1u) == 0 || (value(0x011) & 1u) == 0 || (value(0x002) & 0x02000000u) != 0 || (value(0x000) & 2u) != 0) return false;
    const auto replaces = [](std::uint32_t ops, std::uint32_t refMask) {
        if (((refMask >> 16u) & 0xffu) == 0) return false;
        return (ops & 0xfu) == 4u || ((ops >> 4u) & 0xfu) == 4u || ((ops >> 8u) & 0xfu) == 4u;
    };
    const auto ops = value(0x10b);
    return replaces(ops, value(0x10c)) || ((depthControl & 0x80u) != 0 && replaces(ops >> 12u, value(0x10d)));
}

bool StencilReferenceUnused(const State& state) {
    if (!state.depth || !state.stencilTest) return true;
    if (state.depthTest && (state.depthCompare < VK_COMPARE_OP_NEVER || state.depthCompare > VK_COMPARE_OP_ALWAYS)) return false;
    const auto faceUnused = [&](const VkStencilOpState& face) {
        if (face.compareOp < VK_COMPARE_OP_NEVER || face.compareOp > VK_COMPARE_OP_ALWAYS) return false;
        for (const auto op : {face.failOp, face.passOp, face.depthFailOp}) {
            if (op < VK_STENCIL_OP_KEEP || op > VK_STENCIL_OP_DECREMENT_AND_WRAP) return false;
        }
        if (face.compareOp != VK_COMPARE_OP_ALWAYS && face.compareOp != VK_COMPARE_OP_NEVER && (face.compareMask & 0xffu) != 0) return false;
        if ((face.writeMask & 0xffu) == 0) return true;
        if (state.depthBoundsTest) return face.failOp != VK_STENCIL_OP_REPLACE && face.passOp != VK_STENCIL_OP_REPLACE && face.depthFailOp != VK_STENCIL_OP_REPLACE;
        const bool passes = face.compareOp == VK_COMPARE_OP_ALWAYS || face.compareOp == VK_COMPARE_OP_EQUAL || face.compareOp == VK_COMPARE_OP_LESS_OR_EQUAL || face.compareOp == VK_COMPARE_OP_GREATER_OR_EQUAL;
        if (!passes) return face.failOp != VK_STENCIL_OP_REPLACE;
        const bool depthPass = !state.depthTest || state.depthCompare != VK_COMPARE_OP_NEVER;
        const bool depthFail = state.depthTest && state.depthCompare != VK_COMPARE_OP_ALWAYS;
        return (!depthPass || face.passOp != VK_STENCIL_OP_REPLACE) && (!depthFail || face.depthFailOp != VK_STENCIL_OP_REPLACE);
    };
    return faceUnused(state.stencilFront) && faceUnused(state.stencilBack);
}

bool DrawKeyCovers(RegisterRead read) {
    // One bit per register offset of each bank, set from the table once.
    static const auto covered = [] {
        std::array<std::bitset<1024>, static_cast<std::size_t>(RegisterBank::Count)> bits{};
        for (const auto& range : DrawKeyRegisters) {
            for (std::uint32_t i = 0; i < range.count; ++i) bits[static_cast<std::size_t>(range.bank)].set(range.first + i);
        }
        return bits;
    }();
    return read.bank < RegisterBank::Count && read.offset < 1024 && covered[static_cast<std::size_t>(read.bank)].test(read.offset);
}

}
