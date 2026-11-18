#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::byte Kept{0x40};
constexpr std::byte Written{0xff};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80020005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 3> Vertices{{
    {-1.0f, 1.0f, 0.5f, 1.0f},
    {3.0f, 1.0f, 0.5f, 1.0f},
    {-1.0f, -3.0f, 0.5f, 1.0f},
}};

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

std::vector<std::uint32_t> Setup() {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const auto buffer = reinterpret_cast<std::uintptr_t>(Vertices.data());
    const auto bottomRight = (Height << 16u) | Width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, bottomRight},
        {0x81, 0x80010004}, {0x82, 0x000f003c},
        {0x90, 0x80020008}, {0x91, 0x000e0038},
        {0x94, 0x80040010}, {0x95, 0x000c0030},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-static_cast<float>(Height) / 2.0f)}, {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 4u << 1u},
        {0x8c, static_cast<std::uint32_t>(buffer)}, {0x8d, static_cast<std::uint32_t>((buffer >> 32u) & 0xffffu) | (16u << 16u)},
        {0x8e, static_cast<std::uint32_t>(Vertices.size())}, {0x8f, 0x01016facu},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 0},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    return commands;
}

void DrawScissorEnableTransitions() {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    constexpr std::array<std::uint32_t, 4> modes{2u, 0u, 2u, 0u};
    std::uint32_t failedSubmissions = 0;
    for (std::size_t index = 0; index < modes.size(); ++index) {
        Pixels.fill(Kept);
        auto commands = index == 0 ? Setup() : std::vector<std::uint32_t>{};
        Append(commands, 0x69, {0x292, modes[index]});
        Append(commands, 0x2d, {static_cast<std::uint32_t>(Vertices.size()), 2});
        const auto label = "submission " + std::to_string(index + 1) + " PA_SC_MODE_CNTL_0=" + std::to_string(modes[index]);
        std::printf("BEGIN %s\n", label.c_str());
        std::fflush(stdout);
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Pixels.data()), Pixels.size());
        const bool enabled = modes[index] == 2u;
        const std::uint32_t left = enabled ? 16u : 8u;
        const std::uint32_t right = enabled ? 48u : 56u;
        const std::uint32_t top = enabled ? 4u : 2u;
        const std::uint32_t bottom = enabled ? 12u : 14u;
        const std::uint32_t expectedWrittenPixels = enabled ? 256u : 576u;
        std::uint32_t writtenPixels = 0;
        std::uint32_t mismatchPixels = 0;
        std::uint32_t mismatchChannels = 0;
        std::string firstMismatch;
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto expected = x >= left && x < right && y >= top && y < bottom ? Written : Kept;
                bool written = true;
                bool mismatch = false;
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const auto value = Pixels[(y * Width + x) * 4u + channel];
                    written = written && value == Written;
                    if (value == expected) continue;
                    mismatch = true;
                    ++mismatchChannels;
                    if (firstMismatch.empty()) {
                        firstMismatch = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(std::to_integer<unsigned>(value)) + ", expected " + std::to_string(std::to_integer<unsigned>(expected));
                    }
                }
                if (written) ++writtenPixels;
                if (mismatch) ++mismatchPixels;
            }
        }
        const bool passed = mismatchChannels == 0 && writtenPixels == expectedWrittenPixels;
        if (!passed) ++failedSubmissions;
        std::printf("READBACK_%s %s written_pixels=%u expected_written_pixels=%u mismatch_pixels=%u mismatch_channels=%u\n", passed ? "PASS" : "FAIL", label.c_str(), writtenPixels, expectedWrittenPixels, mismatchPixels, mismatchChannels);
        if (!firstMismatch.empty()) std::printf("FIRST_MISMATCH %s: %s\n", label.c_str(), firstMismatch.c_str());
        std::fflush(stdout);
    }
    Require(failedSubmissions == 0, std::to_string(failedSubmissions) + " scissor enable submissions failed exact pixel readback");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        DrawScissorEnableTransitions();
        AgcDriverShutdown_nid_postfix();
        std::puts("PA_SC_MODE_CNTL_0 values 2, 0, 2, 0 produced four exact readbacks");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& cleanupError) {
            std::cerr << "CLEANUP_FAILURE after the original failure: " << cleanupError.what() << '\n';
        }
        return 1;
    }
}
