#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Records = 4096;
constexpr std::uint32_t OutputVertices = 32;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, OutputVertices * 4> Output{};
alignas(256) std::array<std::uint32_t, Records * 4> Ring{};
alignas(256) std::array<std::uint16_t, 4> Indices{0, 2, 1, 0};
constexpr std::uint32_t ThreeVerticesBytes = 48;
constexpr std::size_t GuestBlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe00c2000u, 0x80020805u, 0xbf8c3f70u, 0x34020a84u, 0xe0781000u, 0x80010801u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 6> ReadOnlyVertexCode{
    0xe00c2000u, 0x80020805u, 0xbf8c3f70u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> InstanceCode{
    0x34021084u, 0xe00c2000u, 0x80020808u, 0xbf8c3f70u, 0xe0781000u, 0x80010801u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

struct Case {
    const char* what;
    bool indexed;
    std::uint32_t count;
    std::uint32_t firstVertex;
    std::array<std::uint32_t, 3> vertices;
    std::uint32_t instances = 0;
    std::uint32_t firstInstance = 0;
};

std::array<std::uint32_t, 4> Descriptor(const void* pointer, std::uint32_t stride, std::uint32_t records, std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return {static_cast<std::uint32_t>(address), (static_cast<std::uint32_t>(address >> 32u) & 0xffffu) | (stride << 16u), records, word3};
}

struct Programs {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    std::uint32_t fragmentPush = 0;

    std::array<AgcDriver::Graphics::CompiledShader, 2> Shaders() const {
        return {{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &fragment, fragmentPush}}};
    }

    std::vector<ShaderRecompiler::DescriptorBinding>::const_iterator GuestBuffers() const {
        return std::ranges::find_if(vertex.bindings, [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers; });
    }
};

Programs Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> vertexCode, const std::vector<std::uint32_t>& userData) {
    Programs programs;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(vertexCode)}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    programs.vertex = ShaderRecompiler::Recompile(vertexRequest);
    const auto vertexPush = static_cast<std::uint32_t>(programs.vertex.pushConstants.size());

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
    programs.fragment = ShaderRecompiler::Recompile(fragment);
    programs.fragmentPush = PixelPushOffset(vertexPush, device.Target());
    return programs;
}

AgcDriver::Graphics::State TargetState(const std::byte* pixels) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
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
    return state;
}

void Draw(AgcDriver::VulkanDevice& device, const Case& value) {
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    const auto output = Descriptor(Output.data(), 0u, static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu);
    const auto ring = Descriptor(Ring.data(), 16u, Records, 0x1104bfacu);
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(ring.begin(), ring.end(), userData.begin() + 8);
    const bool instanced = value.instances != 0u;
    const auto programs = Compile(device, instanced ? std::span<const std::uint32_t>(InstanceCode) : std::span<const std::uint32_t>(VertexCode), userData);
    const auto guest = programs.GuestBuffers();
    Require(guest != programs.vertex.bindings.end() && guest->count == 2u && guest->bufferVertexExtent.size() == 2u && guest->bufferVertexByInstance.size() == 2u, std::string(value.what) + ": the vertex shader does not bind its two V#s with vertex extents");
    for (std::uint32_t element = 0; element < 2u; ++element) {
        const bool ringElement = guest->guestDescriptor[element * 4u + 2u] == Records;
        Require(guest->bufferVertexExtent[element] == (ringElement ? 16u : 0u), std::string(value.what) + ": " + (ringElement ? "the ring read by the vertex index has no vertex extent" : "the output written at a VGPR offset has a vertex extent"));
        Require(guest->bufferVertexByInstance[element] == (ringElement && instanced), std::string(value.what) + ": the ring's extent is not marked by the index that reads it");
    }
    const auto state = TargetState(Pixels.data());
    AgcDriver::Pm4::DrawParameters draw{value.indexed ? reinterpret_cast<std::uintptr_t>(Indices.data()) : 0u, value.count, value.indexed ? 2u : 0u, instanced ? value.instances : 1u, 0, value.indexed};
    draw.firstVertex = value.firstVertex;
    draw.firstInstance = value.firstInstance;
    const auto shaders = programs.Shaders();
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::byte* GuestBlock() {
    constexpr std::size_t bytes = GuestBlockBytes * 2u;
    std::byte* block = nullptr;
#ifdef _WIN32
    block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, GuestBlockBytes));
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* mapped = mmap(nullptr, bytes + GuestBlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped != MAP_FAILED) {
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + GuestBlockBytes - 1u) & ~(static_cast<std::uintptr_t>(GuestBlockBytes) - 1u);
        if (aligned != begin) munmap(mapped, aligned - begin);
        if (aligned + bytes != begin + bytes + GuestBlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + GuestBlockBytes - aligned);
        block = reinterpret_cast<std::byte*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    }
#endif
    Require(block != nullptr, "cannot allocate a guest block");
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

void DrawsUpToTheTarget(AgcDriver::VulkanDevice& device, std::uint32_t past, std::uint32_t instances = 0u, std::uint32_t firstInstance = 0u) {
    const bool instanced = instances != 0u;
    const auto what = (instanced ? "a ring whose instances " + std::to_string(firstInstance) + " to " + std::to_string(firstInstance + instances - 1u) + " reach " : "a ring whose first three vertices reach ") + std::to_string(past) + " bytes into the target";
    static auto* block = GuestBlock();
    auto* target = block + GuestBlockBytes;
    const auto readBytes = instanced ? (firstInstance + instances) * 16u : ThreeVerticesBytes;
    auto* base = target - (readBytes - past);
    for (std::size_t i = 0; i < readBytes - past; ++i) base[i] = static_cast<std::byte>(0x30u + i * 7u);
    Output.fill(0xdeadbeefu);
    const auto targetBytes = AgcDriver::Graphics::ColorTargetLayout(Width, Height, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4u).Bytes();
    Require(targetBytes <= GuestBlockBytes, "the target does not fit its guest block");
    const auto ring = Descriptor(base, 16u, static_cast<std::uint32_t>((target + targetBytes - base) / 16), 0x1104bfacu);
    std::vector<std::uint32_t> userData(12, 0u);
    std::copy(ring.begin(), ring.end(), userData.begin() + 8);
    const auto output = Descriptor(Output.data(), 0u, static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu);
    if (instanced) std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto programs = Compile(device, instanced ? std::span<const std::uint32_t>(InstanceCode) : std::span<const std::uint32_t>(ReadOnlyVertexCode), userData);
    const auto guest = programs.GuestBuffers();
    const auto elements = instanced ? 2u : 1u;
    Require(guest != programs.vertex.bindings.end() && guest->count == elements && guest->bufferVertexExtent.size() == elements && guest->bufferVertexByInstance.size() == elements, what + ": the vertex shader does not bind its V#s with vertex extents");
    for (std::uint32_t element = 0; element < elements; ++element) {
        const bool ringElement = guest->guestDescriptor[element * 4u + 1u] >> 16u == 16u;
        Require(guest->bufferVertexExtent[element] == (ringElement ? 16u : 0u), what + ": " + (ringElement ? "the ring has no vertex extent" : "the output written at a VGPR offset has a vertex extent"));
        Require(guest->bufferVertexByInstance[element] == (ringElement && instanced), what + ": the extent is not marked by the index that reads it");
    }
    auto state = TargetState(target);
    state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    state.color.bytes = targetBytes;
    state.colors = {state.color};
    AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, instanced ? instances : 1u, 0, false};
    draw.firstInstance = firstInstance;
    const auto shaders = programs.Shaders();
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    device.Draw(state, draw, shaders, {}, &recipe);
    const auto context = device.graphicsContext();
    if (context.hostImportAlignment != 0u && !instanced) {
        Require(recipe != nullptr, what + ": the draw built no recipe");
        const auto replayed = AgcDriver::Graphics::DrawWithRecipe(context, state, draw, shaders, {}, *recipe);
        Require(replayed.recorded, what + ": the draw did not replay its recipe: " + AgcDriver::Graphics::DrawRecipeMissName(replayed.miss));
    }
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    if (!instanced) return;
    for (std::uint32_t instance = 0; instance < OutputVertices; ++instance) {
        const bool drawn = instance >= firstInstance && instance < firstInstance + instances;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            std::uint32_t expected = 0xdeadbeefu;
            if (drawn) std::memcpy(&expected, base + static_cast<std::size_t>(instance) * 16u + component * 4u, sizeof(expected));
            const auto actual = Output[instance * 4u + component];
            Require(actual == expected, what + ": instance " + std::to_string(instance) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void Check(const Case& value) {
    for (std::uint32_t vertex = 0; vertex < OutputVertices; ++vertex) {
        const bool drawn = value.instances != 0u ? vertex >= value.firstInstance && vertex < value.firstInstance + value.instances : std::ranges::find(value.vertices, vertex) != value.vertices.end();
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto expected = drawn ? Ring[vertex * 4u + component] : 0xdeadbeefu;
            const auto actual = Output[vertex * 4u + component];
            Require(actual == expected, std::string(value.what) + ": vertex " + std::to_string(vertex) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < Ring.size(); ++index) Ring[index] = 0x01000000u + index * 0x10001u;
        constexpr std::array<Case, 5> cases{{
            {"auto draw from vertex 5", false, 3u, 5u, {5u, 6u, 7u}},
            {"indexed draw with base vertex 20", true, 3u, 20u, {20u, 22u, 21u}},
            {"auto draw from vertex 0", false, 3u, 0u, {0u, 1u, 2u}},
            {"instanced draw of 3 instances from instance 2", false, 3u, 0u, {}, 3u, 2u},
            {"instanced draw of 3 instances from instance 0", false, 3u, 0u, {}, 3u, 0u},
        }};
        for (const auto& value : cases) {
            Draw(*device, value);
            Check(value);
        }
        DrawsUpToTheTarget(*device, 0u);
        DrawsUpToTheTarget(*device, 8u);
        DrawsUpToTheTarget(*device, 0u, 3u, 0u);
        DrawsUpToTheTarget(*device, 8u, 3u, 2u);
        std::puts("vertex-indexed ring tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
