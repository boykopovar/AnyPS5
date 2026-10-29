#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "DepthFastClearHarness.hpp"
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
#include <functional>
#include <memory>
#include <optional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}
#endif

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Float16x4Info = 0x0730u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Float16Export = 4;
constexpr std::uint32_t Unorm16Export = 5;
constexpr std::uint32_t Snorm16Export = 6;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<Color, 4> Destinations{{
    {0.5f, 0.25f, 0.75f, 0.5f}, {1.0f, 0.0f, 0.125f, 0.25f}, {0.0625f, 0.875f, 0.5f, 1.0f}, {0.25f, 0.5f, 0.375f, 0.75f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 16> Programs{};
std::size_t programCount = 0;

std::uint16_t Half(float value) {
    Require(value == 0.0f || (value >= 0.0078125f && value <= 1.0f), "the test's half values are normal and at most 1");
    if (value == 0.0f) return 0;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    Require((bits & 0x1fffu) == 0, "the test's half values are exact");
    return static_cast<std::uint16_t>(((((bits >> 23u) & 0xffu) - 112u) << 10u) | ((bits >> 13u) & 0x3ffu));
}

float FromHalf(std::uint16_t half) {
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    Require((half & 0x8000u) == 0 && exponent != 31u, "a blended texel is negative, infinite or NaN");
    return exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24) : std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
}

std::uint32_t Packed16(float value, std::uint32_t exportFormat) {
    if (exportFormat == Float16Export) return Half(value);
    if (exportFormat == Unorm16Export) {
        Require(value >= 0.0f && value <= 1.0f, "the test's UNORM16 values are in [0, 1]");
        return static_cast<std::uint32_t>(std::lround(value * 65535.0f));
    }
    Require(exportFormat == Snorm16Export && value >= -1.0f && value <= 1.0f, "the test's SNORM16 values are in [-1, 1]");
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(value * 32767.0f)));
}

std::span<const std::uint32_t> Program(const Color& color, std::uint32_t exportFormat, std::optional<std::uint32_t> sampleMask = std::nullopt, std::optional<std::uint16_t> packedAlpha = std::nullopt) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    const auto move = [&](std::uint32_t vgpr, std::uint32_t value) {
        code[at++] = 0x7e0002ffu | (vgpr << 17u);
        code[at++] = value;
    };
    if (sampleMask) {
        move(8, *sampleMask);
        code[at++] = 0xf8001084u;
        code[at++] = 0x00080000u;
    }
    if (exportFormat != Float32Export) {
        move(4, Packed16(color[0], exportFormat) | (Packed16(color[1], exportFormat) << 16u));
        move(5, Packed16(color[2], exportFormat) | ((packedAlpha ? *packedAlpha : Packed16(color[3], exportFormat)) << 16u));
        code[at++] = 0xf8001c0fu;
        code[at++] = 0x00000504u;
    } else {
        for (std::uint32_t channel = 0; channel < 4; ++channel) move(4 + channel, std::bit_cast<std::uint32_t>(color[channel]));
        code[at++] = 0xf800180fu;
        code[at++] = 0x07060504u;
    }
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t samples = 1) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float16x4Info;
    const auto log2 = static_cast<std::uint32_t>(std::countr_zero(samples));
    cx[0x31d] = (log2 << 12u) | (log2 << 15u);
    cx[0x321] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.elementBytes == 8 && color.samples == samples && color.bytes * samples == block.bytes, "the 16_16_16_16 float target did not decode as a 256x128 RGBA16F surface");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, bool alphaToCoverage, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode, bool dithered = false, bool sampleMaskExport = false, std::uint8_t ditheredSamples = 8) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
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
    state.alphaToCoverage = alphaToCoverage;
    state.ditheredAlphaToCoverage = dithered;
    state.ditheredAlphaSamples = ditheredSamples;
    state.samples = color.samples;

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, sampleMaskExport ? 0x100u : 0u}, {0x1c5, exportFormat}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    auto prepared = AgcDriver::Graphics::DecodePixelStageInfo(context, {});
    prepared.alphaToCoverage = alphaToCoverage;
    prepared.ditheredAlphaToCoverage = dithered;
    prepared.ditheredAlphaSamples = ditheredSamples;
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, prepared, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->alphaToCoverage = state.alphaToCoverage;
    fragment.context.pixel->ditheredAlphaToCoverage = state.ditheredAlphaToCoverage;
    fragment.context.pixel->ditheredAlphaSamples = state.ditheredAlphaSamples;
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

alignas(256) std::array<std::array<std::uint32_t, 9>, 3> DepthOnlyPrograms{};

void DepthOnlyCoverage(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& harness, const Block& depth, float alpha, std::size_t slot, bool written, std::uint32_t enable = 0x3u, float fourth = 0.0f) {
    namespace Harness = DepthFastClearHarness;
    const Harness::Surface surface{depth.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    auto& code = DepthOnlyPrograms[slot];
    code = {0x7e0802ffu, 0x3e800000u, 0x7e0a02ffu, std::bit_cast<std::uint32_t>(alpha), 0x7e0e02ffu, std::bit_cast<std::uint32_t>(fourth), 0xf8001800u | enable, 0x07060504u, 0xbf810000u};
    const std::span<const std::uint32_t> pixelCode(code.data(), 9);
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 3;
    pixel.targetExportMapping.fill(0xe4u);
    pixel.alphaToCoverage = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {32, 0, {}, std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(),
        PixelPushLayout(harness.vertexPush, device.Target())
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &harness.vertex, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(harness.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.depth = AgcDriver::Graphics::DepthTarget{surface.depth, 0, {Harness::Width, Harness::Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0, false};
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
    state.alphaToCoverage = true;
    state.renderExtent = {Harness::Width, Harness::Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Harness::Height), static_cast<float>(Harness::Width), -static_cast<float>(Harness::Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Harness::Width, Harness::Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Harness::Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    const auto passed = Harness::Draw(device, harness, surface, {.depthCompare = VK_COMPARE_OP_LESS});
    char detail[160];
    std::snprintf(detail, sizeof(detail), "a depth-only draw with alpha-to-coverage and a 32_AR alpha of %.2f (export mask 0x%x, fourth channel %.2f) left %u of %u pixels at the cleared depth", alpha, enable, fourth, passed, Harness::Covered);
    Require(passed == (written ? 0u : Harness::Covered), detail);
}

std::vector<std::uint16_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint16_t> stored(block.bytes / 2u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::vector<std::uint16_t> Fill(Block& block, const ColorTargetLayout& layout) {
    std::vector<std::uint16_t> halves(block.bytes / 2u);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto& color = Destinations[(x + y) % Destinations.size()];
            const auto index = layout.Offset(x, y) / 2u;
            for (std::uint32_t channel = 0; channel < 4; ++channel) halves[index + channel] = Half(color[channel]);
        }
    }
    std::memcpy(block.data, halves.data(), block.bytes);
    return halves;
}

void Check(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint16_t>& before, const std::function<Color(const Color&)>& expectedColor, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 2u;
            const auto expected = expectedColor(Destinations[(x + y) % Destinations.size()]);
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                const bool inside = x < ScissorWidth;
                const bool match = inside ? std::fabs(FromHalf(stored[index + channel]) - expected[channel]) <= 1.0f / 2048.0f : stored[index + channel] == before[index + channel];
                if (match) continue;
                char detail[160];
                std::snprintf(detail, sizeof(detail), ": channel %u of pixel (%u, %u) %s the scissor holds half 0x%04x, expected %.6g", channel, x, y, inside ? "inside" : "outside", stored[index + channel], inside ? expected[channel] : FromHalf(before[index + channel]));
                throw std::runtime_error(what + detail);
            }
        }
    }
}

struct DitherCase {
    std::uint32_t bits;
    std::uint32_t format;
    std::array<std::uint32_t, 4> masks;
};

void DitheredCoverage(AgcDriver::VulkanDevice& device, std::uint32_t samples, std::span<const DitherCase> cases) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
    std::vector<std::unique_ptr<Block>> blocks;
    for (const auto& test : cases) {
        blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
        auto& source = *blocks.back();
        std::memset(source.data, 0, source.bytes);
        const auto multisampled = DecodeTarget(source, samples);
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            const Color color{std::ldexp(1.0f, static_cast<int>(sample) - static_cast<int>(samples - 1u)), 0, 0, 1};
            Draw(device, multisampled, false, Float32Export, Program(color, Float32Export, 1u << sample), false, true);
        }
        const auto alpha = test.format == Float32Export ? std::bit_cast<float>(test.bits) : 0.0f;
        const Color color{0, 0, 0, alpha};
        const auto half = test.format == Float16Export ? std::optional<std::uint16_t>(static_cast<std::uint16_t>(test.bits)) : std::nullopt;
        Draw(device, multisampled, true, test.format, Program(color, test.format, std::nullopt, half), true, false, static_cast<std::uint8_t>(samples));
        blocks.push_back(std::make_unique<Block>(layout.Bytes()));
        auto& resolved = *blocks.back();
        std::memset(resolved.data, 0, resolved.bytes);
        AgcDriver::Graphics::ColorMetadataPass pass{};
        pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
        pass.source = multisampled;
        pass.targets = {DecodeTarget(resolved)};
        device.ColorMetadataPass(pass);
        const auto stored = ReadBack(device, resolved);
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto mask = test.masks[2u * (x & 1u) + (y & 1u)];
                const auto expected = x < ScissorWidth ? static_cast<float>((1u << samples) - 1u - mask) / static_cast<float>((1u << (samples - 1u)) * samples) : 0.0f;
                const auto actual = FromHalf(stored[layout.Offset(x, y) / 2u]);
                if (actual == expected) continue;
                char detail[192];
                std::snprintf(detail, sizeof(detail), "dithered coverage export %u alpha 0x%08x pixel (%u,%u): resolved %.9g, expected %.9g for mask0x%02x", test.format, test.bits, x, y, actual, expected, mask);
                throw std::runtime_error(detail);
            }
        }
        programCount = 0;
    }
    AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
}

constexpr std::array<DitherCase, 36> EightSampleDitherCases{{
    {0x00000000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x80000000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x3c7fffffu, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x3c800000u, 9u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x3c800001u, 9u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x3cffffffu, 9u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x3d000000u, 9u, {0x04u, 0x00u, 0x00u, 0x00u}},
    {0x3d000001u, 9u, {0x04u, 0x00u, 0x00u, 0x00u}},
    {0x3effffffu, 9u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x3f000000u, 9u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x3f000001u, 9u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x3f77ffffu, 9u, {0xffu, 0xf7u, 0xffu, 0xffu}},
    {0x3f780000u, 9u, {0xffu, 0x7fu, 0xffu, 0xffu}},
    {0x3f780001u, 9u, {0xffu, 0x7fu, 0xffu, 0xffu}},
    {0x3f800000u, 9u, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x7f800000u, 9u, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0xff800000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x7fc00000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00000000u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00008000u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x000023ffu, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00002400u, 4u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x00002401u, 4u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x000027ffu, 4u, {0x10u, 0x00u, 0x00u, 0x00u}},
    {0x00002800u, 4u, {0x04u, 0x00u, 0x00u, 0x00u}},
    {0x00002801u, 4u, {0x04u, 0x00u, 0x00u, 0x00u}},
    {0x000037ffu, 4u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x00003800u, 4u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x00003801u, 4u, {0x35u, 0x36u, 0x36u, 0x35u}},
    {0x00003bbfu, 4u, {0xffu, 0xf7u, 0xffu, 0xffu}},
    {0x00003bc0u, 4u, {0xffu, 0x7fu, 0xffu, 0xffu}},
    {0x00003bc1u, 4u, {0xffu, 0x7fu, 0xffu, 0xffu}},
    {0x00003c00u, 4u, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x00007c00u, 4u, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x0000fc00u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00007e00u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
}};

constexpr std::array<DitherCase, 56> TwoSampleDitherCases{{
    {0x00000000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x80000000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x3dffffffu, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x3e000000u, 9u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x3e000001u, 9u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x3e7fffffu, 9u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x3e800000u, 9u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x3e800001u, 9u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x3ebfffffu, 9u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x3ec00000u, 9u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x3ec00001u, 9u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x3effffffu, 9u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x3f000000u, 9u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x3f000001u, 9u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x3f1fffffu, 9u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x3f200000u, 9u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x3f200001u, 9u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x3f3fffffu, 9u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x3f400000u, 9u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x3f400001u, 9u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x3f5fffffu, 9u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x3f600000u, 9u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x3f600001u, 9u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x3f800000u, 9u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x7f800000u, 9u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0xff800000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x7fc00000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x40000000u, 9u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0xbf800000u, 9u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00000000u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00008000u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00002fffu, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00003000u, 4u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x00003001u, 4u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x000033ffu, 4u, {0x02u, 0x00u, 0x00u, 0x02u}},
    {0x00003400u, 4u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x00003401u, 4u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x000035ffu, 4u, {0x01u, 0x02u, 0x00u, 0x01u}},
    {0x00003600u, 4u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x00003601u, 4u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x000037ffu, 4u, {0x01u, 0x02u, 0x02u, 0x01u}},
    {0x00003800u, 4u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x00003801u, 4u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x000038ffu, 4u, {0x01u, 0x01u, 0x02u, 0x01u}},
    {0x00003900u, 4u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x00003901u, 4u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x000039ffu, 4u, {0x03u, 0x01u, 0x02u, 0x03u}},
    {0x00003a00u, 4u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x00003a01u, 4u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x00003affu, 4u, {0x03u, 0x03u, 0x01u, 0x03u}},
    {0x00003b00u, 4u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x00003b01u, 4u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x00003c00u, 4u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x00007c00u, 4u, {0x03u, 0x03u, 0x03u, 0x03u}},
    {0x0000fc00u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00007e00u, 4u, {0x00u, 0x00u, 0x00u, 0x00u}},
}};

void DccSrgbClearTests(AgcDriver::VulkanDevice& device, std::uint32_t samples) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
    const ColorTargetLayout allocation(Width, Height, ColorTileMode::RenderTarget, 8);
    std::vector<std::unique_ptr<Block>> blocks;
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
    } clearTargets{device};
    const auto targetFor = [&](const Block& block, std::uint32_t count, VkFormat format) {
        auto target = DecodeTarget(block, count);
        target.format = format;
        target.elementBytes = 4;
        target.bytes = layout.Bytes();
        return target;
    };
    for (const auto format : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB}) {
        for (const auto key : {0x00u, 0x40u, 0x80u, 0xc0u}) {
            for (const auto alphaOnMsb : {false, true}) {
                blocks.push_back(std::make_unique<Block>(allocation.Bytes() * samples));
                auto& source = *blocks.back();
                std::memset(source.data, 0x5a, source.bytes);
                blocks.push_back(std::make_unique<Block>(BlockBytes));
                auto& metadata = *blocks.back();
                std::memset(metadata.data, key, metadata.bytes);
                auto target = targetFor(source, samples, format);
                target.dccAddress = metadata.Address();
                target.dccPipeAligned = true;
                target.dccAlphaOnMsb = alphaOnMsb;
                Draw(device, target, false, Float32Export, Program(Color{0.25f, 0.5f, 0.75f, 0.125f}, Float32Export, 0u), false, true);
                blocks.push_back(std::make_unique<Block>(allocation.Bytes()));
                auto& resolved = *blocks.back();
                std::memset(resolved.data, 0xa5, resolved.bytes);
                AgcDriver::Graphics::ColorMetadataPass pass{};
                pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
                pass.source = target;
                pass.targets = {targetFor(resolved, 1u, format)};
                device.ColorMetadataPass(pass);
                const auto stored = ReadBack(device, resolved);
                const auto bytes = std::as_bytes(std::span(stored));
                std::array<std::byte, 4> expected{};
                const auto keys = key == 0x00u ? AgcDriver::Graphics::DccKeys::Clear0000 : key == 0x40u ? AgcDriver::Graphics::DccKeys::Clear0001 : key == 0x80u ? AgcDriver::Graphics::DccKeys::Clear1110 : AgcDriver::Graphics::DccKeys::Clear1111;
                Require(AgcDriver::Graphics::FillDccClear(format, keys, alphaOnMsb, expected), "the sRGB DCC expected texel was not encoded");
                for (std::uint32_t y = 0; y < Height; ++y) {
                    for (std::uint32_t x = 0; x < Width; ++x) {
                        for (std::uint32_t channel = 0; channel < 4; ++channel) Require(bytes[layout.Offset(x, y) + channel] == expected[channel], "an sRGB DCC import or resolve changed an undrawn sample endpoint");
                    }
                }
            }
        }
    }
}

void DccFastClearTests(AgcDriver::VulkanDevice& device, std::uint32_t selectedSamples) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
    const auto expectRange = [](std::string_view message, const char* plane, std::uint64_t address, std::size_t bytes, std::uint32_t samples) {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "(%s range 0x%llx+0x%zx, %u samples)", plane, static_cast<unsigned long long>(address), bytes, samples);
        Require(message.find(detail) != std::string_view::npos, "a resident multisampled DCC refusal did not identify the affected plane, range and sample count");
    };
    std::vector<std::unique_ptr<Block>> blocks;
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
    } clearTargets{device};
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        for (const auto code : {0x00u, 0x40u, 0x80u, 0xc0u}) {
            for (const auto alphaOnMsb : {false, true}) {
                for (const auto cmaskMode : {0u, 0xccu, 0xffu}) {
                    blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
                    auto& source = *blocks.back();
                    std::memset(source.data, 0, source.bytes);
                    blocks.push_back(std::make_unique<Block>(BlockBytes));
                    auto& metadata = *blocks.back();
                    std::memset(metadata.data, code, BlockBytes);
                    auto target = DecodeTarget(source, samples);
                    target.dccAddress = metadata.Address();
                    target.dccPipeAligned = true;
                    target.dccAlphaOnMsb = alphaOnMsb;
                    if (cmaskMode != 0) {
                        blocks.push_back(std::make_unique<Block>(BlockBytes));
                        auto& cmask = *blocks.back();
                        const auto expanded = samples == 2 ? 0xddu : samples == 4 ? 0xeeu : 0xffu;
                        std::memset(cmask.data, cmaskMode == 0xccu ? cmaskMode : expanded, BlockBytes);
                        target.cmaskAddress = cmask.Address();
                        target.cmaskBytes = AgcDriver::Graphics::CmaskBytes(Width, Height);
                        target.cmaskFastClear = true;
                    }
                    const bool colorOne = code == 0x80u || code == 0xc0u;
                    const bool alphaOne = code == 0x40u || code == 0xc0u;
                    Color initial{};
                    for (std::uint32_t channel = 0; channel < 4; ++channel) initial[channel] = channel == (alphaOnMsb ? 3u : 0u) ? static_cast<float>(alphaOne) : static_cast<float>(colorOne);
                    const Color written{0.25f, 0.25f, 0.25f, 0.25f};
                    if (cmaskMode != 0) device.ColorMetadataPass({AgcDriver::Graphics::ColorMetadataPass::Mode::FmaskDecompress, {target}});
                    Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
                    if (cmaskMode != 0) {
                        auto recolored = target;
                        recolored.clearWords = {0x3c003c00u, 0x3c003c00u};
                        device.ColorMetadataPass({AgcDriver::Graphics::ColorMetadataPass::Mode::FmaskDecompress, {recolored}});
                    }
                    blocks.push_back(std::make_unique<Block>(layout.Bytes()));
                    auto& resolved = *blocks.back();
                    std::memset(resolved.data, 0, resolved.bytes);
                    AgcDriver::Graphics::ColorMetadataPass pass{};
                    pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
                    pass.source = target;
                    pass.targets = {DecodeTarget(resolved)};
                    device.ColorMetadataPass(pass);
                    const auto stored = ReadBack(device, resolved);
                    for (std::uint32_t y = 0; y < Height; ++y) {
                        for (std::uint32_t x = 0; x < Width; ++x) {
                            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                                const auto expected = x < ScissorWidth ? initial[channel] + (written[channel] - initial[channel]) / static_cast<float>(samples) : initial[channel];
                                const auto actual = FromHalf(stored[layout.Offset(x, y) / 2u + channel]);
                                Require(actual == expected, "multisampled DCC clear did not preserve undrawn samples, channels or scissor pixels");
                            }
                        }
                    }
                    if (code == 0 && !alphaOnMsb && cmaskMode == 0) {
                        const auto dccBytes = AgcDriver::Graphics::MultisampledDccBytes(target);
                        Require(dccBytes + 8u <= metadata.bytes, "the DCC fixture has no adjacent-page control");
                        const std::array<std::byte, 4> adjacent{std::byte{0x7b}, std::byte{0x7b}, std::byte{0x7b}, std::byte{0x7b}};
                        AgcDriver::GuestMemory::Write(metadata.Address() + dccBytes, adjacent);
                        device.ColorMetadataPass(pass);
                        Require(ReadBack(device, resolved) == stored, "an adjacent driver store changed the resident DCC target");
                        static_cast<volatile std::uint8_t*>(metadata.data)[dccBytes + 4u] = 0x7c;
                        AgcDriver::GuestMemory::CollectWritesUncached(metadata.Address() + dccBytes, 8u);
                        device.ColorMetadataPass(pass);
                        Require(ReadBack(device, resolved) == stored, "an adjacent CPU store changed the resident DCC target");
                        const std::array<std::byte, 1> same{std::byte{0}};
                        AgcDriver::GuestMemory::Write(metadata.Address(), same);
                        bool refused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            refused = std::string_view(error.what()).find("writes to a resident multisampled DCC") != std::string_view::npos;
                        }
                        Require(refused, "a same-value driver DCC store did not invalidate multisampled DCC residency");
                    }
                    if (code == 0 && alphaOnMsb && cmaskMode == 0) {
                        static_cast<volatile std::uint8_t*>(source.data)[0] = 0;
                        bool refused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            refused = std::string_view(error.what()).find("writes to a resident multisampled DCC") != std::string_view::npos;
                            if (refused) expectRange(error.what(), "color", source.Address(), AgcDriver::Graphics::MultisampledColorBytes(target), samples);
                        }
                        Require(refused, "a same-value CPU color store did not invalidate multisampled DCC residency");
                    }
                    if (code == 0x40u && alphaOnMsb && cmaskMode == 0) {
                        static_cast<volatile std::uint8_t*>(metadata.data)[0] = 0x40u;
                        bool refused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            refused = std::string_view(error.what()).find("writes to a resident multisampled DCC") != std::string_view::npos;
                            if (refused) expectRange(error.what(), "DCC", metadata.Address(), AgcDriver::Graphics::MultisampledDccBytes(target), samples);
                        }
                        Require(refused, "a same-value CPU DCC store did not invalidate multisampled DCC residency");
                    }
                    if (code == 0x40u && alphaOnMsb && cmaskMode == 0xccu) {
                        reinterpret_cast<volatile std::uint8_t*>(target.cmaskAddress)[0] = 0xccu;
                        bool refused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            refused = std::string_view(error.what()).find("writes to a resident multisampled DCC") != std::string_view::npos;
                            if (refused) expectRange(error.what(), "CMASK", target.cmaskAddress, target.cmaskBytes, samples);
                        }
                        Require(refused, "a same-value CPU CMASK store did not invalidate multisampled DCC residency");
                    }
                    if (code == 0xc0u && alphaOnMsb && cmaskMode == 0) {
                        AgcDriver::GuestMemory::Unwatch(metadata.Address(), 4096);
                        bool refused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            refused = std::string_view(error.what()).find("reuse requires watched guest memory") != std::string_view::npos;
                            if (refused) expectRange(error.what(), "DCC", metadata.Address(), AgcDriver::Graphics::MultisampledDccBytes(target), samples);
                        }
                        Require(refused, "an unwatched metadata range remained eligible for multisampled DCC reuse");
                    }
                    if (code == 0x80u && alphaOnMsb && cmaskMode == 0) {
                        {
                            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                            device.SubmitRecorded(true);
                            device.WaitIdle();
                        }
                        {
                            GuestAllocations::Mutation mutation;
                            mutation.Remove(metadata.data);
                        }
                        {
                            GuestAllocations::Mutation mutation;
                            mutation.Add(metadata.data, metadata.bytes, true, true, true);
                        }
                        bool readRefused = false;
                        try {
                            AgcDriver::Graphics::RequireNoMultisampleTarget(source.Address(), source.bytes, "a CPU read");
                        } catch (const std::runtime_error&) {
                            readRefused = true;
                        }
                        Require(readRefused, "releasing DCC metadata removed the unpublished color read guard");
                        bool reuseRefused = false;
                        try {
                            device.ColorMetadataPass(pass);
                        } catch (const std::runtime_error& error) {
                            reuseRefused = std::string_view(error.what()).find("metadata was released") != std::string_view::npos;
                        }
                        Require(reuseRefused, "recreated DCC metadata reused a resident color target");
                    }
                    programCount = 0;
                }
            }
        }
    }
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
        auto& source = *blocks.back();
        std::memset(source.data, 0, source.bytes);
        blocks.push_back(std::make_unique<Block>(BlockBytes));
        auto& metadata = *blocks.back();
        std::memset(metadata.data, 0, BlockBytes);
        auto first = DecodeTarget(source, samples);
        first.dccAddress = metadata.Address();
        first.dccPipeAligned = true;
        const Color written{0.25f, 0.25f, 0.25f, 0.25f};
        Draw(device, first, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        const std::vector<std::byte> ones(AgcDriver::Graphics::MultisampledDccBytes(first), std::byte{0xc0});
        AgcDriver::GuestMemory::Write(metadata.Address(), ones);
        auto second = first;
        second.pipeBankXor = 0x4000u;
        Draw(device, second, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        blocks.push_back(std::make_unique<Block>(layout.Bytes()));
        auto& resolved = *blocks.back();
        std::memset(resolved.data, 0, resolved.bytes);
        AgcDriver::Graphics::ColorMetadataPass pass{};
        pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
        pass.source = second;
        pass.targets = {DecodeTarget(resolved)};
        device.ColorMetadataPass(pass);
        const auto stored = ReadBack(device, resolved);
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const auto expected = x < ScissorWidth ? 1.0f + (written[channel] - 1.0f) / static_cast<float>(samples) : 1.0f;
                    Require(FromHalf(stored[layout.Offset(x, y) / 2u + channel]) == expected, "a multisampled DCC target recreated with another pipe-bank XOR did not start from its new fast clear");
                }
            }
        }
        auto third = second;
        third.dccAlphaOnMsb = true;
        bool refused = false;
        try {
            Draw(device, third, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        } catch (const std::runtime_error& error) {
            refused = std::string_view(error.what()).find("layout or metadata changed") != std::string_view::npos;
        }
        Require(refused, "a resident multisampled DCC target accepted a use with other DCC metadata");
        programCount = 0;
    }
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        for (const auto mode : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}) {
            blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
            auto& source = *blocks.back();
            std::memset(source.data, 0, source.bytes);
            blocks.push_back(std::make_unique<Block>(BlockBytes));
            auto& metadata = *blocks.back();
            std::memset(metadata.data, mode == 0 ? 0xff : 0, metadata.bytes);
            auto target = DecodeTarget(source, samples);
            target.dccAddress = metadata.Address();
            target.dccPipeAligned = true;
            std::string_view expected = "not a supported uniform fast clear";
            if (mode == 1) {
                metadata.data[0] = 0x40u;
                expected = "mixed multisampled DCC metadata";
            }
            if (mode == 2 || mode == 3 || mode == 7) {
                blocks.push_back(std::make_unique<Block>(BlockBytes));
                auto& cmask = *blocks.back();
                std::memset(cmask.data, mode == 2 ? 0 : 0xcc, cmask.bytes);
                if (mode == 3) cmask.data[0] = 0xdd;
                target.cmaskAddress = cmask.Address();
                target.cmaskBytes = AgcDriver::Graphics::CmaskBytes(Width, Height);
                target.cmaskFastClear = true;
                expected = "CMASK compression or register clear";
            }
            if (mode == 4) {
                AgcDriver::GuestMemory::Unwatch(metadata.Address(), 4096);
                expected = "initial multisampled DCC contents require watched";
            }
            if (mode == 5) {
                target.dccAddress = source.Address();
                expected = "overlapping multisampled DCC";
            }
            if (mode == 6) {
                target.dccAddress += 256u;
                expected = "metadata base alignment";
            }
            if (mode == 7) {
                target.dccControl = 0x80u;
                expected = "color transform, lossy mode";
            }
            bool refused = false;
            try {
                if (mode == 7) device.ColorMetadataPass({AgcDriver::Graphics::ColorMetadataPass::Mode::FmaskDecompress, {target}});
                else Draw(device, target, false, Float32Export, Program({0.25f, 0.25f, 0.25f, 0.25f}, Float32Export));
            } catch (const std::runtime_error& error) {
                refused = std::string_view(error.what()).find(expected) != std::string_view::npos;
            }
            Require(refused, "unsupported initial multisampled DCC contents were not refused explicitly");
            programCount = 0;
        }
    }
}

}

void DccRefillTests(AgcDriver::VulkanDevice& device, std::uint32_t selectedSamples) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
    std::vector<std::unique_ptr<Block>> blocks;
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
    } clearTargets{device};
#ifndef _WIN32
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
    struct Restore {
        const AgcDriver::Graphics::Context& context;
        AgcDriver::Graphics::ImportWatch decided;
        ~Restore() { AgcDriver::Graphics::SetImportWatch(context, decided); }
    } restore{context, decided};
#endif
    const auto fill = [](const Block& block, std::size_t bytes, std::uint32_t pattern) {
        for (std::size_t offset = 0; offset + sizeof(pattern) <= bytes; offset += sizeof(pattern)) std::memcpy(block.data + offset, &pattern, sizeof(pattern));
        AgcDriver::Graphics::NoteColorMetadataFill(block.Address(), bytes, pattern);
    };
    const Color written{0.25f, 0.25f, 0.25f, 0.25f};
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        for (const auto mode : {0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
            for (const auto cmasked : {false, true}) {
                if (mode == 4 && !cmasked) continue;
#ifdef _WIN32
                if (mode == 6) continue;
#else
                AgcDriver::Graphics::SetImportWatch(context, mode == 5 ? AgcDriver::Graphics::ImportWatch::Watch : mode == 6 ? AgcDriver::Graphics::ImportWatch::Unwatch : decided);
#endif
                blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
                auto& source = *blocks.back();
                std::memset(source.data, 0, source.bytes);
                blocks.push_back(std::make_unique<Block>(BlockBytes));
                auto& metadata = *blocks.back();
                std::memset(metadata.data, 0, BlockBytes);
                auto target = DecodeTarget(source, samples);
                target.dccAddress = metadata.Address();
                target.dccPipeAligned = true;
                target.dccAlphaOnMsb = true;
                Block* cmask = nullptr;
                if (cmasked) {
                    blocks.push_back(std::make_unique<Block>(BlockBytes));
                    cmask = blocks.back().get();
                    std::memset(cmask->data, 0xcc, BlockBytes);
                    target.cmaskAddress = cmask->Address();
                    target.cmaskBytes = AgcDriver::Graphics::CmaskBytes(Width, Height);
                    target.cmaskFastClear = true;
                    device.ColorMetadataPass({AgcDriver::Graphics::ColorMetadataPass::Mode::FmaskDecompress, {target}});
                }
                Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
                const auto dccBytes = AgcDriver::Graphics::MultisampledDccBytes(target);
                std::string_view expected;
                bool rewatchable = false;
                if (mode == 0) {
                    fill(metadata, dccBytes, 0xc0c0c0c0u);
                    if (cmask != nullptr) fill(*cmask, target.cmaskBytes, 0xccccccccu);
                } else if (mode == 1) {
                    fill(metadata, dccBytes, 0xc0c0c0c0u);
                } else if (mode >= 5) {
                    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                    const auto record = [&](const Block& block, std::size_t bytes, std::uint32_t value) {
                        const std::array<std::uint32_t, 4> pattern{value, value, value, value};
                        Require(device.FillBuffer(block.Address(), bytes, pattern), "the metadata fill was not recorded on the GPU");
                        AgcDriver::Graphics::NoteColorMetadataFill(block.Address(), bytes, value);
                    };
                    record(metadata, dccBytes, 0xc0c0c0c0u);
                    if (cmask != nullptr) record(*cmask, target.cmaskBytes, 0xccccccccu);
                    const bool unwatched = !AgcDriver::GuestMemory::Watched(metadata.Address(), dccBytes) || (cmask != nullptr && !AgcDriver::GuestMemory::Watched(cmask->Address(), target.cmaskBytes));
#ifdef _WIN32
                    if (unwatched) expected = "requires watched guest memory";
                    rewatchable = unwatched;
#else
                    Require(unwatched == (mode == 6), "a GPU fill of multisampled DCC metadata did not take the write-watch mode of its host import");
#endif
                } else if (mode == 2) {
                    fill(metadata, dccBytes / 2u, 0xc0c0c0c0u);
                    expected = "writes to a resident multisampled DCC";
                } else if (mode == 3) {
                    fill(metadata, dccBytes, 0xffffffffu);
                    expected = "not a supported uniform fast clear";
                } else {
                    fill(*cmask, target.cmaskBytes, 0xccccccccu);
                    expected = "writes to a resident multisampled DCC";
                }
                bool drawn = false;
                if (!expected.empty()) {
                    bool refused = false;
                    try {
                        Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
                        drawn = true;
                    } catch (const std::runtime_error& error) {
                        refused = std::string_view(error.what()).find(expected) != std::string_view::npos;
                    }
                    Require(refused || (drawn && rewatchable), "a write to resident multisampled DCC metadata that is not a full fast-clear fill was not refused");
                    if (refused) {
                        programCount = 0;
                        continue;
                    }
                }
                if (!drawn) Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
                blocks.push_back(std::make_unique<Block>(layout.Bytes()));
                auto& resolved = *blocks.back();
                std::memset(resolved.data, 0, resolved.bytes);
                AgcDriver::Graphics::ColorMetadataPass pass{};
                pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
                pass.source = target;
                pass.targets = {DecodeTarget(resolved)};
                device.ColorMetadataPass(pass);
                const auto stored = ReadBack(device, resolved);
                for (std::uint32_t y = 0; y < Height; ++y) {
                    for (std::uint32_t x = 0; x < Width; ++x) {
                        for (std::uint32_t channel = 0; channel < 4; ++channel) {
                            const auto expectedValue = x < ScissorWidth ? 1.0f + (written[channel] - 1.0f) / static_cast<float>(samples) : 1.0f;
                            const auto actual = FromHalf(stored[layout.Offset(x, y) / 2u + channel]);
                            Require(actual == expectedValue, "a full DCC fast-clear fill of a resident multisampled target did not clear it to the new color before the next draw");
                        }
                    }
                }
                programCount = 0;
            }
        }
    }
}

void DccImportWindowTests(AgcDriver::VulkanDevice& device, std::uint32_t selectedSamples) {
    if (!AgcDriver::GuestMemory::WriteWatched()) return;
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
    std::vector<std::unique_ptr<Block>> blocks;
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
    } clearTargets{device};
    const Color written{0.25f, 0.25f, 0.25f, 0.25f};
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
        auto& source = *blocks.back();
        std::memset(source.data, 0, source.bytes);
        blocks.push_back(std::make_unique<Block>(BlockBytes));
        auto& metadata = *blocks.back();
        std::memset(metadata.data, 0, BlockBytes);
        auto target = DecodeTarget(source, samples);
        target.dccAddress = metadata.Address();
        target.dccPipeAligned = true;
        target.dccAlphaOnMsb = true;
        Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        Require(AgcDriver::GuestMemory::ImportWatched(metadata.Address(), BlockBytes, [&] {
            auto* first = reinterpret_cast<volatile std::uint8_t*>(metadata.data);
            *first = *first;
            return true;
        }), "the import over resident multisampled DCC metadata failed");
        Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        Require(AgcDriver::GuestMemory::ImportWatched(metadata.Address(), BlockBytes, [&] {
            auto* first = reinterpret_cast<volatile std::uint8_t*>(metadata.data);
            *first = static_cast<std::uint8_t>(*first ^ 0xffu);
            return true;
        }), "the second import over resident multisampled DCC metadata failed");
        bool refused = false;
        try {
            Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
        } catch (const std::runtime_error& error) {
            refused = std::string_view(error.what()).find("writes to a resident multisampled DCC") != std::string_view::npos;
        }
        Require(refused, "a store of new bytes to resident multisampled DCC metadata within an import window was not refused");
        programCount = 0;
    }
}

#ifndef _WIN32
const char* DccComparedImportTests(AgcDriver::VulkanDevice& device, std::uint32_t selectedSamples) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (context.hostImportAlignment == 0) return "skipped, the device imports no host memory, so no multisampled DCC plane lies in a compared host import";
    Require(AgcDriver::GuestMemory::WriteWatched(), "the guest arena is not write-watched");
    const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
    if (decided != AgcDriver::Graphics::ImportWatch::Unwatch) return "skipped, the device reports host imports that keep write protection, so the write-watch probe compares none of them";
    struct Restore {
        const AgcDriver::Graphics::Context& context;
        AgcDriver::Graphics::ImportWatch decided;
        ~Restore() { AgcDriver::Graphics::SetImportWatch(context, decided); }
    } restore{context, decided};
    const auto probe = AgcDriver::Graphics::ProbeImportWriteProtection(context);
    const bool watchedImportsHold = probe.failure == nullptr && probe.writtenAtImport == 0 && probe.writtenAfterSubmit == 0;
    if (!watchedImportsHold) std::cout << "the watched-import control case is skipped: the device reports watched host memory written when it imports or reads it\n";
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
    std::vector<std::unique_ptr<Block>> blocks;
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(device.Device());
            AgcDriver::Graphics::ClearCachedTextures(device.Device());
        }
    } clearTargets{device};
    const auto expectRange = [](std::string_view message, const char* plane, std::uint64_t address, std::size_t bytes, std::uint32_t samples) {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "(%s range 0x%llx+0x%zx, %u samples)", plane, static_cast<unsigned long long>(address), bytes, samples);
        Require(message.find(detail) != std::string_view::npos, "a refusal over a compared host import did not identify the affected plane, range and sample count");
    };
    const Color written{0.25f, 0.25f, 0.25f, 0.25f};
    const Color initial{0.0f, 0.0f, 0.0f, 1.0f};
    constexpr std::string_view deviceLimit = "AGC graphics: the device cannot render color with the target's sample count";
    for (const auto samples : {2u, 4u, 8u}) {
        if (selectedSamples != 0 && samples != selectedSamples) continue;
        for (const auto mode : {7u, 8u, 9u, 10u, 11u, 0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
            if (mode >= 7 && samples != 8) continue;
            if (mode == 5 && !watchedImportsHold) continue;
            const bool watchedImports = mode == 5;
            AgcDriver::Graphics::SetImportWatch(context, watchedImports ? AgcDriver::Graphics::ImportWatch::Watch : decided);
            blocks.push_back(std::make_unique<Block>(layout.Bytes() * samples));
            auto& source = *blocks.back();
            std::memset(source.data, 0, source.bytes);
            blocks.push_back(std::make_unique<Block>(BlockBytes));
            auto& metadata = *blocks.back();
            std::memset(metadata.data, 0x40, metadata.bytes);
            blocks.push_back(std::make_unique<Block>(BlockBytes));
            auto& cmask = *blocks.back();
            std::memset(cmask.data, 0xcc, cmask.bytes);
            auto target = DecodeTarget(source, samples);
            target.dccAddress = metadata.Address();
            target.dccPipeAligned = true;
            target.dccAlphaOnMsb = true;
            target.cmaskAddress = cmask.Address();
            target.cmaskBytes = AgcDriver::Graphics::CmaskBytes(Width, Height);
            target.cmaskFastClear = true;
            Block* fmask = nullptr;
            if (mode >= 7) {
                blocks.push_back(std::make_unique<Block>(2u * BlockBytes));
                fmask = blocks.back().get();
                std::memset(fmask->data, 0x11, fmask->bytes);
                target.fmaskAddress = fmask->Address();
                target.fmaskPipeBankXor = 0;
            }
            const std::array<std::pair<const Block*, std::size_t>, 3> planes{{{&source, static_cast<std::size_t>(AgcDriver::Graphics::MultisampledColorBytes(target))}, {&metadata, AgcDriver::Graphics::MultisampledDccBytes(target)}, {&cmask, static_cast<std::size_t>(target.cmaskBytes)}}};
            if (mode == 4) {
                AgcDriver::GuestMemory::Unwatch(source.Address(), source.bytes);
            } else if (mode == 11) {
                AgcDriver::GuestMemory::Unwatch(fmask->Address(), fmask->bytes);
            } else {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                for (std::size_t plane = 0; plane < planes.size(); ++plane) {
                    if ((mode == 6 && plane != 1) || mode == 8) continue;
                    const auto* import = AgcDriver::Graphics::HostImportFor(context, planes[plane].first->Address(), planes[plane].first->bytes);
                    Require(import != nullptr, "the host import of a multisampled DCC plane failed");
                    Require(import->unwatched != watchedImports && AgcDriver::GuestMemory::Watched(planes[plane].first->Address(), planes[plane].second) == watchedImports, "a host import of a multisampled DCC plane did not take the write-watch mode the test set");
                }
                if (fmask != nullptr) {
                    const auto* import = AgcDriver::Graphics::HostImportFor(context, fmask->Address(), fmask->bytes);
                    Require(import != nullptr && import->unwatched && !AgcDriver::GuestMemory::Watched(fmask->Address(), fmask->bytes), "the host import of the FMASK plane was not compared");
                }
                if (mode == 8) {
                    for (const auto& [block, bytes] : planes) Require(AgcDriver::GuestMemory::Watched(block->Address(), bytes), "the FMASK plane's import took a color or metadata plane out of the write watch");
                }
            }
            const char* fmaskCase = mode == 7 ? "color, DCC, CMASK and FMASK compared" : mode == 8 ? "only the FMASK compared" : mode == 9 ? "a CPU store of a new byte into a compared FMASK" : mode == 10 ? "a same-value driver store into a compared FMASK" : "an explicitly unwatched FMASK";
            const std::string_view refusal = mode == 4 ? "initial multisampled DCC contents require watched" : mode == 11 ? "native FMASK contents require watched guest memory" : "";
            bool refused = false;
            try {
                device.ColorMetadataPass({AgcDriver::Graphics::ColorMetadataPass::Mode::FmaskDecompress, {target}});
                Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
            } catch (const std::runtime_error& error) {
                if (mode >= 7 && mode != 11 && std::string_view(error.what()) == deviceLimit) {
                    std::printf("8 samples, %s: the FMASK passed the initial import checks, then %s, so its reuse is not tested\n", fmaskCase, error.what());
                    programCount = 0;
                    continue;
                }
                if (refusal.empty()) throw;
                refused = std::string_view(error.what()).find(refusal) != std::string_view::npos;
            }
            if (!refusal.empty()) {
                Require(refused, mode == 11 ? "an explicitly unwatched FMASK range was not refused at its initial import" : "an explicitly unwatched multisampled DCC color range was not refused at its initial import");
                if (mode == 11) std::printf("8 samples, %s: refused at the initial import\n", fmaskCase);
                programCount = 0;
                continue;
            }
            blocks.push_back(std::make_unique<Block>(layout.Bytes()));
            auto& resolved = *blocks.back();
            AgcDriver::Graphics::ColorMetadataPass pass{};
            pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
            pass.source = target;
            pass.targets = {DecodeTarget(resolved)};
            const auto resolve = [&] {
                std::memset(resolved.data, 0, resolved.bytes);
                device.ColorMetadataPass(pass);
                const auto stored = ReadBack(device, resolved);
                for (std::uint32_t y = 0; y < Height; ++y) {
                    for (std::uint32_t x = 0; x < Width; ++x) {
                        for (std::uint32_t channel = 0; channel < 4; ++channel) {
                            const auto expectedValue = x < ScissorWidth ? initial[channel] + (written[channel] - initial[channel]) / static_cast<float>(samples) : initial[channel];
                            Require(FromHalf(stored[layout.Offset(x, y) / 2u + channel]) == expectedValue, "a multisampled DCC target over a compared host import did not start from the fast clear its metadata holds");
                        }
                    }
                }
            };
            resolve();
            if (mode == 0 || (mode >= 5 && mode <= 8)) {
                Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
                resolve();
                if (mode >= 7) std::printf("8 samples, %s: imported and reused\n", fmaskCase);
                programCount = 0;
                continue;
            }
            const char* plane = mode == 1 ? "color" : mode == 2 ? "DCC" : mode == 3 ? "CMASK" : "FMASK";
            const auto& [block, bytes] = mode >= 9 ? std::pair<const Block*, std::size_t>{fmask, fmask->bytes} : planes[mode - 1u];
            if (mode == 1) static_cast<volatile std::uint8_t*>(source.data)[0] = 0x5au;
            if (mode == 2) static_cast<volatile std::uint8_t*>(metadata.data)[0] = 0x00u;
            if (mode == 3) {
                const std::array<std::byte, 1> same{std::byte{0xcc}};
                AgcDriver::GuestMemory::Write(cmask.Address(), same);
            }
            if (mode == 9) static_cast<volatile std::uint8_t*>(fmask->data)[0] = 0x5au;
            if (mode == 10) {
                const std::array<std::byte, 1> same{std::byte{0x11}};
                AgcDriver::GuestMemory::Write(fmask->Address(), same);
            }
            const std::string_view storeRefusal = mode >= 9 ? "writes to a resident native FMASK" : "writes to a resident multisampled DCC";
            try {
                Draw(device, target, false, Float32Export, Program(written, Float32Export, 1u), false, true);
            } catch (const std::runtime_error& error) {
                refused = std::string_view(error.what()).find(storeRefusal) != std::string_view::npos;
                if (refused) expectRange(error.what(), plane, block->Address(), bytes, samples);
            }
            Require(refused, mode == 3 ? "a same-value driver store over a compared CMASK range did not invalidate multisampled DCC residency" : mode == 10 ? "a same-value driver store over a compared FMASK range did not invalidate its residency" : mode == 9 ? "a CPU store of a new byte over a compared FMASK range did not invalidate its residency" : "a CPU store of new bytes over a compared multisampled DCC plane did not invalidate its residency");
            if (mode >= 9) std::printf("8 samples, %s: refused at reuse\n", fmaskCase);
            programCount = 0;
        }
    }
    return nullptr;
}
#endif

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 3 && std::string_view(argv[1]) == "--dcc-srgb-clear") {
            const auto samples = std::string_view(argv[2]) == "2" ? 2u : std::string_view(argv[2]) == "4" ? 4u : std::string_view(argv[2]) == "8" ? 8u : 0u;
            Require(samples != 0, "unknown sRGB DCC test sample count");
            try {
                DccSrgbClearTests(*device, samples);
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "AGC graphics: the device cannot render color with the target's sample count") throw;
                std::printf("skipped, the device lacks the requested multisample color attachments: %s\n", error.what());
                return VulkanTestSkipped;
            }
            std::cout << "Multisampled sRGB DCC clear tests passed\n";
            return 0;
        }
        if ((argc == 2 || argc == 3) && std::string_view(argv[1]) == "--dcc-fast-clear") {
            const auto samples = argc == 2 ? 0u : std::string_view(argv[2]) == "2" ? 2u : std::string_view(argv[2]) == "4" ? 4u : std::string_view(argv[2]) == "8" ? 8u : 0u;
            Require(argc == 2 || samples != 0, "unknown DCC test sample count");
            try {
                DccFastClearTests(*device, samples);
                DccRefillTests(*device, samples);
                DccImportWindowTests(*device, samples);
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "AGC graphics: the device cannot render color with the target's sample count") throw;
                std::printf("skipped, the device lacks the requested multisample color attachments: %s\n", error.what());
                return VulkanTestSkipped;
            }
            std::cout << "Multisampled DCC fast clear tests passed\n";
            return 0;
        }
        if ((argc == 2 || argc == 3) && std::string_view(argv[1]) == "--dcc-compared-import") {
            const auto samples = argc == 2 ? 0u : std::string_view(argv[2]) == "2" ? 2u : std::string_view(argv[2]) == "4" ? 4u : std::string_view(argv[2]) == "8" ? 8u : 0u;
            Require(argc == 2 || samples != 0, "unknown DCC test sample count");
#ifdef _WIN32
            std::puts("skipped, Linux comparison mode only: Windows host imports settle on a second queue or keep their write watch");
            return VulkanTestSkipped;
#else
            const char* skipped = nullptr;
            try {
                skipped = DccComparedImportTests(*device, samples);
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "AGC graphics: the device cannot render color with the target's sample count") throw;
                std::printf("skipped, the device lacks the requested multisample color attachments: %s\n", error.what());
                return VulkanTestSkipped;
            }
            if (skipped != nullptr) {
                std::puts(skipped);
                return VulkanTestSkipped;
            }
            std::cout << "Multisampled DCC compared host import tests passed\n";
            return 0;
#endif
        }
        if (argc == 2 && std::string_view(argv[1]) == "--dithered8") {
            try {
                DitheredCoverage(*device, 8, EightSampleDitherCases);
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "AGC graphics: the device cannot render color with the target's sample count") throw;
                std::printf("skipped, the device lacks eight-sample color attachments: %s\n", error.what());
                return VulkanTestSkipped;
            }
            std::puts("dithered eight-sample alpha-to-coverage tests passed");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--dithered2") {
            try {
                DitheredCoverage(*device, 2, TwoSampleDitherCases);
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "AGC graphics: the device cannot render color with the target's sample count") throw;
                std::printf("skipped, the device lacks two-sample color attachments: %s\n", error.what());
                return VulkanTestSkipped;
            }
            std::puts("dithered two-sample alpha-to-coverage tests passed");
            return 0;
        }
        Require(argc == 1, "unknown alpha-to-coverage test argument");
        const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
        Block block(layout.Bytes());
        struct Case {
            std::uint32_t alpha;
            bool alphaToCoverage;
            bool written;
        };
        constexpr std::array<Case, 17> cases{{
            {0x00000000u, true, false}, {0x80000000u, true, false}, {0x3e800000u, true, false}, {0x3efffffeu, true, false},
            {0x3effffffu, true, false}, {0x3f000000u, true, true}, {0x3f000001u, true, true}, {0x3f000002u, true, true},
            {0x3f400000u, true, true}, {0x3f800000u, true, true}, {0x40000000u, true, true}, {0xbf800000u, true, false},
            {0x7f800000u, true, true}, {0xff800000u, true, false}, {0x7fc00000u, true, false}, {0xffc00000u, true, false},
            {0x3e800000u, false, true},
        }};
        for (const auto& test : cases) {
            const Color source{0.25f, 0.5f, 0.125f, std::bit_cast<float>(test.alpha)};
            const auto before = Fill(block, layout);
            Draw(*device, DecodeTarget(block), test.alphaToCoverage, Float32Export, Program(source, Float32Export));
            char name[96];
            std::snprintf(name, sizeof(name), "alpha 0x%08x with alpha-to-coverage %s", test.alpha, test.alphaToCoverage ? "on" : "off");
            Check(*device, block, layout, before, [&](const Color& destination) { return test.written ? source : destination; }, name);
            programCount = 0;
        }
        struct PackedCase {
            std::uint32_t exportFormat;
            const char* name;
            float alpha;
            bool written;
        };
        constexpr std::array<PackedCase, 7> packedCases{{
            {Float16Export, "FP16_ABGR", 0.75f, true}, {Float16Export, "FP16_ABGR", 0.25f, false},
            {Unorm16Export, "UNORM16_ABGR", 0.75f, true}, {Unorm16Export, "UNORM16_ABGR", 0.25f, false},
            {Snorm16Export, "SNORM16_ABGR", 0.75f, true}, {Snorm16Export, "SNORM16_ABGR", 0.25f, false}, {Snorm16Export, "SNORM16_ABGR", -0.5f, false},
        }};
        for (const auto& test : packedCases) {
            const Color source{0.25f, 0.5f, 0.125f, test.alpha};
            const auto before = Fill(block, layout);
            Draw(*device, DecodeTarget(block), true, test.exportFormat, Program(source, test.exportFormat));
            char name[96];
            std::snprintf(name, sizeof(name), "alpha %.2f with %s exports", test.alpha, test.name);
            Check(*device, block, layout, before, [&](const Color& destination) { return test.written ? source : destination; }, name);
            programCount = 0;
        }
        const auto harness = DepthFastClearHarness::Compile(*device);
        const Block coveredDepth(BlockBytes);
        const Block discardedDepth(BlockBytes);
        DepthOnlyCoverage(*device, harness, coveredDepth, 0.75f, 0, true);
        DepthOnlyCoverage(*device, harness, discardedDepth, 0.25f, 1, false);
        const Block fullMaskDepth(BlockBytes);
        DepthOnlyCoverage(*device, harness, fullMaskDepth, 0.25f, 2, false, 0xfu, 0.75f);
        std::puts("alpha-to-coverage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
