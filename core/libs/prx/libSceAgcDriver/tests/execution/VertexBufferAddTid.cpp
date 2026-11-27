#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 32u;
constexpr std::uint32_t Height = 32u;
constexpr std::uint32_t Vertices = 96u;
constexpr std::uint32_t BufferBytes = 4096u;
constexpr std::uint32_t StructuredFormat = 0x01016facu;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t AddTid = 1u << 23u;
constexpr std::uint32_t Stored = 0x70000000u;
constexpr std::uint8_t Fill = 0xa5u;
alignas(256) std::array<float, Width * Height * 4u> Pixels{};
alignas(256) std::array<std::uint8_t, BufferBytes> Source{};
alignas(256) std::array<std::uint8_t, BufferBytes> Target{};
alignas(256) std::array<std::uint32_t, Vertices * 2u> Result{};
alignas(256) std::array<std::uint32_t, 32> VertexCode{};
alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0002f2u, 0xf800180fu, 0x00000000u, 0xbf810000u};
std::uint32_t Failures = 0u;

struct Case {
    bool offen;
    std::uint32_t offset;
    std::uint32_t immediate;
    std::uint32_t stride;
    std::uint32_t indexStride;
    bool swizzle;
};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t records, std::uint32_t stride, std::uint32_t word1Flags, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u) | word1Flags, records, format};
}

std::uint32_t Address(const Case& test, std::uint32_t lane) {
    const auto offset = (test.offen ? test.offset : 0u) + test.immediate;
    if (!test.swizzle) return lane * test.stride + offset;
    const auto step = 8u << test.indexStride;
    return ((lane / step) * test.stride + (offset & ~3u)) * step + (lane % step) * 4u + (offset & 3u);
}

std::uint32_t Mubuf(std::uint32_t opcode, const Case& test) {
    return 0xe0000000u | (opcode << 18u) | (test.offen ? 0x1000u : 0u) | test.immediate;
}

std::vector<std::uint32_t> Program(const Case& test) {
    std::vector<std::uint32_t> code{0xd7650001u, 0x000100c1u, 0xd7660001u, 0x000202c1u};
    if (test.offen) code.push_back(0x7e080280u + test.offset);
    code.insert(code.end(), {Mubuf(12u, test), 0x80020204u, 0x4a0602ffu, Stored, Mubuf(28u, test), 0x80000304u, 0x340c0a83u, 0xbf8c3f70u, 0xe0741000u, 0x80030106u});
    code.insert(code.end(), {0x7e100280u, 0x7e120280u, 0x7e140280u, 0x7e1602f2u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u});
    return code;
}

void Draw(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    const auto program = Program(test);
    Require(program.size() <= VertexCode.size(), "ADD_TID vertex program is too long");
    VertexCode.fill(0u);
    std::copy(program.begin(), program.end(), VertexCode.begin());
    const std::span<const std::uint32_t> words(VertexCode.data(), program.size());
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(16, 0u);
    const auto flags = test.swizzle ? 0x80000000u : 0u;
    const auto format = StructuredFormat | (test.indexStride << 21u) | AddTid;
    const auto load = Descriptor(Source.data(), BufferBytes / test.stride, test.stride, flags, format);
    const auto output = Descriptor(Result.data(), sizeof(Result), 0u, 0u, RawFormat);
    const auto store = Descriptor(Target.data(), BufferBytes / test.stride, test.stride, flags, format);
    std::copy(load.begin(), load.end(), vertexUserData.begin() + 8);
    std::copy(output.begin(), output.end(), vertexUserData.begin() + 12);
    std::copy(store.begin(), store.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(words)}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), words, 0, {}},
        {wave, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {wave, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}, {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, wave, wave, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Pixels), 0xe4u, AgcDriver::Graphics::ColorTileMode::Linear, 16u};
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
    device.Draw(state, AgcDriver::Pm4::DrawParameters{0, Vertices, 0, 1, 0, false}, shaders);
    device.WaitIdle();
}

void Report(const Case& test, std::uint32_t wave, const char* what, std::uint32_t at, std::uint32_t actual, std::uint32_t expected) {
    if (++Failures > 24u) return;
    std::fprintf(stderr, "wave=%u offen=%d offset=%u immediate=%u stride=%u indexStride=%u swizzle=%d %s at=%u actual=%08x expected=%08x\n",
                 wave, test.offen, test.offset, test.immediate, test.stride, test.indexStride, test.swizzle, what, at, actual, expected);
}

std::uint32_t Word(const std::array<std::uint8_t, BufferBytes>& bytes, std::uint32_t address) {
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0u; byte < 4u; ++byte) value |= static_cast<std::uint32_t>(bytes[address + byte]) << (8u * byte);
    return value;
}

void Verify(const Case& test, std::uint32_t wave) {
    std::array<bool, 64> reported{};
    for (std::uint32_t vertex = 0u; vertex < Vertices; ++vertex) {
        const auto lane = Result[vertex * 2u];
        if (lane >= wave) {
            Report(test, wave, "lane", vertex, lane, wave);
            continue;
        }
        reported[lane] = true;
        const auto expected = Word(Source, Address(test, lane));
        if (Result[vertex * 2u + 1u] != expected) Report(test, wave, "load", vertex, Result[vertex * 2u + 1u], expected);
    }
    std::array<bool, BufferBytes> slot{};
    for (std::uint32_t lane = 0u; lane < wave; ++lane) {
        const auto address = Address(test, lane);
        std::fill_n(slot.begin() + address, 4u, true);
        const auto actual = Word(Target, address);
        if (actual != Stored + lane && (reported[lane] || actual != 0xa5a5a5a5u)) Report(test, wave, "store", address, actual, Stored + lane);
    }
    for (std::uint32_t index = 0u; index < BufferBytes; ++index) {
        if (!slot[index] && Target[index] != Fill) Report(test, wave, "stray store", index, Target[index], Fill);
    }
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    for (std::uint32_t index = 0u; index < BufferBytes / 4u; ++index) {
        const auto word = index * 0x9e3779b1u + 0x1234567u;
        for (std::uint32_t byte = 0u; byte < 4u; ++byte) Source[index * 4u + byte] = static_cast<std::uint8_t>(word >> (8u * byte));
    }
    Target.fill(Fill);
    Result.fill(0xffffffffu);
    Draw(device, test, wave);
    Verify(test, wave);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        constexpr std::array<Case, 6> cases{{
            {false, 0u, 0u, 16u, 0u, true},
            {true, 4u, 0u, 16u, 0u, true},
            {true, 8u, 4u, 32u, 1u, true},
            {false, 0u, 12u, 16u, 3u, true},
            {true, 12u, 0u, 16u, 2u, false},
            {false, 0u, 4u, 32u, 0u, false},
        }};
        for (const auto wave : {32u, 64u}) {
            for (const auto& test : cases) Run(*device, test, wave);
        }
        Require(Failures == 0u, "vertex buffer accesses with ADD_TID do not address the lane");
        std::puts("vertex buffer ADD_TID addresses the lane, wave32/wave64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
