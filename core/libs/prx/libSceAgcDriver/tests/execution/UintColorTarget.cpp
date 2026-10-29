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
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;

struct TargetFormat {
    const char* name;
    std::uint32_t info;
    VkFormat format;
    std::uint32_t channels;
    std::uint32_t channelBytes;
};

constexpr TargetFormat R32{"R32_UINT", 0x410u, VK_FORMAT_R32_UINT, 1, 4};
constexpr TargetFormat R32G32{"R32G32_UINT", 0x42cu, VK_FORMAT_R32G32_UINT, 2, 4};
constexpr TargetFormat R32G32B32A32{"R32G32B32A32_UINT", 0x438u, VK_FORMAT_R32G32B32A32_UINT, 4, 4};
constexpr TargetFormat R8{"R8_UINT", 0x404u, VK_FORMAT_R8_UINT, 1, 1};

struct ExportCase {
    const char* name;
    std::uint32_t format;
    VkColorComponentFlags writeMask;
};

constexpr ExportCase Export32R{"32_R", 1, VK_COLOR_COMPONENT_R_BIT};
constexpr ExportCase Export32GR{"32_GR", 2, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT};
constexpr ExportCase Export32AR{"32_AR", 3, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_A_BIT};
constexpr ExportCase Export32ABGR{"32_ABGR", 9, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 128> Programs{};
std::size_t programCount = 0;

std::span<const std::uint32_t> Program(const std::array<std::uint32_t, 4>& vgprs, bool compressed) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < (compressed ? 2u : 4u); ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = vgprs[vgpr];
    }
    code[at++] = compressed ? 0xf8001c0fu : 0xf800180fu;
    code[at++] = compressed ? 0x00000504u : 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> WordProgram(const std::array<std::uint32_t, 4>& words) {
    return Program(words, false);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::size_t bytes, const TargetFormat& target, std::uint32_t attrib3) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = target.info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = attrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == target.format && color.elementBytes == target.channels * target.channelBytes && color.componentMapping == 0xe4u && color.packing == ShaderRecompiler::ColorExportPacking::Uint && color.bytes == bytes, std::string("the UINT target did not decode as an ") + target.name + " surface with the unsigned integer packing");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const ExportCase& exported, std::span<const std::uint32_t> pixelCode) {
    const auto exportFormat = exported.format;
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
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = exported.writeMask;
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
    fragment.context.pixel->targetExportPacking = AgcDriver::Graphics::ExportPackings(state);
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    auto unpacked = fragment;
    unpacked.context.pixel->targetExportPacking.fill(ShaderRecompiler::ColorExportPacking::None);
    const auto plain = materialize(unpacked);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId && plain->variantId == pixelResult->variantId, "the draw did not use the registered pixel shader");
    Require(plain->PipelineVariantId() != pixelResult->PipelineVariantId() && materialize(fragment)->PipelineVariantId() == pixelResult->PipelineVariantId(), "the unsigned integer packing did not key its own specialization");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint8_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(block.bytes);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::uint32_t Channel(const std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t channelBytes) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, channelBytes);
    return value;
}

void ExpectSurface(AgcDriver::VulkanDevice& device, const Block& block, const TargetFormat& target, const ColorTargetLayout& layout, const std::vector<std::uint8_t>& before, const ExportCase& exported, const std::array<std::uint32_t, 4>& expected, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            for (std::uint32_t channel = 0; channel < target.channels; ++channel) {
                const auto at = layout.Offset(x, y) + channel * target.channelBytes;
                const bool written = x < ScissorWidth && (exported.writeMask & (1u << channel)) != 0;
                const auto want = written ? expected[channel] : Channel(before, at, target.channelBytes);
                const auto got = Channel(stored, at, target.channelBytes);
                if (got == want) continue;
                char detail[192];
                std::snprintf(detail, sizeof(detail), ": channel %u of pixel (%u, %u) %s holds 0x%08x, expected 0x%08x", channel, x, y, written ? "written" : "not written", got, want);
                throw std::runtime_error(what + detail);
            }
        }
    }
}

std::vector<std::uint8_t> Fill(Block& block) {
    std::vector<std::uint32_t> words(block.bytes / 4u);
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = static_cast<std::uint32_t>(index * 0x9e3779b9u);
    std::memcpy(block.data, words.data(), block.bytes);
    return {block.data, block.data + block.bytes};
}

std::array<std::uint32_t, 4> Expected(const ExportCase& exported, const std::array<std::uint32_t, 4>& vgprs) {
    if (exported.format == Export32AR.format) return {vgprs[0], 0u, 0u, vgprs[1]};
    return vgprs;
}

void TargetTests(AgcDriver::VulkanDevice& device, const TargetFormat& target, const ExportCase& exported, std::span<const std::array<std::uint32_t, 4>> values, ColorTileMode mode, const char* kind) {
    const auto elementBytes = target.channels * target.channelBytes;
    const ColorTargetLayout layout(Width, Height, mode, elementBytes);
    Block block((layout.Bytes() + BlockBytes - 1) / BlockBytes * BlockBytes);
    const auto color = DecodeTarget(block, layout.Bytes(), target, mode == ColorTileMode::Linear ? LinearAttrib3 : TiledAttrib3);
    auto before = Fill(block);
    for (const auto& vgprs : values) {
        Draw(device, color, exported, WordProgram(vgprs));
        char what[160];
        std::snprintf(what, sizeof(what), "a %s export of 0x%08x 0x%08x 0x%08x 0x%08x into a %s %s target", exported.name, vgprs[0], vgprs[1], vgprs[2], vgprs[3], kind, target.name);
        ExpectSurface(device, block, target, layout, before, exported, Expected(exported, vgprs), what);
        before = ReadBack(device, block);
    }
}

void FormatTests(AgcDriver::VulkanDevice& device, const TargetFormat& target, const ExportCase& exported, std::span<const std::array<std::uint32_t, 4>> values) {
    TargetTests(device, target, exported, values, ColorTileMode::RenderTarget, "tiled");
    TargetTests(device, target, exported, values, ColorTileMode::Linear, "linear");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        constexpr std::array<std::uint32_t, 8> words{{0u, 1u, 0x3f800000u, 0x7fc00000u, 0xff800000u, 0x80000000u, 0xdeadbeefu, 0xffffffffu}};
        std::vector<std::array<std::uint32_t, 4>> single;
        for (const auto word : words) single.push_back({word, 0x11111111u, 0x22222222u, 0x33333333u});
        FormatTests(*device, R32, Export32R, single);
        std::vector<std::array<std::uint32_t, 4>> wide;
        for (std::size_t index = 0; index < words.size(); ++index) wide.push_back({words[index], words[(index + 1) % words.size()], words[(index + 2) % words.size()], words[(index + 4) % words.size()]});
        FormatTests(*device, R32G32, Export32GR, wide);
        FormatTests(*device, R32G32B32A32, Export32AR, wide);
        FormatTests(*device, R32G32B32A32, Export32ABGR, wide);
        const std::vector<std::array<std::uint32_t, 4>> narrow{{0u, 0x11111111u, 0x22222222u, 0x33333333u}, {1u, 0u, 0u, 0u}, {0x7fu, 0u, 0u, 0u}, {0xa5u, 0u, 0u, 0u}, {0xffu, 0u, 0u, 0u}};
        FormatTests(*device, R8, Export32R, narrow);
        std::puts("UINT color target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
