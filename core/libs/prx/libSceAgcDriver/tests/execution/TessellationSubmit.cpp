#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 80;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 64> HullUserMemory{};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalCode{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102, 0x7e080d04, 0x7e0a0d05,
    0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666, 0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105,
    0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008, 0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullCode{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff,
    0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008,
    0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 30> DomainCode{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01, 0xe0341020, 0x04030e01,
    0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012,
    0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

constexpr std::array<std::uint32_t, 3> PatchLevels{1u, 0u, 5u};

constexpr std::uint16_t Corner(std::uint32_t patch, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint16_t>((PatchLevels[patch] << 8u) | (patch << 4u) | (x << 2u) | y);
}

alignas(256) constexpr std::array<std::uint16_t, 9> Indices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2), Corner(1, 0, 0), Corner(1, 2, 0),
    Corner(1, 1, 2), Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

struct Header {
    Shader shader{};
    ShaderUserData users{};
};

Header LocalHeader{};
Header HullHeader{};
Header DomainHeader{};
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

std::uint32_t Lo(const void* address) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address) >> 8u);
}

std::uint32_t Hi(const void* address) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address) >> 40u);
}

std::vector<std::uint32_t> Commands() {
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto users = reinterpret_cast<std::uintptr_t>(HullUserMemory.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{{0x2d5, 0x0200210du},
                                                                       {0x2d6, 128u | (3u << 8u) | (3u << 14u)},
                                                                       {0x2db, 1u | (2u << 2u) | (2u << 5u)},
                                                                       {0x286, std::bit_cast<std::uint32_t>(64.0f)},
                                                                       {0x287, std::bit_cast<std::uint32_t>(1.0f)},
                                                                       {0x1b6, 1},
                                                                       {0x191, 0x400},
                                                                       {0x1b3, 0},
                                                                       {0x1b4, 0},
                                                                       {0x260, 0},
                                                                       {0x207, 0},
                                                                       {0x200, 0},
                                                                       {0x203, 0x800},
                                                                       {0x2dc, 0xaa00},
                                                                       {0x2f8, 0},
                                                                       {0x292, 2},
                                                                       {0x293, 0},
                                                                       {0x80, 0},
                                                                       {0x8d, 0},
                                                                       {0x83, 0xffff},
                                                                       {0x8c, 0xa},
                                                                       {0x2f9, 0x2d},
                                                                       {0x313, 0x6000},
                                                                       {0x30e, 0xffffffff},
                                                                       {0x30f, 0xffffffff},
                                                                       {0x206, 0x43f},
                                                                       {0x204, 0x80000},
                                                                       {0x205, 0x240},
                                                                       {0x8e, 0xf},
                                                                       {0x8f, 0xf},
                                                                       {0x202, 0xcc0010},
                                                                       {0x1c4, 0},
                                                                       {0x1c5, 9},
                                                                       {0x1c3, 4},
                                                                       {0x31c, 0x28028},
                                                                       {0x31b, 0},
                                                                       {0x31d, 0},
                                                                       {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
                                                                       {0x3b8, 0x9000000},
                                                                       {0x1e0, 0},
                                                                       {0xc, 0},
                                                                       {0xd, (Height << 16u) | Width},
                                                                       {0x81, 0x80000000},
                                                                       {0x82, (Height << 16u) | Width},
                                                                       {0x90, 0x80000000},
                                                                       {0x91, (Height << 16u) | Width},
                                                                       {0x94, 0x80000000},
                                                                       {0x95, (Height << 16u) | Width},
                                                                       {0x318, static_cast<std::uint32_t>(color >> 8u)},
                                                                       {0x390, static_cast<std::uint32_t>(color >> 40u)},
                                                                       {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)},
                                                                       {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f)},
                                                                       {0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f))},
                                                                       {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
                                                                       {0x113, std::bit_cast<std::uint32_t>(1.0f)},
                                                                       {0x114, 0},
                                                                       {0xb4, 0},
                                                                       {0xb5, std::bit_cast<std::uint32_t>(1.0f)}};
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shaders{{0x148, Lo(LocalCode.data())},
                                                                       {0x149, Hi(LocalCode.data())},
                                                                       {0x108, Lo(HullCode.data())},
                                                                       {0x109, Hi(HullCode.data())},
                                                                       {0x102, static_cast<std::uint32_t>(users)},
                                                                       {0x103, static_cast<std::uint32_t>(users >> 32u)},
                                                                       {0x10b, 0},
                                                                       {0xc8, Lo(DomainCode.data())},
                                                                       {0xc9, Hi(DomainCode.data())},
                                                                       {0x8b, 0},
                                                                       {0x8, Lo(PixelCode.data())},
                                                                       {0x9, Hi(PixelCode.data())},
                                                                       {0xb, 0}};
    std::vector<std::uint32_t> commands;
    Write(commands, 0x79, 0x242, 9);
    for (const auto& [offset, value] : context) Write(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shaders) Write(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0002a00u, 0u});
    const auto address = reinterpret_cast<std::uintptr_t>(Indices.data());
    commands.insert(commands.end(), {0xc0042700u, static_cast<std::uint32_t>(Indices.size()), static_cast<std::uint32_t>(address),
                                     static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(Indices.size()), 0u});
    return commands;
}

std::array<std::uint8_t, 4> PixelAt(std::uint32_t x, std::uint32_t y) {
    const auto offset = (static_cast<std::size_t>(y) * Width + x) * 4u;
    return {std::to_integer<std::uint8_t>(Pixels[offset]), std::to_integer<std::uint8_t>(Pixels[offset + 1]),
            std::to_integer<std::uint8_t>(Pixels[offset + 2]), std::to_integer<std::uint8_t>(Pixels[offset + 3])};
}

std::string PixelText(const std::array<std::uint8_t, 4>& pixel) {
    return "(" + std::to_string(pixel[0]) + ", " + std::to_string(pixel[1]) + ", " + std::to_string(pixel[2]) + ", " +
           std::to_string(pixel[3]) + ")";
}

std::uint32_t Column(std::uint32_t patch, float offset) {
    return static_cast<std::uint32_t>((-0.9f + 0.6f * static_cast<float>(patch) + offset + 1.0f) * 0.5f * Width);
}

std::uint32_t Row(float y) {
    return static_cast<std::uint32_t>((1.0f - y) * 0.5f * Height);
}

std::uint32_t CoveredPixels(std::uint32_t patch, bool interior) {
    std::uint32_t count = 0;
    for (std::uint32_t y = Row(0.5f); y <= Row(-0.5f) && y < Height; ++y) {
        for (std::uint32_t x = Column(patch, 0.0f); x <= Column(patch, 0.5f) && x < Width; ++x) {
            const auto pixel = PixelAt(x, y);
            if (pixel == Background) continue;
            Require(pixel[0] == 255 && pixel[2] == 0 && pixel[3] == 255, "patch " + std::to_string(patch) + " pixel (" + std::to_string(x) +
                                                                             ", " + std::to_string(y) + ") is " + PixelText(pixel));
            if (!interior || pixel[1] > 32) ++count;
        }
    }
    return count;
}

void SubmittedPatches() {
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) Pixels[i + c] = std::byte{Background[c]};
    }
    const auto pushes = AgcDriver::Graphics::DataAddressPushes();
    auto commands = Commands();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "tessellation submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    Require(CoveredPixels(0, false) > 100 && CoveredPixels(0, true) == 0,
            "the level-1 patch was not drawn as one flat triangle, centre " + PixelText(PixelAt(Column(0, 0.25f), center)));
    Require(CoveredPixels(1, false) == 0, "the patch with level 0 was not culled");
    Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), center)[1] > 32,
            "the level-5 patch has no interior domain points at its centre, " + PixelText(PixelAt(Column(2, 0.25f), center)));
    Require(PixelAt(0, 0) == Background && PixelAt(Width - 1, Height - 1) == Background, "the corners changed");
    Require(!AgcDriver::Graphics::DescriptorHeapEnabled() || AgcDriver::Graphics::DataAddressPushes() > pushes,
            "the submitted tessellation draw did not take the descriptor heap");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (!device->Target().tessellation.has_value()) {
                std::cout << "skipped, the device has no tessellation shaders\n";
                return VulkanTestSkipped;
            }
        }
        Register(LocalHeader, LocalCode.data(), sizeof(LocalCode), 5);
        Register(HullHeader, HullCode.data(), sizeof(HullCode), 7);
        Register(DomainHeader, DomainCode.data(), sizeof(DomainCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        SubmittedPatches();
        AgcDriverShutdown_nid_postfix();
        std::cout << "A submitted tessellation draw drew its patches\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
