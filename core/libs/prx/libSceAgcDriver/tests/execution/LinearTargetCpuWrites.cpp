#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TargetBytes = std::size_t{Width} * Height * 16u;
constexpr std::size_t RowBytes = std::size_t{Width} * 16u;
constexpr std::uint32_t Drawn = Width / 2u;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 2> Programs{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::size_t index, bool blue) {
    auto& code = Programs.at(index);
    std::size_t at = 0;
    code[at++] = 0x36000000u | (20u << 17u) | 0xffu;
    code[at++] = 0x0000ffffu;
    code[at++] = 0x2c000000u | (21u << 17u) | 0x90u;
    code[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    code[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    code[at++] = (blue ? 0x7e0002f2u : 0x7e000280u) | (22u << 17u);
    code[at++] = 0x7e0002f2u | (23u << 17u);
    code[at++] = 0xf800180fu;
    code[at++] = 0x17161514u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    Block(std::size_t bytes, bool stayWatched) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (arena) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (arena) {
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
        Require(!arena || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        if (arena && !stayWatched) {
            AgcDriver::GuestMemory::Unwatch(Address(), bytes);
            Require(!AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is still write-watched");
        }
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
        if (arena) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (arena) {
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
    bool arena;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode, std::uint32_t scissorWidth) {
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
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionFixedPoint);
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
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {scissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::array<float, 4> Texel(const std::uint8_t* target, std::uint32_t x, std::uint32_t y) {
    std::array<float, 4> texel{};
    std::memcpy(texel.data(), target + static_cast<std::size_t>(y) * RowBytes + static_cast<std::size_t>(x) * 16u, sizeof(texel));
    return texel;
}

enum class Outcome { Kept, Lost, Skipped };

Outcome Run(AgcDriver::VulkanDevice& device, bool watched, bool seeded, bool hookRead) {
    const auto name = std::string(seeded ? "seeded" : "unseeded") + ", " + (watched ? "write-watched" : "unwatched") + " memory" + (hookRead ? ", driver read before the store" : "");
    Block block(watched ? std::size_t{16} * BlockBytes : BlockBytes, watched);
    std::fill(block.data, block.data + block.bytes, std::uint8_t{0xff});
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) return Outcome::Skipped;
    const auto color = DecodeTarget(block);
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.bytes == TargetBytes, "the target is not a linear R32G32B32A32 target");
    Draw(device, color, Program(0, false), Width);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr && !recorder->Idle(), "the draw into an imported linear target waited for the GPU instead of being recorded");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(TargetBytes);
    std::memcpy(stored.data(), block.data, TargetBytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), 0.0f, 1.0f};
            const auto texel = Texel(stored.data(), x, y);
            Require(texel == expected, name + ": after the first draw, raw guest read: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds (" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + ")");
        }
    }
    if (seeded) {
        for (std::uint32_t y = 0; y < Height; ++y) std::memset(block.data + y * RowBytes, 0x5a, Drawn * 16u);
    }
    Draw(device, color, Program(1, true), Drawn);
    for (std::uint32_t y = 0; y < Height; ++y) std::memset(block.data + y * RowBytes + Drawn * 16u, 0x42, RowBytes - Drawn * 16u);
    const bool watchedAtStore = AgcDriver::GuestMemory::Watched(block.Address(), TargetBytes);
    if (hookRead) {
        std::vector<std::uint8_t> row(RowBytes);
        AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(row)), 1);
        for (std::uint32_t x = 0; x < Drawn; ++x) {
            const std::array<float, 4> expected{static_cast<float>(x), 0.0f, 1.0f, 1.0f};
            const auto texel = Texel(row.data(), x, 0);
            Require(texel == expected, name + ": a driver read of row 0 before the store: pixel (" + std::to_string(x) + ", 0) holds (" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + "), not the second draw's");
        }
    }
    device.WaitIdle();
    std::size_t wrongGpu = 0;
    std::size_t wrongCpu = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Drawn; ++x) {
            const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), 1.0f, 1.0f};
            if (Texel(block.data, x, y) != expected) ++wrongGpu;
        }
        auto* begin = block.data + y * RowBytes + Drawn * 16u;
        wrongCpu += static_cast<std::size_t>(std::count_if(begin, block.data + (y + 1u) * RowBytes, [](std::uint8_t byte) { return byte != 0x42u; }));
    }
    std::printf("linear target CPU writes, %s (watched at the store: %d): wrong GPU pixels %zu/%u, wrong CPU bytes %zu/%zu\n", name.c_str(), watchedAtStore ? 1 : 0, wrongGpu, Drawn * Height, wrongCpu, (RowBytes - Drawn * 16u) * Height);
    return wrongGpu == 0 && wrongCpu == 0 ? Outcome::Kept : Outcome::Lost;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        bool kept = true;
        bool ran = false;
        for (const bool watched : {true, false}) {
            for (const bool seeded : {false, true}) {
                for (const bool hookRead : {false, true}) {
                    const auto outcome = Run(*device, watched, seeded, hookRead);
                    ran = ran || outcome != Outcome::Skipped;
                    kept = kept && outcome != Outcome::Lost;
                }
            }
        }
        if (!ran) {
            std::puts("linear target CPU write test skipped: no host import on this device");
            return VulkanTestSkipped;
        }
        Require(kept, "a drawn linear target's store lost the drawn half or the CPU's later stores to the other half");
        std::puts("linear target CPU write tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
