#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using DepthFastClearHarness::BufferDescriptor;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Triangles;
using DepthFastClearHarness::VertexCode;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
const ColorTargetLayout TargetLayout(Width, Height, ColorTileMode::RenderTarget, 4);

alignas(4096) std::array<float, Width * Height * 2> FirstDepth{};
alignas(4096) std::array<float, Width * Height * 2> SecondDepth{};

alignas(256) constexpr std::array<std::uint32_t, 4> CoverCode{0x7e0002f2, 0xf800180f, 0x00000000, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 8> StoreCode{0x7e0a0280, 0x7e0c0280, 0xe0702000, 0x80010506, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000};

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
            Require(raw != MAP_FAILED, "pass input hazard: cannot map a block");
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
        Require(data != nullptr, "pass input hazard: cannot allocate a block");
        std::memset(data, 0, bytes);
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

struct Program {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t vertexPush = 0;
};

Program Compile(AgcDriver::VulkanDevice& device, const void* vertices, std::span<const std::uint32_t> pixelCode, const void* stored) {
    const auto target = device.Target();
    Program program;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices, 16u, static_cast<std::uint32_t>(Triangles().size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    program.vertex = ShaderRecompiler::Recompile(vertex);
    program.vertexPush = static_cast<std::uint32_t>(program.vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    if (stored != nullptr) {
        const auto storeBuffer = BufferDescriptor(stored, 4u, 1u);
        std::copy(storeBuffer.begin(), storeBuffer.end(), pixelUserData.begin() + 4);
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(program.vertexPush, target)
    };
    fragment.useCache = false;
    program.pixel = ShaderRecompiler::Recompile(fragment);
    return program;
}

void Draw(AgcDriver::VulkanDevice& device, const Program& program, const Block& target, const float* depth, const std::uint32_t* indices) {
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &program.vertex, 0},
        {ShaderStage::Fragment, &program.pixel, PixelPushOffset(program.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {target.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
    state.colors = {state.color};
    state.hasColorTarget = true;
    if (depth != nullptr) {
        state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(depth), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        state.depthTest = true;
        state.depthWrite = true;
        state.depthCompare = VK_COMPARE_OP_ALWAYS;
    }
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{reinterpret_cast<std::uintptr_t>(indices), static_cast<std::uint32_t>(Triangles().size()), indices != nullptr ? 4u : 0u, 1, 0, indices != nullptr};
    device.Draw(state, draw, shaders);
}

bool WriterFenced(std::uint64_t address, std::size_t bytes) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "pass input hazard: no active recorder");
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> writes{{{address, address + bytes}}};
    return recorder->StartDrawPass(0x5eed, false, false, AgcDriver::Graphics::PassAccess{{}, writes, {}, {}, false, false}).barrier;
}

std::size_t Covered(AgcDriver::VulkanDevice& device, const Block& target) {
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(TargetLayout.Bytes());
    AgcDriver::GuestMemory::Read(target.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return static_cast<std::size_t>(std::count(stored.begin(), stored.end(), std::uint8_t{0xff}));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Block inputs(BlockBytes);
        auto* indices = reinterpret_cast<std::uint32_t*>(inputs.data);
        constexpr std::array<std::uint32_t, 3> triangle{0u, 1u, 2u};
        const auto reader = Compile(*device, Triangles().data(), CoverCode, nullptr);
        const auto writer = Compile(*device, Triangles().data(), StoreCode, indices + 1);
        constexpr std::size_t all = std::size_t{Width} * Height * 4u;
        const std::array<std::pair<const float*, const float*>, 3> depths{{{nullptr, nullptr}, {FirstDepth.data(), nullptr}, {FirstDepth.data(), SecondDepth.data()}}};
        for (const auto& [readDepth, writeDepth] : depths) {
            const Block read(TargetLayout.Bytes());
            const Block written(TargetLayout.Bytes());
            std::memcpy(indices, triangle.data(), sizeof(triangle));
            const auto variant = std::string(readDepth == nullptr ? "without depth" : writeDepth == nullptr ? "with depth on the reader" : "with a depth target each");
            Draw(*device, reader, read, readDepth, indices);
            Require(WriterFenced(inputs.Address(), sizeof(triangle)), "pass input hazard " + variant + ": a pass writing the indices an open pass reads in place took no barrier");
            Draw(*device, reader, read, readDepth, indices);
            Draw(*device, writer, written, writeDepth, nullptr);
            Require(Covered(*device, read) == all, "pass input hazard " + variant + ": the draw reading its indices in place did not cover its target, a later pass overwrote them first");
            Require(Covered(*device, written) == all, "pass input hazard " + variant + ": the storing draw did not cover its target");
            std::array<std::uint32_t, 3> stored{};
            AgcDriver::GuestMemory::Read(inputs.Address(), std::as_writable_bytes(std::span(stored)), 4);
            Require(stored == std::array<std::uint32_t, 3>{0u, 0u, 2u}, "pass input hazard " + variant + ": the storing draw's write did not reach the indices");
        }
        std::puts("pass input hazard tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
