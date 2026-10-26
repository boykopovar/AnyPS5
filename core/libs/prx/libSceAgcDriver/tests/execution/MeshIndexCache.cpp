#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "../MeshGeometryFixture.hpp"
#include "VulkanTestDevice.hpp"
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
alignas(256) constexpr std::array<std::uint16_t, 10> Indices{0, 1, 2, 3, 4, 5, 0, 0, 0, 0};
alignas(256) constexpr std::array<std::uint16_t, 4> RightIndices{3, 4, 5, 0};
alignas(256) constexpr std::array<std::array<float, 8>, 6> Vertices{{
    {-0.9f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.1f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.5f, 0.8f, 0, 1, 1, 1, 1, 1},
    {0.1f, -0.8f, 0, 1, 1, 1, 1, 1}, {0.9f, -0.8f, 0, 1, 1, 1, 1, 1}, {0.5f, 0.8f, 0, 1, 1, 1, 1, 1}
}};
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

std::vector<std::uint32_t> Setup() {
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto geometry = reinterpret_cast<std::uintptr_t>(AgcDriver::Test::MeshGeometryCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto vertices = reinterpret_cast<std::uintptr_t>(Vertices.data());
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
        {0x8a, 0x600c0000u}, {0x8b, (8u << 19u) | (3u << 16u) | (4u << 1u)},
        {0x8c, static_cast<std::uint32_t>(vertices)}, {0x8d, static_cast<std::uint32_t>(vertices >> 32u) | (32u << 16u)},
        {0x8e, static_cast<std::uint32_t>(Vertices.size())}, {0x8f, 0x01016facu},
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

void Draw(const std::uint16_t* indices, std::uint32_t count, bool left, bool right) {
    Pixels.fill(std::byte{0});
    auto commands = Setup();
    const auto address = reinterpret_cast<std::uintptr_t>(indices);
    commands.insert(commands.end(), {0xc0042700u, count, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), count, 0u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "mesh submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto lit = [](std::uint32_t x) { return std::to_integer<unsigned>(Pixels[(24u * Width + x) * 4u]) > 200u; };
    Require(lit(16) == left && lit(48) == right, "cached mesh draw used another draw's indices: count=" + std::to_string(count));
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
        Register(GeometryHeader, AgcDriver::Test::MeshGeometryCode.data(), sizeof(AgcDriver::Test::MeshGeometryCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        for (unsigned i = 0; i < 3; ++i) Draw(Indices.data(), 3, true, false);
        const auto warm = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(warm.hits != 0, "mesh warm-up did not reach the draw cache");
        const auto front = static_cast<std::size_t>(DrawMiss::FrontDiffering);
        const auto layout = static_cast<std::size_t>(DrawMiss::Layout);
        Draw(Indices.data(), 6, true, true);
        const auto range = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(range.misses[front] == warm.misses[front] + 1 && range.misses[layout] == warm.misses[layout], "an index-range change was not counted as a differing front stage");
        Draw(Indices.data(), 6, true, true);
        const auto rangeReuse = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(rangeReuse.hits == range.hits + 1, "the changed index range was not retained for cache reuse");
        Draw(RightIndices.data(), 3, false, true);
        const auto address = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(address.misses[front] == rangeReuse.misses[front] + 1 && address.misses[layout] == rangeReuse.misses[layout], "an index-address change was not counted as a differing front stage");
        Draw(RightIndices.data(), 3, false, true);
        const auto addressReuse = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(addressReuse.hits == address.hits + 1, "the changed index address was not retained for cache reuse");
        Draw(Indices.data(), 3, true, false);
        const auto earlier = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
        Require(earlier.hits == addressReuse.hits + 1, "returning to an earlier index descriptor did not hit the cache");
        AgcDriverShutdown_nid_postfix();
        std::cout << "Cached mesh draws retained their index ranges and addresses\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
