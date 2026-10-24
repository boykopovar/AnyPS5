#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Vertices = 3;
constexpr std::uint32_t Stride = 16;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Vertices * 4> Output{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<float, Vertices * 4> Elements{-0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 1.0f};

std::array<std::uint32_t, 4> VertexBufferDescriptor(std::uint32_t select, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(Elements.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (Stride << 16u), records, (0x0104dfacu & ~0xfffu) | 0xfacu | (select << 28u)};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

void IndirectDraw(const std::shared_ptr<AgcDriver::VulkanDevice>& device, std::uint32_t* record, std::uint32_t select, std::uint32_t records) {
    const std::string what = "indirect draw with a strided OOB_SELECT " + std::to_string(select) + " descriptor of " + std::to_string(records) + " records";
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor(select, records);
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device->Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertexRequest);
    Require(vertexResult.vertexAttributes.size() == 1u, what + ": the fetch did not become a vertex attribute");
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        device->Target(),
        PixelPushLayout(vertexPush, device->Target())
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, device->Target())}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
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
    AgcDriver::Pm4::DrawParameters draw{0, 0, 0, 0, 0, false, 0, 0};
    draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{reinterpret_cast<std::uintptr_t>(record), 0x24u, 16u, 16u, 1u, false, 0u, 0x280u, 0x280u, 0x280u, false, 0u};
    AgcDriver::DriverDetail::DrawProgram program{};
    program.firstUserSgpr = 8;
    program.userData = userData;
    AgcDriver::DriverDetail::Driver::ClassifyIndirectDraw(vertexResult, state, program, device, draw, false);
    device->Draw(state, draw, shaders);
    device->WaitIdle();
    const auto covered = std::count_if(Pixels.begin(), Pixels.end(), [](std::byte value) { return value != std::byte{0x40}; });
    Require(covered != 0, what + ": nothing was drawn");
}

}

int main() {
    try {
        std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        constexpr std::size_t RecordBytes = 65536;
        auto* record = static_cast<std::uint32_t*>(::operator new(RecordBytes, std::align_val_t{65536}));
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(record, RecordBytes, true, true, true);
        }
        const std::array<std::uint32_t, 4> arguments{Vertices, 1, 0, 0};
        std::copy(arguments.begin(), arguments.end(), record);
        for (const auto select : {2u, 3u}) {
            IndirectDraw(device, record, select, Vertices);
        }
        std::puts("strided indirect vertex fetch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
