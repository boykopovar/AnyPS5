#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr float DrawnDepth = 0.25f;
constexpr float MemoryDepth = 0.75f;
constexpr float Untouched = -1.0f;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 4> WhitePixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
alignas(256) std::array<float, Width * Height> CopyTarget{};
alignas(256) std::array<float, Width * Height> ReferenceTarget{};
constexpr std::size_t DepthFloats = 16384;
constexpr std::size_t DepthAlignment = 65536;
std::array<float, DepthFloats + DepthAlignment / sizeof(float)> DepthStorage{};
const std::span<float, DepthFloats> DepthMemory{reinterpret_cast<float*>((reinterpret_cast<std::uintptr_t>(DepthStorage.data()) + DepthAlignment - 1) & ~(DepthAlignment - 1)), DepthFloats};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
    std::array<ShaderRegister, 4> registers{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    if (type == 1) {
        const auto address = reinterpret_cast<std::uintptr_t>(code);
        header.registers = {{{0x008, static_cast<std::uint32_t>(address >> 8u)}, {0x009, static_cast<std::uint32_t>(address >> 40u)}, {0x00b, 0}, {0x00a, 0}}};
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
    }
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

struct DrawOptions {
    float* color = nullptr;
    std::uint32_t targetMask = 1;
    std::uint32_t renderControl = 0;
    std::uint32_t depthControl = 0;
};

void AppendDraw(std::vector<std::uint32_t>& commands, const DrawOptions& options) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(WhitePixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(options.color);
    const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory.data());
    Require(depth % DepthAlignment == 0, "the test depth plane is not 64 KiB aligned");
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 0x2}, {0x1b4, 0x2}, {0x1b6, 0}, {0x207, 0}, {0x200, options.depthControl}, {0x203, 0},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, options.targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 1}, {0x1c3, 4}, {0x31c, 0x8710},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f))}, {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, std::bit_cast<std::uint32_t>(DrawnDepth)},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)}, {0x292, 2},
        {0x000, options.renderControl}, {0x002, 0}, {0x007, ((Height - 1u) << 16u) | (Width - 1u)},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, 3}, {0x011, 0},
        {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
        {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00a, 0}, {0x00b, 0},
    };
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
}

void CopyCoversTheDraw() {
    CopyTarget.fill(Untouched);
    ReferenceTarget.fill(Untouched);
    std::ranges::fill(DepthMemory, MemoryDepth);
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, WhitePixelCode.data(), sizeof(WhitePixelCode), 1);
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, {.color = CopyTarget.data(), .targetMask = 0, .depthControl = 0x76});
    AppendDraw(commands, {.color = CopyTarget.data(), .renderControl = 0x84});
    AppendDraw(commands, {.color = ReferenceTarget.data()});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the depth write, DB depth copy and reference draws were not submitted");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(CopyTarget.data()), sizeof(CopyTarget));
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ReferenceTarget.data()), sizeof(ReferenceTarget));
    std::size_t covered = 0;
    for (std::size_t index = 0; index < CopyTarget.size(); ++index) {
        const bool drawn = ReferenceTarget[index] == 1.0f;
        Require(drawn || ReferenceTarget[index] == Untouched, "the reference draw wrote " + std::to_string(ReferenceTarget[index]) + " at texel " + std::to_string(index));
        covered += drawn;
        const auto expected = drawn ? DrawnDepth : Untouched;
        Require(CopyTarget[index] == expected, "the DB depth copy holds " + std::to_string(CopyTarget[index]) + " at texel " + std::to_string(index) + ", expected " + std::to_string(expected) + (drawn ? " (the triangle covers it)" : " (outside the triangle)"));
    }
    Require(covered != 0 && covered != CopyTarget.size(), "the triangle covered " + std::to_string(covered) + " of " + std::to_string(CopyTarget.size()) + " texels, expected an edge through the target");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        CopyCoversTheDraw();
        AgcDriverShutdown_nid_postfix();
        std::puts("the DB depth copy wrote the drawn depth exactly where its draw rasterized");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
