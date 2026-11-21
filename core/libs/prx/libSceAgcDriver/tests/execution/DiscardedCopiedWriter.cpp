#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t OutputBytes = Threads * 16u * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t StoredBytes = 8;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> PixelStoreCode{
    0x7e020280, 0x7e0402f2, 0x7e0602f2, 0xe0741000, 0x80000201, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

const ColorTargetLayout TargetLayout(Width, Height, ColorTileMode::RenderTarget, 4);

alignas(256) constexpr std::array<std::uint32_t, 78> LoadsCode{
    0x34020084, 0x34040086, 0xdc388000, 0x04080001, 0xbf8c3f70, 0x7e1402ff, 0x5a5a0000, 0x7e1602ff,
    0x5a5a0001, 0x7e1802ff, 0x5a5a0002, 0x7e1a02ff, 0x5a5a0003, 0x7e1c02ff, 0x5a5a0004, 0x7e1e02ff,
    0x5a5a0005, 0x7e2002ff, 0x5a5a0006, 0x7e2202ff, 0x5a5a0007, 0x7e2402ff, 0x5a5a0008, 0x7e2602ff,
    0x5a5a0009, 0x7e2802ff, 0x5a5a000a, 0x7e2a02ff, 0x5a5a000b, 0x7e2c02ff, 0x5a5a000c, 0x7e2e02ff,
    0x5a5a000d, 0x7e3002ff, 0x5a5a000e, 0x7e3202ff, 0x5a5a000f, 0x4a4008ff, 0x00000a00, 0x4a420eff,
    0x00000200, 0xd70f6a22, 0x02024008, 0x7e460209, 0x50464680, 0x4a4c0aff, 0x00000200, 0xd70f6a24,
    0x02024c08, 0x7e4a0209, 0x504a4a80, 0x36500c81, 0x7daa5080, 0xdc208800, 0x0a080020, 0xdc2487ff,
    0x0b080020, 0xdc288ffd, 0x0c080020, 0xdc2c8001, 0x0d7d0022, 0xdc308fff, 0x0e080020, 0xdc308000,
    0x18080021, 0xdc208800, 0x197d0022, 0xbf8c0070, 0xbefe04c1, 0xe0781000, 0x80010a02, 0xe0781010,
    0x80010e02, 0xe0781020, 0x80011202, 0xe0781030, 0x80011602, 0xbf810000
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "discarded copied writer: cannot allocate a guest block");
        std::memset(block, Fill, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

void RecordDispatch(AgcDriver::VulkanDevice& device, const GuestBlock& input, const GuestBlock& output) {
    std::vector<std::uint32_t> userData(10, 0u);
    const auto address = output.Address();
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), OutputBytes, 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(input.Address());
    userData[9] = static_cast<std::uint32_t>(input.Address() >> 32u);
    const std::span<const std::uint32_t> code(LoadsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

class WatchedTarget {
public:
    WatchedTarget() : bytes(TargetLayout.Bytes()), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
            if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
        } else {
            block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            constexpr std::uintptr_t alignment = 65536;
            void* mapped = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "discarded copied writer: cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
        } else {
            block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, bytes));
        }
#endif
        Require(block != nullptr, "discarded copied writer: cannot allocate the color target");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    ~WatchedTarget() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(block, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
        } else {
            VirtualFree(block, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(block, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
        } else {
            std::free(block);
        }
#endif
    }

    WatchedTarget(const WatchedTarget&) = delete;
    WatchedTarget& operator=(const WatchedTarget&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::size_t bytes;
    bool watched;
    std::uint8_t* block = nullptr;
};

void RecordDraw(AgcDriver::VulkanDevice& device, const WatchedTarget& pixels, const GuestBlock& vertices, const GuestBlock& output) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexAddress = vertices.Address();
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(vertexAddress), static_cast<std::uint32_t>((vertexAddress >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const auto address = output.Address();
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), StoredBytes, 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), pixelUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelStoreCode.data()), std::as_bytes(std::span(PixelStoreCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelStoreCode.data()), PixelStoreCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {pixels.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

bool DrawListed() {
    return !AgcDriver::Graphics::DrawCopiedWriters()->empty();
}

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::_Exit(1);
}

void SetNoHostImport() {
#ifdef _WIN32
    _putenv_s("APS5_NO_HOST_IMPORT", "1");
#else
    setenv("APS5_NO_HOST_IMPORT", "1", 1);
#endif
}

}

int main() {
    try {
        SetNoHostImport();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto& context = device->graphicsContext();
        const GuestBlock input;
        std::memset(input.Data(), 0, BlockBytes);
        const GuestBlock output;
        RecordDispatch(*device, input, output);
        if (!AgcDriver::Graphics::AnyPendingCpuWrites(context)) {
            device->WaitIdle();
            std::puts("skipped, the dispatch's stores were not copied");
            return VulkanTestSkipped;
        }
        device->WaitIdle();
        Check(!AgcDriver::Graphics::AnyPendingCpuWrites(context), "discarded copied writer: a completed dispatch left its copied writer listed");
        Check(std::any_of(output.Data(), output.Data() + OutputBytes, [](std::uint8_t byte) { return byte != Fill; }), "discarded copied writer: the completed dispatch stored nothing");
        std::memset(output.Data(), Fill, BlockBytes);
        AgcDriver::Graphics::Recorder::FailNextSubmitsForTest(1u);
        RecordDispatch(*device, input, output);
        Check(AgcDriver::Graphics::AnyPendingCpuWrites(context), "discarded copied writer: the dispatch before the failed submit was not listed");
        bool discarded = false;
        try {
            device->WaitIdle();
        } catch (const std::exception& error) {
            discarded = std::string(error.what()).find("injected failure") != std::string::npos;
        }
        AgcDriver::Graphics::Recorder::FailNextSubmitsForTest(0u);
        Check(discarded, "discarded copied writer: the batch was not discarded");
        Check(!AgcDriver::Graphics::AnyPendingCpuWrites(context), "discarded copied writer: a discarded dispatch left its copied writer listed");
        Check(std::all_of(output.Data(), output.Data() + OutputBytes, [](std::uint8_t byte) { return byte == Fill; }), "discarded copied writer: a discarded dispatch stored into guest memory");
        RecordDispatch(*device, input, output);
        device->WaitIdle();
        Check(!AgcDriver::Graphics::AnyPendingCpuWrites(context), "discarded copied writer: the dispatch after a discarded batch left its copied writer listed");
        Check(std::any_of(output.Data(), output.Data() + OutputBytes, [](std::uint8_t byte) { return byte != Fill; }), "discarded copied writer: the dispatch after a discarded batch stored nothing");
        const WatchedTarget pixels;
        const GuestBlock vertices;
        std::memcpy(vertices.Data(), Triangle.data(), sizeof(Triangle));
        std::memset(output.Data(), Fill, BlockBytes);
        RecordDraw(*device, pixels, vertices, output);
        if (!DrawListed()) {
            device->WaitIdle();
            std::puts("discarded copied writer tests passed (the draw's stores were not listed: the draw case is not tested)");
            return 0;
        }
        device->WaitIdle();
        Check(!DrawListed(), "discarded copied writer: a completed draw left its copied writer listed");
        Check(std::any_of(output.Data(), output.Data() + StoredBytes, [](std::uint8_t byte) { return byte != Fill; }), "discarded copied writer: the completed draw stored nothing");
        std::memset(output.Data(), Fill, BlockBytes);
        AgcDriver::Graphics::Recorder::FailNextSubmitsForTest(1u);
        RecordDraw(*device, pixels, vertices, output);
        Check(DrawListed(), "discarded copied writer: the draw before the failed submit was not listed");
        bool drawDiscarded = false;
        try {
            device->WaitIdle();
        } catch (const std::exception& error) {
            drawDiscarded = std::string(error.what()).find("injected failure") != std::string::npos;
        }
        AgcDriver::Graphics::Recorder::FailNextSubmitsForTest(0u);
        Check(drawDiscarded, "discarded copied writer: the draw's batch was not discarded");
        Check(!DrawListed(), "discarded copied writer: a discarded draw left its copied writer listed");
        Check(!AgcDriver::Graphics::AnyPendingCpuWrites(context), "discarded copied writer: a discarded draw left CPU writes pending");
        Check(std::all_of(output.Data(), output.Data() + StoredBytes, [](std::uint8_t byte) { return byte == Fill; }), "discarded copied writer: a discarded draw stored into guest memory");
        RecordDraw(*device, pixels, vertices, output);
        device->WaitIdle();
        Check(!DrawListed(), "discarded copied writer: the draw after a discarded batch left its copied writer listed");
        Check(std::any_of(output.Data(), output.Data() + StoredBytes, [](std::uint8_t byte) { return byte != Fill; }), "discarded copied writer: the draw after a discarded batch stored nothing");
        std::puts("discarded copied writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
