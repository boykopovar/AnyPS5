#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 48;
constexpr std::uint32_t Slices = 2;
constexpr std::size_t SliceBytes = 65536;
constexpr std::size_t SurfaceBytes = SliceBytes * Slices;
constexpr std::uint32_t Color32x4FloatInfo = (14u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t LastProvokingVertex = 1u << 19u;
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;
constexpr std::uint32_t DrawIndexAuto = 0x2du;
constexpr std::uint32_t TriangleList = 4u;
constexpr std::uint32_t RectList = 17u;
constexpr float Unwritten = -7.0f;

alignas(64) volatile std::uint32_t EndOfPipe = 0;

alignas(256) constexpr std::array<std::uint32_t, 12> VertexCode{
    0xe0382000, 0x80020005, 0xe030201c, 0x80020405, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d4, 0x04040404, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> ConstantPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexValuePixelCode{
    0xc80a0000, 0xc80e0001, 0xc8120002, 0xf800180f, 0x04040302, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> pad;
    std::uint32_t layer;
};

constexpr std::array<std::array<float, 2>, 3> Corners{{{-0.8f, -0.8f}, {0.7f, -0.8f}, {-0.8f, 0.6f}}};

constexpr std::array<Vertex, 3> VerticesFor(std::array<std::uint32_t, 3> layers) {
    std::array<Vertex, 3> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) vertices[i] = {{Corners[i][0], Corners[i][1], 0.5f, 1.0f}, {}, layers[i]};
    return vertices;
}

alignas(256) constexpr std::array<Vertex, 3> ThirdVertexLayer = VerticesFor({0u, 0u, 1u});
alignas(256) constexpr std::array<Vertex, 3> FirstVertexLayer = VerticesFor({1u, 0u, 0u});

struct RegisteredShader {
    Shader shader;
    ShaderUserData userData;
};
RegisteredShader VertexShader{};
RegisteredShader ConstantPixelShader{};
RegisteredShader VertexValuePixelShader{};

const std::vector<std::pair<std::uint32_t, std::uint32_t>> ContextBaseline{
    {0x2d5, 0x2000},
    {0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0},
    {0x200, 0}, {0x203, 0x800},
    {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
    {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
    {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
    {0x206, 0x43f}, {0x204, 0x80000},
    {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
    {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4},
    {0x1e0, 0},
    {0xc, 0},
    {0x81, 0x80000000}, {0x90, 0x80000000}, {0x94, 0x80000000},
    {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0x113, std::bit_cast<std::uint32_t>(1.0f)},
};

std::uint8_t* AllocateWatched() {
#ifdef _WIN32
    auto* block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SliceBytes));
    Require(block != nullptr, "cannot allocate the two-slice color array");
    GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
    void* raw = mmap(nullptr, SurfaceBytes + SliceBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the two-slice color array");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + SliceBytes - 1) & ~static_cast<std::uintptr_t>(SliceBytes - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + SurfaceBytes != begin + SurfaceBytes + SliceBytes) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + SliceBytes - aligned);
    auto* block = reinterpret_cast<std::uint8_t*>(aligned);
    if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
#endif
    GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    return block;
}

void Register(RegisteredShader& registered, const void* code, std::uint32_t bytes, std::uint8_t type) {
    registered.shader.file_header = 0x34333231u;
    registered.shader.version = 0x18u;
    registered.shader.user_data = &registered.userData;
    registered.shader.code = code;
    registered.shader.header_size = sizeof(RegisteredShader);
    registered.shader.shader_size = bytes;
    registered.shader.type = type;
    AgcDriverRegisterShader_nid_postfix(&registered.shader);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

void CheckColorTarget(std::uint64_t address) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = (Slices - 1u) << 13u;
    cx[0x31c] = Color32x4FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = (TiledAttrib3 & ~((0x1fu << 14u) | 0x1fffu)) | (static_cast<std::uint32_t>(ColorTileMode::RenderTarget) << 14u) | (Slices - 1u);
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    cx[0x207] = LayerExport;
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.layers == Slices && color.arraySlice == 0u && color.arrayLastSlice == Slices - 1u && color.format == VK_FORMAT_R32G32B32A32_SFLOAT, "the two-slice RGBA32F color view did not decode");
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::ArraySurfaceForTarget(color));
    Require(geometry.guestBytes <= SurfaceBytes && geometry.GuestLayerOffset(1u) == SliceBytes, "the two-slice RGBA32F color array is not two 64 KiB slices");
}

struct Case {
    const char* route;
    std::uint32_t primitive;
    const RegisteredShader* pixel;
    bool last;
    const std::array<Vertex, 3>* vertices;
};

std::vector<std::uint32_t> Frame(const std::uint8_t* surface, const Case& item) {
    const auto colorAddress = reinterpret_cast<std::uintptr_t>(surface);
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(item.pixel->shader.code);
    std::vector<std::uint32_t> words;
    for (const auto& [offset, value] : ContextBaseline) SetRegister(words, SetContextReg, offset, value);
    SetRegister(words, SetContextReg, 0x205, 0x240u | (item.last ? LastProvokingVertex : 0u));
    SetRegister(words, SetContextReg, 0x207, LayerExport);
    SetRegister(words, SetContextReg, 0x318, static_cast<std::uint32_t>(colorAddress >> 8u));
    SetRegister(words, SetContextReg, 0x390, static_cast<std::uint32_t>(colorAddress >> 40u));
    SetRegister(words, SetContextReg, 0x31b, (Slices - 1u) << 13u);
    SetRegister(words, SetContextReg, 0x31c, Color32x4FloatInfo);
    SetRegister(words, SetContextReg, 0x31d, 0u);
    SetRegister(words, SetContextReg, 0x3b0, ((Width - 1u) << 14u) | (Height - 1u));
    SetRegister(words, SetContextReg, 0x3b8, (TiledAttrib3 & ~((0x1fu << 14u) | 0x1fffu)) | (static_cast<std::uint32_t>(ColorTileMode::RenderTarget) << 14u) | (Slices - 1u));
    SetRegister(words, SetContextReg, 0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x110, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f)));
    SetRegister(words, SetContextReg, 0x112, std::bit_cast<std::uint32_t>(Height / 2.0f));
    const auto extent = (Height << 16u) | Width;
    SetRegister(words, SetContextReg, 0xd, extent);
    SetRegister(words, SetContextReg, 0x82, extent);
    SetRegister(words, SetContextReg, 0x91, extent);
    SetRegister(words, SetContextReg, 0x95, extent);
    SetRegister(words, SetUconfigReg, 0x242, item.primitive);
    SetRegister(words, SetShReg, 0x008, static_cast<std::uint32_t>(pixelAddress >> 8u));
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(pixelAddress >> 40u));
    SetRegister(words, SetShReg, 0x00b, 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertexAddress >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(vertexAddress >> 40u));
    SetRegister(words, SetShReg, 0x08b, 4u << 1u);
    const auto descriptor = BufferDescriptor(item.vertices->data(), sizeof(Vertex), static_cast<std::uint32_t>(item.vertices->size()));
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    words.insert(words.end(), {0xc0000000u | (1u << 16u) | (DrawIndexAuto << 8u), static_cast<std::uint32_t>(item.vertices->size()), 2u});
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&EndOfPipe);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 1u, 0u, 0u});
    return words;
}

std::array<std::size_t, Slices> WrittenTexels(const std::vector<std::byte>& stored) {
    std::array<std::size_t, Slices> written{};
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        for (std::size_t texel = 0; texel < SliceBytes / 16u; ++texel) {
            std::array<float, 4> value{};
            std::memcpy(value.data(), stored.data() + slice * SliceBytes + texel * 16u, sizeof(value));
            if (std::any_of(value.begin(), value.end(), [](float channel) { return channel != Unwritten; })) ++written[slice];
        }
    }
    return written;
}

std::string Run(const Case& item) {
    const auto& layers = *item.vertices;
    const auto expected = item.last ? layers[2].layer : layers[0].layer;
    const std::string what = std::string(item.route) + ", " + (item.last ? "last" : "first") + " provoking vertex, vertex layers " + std::to_string(layers[0].layer) + ", " + std::to_string(layers[1].layer) + " and " + std::to_string(layers[2].layer);
    auto* surface = AllocateWatched();
    CheckColorTarget(reinterpret_cast<std::uint64_t>(surface));
    std::fill_n(reinterpret_cast<float*>(surface), SurfaceBytes / sizeof(float), Unwritten);
    EndOfPipe = 0;
    auto words = Frame(surface, item);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    try {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the DCB submission failed");
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        return what + ": " + error.what();
    }
    if (EndOfPipe != 1u) return what + ": the end-of-pipe label was not written";
    std::vector<std::byte> stored(SurfaceBytes);
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uint64_t>(surface), stored);
    const auto written = WrittenTexels(stored);
    const auto other = 1u - expected;
    if (written[expected] > 200u && written[other] == 0u) return {};
    return what + ": slice " + std::to_string(expected) + " (the provoking vertex's layer) has " + std::to_string(written[expected]) + " written texels and slice " + std::to_string(other) + " has " + std::to_string(written[other]) + ", expected the draw in slice " + std::to_string(expected) + " only";
}

}

int main() {
    try {
        bool rectLists = false;
        bool emulatedBarycentrics = false;
        bool geometryStage = false;
        bool provokingVertexLast = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            const auto target = device->Target();
            if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
                std::puts("skipped, the device has no VK_EXT_shader_viewport_index_layer");
                return VulkanTestSkipped;
            }
            rectLists = target.tessellation.has_value();
            emulatedBarycentrics = !target.fragmentShaderBarycentricEnabled;
            geometryStage = device->GeometryLimits().has_value() || !target.tessellation.has_value();
            provokingVertexLast = device->ProvokingVertexLast();
        }
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(ConstantPixelShader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
        Register(VertexValuePixelShader, VertexValuePixelCode.data(), sizeof(VertexValuePixelCode), 1);
        const char* barycentricRoute = geometryStage ? "a triangle list whose pixel shader reads emulated barycentrics (geometry stage)" : "a triangle list whose pixel shader reads emulated barycentrics (tessellation stages)";
        std::vector<Case> cases;
        for (const auto* vertices : {&ThirdVertexLayer, &FirstVertexLayer}) {
            for (const bool last : {false, true}) {
                if (last && !provokingVertexLast) continue;
                if (rectLists) cases.push_back({"a rect list", RectList, &ConstantPixelShader, last, vertices});
                if (emulatedBarycentrics) cases.push_back({barycentricRoute, TriangleList, &VertexValuePixelShader, last, vertices});
            }
        }
        if (!rectLists) std::puts("rect lists are not checked, the device lacks tessellationShader");
        if (!emulatedBarycentrics) std::puts("emulated barycentrics are not checked, the device has VK_KHR_fragment_shader_barycentric");
        if (!provokingVertexLast) std::puts("the last provoking vertex mode is not checked, the device lacks it");
        if (cases.empty()) {
            std::puts("skipped, the device takes neither generated-stage route");
            AgcDriverShutdown_nid_postfix();
            return VulkanTestSkipped;
        }
        std::uint32_t failures = 0;
        for (const auto& item : cases) {
            const auto error = Run(item);
            if (error.empty()) {
                std::printf("passed: %s, %s provoking vertex, vertex layers %u, %u and %u\n", item.route, item.last ? "last" : "first", (*item.vertices)[0].layer, (*item.vertices)[1].layer, (*item.vertices)[2].layer);
                continue;
            }
            std::cerr << error << '\n';
            ++failures;
        }
        AgcDriverShutdown_nid_postfix();
        if (failures != 0u) {
            std::cerr << failures << " provoking vertex layer draws through the driver failed\n";
            return 1;
        }
        std::puts("draws through the driver took the layer of the provoking vertex the guest selected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
