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
static bool SignedNormalizedFormat(VkFormat format) {
    return format == VK_FORMAT_R8_SNORM || format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R8G8_SNORM || format == VK_FORMAT_R16G16_SNORM || format == VK_FORMAT_R8G8B8A8_SNORM || format == VK_FORMAT_B8G8R8A8_SNORM || format == VK_FORMAT_R16G16B16A16_SNORM;
}

static std::uint32_t NormalizedChannelMask(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R16_SNORM: return 0x1u;
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R16G16_SNORM: return 0x3u;
        default: return 0xfu;
    }
}

namespace {

std::vector<std::uint32_t> ColorExportSlots(std::uint32_t shaderMask) {
    std::vector<std::uint32_t> slots;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        if (((shaderMask >> (4u * slot)) & 0xfu) != 0) slots.push_back(slot);
    }
    return slots;
}

std::uint32_t ColorExportCount(const Registers& context, std::uint32_t written, const std::vector<std::uint32_t>& slots) {
    const auto formatEntry = find(context, 0x1c5);
    const auto format = formatEntry == context.end() ? 0u : formatEntry->second;
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < slots.size(); ++index) {
        if (((written >> (4u * slots[index])) & 0xfu) == 0) continue;
        if (((format >> (4u * index)) & 0xfu) == 0) continue;
        count = index + 1;
    }
    if (((read(context, 0x202) >> 4u) & 7u) == 0u) count = 0;
    return count;
}

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
    const bool alphaDisabled = ditheredAlphaToCoverageWord(alpha) && alphaToCoverageDisabled(cx);
    result.alphaToCoverage = alpha == AlphaToCoverageUndithered || (!alphaDisabled && ditheredAlphaToCoverageWord(alpha));
    result.ditheredAlphaToCoverage = result.alphaToCoverage && ditheredAlphaToCoverageWord(alpha);
    if (!result.alphaToCoverage) zero(cx, 0x2dc, alphaDisabled ? AlphaToCoverageMask & ~1u : AlphaToCoverageMask, "dithered or rounded alpha-to-coverage");
    if (result.ditheredAlphaToCoverage && alpha == AlphaToCoverageDithered) {
        Require(ditheredAlphaSamplesSupported(cx), "dithered alpha-to-coverage requires eight matched raster/DB_EQAA samples and one pixel iteration");
        Require(!result.customSampleLocations, "dithered alpha-to-coverage with custom sample locations is unsupported");
    }
    if (result.ditheredAlphaToCoverage && alpha == AlphaToCoverageTwoSampleDithered) {
        Require(twoSampleDitheredAlphaSamplesSupported(cx), "DB_ALPHA_TO_MASK 0xe701 requires two matched raster/DB_EQAA samples and one pixel iteration");
        Require(twoSampleDitheredAlphaLocationsSupported(cx), "DB_ALPHA_TO_MASK 0xe701 is supported only with the two-sample locations (4,4),(-4,-4) or (-4,4),(4,-4) in every pixel");
        result.ditheredAlphaSamples = 2;
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
    if (result.depth && result.depth->samples > 1 && result.samples == 1) {
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
    const bool pointsOrLines = result.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || (result.stages.mesh.has_value() && result.stages.mesh->outputPrimitiveSize <= 2u);
    const auto lineOffset = (raster & 0x1800u) != 0 && !pointsOrLines ? 0x2000u : 0u;
    const auto rasterMode = raster & ~0x7u & ~0x7f8u & ~(1u << 24u) & ~0x1800u & ~lineOffset & ~(1u << 19u) & ~(1u << 16u);
    if (rasterMode != 0) throw std::runtime_error("AGC graphics: " + zeroMessage(0x205, raster, "depth bias or nonstandard rasterization"));
    result.cullMode = ((raster & 1u) != 0 ? VK_CULL_MODE_FRONT_BIT : 0u) | ((raster & 2u) != 0 ? VK_CULL_MODE_BACK_BIT : 0u);
    if (result.rectList) result.cullMode = VK_CULL_MODE_NONE;
    result.polygonMode = decodePolygonMode(cx, raster, result, pointsOrLines);
    if (result.polygonMode == VK_POLYGON_MODE_LINE) result.lineWidth = static_cast<float>(find(cx, 0x282)->second & 0xffffu) / 8.0f;
    if ((raster & 0x1800u) != 0 && result.depth) decodeDepthBias(cx, raster, result);
    result.frontFace = (raster & 4u) != 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    result.provokingVertexMode = (raster & (1u << 19u)) != 0 ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    APS5_LOG_OUT_DEBUG("Raster=0x%x cullMode=0x%x frontFace=%u negativeOneToOne=%u", raster, static_cast<unsigned>(result.cullMode), static_cast<unsigned>(result.frontFace), result.negativeOneToOne ? 1u : 0u);
    const auto shaderMask = read(cx, 0x8f);
    const auto targetMask = ColorWriteMask(cx);
    APS5_LOG_OUT_DEBUG("CB_TARGET_MASK=0x%x CB_SHADER_MASK=0x%x", targetMask, shaderMask);
    const auto exportFormatEntry = find(cx, 0x1c5);
    const auto exportFormat = exportFormatEntry == cx.end() ? 0u : exportFormatEntry->second;
    const auto exportSlots = ColorExportSlots(shaderMask);
    const auto exportCount = ColorExportCount(cx, targetMask, exportSlots);
    result.hasColorTarget = exportCount != 0;
    APS5_LOG_OUT_DEBUG("hasColorTarget=%u exports=%u", result.hasColorTarget ? 1u : 0u, exportCount);

    if (const auto colorControl = read(cx, 0x202); !colorControlSupported(colorControl, result.hasColorTarget)) throw std::runtime_error(colorControlMessage(colorControl));
    if (const auto colorControl = read(cx, 0x202); ((colorControl >> 4u) & 7u) == 1u) result.colorRop = static_cast<std::uint8_t>(colorControl >> 16u);
    zero(cx, 0x1c4, zFormatSupported(read(cx, 0x1c4)) ? 0u : ~0u, "depth or sample-mask export");
    APS5_LOG_OUT_DEBUG("Export format=%u", exportFormat);
    const auto positionFormat = read(cx, 0x1c3) & 0xfu;
    if (positionFormat == 0u) {
        result.rasterizerDiscard = true;
    } else {
        Require(positionFormat == 4u, "position export format " + std::to_string(positionFormat) + " is unsupported");
    }
    for (std::uint32_t index = 0; index < exportCount; ++index) {
        const auto slot = exportSlots[index];
        if (((targetMask >> (4u * slot)) & 0xfu) == 0) continue;
        const auto slotExport = (exportFormat >> (4u * index)) & 0xfu;
        if (slotExport == 0) continue;
        if (slotExport == 8 || slotExport > 9) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " is unsupported");
        auto color = DecodeColorBuffer(cx, slot);
        if (slotExport == 7 && ((read(cx, 0x31c + slot * 0xfu) >> 8u) & 7u) != 4u) {
            const auto alias = Uint16ExportAlias(color.format);
            if (alias == VK_FORMAT_UNDEFINED) throw std::runtime_error("AGC graphics: color export format 7 (UINT16_ABGR) into a target that is not unsigned integer is unsupported");
            color.format = alias;
        }
        color.exportIndex = index;
        color.uintExport = slotExport == 7;
        if (color.uintExport && color.packing == ShaderRecompiler::ColorExportPacking::Uint) color.packing = ShaderRecompiler::ColorExportPacking::None;
        if (color.samples == 1 && result.samples > 1 && result.colors.empty() && !result.depth && (read(cx, 0x292) & 1u) == 0) {
            const auto sampleIndex = (read(cx, 0x293) & PixelShaderIteratesSamples) != 0u ? ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary) : 0u;
            const auto sampleInputs = read(cx, 0x1b3) & read(cx, 0x1b4) & (sampleIndex | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::SampleCoverage));
            Require(sampleInputs == 0 && (read(cx, 0x203) & 0x100u) == 0, "a one-sample color target under a multisampled rasterizer without MSAA_ENABLE is unsupported with a sample index input under PS_ITER_SAMPLE, a coverage input, or a sample-mask export");
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
    result.viewportExports = read(cx, 0x207) & ViewportIndexExport;
    if ((read(cx, 0x207) & LayerExports & ~result.layerExports & ~result.viewportExports & ~(1u << 21u)) != 0) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] layer/viewport index vertex exports are ignored (PA_CL_VS_OUT_CNTL=0x%08x)\n", read(cx, 0x207));
        }
    }
    const bool areaFree = result.stages.path == ShaderPath::Vertex && (result.rectList || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN || result.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    bool collapsed = false;
    if (viewportControl == 0u && !viewportProgrammed(cx)) {
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
    if (result.viewportExports != 0u) {
        const auto clearControl = find(cx, 0x000);
        const bool depthClear = result.depth && clearControl != cx.end() && (clearControl->second & 1u) != 0;
        result.viewportScissors.assign(1, result.scissor);
        auto bounds = result.scissor;
        for (std::uint32_t index = 1; index < MaxViewports; ++index) {
            const auto base = 0x10fu + 6u * index;
            const auto optionalFloat = [&](std::uint32_t offset) {
                const auto it = find(cx, offset);
                return it == cx.end() ? std::numeric_limits<float>::quiet_NaN() : std::bit_cast<float>(it->second);
            };
            const auto vxs = optionalFloat(base);
            const auto vxo = optionalFloat(base + 1u);
            const auto vys = optionalFloat(base + 2u);
            const auto vyo = optionalFloat(base + 3u);
            const auto vzs = optionalFloat(base + 4u);
            const auto vzo = optionalFloat(base + 5u);
            const auto vMinDepth = result.negativeOneToOne ? vzo - vzs : vzo;
            const auto vMaxDepth = vzo + vzs;
            const bool vCollapsed = (vxs == 0 || vys == 0) && vxs >= 0 && areaFree && std::isfinite(vxo) && std::isfinite(vyo) && std::isfinite(vMinDepth) && std::isfinite(vMaxDepth);
            bool usable = vCollapsed || (vxs > 0 && std::isfinite(vxs) && vys != 0 && std::isfinite(vys) && std::isfinite(vxo) && std::isfinite(vyo) && std::isfinite(vMinDepth) && std::isfinite(vMaxDepth));
            auto viewport = vCollapsed ? VkViewport{vxo, vyo, 1, 1, vMinDepth, vMaxDepth} : VkViewport{vxo - vxs, vyo - vys, 2 * vxs, 2 * vys, vMinDepth, vMaxDepth};
            if (depthClear) {
                viewport.minDepth = result.depth->clearDepth;
                viewport.maxDepth = result.depth->clearDepth;
            }
            VkRect2D scissor{{0, 0}, result.renderExtent};
            intersect(scissor, cx, 0xc, true);
            intersect(scissor, cx, 0x81, false);
            intersect(scissor, cx, 0x90, false);
            const auto vportScissor = 0x94u + 2u * index;
            if ((read(cx, 0x292) & 2u) != 0 && (find(cx, vportScissor) == cx.end() || find(cx, vportScissor + 1u) == cx.end() || !intersectViewportScissor(scissor, cx, vportScissor))) usable = false;
            if (!usable) viewport = VkViewport{0, 0, 1, 1, 0, 1};
            if (!usable || vCollapsed) scissor.extent = {0, 0};
            result.extraViewports.push_back(viewport);
            result.viewportScissors.push_back(scissor);
            unite(bounds, scissor);
        }
        result.scissor = bounds;
    }
    result.blends.assign(exportCount, VkPipelineColorBlendAttachmentState{});
    std::array<float, 4> requiredBlendConstants{};
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
            for (std::uint32_t i = 0; i < 4; ++i) result.blendConstants[i] = readFloat(cx, 0x105 + i);
            if (reversed) {
                const auto translated = TranslateReversedBlend(blend, mapping, color.format, opaqueDestination, result.blendConstants);
                state.srcColorBlendFactor = translated.blend.srcColorBlendFactor;
                state.dstColorBlendFactor = translated.blend.dstColorBlendFactor;
                state.colorBlendOp = translated.blend.colorBlendOp;
                state.srcAlphaBlendFactor = translated.blend.srcAlphaBlendFactor;
                state.dstAlphaBlendFactor = translated.blend.dstAlphaBlendFactor;
                state.alphaBlendOp = translated.blend.alphaBlendOp;
                if (translated.shaderCode != 0u) result.reversedBlend = translated.shaderCode;
                for (std::uint32_t i = 0; i < 4; ++i) {
                    if ((translated.constantMask & (1u << i)) == 0u || (state.colorWriteMask & (1u << i)) == 0u) continue;
                    Require((requiredBlendMask & (1u << i)) == 0 || requiredBlendConstants[i] == translated.constants[i], "color targets require conflicting blend constants after component swap");
                    requiredBlendConstants[i] = translated.constants[i];
                    requiredBlendMask |= 1u << i;
                }
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
                const auto constantAlpha = [](VkBlendFactor factor) { return factor == VK_BLEND_FACTOR_CONSTANT_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA; };
                const auto anyConstant = [&](VkBlendFactor factor) { return constantAlpha(factor) || factor == VK_BLEND_FACTOR_CONSTANT_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR; };
                const auto factorsCount = [](VkBlendOp op) { return op != VK_BLEND_OP_MIN && op != VK_BLEND_OP_MAX; };
                const bool colorReadsAlpha = (state.colorWriteMask & 7u) != 0 && factorsCount(state.colorBlendOp) && (constantAlpha(state.srcColorBlendFactor) || constantAlpha(state.dstColorBlendFactor));
                const bool alphaReadsAlpha = (state.colorWriteMask & 8u) != 0 && factorsCount(state.alphaBlendOp) && (anyConstant(state.srcAlphaBlendFactor) || anyConstant(state.dstAlphaBlendFactor));
                if (colorReadsAlpha || alphaReadsAlpha) {
                    const auto value = result.blendConstants[3];
                    Require((requiredBlendMask & 8u) == 0 || requiredBlendConstants[3] == value, "color targets require conflicting blend constants after component swap");
                    requiredBlendConstants[3] = value;
                    requiredBlendMask |= 8u;
                }
            }
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
                if (destinationAlpha(state.srcColorBlendFactor) || destinationAlpha(state.dstColorBlendFactor) || destinationAlpha(state.srcAlphaBlendFactor) || destinationAlpha(state.dstAlphaBlendFactor)) throw std::runtime_error("AGC graphics: blending into a two-channel color target with the alternate component order with a factor of the destination alpha, which the target keeps in its second channel, is not implemented");
                const auto weighted = [](VkBlendOp operation) { return operation != VK_BLEND_OP_MIN && operation != VK_BLEND_OP_MAX; };
                if ((weighted(state.colorBlendOp) && (constantColor(state.srcColorBlendFactor) || constantColor(state.dstColorBlendFactor))) || (weighted(state.alphaBlendOp) && (constantColor(state.srcAlphaBlendFactor) || constantColor(state.dstAlphaBlendFactor)))) throw std::runtime_error("AGC graphics: blending into a two-channel color target with the alternate component order with a constant color factor, which Vulkan applies to the second channel with the green blend constant instead of the alpha one, is not implemented");
            }
        }
        if (UintPacked(color.packing)) state.blendEnable = VK_FALSE;
        if (WordPacked(color.packing)) {
            const std::string packed = std::string(color.packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 ? "an " : "a ") + PackedColorTargetName(color.packing);
            const auto slotExport = (exportFormat >> (4u * color.exportIndex)) & 0xfu;
            if (slotExport == 5 || slotExport == 6) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " into " + packed + " target is unsupported");
            if (state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && state.dstColorBlendFactor == VK_BLEND_FACTOR_ZERO && state.colorBlendOp == VK_BLEND_OP_ADD) state.blendEnable = VK_FALSE;
            const bool floating = color.packing == ShaderRecompiler::ColorExportPacking::Float11_11_10;
            if (floating && state.blendEnable) {
                color.mergeBlend = PackedBlendControl(blend);
                state.blendEnable = VK_FALSE;
            }
            Require(!state.blendEnable, "blending into " + packed + " color target is unsupported");
            const bool nibbles = color.packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4;
            const std::uint32_t packedMask = nibbles ? 0xfu : 7u;
            Require(nibbles || floating || (exportedMask & packedMask) == packedMask, "partial writes of " + packed + " color target are unsupported");
            if ((exportedMask & packedMask) != packedMask || color.mergeBlend != 0u) {
                const std::string merged = color.mergeBlend != 0u ? "blending into " : "partial writes of ";
                if (color.depth != 1u || (color.samples > 1u && (color.mipCount != 1u || color.layers != 1u || color.arrayLastSlice != 0u))) {
                    char detail[160];
                    std::snprintf(detail, sizeof(detail), " (%ux%u, %u samples, mip %u of %u%s, depth %u slice %u, layers %u, last slice %u)", color.extent.width, color.extent.height, color.samples, color.mip, color.mipCount, color.mipTail ? " in the tail" : "", color.depth, color.depthSlice, color.layers, color.arrayLastSlice);
                    Require(false, merged + "a volume " + std::string(PackedColorTargetName(color.packing)) + " color target, or a mipmapped or array multisampled one, are not implemented" + detail);
                }
                Require(std::ranges::none_of(result.colors, [](const ColorTarget& other) { return other.partialWriteMask != 0 || other.staged; }), std::string(color.mergeBlend != 0u ? "blending into a packed color target merges into its target alone" : "partial writes of a packed color target merge into their target alone") + " and cannot join another merged color target of the draw");
                const auto written = PackedWriteMask(color.packing, mapping, exportedMask & packedMask);
                Require(written != 0u, merged + packed + " color target that writes none of its channels is not implemented");
                Require(!floating || color.samples == 1u, merged + "a multisampled 11_11_10 float color target are not implemented");
                if (color.samples > 1u) {
                    Require(LogicOpFromRop(result.colorRop) == VK_LOGIC_OP_COPY, "a partial write of a multisampled 4_4_4_4 unorm color target under a color logic operation is not implemented");
                    color.staged = true;
                    color.stagedMask = static_cast<std::uint16_t>(written);
                    color.stagedFormat = color.format;
                    color.format = VK_FORMAT_R32_UINT;
                    state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
                } else {
                    color.partialWriteMask = written;
                    state.colorWriteMask = 0;
                }
            }
        }
        if (color.packing == ShaderRecompiler::ColorExportPacking::Uint) {
            const auto slotExport = (exportFormat >> (4u * color.exportIndex)) & 0xfu;
            if (slotExport == 2 || slotExport == 4 || slotExport == 9) color.packing = NarrowUintPacking(color.format, ShaderRecompiler::ColorExportPacking::UintClamp8, ShaderRecompiler::ColorExportPacking::UintClamp16);
            else if (slotExport == 1) color.packing = NarrowUintPacking(color.format, ShaderRecompiler::ColorExportPacking::UintMask8, ShaderRecompiler::ColorExportPacking::UintMask16);
            else if (slotExport != 3) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " into an unsigned integer target is unsupported");
        }
        if (state.blendEnable && !reversed && (secondSource(state.srcColorBlendFactor) || secondSource(state.dstColorBlendFactor) || secondSource(state.srcAlphaBlendFactor) || secondSource(state.dstAlphaBlendFactor))) {
            const auto supported = [](VkBlendOp op, VkBlendFactor source, VkBlendFactor destination) {
                return (op != VK_BLEND_OP_MIN && op != VK_BLEND_OP_MAX) || (source == VK_BLEND_FACTOR_ONE && destination == VK_BLEND_FACTOR_ONE);
            };
            Require(supported(state.colorBlendOp, state.srcColorBlendFactor, state.dstColorBlendFactor) && supported(state.alphaBlendOp, state.srcAlphaBlendFactor, state.dstAlphaBlendFactor), "dual-source blending with a MIN or MAX operation whose factors are not ONE is unsupported");
            result.dualSourceBlend = true;
        }
        if (const auto info = read(cx, 0x31c + slot * 0xfu); state.blendEnable && (info & 0x8000u) == 0 && ((info >> 8u) & 7u) <= 1u) {
            char detail[80];
            std::snprintf(detail, sizeof(detail), " (CB_COLOR_INFO 0x%08x, CB_BLEND%u_CONTROL 0x%08x)", info, slot, blend);
            const std::string where = detail;
            Require(color.samples == 1u && color.packing == ShaderRecompiler::ColorExportPacking::None, "blending unclamped normalized color into a multisampled or packed color target is not implemented" + where);
            Require(!reversed && mapping != 0xecu, "blending unclamped normalized color into a color target with a reversed or alternate component order is not implemented" + where);
            Require(!(secondSource(state.srcColorBlendFactor) || secondSource(state.dstColorBlendFactor) || secondSource(state.srcAlphaBlendFactor) || secondSource(state.dstAlphaBlendFactor)), "dual-source blending of unclamped normalized color is not implemented" + where);
            Require(std::ranges::none_of(result.colors, [&](const ColorTarget& other) { return &other != &color && other.partialWriteMask != 0 && other.partialWriteMask != UnclampedBlendMask; }), "blending unclamped normalized color cannot join a partial write of a packed color target in one draw" + where);
            Require(!SignedNormalizedFormat(color.format) || (state.colorWriteMask & NormalizedChannelMask(color.format)) == NormalizedChannelMask(color.format), "blending unclamped signed normalized color with a write mask that skips a component is not implemented, since the shader rewrites the skipped component through a float and the code -128 would become -127" + where);
            color.partialWriteMask = UnclampedBlendMask;
        }
        result.blends[color.exportIndex] = state;
    }
    for (std::uint32_t i = 0; i < 4; ++i) {
        if ((requiredBlendMask & (1u << i)) != 0) result.blendConstants[i] = requiredBlendConstants[i];
    }
    Require((!result.mergedCoverage && !result.truncatedCoverage) || std::ranges::none_of(result.colors, [](const ColorTarget& color) { return color.partialWriteMask != 0 || color.staged; }), "partial writes of a 4_4_4_4 unorm color target under merged or truncated coverage (a one-sample target under a multisampled rasterizer) are not implemented");
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

}
}
