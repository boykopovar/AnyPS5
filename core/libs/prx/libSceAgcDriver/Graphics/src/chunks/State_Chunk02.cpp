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
DepthTarget decodeDepthPlane(const Registers& cx) {
    const auto viewRegister = find(cx, 0x002);
    const auto view = viewRegister == cx.end() ? 0u : viewRegister->second;
    const auto maxMip = (read(cx, 0x010) >> 16u) & 0xfu;
    const auto viewMip = maxMip == 0 ? 0u : (view >> 26u) & 0xfu;
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
    const bool gcn = ShaderRecompiler::GcnGuestShaders();
    depth.gcn = gcn;
    depth.depthSwizzle = gcn ? 0u : (read(cx, 0x010) >> 4u) & 31u;
    depth.iterate256 = !gcn && (read(cx, 0x010) & (1u << 20u)) != 0;
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
    if (gcn) {
        const auto size = read(cx, 0x016);
        depth.extent = {((size & 0x7ffu) + 1u) * 8u, (((size >> 11u) & 0x7ffu) + 1u) * 8u};
    } else {
        const auto size = read(cx, 0x007);
        depth.extent = {(size & 0x3fffu) + 1u, ((size >> 16u) & 0x3fffu) + 1u};
    }
    Require(!gcn || (maxMip == 0 && viewMip == 0), "PS4 depth targets with mip levels are not implemented");
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
    depth.depthPartiallyResident = zFormat != 0 && (read(cx, 0x010) & 0x1000u) != 0;
    depth.stencilPartiallyResident = stencil && (stencilInfo & 0x1000u) != 0;
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

std::string nonTriangleRejection(const QueueState& queue, const char* message, bool rectangles = false, bool lines = false) {
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
    } else if (const auto primitive = word(queue.userConfig, 0x242, RegisterBank::UserConfig); primitive && *primitive != 4u && *primitive != 5u && *primitive != 6u && *primitive != 21u && !(rectangles && (*primitive == 7u || *primitive == 17u)) && !(lines && (*primitive == 2u || *primitive == 3u))) {
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
    if (const auto alphaToMask = word(cx, 0x2dc); alphaToMask && (*alphaToMask & 1u) != 0 && !(ditheredAlphaToCoverageWord(*alphaToMask) && alphaToCoverageDisabled(cx))) return zeroMessage(0x2dc, *alphaToMask, "MSAA_ENABLE on a single-sample target with alpha-to-coverage");
    if (const auto conservative = word(cx, 0x313); conservative && (*conservative & ~0x001f6000u) != 0) return zeroMessage(0x313, *conservative, "MSAA_ENABLE on a single-sample target with conservative rasterization");
    return nonTriangleRejection(queue, "MSAA_ENABLE on a single-sample target for primitives other than triangles, lines and rectangles", true, true);
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

}
}
