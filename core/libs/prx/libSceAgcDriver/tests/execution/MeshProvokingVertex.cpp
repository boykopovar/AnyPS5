#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "../MeshGeometryFixture.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
using Image = std::array<std::byte, Width * Height * 4>;
alignas(256) Image Pixels{};
alignas(256) constexpr std::array<std::uint16_t, 4> Indices{0, 1, 2, 3};
alignas(256) constexpr std::array<std::uint16_t, 4> FanIndices{1, 0, 2, 3};
alignas(256) constexpr std::array<std::array<float, 8>, 4> Vertices{{
    {-0.9f, -0.8f, 0, 1, 1, 0, 0, 1}, {-0.1f, -0.8f, 0, 1, 0, 1, 0, 1}, {-0.5f, 0.8f, 0, 1, 0, 0, 1, 1}, {0.3f, 0.8f, 0, 1, 1, 1, 0, 1}
}};
alignas(256) constexpr std::array<std::uint32_t, 7> FlatPixelCode{0xc8120002u, 0xc8160102u, 0xc81a0202u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> SmoothPixelCode{0xc8100000u, 0xc8110001u, 0xc8140100u, 0xc8150101u, 0xc8180200u, 0xc8190201u, 0xc81c0300u, 0xc81d0301u, 0xf800180fu, 0x07060504u, 0xbf810000u};

constexpr std::array<std::uint32_t, 3> LoadBufferDescriptor{0xf4080204u, 0xfa000000u, 0xbf8cc07fu};
alignas(256) constexpr auto GeometryCode = [] {
    std::array<std::uint32_t, LoadBufferDescriptor.size() + AgcDriver::Test::MeshGeometryCode.size()> code{};
    std::copy(LoadBufferDescriptor.begin(), LoadBufferDescriptor.end(), code.begin());
    std::copy(AgcDriver::Test::MeshGeometryCode.begin(), AgcDriver::Test::MeshGeometryCode.end(), code.begin() + LoadBufferDescriptor.size());
    return code;
}();

alignas(16) std::array<std::uint32_t, 4> Descriptor{};

struct Header {
    Shader shader{};
    ShaderUserData users{};
};

Header GeometryHeader{};
Header FlatPixelHeader{};
Header SmoothPixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t size, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = static_cast<std::uint32_t>(size);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.users;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Write(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    commands.insert(commands.end(), {0xc0010000u | (opcode << 8u), offset, value});
}

Image Draw(const std::uint32_t* pixelCode, bool flat, bool last, std::uint32_t primitive = 4u) {
    Pixels.fill(std::byte{0});
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto geometry = reinterpret_cast<std::uintptr_t>(GeometryCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
    const auto table = reinterpret_cast<std::uintptr_t>(Descriptor.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x20}, {0x1ff, 12}, {0x2ce, 3}, {0x29b, 2}, {0x2ab, 4}, {0x260, 0},
        {0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, flat ? 0x400u : 0u},
        {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240u | (last ? 1u << 19u : 0u)},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Height - 1u) << 14u) | (Width - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0}, {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(32.0f)}, {0x112, std::bit_cast<std::uint32_t>(32.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shaders{
        {0xc8, static_cast<std::uint32_t>(geometry >> 8u)}, {0xc9, static_cast<std::uint32_t>(geometry >> 40u)},
        {0x8a, 0x600c0000u}, {0x8b, (8u << 19u) | (3u << 16u) | (2u << 1u)},
        {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)},
        {0x8, static_cast<std::uint32_t>(pixel >> 8u)}, {0x9, static_cast<std::uint32_t>(pixel >> 40u)}, {0xb, 0}
    };
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, primitive);
    Write(commands, 0x79, 0x25b, (12u << 9u) | 4u);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0002a00u, 0u});
    const auto address = reinterpret_cast<std::uintptr_t>(primitive == 5u ? FanIndices.data() : Indices.data());
    const std::uint32_t count = primitive == 4u ? 3u : 4u;
    commands.insert(commands.end(), {0xc0042700u, count, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), count, 0u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "mesh submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, Pixels.size());
    return Pixels;
}

unsigned Channel(const Image& image, std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return std::to_integer<unsigned>(image[(y * Width + x) * 4u + channel]);
}

constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 5> Inside{{{12, 32}, {16, 32}, {20, 32}, {16, 28}, {16, 36}}};

void FlatInputs() {
    for (const auto last : {false, true, false}) {
        const auto image = Draw(FlatPixelCode.data(), true, last);
        const std::array<unsigned, 4> expected{last ? 0u : 255u, 0u, last ? 255u : 0u, 255u};
        const std::string mode = last ? "last" : "first";
        for (const auto& [x, y] : Inside) {
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                Require(Channel(image, x, y, channel) == expected[channel], "flat input, " + mode + " provoking vertex: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Channel(image, x, y, channel)) + ", expected " + std::to_string(expected[channel]) + " (the color of vertex " + (last ? "2" : "0") + ")");
            }
        }
        Require(Channel(image, 48, 48, 3) == 0, "flat input, " + mode + " provoking vertex: the draw wrote outside its triangle");
    }
}

constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> SecondInside{{{26, 48}, {32, 48}, {29, 44}, {29, 52}}};

void CheckFlat(const Image& image, std::span<const std::pair<std::uint32_t, std::uint32_t>> pixels, const std::array<unsigned, 4>& expected, const std::string& what) {
    for (const auto& [x, y] : pixels) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            Require(Channel(image, x, y, channel) == expected[channel], what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Channel(image, x, y, channel)) + ", expected " + std::to_string(expected[channel]));
        }
    }
}

void StripFlatInputs() {
    const auto last = Draw(FlatPixelCode.data(), true, true, 6u);
    CheckFlat(last, Inside, {0u, 0u, 255u, 255u}, "triangle strip, last provoking vertex, triangle 0 (the color of vertex 2)");
    CheckFlat(last, SecondInside, {255u, 255u, 0u, 255u}, "triangle strip, last provoking vertex, triangle 1 (the color of vertex 3)");
    Require(Channel(last, 48, 48, 3) == 0, "triangle strip, last provoking vertex: the draw wrote outside its triangles");
    const auto first = Draw(FlatPixelCode.data(), true, false, 6u);
    CheckFlat(first, Inside, {255u, 0u, 0u, 255u}, "triangle strip, first provoking vertex, triangle 0 (the color of vertex 0)");
    CheckFlat(first, SecondInside, {0u, 255u, 0u, 255u}, "triangle strip, first provoking vertex, triangle 1 (the color of vertex 1)");
}

void FanFlatInputs() {
    const auto last = Draw(FlatPixelCode.data(), true, true, 5u);
    CheckFlat(last, Inside, {0u, 0u, 255u, 255u}, "triangle fan, last provoking vertex, triangle 0 (the color of vertex 2)");
    CheckFlat(last, SecondInside, {255u, 255u, 0u, 255u}, "triangle fan, last provoking vertex, triangle 1 (the color of vertex 3)");
    Require(Channel(last, 48, 48, 3) == 0, "triangle fan, last provoking vertex: the draw wrote outside its triangles");
    const auto first = Draw(FlatPixelCode.data(), true, false, 5u);
    CheckFlat(first, Inside, {255u, 0u, 0u, 255u}, "triangle fan, first provoking vertex, triangle 0 (the color of vertex 0)");
    CheckFlat(first, SecondInside, {0u, 0u, 255u, 255u}, "triangle fan, first provoking vertex, triangle 1 (the color of vertex 2)");
    Require(Channel(first, 48, 48, 3) == 0, "triangle fan, first provoking vertex: the draw wrote outside its triangles");
}

void SmoothInputs() {
    const auto first = Draw(SmoothPixelCode.data(), false, false);
    for (const auto& [x, y] : Inside) {
        for (std::uint32_t channel = 0; channel < 3; ++channel) Require(Channel(first, x, y, channel) > 16u, "smooth input: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Channel(first, x, y, channel)) + ", not a blend of the three vertex colors");
    }
    const auto last = Draw(SmoothPixelCode.data(), false, true);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            for (std::uint32_t channel = 0; channel < 4; ++channel) Require(Channel(last, x, y, channel) == Channel(first, x, y, channel), "smooth input: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Channel(last, x, y, channel)) + " with the last provoking vertex and " + std::to_string(Channel(first, x, y, channel)) + " with the first");
        }
    }
}

}

int main() {
    try {
        bool smooth = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device || !TargetHasCapability(device->Target(), spv::CapabilityMeshShadingEXT)) return VulkanTestSkipped;
            if (device->DeviceName().starts_with("llvmpipe")) {
                std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
                return VulkanTestSkipped;
            }
            if (!device->ProvokingVertexLast()) {
                std::puts("skipped, the device has no last provoking vertex mode");
                return VulkanTestSkipped;
            }
            smooth = TargetHasCapability(device->Target(), spv::CapabilityFragmentBarycentricKHR);
        }
        const auto vertices = reinterpret_cast<std::uintptr_t>(Vertices.data());
        Descriptor = {static_cast<std::uint32_t>(vertices), static_cast<std::uint32_t>(vertices >> 32u) | (32u << 16u), static_cast<std::uint32_t>(Vertices.size()), 0x01016facu};
        Register(GeometryHeader, GeometryCode.data(), sizeof(GeometryCode), 2);
        Register(FlatPixelHeader, FlatPixelCode.data(), sizeof(FlatPixelCode), 1);
        Register(SmoothPixelHeader, SmoothPixelCode.data(), sizeof(SmoothPixelCode), 1);
        FlatInputs();
        StripFlatInputs();
        FanFlatInputs();
        if (smooth) SmoothInputs();
        AgcDriverShutdown_nid_postfix();
        std::cout << "geometry-program triangles take flat inputs from vertex 0 with the first provoking vertex and from vertex 2 with the last, the second triangle of a strip takes them from vertex 1 with the first and vertex 3 with the last, and a fan around vertex 1 takes them from vertices 0 and 2 with the first and vertices 2 and 3 with the last" << (smooth ? ", and interpolate smooth inputs the same in both modes\n" : "; smooth inputs skipped without fragment shader barycentrics\n");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
