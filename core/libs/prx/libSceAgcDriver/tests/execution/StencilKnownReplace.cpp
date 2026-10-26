#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
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

constexpr std::byte Kept{0x40};

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 4> WhitePixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
alignas(256) std::array<std::byte, 65536> DepthMemory{};
alignas(256) std::array<std::byte, 65536> StencilMemory{};

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
        header.registers = {{{0x008, static_cast<std::uint32_t>(address >> 8u)}, {0x009, static_cast<std::uint32_t>(address >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)}}};
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
    }
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

struct StencilState {
    std::uint32_t depthControl;
    std::uint32_t operations;
    std::uint32_t refMask;
};

std::vector<std::uint32_t> Draw(const StencilState& stencilState) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(WhitePixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory.data());
    const auto stencil = reinterpret_cast<std::uintptr_t>(StencilMemory.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 0}, {0x1b4, 0}, {0x1b6, 0}, {0x207, 0}, {0x200, stencilState.depthControl}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 9}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
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
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)}, {0x292, 2},
        {0x000, 0}, {0x002, 0}, {0x007, (3u << 16u) | 63u},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, 3}, {0x011, 1},
        {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
        {0x013, static_cast<std::uint32_t>(stencil >> 8u)}, {0x015, static_cast<std::uint32_t>(stencil >> 8u)},
        {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
        {0x01b, static_cast<std::uint32_t>(stencil >> 40u)}, {0x01d, static_cast<std::uint32_t>(stencil >> 40u)},
        {0x10b, stencilState.operations}, {0x10c, stencilState.refMask}, {0x10d, stencilState.refMask},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
    return commands;
}

void Submit(const StencilState& stencilState) {
    ColorMemory.fill(Kept);
    auto commands = Draw(stencilState);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the stencil draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

constexpr std::uint32_t StencilEnable = 1u;
constexpr std::uint32_t Equal = 2u << 8u;
constexpr std::uint32_t LessEqual = 3u << 8u;
constexpr std::uint32_t Greater = 4u << 8u;
constexpr std::uint32_t NotEqual = 5u << 8u;
constexpr std::uint32_t Always = 7u << 8u;

std::uint32_t RefMask(std::uint32_t test, std::uint32_t compareMask, std::uint32_t writeMask, std::uint32_t opValue) {
    return test | (compareMask << 8u) | (writeMask << 16u) | (opValue << 24u);
}

std::array<std::byte, 1024> Fill(std::uint32_t value) {
    Submit({StencilEnable | Always, 0x30u, RefMask(value, 0xff, 0xff, 0)});
    const auto coverage = ColorMemory;
    Require(std::count(coverage.begin(), coverage.end(), Kept) != static_cast<std::ptrdiff_t>(coverage.size()), "the stencil fill covered no pixels");
    return coverage;
}

void Expect(const std::array<std::byte, 1024>& coverage, std::uint32_t value, const std::string& what) {
    Submit({StencilEnable | Equal, 0u, RefMask(value, 0xff, 0, 0)});
    std::size_t drawn = 0;
    for (std::size_t pixel = 0; pixel < coverage.size(); pixel += 4u) {
        const bool covered = coverage[pixel] != Kept;
        drawn += covered;
        for (std::size_t channel = 0; channel < 4u; ++channel) Require(ColorMemory[pixel + channel] == (covered ? std::byte{255} : Kept), what + ": the stencil is not " + std::to_string(value) + " exactly where the draw covered");
    }
    Require(drawn != 0, what + ": the probe covered no pixels");
}

void Case(const std::string& what, std::uint32_t initial, const StencilState& stencilState, std::uint32_t expected, bool testPasses = true) {
    const auto coverage = Fill(initial);
    Submit(stencilState);
    if (testPasses) Require(ColorMemory == coverage, what + ": the draw under test did not color exactly the filled pixels");
    else Require(std::all_of(ColorMemory.begin(), ColorMemory.end(), [](auto value) { return value == Kept; }), what + ": a draw whose stencil test fails wrote color");
    Expect(coverage, expected, what);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, WhitePixelCode.data(), sizeof(WhitePixelCode), 1);
        const std::array<ShaderRegister, 3> pixelContext{{{0x203, 0x800}, {0x1c4, 9}, {0x1b6, 0}}};
        AgcDriverResolveShaderAbi_nid_postfix(&PixelHeader.shader, pixelContext, {});
        Case("an equal test replaced by the operation value", 0xff, {StencilEnable | Equal, 0x40u, RefMask(0xff, 0xff, 0xff, 0x40)}, 0x40);
        Case("an equal test replaced by the operation value on pass and depth fail", 0xff, {StencilEnable | Equal, 0x440u, RefMask(0xff, 0xff, 0xff, 0x40)}, 0x40);
        Case("an equal test replaced by 0xff", 0x25, {StencilEnable | Equal, 0x20u, RefMask(0x25, 0xff, 0xff, 0)}, 0xff);
        Case("an equal test replaced through a partial write mask", 0xff, {StencilEnable | Equal, 0x40u, RefMask(0xff, 0xff, 0xf0, 0x40)}, 0x4f);
        Case("a failed not-equal test replaced by the operation value", 0xff, {StencilEnable | NotEqual, 0x4u, RefMask(0xff, 0xff, 0xff, 0x40)}, 0x40, false);
        Case("a less-or-equal test against 0xff replaced by the operation value", 0xff, {StencilEnable | LessEqual, 0x40u, RefMask(0xff, 0xff, 0xff, 0x40)}, 0x40);
        Case("a failed greater test against 0xff replaced by the operation value", 0xff, {StencilEnable | Greater, 0x4u, RefMask(0xff, 0xff, 0xff, 0x40)}, 0x40, false);
        AgcDriverShutdown_nid_postfix();
        std::puts("stencil replacements with a known stencil value passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
