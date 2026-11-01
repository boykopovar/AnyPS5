#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 8> ValidVertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};

struct ColorBuffer {
    static constexpr std::size_t Size = 65536;
    ColorBuffer() {
#ifdef _WIN32
        data = static_cast<std::byte*>(VirtualAlloc(nullptr, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "cannot allocate the color target");
#else
        void* raw = mmap(nullptr, 2 * Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot map the color target");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + Size - 1) & ~(static_cast<std::uintptr_t>(Size) - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + Size != begin + 2 * Size) munmap(reinterpret_cast<void*>(aligned + Size), begin + 2 * Size - (aligned + Size));
        data = reinterpret_cast<std::byte*>(aligned);
#endif
        std::memset(data, 0, Size);
        GuestAllocations::Mutation().Add(data, Size, true, true, true);
    }
    ~ColorBuffer() {
        if (data != nullptr) {
            GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
            VirtualFree(data, 0, MEM_RELEASE);
#else
            munmap(data, Size);
#endif
            data = nullptr;
        }
    }
    ColorBuffer(const ColorBuffer&) = delete;
    ColorBuffer& operator=(const ColorBuffer&) = delete;

    std::byte* data = nullptr;
};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

const Shader* Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    return &header.shader;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> DrawPacket(std::uintptr_t color, const std::uint32_t* vertex, const std::uint32_t* pixel) {
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(vertex);
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(pixel);
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
        {0x3b8, 0x4dc6c000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertexAddress >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertexAddress >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixelAddress >> 8u)}, {0x009, static_cast<std::uint32_t>(pixelAddress >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 19});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {2863311532u, 2});
    return commands;
}

void QuadListIndexOverflowIsRejected() {
    ColorBuffer color;
    Registered(VertexHeader, ValidVertexCode.data(), ValidVertexCode.size() * sizeof(std::uint32_t), 2);
    Registered(PixelHeader, PixelCode.data(), PixelCode.size() * sizeof(std::uint32_t), 1);
    auto commands = DrawPacket(reinterpret_cast<std::uintptr_t>(color.data), ValidVertexCode.data(), PixelCode.data());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    std::string failure;
    try {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission was not accepted");
        AgcDriverWaitIdle_nid_postfix();
        Require(false, "the overflowing quad-list draw was accepted");
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find("expanded index count overflow") != std::string::npos, "the overflowing quad-list draw failed for the wrong reason: " + failure);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        QuadListIndexOverflowIsRejected();
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (...) {
        }
        std::cout << "quad-list index overflow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (...) {
        }
        return 1;
    }
}
