#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};
constexpr std::uint32_t BaseVertexLocation = 0x8c;

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> FirstMemory{};
alignas(256) std::array<std::byte, 1024> NoInstancesMemory{};
alignas(256) std::array<std::byte, 1024> NoVerticesMemory{};
alignas(256) std::array<std::byte, 1024> LastMemory{};
alignas(256) std::array<std::uint32_t, 16> Records{3, 1, 0, 0, 3, 0, 0, 0, 0, 1, 0, 0, 3, 1, 0, 0};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
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
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> DrawFour() {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto records = reinterpret_cast<std::uintptr_t>(Records.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
    const auto draw = [&](std::array<std::byte, 1024>& memory, std::uint32_t recordOffset) {
        const auto color = reinterpret_cast<std::uintptr_t>(memory.data());
        Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
        Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
        Append(commands, 0x76, {BaseVertexLocation, 0});
        Append(commands, 0x24, {recordOffset, BaseVertexLocation, 0, 2});
    };
    draw(FirstMemory, 0);
    draw(NoInstancesMemory, 16);
    draw(NoVerticesMemory, 32);
    draw(LastMemory, 48);
    return commands;
}

std::size_t Covered(const std::array<std::byte, 1024>& memory) {
    return static_cast<std::size_t>(std::count_if(memory.begin(), memory.end(), [](std::byte value) { return value != Kept; }));
}

void EmptyIndirectDrawsSkipTheirStages() {
    FirstMemory.fill(Kept);
    NoInstancesMemory.fill(Kept);
    NoVerticesMemory.fill(Kept);
    LastMemory.fill(Kept);
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    auto commands = DrawFour();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto first = Covered(FirstMemory);
    Require(first != 0, "the first indirect draw, whose record draws 3 vertices, covered nothing");
    Require(Covered(NoInstancesMemory) == 0, "the indirect draw whose record has no instances covered " + std::to_string(Covered(NoInstancesMemory)) + " bytes, expected none");
    Require(Covered(NoVerticesMemory) == 0, "the indirect draw whose record has no vertices covered " + std::to_string(Covered(NoVerticesMemory)) + " bytes, expected none");
    for (std::size_t index = 0; index < FirstMemory.size(); ++index) {
        Require(LastMemory[index] == FirstMemory[index], "byte " + std::to_string(index) + " of the indirect draw after the empty ones is " + std::to_string(static_cast<unsigned>(LastMemory[index])) + ", the first indirect draw wrote " + std::to_string(static_cast<unsigned>(FirstMemory[index])));
    }
    const auto empty = AgcDriver::Graphics::EmptyIndirectDrawCount();
    Require(empty == 2, std::to_string(empty) + " of the 2 indirect draws whose records draw nothing were skipped before their stages were built");
    const auto counts = AgcDriver::Graphics::IndirectDrawCounts();
    std::uint64_t cpu = 0;
    for (std::size_t path = 1; path < counts.size(); ++path) cpu += counts[path];
    Require(counts[0] == 0 && cpu == 2, "the drawing indirect draws took the GPU-side path " + std::to_string(counts[0]) + " times and the CPU path " + std::to_string(cpu) + " times, expected the CPU path twice");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        EmptyIndirectDrawsSkipTheirStages();
        AgcDriverShutdown_nid_postfix();
        std::puts("CPU-path indirect draws whose records draw nothing were skipped before their stages were built");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
