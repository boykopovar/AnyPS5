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
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TargetBytes = std::size_t{Width} * Height * 16u;
constexpr std::size_t LabelOffset = BlockBytes;
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
        StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
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

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode, VkRect2D scissor) {
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
    state.scissor = scissor;
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
    std::memcpy(texel.data(), target + (static_cast<std::size_t>(y) * Width + x) * 16u, sizeof(texel));
    return texel;
}

void Label(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t value, std::uint64_t submissions, bool watched, const char* what) {
    const auto bytes = std::as_bytes(std::span(&value, 1));
    const auto reason = device.WriteLabelOnGpu(block.Address() + LabelOffset, bytes, value, 0);
    Require(reason == 0, std::string(what) + ": the label was not recorded on the GPU (outcome " + std::to_string(reason) + ")");
    Require(StorageTexture::FindPending(block.Address(), TargetBytes) == nullptr, std::string(what) + ": the drawn linear target was still pending after a label the GPU writes");
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(!watched || (recorder != nullptr && recorder->Submissions() == submissions), std::string(what) + ": storing the drawn linear target before the label waited for recorded work (" + std::to_string(recorder != nullptr ? recorder->Submissions() - submissions : 0) + " submissions)");
}

bool Expect(AgcDriver::VulkanDevice& device) {
    Block block(16 * BlockBytes);
    std::fill(block.data, block.data + block.bytes, std::uint8_t{0xff});
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) return false;
    const auto color = DecodeTarget(block);
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.bytes == TargetBytes, "the target is not a linear R32G32B32A32 target");
    const auto red = Program(0, false);
    const auto blue = Program(1, true);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "the device has no recorder");
    const bool watched = AgcDriver::GuestMemory::Watched(block.Address(), TargetBytes);
    if (!watched) std::puts("linear target completion stores: the target is outside the write watch, so a store may wait for the previous store's saved bytes; the submission checks are skipped");
    const auto pending = [&](const char* what) {
        Require(StorageTexture::FindPending(block.Address(), TargetBytes) != nullptr, std::string(what) + ": the drawn linear target was stored before any point where its completion is observable");
    };
    Draw(device, color, red, {{0, 0}, {Width, Height}});
    pending("after the first draw");
    const auto submissions = recorder->Submissions();
    Draw(device, color, blue, {{0, 0}, {Width / 2u, Height}});
    pending("after the second draw");
    Require(!watched || recorder->Submissions() == submissions, "the second draw into the linear target submitted recorded work");
    Draw(device, color, red, {{Width / 4u, Height / 4u}, {Width / 4u, Height / 2u}});
    pending("after the third draw");
    Require(!watched || recorder->Submissions() == submissions, "the third draw into the linear target submitted recorded work");
    Label(device, block, 1u, submissions, watched, "the first label");
    Draw(device, color, blue, {{Width / 2u, 0}, {Width / 2u, Height}});
    pending("after the fourth draw");
    Label(device, block, 2u, submissions, watched, "the second label");
    Draw(device, color, red, {{0, Height / 2u}, {Width, Height / 2u}});
    pending("after the fifth draw");
    Require(!watched || recorder->Submissions() == submissions, "the fifth draw into the linear target submitted recorded work");
    device.WaitIdle();
    Require(StorageTexture::FindPending(block.Address(), TargetBytes) == nullptr, "the drawn linear target was still pending after the device wait");
    std::vector<std::uint8_t> stored(TargetBytes);
    std::memcpy(stored.data(), block.data, TargetBytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const bool lastRed = y >= Height / 2u;
            const bool lastBlue = !lastRed && x >= Width / 2u;
            const bool thirdRed = !lastRed && !lastBlue && x >= Width / 4u && x < Width / 2u && y >= Height / 4u;
            const bool secondBlue = !lastRed && !lastBlue && !thirdRed;
            const float z = lastRed || thirdRed ? 0.0f : secondBlue || lastBlue ? 1.0f : -1.0f;
            const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), z, 1.0f};
            const auto texel = Texel(stored.data(), x, y);
            Require(texel == expected, "after the device wait, raw guest read: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds (" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + ")");
        }
    }
    std::uint32_t label = 0;
    std::memcpy(&label, block.data + LabelOffset, sizeof(label));
    Require(label == 2u, "the second label did not land: it holds " + std::to_string(label));
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Expect(*device)) {
            std::puts("linear target completion store test skipped: no host import on this device");
            return VulkanTestSkipped;
        }
        std::puts("linear target completion store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
