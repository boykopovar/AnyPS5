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
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t Waves = Threads / WaveSize;
constexpr std::uint32_t GroupVertices = 192;
constexpr std::uint32_t GroupPrimitives = 64;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;

alignas(256) std::array<std::uint32_t, Threads> Mismatches{};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(16) std::array<std::uint32_t, 4> Descriptor{};

constexpr std::uint32_t WindowBase = 10;
constexpr std::uint32_t WindowSize = 128;

constexpr std::uint32_t Branch(std::uint32_t opcode, std::size_t from, std::size_t to) {
    return opcode | (static_cast<std::uint32_t>(static_cast<std::int32_t>(to) - static_cast<std::int32_t>(from) - 1) & 0xffffu);
}

alignas(256) constexpr auto GeometryCode = [] {
    std::array<std::uint32_t, 184> code{};
    std::size_t at = 0;
    const auto put = [&](std::initializer_list<std::uint32_t> words) {
        for (const auto word : words) code[at++] = word;
    };
    put({0xd7650001u, 0x000100c1u, 0xd7660001u, 0x000202c1u, 0x9382ff03u, 0x00040018u, 0xd7460002u, 0x04050c02u});
    put({0xbefe04c1u, 0xf4080204u, 0xfa000000u, 0x34100482u, 0x34060488u, 0x8f068502u});
    put({0x4a000000u | (WindowBase << 17u) | (3u << 9u) | 0x81u});
    for (std::uint32_t reg = WindowBase + 1u; reg < WindowBase + WindowSize; ++reg) put({0x4a000000u | (reg << 17u) | ((reg - 1u) << 9u) | 0x81u});
    put({0xbf8a0000u, 0x7e0c0280u, 0xbe850380u});
    const auto loop = at;
    put({0x807c0605u, 0x877cff7cu, WindowSize - 1u, 0x7d840c80u, 0xbe8c246au});
    const auto skip = at;
    put({0u, 0x7e08870au, 0x7e148504u, 0x4a0a067cu, 0x4a0a0a81u, 0x7d8a0b04u, 0x500c0c80u});
    code[skip] = Branch(0xbf880000u, skip, at);
    put({0x88fe0c7eu, 0x80058105u, 0xbf0aff05u, WindowSize});
    put({Branch(0xbf850000u, at, loop)});
    put({0x4a0a0681u, 0x7d8a0b0au, 0x500c0c80u, 0x4a0a06c0u, 0x7d8a0b49u, 0x500c0c80u, 0x4a0a06ffu, WindowSize, 0x7d8a0b89u, 0x500c0c80u});
    put({0xbf8cc07fu, 0xe0701000u, 0x80020608u, 0xbf8c0000u});
    put({0xbefc0380u, 0xbf900009u, 0xbefe0480u, 0xf8000941u, 0x00000000u, 0xf80008cfu, 0x00000000u, 0xbf810000u});
    if (at != code.size()) throw "geometry program size";
    return code;
}();

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData users{};
};

Header GeometryHeader{};
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

void Draw() {
    Mismatches.fill(0xdeadbeefu);
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto geometry = reinterpret_cast<std::uintptr_t>(GeometryCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto table = reinterpret_cast<std::uintptr_t>(Descriptor.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x20}, {0x1ff, Threads}, {0x2ce, 3}, {0x29b, 2}, {0x2ab, 4}, {0x260, 0},
        {0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0},
        {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
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
        {0x8a, 0x600c0000u}, {0x8b, (32u << 19u) | (3u << 16u) | (2u << 1u)},
        {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)},
        {0x8, static_cast<std::uint32_t>(pixel >> 8u)}, {0x9, static_cast<std::uint32_t>(pixel >> 40u)}, {0xb, 0}
    };
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, 4u);
    Write(commands, 0x79, 0x25b, (GroupVertices << 9u) | GroupPrimitives);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0012d00u, GroupVertices, 2u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "mesh vector window submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Mismatches.data()), sizeof(Mismatches));
}

void Check() {
    std::array<std::uint32_t, Waves> wrong{};
    std::string first;
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        if (Mismatches[thread] == 0u) continue;
        ++wrong[thread / WaveSize];
        if (first.empty()) first = "thread " + std::to_string(thread) + " read " + std::to_string(Mismatches[thread]) + " window registers wrong";
    }
    if (first.empty()) return;
    std::string summary = first + "; wrong threads per wave:";
    for (const auto count : wrong) summary += " " + std::to_string(count);
    Require(false, "mesh vector window: " + summary);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (!TargetHasCapability(device->Target(), spv::CapabilityMeshShadingEXT)) {
                std::puts("skipped, the device has no VK_EXT_mesh_shader");
                return VulkanTestSkipped;
            }
            if (device->DeviceName().starts_with("llvmpipe")) {
                std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
                return VulkanTestSkipped;
            }
        }
        const auto address = reinterpret_cast<std::uintptr_t>(Mismatches.data());
        Descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Mismatches)), 0x31016facu};
        Register(GeometryHeader, GeometryCode.data(), sizeof(GeometryCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        for (std::uint32_t pass = 0; pass < 2; ++pass) {
            Draw();
            Check();
        }
        AgcDriverShutdown_nid_postfix();
        std::cout << "mesh vector window tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
