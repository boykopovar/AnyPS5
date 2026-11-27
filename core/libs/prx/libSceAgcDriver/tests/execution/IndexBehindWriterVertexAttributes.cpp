#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "../WatchedBlock.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::IndexBehindWriter;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::RuntimeSwitch;
using ShaderRecompiler::ShaderStage;
namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Records = 16;
constexpr std::uint32_t Stride = 32;
constexpr std::uint32_t Format = 77;
constexpr std::uint32_t OutOfBoundsSelect = 2;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t IndexOffset = 4096;
constexpr std::uint32_t DrawnIndices = 6;

alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Records * 4u> Output{};
alignas(256) std::array<std::array<float, Stride / 4u>, Records> Elements{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

constexpr std::array<std::uint16_t, 8> StaleIndices{0, 1, 2, 0, 2, 1, 0, 1};
constexpr std::array<std::uint16_t, 8> GpuIndices{0, 9, 15, 3, 12, 7, 0, 9};

std::array<std::uint32_t, 4> Pattern(const std::array<std::uint16_t, 8>& indices) {
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), indices.data(), sizeof(pattern));
    return pattern;
}

std::array<std::uint32_t, 4> VertexBufferDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Elements.front().data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (Stride << 16u), Records, (0x0104dfacu & ~0x7ffffu) | (Format << 12u) | 0xfacu | (OutOfBoundsSelect << 28u)};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

struct Program {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
};

std::unique_ptr<Program> Compile(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor();
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    auto program = std::make_unique<Program>();
    program->vertex = ShaderRecompiler::Recompile(vertexRequest);
    Require(program->vertex.vertexAttributes.size() == 1u, "the vertex fetch did not become a fixed vertex attribute");
    const auto vertexPush = static_cast<std::uint32_t>(program->vertex.pushConstants.size());
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
        device.Target(),
        PixelPushLayout(vertexPush, device.Target())
    };
    fragment.useCache = false;
    program->pixel = ShaderRecompiler::Recompile(fragment);
    program->shaders = {{{ShaderStage::Vertex, &program->vertex, 0}, {ShaderStage::Fragment, &program->pixel, PixelPushOffset(vertexPush, device.Target())}}};
    return program;
}

AgcDriver::Graphics::State DrawState() {
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
    return state;
}

std::array<std::uint32_t, Records * 4u> Expected() {
    std::array<std::uint32_t, Records * 4u> expected{};
    expected.fill(Unwritten);
    for (std::uint32_t i = 0; i < DrawnIndices; ++i) {
        const auto vertex = GpuIndices[i];
        for (std::uint32_t component = 0; component < 4u; ++component) expected[vertex * 4u + component] = std::bit_cast<std::uint32_t>(Elements[vertex][component]);
    }
    return expected;
}

std::array<std::uint32_t, Records * 4u> Run(AgcDriver::VulkanDevice& device, const Program& program, std::uint64_t indices, bool behindWriter, const std::string& what) {
    RuntimeSwitches::Assign(RuntimeSwitch::IndexInPlaceBehindWriter, behindWriter);
    std::memcpy(reinterpret_cast<void*>(indices), StaleIndices.data(), sizeof(StaleIndices));
    Pixels.fill(std::byte{0x40});
    Output.fill(Unwritten);
    const auto pattern = Pattern(GpuIndices);
    const auto state = DrawState();
    const AgcDriver::Pm4::DrawParameters draw{indices, DrawnIndices, 2, 1, 0, true, 0, 0};
    const auto before = AgcDriver::Graphics::IndexBehindWriterCounts();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(device.FillBuffer(indices, sizeof(GpuIndices), pattern), what + ": the fill of the index range was not recorded on the GPU");
        Require(device.OpenWriteOverlaps(indices, sizeof(GpuIndices)), what + ": the fill of the index range is not a pending write in the open batch");
        device.Draw(state, draw, program.shaders);
    }
    const auto after = AgcDriver::Graphics::IndexBehindWriterCounts();
    device.WaitIdle();
    for (std::size_t outcome = 0; outcome < after.size(); ++outcome) {
        const auto expected = behindWriter && outcome == static_cast<std::size_t>(IndexBehindWriter::Taken) ? 1u : 0u;
        Require(after[outcome] - before[outcome] == expected, what + ": index behind writer outcome " + std::to_string(outcome) + " counted " + std::to_string(after[outcome] - before[outcome]) + ", expected " + std::to_string(expected));
    }
    Require(std::memcmp(reinterpret_cast<const void*>(indices), GpuIndices.data(), sizeof(GpuIndices)) == 0, what + ": guest memory does not hold the indices the GPU wrote");
    const auto expected = Expected();
    for (std::uint32_t vertex = 0; vertex < Records; ++vertex) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto slot = vertex * 4u + component;
            Require(Output[slot] == expected[slot], what + ": vertex " + std::to_string(vertex) + " v" + std::to_string(8u + component) + " is " + std::to_string(Output[slot]) + ", expected " + std::to_string(expected[slot]));
        }
    }
    return Output;
}

int Test(AgcDriver::VulkanDevice& device) {
    const auto context = device.graphicsContext();
    if (context.hostImportAlignment == 0) {
        std::puts("skipped, host imports are unavailable: indices behind a pending GPU writer are read in place from one");
        return VulkanTestSkipped;
    }
    if (context.limits.maxDrawIndexedIndexValue != std::numeric_limits<std::uint32_t>::max()) {
        std::puts("skipped, the device limits indexed draws: indices behind a pending GPU writer need the full index range");
        return VulkanTestSkipped;
    }
    void* block = AllocateWatched(BlockBytes, BlockBytes);
    if (block == nullptr) {
        std::puts("skipped, guest write watching is unavailable");
        return VulkanTestSkipped;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, BlockBytes); }
    } release{block};
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, BlockBytes, true, true, true);
    }
    struct Unregister {
        AgcDriver::VulkanDevice& device;
        void* block;
        ~Unregister() {
            device.WaitIdle();
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            AgcDriver::Graphics::HostImportFor(device.graphicsContext(), reinterpret_cast<std::uint64_t>(block), BlockBytes);
        }
    } unregister{device, block};
    const bool configured = RuntimeSwitches::Set(RuntimeSwitch::IndexInPlaceBehindWriter);
    struct Restore {
        bool configured;
        ~Restore() { RuntimeSwitches::Assign(RuntimeSwitch::IndexInPlaceBehindWriter, configured); }
    } restore{configured};
    for (std::uint32_t record = 0; record < Records; ++record) {
        const auto value = static_cast<float>(record);
        Elements[record] = {value * 0.125f - 1.0f, 0.75f - value * 0.0625f, 0.5f, 1.0f + value, 0.0f, 0.0f, 0.0f, 0.0f};
    }
    const auto program = Compile(device);
    const auto indices = reinterpret_cast<std::uint64_t>(block) + IndexOffset;
    const auto behind = Run(device, *program, indices, true, "indices bound behind their GPU writer");
    const auto copied = Run(device, *program, indices, false, "indices read after their GPU writer");
    Require(behind == copied, "the draw behind the GPU writer of its indices fetched other vertex attributes than the draw after it");
    return 0;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto result = Test(*device);
        if (result == 0) std::puts("index behind writer vertex attribute tests passed");
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
