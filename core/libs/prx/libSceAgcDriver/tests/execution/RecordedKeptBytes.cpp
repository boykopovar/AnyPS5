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
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::size_t BufferBytes = 65536;
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;

alignas(256) constexpr std::array<std::uint32_t, 6> StoreCode{
    0x34020083, 0x7e040280, 0x7e060280, 0xe0741000, 0x80000201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> RawBuffer(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> VertexBuffer(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

class GuestBuffer {
public:
    explicit GuestBuffer(bool registered, std::size_t bytes = BufferBytes) : registered(registered), bytes(bytes), watched(registered && AgcDriver::GuestMemory::WriteWatched()) {
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
            Require(mapped != MAP_FAILED, "recorded kept bytes: cannot map a guest buffer");
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
        Require(block != nullptr, "recorded kept bytes: cannot allocate a guest buffer");
        std::memset(block, 0, bytes);
        if (registered) GuestAllocations::Mutation().Add(block, bytes, true, true);
    }

    ~GuestBuffer() {
        if (registered) {
            AgcDriver::Graphics::StorageTexture::FlushPending(AddressOf(block), bytes, nullptr, "test release");
            GuestAllocations::Mutation().Remove(block);
        }
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

    GuestBuffer(const GuestBuffer&) = delete;
    GuestBuffer& operator=(const GuestBuffer&) = delete;

    std::uint8_t* Data() { return block; }

private:
    bool registered;
    std::size_t bytes;
    bool watched;
    std::uint8_t* block = nullptr;
};

Recorder& ActiveRecorder() {
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "recorded kept bytes: no active recorder");
    return *recorder;
}

std::string Bytes(std::size_t bytes) {
    return std::to_string(bytes) + " bytes";
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* target) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto descriptor = RawBuffer(target, static_cast<std::uint32_t>(BufferBytes));
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    const std::span<const std::uint32_t> code(StoreCode);
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
}

const ColorTargetLayout TargetLayout(Width, Height, ColorTileMode::RenderTarget, 4);

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

void FillVertices(GuestBuffer& vertices) {
    std::memcpy(vertices.Data(), Triangle.data(), sizeof(Triangle));
}

void Draw(AgcDriver::VulkanDevice& device, GuestBuffer& pixels, GuestBuffer& vertices) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = VertexBuffer(vertices.Data(), 16u, static_cast<std::uint32_t>(BufferBytes / 16u));
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
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {AddressOf(pixels.Data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
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

void CopiedDispatch(AgcDriver::VulkanDevice& device) {
    GuestBuffer target(false);
    device.WaitIdle();
    Dispatch(device, target.Data());
    const auto& recorder = ActiveRecorder();
    Require(recorder.Recording(), "recorded kept bytes: the dispatch was not left in the open batch");
    const auto kept = recorder.OpenKeptBytes();
    Require(kept >= BufferBytes, "a dispatch writing a copied " + Bytes(BufferBytes) + " buffer left " + Bytes(kept) + " in its batch's kept bytes");
    device.WaitIdle();
}

void ResidentTarget(AgcDriver::VulkanDevice& device, GuestBuffer& pixels) {
    GuestBuffer vertices(false);
    FillVertices(vertices);
    Draw(device, pixels, vertices);
    device.WaitIdle();
}

void CopiedDraw(AgcDriver::VulkanDevice& device, GuestBuffer& pixels) {
    GuestBuffer vertices(false);
    FillVertices(vertices);
    device.WaitIdle();
    Draw(device, pixels, vertices);
    const auto& recorder = ActiveRecorder();
    Require(recorder.Recording(), "recorded kept bytes: the draw was not left in the open batch");
    const auto kept = recorder.OpenKeptBytes();
    Require(kept >= BufferBytes, "a draw reading a copied " + Bytes(BufferBytes) + " buffer left " + Bytes(kept) + " in its batch's kept bytes");
    device.WaitIdle();
}

bool SnapshotDraws(AgcDriver::VulkanDevice& device, GuestBuffer& pixels) {
    GuestBuffer vertices(true);
    FillVertices(vertices);
    device.WaitIdle();
    Draw(device, pixels, vertices);
    auto& recorder = ActiveRecorder();
    Require(recorder.Recording(), "recorded kept bytes: the snapshot draw was not left in the open batch");
    if (recorder.ReusableDrawSnapshot(AddressOf(vertices.Data()), BufferBytes) == nullptr) {
        std::puts("the draw took no reusable snapshot (no host import, or the buffer is not write-watched): draw snapshots are not tested");
        device.WaitIdle();
        return false;
    }
    const auto first = recorder.OpenKeptBytes();
    Require(first >= BufferBytes, "a draw snapshotting a " + Bytes(BufferBytes) + " buffer left " + Bytes(first) + " in its batch's kept bytes");
    Draw(device, pixels, vertices);
    Require(recorder.Recording(), "recorded kept bytes: the second snapshot draw was not left in the open batch");
    const auto second = recorder.OpenKeptBytes();
    Require(second == first, "a second draw reusing the batch's snapshot raised its kept bytes from " + Bytes(first) + " to " + Bytes(second));
    device.WaitIdle();
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CopiedDispatch(*device);
        GuestBuffer pixels(true, TargetLayout.Bytes());
        ResidentTarget(*device, pixels);
        CopiedDraw(*device, pixels);
        const bool snapshots = SnapshotDraws(*device, pixels);
        std::printf("recorded kept bytes tests passed%s\n", snapshots ? "" : " (draw snapshots not tested)");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
