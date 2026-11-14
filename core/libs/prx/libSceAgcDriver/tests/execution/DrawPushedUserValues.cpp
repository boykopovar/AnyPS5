#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> UserPixelCode{0x7e00020cu, 0x7e02020du, 0x7e04020eu, 0x7e06020fu,
                                                                  0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u};

alignas(256) std::array<std::byte, 1024> ColorMemory{};

std::span<std::byte> MergedTarget(std::size_t index) {
    constexpr std::size_t Bytes = 65536;
    static std::array<std::byte*, 2> data{};
    if (data.at(index) == nullptr) data[index] = [] {
        std::byte* block = nullptr;
#ifdef _WIN32
        block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Bytes, Bytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, Bytes, PAGE_READWRITE, Bytes);
#else
        void* raw = mmap(nullptr, 2u * Bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot map the merged color target");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        block = reinterpret_cast<std::byte*>((begin + Bytes - 1u) & ~static_cast<std::uintptr_t>(Bytes - 1u));
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, Bytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(block, Bytes, true, true, true);
        return block;
    }();
    return {data[index], ColorMemory.size()};
}
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

std::vector<std::uint32_t> Frame(std::span<const float> alphas) {
    const auto color = reinterpret_cast<std::uintptr_t>(MergedTarget(0).data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0},
        {0x207, 0},
        {0x200, 0},
        {0x203, 0x800},
        {0x2dc, 0xaa00},
        {0x2f8, 0},
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
        {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000},
        {0x1e0, 0},
        {0xc, 0},
        {0xd, 0x40040},
        {0x81, 0x80000000},
        {0x82, 0x40040},
        {0x90, 0x80000000},
        {0x91, 0x40040},
        {0x94, 0x80000000},
        {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)},
        {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)},
        {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)},
        {0x114, 0},
        {0xb4, 0},
        {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0},
        {0x302, 0},
        {0x306, 0},
        {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)},
        {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x292, 2},
    };

    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, 1u << 1u});
    Append(commands, 0x76, {0x8c, 0});
    const auto pixel = reinterpret_cast<std::uintptr_t>(UserPixelCode.data());
    for (const auto alpha : alphas) {
        const std::array<std::uint32_t, 4> words{std::bit_cast<std::uint32_t>(0.25f), std::bit_cast<std::uint32_t>(0.5f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(alpha)};
        Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u)});
        Append(commands, 0x76, {0x009, static_cast<std::uint32_t>(pixel >> 40u)});
        Append(commands, 0x76, {0x00b, 16u << 1u});
        for (std::uint32_t index = 0; index < 12u; ++index) Append(commands, 0x76, {0x00cu + index, 0u});
        for (std::uint32_t index = 0; index < 4u; ++index) Append(commands, 0x76, {0x018u + index, words[index]});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

unsigned Submit(std::span<const float> alphas) {
    std::ranges::fill(MergedTarget(0), Kept);
    auto commands = Frame(alphas);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto color = MergedTarget(0);
    for (std::size_t offset = 0; offset < color.size(); offset += 4) {
        if (std::all_of(color.begin() + static_cast<std::ptrdiff_t>(offset), color.begin() + static_cast<std::ptrdiff_t>(offset + 4), [](std::byte value) { return value == Kept; })) continue;
        return static_cast<unsigned>(color[offset + 3]);
    }
    throw std::runtime_error("the draws covered no pixel");
}

unsigned Byte(float value) {
    return static_cast<unsigned>(value * 255.0f + 0.5f);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, UserPixelCode.data(), sizeof(UserPixelCode), 1);
        const auto keepsBefore = AgcDriver::Graphics::PushedUserDataKeeps();
        const std::vector<std::vector<float>> frames{{0.2f, 0.6f}, {0.6f, 0.2f}, {0.2f, 0.2f}, {0.8f}, {0.8f, 0.4f, 0.8f}, {0.4f}, {0.4f}};
        for (const auto& frame : frames) {
            const auto drawn = Submit(frame);
            Require(drawn == Byte(frame.back()), "a draw whose only change was a user word read from push data drew alpha " + std::to_string(drawn) + ", expected " + std::to_string(Byte(frame.back())));
        }
        if (AgcDriver::Graphics::DescriptorHeapEnabled())
            Require(AgcDriver::Graphics::PushedUserDataKeeps() > keepsBefore, "heap draws whose user words all fit push data did not take the pushed user-data path");
        std::puts("draws whose only change was a pushed user word read the new word");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
