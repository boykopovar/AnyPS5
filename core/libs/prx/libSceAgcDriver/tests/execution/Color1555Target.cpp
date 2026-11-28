#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
constexpr std::uint32_t Color1555Info = 0x8044u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Float16Export = 4;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 16> Programs{};
std::size_t programCount = 0;

using Color = std::array<float, 4>;

constexpr std::array<std::array<std::uint32_t, 4>, 4> FieldSources{{{0, 1, 2, 3}, {2, 1, 0, 3}, {3, 2, 1, 0}, {3, 0, 1, 2}}};
constexpr std::array<const char*, 4> SwapNames{"STD", "ALT", "STD_REV", "ALT_REV"};

std::uint16_t Texel(const Color& color, std::uint32_t swap) {
    const auto unorm = [](float value, std::uint32_t maximum) { return static_cast<std::uint32_t>(std::lround(std::fmin(std::fmax(static_cast<double>(value), 0.0), 1.0) * maximum)); };
    const auto& source = FieldSources[swap];
    return static_cast<std::uint16_t>(unorm(color[source[0]], 31) | (unorm(color[source[1]], 31) << 5u) | (unorm(color[source[2]], 31) << 10u) | (unorm(color[source[3]], 1) << 15u));
}

std::span<const std::uint32_t> Program(const Color& color) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> HalfProgram(const std::array<std::uint16_t, 4>& halves) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t pair = 0; pair < 2; ++pair) {
        code[at++] = 0x7e0002ffu | ((4u + pair) << 17u);
        code[at++] = halves[2u * pair] | (static_cast<std::uint32_t>(halves[2u * pair + 1u]) << 16u);
    }
    code[at++] = 0xf8001c0fu;
    code[at++] = 0x00000504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

VkPipelineColorBlendAttachmentState Written(VkColorComponentFlags mask = 0xf) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = mask;
    return blend;
}

std::uint16_t FieldMask(std::uint32_t exportMask, std::uint32_t swap) {
    constexpr std::array<std::uint16_t, 4> fields{0x001fu, 0x03e0u, 0x7c00u, 0x8000u};
    std::uint16_t mask = 0;
    for (std::uint32_t field = 0; field < 4; ++field) {
        if (((exportMask >> FieldSources[swap][field]) & 1u) != 0) mask = static_cast<std::uint16_t>(mask | fields[field]);
    }
    return mask;
}

Color Unpack(std::uint16_t texel, std::uint32_t swap) {
    Color color{};
    for (std::uint32_t field = 0; field < 4; ++field) {
        const auto maximum = field == 3 ? 1u : 31u;
        color[FieldSources[swap][field]] = static_cast<float>((texel >> (5u * field)) & maximum) / static_cast<float>(maximum);
    }
    return color;
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t attrib3, std::uint32_t swap) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Color1555Info | (swap << 11u);
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = attrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_A1R5G5B5_UNORM_PACK16 && color.elementBytes == 2 && color.bytes == block.bytes, "the 1_5_5_5 unorm target did not decode as an A1R5G5B5 surface");
    return color;
}

VkColorComponentFlags DecodedWriteMask(const Block& block, std::uint32_t attrib3, std::uint32_t swap, std::uint32_t targetMask) {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, Float32Export}, {0x1c3, 4},
        {0x318, static_cast<std::uint32_t>(block.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(block.Address() >> 40u)},
        {0x31b, 0}, {0x31c, Color1555Info | (swap << 11u)}, {0x31d, 0},
        {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)}, {0x3b8, attrib3}, {0x1e0, 0},
        {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x10f, std::bit_cast<std::uint32_t>(static_cast<float>(Width) / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(static_cast<float>(Width) / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-static_cast<float>(Height) / 2.0f)}, {0x112, std::bit_cast<std::uint32_t>(static_cast<float>(Height) / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}
    };
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    const auto decoded = AgcDriver::Graphics::DecodeState(queue);
    Require(decoded.colors.size() == 1u && decoded.colors[0].format == VK_FORMAT_A1R5G5B5_UNORM_PACK16 && decoded.colors[0].partialWriteMask == 0u && decoded.blends.size() == 1u, "a 1_5_5_5 target with a partial CB_TARGET_MASK did not decode to one A1R5G5B5 target written by its channel mask");
    return decoded.blends[0].colorWriteMask;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode, const VkPipelineColorBlendAttachmentState& blend = Written()) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, exportFormat}};
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
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId, "the draw did not use the registered pixel shader");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint16_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint16_t> stored(block.bytes / 2u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::vector<std::uint16_t> Fill(Block& block) {
    std::vector<std::uint16_t> halves(block.bytes / 2u);
    for (std::size_t index = 0; index < halves.size(); ++index) halves[index] = static_cast<std::uint16_t>((index * 0x9e3779b9u) >> 16u);
    std::memcpy(block.data, halves.data(), block.bytes);
    return halves;
}

void ExpectSurface(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint16_t>& before, std::uint16_t texel, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 2u;
            const bool inside = x < ScissorWidth;
            const auto expected = inside ? texel : before[index];
            if (stored[index] == expected) continue;
            char detail[160];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) %s the scissor holds 0x%04x, expected 0x%04x", x, y, inside ? "inside" : "outside", stored[index], expected);
            throw std::runtime_error(what + detail);
        }
    }
}

void TargetTests(AgcDriver::VulkanDevice& device, Block& block, std::uint32_t attrib3, ColorTileMode mode, const char* kind, std::uint32_t swap) {
    const ColorTargetLayout layout(Width, Height, mode, 2);
    Require(layout.Bytes() == block.bytes, "the test surface size disagrees with its layout");
    const auto color = DecodeTarget(block, attrib3, swap);
    auto before = Fill(block);
    struct Case {
        Color color;
        std::array<std::uint16_t, 4> texels;
    };
    const std::array<Case, 11> cases{{
        {{1.0f, 0.0f, 0.0f, 1.0f}, {0x801f, 0xfc00, 0x801f, 0x03ff}},
        {{0.0f, 1.0f, 0.0f, 0.0f}, {0x03e0, 0x03e0, 0x7c00, 0x7c00}},
        {{0.0f, 0.0f, 1.0f, 0.0f}, {0x7c00, 0x001f, 0x03e0, 0x8000}},
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0x8000, 0x8000, 0x001f, 0x001f}},
        {{1.0f, 0.0f, 1.0f, 0.0f}, {0x7c1f, 0x7c1f, 0x83e0, 0x83e0}},
        {{1.0f, 1.0f, 1.0f, 1.0f}, {0xffff, 0xffff, 0xffff, 0xffff}},
        {{0.0f, 0.0f, 0.0f, 0.0f}, {0x0000, 0x0000, 0x0000, 0x0000}},
        {{3.0f / 31.0f, 10.0f / 31.0f, 21.0f / 31.0f, 1.0f}, {0xd543, 0x8d55, 0x2abf, 0xa87f}},
        {{21.0f / 31.0f, 3.0f / 31.0f, 10.0f / 31.0f, 0.0f}, {0x2875, 0x546a, 0x8d40, 0x0ea0}},
        {{1.0f / 31.0f, 30.0f / 31.0f, 2.0f / 31.0f, 0.75f}, {0x8bc1, 0x87c2, 0x7857, 0x7837}},
        {{2.0f, -1.0f, 10.0f / 31.0f, 0.25f}, {0x281f, 0x7c0a, 0x8148, 0x03e8}},
    }};
    for (const auto& test : cases) {
        const auto texel = test.texels[swap];
        Require(Texel(test.color, swap) == texel, "the test's expected texel disagrees with the 1_5_5_5 field layout");
        Draw(device, color, Float32Export, Program(test.color));
        char what[192];
        std::snprintf(what, sizeof(what), "a 32_ABGR export of (%.9g, %.9g, %.9g, %.9g) into a %s 1_5_5_5 unorm target with component swap %s", test.color[0], test.color[1], test.color[2], test.color[3], kind, SwapNames[swap]);
        ExpectSurface(device, block, layout, before, texel, what);
        before = ReadBack(device, block);
        programCount = 0;
    }
    const Color written{3.0f / 31.0f, 10.0f / 31.0f, 21.0f / 31.0f, 1.0f};
    for (const std::uint32_t exportMask : {0x1u, 0x2u, 0x8u, 0x5u, 0xau, 0x7u}) {
        const std::vector<std::uint16_t> known(before.size(), static_cast<std::uint16_t>(0x6b5au));
        std::memcpy(block.data, known.data(), block.bytes);
        Draw(device, color, Float32Export, Program(written), Written(DecodedWriteMask(block, attrib3, swap, exportMask)));
        const auto fields = FieldMask(exportMask, swap);
        const auto texel = static_cast<std::uint16_t>((Texel(written, swap) & fields) | (0x6b5au & ~fields));
        char what[192];
        std::snprintf(what, sizeof(what), "a 32_ABGR export with CB_TARGET_MASK 0x%x over the halfword 0x6b5a of a %s 1_5_5_5 unorm target with component swap %s", exportMask, kind, SwapNames[swap]);
        ExpectSurface(device, block, layout, known, texel, what);
        programCount = 0;
    }
    const std::array<std::uint16_t, 4> halves{0x3400u, 0x3a00u, 0x3900u, 0x3c00u};
    const Color half{0.25f, 0.75f, 0.625f, 1.0f};
    before = ReadBack(device, block);
    Draw(device, color, Float16Export, HalfProgram(halves));
    char what[192];
    std::snprintf(what, sizeof(what), "an FP16_ABGR export of (0.25, 0.75, 0.625, 1) into a %s 1_5_5_5 unorm target with component swap %s", kind, SwapNames[swap]);
    ExpectSurface(device, block, layout, before, Texel(half, swap), what);
    programCount = 0;
    if (swap != 1u) return;
    const auto destination = static_cast<std::uint16_t>(1u | (2u << 5u) | (9u << 10u) | 0x8000u);
    const Color source{5.0f / 31.0f, 10.0f / 31.0f, 21.0f / 31.0f, 0.5f};
    const std::vector<std::uint16_t> known(before.size(), destination);
    std::memcpy(block.data, known.data(), block.bytes);
    auto blend = Written();
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    Draw(device, color, Float32Export, Program(source), blend);
    const auto stored = Unpack(destination, swap);
    Color blended{};
    for (std::uint32_t component = 0; component < 4; ++component) blended[component] = source[component] * source[3] + stored[component] * (1.0f - source[3]);
    std::snprintf(what, sizeof(what), "a SRC_ALPHA, ONE_MINUS_SRC_ALPHA blend of (5/31, 10/31, 21/31, 0.5) over the halfword 0x%04x of a %s 1_5_5_5 unorm target with component swap ALT", destination, kind);
    ExpectSurface(device, block, layout, known, Texel(blended, swap), what);
    programCount = 0;
}

void DccClearTests(AgcDriver::VulkanDevice& device, Block& block, Block& keys, std::uint32_t swap) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 2);
    struct Code {
        std::uint8_t key;
        const char* name;
        std::uint16_t msb;
        std::uint16_t lsb;
    };
    const Color drawn{3.0f / 31.0f, 10.0f / 31.0f, 21.0f / 31.0f, 1.0f};
    for (const auto& code : {Code{0x40u, "0001", 0x8000u, 0x001fu}, Code{0x80u, "1110", 0x7fffu, 0xffe0u}, Code{0xc0u, "1111", 0xffffu, 0xffffu}}) {
        AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test refill");
        device.WaitIdle();
        Fill(block);
        auto color = DecodeTarget(block, TiledAttrib3, swap);
        color.dccAddress = keys.Address();
        color.dccAlphaOnMsb = AgcDriver::Graphics::DccAlphaOnMsb(color.format, swap);
        AgcDriver::GuestMemory::Write(keys.Address(), std::vector<std::byte>(color.bytes / 256u, std::byte{code.key}));
        const auto clears = AgcDriver::Graphics::StorageTexture::GpuClears();
        Draw(device, color, Float32Export, Program(drawn));
        Require(AgcDriver::Graphics::StorageTexture::GpuClears() > clears, std::string("the ") + code.name + " DCC clear of a 1_5_5_5 target with component swap " + SwapNames[swap] + " was not cleared on the GPU");
        const auto cleared = color.dccAlphaOnMsb ? code.msb : code.lsb;
        char what[192];
        std::snprintf(what, sizeof(what), "a draw scissored to part of a tiled 1_5_5_5 unorm target with component swap %s under %s DCC keys (clear texel 0x%04x)", SwapNames[swap], code.name, cleared);
        ExpectSurface(device, block, layout, std::vector<std::uint16_t>(block.bytes / 2u, cleared), Texel(drawn, swap), what);
        programCount = 0;
    }
}

void AlphaPlacementTests(AgcDriver::VulkanDevice& device, Block& block, Block& keys) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 2);
    const Color drawn{3.0f / 31.0f, 10.0f / 31.0f, 21.0f / 31.0f, 1.0f};
    constexpr std::uint32_t swap = 1;
    for (const bool imageOnMsb : {true, false}) {
        AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test refill");
        device.WaitIdle();
        Fill(block);
        auto color = DecodeTarget(block, TiledAttrib3, swap);
        color.dccAddress = keys.Address();
        AgcDriver::GuestMemory::Write(keys.Address(), std::vector<std::byte>(color.bytes / 256u, std::byte{0xff}));
        color.dccAlphaOnMsb = imageOnMsb;
        Draw(device, color, Float32Export, Program(drawn));
        programCount = 0;
        color.dccAlphaOnMsb = !imageOnMsb;
        Draw(device, color, Float32Export, Program(drawn));
        programCount = 0;
        AgcDriver::GuestMemory::Write(keys.Address(), std::vector<std::byte>(color.bytes / 256u, std::byte{0x40}));
        Draw(device, color, Float32Export, Program(drawn));
        programCount = 0;
        const std::uint16_t cleared = color.dccAlphaOnMsb ? 0x8000u : 0x001fu;
        char what[192];
        std::snprintf(what, sizeof(what), "a draw into a 1_5_5_5 target with the DCC alpha %s, whose image was made for the other placement under uncompressed keys, after a 0001 clear (clear texel 0x%04x)", color.dccAlphaOnMsb ? "last" : "first", cleared);
        ExpectSurface(device, block, layout, std::vector<std::uint16_t>(block.bytes / 2u, cleared), Texel(drawn, swap), what);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block tiled(ColorTargetLayout(Width, Height, ColorTileMode::RenderTarget, 2).Bytes());
        Block linear(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 2).Bytes());
        Block compressed(ColorTargetLayout(Width, Height, ColorTileMode::RenderTarget, 2).Bytes());
        Block keys(BlockBytes);
        for (std::uint32_t swap = 0; swap < 4; ++swap) {
            TargetTests(*device, tiled, TiledAttrib3, ColorTileMode::RenderTarget, "tiled", swap);
            TargetTests(*device, linear, LinearAttrib3, ColorTileMode::Linear, "linear", swap);
            DccClearTests(*device, compressed, keys, swap);
        }
        AlphaPlacementTests(*device, compressed, keys);
        std::puts("1_5_5_5 unorm color target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
