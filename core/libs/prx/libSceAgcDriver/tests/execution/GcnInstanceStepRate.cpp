#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 8;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::uint32_t Instances = 7;
constexpr std::uint32_t RecordBytes = 16;
constexpr std::size_t Slot = 0x10000;
constexpr std::size_t BlockBytes = 2u * Slot;
constexpr std::size_t ColorOffset = 0u;
constexpr std::size_t RecordOffset = Slot;
constexpr std::size_t RecordSpacing = 0x1000;
constexpr std::array<std::uint32_t, 4> StepRates{1u, 2u, 3u, 8u};
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;

alignas(256) constexpr std::array<std::uint32_t, 12> VertexCode{
    0xe0702000u, 0x80000103u,
    0xe0702004u, 0x80000203u,
    0xe0702008u, 0x80000303u,
    0x7e0802f4u,
    0x7e0a02f2u,
    0xbf8c0000u,
    0xf80008cfu, 0x05040404u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, 0x7e0e02f2u,
    0xf800180fu, 0x07060504u,
    0xbf810000u,
};

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t offset) const { return Address() + offset; }
    std::uint8_t* Data(std::size_t offset) const { return data + offset; }

private:
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t color) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(color >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = 0u;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp;
    cx[0x31d] = Thin1D;
    return AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    return pixel;
}

std::array<std::uint32_t, Instances * RecordBytes / 4u> Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, std::size_t recordOffset, std::uint32_t stepRate) {
    std::array<std::uint32_t, Instances * RecordBytes / 4u> records{};
    const auto target = device.Target();
    const auto address = block.At(recordOffset);
    const std::vector<std::uint32_t> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (RecordBytes << 16u), Instances * RecordBytes, 0x00027facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.instanceStepRate = stepRate;
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, PixelInfo(), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const auto color = DecodeTarget(block.At(ColorOffset));
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.stages.legacyVertex = true;
    state.stages.instanceStepRate = stepRate;
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = 1;
    const AgcDriver::Pm4::DrawParameters draw{0, 3, 0, Instances, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(records)), 4);
    return records;
}

void Check(AgcDriver::VulkanDevice& device, const LowBlock& block, std::size_t recordOffset, std::uint32_t stepRate) {
    const auto records = Draw(device, block, recordOffset, stepRate);
    for (std::uint32_t instance = 0; instance < Instances; ++instance) {
        const auto* record = records.data() + instance * RecordBytes / 4u;
        const std::string name = "GCN instance step rate " + std::to_string(stepRate) + ", instance " + std::to_string(instance) + ": ";
        Require(record[0] == instance / stepRate, name + "v1 is " + std::to_string(record[0]) + ", not the instance divided by VGT_INSTANCE_STEP_RATE_0 (" + std::to_string(instance / stepRate) + ")");
        Require(record[1] == instance, name + "v2 is " + std::to_string(record[1]) + ", not the instance");
        Require(record[2] == instance, name + "v3 is " + std::to_string(record[2]) + ", not the instance");
        Require(record[3] == Unwritten, name + "the vertex program wrote past its three VGPRs");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("skipped, no guest block below the 40-bit GCN address range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        for (std::size_t rate = 0; rate < StepRates.size(); ++rate) {
            auto* words = reinterpret_cast<std::uint32_t*>(block.Data(RecordOffset + rate * RecordSpacing));
            std::fill_n(words, Instances * RecordBytes / 4u, Unwritten);
        }
        for (std::size_t rate = 0; rate < StepRates.size(); ++rate) Check(*device, block, RecordOffset + rate * RecordSpacing, StepRates[rate]);
        std::printf("GCN instance step rate tests passed: v1 holds the instance divided by VGT_INSTANCE_STEP_RATE_0 1, 2, 3 and 8 over %u instances, v2 and v3 the instance\n", Instances);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
