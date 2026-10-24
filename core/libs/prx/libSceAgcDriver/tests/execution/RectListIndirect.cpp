#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
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
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 36> SlotVertexCode{
    0x7e000d05u, 0x100c00ffu, 0x3eaaaaabu, 0x7e0c4906u, 0x100e0cffu, 0x40400000u, 0x08000f00u,
    0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e120d08u, 0x101212f4u, 0x060c0d09u, 0x100c0cf0u, 0x100202ffu, 0x3e000000u, 0x06020306u, 0x060202ffu, 0xbf600000u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> DirectMemory{};
alignas(256) std::array<std::byte, 1024> IndirectMemory{};
alignas(256) std::array<std::byte, 1024> EmptyMemory{};
alignas(256) std::array<std::byte, 1024> DirectInstancedMemory{};
alignas(256) std::array<std::byte, 1024> IndirectInstancedMemory{};
alignas(256) std::array<std::byte, 1024> DirectIndexedMemory{};
alignas(256) std::array<std::byte, 1024> IndirectIndexedMemory{};
constexpr std::array<std::uint32_t, 20> RecordWords{3, 1, 0, 0, 0, 1, 0, 0, 6, 2, 0, 0, 6, 1, 0, 0, 0, 0, 0, 0};
constexpr std::size_t RecordBytes = 65536;
std::uint32_t* Records = nullptr;
alignas(256) std::array<std::uint16_t, 8> Indices{6, 7, 8, 3, 4, 5, 0, 0};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header SlotVertexHeader{};
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

std::vector<std::uint32_t> DrawRectangles() {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto slotVertex = reinterpret_cast<std::uintptr_t>(SlotVertexCode.data());
    const auto indices = reinterpret_cast<std::uintptr_t>(Indices.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto records = reinterpret_cast<std::uintptr_t>(Records);
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
    Append(commands, 0x79, {0x242, 17});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
    const auto target = [&](std::array<std::byte, 1024>& memory) {
        const auto color = reinterpret_cast<std::uintptr_t>(memory.data());
        Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
        Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    };
    target(DirectMemory);
    Append(commands, 0x2d, {3, 2});
    target(IndirectMemory);
    Append(commands, 0x24, {0, 0, 0, 2});
    target(EmptyMemory);
    Append(commands, 0x24, {16, 0, 0, 2});
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(slotVertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(slotVertex >> 40u)});
    target(DirectInstancedMemory);
    Append(commands, 0x2f, {2});
    Append(commands, 0x2d, {6, 2});
    Append(commands, 0x2f, {1});
    target(IndirectInstancedMemory);
    Append(commands, 0x24, {32, 0, 0, 2});
    Append(commands, 0x2a, {0});
    target(DirectIndexedMemory);
    Append(commands, 0x27, {6, static_cast<std::uint32_t>(indices), static_cast<std::uint32_t>(indices >> 32u), 6, 0});
    Append(commands, 0x26, {static_cast<std::uint32_t>(indices), static_cast<std::uint32_t>(indices >> 32u)});
    Append(commands, 0x13, {6});
    target(IndirectIndexedMemory);
    Append(commands, 0x25, {48, 0, 0, 0});
    return commands;
}

std::size_t Covered(const std::array<std::byte, 1024>& memory) {
    return static_cast<std::size_t>(std::count_if(memory.begin(), memory.end(), [](std::byte value) { return value != Kept; }));
}

std::string Slots(const std::array<std::byte, 1024>& memory) {
    std::string slots;
    for (std::size_t slot = 0; slot < 4; ++slot) {
        std::size_t covered = 0;
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t x = slot * 16; x < slot * 16 + 16; ++x) {
                const auto pixel = (row * 64 + x) * 4;
                covered += std::any_of(memory.begin() + static_cast<std::ptrdiff_t>(pixel), memory.begin() + static_cast<std::ptrdiff_t>(pixel + 4), [](std::byte value) { return value != Kept; }) ? 1u : 0u;
            }
        }
        if (!slots.empty()) slots += ' ';
        slots += std::to_string(covered);
    }
    return slots;
}

void Same(const std::array<std::byte, 1024>& indirect, const std::array<std::byte, 1024>& direct, const std::string& what) {
    for (std::size_t index = 0; index < direct.size(); ++index) {
        Require(indirect[index] == direct[index], "byte " + std::to_string(index) + " of the indirect " + what + " is " + std::to_string(static_cast<unsigned>(indirect[index])) + ", the direct draw wrote " + std::to_string(static_cast<unsigned>(direct[index])));
    }
}

void DrawnOnTheGpu() {
    const auto counts = AgcDriver::Graphics::IndirectDrawCounts();
    std::string cpu;
    for (std::size_t path = 1; path < counts.size(); ++path) {
        if (counts[path] == 0) continue;
        if (!cpu.empty()) cpu += ", ";
        cpu += std::to_string(counts[path]) + " " + AgcDriver::Graphics::IndirectDrawPathName(static_cast<AgcDriver::Graphics::IndirectDrawPath>(path));
    }
    Require(cpu.empty(), "indirect rect-list draws read their records on the CPU: " + cpu);
    Require(counts[0] == 4, "the GPU drew " + std::to_string(counts[0]) + " indirect rect-list draws from their records, expected 4");
}

void IndirectRectListsDrawAsDirectOnes() {
    DirectMemory.fill(Kept);
    IndirectMemory.fill(Kept);
    EmptyMemory.fill(Kept);
    DirectInstancedMemory.fill(Kept);
    IndirectInstancedMemory.fill(Kept);
    DirectIndexedMemory.fill(Kept);
    IndirectIndexedMemory.fill(Kept);
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(SlotVertexHeader, SlotVertexCode.data(), sizeof(SlotVertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    Records = static_cast<std::uint32_t*>(::operator new(RecordBytes, std::align_val_t{RecordBytes}));
    std::copy(RecordWords.begin(), RecordWords.end(), Records);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(Records, RecordBytes, true, true, true);
    }
    auto commands = DrawRectangles();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto direct = Covered(DirectMemory);
    Require(direct != 0, "the direct rect-list draw covered nothing");
    Same(IndirectMemory, DirectMemory, "rect-list draw");
    const auto empty = Covered(EmptyMemory);
    Require(empty == 0, "the indirect rect-list draw whose record has no vertices covered " + std::to_string(empty) + " bytes, expected none");
    Require(Slots(DirectInstancedMemory) == "32 32 32 32", "the direct rect-list draw of 6 vertices and 2 instances covered pixels " + Slots(DirectInstancedMemory) + " of its four rectangles, expected 32 each");
    Same(IndirectInstancedMemory, DirectInstancedMemory, "rect-list draw of 6 vertices and 2 instances");
    Require(Slots(DirectIndexedMemory) == "0 32 32 0", "the direct indexed rect-list draw covered pixels " + Slots(DirectIndexedMemory) + " of the four rectangles, expected only the second and third");
    Same(IndirectIndexedMemory, DirectIndexedMemory, "indexed rect-list draw");
    DrawnOnTheGpu();
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
        IndirectRectListsDrawAsDirectOnes();
        AgcDriverShutdown_nid_postfix();
        std::puts("indirect rect-list draws were drawn on the GPU and drew what the direct draw did");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
