#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Unorm10_11_11Info = 0x8018u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t DepthInput = 0x400u;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 64> Programs{};
std::size_t programCount = 0;

constexpr std::array<std::uint32_t, 3> FieldOffset{0u, 11u, 22u};
constexpr std::array<std::uint32_t, 3> FieldBits{11u, 11u, 10u};

float Unit(float value) {
    return value > 0.0f ? (value < 1.0f ? value : 1.0f) : 0.0f;
}

std::uint32_t Unorm(float value, std::uint32_t bits) {
    const auto maximum = (1u << bits) - 1u;
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(Unit(value)) * maximum));
}

float Factor(std::uint32_t code, float source, float destination, float sourceAlpha) {
    switch (code) {
        case 0: return 0.0f;
        case 1: return 1.0f;
        case 2: return source;
        case 3: return 1.0f - source;
        case 4: return sourceAlpha;
        case 5: return 1.0f - sourceAlpha;
        case 6: return 1.0f;
        case 7: return 0.0f;
        case 8: return destination;
        case 9: return 1.0f - destination;
        case 10: return std::fmin(sourceAlpha, 0.0f);
        default: throw std::runtime_error("the test uses a blend factor the reference does not model");
    }
}

float Term(std::uint32_t code, float value, float source, float destination, float sourceAlpha) {
    if (code == 0u) return 0.0f;
    if (code == 1u) return value;
    return value * Factor(code, source, destination, sourceAlpha);
}

float BlendChannel(std::uint32_t control, float source, float destination, float sourceAlpha) {
    const auto sourceFactor = control & 0x1fu;
    const auto combine = (control >> 5u) & 7u;
    const auto destinationFactor = (control >> 8u) & 0x1fu;
    const auto s = Term(sourceFactor, source, source, destination, sourceAlpha);
    const auto d = Term(destinationFactor, destination, source, destination, sourceAlpha);
    switch (combine) {
        case 0: return s + d;
        case 1: return s - d;
        case 2: return std::fmin(source, destination);
        case 3: return std::fmax(source, destination);
        case 4: return d - s;
        default: throw std::runtime_error("the test uses a combine function the reference does not model");
    }
}

struct Expected {
    std::uint32_t word;
    std::uint8_t ties;
};

Expected Blended(std::uint32_t control, std::uint32_t wordMask, std::array<float, 4> rgba, std::uint32_t stored) {
    const auto combine = (control >> 5u) & 7u;
    const bool arithmetic = control != 0u && combine != 2u && combine != 3u;
    std::uint32_t word = 0;
    std::uint8_t ties = 0;
    for (std::uint32_t field = 0; field < 3; ++field) {
        const auto maximum = (1u << FieldBits[field]) - 1u;
        const auto destination = static_cast<float>(static_cast<double>((stored >> FieldOffset[field]) & maximum) / maximum);
        const auto value = control == 0u ? rgba[field] : BlendChannel(control, Unit(rgba[field]), destination, Unit(rgba[3]));
        word |= Unorm(value, FieldBits[field]) << FieldOffset[field];
        const auto scaled = static_cast<double>(Unit(value)) * maximum;
        if (arithmetic && ((wordMask >> FieldOffset[field]) & 1u) != 0 && std::fabs(scaled - std::floor(scaled) - 0.5) < 1e-3) ties |= static_cast<std::uint8_t>(1u << field);
    }
    return {(stored & ~wordMask) | (word & wordMask), ties};
}

bool Matches(std::uint32_t stored, std::uint32_t expected, std::uint8_t ties) {
    for (std::uint32_t field = 0; field < 3; ++field) {
        const auto maximum = (1u << FieldBits[field]) - 1u;
        const auto a = static_cast<std::int64_t>((stored >> FieldOffset[field]) & maximum);
        const auto b = static_cast<std::int64_t>((expected >> FieldOffset[field]) & maximum);
        const std::int64_t slack = ((ties >> field) & 1u) != 0 ? 1 : 0;
        if (a - b > slack || b - a > slack) return false;
    }
    return true;
}

std::span<const std::uint32_t> Program(const std::array<std::uint32_t, 4>& vgprs) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = vgprs[vgpr];
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> Float32Program(std::array<float, 4> rgba) {
    return Program({std::bit_cast<std::uint32_t>(rgba[0]), std::bit_cast<std::uint32_t>(rgba[1]), std::bit_cast<std::uint32_t>(rgba[2]), std::bit_cast<std::uint32_t>(rgba[3])});
}

std::span<const std::uint32_t> DepthAsRedProgram(std::array<float, 4> rgba) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    code[at++] = 0x7e080300u;
    for (std::uint32_t vgpr = 1; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(rgba[vgpr]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

const std::array<std::array<float, 4>, 3>& TriangleAt(float z) {
    static std::map<float, std::array<std::array<float, 4>, 3>> triangles;
    return triangles.try_emplace(z, std::array<std::array<float, 4>, 3>{{{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}}}).first->second;
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Unorm10_11_11Info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R32_UINT && color.componentMapping == 0xe4u && color.packing == ShaderRecompiler::ColorExportPacking::Unorm10_11_11 && color.tileMode == ColorTileMode::RenderTarget && color.bytes == block.bytes, "the tiled 10_11_11 unorm target did not decode as a packed R32_UINT target");
    return color;
}

struct DrawSetup {
    std::uint32_t inputs = 0;
    std::span<const std::array<float, 4>> vertices = Triangle;
};

std::uint32_t Draw(AgcDriver::VulkanDevice& device, AgcDriver::Graphics::ColorTarget color, std::uint32_t blendControl, std::uint32_t exportedMask, std::span<const std::uint32_t> pixelCode, const DrawSetup& setup = {}) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(setup.vertices.data());
    const auto vertexCount = static_cast<std::uint32_t>(setup.vertices.size());
    const std::uint32_t stride = 16u;
    const auto vertexCode = std::span<const std::uint32_t>(VertexCode);
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), vertexCount, 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(vertexCode)}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    const bool merged = (blendControl != 0u || (exportedMask & 7u) != 7u);
    color.mergeBlend = blendControl == 0u || !merged ? 0u : AgcDriver::Graphics::PackedBlendControl(color.packing, blendControl);
    color.partialWriteMask = merged ? AgcDriver::Graphics::PackedWriteMask(color.packing, color.componentMapping, exportedMask & 7u) : 0u;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.renderLayers = color.layers;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = merged ? 0u : VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, setup.inputs}, {0x1b4, setup.inputs}, {0x203, 0}, {0x1c5, Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->targetExportPacking = AgcDriver::Graphics::ExportPackings(state);
    fragment.context.pixel->targetMerge = AgcDriver::Graphics::ExportTargetMerge(state);
    const auto& merge = fragment.context.pixel->targetMerge;
    Require((merge.mask != 0u) == merged && merge.mask == color.partialWriteMask && merge.blendControl == color.mergeBlend && (!merged || merge.packing == ShaderRecompiler::ColorExportPacking::Unorm10_11_11), "the draw's target merge disagrees with its blend and write mask");
    Require(!merge.layered, "the draw's target merge into a one-layer target was marked layered");
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, fragment);
    const auto pixelResult = invocation.Materialize(*invocation.Capture(runtime));
    const bool registered = pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId;
    Require(registered != merged, merged ? "the blending draw reused the registered pixel shader" : "the plain write did not use the registered pixel shader");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, vertexCount, 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    return color.partialWriteMask;
}

std::vector<std::uint32_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint32_t> stored(block.bytes / 4u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::vector<std::uint32_t> Fill(Block& block, std::uint32_t seed) {
    std::vector<std::uint32_t> words(block.bytes / 4u);
    for (std::size_t index = 0; index < words.size(); ++index) {
        words[index] = static_cast<std::uint32_t>((index + seed) * 0x9e3779b9u) ^ static_cast<std::uint32_t>(index >> 3u);
    }
    std::memcpy(block.data, words.data(), block.bytes);
    return words;
}

void ExpectWords(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint32_t>& expected, const std::vector<std::uint8_t>& ties, const std::vector<std::uint32_t>& before, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 4u;
            if (x >= ScissorWidth && stored[index] != expected[index]) throw std::runtime_error(what + ": a pixel outside the scissor changed");
            if (Matches(stored[index], expected[index], ties[index])) continue;
            char detail[192];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) holds 0x%08x, expected 0x%08x (it held 0x%08x)", x, y, stored[index], expected[index], before[index]);
            throw std::runtime_error(what + detail);
        }
    }
}

void ApplyInside(const ColorTargetLayout& layout, std::vector<std::uint32_t>& words, std::vector<std::uint8_t>& ties, std::uint32_t control, std::uint32_t wordMask, std::array<float, 4> rgba) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < ScissorWidth; ++x) {
            const auto index = layout.Offset(x, y) / 4u;
            const auto blended = Blended(control, wordMask, rgba, words[index]);
            words[index] = blended.word;
            ties[index] |= blended.ties;
        }
    }
}

struct Case {
    std::uint32_t control;
    std::uint32_t mask;
    std::array<float, 4> rgba;
    const char* name;
};

std::string Describe(const Case& draw) {
    char text[192];
    std::snprintf(text, sizeof(text), "%s (CB_BLEND_CONTROL 0x%08x, CB_TARGET_MASK 0x%x) of (%.9g, %.9g, %.9g, %.9g)", draw.name, draw.control, draw.mask, draw.rgba[0], draw.rgba[1], draw.rgba[2], draw.rgba[3]);
    return text;
}

void Sequence(AgcDriver::VulkanDevice& device, Block& block, const ColorTargetLayout& layout, const AgcDriver::Graphics::ColorTarget& color, std::span<const Case> draws, std::uint32_t seed, const std::string& what) {
    const auto before = Fill(block, seed);
    auto expected = before;
    std::vector<std::uint8_t> ties(expected.size());
    for (const auto& draw : draws) {
        const auto wordMask = Draw(device, color, draw.control, draw.mask, Float32Program(draw.rgba));
        ApplyInside(layout, expected, ties, draw.control, wordMask == 0u ? 0xffffffffu : wordMask, draw.rgba);
    }
    ExpectWords(device, block, layout, expected, ties, before, what);
}

void SameDrawOverdraw(AgcDriver::VulkanDevice& device, Block& block, const ColorTargetLayout& layout, const AgcDriver::Graphics::ColorTarget& color, float first, float last, std::uint32_t seed) {
    constexpr std::uint32_t Layers = 16;
    constexpr std::uint32_t SourceOver = 0x65040504u;
    const std::array<float, 4> constant{0.0f, 0.75f, 1.5f, 0.5f};
    static std::map<float, std::vector<std::array<float, 4>>> stacks;
    auto& vertices = stacks[last];
    std::vector<float> order;
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        const auto z = layer % 2u == 0u ? first : last;
        const auto& triangle = TriangleAt(z);
        vertices.insert(vertices.end(), triangle.begin(), triangle.end());
        order.push_back(z);
    }
    const auto before = Fill(block, seed);
    auto expected = before;
    std::vector<std::uint8_t> ties(expected.size());
    for (const auto z : order) ApplyInside(layout, expected, ties, SourceOver, 0xffffffffu, {z, constant[1], constant[2], constant[3]});
    Draw(device, color, SourceOver, 0xfu, DepthAsRedProgram(constant), {DepthInput, vertices});
    char what[192];
    std::snprintf(what, sizeof(what), "one source-over draw of %u full-screen triangles alternating z %.2f and %.2f, z blended into red in primitive order into a 10_11_11 unorm target", Layers, first, last);
    ExpectWords(device, block, layout, expected, ties, before, what);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFragmentShaderPixelInterlockEXT)) {
            std::puts("skipped, the device lacks fragmentShaderPixelInterlock");
            return VulkanTestSkipped;
        }
        const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
        Block block(layout.Bytes());
        const auto color = DecodeTarget(block);
        Require(AgcDriver::Graphics::PackedWriteMask(color.packing, color.componentMapping, 1u) == 0x000007ffu && AgcDriver::Graphics::PackedWriteMask(color.packing, color.componentMapping, 2u) == 0x003ff800u && AgcDriver::Graphics::PackedWriteMask(color.packing, color.componentMapping, 4u) == 0xffc00000u, "the export components did not map to the guest fields (red in 10..0, green in 21..11, blue in 31..22)");
        const std::array<Case, 14> cases{{
            {0x65050504u, 0xfu, {0.9f, 0.1f, 0.6f, 0.3f}, "a source-over blend"},
            {0x40000101u, 0xfu, {0.25f, 0.5f, 0.125f, 1.0f}, "an additive blend"},
            {0x40000101u, 0xfu, {1.5f, -0.25f, 0.5f, 2.0f}, "an additive blend of a source outside [0, 1]"},
            {0x40000121u, 0xfu, {0.75f, 0.125f, 0.0f, 1.0f}, "a subtracting blend"},
            {0x40000181u, 0xfu, {0.25f, 0.5f, 0.875f, 1.0f}, "a reverse-subtracting blend"},
            {0x40000141u, 0xfu, {0.5f, 1.0f, 0.25f, 1.0f}, "a MIN blend"},
            {0x40000161u, 0xfu, {0.5f, 1.0f, 0.25f, 1.0f}, "a MAX blend"},
            {0x40000008u, 0xfu, {0.5f, 0.75f, 0.25f, 1.0f}, "a multiplying blend (DST_COLOR, ZERO)"},
            {0x40000109u, 0xfu, {0.5f, 0.25f, 0.75f, 1.0f}, "a screen blend (ONE_MINUS_DST_COLOR, ONE)"},
            {0x40000302u, 0xfu, {0.5f, 0.75f, 0.25f, 1.0f}, "a SRC_COLOR, ONE_MINUS_SRC_COLOR blend"},
            {0x40000501u, 0xfu, {0.2f, 0.4f, 0.6f, 0.35f}, "a ONE, ONE_MINUS_SRC_ALPHA blend"},
            {0x40000706u, 0xfu, {0.5f, 0.75f, 0.25f, 1.0f}, "a DST_ALPHA, ONE_MINUS_DST_ALPHA blend into the alpha-less target"},
            {0x65050504u, 0xfu, {0.9f, 0.1f, 0.6f, 1.5f}, "a source-over blend with a source alpha above 1"},
            {0x65050504u, 0xfu, {0.9f, 0.1f, 0.6f, -0.25f}, "a source-over blend with a negative source alpha"},
        }};
        for (std::size_t index = 0; index < cases.size(); ++index) {
            const std::array<Case, 1> one{cases[index]};
            Sequence(*device, block, layout, color, one, static_cast<std::uint32_t>(index), Describe(cases[index]) + " into a tiled 10_11_11 unorm target");
        }
        const std::array<Case, 2> partials{{
            {0x65050504u, 0x1u, {0.8f, 0.5f, 0.25f, 0.5f}, "a source-over blend of red alone"},
            {0x00000000u, 0x6u, {0.0f, 0.625f, 0.375f, 1.0f}, "a plain write of green and blue alone"},
        }};
        for (std::size_t index = 0; index < partials.size(); ++index) {
            const std::array<Case, 1> one{partials[index]};
            Sequence(*device, block, layout, color, one, 40u + static_cast<std::uint32_t>(index), Describe(partials[index]) + " into a tiled 10_11_11 unorm target");
        }
        const std::array<Case, 4> chain{{cases[0], {0x00000000u, 0xfu, {0.5f, 0.5f, 0.5f, 1.0f}, "a plain write"}, cases[1], cases[3]}};
        Sequence(*device, block, layout, color, chain, 60u, "a source-over blend, a plain write, an additive and a subtracting blend drawn back to back into a tiled 10_11_11 unorm target");
        SameDrawOverdraw(*device, block, layout, color, 0.25f, 0.75f, 70u);
        SameDrawOverdraw(*device, block, layout, color, 0.75f, 0.25f, 71u);
        std::printf("10_11_11 unorm blend tests passed: %zu single blends, %zu partial writes, a back-to-back chain and 2 same-draw overdraws of 16 layers into a %ux%u tiled target, the unscissored pixels kept\n", cases.size(), partials.size(), Width, Height);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
