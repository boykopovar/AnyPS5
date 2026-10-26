#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
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
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DrawMiss;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) constexpr std::array<std::uint16_t, 4> Indices{0, 1, 2, 0};
alignas(256) constexpr std::array<std::array<float, 8>, 3> Vertices{{
    {-0.9f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.1f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.5f, 0.8f, 0, 1, 1, 1, 1, 1}
}};
alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};

constexpr std::array<std::uint32_t, 3> LoadBufferDescriptor{0xf4080204u, 0xfa000000u, 0xbf8cc07fu};
alignas(256) constexpr auto GeometryCode = [] {
    std::array<std::uint32_t, LoadBufferDescriptor.size() + AgcDriver::Test::MeshGeometryCode.size()> code{};
    std::copy(LoadBufferDescriptor.begin(), LoadBufferDescriptor.end(), code.begin());
    std::copy(AgcDriver::Test::MeshGeometryCode.begin(), AgcDriver::Test::MeshGeometryCode.end(), code.begin() + LoadBufferDescriptor.size());
    return code;
}();

alignas(16) std::array<std::array<std::uint32_t, 4>, 8> Descriptors{};

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

std::vector<std::uint32_t> Setup(const std::array<std::uint32_t, 4>& descriptor) {
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto geometry = reinterpret_cast<std::uintptr_t>(GeometryCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto table = reinterpret_cast<std::uintptr_t>(descriptor.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x20}, {0x1ff, 12}, {0x2ce, 3}, {0x29b, 2}, {0x2ab, 4}, {0x260, 0},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
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
        {0x8a, 0x600c0000u}, {0x8b, (8u << 19u) | (3u << 16u) | (2u << 1u)},
        {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)},
        {0x8, static_cast<std::uint32_t>(pixel >> 8u)}, {0x9, static_cast<std::uint32_t>(pixel >> 40u)}, {0xb, 0}
    };
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, 4);
    Write(commands, 0x79, 0x25b, (12u << 9u) | 4u);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0002a00u, 0u});
    return commands;
}

AgcDriver::DriverDetail::DrawEntryCounters Draw(const std::array<std::uint32_t, 4>& descriptor) {
    Pixels.fill(std::byte{0});
    auto commands = Setup(descriptor);
    const auto address = reinterpret_cast<std::uintptr_t>(Indices.data());
    commands.insert(commands.end(), {0xc0042700u, 3u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 3u, 0u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "mesh submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(std::to_integer<unsigned>(Pixels[(24u * Width + 16u) * 4u]) > 200u, "the mesh draw did not cover its triangle");
    return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
}

void MovedDescriptorTableHitsTheCache() {
    const auto vertices = reinterpret_cast<std::uintptr_t>(Vertices.data());
    for (auto& descriptor : Descriptors) descriptor = {static_cast<std::uint32_t>(vertices), static_cast<std::uint32_t>(vertices >> 32u) | (32u << 16u), static_cast<std::uint32_t>(Vertices.size()), 0x01016facu};
    const auto front = static_cast<std::size_t>(DrawMiss::FrontDiffering);
    AgcDriver::DriverDetail::DrawEntryCounters counters{};
    for (std::size_t i = 0; i < 4; ++i) counters = Draw(Descriptors[i]);
    Require(counters.lookups == 4 && counters.hits == 0, "the draws that teach the moved table made " + std::to_string(counters.hits) + " hits in " + std::to_string(counters.lookups) + " lookups, expected 0 in 4");
    const auto learned = counters;
    for (std::size_t i = 4; i < Descriptors.size(); ++i) counters = Draw(Descriptors[i]);
    Require(counters.misses[front] == learned.misses[front], "a relocated mesh stage was refused as a differing front stage (" + std::to_string(counters.misses[front] - learned.misses[front]) + " times)");
    Require(counters.hits - learned.hits == Descriptors.size() - 4, "mesh draws whose table moved again did not reuse the cached draw (" + std::to_string(counters.hits - learned.hits) + " hits in " + std::to_string(Descriptors.size() - 4) + " draws)");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device || !TargetHasCapability(device->Target(), spv::CapabilityMeshShadingEXT)) return VulkanTestSkipped;
            if (device->DeviceName().starts_with("llvmpipe")) {
                std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
                return VulkanTestSkipped;
            }
        }
        Register(GeometryHeader, GeometryCode.data(), sizeof(GeometryCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        MovedDescriptorTableHitsTheCache();
        AgcDriverShutdown_nid_postfix();
        std::cout << "A mesh draw whose descriptor table moved reused the cached draw\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
