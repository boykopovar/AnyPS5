#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
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
alignas(256) constexpr std::array<std::array<float, 8>, 6> ListVertices{{
    {-0.9f, -0.8f, 0, 1, 1, 0, 0, 1}, {-0.1f, -0.8f, 0, 1, 0, 1, 0, 1}, {-0.5f, 0.8f, 0, 1, 0, 0, 1, 1},
    {-0.1f, -0.8f, 0, 1, 0, 1, 0, 1}, {-0.5f, 0.8f, 0, 1, 0, 0, 1, 1}, {0.3f, 0.8f, 0, 1, 1, 1, 0, 1}
}};
alignas(256) constexpr std::array<std::array<float, 8>, 4> StripVertices{{
    {-0.9f, -0.8f, 0, 1, 1, 0, 0, 1}, {-0.1f, -0.8f, 0, 1, 0, 1, 0, 1}, {-0.5f, 0.8f, 0, 1, 0, 0, 1, 1}, {0.3f, 0.8f, 0, 1, 1, 1, 0, 1}
}};
alignas(256) constexpr std::array<std::uint32_t, 13> VertexCode{
    0xf4080204u, 0xfa000000u, 0xbf8cc07fu,
    0xe0382000u, 0x80021005u, 0xe0382010u, 0x80021405u, 0xbf8c3f70u,
    0xf80008cfu, 0x13121110u, 0xf800020fu, 0x17161514u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 7> FlatPixelCode{0xc8120002u, 0xc8160102u, 0xc81a0202u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
alignas(16) std::array<std::uint32_t, 4> Descriptor{};

struct Header {
    Shader shader{};
    ShaderUserData users{};
};

Header VertexHeader{};
Header PixelHeader{};

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

Image Draw(std::span<const std::array<float, 8>> vertices, std::uint32_t primitive, bool last) {
    Pixels.fill(std::byte{0});
    const auto data = reinterpret_cast<std::uintptr_t>(vertices.data());
    Descriptor = {static_cast<std::uint32_t>(data), static_cast<std::uint32_t>(data >> 32u) | (32u << 16u), static_cast<std::uint32_t>(vertices.size()), 0x01016facu};
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(FlatPixelCode.data());
    const auto table = reinterpret_cast<std::uintptr_t>(Descriptor.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0x400u},
        {0x207, 0}, {0x200, 0}, {0x203, 0},
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
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)},
        {0x8b, 2u << 1u}, {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)},
        {0x8, static_cast<std::uint32_t>(pixel >> 8u)}, {0x9, static_cast<std::uint32_t>(pixel >> 40u)}, {0xb, 0}
    };
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, primitive);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0012d00u, static_cast<std::uint32_t>(vertices.size()), 2u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "vertex draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, Pixels.size());
    return Pixels;
}

unsigned Channel(const Image& image, std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return std::to_integer<unsigned>(image[(y * Width + x) * 4u + channel]);
}

constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 5> Inside{{{12, 32}, {16, 32}, {20, 32}, {16, 28}, {16, 36}}};
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> SecondInside{{{26, 48}, {32, 48}, {29, 44}, {29, 52}}};

void CheckFlat(const Image& image, std::span<const std::pair<std::uint32_t, std::uint32_t>> pixels, const std::array<unsigned, 4>& expected, const std::string& what) {
    for (const auto& [x, y] : pixels) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            Require(Channel(image, x, y, channel) == expected[channel], what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Channel(image, x, y, channel)) + ", expected " + std::to_string(expected[channel]));
        }
    }
}

void FlatInputs(std::span<const std::array<float, 8>> vertices, std::uint32_t primitive, const std::string& name) {
    for (const auto last : {false, true, false}) {
        const auto image = Draw(vertices, primitive, last);
        const std::string mode = name + (last ? ", last provoking vertex" : ", first provoking vertex");
        CheckFlat(image, Inside, last ? std::array<unsigned, 4>{0u, 0u, 255u, 255u} : std::array<unsigned, 4>{255u, 0u, 0u, 255u}, mode + ", triangle 0 (the color of vertex " + (last ? "2" : "0") + ")");
        CheckFlat(image, SecondInside, last ? std::array<unsigned, 4>{255u, 255u, 0u, 255u} : std::array<unsigned, 4>{0u, 255u, 0u, 255u}, mode + ", triangle 1 (the color of its " + (last ? "last" : "first") + " vertex)");
        Require(Channel(image, 48, 48, 3) == 0, mode + ": the draw wrote outside its triangles");
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (!device->ProvokingVertexLast() && !device->Target().tessellation.has_value()) {
                std::puts("skipped, the device has neither a last provoking vertex mode nor tessellation shaders");
                return VulkanTestSkipped;
            }
        }
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, FlatPixelCode.data(), sizeof(FlatPixelCode), 1);
        FlatInputs(ListVertices, 4u, "triangle list");
        FlatInputs(StripVertices, 6u, "triangle strip");
        AgcDriverShutdown_nid_postfix();
        std::puts("vertex-program triangles take flat inputs from vertex 0 with the first provoking vertex and from vertex 2 with the last, and the second triangle from its first and last vertex, in a triangle list and a triangle strip");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
