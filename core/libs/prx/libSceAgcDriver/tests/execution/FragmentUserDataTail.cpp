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
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::byte Kept{0x40};
using Image = std::array<std::byte, 1024>;
alignas(256) Image Pixels{};
alignas(256) std::array<std::uint32_t, 64> Body{};
alignas(256) std::array<std::uint32_t, 64> Reference{};
alignas(256) std::array<std::uint32_t, 64> Epilog{0xbf850001u, 0x7e000280u, 0x7e040208u, 0x7e0602f2u, 0xf800180fu, 0x03010002u, 0xbf810000u};
alignas(256) const std::array<std::uint32_t, 64> OtherEpilog{0xbf850001u, 0x7e000280u, 0x7e040201u, 0x7e0602f2u, 0xf800180fu, 0x03010002u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

Image Draw(const std::uint32_t* code, const std::uint32_t* epilog, std::uint32_t userCount) {
    Pixels.fill(Kept);
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(code);
    const auto next = reinterpret_cast<std::uintptr_t>(epilog);
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
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, userCount << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
        {0x00d, std::bit_cast<std::uint32_t>(0.5f)}, {0x010, static_cast<std::uint32_t>(next)}, {0x011, 0x00803fc1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
    Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "fragment tail draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, Pixels.size());
    return Pixels;
}

Image Check(std::span<const std::uint32_t> prefix, std::span<const std::uint32_t> epilog, std::uint32_t userCount = 6u) {
    Body.fill(0xbf9f0000u);
    Reference.fill(0xbf9f0000u);
    std::copy(prefix.begin(), prefix.end(), Body.begin());
    Body[prefix.size()] = 0xbe802004u;
    std::copy(prefix.begin(), prefix.end(), Reference.begin());
    std::copy_n(epilog.begin(), 7u, Reference.begin() + prefix.size());
    const auto expected = Draw(Reference.data(), epilog.data(), userCount);
    for (unsigned repeat = 0; repeat < 2u; ++repeat) {
        const auto actual = Draw(Body.data(), epilog.data(), userCount);
        Require(actual == expected, "linked fragment tail pixels differ from the monolithic shader");
    }
    return expected;
}

void FragmentTailDraws() {
    const auto high = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Epilog.data()) >> 32u);
    std::vector<std::uint32_t> prefix{
        0xbe880300u, 0x7e000201u, 0x7e020280u, 0xbf060505u, 0xbe8503ffu, high,
    };
    const auto original = Check(prefix, Epilog);
    const auto kept = std::count(original.begin(), original.end(), Kept);
    Require(kept != 0 && kept != static_cast<std::ptrdiff_t>(original.size()), "reference triangle did not cross the render target");
    Require(std::count(original.begin(), original.end(), std::byte{0xff}) > 0, "fragment continuation did not export its carried scalar color");
    Require(Check(prefix, Epilog, 5u) == original, "constant high tail required a second user-data word");
    prefix.back() = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(OtherEpilog.data()) >> 32u);
    const auto changedAddress = Check(prefix, OtherEpilog);
    Require(changedAddress != original, "changing the epilog did not change the reference pixels");
    prefix.back() = high;
    Epilog[2] = OtherEpilog[2];
    Require(Check(prefix, Epilog) == changedAddress, "rewritten epilog kept the old pixels");
    Epilog[2] = 0x7e040208u;
    Require(Check(prefix, Epilog) == original, "restored epilog did not restore the pixels");
    prefix[3] = 0xbf070505u;
    Require(Check(prefix, Epilog) != original, "SCC was not preserved across the fragment tail jump");
    prefix[3] = 0xbf060505u;
    prefix.push_back(0xbefe0480u);
    const auto emptyExec = Check(prefix, Epilog);
    Require(std::ranges::all_of(emptyExec, [](std::byte value) { return value == Kept; }), "EXEC was not preserved across the fragment tail jump");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        FragmentTailDraws();
        AgcDriverShutdown_nid_postfix();
        std::puts("fragment user-data tail tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
