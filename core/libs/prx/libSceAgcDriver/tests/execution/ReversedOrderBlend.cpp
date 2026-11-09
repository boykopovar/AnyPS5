#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
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
#include <algorithm>
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
using Color = std::array<float, 4>;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Float16Export = 4;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<std::uint8_t, 4>, 4> UnormDestinations{{
    {64, 128, 32, 200}, {255, 0, 16, 128}, {32, 224, 96, 0}, {128, 64, 192, 255}
}};

constexpr std::array<Color, 4> FloatDestinations{{
    {0.5f, 0.25f, 0.75f, 0.5f}, {1.0f, 0.0f, 0.125f, 0.25f}, {0.0625f, 0.875f, 0.5f, 1.0f}, {0.25f, 0.5f, 0.375f, 0.75f}
}};

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t number;
    std::uint32_t elementBytes;
    VkFormat vkFormat;
    bool normalized;
};

constexpr std::array<Format, 2> Formats{{
    {"8_8_8_8 UNORM", 10u, 0u, 4u, VK_FORMAT_R8G8B8A8_UNORM, true},
    {"16_16_16_16 FLOAT", 12u, 7u, 8u, VK_FORMAT_R16G16B16A16_SFLOAT, false},
}};

struct Swap {
    const char* name;
    std::uint32_t value;
    std::uint8_t mapping;
    std::array<std::uint32_t, 4> memoryChannels;
};

constexpr Swap Standard{"SWAP_STD", 0u, 0xe4u, {0u, 1u, 2u, 3u}};

constexpr std::array<Swap, 2> Swaps{{
    {"SWAP_STD_REV", 2u, 0x1bu, {3u, 2u, 1u, 0u}},
    {"SWAP_ALT_REV", 3u, 0x93u, {3u, 0u, 1u, 2u}},
}};

alignas(256) std::array<std::array<std::uint32_t, 32>, 8> Programs{};
std::size_t programCount = 0;

std::uint16_t Half(float value) {
    Require(value == 0.0f || (value >= 0.0625f && value <= 1.0f), "the test's half values are normal and at most 1");
    if (value == 0.0f) return 0;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    Require((bits & 0x1fffu) == 0, "the test's half values are exact");
    return static_cast<std::uint16_t>(((((bits >> 23u) & 0xffu) - 112u) << 10u) | ((bits >> 13u) & 0x3ffu));
}

float FromHalf(std::uint16_t half) {
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    Require(exponent != 31u, "a blended texel is infinite or NaN");
    const auto magnitude = exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24) : std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
    return (half & 0x8000u) != 0 ? -magnitude : magnitude;
}

std::span<const std::uint32_t> Program(const Color& exported, bool compressed) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    const auto move = [&](std::uint32_t vgpr, std::uint32_t value) {
        code[at++] = 0x7e0002ffu | (vgpr << 17u);
        code[at++] = value;
    };
    if (compressed) {
        move(4, Half(exported[0]) | (Half(exported[1]) << 16u));
        move(5, Half(exported[2]) | (Half(exported[3]) << 16u));
        code[at++] = 0xf8001c0fu;
        code[at++] = 0x00000504u;
    } else {
        for (std::uint32_t channel = 0; channel < 4; ++channel) move(4 + channel, std::bit_cast<std::uint32_t>(exported[channel]));
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

AgcDriver::QueueState Queue(const Block& block, const Format& format, const Swap& swap, std::uint32_t control, std::uint32_t targetMask, std::uint32_t exportFormat) {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, exportFormat}, {0x1c3, 4},
        {0x318, static_cast<std::uint32_t>(block.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(block.Address() >> 40u)},
        {0x31b, 0}, {0x31c, (format.format << 2u) | (format.number << 8u) | (swap.value << 11u) | (format.normalized ? 0x8000u : 0u)}, {0x31d, 0},
        {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)}, {0x3b8, TiledAttrib3}, {0x1e0, control},
        {0x105, 0}, {0x106, 0}, {0x107, 0}, {0x108, 0},
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
    return queue;
}

struct Renderer {
    AgcDriver::VulkanDevice& device;
    ShaderRecompiler::RecompileResult vertex;
    std::uint32_t vertexPush = 0;
};

ShaderRecompiler::RecompileResult CompileVertex(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    return ShaderRecompiler::Recompile(vertex);
}

void Draw(Renderer& renderer, const AgcDriver::Graphics::State& decoded, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode) {
    const auto target = renderer.device.Target();
    constexpr std::uint32_t waveSize = 64;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = decoded.colors;
    state.color = decoded.color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = decoded.blends;
    state.blend = decoded.blend;
    state.blendConstants = decoded.blendConstants;
    state.dualSourceBlend = decoded.dualSourceBlend;
    state.reversedBlend = decoded.reversedBlend;

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, exportFormat}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(renderer.vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->reversedBlend = state.reversedBlend;
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId, "the draw did not use the registered pixel shader");
    if (state.reversedBlend != 0u) {
        auto plain = fragment;
        plain.context.pixel->reversedBlend = 0u;
        const auto unblended = materialize(plain);
        Require(unblended->variantId == pixelResult->variantId && unblended->PipelineVariantId() != pixelResult->PipelineVariantId(), "the reversed-order blend code did not key its own specialization of the registered shader");
    }
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &renderer.vertex, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(renderer.vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    renderer.device.Draw(state, draw, shaders);
}

Color Destination(const Format& format, std::uint32_t pick) {
    if (!format.normalized) return FloatDestinations[pick];
    Color color{};
    for (std::uint32_t channel = 0; channel < 4; ++channel) color[channel] = static_cast<float>(UnormDestinations[pick][channel]) / 255.0f;
    return color;
}

std::vector<std::uint8_t> Fill(Block& block, const ColorTargetLayout& layout, const Format& format, const Swap& swap) {
    std::vector<std::uint8_t> bytes(block.bytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto pick = (x + y) % 4u;
            const auto index = layout.Offset(x, y);
            for (std::uint32_t component = 0; component < 4; ++component) {
                const auto channel = swap.memoryChannels[component];
                if (format.normalized) {
                    bytes[index + component] = UnormDestinations[pick][channel];
                } else {
                    const auto half = Half(FloatDestinations[pick][channel]);
                    bytes[index + 2u * component] = static_cast<std::uint8_t>(half);
                    bytes[index + 2u * component + 1u] = static_cast<std::uint8_t>(half >> 8u);
                }
            }
        }
    }
    std::memcpy(block.data, bytes.data(), block.bytes);
    return bytes;
}

float Factor(std::uint32_t factor, std::uint32_t channel, const Color& source, const Color& destination) {
    const bool alpha = channel == 3u;
    switch (factor) {
        case 0: return 0.0f;
        case 1: return 1.0f;
        case 2: return source[channel];
        case 3: return 1.0f - source[channel];
        case 4: return source[3];
        case 5: return 1.0f - source[3];
        case 6: return destination[3];
        case 7: return 1.0f - destination[3];
        case 8: return destination[channel];
        case 9: return 1.0f - destination[channel];
        case 10: return alpha ? 1.0f : std::min(source[3], 1.0f - destination[3]);
        default: throw std::runtime_error("the reference blend has no factor " + std::to_string(factor));
    }
}

Color Blend(std::uint32_t control, const Color& exported, const Color& destination, bool normalized) {
    Color source = exported;
    if (normalized) {
        for (auto& value : source) value = std::clamp(value, 0.0f, 1.0f);
    }
    const auto alphaControl = (control & 0x20000000u) != 0 ? control >> 16u : control;
    Color result{};
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        const auto fields = channel == 3u ? alphaControl : control;
        const auto weighted = source[channel] * Factor(fields & 0x1fu, channel, source, destination);
        const auto kept = destination[channel] * Factor((fields >> 8u) & 0x1fu, channel, source, destination);
        float value = 0.0f;
        switch ((fields >> 5u) & 7u) {
            case 0: value = weighted + kept; break;
            case 1: value = weighted - kept; break;
            case 2: value = std::min(weighted, kept); break;
            case 3: value = std::max(weighted, kept); break;
            case 4: value = kept - weighted; break;
            default: throw std::runtime_error("the reference blend has no combine function " + std::to_string((fields >> 5u) & 7u));
        }
        result[channel] = normalized ? std::clamp(value, 0.0f, 1.0f) : value;
    }
    return result;
}

void Check(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const Format& format, const Swap& swap, std::uint32_t targetMask, const std::vector<std::uint8_t>& before, std::uint32_t control, const Color& exported, const std::string& what) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(block.bytes);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto pick = (x + y) % 4u;
            const auto index = layout.Offset(x, y);
            const auto expected = Blend(control, exported, Destination(format, pick), format.normalized);
            for (std::uint32_t component = 0; component < 4; ++component) {
                const auto channel = swap.memoryChannels[component];
                const bool written = x < ScissorWidth && ((targetMask >> channel) & 1u) != 0;
                bool match = false;
                double held = 0.0;
                double want = 0.0;
                if (format.normalized) {
                    held = stored[index + component];
                    want = written ? expected[channel] * 255.0 : before[index + component];
                    match = written ? std::fabs(held - want) <= 1.0 : stored[index + component] == before[index + component];
                } else {
                    const auto at = index + 2u * component;
                    const auto half = static_cast<std::uint16_t>(stored[at] | (stored[at + 1u] << 8u));
                    const auto previous = static_cast<std::uint16_t>(before[at] | (before[at + 1u] << 8u));
                    held = FromHalf(half);
                    want = written ? expected[channel] : FromHalf(previous);
                    match = written ? std::fabs(held - want) <= std::ldexp(std::max(1.0, std::fabs(want)), -11) : half == previous;
                }
                if (match) continue;
                char detail[192];
                std::snprintf(detail, sizeof(detail), ": memory component %u (channel %u) of pixel (%u, %u) %s holds %.6g, expected %.6g", component, channel, x, y, written ? "inside the scissor" : "outside the written area", held, want);
                throw std::runtime_error(what + detail);
            }
        }
    }
}

std::vector<double> Stored(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const Format& format, const Swap& swap, std::vector<std::uint16_t>& bits) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(block.bytes);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    std::vector<double> values(static_cast<std::size_t>(Width) * Height * 4u);
    bits.assign(values.size(), 0u);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y);
            for (std::uint32_t component = 0; component < 4; ++component) {
                const auto at = (static_cast<std::size_t>(y) * Width + x) * 4u + swap.memoryChannels[component];
                bits[at] = format.normalized ? stored[index + component] : static_cast<std::uint16_t>(stored[index + 2u * component] | (stored[index + 2u * component + 1u] << 8u));
                values[at] = format.normalized ? static_cast<double>(bits[at]) : static_cast<double>(FromHalf(bits[at]));
            }
        }
    }
    return values;
}

void CompareWithStandardOrder(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const Format& format, const Swap& swap, const std::vector<std::uint16_t>& standard, const std::string& what) {
    std::vector<std::uint16_t> reversed;
    const auto values = Stored(device, block, layout, format, swap, reversed);
    for (std::size_t at = 0; at < reversed.size(); ++at) {
        if (reversed[at] == standard[at]) continue;
        const auto pixel = at / 4u;
        char detail[224];
        std::snprintf(detail, sizeof(detail), ": channel %u of pixel (%u, %u) holds %.6g (bits 0x%04x), while the same draw into a SWAP_STD target, which Vulkan blends alone, holds bits 0x%04x", static_cast<unsigned>(at % 4u), static_cast<unsigned>(pixel % Width), static_cast<unsigned>(pixel / Width), values[at], reversed[at], standard[at]);
        throw std::runtime_error(what + detail);
    }
}

struct BlendCase {
    const char* name;
    std::uint32_t control;
    std::uint32_t targetMask;
    bool shaded;
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool dualSource = device->DualSrcBlend();
        Renderer renderer{*device, CompileVertex(*device)};
        renderer.vertexPush = static_cast<std::uint32_t>(renderer.vertex.pushConstants.size());
        const Color exported{0.25f, 0.75f, 0.125f, 0.5f};
        const Color outOfRange{1.5f, -0.25f, 0.5f, 1.25f};
        const Color strongAlpha{0.25f, 0.75f, 0.125f, 1.5f};
        const auto wide = Program(exported, false);
        const auto compressed = Program(exported, true);
        const auto clamped = Program(outOfRange, false);
        const auto unclamped = Program(strongAlpha, false);
        const std::array<BlendCase, 10> cases{{
            {"SRC_ALPHA + ONE_MINUS_SRC_ALPHA, alpha ONE + ONE_MINUS_SRC_ALPHA", 0x65010504u, 0xfu, true},
            {"SRC_ALPHA + ONE_MINUS_SRC_ALPHA, alpha ONE + ONE_MINUS_SRC_ALPHA, red, green and blue written", 0x65010504u, 0x7u, true},
            {"SRC_ALPHA reverse-subtract ONE_MINUS_SRC_ALPHA, alpha ONE reverse-subtract ONE_MINUS_SRC_ALPHA", 0x65810584u, 0xfu, true},
            {"ONE + ONE_MINUS_SRC_ALPHA", 0x40000501u, 0xfu, true},
            {"SRC_ALPHA + ONE, alpha ZERO + ONE", 0x61000104u, 0xfu, true},
            {"SRC_COLOR + ONE_MINUS_SRC_COLOR, alpha SRC_COLOR + ONE", 0x61020302u, 0xfu, true},
            {"SRC_ALPHA + ONE_MINUS_SRC_ALPHA, alpha ONE + ONE", 0x61010504u, 0xfu, true},
            {"DST_COLOR + ZERO, alpha DST_ALPHA + ZERO", 0x60060008u, 0xfu, false},
            {"ONE + ONE", 0x40000101u, 0xfu, false},
            {"ONE MIN ONE", 0x40000141u, 0xfu, false},
        }};
        bool skipped = false;
        for (const auto& format : Formats) {
            const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, format.elementBytes);
            Block block(layout.Bytes());
            for (const auto& swap : Swaps) {
                const auto run = [&](const BlendCase& test, std::uint32_t exportFormat, std::span<const std::uint32_t> program, const Color& values, const char* exports) {
                    const auto what = std::string(format.name) + " " + swap.name + ", " + test.name + ", " + exports;
                    Fill(block, layout, format, Standard);
                    const auto plain = AgcDriver::Graphics::DecodeState(Queue(block, format, Standard, test.control, test.targetMask, exportFormat));
                    Require(plain.colors.size() == 1 && plain.colors[0].componentMapping == Standard.mapping && plain.reversedBlend == 0u && !plain.dualSourceBlend, what + ": the SWAP_STD reference target did not decode as a plain Vulkan blend");
                    Draw(renderer, plain, exportFormat, program);
                    std::vector<std::uint16_t> standard;
                    Stored(*device, block, layout, format, Standard, standard);
                    const auto before = Fill(block, layout, format, swap);
                    const auto decoded = AgcDriver::Graphics::DecodeState(Queue(block, format, swap, test.control, test.targetMask, exportFormat));
                    Require(decoded.colors.size() == 1 && decoded.colors[0].format == format.vkFormat && decoded.colors[0].componentMapping == swap.mapping && decoded.colors[0].bytes == block.bytes, what + ": the target did not decode as a reversed 256x128 surface");
                    Require((decoded.reversedBlend != 0u) == test.shaded && !decoded.dualSourceBlend, what + ": the blend was not split between Vulkan and the pixel shader as expected");
                    if (test.shaded && !dualSource) {
                        skipped = true;
                        return;
                    }
                    Draw(renderer, decoded, exportFormat, program);
                    Check(*device, block, layout, format, swap, test.targetMask, before, test.control, values, what);
                    CompareWithStandardOrder(*device, block, layout, format, swap, standard, what);
                };
                for (const auto& test : cases) run(test, Float32Export, wide, exported, "32_ABGR exports");
                run(cases[0], Float16Export, compressed, exported, "FP16_ABGR exports");
                if (format.normalized) {
                    run(cases[0], Float32Export, clamped, outOfRange, "32_ABGR exports outside [0, 1]");
                    run(cases[5], Float32Export, clamped, outOfRange, "32_ABGR exports outside [0, 1]");
                } else {
                    run(cases[0], Float32Export, unclamped, strongAlpha, "32_ABGR exports with an alpha of 1.5");
                }
            }
        }
        if (skipped) {
            std::puts("skipped, the device has no dual-source blending, so only the blends Vulkan applies alone ran");
            return VulkanTestSkipped;
        }
        std::puts("reversed component order blend tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
