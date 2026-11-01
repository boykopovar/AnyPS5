#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
alignas(256) std::array<std::byte, 1024> Pixels{};
alignas(256) std::array<std::uint32_t, 64> Source{};
alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 16> PixelCode{
    0x7e020280u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x7e000280u, 0x7e0602f2u, 0xf800180fu, 0x03010002u, 0xbf810000u,
};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void Draw(const std::array<std::uint32_t, 4>& descriptor, std::uint32_t* label = nullptr) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
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
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 4u << 1u},
        {0x00c, descriptor[0]}, {0x00d, descriptor[1]}, {0x00e, descriptor[2]}, {0x00f, descriptor[3]},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
    Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    Append(commands, 0x2d, {3, 2});
    if (label != nullptr) {
        const auto address = reinterpret_cast<std::uintptr_t>(label);
        Append(commands, 0x37, {0x00100200u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 7u});
    }
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "unsupported buffer type draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void NamesShaderAndDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Source.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), sizeof(Source), 0xa1b00facu};
    try {
        Draw(descriptor);
    } catch (const std::exception& error) {
        const std::string message = error.what();
        char shader[64];
        std::snprintf(shader, sizeof(shader), "shader 0x%llx stage %u", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(PixelCode.data())), static_cast<unsigned>(ShaderRecompiler::ShaderStage::Fragment));
        char words[64];
        std::snprintf(words, sizeof(words), "(buffer 0 V# %08x %08x %08x %08x)", descriptor[0], descriptor[1], descriptor[2], descriptor[3]);
        Require(message.find("buffer descriptor uses an unsupported type") != std::string::npos, "draw failed for another reason: " + message);
        Require(message.find(shader) != std::string::npos, std::string("draw error does not name ") + shader + ": " + message);
        Require(message.find(words) != std::string::npos, std::string("draw error does not name ") + words + ": " + message);
        if (std::getenv("APS5_DRAW_PIPELINE") != nullptr) Require(message.find("draw at submission ") != std::string::npos && message.find("(finished on the draw finisher)") != std::string::npos, "a draw finished on the draw finisher does not name its submission and offset: " + message);
        return;
    }
    throw std::runtime_error("a pixel shader read a buffer through a descriptor of type 2");
}

void SkipsRefusedDraw() {
    const auto address = reinterpret_cast<std::uintptr_t>(Source.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), sizeof(Source), 0xa1b00facu};
    alignas(8) static std::uint32_t label = 0;
    for (int submission = 0; submission < 2; ++submission) {
        label = 0;
        Draw(descriptor, &label);
        Require(label == 7u, "the packet after a skipped refused draw was not executed");
    }
    for (const auto byte : Pixels) Require(byte == std::byte{0}, "a skipped refused draw wrote its color target");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--skip-refused"), "invalid test arguments");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (argc == 2) SkipsRefusedDraw();
        else NamesShaderAndDescriptor();
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        std::puts("draw unsupported buffer type tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
