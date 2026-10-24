#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 16;
constexpr std::array<std::uint32_t, 3> SampleCounts{2, 4, 8};
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Unorm4_4_4_4Info = 0x8000u | (19u << 2u) | (2u << 11u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t LoadResult = 12;
constexpr double EdgeMargin = 1.0 / 32.0;
constexpr std::array<float, 4> First{0.2f, 0.4f, 0.6f, 0.8f};
constexpr std::array<float, 4> Second{13.0f / 15.0f, 0.0f, 1.0f, 0.0f};
constexpr std::array<float, 4> Third{1.0f / 15.0f, 1.0f, 0.0f, 4.0f / 15.0f};
constexpr std::array<float, 4> Fourth{0.0f, 7.0f / 15.0f, 0.0f, 11.0f / 15.0f};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 26> LoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f30, 0x00011005,
    0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c,
    0x80001301, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Covering{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<float, 4>, 3> Cutting{{
    {-0.93f, -1.0f, 0.5f, 1.0f}, {0.37f, -1.0f, 0.5f, 1.0f}, {-0.21f, 0.94f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<float, 4>, 3> Crossing{{
    {-0.6f, -0.2f, 0.5f, 1.0f}, {0.9f, -0.85f, 0.5f, 1.0f}, {0.5f, 0.9f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<float, 4>, 3> Overlapping{{
    {-0.08f, -0.47f, 0.5f, 1.0f}, {1.0f, -0.13f, 0.5f, 1.0f}, {0.29f, 1.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 24> Programs{};
std::size_t programCount = 0;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};

std::uint32_t MultisampledAttrib(std::uint32_t samples) {
    const auto shift = static_cast<std::uint32_t>(std::countr_zero(samples));
    return (shift << 12u) | (shift << 15u);
}

std::uint32_t Unorm(float value) {
    if (!(value > 0.0f)) return 0;
    if (value >= 1.0f) return 15;
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(value) * 15.0));
}

std::uint32_t Packed(std::array<float, 4> rgba) {
    return Unorm(rgba[3]) | (Unorm(rgba[2]) << 4u) | (Unorm(rgba[1]) << 8u) | (Unorm(rgba[0]) << 12u);
}

std::span<const std::uint32_t> Float32Program(std::array<float, 4> rgba) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(rgba[vgpr]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
#endif
        Require(data != nullptr, "cannot allocate the multisampled color target");
        std::memset(data, 0, bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t samples) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Unorm4_4_4_4Info;
    cx[0x31d] = MultisampledAttrib(samples);
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16_UINT && color.componentMapping == 0x1bu && color.packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4 && color.samples == samples && color.extent.width == Side && color.extent.height == Side, "the target did not decode as a 16x16 packed R16_UINT surface of " + std::to_string(samples) + " samples");
    return color;
}

AgcDriver::QueueState Queue(const Block& block, std::uint32_t targetMask, std::uint32_t samples) {
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
        {0x31b, 0}, {0x31c, Unorm4_4_4_4Info}, {0x31d, MultisampledAttrib(samples)},
        {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}, {0x1e0, 0},
        {0xc, 0}, {0xd, (Side << 16u) | Side},
        {0x81, 0x80000000}, {0x82, (Side << 16u) | Side},
        {0x90, 0x80000000}, {0x91, (Side << 16u) | Side},
        {0x94, 0x80000000}, {0x95, (Side << 16u) | Side},
        {0x10f, std::bit_cast<std::uint32_t>(static_cast<float>(Side) / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(static_cast<float>(Side) / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-static_cast<float>(Side) / 2.0f)}, {0x112, std::bit_cast<std::uint32_t>(static_cast<float>(Side) / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}
    };
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    return queue;
}

AgcDriver::Graphics::State TargetState(const Block& block, std::uint32_t exportedMask, std::uint32_t samples) {
    const bool partial = exportedMask != 0xfu;
    const auto decoded = AgcDriver::Graphics::DecodeState(Queue(block, exportedMask, samples));
    const auto& color = decoded.colors.front();
    char detail[256];
    std::snprintf(detail, sizeof(detail), "CB_TARGET_MASK 0x%x of the %u-sample 4_4_4_4 target decoded %zu targets, %u samples, word mask 0x%04x, write mask 0x%x and %s shading", exportedMask, samples, decoded.colors.size(), decoded.samples, color.partialWriteMask, decoded.blends.front().colorWriteMask, decoded.perSampleShading ? "per-sample" : "pixel-rate");
    Require(decoded.colors.size() == 1u && decoded.samples == samples && color.samples == samples && color.partialWriteMask == (partial ? AgcDriver::Graphics::PackedWriteMask(color.componentMapping, exportedMask) : 0u) && decoded.blends.size() == 1u && decoded.blends.front().colorWriteMask == (partial ? 0u : 0xfu) && decoded.blend.colorWriteMask == decoded.blends.front().colorWriteMask && decoded.perSampleShading == partial, detail);
    const auto merge = AgcDriver::Graphics::ExportTargetMerge(decoded);
    Require(merge.mask == color.partialWriteMask && merge.samples == (partial ? samples : 1u) && !merge.depthStencilWrites && merge.descriptor == (partial ? AgcDriver::Graphics::EncodeTextureResource(AgcDriver::Graphics::TargetMergeSurface(color)) : std::array<std::uint32_t, 8>{}), "the decoded " + std::to_string(samples) + "-sample target exported another merge than its own surface");
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.colors = decoded.colors;
    state.color = decoded.color;
    state.hasColorTarget = true;
    state.renderExtent = {Side, Side};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Side), static_cast<float>(Side), -static_cast<float>(Side), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Side, Side}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = decoded.blends;
    state.blend = decoded.blend;
    state.blendConstants = {};
    state.samples = decoded.samples;
    state.perSampleShading = decoded.perSampleShading;
    return state;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, const std::array<std::array<float, 4>, 3>& triangle, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(triangle.size()), 0x01016facu};
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
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
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
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, fragment);
    const auto pixelResult = invocation.Materialize(*invocation.Capture(runtime));
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint32_t> LoadSamples(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::uint32_t samples) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Buffer.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu};
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(LoadCode.data()), std::as_bytes(std::span(LoadCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(LoadCode.data()), LoadCode, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto program = ShaderRecompiler::Recompile(request);
    std::vector<std::uint32_t> loaded(Side * Side * samples);
    for (std::uint32_t first = 0; first < loaded.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto index = first + tid;
            const auto pixel = index / samples;
            const std::array<std::uint32_t, 6> words{0u, 0u, 0u, pixel % Side, pixel / Side, index % samples};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) loaded[first + tid] = Buffer[tid * Words + LoadResult];
    }
    return loaded;
}

double EdgeDistance(const std::array<std::array<float, 4>, 3>& triangle, double x, double y, std::size_t from) {
    const auto pixels = [](const std::array<float, 4>& vertex) {
        return std::array<double, 2>{(static_cast<double>(vertex[0]) + 1.0) * 0.5 * Side, Side - (static_cast<double>(vertex[1]) + 1.0) * 0.5 * Side};
    };
    const auto a = pixels(triangle[from]);
    const auto b = pixels(triangle[(from + 1u) % triangle.size()]);
    const auto c = pixels(triangle[(from + 2u) % triangle.size()]);
    const auto length = std::hypot(b[0] - a[0], b[1] - a[1]);
    const auto side = [&](double px, double py) { return ((b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0])) / length; };
    return side(c[0], c[1]) < 0 ? -side(x, y) : side(x, y);
}

double Inside(const std::array<std::array<float, 4>, 3>& triangle, double x, double y) {
    return std::min({EdgeDistance(triangle, x, y, 0), EdgeDistance(triangle, x, y, 1), EdgeDistance(triangle, x, y, 2)});
}

void RunSamples(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t samples) {
    const auto color = DecodeTarget(block, samples);
    const auto full = TargetState(block, 0xfu, samples);
    Draw(device, full, Covering, Float32Program(First));
    const auto merged = TargetState(block, 0x1u, samples);
    Require(merged.colors.front().partialWriteMask == 0xf000u, "an X write of the 4_4_4_4 target did not merge bits 15..12");
    Draw(device, merged, Cutting, Float32Program(Second));
    Draw(device, full, Overlapping, Float32Program(Third));
    const auto texture = AgcDriver::Graphics::EncodeTextureResource(AgcDriver::Graphics::TargetMergeSurface(color));
    const auto loaded = LoadSamples(device, texture, samples);
    const auto background = Packed(First);
    const auto written = (background & 0x0fffu) | (Packed(Second) & 0xf000u);
    const auto over = Packed(Third);
    Require(written != background && over != written && over != background, "the test colors do not differ in the merged nibble");
    const auto locations = AgcDriver::Graphics::SampleLocations(merged);
    std::vector<std::uint32_t> pixelsByCoverage(samples + 1u);
    std::uint32_t checked = 0;
    std::uint32_t overdrawn = 0;
    std::uint32_t mergedAfterOverdraw = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            std::uint32_t covered = 0;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const auto word = loaded[(y * Side + x) * samples + sample];
                char where[160];
                std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u) holds 0x%04x", sample, x, y, word);
                Require(word == written || word == background || word == over, std::string(where) + ", neither the merged word, the first draw's word nor the last draw's word");
                covered += word == written ? 1u : 0u;
                const auto px = x + static_cast<double>(locations[sample].x);
                const auto py = y + static_cast<double>(locations[sample].y);
                const auto distance = Inside(Cutting, px, py);
                const auto overDistance = Inside(Overlapping, px, py);
                if (std::fabs(distance) < EdgeMargin || std::fabs(overDistance) < EdgeMargin) continue;
                ++checked;
                const auto expected = overDistance > 0 ? over : distance > 0 ? written : background;
                overdrawn += overDistance > 0 && distance > 0 ? 1u : 0u;
                mergedAfterOverdraw += overDistance <= 0 && distance > 0 ? 1u : 0u;
                Require(word == expected, std::string(where) + (overDistance > 0 ? ", which the last full write covers" : distance > 0 ? ", which only the merging triangle covers" : ", which no later triangle covers"));
            }
            ++pixelsByCoverage[covered];
        }
    }
    const auto partlyMerged = std::accumulate(pixelsByCoverage.begin() + 1, pixelsByCoverage.end() - 1, 0u);
    Require(checked >= Side * Side * samples * 8u / 10u, "fewer than eight in ten samples are clear of the triangles' edges");
    Require(pixelsByCoverage[0] != 0 && pixelsByCoverage[samples] != 0 && partlyMerged != 0, "the merged samples do not leave pixels unmerged, merged and partly merged");
    Require(overdrawn != 0 && mergedAfterOverdraw != 0, "the last full write does not cover some merged samples and leave others");
    Draw(device, full, Covering, Float32Program(First));
    const auto lowMerged = TargetState(block, 0x8u, samples);
    Require(lowMerged.colors.front().partialWriteMask == 0x000fu, "a W write of the 4_4_4_4 target did not merge bits 3..0");
    Draw(device, merged, Cutting, Float32Program(Second));
    Draw(device, lowMerged, Crossing, Float32Program(Fourth));
    const auto twice = LoadSamples(device, texture, samples);
    std::uint32_t checkedTwice = 0;
    std::uint32_t mergedTwice = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const auto px = x + static_cast<double>(locations[sample].x);
                const auto py = y + static_cast<double>(locations[sample].y);
                const auto high = Inside(Cutting, px, py);
                const auto low = Inside(Crossing, px, py);
                if (std::fabs(high) < EdgeMargin || std::fabs(low) < EdgeMargin) continue;
                ++checkedTwice;
                mergedTwice += high > 0 && low > 0 ? 1u : 0u;
                auto expected = background;
                if (high > 0) expected = (expected & 0x0fffu) | (Packed(Second) & 0xf000u);
                if (low > 0) expected = (expected & 0xfff0u) | (Packed(Fourth) & 0x000fu);
                const auto word = twice[(y * Side + x) * samples + sample];
                char where[192];
                std::snprintf(where, sizeof(where), "after an X merge and a W merge drawn back to back, sample %u of pixel (%u, %u) holds 0x%04x, expected 0x%04x", sample, x, y, word, expected);
                Require(word == expected, where);
            }
        }
    }
    Require(checkedTwice >= Side * Side * samples * 8u / 10u && mergedTwice != 0, "the back-to-back merges leave fewer than eight in ten samples clear of their edges, or no sample inside both");
    std::printf("4_4_4_4 unorm %u-sample partial write tests passed: %u partly merged pixels kept bits 15..12 merged into their covered samples only, a full write drawn right after covered %u merged samples, and %u samples kept both of two merges drawn back to back\n", samples, partlyMerged, overdrawn, mergedTwice);
}

bool SupportsSamples(const AgcDriver::Graphics::Context& context, std::uint32_t samples) {
    VkImageFormatProperties properties{};
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    return (context.limits.framebufferColorSampleCounts & samples) != 0u && context.imageFormatProperties(context.physical, VK_FORMAT_R16_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties) == VK_SUCCESS && (properties.sampleCounts & samples) != 0u;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        for (const auto& [present, feature] : {std::pair{context.storageImageMultisample, "shaderStorageImageMultisample"}, std::pair{context.sampleRateShading, "sampleRateShading"}, std::pair{TargetHasCapability(device->Target(), spv::CapabilityFragmentShaderPixelInterlockEXT), "fragmentShaderPixelInterlock"}}) {
            if (present) continue;
            std::printf("skipped, the device lacks %s\n", feature);
            return VulkanTestSkipped;
        }
        std::vector<std::unique_ptr<Block>> blocks;
        std::uint32_t tested = 0;
        for (const auto samples : SampleCounts) {
            if (!SupportsSamples(context, samples)) {
                std::printf("%u-sample R16_UINT storage color targets skipped, the device lacks them\n", samples);
                continue;
            }
            blocks.push_back(std::make_unique<Block>(4u * BlockBytes));
            RunSamples(*device, *blocks.back(), samples);
            ++tested;
        }
        if (tested == 0) return VulkanTestSkipped;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
