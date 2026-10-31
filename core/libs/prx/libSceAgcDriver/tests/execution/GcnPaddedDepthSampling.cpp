#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Swizzle64KbZX = 24;
constexpr std::uint32_t ClampEdge = 2;
constexpr VkExtent2D Padded{64, 64};
constexpr std::uint32_t Width = 48;
constexpr std::uint32_t Height = 40;
constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::size_t InputOffset = 0;
constexpr std::size_t OutputOffset = 0x1000;
constexpr std::size_t D32Offset = 0x10000;
constexpr std::size_t D16Offset = 0x40000;

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f08, 0x00820602, 0xf0001f08, 0x00020a04,
    0x34020085, 0xbf8c3f70, 0xe0781000, 0x80010601, 0xe0781010, 0x80010a01, 0xbf810000,
};

alignas(256) std::array<std::byte, Padded.width * Padded.height * 4> Pixels{};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "GCN padded depth sampling: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* At(std::size_t offset) { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

struct Probe {
    float u;
    float v;
    std::uint32_t x;
    std::uint32_t y;
};

std::array<std::uint32_t, 4> RawBuffer(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Swizzle64KbZX << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void DrawDepth(AgcDriver::VulkanDevice& device, std::uint64_t address, VkFormat format, float z, VkRect2D scissor) {
    using DepthFastClearHarness::PixelCode;
    using DepthFastClearHarness::VertexCode;
    static std::deque<std::vector<std::array<float, 4>>> vertexData;
    vertexData.push_back({{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}});
    const auto& vertices = vertexData.back();
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = DepthFastClearHarness::BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = true;
    pixelInfo.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixelInfo.posX = true;
    pixelInfo.posY = true;
    pixelInfo.targetOutputMode[0] = 9;
    pixelInfo.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragmentRequest{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixelInfo, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragmentRequest.useCache = false;
    const auto pixel = ShaderRecompiler::Recompile(fragmentRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertex, 0},
        {ShaderStage::Fragment, &pixel, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), Padded, VK_FORMAT_R8G8B8A8_UNORM, AgcDriver::Graphics::ColorTargetLayout(Padded.width, Padded.height, AgcDriver::Graphics::ColorTileMode::Linear).Bytes(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{address, 0, Padded, format, 1.0f, 0};
    state.depth->gcn = true;
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
    state.renderExtent = Padded;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Padded.height), static_cast<float>(Padded.width), -static_cast<float>(Padded.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = scissor;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::vector<Probe> Probes() {
    std::vector<Probe> probes;
    constexpr std::array<std::array<std::uint32_t, 2>, 26> texels{{
        {0, 0}, {47, 0}, {0, 39}, {39, 31}, {40, 32}, {47, 39}, {46, 38}, {47, 32}, {40, 39}, {20, 20}, {10, 35}, {45, 5}, {41, 33},
        {44, 36}, {47, 36}, {44, 39}, {43, 35}, {43, 39}, {47, 35}, {39, 39}, {47, 31}, {40, 31}, {39, 32}, {46, 39}, {5, 30}, {30, 10},
    }};
    for (const auto& [x, y] : texels) probes.push_back({(static_cast<float>(x) + 0.5f) / Width, (static_cast<float>(y) + 0.5f) / Height, x, y});
    probes.push_back({1.25f, 1.25f, 47, 39});
    probes.push_back({-0.1f, -0.1f, 0, 0});
    probes.push_back({1.25f, 0.4875f, 47, 19});
    probes.push_back({0.5125f, 1.25f, 24, 39});
    probes.push_back({1.25f, 0.8125f, 47, 32});
    probes.push_back({0.8125f, 1.25f, 39, 39});
    return probes;
}

float DepthAt(std::uint32_t x, std::uint32_t y, bool refreshed) {
    if (refreshed && x >= 44 && y >= 36) return 0.625f;
    return x >= 40 && y >= 32 ? 0.5f : 0.25f;
}

std::uint32_t SampledTexel(float coordinate, std::uint32_t size) {
    return static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(coordinate * static_cast<float>(size))), 0, static_cast<int>(size) - 1));
}

void Check(AgcDriver::VulkanDevice& device, GuestBlock& block, std::uint8_t* depth, std::uint32_t format, bool d16, bool refreshed, const char* what) {
    const auto probes = Probes();
    Require(probes.size() == Threads, "GCN padded depth sampling needs one probe a thread");
    auto* input = reinterpret_cast<std::uint32_t*>(block.At(InputOffset));
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(probes[tid].u);
        input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(probes[tid].v);
        input[tid * 4u + 2u] = probes[tid].x;
        input[tid * 4u + 3u] = probes[tid].y;
    }
    auto* output = reinterpret_cast<float*>(block.At(OutputOffset));
    std::fill(output, output + Threads * 8u, -1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto in = RawBuffer(input, Threads * 16u);
    const auto out = RawBuffer(output, Threads * 32u);
    const auto texture = TextureDescriptor(depth, format);
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::copy(in.begin(), in.end(), userData.begin());
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto expect = [&](std::uint32_t tid, const char* op, float actual, std::uint32_t x, std::uint32_t y) {
        const float expected = DepthAt(x, y, refreshed);
        const bool match = d16 ? std::fabs(actual - expected) <= 2.0f / 65535.0f : std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected);
        char text[224];
        std::snprintf(text, sizeof(text), "%s, %s thread %u at texel (%u,%u) of the %ux%u view: read %.8f, expected %.8f", what, op, tid, x, y, Width, Height, actual, expected);
        Require(match, text);
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        expect(tid, "image_sample_lz", output[tid * 8u], SampledTexel(probes[tid].u, Width), SampledTexel(probes[tid].v, Height));
        expect(tid, "image_load", output[tid * 8u + 4u], probes[tid].x, probes[tid].y);
    }
}

void Case(AgcDriver::VulkanDevice& device, GuestBlock& block, std::size_t offset, VkFormat depthFormat, std::uint32_t format, const char* what) {
    auto* depth = block.At(offset);
    const auto address = reinterpret_cast<std::uintptr_t>(depth);
    const bool d16 = depthFormat == VK_FORMAT_D16_UNORM;
    DrawDepth(device, address, depthFormat, 0.75f, {{0, 0}, Padded});
    DrawDepth(device, address, depthFormat, 0.25f, {{0, 0}, {Width, Height}});
    DrawDepth(device, address, depthFormat, 0.5f, {{40, 32}, {8, 8}});
    Check(device, block, depth, format, d16, false, what);
    DrawDepth(device, address, depthFormat, 0.625f, {{44, 36}, {4, 4}});
    Check(device, block, depth, format, d16, true, (std::string(what) + " after a redraw").c_str());
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Case(*device, block, D32Offset, VK_FORMAT_D32_SFLOAT, Format32Float, "D32 64x64 PS4 depth through a 48x40 R32F view");
        Case(*device, block, D16Offset, VK_FORMAT_D16_UNORM, Format16Unorm, "D16 64x64 PS4 depth through a 48x40 R16 view");
        std::puts("GCN padded depth sampling tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
