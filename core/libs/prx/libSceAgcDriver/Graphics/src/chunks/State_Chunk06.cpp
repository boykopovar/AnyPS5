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
std::array<ShaderRecompiler::ColorExportPacking, 8> ExportPackings(const State& state) {
    std::array<ShaderRecompiler::ColorExportPacking, 8> packings{};
    for (const auto& color : state.colors) {
        if (color.exportIndex < packings.size()) packings[color.exportIndex] = color.packing;
    }
    return packings;
}

std::uint32_t PackedWriteMask(std::uint8_t componentMapping, std::uint32_t exportedMask) {
    return PackedWriteMask(ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4, componentMapping, exportedMask);
}

std::uint32_t PackedWriteMask(ShaderRecompiler::ColorExportPacking packing, std::uint8_t componentMapping, std::uint32_t exportedMask) {
    constexpr std::array<std::uint32_t, 4> nibbles{0x000fu, 0x00f0u, 0x0f00u, 0xf000u};
    constexpr std::array<std::uint32_t, 4> floats{0x000003ffu, 0x001ffc00u, 0xffe00000u, 0u};
    Require(packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4 || packing == ShaderRecompiler::ColorExportPacking::Float11_11_10, "a write mask of a packed color target that is neither 4_4_4_4 unorm nor 11_11_10 float");
    const auto& fields = packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4 ? nibbles : floats;
    std::uint32_t mask = 0;
    for (std::uint32_t component = 0; component < 4; ++component) {
        if (((exportedMask >> ((componentMapping >> (2u * component)) & 3u)) & 1u) != 0) mask |= fields[component];
    }
    return mask;
}

std::uint32_t PackedBlendControl(std::uint32_t blendControl) {
    const auto source = blendControl & 0x1fu;
    const auto combine = (blendControl >> 5u) & 7u;
    const auto destination = (blendControl >> 8u) & 0x1fu;
    char detail[96];
    std::snprintf(detail, sizeof(detail), " (CB_BLEND_CONTROL 0x%08x)", blendControl);
    Require(combine <= 4u, std::string("blending into an 11_11_10 float color target with an unknown combine function is not implemented") + detail);
    const auto factor = [&](std::uint32_t value) {
        if (value >= 15u && value <= 18u) Require(false, std::string("dual-source blending into an 11_11_10 float color target is not implemented") + detail);
        if (value == 13u || value == 14u || value == 19u || value == 20u) Require(false, std::string("blending into an 11_11_10 float color target with a constant color factor is not implemented") + detail);
        Require(value <= 10u, std::string("blending into an 11_11_10 float color target with an unknown blend factor is not implemented") + detail);
    };
    factor(source);
    factor(destination);
    return 0x40000000u | (destination << 8u) | (combine << 5u) | source;
}

ColorTarget StagedMergeTarget(const ColorTarget& staged) {
    Require(staged.staged, "only a staged color target has a merge target");
    auto target = staged;
    target.format = staged.stagedFormat;
    target.staged = false;
    target.stagedMask = 0;
    target.stagedFormat = VK_FORMAT_UNDEFINED;
    target.partialWriteMask = staged.stagedMask;
    return target;
}

GuestTextureResource TargetMergeSurface(const ColorTarget& color) {
    auto surface = color.arrayLastSlice != 0u ? ArraySurfaceForTarget(color) : SurfaceForTarget(color);
    if (color.samples > 1u) {
        Require(surface.baseAddress == color.surfaceAddress && surface.dimension == TextureDimension::k2D && surface.mipCount == 1u, "a merged multisampled color target does not start at its single-level surface");
        surface.dimension = TextureDimension::k2DMsaa;
        surface.samples = color.samples;
    }
    surface.baseLevel = color.mip;
    surface.lastLevel = color.mip;
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

std::vector<ShaderRecompiler::TargetMerge> ExportTargetMerges(const State& state) {
    static_assert(VK_BLEND_FACTOR_ZERO == 0 && VK_BLEND_FACTOR_SRC_ALPHA_SATURATE == 14 && VK_BLEND_OP_ADD == 0 && VK_BLEND_OP_MAX == 4);
    std::vector<ShaderRecompiler::TargetMerge> merges;
    for (const auto& color : state.colors) {
        if (color.partialWriteMask == 0) continue;
        Require(color.exportIndex < 8u && color.samples <= 8u, "a merged color target needs an MRT slot below 8 and at most 8 samples");
        Require(std::ranges::none_of(state.colors, [&](const ColorTarget& other) { return &other != &color && other.partialWriteMask != 0 && other.address < color.address + color.bytes && color.address < other.address + other.bytes; }), "two merged color targets of one draw share memory");
        auto& merge = merges.emplace_back();
        Require(color.samples == 1u || state.perSampleShading, "a merged multisampled color target needs per-sample shading");
        merge.target = static_cast<std::uint8_t>(color.exportIndex);
        merge.mask = color.partialWriteMask;
        merge.samples = static_cast<std::uint8_t>(color.samples);
        merge.packing = color.packing;
        merge.blendControl = color.mergeBlend;
        merge.layered = color.arrayLastSlice != 0u;
        merge.firstSlice = merge.layered ? color.arraySlice : 0u;
        merge.lastSlice = merge.layered ? color.arraySlice + color.layers - 1u : 0u;
        merge.descriptor = EncodeTextureResource(TargetMergeSurface(color));
        merge.depthStencilWrites = DepthStencilWrites(state);
        merge.depthStencilTests = DepthStencilTests(state);
        if (color.partialWriteMask != UnclampedBlendMask) continue;
        const auto& attachment = state.blends.at(color.exportIndex);
        auto& blend = merge.blend;
        blend.enabled = true;
        blend.snorm = SignedNormalizedFormat(color.format);
        blend.colorSource = static_cast<std::uint8_t>(attachment.srcColorBlendFactor);
        blend.colorDestination = static_cast<std::uint8_t>(attachment.dstColorBlendFactor);
        blend.colorOperation = static_cast<std::uint8_t>(attachment.colorBlendOp);
        blend.alphaSource = static_cast<std::uint8_t>(attachment.srcAlphaBlendFactor);
        blend.alphaDestination = static_cast<std::uint8_t>(attachment.dstAlphaBlendFactor);
        blend.alphaOperation = static_cast<std::uint8_t>(attachment.alphaBlendOp);
        blend.writeMask = static_cast<std::uint8_t>(attachment.colorWriteMask & 0xfu);
        for (std::size_t i = 0; i < 4; ++i) blend.constants[i] = std::bit_cast<std::uint32_t>(state.blendConstants[i]);
    }
    return merges;
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
    Require((residentLinear && color.tileMode == ColorTileMode::Linear) || !ColorTileModeIsLinear(color.tileMode), "linear color targets are not resident");
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
    surface.dimension = color.volume || color.depth > 1 ? TextureDimension::k3D : TextureDimension::k2D;
    surface.format = GuestFormatFor(color.format, color.elementBytes);
    surface.dstSelX = 4;
    surface.dstSelY = 5;
    surface.dstSelZ = 6;
    surface.dstSelW = 7;
    surface.dccAddress = color.dccAddress != 0 ? color.dccAddress : color.chainDccAddress;
    surface.dccAlphaOnMsb = color.dccAlphaOnMsb;
    surface.dccPipeAligned = color.dccPipeAligned;
    if (ColorTileModeIsGcn(color.tileMode)) surface.gcnPitch = surface.width;
    return surface;
}
DecodedColorFormat DecodeColorFormat(std::uint32_t format, std::uint32_t number, std::uint32_t swap) {
    auto decoded = DecodeColorFormatRaw(format, number, swap);
    if (number == 4u && decoded.packing == ShaderRecompiler::ColorExportPacking::None) decoded.packing = ShaderRecompiler::ColorExportPacking::Uint;
    return decoded;
}


ColorTarget DecodeGcnColorBuffer(const Registers& cx, std::uint32_t slot) {
    const auto stride = slot * 0xfu;
    ColorTarget color{};
    color.slot = slot;
    const auto info = read(cx, 0x31c + stride);
    const auto number = (info >> 8u) & 7u;
    const auto swap = (info >> 11u) & 3u;
    const auto decoded = DecodeColorFormat((info >> 2u) & 0x1fu, number, swap);
    const auto attrib = read(cx, 0x31d + stride);
    color.coverageSamples = 1u << ((attrib >> 12u) & 7u);
    Require(color.coverageSamples <= 8u, "GCN color targets with more than eight samples are not implemented");
    const auto fragments = 1u << ((attrib >> 15u) & 3u);
    Require(fragments <= color.coverageSamples, "GCN color targets with more fragments than samples are not implemented");
    color.samples = fragments == 1u ? 1u : color.coverageSamples;
    if (fragments > 1u && fragments < color.coverageSamples) color.eqaaFragments = fragments;
    Require((info & 0x3u) == 0 && (info & 0x80u) == 0, "endian-swapped or linear-general GCN color targets are not implemented");
    Require((info & 0x8000u) != 0 || number == 7 || number == 4 || number == 5, "unclamped normalized color is unsupported");
    const auto view = read(cx, 0x31b + stride);
    const auto slice = view & 0x7ffu;
    Require(((view >> 13u) & 0x7ffu) == slice, "GCN color views of several array slices are not implemented");
    const auto pitch = ((read(cx, 0x319 + stride) & 0x7ffu) + 1u) * 8u;
    const auto sliceElements = ((read(cx, 0x31a + stride) & 0x3fffffu) + 1u) * 64u;
    color.extent = {pitch, sliceElements / pitch};
    color.surfaceExtent = color.extent;
    color.elementBytes = decoded.elementBytes;
    switch (attrib & 0x1fu) {
        case 8u: color.tileMode = ColorTileMode::GcnLinear; break;
        case 5u: case 13u: color.tileMode = ColorTileMode::GcnThin1D; break;
        case 9u: color.tileMode = ColorTileMode::GcnDisplay1D; break;
        case 10u: color.tileMode = ColorTileMode::GcnDisplay2D; break;
        case 14u: color.tileMode = ColorTileMode::GcnThin2D; break;
        default: throw std::runtime_error("AGC graphics: PS4 color target tile mode index " + std::to_string(attrib & 0x1fu) + " is not implemented");
    }
    const bool macroTiled = color.tileMode == ColorTileMode::GcnDisplay2D || color.tileMode == ColorTileMode::GcnThin2D;
    Require(slice == 0 || !macroTiled || color.samples == 1u, "GCN multisampled color views of a later slice of a 2D-tiled array are not implemented");
    if (macroTiled) color.pipeBankXor = GcnLayerBankXor(GcnTiling(ColorTextureTileMode(color.tileMode), color.elementBytes), slice);
    color.mipCount = 1;
    const ColorTargetLayout layout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes, color.pipeBankXor);
    Require(layout.Bytes() == static_cast<std::size_t>(pitch) * color.extent.height * color.elementBytes, "PS4 color target pitch or slice is not padded the way its tile mode pads them");
    color.surfaceAddress = (static_cast<std::uint64_t>(read(cx, 0x318 + stride)) << 8u) + static_cast<std::uint64_t>(slice) * layout.Bytes() * fragments;
    color.address = color.surfaceAddress;
    color.bytes = layout.Bytes();
    color.format = decoded.format;
    color.componentMapping = decoded.componentMapping;
    color.packing = decoded.packing;
    for (std::uint32_t word = 0; word < 2; ++word) {
        const auto clear = find(cx, 0x323 + word + stride);
        color.clearWords[word] = clear == cx.end() ? 0u : clear->second;
    }
    Require(color.samples > 1u || ((info >> 13u) & 1u) == 0, "PS4 color target fast clear (CMASK) is not implemented");
    if (color.samples > 1u && (info & 0x6000u) != 0) {
        color.cmaskAddress = static_cast<std::uint64_t>(read(cx, 0x31f + stride)) << 8u;
        Require(color.cmaskAddress != 0, "a multisampled GCN color target with fast clear or FMASK compression has no CMASK address");
        color.cmaskBytes = (static_cast<std::size_t>(read(cx, 0x320 + stride) & 0x3fffu) + 1u) * 128u;
        color.cmaskAddress += static_cast<std::uint64_t>(slice) * color.cmaskBytes;
        color.cmaskFastClear = (info & 0x2000u) != 0;
    }
    if (fragments < color.coverageSamples && (info & 0x4000u) != 0) {
        color.fmaskAddress = static_cast<std::uint64_t>(read(cx, 0x321 + stride)) << 8u;
        Require(color.fmaskAddress != 0, "a compressed GCN color target with fewer fragments than samples has no FMASK address");
        const auto fmaskBitsPerPixel = fragments * (fragments == 1u ? 8u : color.coverageSamples);
        color.gcnFmaskBytes = ((read(cx, 0x322 + stride) & 0x3fffffu) + 1u) * 8u * fmaskBitsPerPixel;
        color.fmaskAddress += static_cast<std::uint64_t>(slice) * color.gcnFmaskBytes;
    }
    Require(color.eqaaFragments == 0 || color.fmaskAddress != 0, "GCN color targets with fewer fragments than samples (EQAA) are supported only with FMASK (CB_COLOR_INFO COMPRESSION and a CB_COLOR_FMASK base)");
    GuestMemory::CheckRange(reinterpret_cast<const void*>(color.address), color.bytes, layout.Alignment(), true);
    return color;
}

GuestTextureResource ArraySurfaceForTarget(const ColorTarget& color) {
    Require(color.arrayLastSlice != 0, "the color target is not a view of a 2D array");
    auto surface = SurfaceForTarget(color);
    surface.baseAddress = color.arrayAddress;
    surface.width = color.surfaceExtent.width;
    surface.height = color.surfaceExtent.height;
    surface.depthOrLastArray = color.arrayLastSlice;
    surface.dimension = TextureDimension::k2DArray;
    return surface;
}

ColorTarget DecodeColorBuffer(const Registers& cx, std::uint32_t slot, bool allowMultisampleDcc) {
    if (ShaderRecompiler::GcnGuestShaders()) return DecodeGcnColorBuffer(cx, slot);
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
    const auto fragments = 1u << ((attrib >> 15u) & 3u);
    Require(fragments <= color.coverageSamples, "color targets with more fragments than samples are unsupported");
    color.samples = fragments == 1u ? 1u : color.coverageSamples;
    if (fragments > 1u && fragments < color.coverageSamples) color.eqaaFragments = fragments;
    const auto multisampleInfo = color.coverageSamples > 1 ? 0x0c006000u : 0u;
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
    color.volume = volume;
    Require(((attrib3 >> 24u) & 3u) != 0u || (attrib2 & 0x3fffu) == 0u, "1D color targets taller than one row are unsupported");
    if (volume) {
        Require(maxMip == 0 || (info & 0x10000000u) == 0, "DCC of mipmapped 3D color targets is unsupported");
        Require(maxMip == 0 || (info & 0x2000u) == 0, "CMASK fast clears of mipmapped 3D color targets are unsupported");
        color.depth = (attrib3 & 0x1fffu) + 1u;
        const auto viewDepth = std::max(color.depth >> viewMip, 1u);
        Require(slice < viewDepth && lastSlice >= slice, "the color view slices are outside the 3D surface");
        color.depthSlice = slice;
        color.layers = std::min(lastSlice + 1u, viewDepth) - slice;
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
        const auto mips = [&] {
            if (!volume) return ComputeElementMipLayout(ColorTextureTileMode(color.tileMode), color.elementBytes, color.extent.width, color.extent.height, maxMip + 1u);
            GuestTextureResource surface{};
            surface.width = color.extent.width;
            surface.height = color.extent.height;
            surface.depthOrLastArray = color.depth - 1u;
            surface.mipCount = maxMip + 1u;
            surface.tileMode = ColorTextureTileMode(color.tileMode);
            surface.dimension = TextureDimension::k3D;
            surface.format = GuestFormatFor(decoded.format, color.elementBytes);
            return DescribeSurface(surface).mips;
        }();
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
    if (WordPacked(color.packing) && (info & 0x10000000u) != 0) throw std::runtime_error("AGC graphics: DCC-compressed " + std::string(PackedColorTargetName(color.packing)) + " color targets are unsupported");
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
    Require(color.eqaaFragments == 0 || ((info & 0x4000u) != 0 && color.fmaskAddress != 0 && (info & 0x10000000u) == 0), "color targets with fewer fragments than samples (EQAA) are supported only with FMASK (CB_COLOR_INFO COMPRESSION and an FMASK base) and without DCC");
    if (color.coverageSamples > color.samples && (info & 0x4000u) != 0) {
        const auto fmaskHigh = find(cx, 0x3a0 + slot);
        color.fmaskAddress = (((fmaskHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(fmaskHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x321 + stride)) << 8u)) & ~0xffffull;
        Require(color.fmaskAddress != 0, "a compressed one-fragment color target without an FMASK address is unsupported");
    }
    if ((info & 0x10000000u) != 0) {
        const auto dccHigh = find(cx, 0x3a8 + slot);
        const auto dccBase = ((dccHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(dccHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x325 + stride)) << 8u);
        if (maxMip == 0) {
            color.dccAddress = dccBase;
            color.dccAlphaOnMsb = DccAlphaOnMsb(color.format, swap);
            color.dccPipeAligned = ((attrib3 >> 30u) & 1u) != 0;
            if (color.samples > 1) {
                if (!allowMultisampleDcc) Require(color.tileMode == ColorTileMode::RenderTarget && color.dccAddress != 0 && color.dccPipeAligned && !volume && color.arrayLastSlice == 0 && slice == 0, "multisampled DCC targets require pipe-aligned single-layer SW_64KB_R_X");
                color.dccControl = read(cx, 0x31e + stride);
                if (!allowMultisampleDcc) Require((color.dccControl & 0x3ff80u) == 0, "multisampled DCC color transforms or lossy modes are not implemented");
            }
        } else if (!volume && color.arrayLastSlice == 0 && slice == 0) {
            color.chainDccAddress = dccBase;
            color.dccAlphaOnMsb = DccAlphaOnMsb(color.format, swap);
            color.dccPipeAligned = ((attrib3 >> 30u) & 1u) != 0;
        } else {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[gpu] DCC keys of mipmapped array and 3D color targets are ignored\n");
            }
        }
    }
    if (volume && color.tileMode != ColorTileMode::Linear) {
        color.bytes = SurfaceGuestBytes(SurfaceForTarget(color));
        GuestMemory::CheckRange(reinterpret_cast<const void*>(color.surfaceAddress), color.bytes, colorLayout.Alignment(), true);
    }
    return color;
}

namespace {

}
}
}
}
