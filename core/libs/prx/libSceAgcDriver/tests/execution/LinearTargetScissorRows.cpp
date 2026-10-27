#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::size_t TexelBytes = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, 16> PositionProgram{
    0x36000000u | (20u << 17u) | 0xffu, 0x0000ffffu,
    0x2c000000u | (21u << 17u) | 0x90u,
    0x7e000c00u | (20u << 17u) | (256u + 20u),
    0x7e000c00u | (21u << 17u) | (256u + 21u),
    0x7e000280u | (22u << 17u),
    0x7e0002f2u | (23u << 17u),
    0xf800180fu, 0x17161514u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t height) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t inputAddr, std::span<const std::uint32_t> pixelCode, VkRect2D scissor, std::uint32_t height) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
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

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = inputAddr;
    pixel.targetOutputMode.at(0) = Float32AbgrExport;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(height), static_cast<float>(Width), -static_cast<float>(height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = scissor;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint8_t> ReadBack(AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(bytes);
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

struct CpuStore {
    std::uint32_t x;
    std::uint32_t y;
    std::uint8_t value;
};

std::uint8_t* storeTarget = nullptr;
std::size_t storeTargetBytes = 0;
std::span<const CpuStore> storesAtWriteBack;
bool stored = false;

void StoreDuringDraw(std::uint64_t address, std::size_t bytes) {
    const auto target = reinterpret_cast<std::uintptr_t>(storeTarget);
    if (stored || storeTarget == nullptr || AgcDriver::GuestMemory::CurrentReadSite() != AgcDriver::GuestMemory::ReadSite::Store || address >= target + storeTargetBytes || address + bytes <= target) return;
    for (const auto& store : storesAtWriteBack) std::memset(storeTarget + (std::size_t{store.y} * Width + store.x) * TexelBytes, store.value, TexelBytes);
    stored = true;
}

void Check(AgcDriver::VulkanDevice& device, std::uint32_t height, VkRect2D scissor, std::span<const CpuStore> stores, const std::string& what) {
    const auto targetBytes = std::size_t{Width} * height * TexelBytes;
    Require(AgcDriver::Graphics::ColorTargetLayout(Width, height, AgcDriver::Graphics::ColorTileMode::Linear, TexelBytes).Bytes() == targetBytes, what + ": the linear target is padded");
    Block block(std::max(targetBytes, BlockBytes));
    std::fill(block.data, block.data + targetBytes, std::uint8_t{0xff});
    const auto color = DecodeTarget(block, height);
    Require(color.format == VK_FORMAT_R32G32B32A32_SFLOAT && color.elementBytes == TexelBytes && color.bytes == targetBytes, what + ": the target decoded to the wrong format");
    storeTarget = block.data;
    storeTargetBytes = targetBytes;
    storesAtWriteBack = stores;
    stored = false;
    if (!stores.empty()) AgcDriver::GuestMemory::SetFlushHook(&StoreDuringDraw);
    const std::span<const std::uint32_t> program(PositionProgram.data(), 10);
    try {
        Draw(device, color, ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionFixedPoint), program, scissor, height);
    } catch (...) {
        if (auto* recorder = AgcDriver::Graphics::Recorder::Active(); recorder != nullptr && !stores.empty()) recorder->Activate();
        throw;
    }
    if (!stores.empty()) {
        if (auto* recorder = AgcDriver::Graphics::Recorder::Active()) recorder->Activate();
        else AgcDriver::GuestMemory::SetFlushHook(nullptr);
        Require(stored, what + ": the draw stored the target without passing the flush hook");
    }
    storeTarget = nullptr;
    const auto result = ReadBack(device, block.Address(), targetBytes);
    const auto x0 = static_cast<std::uint32_t>(scissor.offset.x);
    const auto y0 = static_cast<std::uint32_t>(scissor.offset.y);
    const auto inside = [&](std::uint32_t x, std::uint32_t y) {
        return x >= x0 && x < x0 + scissor.extent.width && y >= y0 && y < y0 + scissor.extent.height;
    };
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto* texel = result.data() + (std::size_t{y} * Width + x) * TexelBytes;
            const auto pixel = what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ")";
            if (inside(x, y)) {
                std::array<float, 4> drawn{};
                std::memcpy(drawn.data(), texel, sizeof(drawn));
                const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), 0.0f, 1.0f};
                Require(drawn == expected, pixel + " inside the scissor holds (" + std::to_string(drawn[0]) + ", " + std::to_string(drawn[1]) + ", " + std::to_string(drawn[2]) + ", " + std::to_string(drawn[3]) + ")");
                continue;
            }
            std::uint8_t expected = 0xff;
            for (const auto& store : stores) {
                if (store.x == x && store.y == y) expected = store.value;
            }
            for (std::size_t at = 0; at < TexelBytes; ++at) {
                Require(texel[at] == expected, pixel + " outside the scissor holds byte " + std::to_string(texel[at]) + " at " + std::to_string(at) + ", expected " + std::to_string(expected) + (expected != 0xff ? " (the CPU stored it while the draw ran)" : ""));
            }
        }
    }
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_RESIDENT_LINEAR", "1");
#else
        setenv("APS5_NO_RESIDENT_LINEAR", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, 64, {{0, 0}, {Width, 64}}, {}, "whole-target scissor");
        Check(*device, 64, {{0, 10}, {Width, 5}}, {}, "full-width rows");
        constexpr std::array<CpuStore, 2> stores{{{2, 2, 0x11}, {50, 20, 0x22}}};
        Check(*device, 64, {{8, 16}, {32, 16}}, stores, "partial scissor");
        constexpr std::array<CpuStore, 1> rowStore{{{50, 0, 0x33}}};
        Check(*device, 1, {{8, 0}, {32, 1}}, rowStore, "partial scissor on a one-row target");
        std::puts("linear target scissor rows tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
