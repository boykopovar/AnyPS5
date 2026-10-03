#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace AgcDriver::Graphics {
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
    Require(std::isfinite(value), "non-finite register at DWORD " + std::to_string(offset));
    return value;
}

std::string zeroMessage(std::uint32_t offset, std::uint32_t value, const char* name) {
    char detail[64];
    std::snprintf(detail, sizeof(detail), " (register 0x%x = 0x%08x)", offset, value);
    return std::string(name) + " is unsupported" + detail;
}

void zero(const Registers& registers, std::uint32_t offset, std::uint32_t mask, const char* name, RegisterBank bank = RegisterBank::Context) {
    const auto value = read(registers, offset, bank);
    if ((value & mask) != 0) throw std::runtime_error(zeroMessage(offset, value, name));
}

std::string vteMessage(std::uint32_t viewportControl) {
    std::ostringstream message;
    message << "AGC graphics: PA_CL_VTE_CNTL=0x" << std::hex << viewportControl << ": expected 0x43f for homogeneous positions and all viewport transforms; pre-divided coordinates, reciprocal W or disabled transforms are unsupported";
    return message.str();
}

// The register rules DecodeState and DrawRejection share (the precheck must reject exactly what
// DecodeState would): the masks whose set bits are unsupported, and the depth-control verdict.
// Render target index, viewport index and the misc export vector that carries them are accepted but
// not routed: color targets are single-layer, so layered draws land in layer 0.
constexpr std::uint32_t LayerExports = (1u << 18u) | (1u << 19u) | (1u << 21u) | (1u << 24u);
constexpr std::uint32_t DepthControlMask = ~0x007007f0u;
// EXEC_ON_HIER_FAIL / EXEC_ON_NOOP / EXEC_IF_OVERLAPPED (bits 9, 10, 17) only force the pixel shader
// to run, which it always does here.
constexpr std::uint32_t ShaderControlMask = ~(0x00009870u | 0x00020600u);
constexpr std::uint32_t AlphaToCoverageMask = ~0x0001ff00u;
constexpr std::uint32_t ScanModeMask = ~2u;
constexpr std::uint32_t ScanControlMask = ~0x06003fffu;
constexpr std::uint32_t ScreenOffsetMask = ~0x01ff01ffu;
// Bits 26/27 (ZCLIP_NEAR/FAR_DISABLE) become depth clamping; bit 19 selects the [0, 1] clip space.
constexpr std::uint32_t ClipControlMask = ~(0x80000u | 0x0c000000u);

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

VkStencilOpState stencilFace(std::uint32_t compare, std::uint32_t ops, std::uint32_t refMask, bool readOnly) {
    VkStencilOpState face{};
    face.compareOp = static_cast<VkCompareOp>(compare);
    face.compareMask = (refMask >> 8u) & 0xffu;
    face.writeMask = readOnly ? 0u : (refMask >> 16u) & 0xffu;
    const auto test = refMask & 0xffu;
    const auto opValue = refMask >> 24u;
    auto reference = test;
    auto fixed = compare == 0 || compare == 7 ? 0u : face.compareMask;
    const auto convert = [&](std::uint32_t op) {
        if (face.writeMask == 0) return VK_STENCIL_OP_KEEP;
        char detail[96];
        switch (op) {
            case 0: return VK_STENCIL_OP_KEEP;
            case 1: return VK_STENCIL_OP_ZERO;
            case 2:
            case 3:
            case 4: {
                const auto value = op == 2 ? 0xffu : op == 3 ? test : opValue;
                if (((reference ^ value) & fixed & face.writeMask) != 0) {
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
    face.failOp = convert(ops & 0xfu);
    face.passOp = convert((ops >> 4u) & 0xfu);
    face.depthFailOp = convert((ops >> 8u) & 0xfu);
    face.reference = reference;
    return face;
}

void decodeDepth(const Registers& cx, std::uint32_t depthControl, State& result) {
    zero(cx, 0x000, 0x00001f9fu, "depth/stencil clear, copy, resummarize or decompress draws (DB_RENDER_CONTROL)");
    const auto view = read(cx, 0x002);
    zero(cx, 0x002, ~0x03000000u, "depth array slices or mips (DB_DEPTH_VIEW)");
    zero(cx, 0x010, 0x000f100cu, "multisampled, partially resident or mipmapped depth (DB_Z_INFO)");
    zero(cx, 0x011, 0x00001000u, "partially resident stencil (DB_STENCIL_INFO)");
    const auto zFormat = read(cx, 0x010) & 3u;
    const bool stencil = (read(cx, 0x011) & 1u) != 0;
    Require(zFormat != 2, "Z_24 depth is unsupported");
    Require(zFormat != 0 || (depthControl & 2u) == 0, "depth test without a depth plane");
    Require(stencil || (depthControl & 1u) == 0, "stencil test without a stencil plane");
    const auto base = [&](std::uint32_t low, std::uint32_t highOffset) {
        const auto high = find(cx, highOffset);
        return (high == cx.end() ? 0ull : static_cast<std::uint64_t>(high->second & 0xffu) << 40u) | (static_cast<std::uint64_t>(read(cx, low)) << 8u);
    };
    const bool depthReadOnly = (view & 0x01000000u) != 0;
    const bool stencilReadOnly = (view & 0x02000000u) != 0;
    DepthTarget depth{};
    depth.address = zFormat != 0 ? base(0x012, 0x01a) : 0;
    depth.stencilAddress = stencil ? base(0x013, 0x01b) : 0;
    Require(zFormat == 0 || depthReadOnly || base(0x014, 0x01c) == depth.address, "depth read and written at different addresses is unsupported");
    Require(!stencil || stencilReadOnly || base(0x015, 0x01d) == depth.stencilAddress, "stencil read and written at different addresses is unsupported");
    const auto size = read(cx, 0x007);
    depth.extent = {(size & 0x3fffu) + 1u, ((size >> 16u) & 0x3fffu) + 1u};
    depth.format = zFormat == 1 ? (stencil ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D16_UNORM) : (stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT);
    depth.clearDepth = readFloat(cx, 0x00b);
    depth.clearStencil = static_cast<std::uint8_t>(read(cx, 0x00a) & 0xffu);
    if (const auto htile = find(cx, 0x005); htile != cx.end() && htile->second != 0) depth.htileAddress = base(0x005, 0x01e);
    result.depth = depth;
    result.depthTest = (depthControl & 2u) != 0;
    result.depthWrite = result.depthTest && (depthControl & 4u) != 0 && !depthReadOnly;
    result.depthCompare = static_cast<VkCompareOp>((depthControl >> 4u) & 7u);
    result.stencilTest = (depthControl & 1u) != 0;
    if (result.stencilTest) {
        const auto ops = read(cx, 0x10b);
        result.stencilFront = stencilFace((depthControl >> 8u) & 7u, ops, read(cx, 0x10c), stencilReadOnly);
        result.stencilBack = (depthControl & 0x80u) != 0 ? stencilFace((depthControl >> 20u) & 7u, ops >> 12u, read(cx, 0x10d), stencilReadOnly) : result.stencilFront;
    }
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

bool colorControlSupported(std::uint32_t colorControl, bool hasColorTarget) {
    return colorControl == 0xcc0010u || (!hasColorTarget && (colorControl & ~0x70u) == 0xcc0000u);
}

std::string colorControlMessage(std::uint32_t colorControl) {
    static constexpr const char* modes[8] = {"disable", "normal", "eliminate fast clear", "resolve", "decompress", "FMASK decompress", "DCC decompress", "reserved"};
    char text[160];
    std::snprintf(text, sizeof(text), "AGC graphics: only normal color rendering with copy ROP is supported (CB_COLOR_CONTROL 0x%08x, mode %s)", colorControl, modes[(colorControl >> 4u) & 7u]);
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
        case 19: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case 20: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        default: throw std::runtime_error("AGC graphics: unsupported blend factor " + std::to_string(value));
    }
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

struct DecodedColorFormat {
    VkFormat format;
    std::uint32_t elementBytes;
    std::uint8_t componentMapping = 0xe4u;
};

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
    if (swap > 1 && format != 1 && format != 2 && format != 4) return fail();
    switch (format) {
        case 1:
            if (number == unorm) return single(VK_FORMAT_R8_UNORM, 1);
            if (number == snorm) return single(VK_FORMAT_R8_SNORM, 1);
            if (number == uint) return single(VK_FORMAT_R8_UINT, 1);
            return fail();
        case 2:
            if (number == unorm) return single(VK_FORMAT_R16_UNORM, 2);
            if (number == snorm) return single(VK_FORMAT_R16_SNORM, 2);
            if (number == floating) return single(VK_FORMAT_R16_SFLOAT, 2);
            if (number == uint) return single(VK_FORMAT_R16_UINT, 2);
            return fail();
        case 3:
            if (swap != 0) return fail();
            if (number == unorm) return {VK_FORMAT_R8G8_UNORM, 2};
            if (number == snorm) return {VK_FORMAT_R8G8_SNORM, 2};
            return fail();
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
            if (swap != 0 || number != floating) return fail();
            return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4};
        case 9:
            // COLOR_2_10_10_10 keeps red in the low bits, the Vulkan A2B10G10R10 packing.
            if (number != unorm) return fail();
            return {alternate ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4};
        case 10:
            if (number == unorm) return {alternate ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, 4};
            if (number == snorm) return {alternate ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM, 4};
            if (number == srgb) return {alternate ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, 4};
            return fail();
        case 11:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32_SFLOAT, 8};
            if (number == uint) return {VK_FORMAT_R32G32_UINT, 8};
            return fail();
        case 12:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R16G16B16A16_SFLOAT, 8};
            if (number == unorm) return {VK_FORMAT_R16G16B16A16_UNORM, 8};
            if (number == snorm) return {VK_FORMAT_R16G16B16A16_SNORM, 8};
            if (number == uint) return {VK_FORMAT_R16G16B16A16_UINT, 8};
            return fail();
        case 14:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32B32A32_SFLOAT, 16};
            if (number == uint) return {VK_FORMAT_R32G32B32A32_UINT, 16};
            return fail();
        default:
            return fail();
    }
}

void intersect(VkRect2D& result, const Registers& registers, std::uint32_t offset, bool screen) {
    const auto tl = read(registers, offset);
    const auto br = read(registers, offset + 1);
    if (!screen) Require((tl & 0x80008000u) == 0x80000000u && (br & 0x80008000u) == 0, "scissor window offsets or reserved bits are unsupported");
    const auto x = tl & 0xffffu;
    const auto y = (tl >> 16u) & (screen ? 0xffffu : 0x7fffu);
    const auto right = br & 0xffffu;
    const auto bottom = br >> 16u;
    Require(x <= right && y <= bottom, "inverted scissor rectangle");
    const auto oldRight = static_cast<std::uint32_t>(result.offset.x) + result.extent.width;
    const auto oldBottom = static_cast<std::uint32_t>(result.offset.y) + result.extent.height;
    const auto left = std::max(static_cast<std::uint32_t>(result.offset.x), x);
    const auto top = std::max(static_cast<std::uint32_t>(result.offset.y), y);
    result.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    result.extent = {std::min(oldRight, right) > left ? std::min(oldRight, right) - left : 0, std::min(oldBottom, bottom) > top ? std::min(oldBottom, bottom) - top : 0};
}

}

ShaderStages DecodeShaderStages(const QueueState& queue) {
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
    const bool geometry = (value & 0x20u) != 0;
    validate(tessellation == ((value & 4u) != 0), "Patch topology and HS_EN disagree");
    validate(!tessellation || !geometry, "combined tessellation and geometry is unsupported by the reference path");
    const auto path = tessellation ? ShaderPath::Tessellation : geometry ? ShaderPath::Geometry : ShaderPath::Vertex;
    ShaderStages result{path, value, (value & 0x00400000u) != 0 ? 32u : 64u, (read(queue.context, 0x1b6) & 0x8000u) != 0 ? 32u : 64u, {}, {}};
    APS5_LOG_OUT_DEBUG("DecodeShaderStages value=0x%x primitive=%u path=%u vertexWave=%u", value, primitive, static_cast<unsigned>(result.path), result.vertexWaveSize);
    if (path == ShaderPath::Vertex) {
        validate((value & 0x2000u) != 0, "legacy vertex routing without PRIMGEN_EN is unsupported");
        validate((value & ~0x02402010u) == 0, "unsupported vertex routing, scheduling or wave-ID state");
    } else if (path == ShaderPath::Tessellation) {
        validate((value & 0x00600020u) == 0, "wave32 tessellation or geometry amplification is unsupported");
        validate((value & ~0x0007ed0du) == 0 && (value & 3u) == 1u && ((value >> 3u) & 3u) == 1u, "unsupported tessellation routing");
        const auto config = read(queue.context, 0x2d6);
        const auto parameters = read(queue.context, 0x2db);
        ShaderRecompiler::TessellationConfiguration tess{(config >> 8u) & 0x3fu, (config >> 14u) & 0x3fu, parameters & 3u, (parameters >> 2u) & 3u, (parameters >> 5u) & 3u};
        validate(tess.inputControlPoints != 0 && tess.inputControlPoints <= 32 && tess.outputControlPoints != 0 && tess.outputControlPoints <= 32, "invalid tessellation control-point counts");
        validate(tess.domain == 1 && tess.partitioning == 2 && tess.outputTopology == 2, "only triangular, fractional-odd, clockwise tessellation is supported by the reference path");
        result.tessellation = tess;
    } else {
        validate((value & ~0x0047ec30u) == 0, "unsupported geometry routing, fast launch or wave-ID state");
        const auto group = read(queue.userConfig, 0x25b, RegisterBank::UserConfig);
        const auto vertices = (group >> 9u) & 0x1ffu;
        const auto primitives = group & 0x1ffu;
        const auto maxVertices = read(queue.context, 0x1ff);
        const auto verticesPerPrimitive = read(queue.context, 0x2ce);
        validate((primitive == 1 || primitive == 2 || primitive == 4 || primitive == 6) && read(queue.context, 0x29b) == 2 && verticesPerPrimitive >= 3, "unsupported geometry input or output assembly");
        const auto inputSize = primitive == 1 ? 1u : primitive == 2 ? 2u : 3u;
        validate(vertices >= inputSize && maxVertices != 0 && maxVertices <= 256 && verticesPerPrimitive <= 256, "invalid geometry subgroup output");
        const auto inputStep = primitive == 6 ? 1u : inputSize;
        const auto groupPrimitives = std::min({primitives, (vertices - inputSize) / inputStep + 1u, maxVertices / verticesPerPrimitive});
        validate(groupPrimitives != 0, "geometry subgroup contains no primitives");
        const auto resources = read(queue.shader, 0x8b, RegisterBank::Shader);
        validate(((read(queue.shader, 0x8a, RegisterBank::Shader) >> 29u) & 3u) == 3 && ((resources >> 16u) & 3u) == 3, "unsupported geometry VGPR allocation");
        const auto esgsItemSize = read(queue.context, 0x2ab);
        validate(esgsItemSize != 0 && esgsItemSize * vertices <= 0xffffu, "invalid VGT_ESGS_RING_ITEMSIZE");
        const auto threads = std::max({(groupPrimitives - 1u) * inputStep + inputSize, primitives, maxVertices, primitives * (verticesPerPrimitive - 2u)});
        result.mesh = ShaderRecompiler::MeshConfiguration{primitive, groupPrimitives, (groupPrimitives - 1u) * inputStep + inputSize, maxVertices, primitives * (verticesPerPrimitive - 2u), ((threads + result.vertexWaveSize - 1u) / result.vertexWaveSize) * result.vertexWaveSize, ((resources >> 19u) & 0xffu) * 128u, 0, esgsItemSize};
    }
    return result;
}

State DecodeState(const QueueState& queue) {
    const auto& cx = queue.context;
    State result{};
    result.stages = DecodeShaderStages(queue);
    const auto primitive = read(queue.userConfig, 0x242, RegisterBank::UserConfig);
    APS5_LOG_OUT_DEBUG("DecodeState primitive=%u path=%u vertexWave=%u", primitive, static_cast<unsigned>(result.stages.path), result.stages.vertexWaveSize);
    switch (primitive) {
        case 1: Require(result.stages.mesh.has_value(), "point-list vertex rendering requires point-size output support"); result.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
        case 2: result.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case 7:
        case 17:
            Require(result.stages.path == ShaderPath::Vertex, "rect-list requires vertex routing");
            result.rectList = true;
            result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            break;
        case 9: result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
        case 4: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case 5: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
        case 6: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        default: throw std::runtime_error("AGC graphics: unsupported primitive type " + std::to_string(primitive));
    }
    APS5_LOG_OUT_DEBUG("Topology=%u", static_cast<unsigned>(result.topology));
    result.primitiveRestart = read(queue.userConfig, 0x24b, RegisterBank::UserConfig) != 0 && !result.rectList && result.topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    if ((read(cx, 0x207) & LayerExports) != 0) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[gpu] layer/viewport index vertex exports are ignored (PA_CL_VS_OUT_CNTL=0x%08x)\n", read(cx, 0x207));
        }
    }
    zero(cx, 0x207, ~LayerExports, "clip distances, layer, viewport or auxiliary vertex exports");
    {
        const auto depthControl = read(cx, 0x200);
        if ((depthControl & 3u) != 0 && depthSurfaceBound(cx)) {
            decodeDepth(cx, depthControl, result);
        } else if (depthPassThrough(depthControl)) {
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
        Require((depthControl & 0xc0000008u) == 0, "depth bounds or depth-conditional color writes are unsupported");
    }
    zero(cx, 0x203, ShaderControlMask, "depth export, shader coverage or ordered fragment execution");
    zero(cx, 0x2dc, AlphaToCoverageMask, "alpha-to-coverage");
    zero(cx, 0x2f8, ~0u, "multisampling or coverage conversion");
    zero(cx, 0x292, ScanModeMask, "scan conversion mode");
    zero(cx, 0x293, ScanControlMask, "sample iteration, primitive discard or out-of-order rasterization");
    zero(cx, 0x80, ~0u, "window offset");
    zero(cx, 0x8d, ScreenOffsetMask, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits");
    Require(read(cx, 0x83) == 0xffffu, "clip rectangles are unsupported");
    Require((read(cx, 0x8c) & 0xfu) == 0xau, "nonstandard triangle edge rules are unsupported");
    Require(read(cx, 0x2f9) == 0x2du, "nonstandard pixel center or vertex quantization is unsupported");
    Require(read(cx, 0x313) == 0x6000u, "conservative rasterization is unsupported");
    Require(read(cx, 0x30e) == 0xffffffffu && read(cx, 0x30f) == 0xffffffffu, "sample masks are unsupported");
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu) throw std::runtime_error(vteMessage(viewportControl));
    zero(cx, 0x204, ClipControlMask, "unsupported PA_CL_CLIP_CNTL flags");
    result.depthClamp = (read(cx, 0x204) & 0x0c000000u) != 0;
    result.negativeOneToOne = (read(cx, 0x204) & 0x80000u) == 0;
    const auto raster = read(cx, 0x205);
    // Bits 5-10 give the front/back polygon type (2 = filled triangles), which POLY_MODE (bit 3) turns on
    // explicitly; KEEP_TOGETHER_ENABLE (bit 24) only affects primitive distribution across the chip.
    const auto rasterMode = raster & ~0x7u & ~(1u << 24u);
    Require(rasterMode == 0 || rasterMode == 0x240u || rasterMode == 0x248u, "polygon mode, depth bias, provoking vertex or nonstandard rasterization is unsupported");
    result.cullMode = ((raster & 1u) != 0 ? VK_CULL_MODE_FRONT_BIT : 0u) | ((raster & 2u) != 0 ? VK_CULL_MODE_BACK_BIT : 0u);
    if (result.rectList) result.cullMode = VK_CULL_MODE_NONE;
    result.frontFace = (raster & 4u) != 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    APS5_LOG_OUT_DEBUG("Raster=0x%x cullMode=0x%x frontFace=%u negativeOneToOne=%u", raster, static_cast<unsigned>(result.cullMode), static_cast<unsigned>(result.frontFace), result.negativeOneToOne ? 1u : 0u);
    const auto shaderMask = read(cx, 0x8f);
    // Channels of targets the pixel shader does not export are never written, so the target mask only
    // matters where the shader exports.
    const auto targetMask = read(cx, 0x8e) & shaderMask;
    APS5_LOG_OUT_DEBUG("CB_TARGET_MASK=0x%x CB_SHADER_MASK=0x%x", targetMask, shaderMask);
    // MRT slots 0..n-1 become attachments 0..n-1; a slot between written slots must be written too.
    std::uint32_t slotCount = 0;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        if (((targetMask >> (4u * slot)) & 0xfu) != 0) slotCount = slot + 1;
    }
    for (std::uint32_t slot = 0; slot < slotCount; ++slot) {
        if (((targetMask >> (4u * slot)) & 0xfu) == 0) {
            std::ostringstream message;
            message << "AGC graphics: color targets with gaps are unsupported: CB_TARGET_MASK=0x" << std::hex << targetMask << ", CB_SHADER_MASK=0x" << shaderMask;
            throw std::runtime_error(message.str());
        }
    }
    result.hasColorTarget = slotCount != 0;
    APS5_LOG_OUT_DEBUG("hasColorTarget=%u slots=%u", result.hasColorTarget ? 1u : 0u, slotCount);

    // CB_COLOR_CONTROL mode 0 disables color writes, which only matters when a target is written.
    if (const auto colorControl = read(cx, 0x202); !colorControlSupported(colorControl, result.hasColorTarget)) throw std::runtime_error(colorControlMessage(colorControl));
    zero(cx, 0x1c4, ~0u, "depth or sample-mask export");
    const auto exportFormat = read(cx, 0x1c5);
    APS5_LOG_OUT_DEBUG("Export format=%u", exportFormat);
    // SPI_SHADER_POS_FORMAT: POS0 must be a 4-component position; later vectors carry the misc/clip
    // exports that PA_CL_VS_OUT_CNTL validation above already limits to ignored layer/viewport data.
    Require((read(cx, 0x1c3) & 0xfu) == 4, "additional position exports are unsupported");
    for (std::uint32_t slot = 0; slot < slotCount; ++slot) {
        // Export formats only matter for the targets the draw writes.
        const auto slotExport = (exportFormat >> (4u * slot)) & 0xfu;
        if (slotExport == 0 || slotExport == 7 || slotExport == 8 || slotExport > 9) throw std::runtime_error("AGC graphics: color export format " + std::to_string(slotExport) + " is unsupported");
        const auto color = DecodeColorBuffer(cx, slot);
        APS5_LOG_OUT_DEBUG("Color %u address=0x%llx extent=%ux%u bytes=%llu VkFormat=%u", slot, static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height, static_cast<unsigned long long>(color.bytes), static_cast<unsigned>(color.format));
        if (slot == 0) {
            result.renderExtent = color.extent;
        } else {
            result.renderExtent = {std::min(result.renderExtent.width, color.extent.width), std::min(result.renderExtent.height, color.extent.height)};
        }
        result.colors.push_back(color);
    }
    if (result.hasColorTarget) {
        result.color = result.colors.front();
        if (result.depth) result.renderExtent = {std::min(result.renderExtent.width, result.depth->extent.width), std::min(result.renderExtent.height, result.depth->extent.height)};
    } else if (result.depth) {
        result.renderExtent = result.depth->extent;
    } else {
        const auto screenBottomRight = read(cx, 0xd);
        APS5_LOG_OUT_DEBUG("No color target, screen BR register=0x%x", screenBottomRight);
        result.renderExtent = {screenBottomRight & 0xffffu, screenBottomRight >> 16u};
        APS5_LOG_OUT_DEBUG("Render extent from screen=%ux%u", result.renderExtent.width, result.renderExtent.height);
        Require(result.renderExtent.width != 0 && result.renderExtent.height != 0, "empty framebuffer extent for a draw without color writes");
    }
    const auto xs = readFloat(cx, 0x10f);
    const auto xo = readFloat(cx, 0x110);
    const auto ys = readFloat(cx, 0x111);
    const auto yo = readFloat(cx, 0x112);
    const auto zs = readFloat(cx, 0x113);
    const auto zo = readFloat(cx, 0x114);
    const auto minDepth = result.negativeOneToOne ? zo - zs : zo;
    const auto maxDepth = zo + zs;
    APS5_LOG_OUT_DEBUG("Viewport transform scale=(%f,%f,%f) offset=(%f,%f,%f) depth=(%f,%f)", xs, ys, zs, xo, yo, zo, minDepth, maxDepth);
    if (!(xs > 0 && ys != 0 && std::isfinite(minDepth) && std::isfinite(maxDepth))) {
        std::ostringstream message;
        message << "AGC graphics: unsupported viewport transform: scale=(" << xs << ", " << ys << ", " << zs << "), offset=(" << xo << ", " << yo << ", " << zo << "), depth=(" << minDepth << ", " << maxDepth << "), negativeOneToOne=" << result.negativeOneToOne;
        throw std::runtime_error(message.str());
    }
    Require(readFloat(cx, 0xb4) <= readFloat(cx, 0xb5), "inverted viewport depth clamp bounds");
    result.viewport = {xo - xs, yo - ys, 2 * xs, 2 * ys, minDepth, maxDepth};
    APS5_LOG_OUT_DEBUG("Viewport x=%f y=%f w=%f h=%f minDepth=%f maxDepth=%f", result.viewport.x, result.viewport.y, result.viewport.width, result.viewport.height, result.viewport.minDepth, result.viewport.maxDepth);
    result.scissor = {{0, 0}, result.renderExtent};
    intersect(result.scissor, cx, 0xc, true);
    intersect(result.scissor, cx, 0x81, false);
    intersect(result.scissor, cx, 0x90, false);
    if ((read(cx, 0x292) & 2u) != 0) intersect(result.scissor, cx, 0x94, false);
    APS5_LOG_OUT_DEBUG("Scissor offset=(%d,%d) extent=%ux%u", result.scissor.offset.x, result.scissor.offset.y, result.scissor.extent.width, result.scissor.extent.height);
    for (std::uint32_t slot = 0; slot < result.colors.size(); ++slot) {
        const auto blend = read(cx, 0x1e0 + slot);
        APS5_LOG_OUT_DEBUG("Blend %u control=0x%x", slot, blend);
        Require((blend & 0x0000e000u) == 0, "reserved blend control bits");
        VkPipelineColorBlendAttachmentState state{};
        const auto mapping = result.colors[slot].componentMapping;
        const auto exportedMask = (targetMask >> (4u * slot)) & 0xfu;
        for (std::uint32_t component = 0; component < 4; ++component) {
            if (((exportedMask >> ((mapping >> (2u * component)) & 3u)) & 1u) != 0) state.colorWriteMask |= 1u << component;
        }
        state.blendEnable = (blend >> 30u) & 1u;
        if (state.blendEnable) {
            Require((read(cx, 0x31c + slot * 0xfu) & 0x10000u) == 0, "blend bypass conflicts with enabled blending");
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
            for (std::uint32_t i = 0; i < 4; ++i) result.blendConstants[i] = readFloat(cx, 0x105 + i);
        }
        result.blends.push_back(state);
    }
    if (!result.blends.empty()) result.blend = result.blends.front();
    APS5_LOG_OUT_DEBUG("DecodeState done colorTarget=%u render=%ux%u topology=%u", result.hasColorTarget ? 1u : 0u, result.renderExtent.width, result.renderExtent.height, static_cast<unsigned>(result.topology));
    return result;
}

std::array<std::uint8_t, 8> ExportMappings(const State& state) {
    std::array<std::uint8_t, 8> mappings{};
    mappings.fill(0xe4u);
    for (std::size_t slot = 0; slot < state.colors.size() && slot < mappings.size(); ++slot) mappings[slot] = state.colors[slot].componentMapping;
    return mappings;
}

ColorTarget DecodeColorBuffer(const Registers& cx, std::uint32_t slot) {
    const auto stride = slot * 0xfu;
    ColorTarget color{};
    const auto info = read(cx, 0x31c + stride);
    const auto number = (info >> 8u) & 7u;
    const auto swap = (info >> 11u) & 3u;
    APS5_LOG_OUT_DEBUG("Color %u info=0x%x number=%u swap=%u", slot, info, number, swap);
    const auto format = (info >> 2u) & 0x1fu;
    const auto decoded = DecodeColorFormat(format, number, swap);
    // ROUND_MODE (bit 18) only affects unorm rounding. With DCC_ENABLE (bit 28) the target is written
    if ((info & ~(0x00039f7cu | 0x00040000u | 0x10000000u)) != 0) throw std::runtime_error("AGC graphics: color compression, DCC, endian conversion, nonstandard rounding or color optimization is unsupported (CB_COLOR_INFO 0x" + [&] { char text[16]; std::snprintf(text, sizeof(text), "%08x", info); return std::string(text); }() + ")");
    Require((info & 0x8000u) != 0 || number == 7 || number == 4 || number == 5, "unclamped normalized color is unsupported");
    const auto view = read(cx, 0x31b + stride);
    Require((view & ~0x3c000000u) == 0, "color array views are unsupported");
    const auto viewMip = (view >> 26u) & 0xfu;
    zero(cx, 0x31d + stride, ~0u, "color samples, fragments or destination alpha override");
    const auto attrib2 = read(cx, 0x3b0 + slot);
    const auto maxMip = attrib2 >> 28u;
    Require(viewMip <= maxMip, "color view mip exceeds the surface");
    const auto attrib3 = read(cx, 0x3b8 + slot);
    color.tileMode = DecodeColorTileMode(attrib3);
    color.extent = {((attrib2 >> 14u) & 0x3fffu) + 1u, (attrib2 & 0x3fffu) + 1u};
    color.elementBytes = decoded.elementBytes;
    std::uint64_t mipOffset = 0;
    color.surfaceExtent = color.extent;
    color.mipCount = maxMip + 1u;
    color.mip = viewMip;
    if (maxMip != 0) {
        // A mipmapped surface is addressed like a texture; the view renders into one mip of it.
        const auto mips = ComputeElementMipLayout(color.tileMode == ColorTileMode::Linear ? TextureTileMode::kLinear : TextureTileMode::kR64KBX, color.elementBytes, color.extent.width, color.extent.height, maxMip + 1u);
        const auto& mip = mips.at(viewMip);
        mipOffset = mip.tiledOffset;
        color.mipTail = mip.tail;
        color.extent = {mip.width, mip.height};
    }
    const ColorTargetLayout colorLayout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
    const auto high = read(cx, 0x390 + slot);
    Require((high & ~0xffu) == 0, "invalid color address extension");
    color.surfaceAddress = (static_cast<std::uint64_t>(high) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x318 + stride)) << 8u);
    color.address = color.surfaceAddress + mipOffset;
    color.bytes = colorLayout.Bytes();
    GuestMemory::CheckRange(reinterpret_cast<const void*>(color.address), color.bytes, colorLayout.Alignment(), true);
    color.format = decoded.format;
    color.componentMapping = decoded.componentMapping;
    for (std::uint32_t word = 0; word < 2; ++word) {
        const auto clear = find(cx, 0x323 + word + stride);
        color.clearWords[word] = clear == cx.end() ? 0u : clear->second;
    }
    if ((info & 0x10000000u) != 0) {
        if (maxMip == 0) {
            const auto dccHigh = find(cx, 0x3a8 + slot);
            color.dccAddress = ((dccHigh == cx.end() ? 0ull : static_cast<std::uint64_t>(dccHigh->second & 0xffu)) << 40u) | (static_cast<std::uint64_t>(read(cx, 0x325 + stride)) << 8u);
            color.dccAlphaOnMsb = DccAlphaOnMsb(color.format, swap);
        } else {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[gpu] DCC keys of mipmapped color targets are ignored\n");
            }
        }
    }
    return color;
}

std::optional<ColorMetadataPass> DecodeColorMetadataPass(const QueueState& queue) {
    const auto& cx = queue.context;
    const auto control = find(cx, 0x202);
    if (control == cx.end()) return std::nullopt;
    const auto mode = (control->second >> 4u) & 7u;
    if (mode != 2u && mode != 6u) return std::nullopt;
    ColorMetadataPass pass{mode == 2u ? ColorMetadataPass::Mode::EliminateFastClear : ColorMetadataPass::Mode::DccDecompress, {}};
    Require((control->second & ~0x70u) == 0xcc0000u, "CB metadata pass with a nonstandard ROP, dual quads disabled or degamma");
    Require((read(cx, 0x200) & 0xfu) == 0 && (read(cx, 0x0) & 0xfu) == 0, "CB metadata pass with depth or stencil work");
    zero(cx, 0x2f8, ~0u, "multisampling or coverage conversion");
    zero(cx, 0x80, ~0u, "window offset");
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu) throw std::runtime_error(vteMessage(viewportControl));
    const auto xs = std::fabs(readFloat(cx, 0x10f));
    const auto xo = readFloat(cx, 0x110);
    const auto ys = std::fabs(readFloat(cx, 0x111));
    const auto yo = readFloat(cx, 0x112);
    VkRect2D covered{{0, 0}, {0x7fffu, 0x7fffu}};
    intersect(covered, cx, 0xc, true);
    intersect(covered, cx, 0x81, false);
    intersect(covered, cx, 0x90, false);
    if ((read(cx, 0x292) & 2u) != 0) intersect(covered, cx, 0x94, false);
    const auto targetMask = read(cx, 0x8e);
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        if (((targetMask >> (4u * slot)) & 0xfu) == 0 || ((read(cx, 0x31c + slot * 0xfu) >> 2u) & 0x1fu) == 0) continue;
        const auto target = DecodeColorBuffer(cx, slot);
        Require((read(cx, 0x31c + slot * 0xfu) & 0x10000000u) == 0 || target.dccAddress != 0, "CB metadata pass over a mipmapped DCC color target, whose keys are not modeled");
        const auto width = static_cast<float>(target.extent.width);
        const auto height = static_cast<float>(target.extent.height);
        const bool viewportCovers = xo - xs <= 0.0f && xo + xs >= width && yo - ys <= 0.0f && yo + ys >= height;
        const bool scissorCovers = covered.offset.x == 0 && covered.offset.y == 0 && covered.extent.width >= target.extent.width && covered.extent.height >= target.extent.height;
        Require(viewportCovers && scissorCovers, "CB metadata pass over part of a color target");
        pass.targets.push_back(target);
    }
    return pass;
}

std::string DrawRejection(const QueueState& queue, bool indexed) {
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
        if (value(queue.userConfig, 0x242, primitive) && (primitive & 0x3fu) != 1 && (primitive & 0x3fu) != 2 && (primitive & 0x3fu) != 3 && (primitive & 0x3fu) != 4 && (primitive & 0x3fu) != 5 && (primitive & 0x3fu) != 6) return "AGC graphics: primitive restart is only supported for point, line and triangle topologies";
        if (value(cx, 0x103, resetIndex) && (resetIndex & 0xffffu) != 0xffffu) return "AGC graphics: primitive restart index other than all ones is unsupported";
    }
    if (auto reason = nonzero(cx, 0x207, ~LayerExports, "clip distances, layer, viewport or auxiliary vertex exports"); !reason.empty()) return reason;
    if (value(cx, 0x200, word)) {
        const bool surface = (word & 3u) != 0 && depthSurfaceBound(cx);
        if (!surface && !depthPassThrough(word) && !IgnoreDepthTest() && (effectiveDepthControl(word) & DepthControlMask) != 0) return zeroMessage(0x200, word, "depth, stencil or conditional color writes");
        if (auto reason = require((word & 0xc0000008u) == 0, "depth bounds or depth-conditional color writes are unsupported"); !reason.empty()) return reason;
    }
    if (auto reason = nonzero(cx, 0x203, ShaderControlMask, "depth export, shader coverage or ordered fragment execution"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x2dc, AlphaToCoverageMask, "alpha-to-coverage"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x2f8, ~0u, "multisampling or coverage conversion"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x292, ScanModeMask, "scan conversion mode"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x293, ScanControlMask, "sample iteration, primitive discard or out-of-order rasterization"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x80, ~0u, "window offset"); !reason.empty()) return reason;
    if (auto reason = nonzero(cx, 0x8d, ScreenOffsetMask, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits"); !reason.empty()) return reason;
    if (value(cx, 0x83, word) && word != 0xffffu) return require(false, "clip rectangles are unsupported");
    if (value(cx, 0x8c, word) && (word & 0xfu) != 0xau) return require(false, "nonstandard triangle edge rules are unsupported");
    if (value(cx, 0x2f9, word) && word != 0x2du) return require(false, "nonstandard pixel center or vertex quantization is unsupported");
    if (value(cx, 0x313, word) && word != 0x6000u) return require(false, "conservative rasterization is unsupported");
    std::uint32_t other = 0;
    if (value(cx, 0x30e, word) && value(cx, 0x30f, other) && (word != 0xffffffffu || other != 0xffffffffu)) return require(false, "sample masks are unsupported");
    if (value(cx, 0x206, word) && word != 0x43fu) return vteMessage(word);
    if (auto reason = nonzero(cx, 0x204, ClipControlMask, "unsupported PA_CL_CLIP_CNTL flags"); !reason.empty()) return reason;
    std::uint32_t targetMask = 0, shaderMask = 0;
    if (value(cx, 0x8e, targetMask) && value(cx, 0x8f, shaderMask) && value(cx, 0x202, word) && !colorControlSupported(word, (targetMask & shaderMask) != 0)) return colorControlMessage(word);
    if (auto reason = nonzero(cx, 0x1c4, ~0u, "depth or sample-mask export"); !reason.empty()) return reason;
    // The pixel stage decode (ShaderInputState.cpp) reads these after DecodeState and the program
    // prepare; a bank without them fails there with this message.
    for (const auto offset : {0x1b3u, 0x1b4u, 0x1c5u}) {
        if (find(cx, offset) != cx.end()) continue;
        char text[64];
        std::snprintf(text, sizeof(text), "AGC graphics: missing register at DWORD 0x%x", offset);
        return text;
    }
    return {};
}

std::vector<RegisterRead>*& RegisterReadLog() {
    static thread_local std::vector<RegisterRead>* log = nullptr;
    return log;
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
