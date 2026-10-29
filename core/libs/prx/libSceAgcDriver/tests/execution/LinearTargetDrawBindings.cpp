#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
constexpr std::size_t CopyTargetOffset = 0x40000;
constexpr std::size_t FirstSourceOffset = 0x80000;
constexpr std::size_t SecondSourceOffset = 0x90000;
constexpr std::size_t PointedOffset = 0xa0000;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint32_t FirstSource = 0x3e800000u;
constexpr std::uint32_t SecondSource = 0x3f400000u;
constexpr std::uint32_t Pointed = 0x3f800000u;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelOpaque{0x7e100280u, 0x7e120280u, 0x7e140280u, 0x7e1602f2u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> PixelRead{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 13> PixelCopy{0x7e040280u, 0xe0301000u, 0x80010102u, 0xbf8c3f70u, 0xe0701000u, 0x80020102u, 0x7e100301u, 0x7e120280u, 0x7e140280u, 0x7e1602f2u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, 4> FirstOutput{};
alignas(256) std::array<std::uint32_t, 4> SecondOutput{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 4> RawBuffer(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
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
            Require(raw != MAP_FAILED, "cannot map the color targets");
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
        Require(data != nullptr, "cannot allocate the color targets");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color targets are not write-watched");
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

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t address) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode, std::span<const std::uint32_t> pixelUserData, VkRect2D scissor) {
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

std::array<std::uint32_t, 12> CopyUserData(std::uint64_t source, const std::array<std::uint32_t, 4>& output) {
    std::array<std::uint32_t, 12> userData{};
    const auto read = RawBuffer(source, 16u);
    const auto written = RawBuffer(reinterpret_cast<std::uintptr_t>(output.data()), 16u);
    std::copy(read.begin(), read.end(), userData.begin() + 4);
    std::copy(written.begin(), written.end(), userData.begin() + 8);
    return userData;
}

std::array<float, 4> Texel(const std::uint8_t* target, std::uint32_t x, std::uint32_t y) {
    std::array<float, 4> texel{};
    std::memcpy(texel.data(), target + (static_cast<std::size_t>(y) * Width + x) * 16u, sizeof(texel));
    return texel;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Describe(const std::array<float, 4>& texel) {
    return "(" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + ")";
}

void ExpectTexels(const std::uint8_t* target, const char* what, std::uint32_t firstX, std::uint32_t lastX, const std::array<float, 4>& expected) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = firstX; x < lastX; ++x) {
            const auto texel = Texel(target, x, y);
            Require(texel == expected, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds " + Describe(texel) + ", expected " + Describe(expected));
        }
    }
}

bool Expect(AgcDriver::VulkanDevice& device, bool unwatch) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!context.bufferDeviceAddress) {
        std::puts("skipped, the device lacks bufferDeviceAddress");
        return false;
    }
    Block block(16 * BlockBytes);
    std::fill(block.data, block.data + block.bytes, std::uint8_t{0});
    const auto put = [&](std::size_t offset, std::uint32_t value, std::size_t dwords) {
        for (std::size_t dword = 0; dword < dwords; ++dword) std::memcpy(block.data + offset + dword * 4u, &value, sizeof(value));
    };
    put(FirstSourceOffset, FirstSource, 4);
    put(SecondSourceOffset, SecondSource, 4);
    put(PointedOffset, Pointed, 1);
    FirstOutput.fill(Unwritten);
    SecondOutput.fill(Unwritten);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) {
        std::puts("skipped, host imports are unavailable");
        return false;
    }
    const auto drawn = DecodeTarget(block.Address());
    const auto copied = DecodeTarget(block.Address() + CopyTargetOffset);
    Require(drawn.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && drawn.bytes == TargetBytes && copied.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && copied.bytes == TargetBytes, "the targets are not linear R32G32B32A32 targets");
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr && context.descriptorCache != nullptr, "the device has no recorder or descriptor cache");
    const auto pending = [](std::uint64_t address) { return StorageTexture::FindPending(address, TargetBytes) != nullptr; };
    const VkRect2D whole{{0, 0}, {Width, Height}};
    Draw(device, drawn, PixelOpaque, {}, whole);
    Draw(device, copied, PixelOpaque, {}, whole);
    device.WaitIdle();
    Require(!pending(drawn.address) && !pending(copied.address), "a linear target was still pending after the device wait");
    if (unwatch) AgcDriver::GuestMemory::Unwatch(block.Address(), block.bytes);
    const bool watched = AgcDriver::GuestMemory::Watched(copied.address, TargetBytes);
    Require(!unwatch || !watched, "the copied target is still write-watched after its block was taken out of the write watch");
    const auto submitted = recorder->Submissions();
    const auto pointed = block.Address() + PointedOffset;
    const std::array<std::uint32_t, 4> pointer{0u, 0u, static_cast<std::uint32_t>(pointed), static_cast<std::uint32_t>(pointed >> 32u)};
    Draw(device, drawn, PixelRead, pointer, whole);
    Require(pending(drawn.address), "the address-based draw did not leave its linear target pending");
    Require(recorder->Recording() && recorder->Submissions() == submitted, "the address-based draw did not stay in the open batch");
    const auto setsBefore = context.descriptorCache->Counters().sets;
    const auto firstCopy = CopyUserData(block.Address() + FirstSourceOffset, FirstOutput);
    Draw(device, copied, PixelCopy, firstCopy, {{0, 0}, {Width / 2u, Height}});
    Require(!pending(drawn.address), "the draw writing a copied buffer did not store the drawn linear target first");
    Require(pending(copied.address), "the first copying draw did not leave its linear target pending");
    Require(recorder->Recording() && recorder->Submissions() == submitted + 1, "storing the drawn linear target did not submit the batch the address-based draw read it in (" + std::to_string(recorder->Submissions() - submitted) + " submissions)");
    const auto setsBetween = context.descriptorCache->Counters().sets;
    const auto secondCopy = CopyUserData(block.Address() + SecondSourceOffset, SecondOutput);
    Draw(device, copied, PixelCopy, secondCopy, {{static_cast<std::int32_t>(Width / 2u), 0}, {Width / 2u, Height}});
    if (watched) Require(recorder->Recording() && recorder->Submissions() == submitted + 1, "the second copying draw submitted recorded work");
    else Require(recorder->Submissions() == submitted + 2, "the second copying draw into a stored target outside the write watch did not submit once to capture its bytes (" + std::to_string(recorder->Submissions() - submitted - 1) + " submissions)");
    const auto setsAfter = context.descriptorCache->Counters().sets;
    device.WaitIdle();
    Require(FirstOutput[0] == FirstSource, "the first copying draw's output holds " + Hex(FirstOutput[0]) + ", not its source's " + Hex(FirstSource));
    Require(SecondOutput[0] == SecondSource, "the second copying draw's output holds " + Hex(SecondOutput[0]) + ", not its source's " + Hex(SecondSource));
    ExpectTexels(block.data + CopyTargetOffset, "the first copying draw's half", 0, Width / 2u, {0.25f, 0.0f, 0.0f, 1.0f});
    ExpectTexels(block.data + CopyTargetOffset, "the second copying draw's half", Width / 2u, Width, {0.75f, 0.0f, 0.0f, 1.0f});
    ExpectTexels(block.data, "the address-based draw's target", 0, Width, {1.0f, 0.0f, 0.0f, 0.0f});
    Require(setsAfter - setsBetween == setsBetween - setsBefore, "the second copying draw allocated " + std::to_string(setsAfter - setsBetween) + " descriptor sets, the first " + std::to_string(setsBetween - setsBefore));
    return true;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_RELEASE_ON_UNLOCK", "1");
#else
    setenv("APS5_RELEASE_ON_UNLOCK", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Expect(*device, false)) return VulkanTestSkipped;
        if (!Expect(*device, true)) return VulkanTestSkipped;
        std::puts("linear target draw binding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
