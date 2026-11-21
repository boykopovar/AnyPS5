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
namespace {
namespace {
namespace {
bool drawCovers(const Registers& cx, VkExtent2D extent, const ColorTarget* gcnTarget = nullptr) {
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu && viewportControl != 0u) throw std::runtime_error(vteMessage(viewportControl));
    const auto xs = std::fabs(readFloat(cx, 0x10f));
    const auto xo = readFloat(cx, 0x110);
    const auto ys = std::fabs(readFloat(cx, 0x111));
    const auto yo = readFloat(cx, 0x112);
    VkRect2D covered{{0, 0}, {0x7fffu, 0x7fffu}};
    intersect(covered, cx, 0xc, true);
    intersect(covered, cx, 0x81, false);
    intersect(covered, cx, 0x90, false);
    if ((read(cx, 0x292) & 2u) != 0) intersect(covered, cx, 0x94, false);
    if (gcnTarget != nullptr && covered.offset.x == 0 && covered.offset.y == 0 && xo - xs <= 0.0f && yo - ys <= 0.0f) {
        const VkExtent2D visible{std::min({extent.width, covered.extent.width, static_cast<std::uint32_t>(std::max(0.0f, xo + xs))}), std::min({extent.height, covered.extent.height, static_cast<std::uint32_t>(std::max(0.0f, yo + ys))})};
        if (visible.width != 0 && visible.height != 0 && ColorTargetLayout(visible.width, visible.height, gcnTarget->tileMode, gcnTarget->elementBytes).Bytes() == gcnTarget->bytes) extent = visible;
    }
    const auto width = static_cast<float>(extent.width);
    const auto height = static_cast<float>(extent.height);
    const bool viewportCovers = (viewportControl == 0u && !viewportProgrammed(cx)) || (xo - xs <= 0.0f && xo + xs >= width && yo - ys <= 0.0f && yo + ys >= height);
    const bool scissorCovers = covered.offset.x == 0 && covered.offset.y == 0 && covered.extent.width >= extent.width && covered.extent.height >= extent.height;
    return viewportCovers && scissorCovers;
}

}

std::optional<DepthCopyPass> DecodeDepthCopyPass(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x000);
    if (control == cx.end() || (control->second & 0xcu) == 0) return std::nullopt;
    const bool stencil = (control->second & 8u) != 0;
    Require((control->second & ~(stencil ? 0xf88u : 0xf84u)) == 0, "DB depth copy combined with a stencil copy, clear, resummarize or decompress");
    Require((read(cx, 0x200) & 0xfu) == 0, "DB depth copy with depth or stencil tests");
    Require(stencil ? (read(cx, 0x011) & 1u) != 0 : (read(cx, 0x010) & 3u) == 3u, stencil ? "DB stencil copy without a stencil plane" : "DB depth copy from a depth plane that is not Z_32_FLOAT");
    const auto viewRegister = find(cx, 0x002);
    Require((read(cx, 0x010) & 0x000f1000u) == 0 && ((viewRegister == cx.end() ? 0u : viewRegister->second) & 0x3c000000u) == 0, "DB depth copy from a mipmapped or partially resident depth plane");
    Require(stencil ? ColorWriteMask(cx) != 0u : ColorWriteMask(cx) == 1u, "DB depth copy into anything but the red channel of color target 0");
    Require((read(cx, 0x1e0) & (1u << 30u)) == 0, "DB depth copy with color target 0 blending is unsupported");
    DepthCopyPass pass{};
    pass.source = decodeDepthPlane(cx);
    pass.stencil = stencil;
    const auto maskLow = find(cx, 0x30e);
    const auto maskHigh = find(cx, 0x30f);
    Require((maskLow == cx.end() || maskLow->second == 0xffffffffu) && (maskHigh == cx.end() || maskHigh->second == 0xffffffffu), "DB depth copy with a sample mask is unsupported");
    pass.sample = (control->second & 0x80u) != 0 ? 0u : (control->second >> 8u) & 0xfu;
    Require(pass.sample < pass.source.samples, "DB depth copy of a sample the depth plane does not have");
    pass.color = DecodeColorBuffer(cx, 0);
    Require(pass.color.format == (stencil ? VK_FORMAT_R8_UINT : VK_FORMAT_R32_SFLOAT) && pass.color.samples == 1 && pass.color.mip == 0 && pass.color.layers == 1, stencil ? "DB stencil copy into a color target that is not a single-sample R8 uint" : "DB depth copy into a color target that is not a single-sample R32 float");
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
    const auto requireCovered = [&](const ColorTarget& target) { Require(drawCovers(cx, target.extent, ColorTileModeIsGcn(target.tileMode) ? &target : nullptr), "CB metadata pass over part of a color target"); };
    if (resolve) {
        const auto source = DecodeColorBuffer(cx, 0);
        const auto destination = DecodeColorBuffer(cx, 1);
        Require(samples == 1 || source.samples == samples, "CB resolve from a target whose sample count differs from the rasterizer's");
        Require(destination.samples == 1, "CB resolve into a multisampled target is unsupported");
        Require(destination.cmaskAddress == 0, "CB resolve into a fast-clear color target is unsupported (the resolve does not update its CMASK)");
        Require(source.format == destination.format && source.extent.width == destination.extent.width && source.extent.height == destination.extent.height, "CB resolve between different formats or extents is unsupported");
        if (WordPacked(source.packing)) throw std::runtime_error("AGC graphics: CB resolves of " + std::string(PackedColorTargetName(source.packing)) + " color targets are unsupported");
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
        if (target.dccAddress != 0 || (target.cmaskAddress != 0 && pass.mode != ColorMetadataPass::Mode::EliminateFastClear)) requireCovered(target);
        pass.targets.push_back(target);
    }
    return pass;
}

std::optional<DepthExpandPass> DecodeDepthExpandPass(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto planes = outOfPlacePlanes(cx);
    if (!planes.depth && !planes.stencil) return std::nullopt;
    const auto control = read(cx, 0x000);
    const auto value = [&](std::uint32_t offset) {
        const auto found = find(cx, offset);
        return found == cx.end() ? 0u : found->second;
    };
    const auto base = [&](std::uint32_t low, std::uint32_t high) { return (static_cast<std::uint64_t>(value(high) & 0xffu) << 40u) | (static_cast<std::uint64_t>(value(low)) << 8u); };
    const auto zInfo = value(0x010);
    const auto view = value(0x002);
    const auto source = base(0x012, 0x01a);
    const auto destination = base(0x014, 0x01c);
    const auto reject = [&](const char* what) {
        char text[320];
        std::snprintf(text, sizeof(text), "AGC graphics: an out-of-place DB expand (DB_RENDER_CONTROL 0x%08x, DB_Z_INFO 0x%08x, DB_Z_READ_BASE 0x%llx, DB_Z_WRITE_BASE 0x%llx) %s is not implemented", control, zInfo, static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), what);
        throw std::runtime_error(text);
    };
    const auto copySelection = (control & 0xcu) == 0 ? 0xf80u : 0u;
    if ((control & ~(0x60u | 0x2000u | copySelection)) != 0) reject("with clear, copy, resummarize or decompress bits");
    if ((control & 0x40u) == 0) reject("without DEPTH_COMPRESS_DISABLE");
    if ((value(0x200) & 0xc000000fu) != 0) reject("with depth, stencil or depth bounds tests");
    const bool colorMasks = find(cx, 0x8e) != cx.end() && find(cx, 0x8f) != cx.end();
    if (((value(0x202) >> 4u) & 7u) != 0 && colorMasks && ColorWriteMask(cx) != 0) reject("with color writes");
    if ((zInfo & 3u) == 0u) reject("of a surface without a depth plane");
    if ((zInfo & 3u) == 2u) reject("of a Z_24 depth plane");
    if (((zInfo >> 2u) & 3u) != 0) reject("of a multisampled depth surface");
    if (((zInfo >> 4u) & 0x1fu) != 24u) reject("of a depth surface in a swizzle mode other than SW_64KB_Z_X");
    if ((zInfo & 0x000f1000u) != 0) reject("of a partially resident or mipmapped depth surface");
    if ((view & 0x3c000000u) != 0) reject("of a depth view with a mip level");
    if ((value(0x207) & LayerExports) != 0) reject("with layer exports");
    if (((source | destination | (planes.stencil ? base(0x013, 0x01b) | base(0x015, 0x01d) : 0u)) & 0xffffu) != 0) reject("between bases that are not 64 KiB aligned");
    zero(cx, 0x80, ~0u, "window offset");
    if (const auto viewportControl = read(cx, 0x206); viewportControl != 0x43fu) throw std::runtime_error(vteMessage(viewportControl));
    auto target = decodeDepthPlane(cx);
    target.clearDepth = readFloat(cx, 0x00b);
    target.clearStencil = static_cast<std::uint8_t>(read(cx, 0x00a) & 0xffu);
    if (!drawCovers(cx, target.extent)) reject("over part of the depth surface");
    const auto slice = static_cast<std::uint64_t>(target.firstSlice);
    DepthExpandPass pass{target, planes.depth ? destination + slice * DepthSliceBytes(target.extent, (zInfo & 3u) == 1u ? 2u : 4u) : 0u};
    if (planes.stencil) pass.stencilDestination = base(0x015, 0x01d) + slice * DepthSliceBytes(target.extent, 1u);
    return pass;
}

std::string DrawRejection(const QueueState& queue, bool indexed) {
    if (auto reason = DepthMaintenanceRejection(queue); !reason.empty()) return reason;
    const auto& cx = queue.context;
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
        const bool alphaDisabled = ditheredAlphaToCoverageWord(word) && alphaToCoverageDisabled(cx);
        if ((word != AlphaToCoverageUndithered && !ditheredAlphaToCoverageWord(word)) || alphaDisabled) {
            if (auto reason = nonzero(cx, 0x2dc, alphaDisabled ? AlphaToCoverageMask & ~1u : AlphaToCoverageMask, "dithered or rounded alpha-to-coverage"); !reason.empty()) return reason;
        }
        if (!alphaDisabled && word == AlphaToCoverageDithered && !ditheredAlphaSamplesSupported(cx)) return "AGC graphics: dithered alpha-to-coverage requires eight matched raster/DB_EQAA samples and one pixel iteration";
        if (!alphaDisabled && word == AlphaToCoverageTwoSampleDithered && !twoSampleDitheredAlphaSamplesSupported(cx)) return "AGC graphics: DB_ALPHA_TO_MASK 0xe701 requires two matched raster/DB_EQAA samples and one pixel iteration";
        if (!alphaDisabled && ditheredAlphaToCoverageWord(word)) {
            if (word == AlphaToCoverageDithered && !ditheredAlphaLocationsSupported(cx)) return "AGC graphics: dithered alpha-to-coverage with custom sample locations is unsupported";
            if (word == AlphaToCoverageTwoSampleDithered && !twoSampleDitheredAlphaLocationsSupported(cx)) return "AGC graphics: DB_ALPHA_TO_MASK 0xe701 is supported only with the two-sample locations (4,4),(-4,-4) or (-4,4),(4,-4) in every pixel";
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
    if (value(cx, 0x8e, targetMask) && value(cx, 0x8f, shaderMask) && value(cx, 0x202, word)) {
        const auto slots = ColorExportSlots(shaderMask);
        if (!colorControlSupported(word, ColorExportCount(cx, ColorWriteMask(cx), slots) != 0)) return colorControlMessage(word);
    }
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
}
}
}
