#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 4> ComputeCode{0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> Target{};
alignas(64) std::atomic<std::uint32_t> Released{0};
alignas(64) std::uint32_t Condition = 0;

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::uint32_t Low(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u);
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the submission failed");
}

void CreateDevice() {
    const auto code = reinterpret_cast<std::uintptr_t>(ComputeCode.data());
    std::vector<std::uint32_t> commands;
    Append(commands, 0x76, {0x207, 1, 1, 1});
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)});
    Append(commands, 0x76, {0x213, 0});
    Append(commands, 0x15, {1, 1, 1, 0x41});
    Submit(commands);
    AgcDriverWaitIdle_nid_postfix();
}

std::vector<std::uint32_t> RawDrawInFalseCondition() {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(Target.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
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
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x3c, {0x13u, Low(&Released), High(&Released), 1u, 0xffffffffu, 4u});
    Append(commands, 0x22, {Low(&Condition), High(&Condition), 0u, 3u});
    Append(commands, 0x2d, {3, 2});
    return commands;
}

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void SubmitSkippedRawDraw() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto before = driver.DrawAheadStatistics();
    Released.store(0, std::memory_order_release);
    auto commands = RawDrawInFalseCondition();
    Submit(commands);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (driver.DrawAheadStatistics().published == before.published) {
        if (std::chrono::steady_clock::now() >= deadline) {
            Released.store(1, std::memory_order_release);
            throw std::runtime_error("timed out waiting for the scan to publish the raw draw");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Released.store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
}

void SkippedRawDrawLeavesRegistrationOpen() {
    CreateDevice();
    SubmitSkippedRawDraw();
    Require(!AgcDriver::DriverDetail::RawGraphicsDrawn(), "the draw-ahead scan noted a raw graphics draw that the queue skipped");
    Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
}

void SkippedMixedDrawLeavesRegistrationOpen() {
    SubmitSkippedRawDraw();
    Require(!AgcDriver::DriverDetail::RawGraphicsDrawn(), "the draw-ahead scan noted the raw fragment program of a draw with a registered vertex shader that the queue skipped");
    Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        SkippedRawDrawLeavesRegistrationOpen();
        SkippedMixedDrawLeavesRegistrationOpen();
        AgcDriverShutdown_nid_postfix();
        std::cout << "Raw and mixed raw draws the queue skipped left shader registration open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Released.store(1, std::memory_order_release);
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
