#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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

alignas(256) constexpr std::array<std::uint32_t, 8> AddCode{0x7e0a0281, 0x7e0c0280, 0xe0c82000, 0x80010506, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000};

alignas(4096) std::array<std::uint32_t, 1024> CopiedCounter{};

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
            Require(raw != MAP_FAILED, "atomic passes: cannot map a block");
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
        Require(data != nullptr, "atomic passes: cannot allocate a block");
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

Program Compile(AgcDriver::VulkanDevice& device, const void* counter) {
    const auto target = device.Target();
    Program program;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangles().data(), 16u, static_cast<std::uint32_t>(Triangles().size()));
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
    const auto counterBuffer = BufferDescriptor(counter, 4u, 1u);
    std::copy(counterBuffer.begin(), counterBuffer.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(AddCode.data()), std::as_bytes(std::span(AddCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(AddCode.data()), AddCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(program.vertexPush, target)
    };
    fragment.useCache = false;
    program.pixel = ShaderRecompiler::Recompile(fragment);
    return program;
}

void Draw(AgcDriver::VulkanDevice& device, const Program& program, const Block& target) {
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &program.vertex, 0},
        {ShaderStage::Fragment, &program.pixel, PixelPushOffset(program.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {target.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
    state.colors = {state.color};
    state.hasColorTarget = true;
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0u, 1, 0, false};
    device.Draw(state, draw, shaders);
}

void Settle(AgcDriver::VulkanDevice& device, const Block& target, std::uint64_t counter) {
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test read-back");
    AgcDriver::Graphics::StorageTexture::FlushPending(counter, sizeof(std::uint32_t), nullptr, "test read-back");
    device.WaitIdle();
}

std::uint32_t ReadCounter(std::uint64_t counter) {
    std::uint32_t value = 0;
    AgcDriver::GuestMemory::Read(counter, std::as_writable_bytes(std::span(&value, 1)), 4);
    return value;
}

void WriteCounter(std::uint64_t counter, std::uint32_t value) {
    AgcDriver::GuestMemory::Write(counter, std::as_bytes(std::span(&value, 1)));
}

void CheckTwoDraws(AgcDriver::VulkanDevice& device, const std::string& what, std::uint64_t counter, bool expectContinued) {
    const Block target(TargetLayout.Bytes());
    const auto program = Compile(device, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(counter)));
    WriteCounter(counter, 0);
    Draw(device, program, target);
    Settle(device, target, counter);
    const auto single = ReadCounter(counter);
    Require(single == Width * Height, what + ": one draw added " + std::to_string(single) + " to the counter, expected " + std::to_string(Width * Height));
    WriteCounter(counter, 0);
    const auto before = AgcDriver::Graphics::DrawPassTotals()[0];
    Draw(device, program, target);
    Draw(device, program, target);
    const auto after = AgcDriver::Graphics::DrawPassTotals()[0];
    Settle(device, target, counter);
    const auto total = ReadCounter(counter);
    Require(total == 2 * single, what + ": two draws added " + std::to_string(total) + " to the counter, expected " + std::to_string(2 * single));
    Require(after.draws - before.draws == 2, what + ": " + std::to_string(after.draws - before.draws) + " draws were recorded, expected 2");
    const auto continued = after.passesContinued - before.passesContinued;
    Require(continued == (expectContinued ? 1u : 0u), what + ": " + std::to_string(continued) + " of the two draws continued a pass, expected " + (expectContinued ? "1" : "0"));
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool split = std::getenv("APS5_NO_ATOMIC_PASSES") != nullptr;
        const Block imported(BlockBytes);
        const bool inPlace = device->DrawInputsInPlace();
        CheckTwoDraws(*device, std::string("a host-imported counter") + (split ? " with APS5_NO_ATOMIC_PASSES=1" : ""), imported.Address(), inPlace && !split);
        CheckTwoDraws(*device, std::string("a copied counter") + (split ? " with APS5_NO_ATOMIC_PASSES=1" : ""), reinterpret_cast<std::uintptr_t>(CopiedCounter.data()), false);
        std::puts("atomic passes: draws only adding into a host-imported counter share a render pass, and every add lands");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
