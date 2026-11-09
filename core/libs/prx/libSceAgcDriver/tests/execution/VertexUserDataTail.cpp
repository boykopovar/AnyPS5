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
alignas(256) std::array<std::uint32_t, 64> Prolog{};
alignas(256) std::array<std::uint32_t, 64> Reference{};
alignas(256) std::array<std::uint32_t, 64> Continuation{0xbf840003u, 0x7e08020cu, 0xf80008cfu, 0x04030201u, 0xbf810000u};
alignas(256) const std::array<std::uint32_t, 64> OtherContinuation{0xbf840003u, 0x7e0802f4u, 0xf80008cfu, 0x04030201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> VertexColorPixelCode{0xc8080000u, 0xc8090001u, 0xf800180fu, 0x02020202u, 0xbf810000u};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

Image Draw(const std::uint32_t* code, const std::uint32_t* continuation, std::uint32_t distanceLayout = 0, bool vertexColor = false) {
    Pixels.fill(Kept);
    const auto vertex = reinterpret_cast<std::uintptr_t>(code);
    const auto pixel = reinterpret_cast<std::uintptr_t>(vertexColor ? VertexColorPixelCode.data() : PixelCode.data());
    const auto next = reinterpret_cast<std::uintptr_t>(continuation);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, vertexColor ? 1u : 0u}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0},
        {0x207, distanceLayout}, {0x200, 0}, {0x203, 0x800},
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
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 2u << 1u},
        {0x8c, static_cast<std::uint32_t>(next)}, {0x8d, static_cast<std::uint32_t>(next >> 32u)},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
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
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "vertex tail draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, Pixels.size());
    return Pixels;
}

Image Check(std::span<const std::uint32_t> prefix, std::span<const std::uint32_t> continuation, std::uint32_t distanceLayout = 0, std::size_t continuationWords = 5, bool vertexColor = false) {
    Prolog.fill(0xbf9f0000u);
    Reference.fill(0xbf9f0000u);
    std::copy(prefix.begin(), prefix.end(), Prolog.begin());
    Prolog[prefix.size()] = 0xbe802008u;
    std::copy(prefix.begin(), prefix.end(), Reference.begin());
    std::copy_n(continuation.begin(), continuationWords, Reference.begin() + prefix.size());
    const auto expected = Draw(Reference.data(), continuation.data(), distanceLayout, vertexColor);
    for (unsigned repeat = 0; repeat < 2u; ++repeat) {
        const auto actual = Draw(Prolog.data(), continuation.data(), distanceLayout, vertexColor);
        Require(actual == expected, "linked vertex tail pixels differ from the monolithic shader");
    }
    return expected;
}

void CheckBranchColors(const Image& written, const Image& skipped, const char* state) {
    std::size_t covered = 0;
    for (std::size_t pixel = 0; pixel < written.size(); pixel += 4) {
        const bool untouched = written[pixel] == Kept;
        for (std::size_t component = 0; component < 4; ++component) {
            Require(written[pixel + component] == (untouched ? Kept : std::byte{0xff}) &&
                    skipped[pixel + component] == (untouched ? Kept : std::byte{0}), std::string(state) + " did not select the expected vertex color");
        }
        if (!untouched) ++covered;
    }
    Require(covered != 0 && covered != written.size() / 4, std::string(state) + " reference triangle did not cross the render target");
}

void ControlStateDraws(std::vector<std::uint32_t> prefix) {
    alignas(256) const std::array<std::uint32_t, 7> sccContinuation{0xbf840001u, 0x7e0c02f2u, 0xf800020fu, 0x06060606u, 0xf80008cfu, 0x04030201u, 0xbf810000u};
    prefix.insert(prefix.end() - 1, 0x7e0c0280u);
    const auto trueScc = Check(prefix, sccContinuation, 0, sccContinuation.size(), true);
    prefix.back() = 0xbf070c0cu;
    const auto falseScc = Check(prefix, sccContinuation, 0, sccContinuation.size(), true);
    CheckBranchColors(trueScc, falseScc, "SCC");
    alignas(256) const std::array<std::uint32_t, 7> execContinuation{0x7e0c02f2u, 0xbefe040eu, 0xf800020fu, 0x06060606u, 0xf80008cfu, 0x04030201u, 0xbf810000u};
    prefix.back() = 0xbf060c0cu;
    prefix.push_back(0xbe8e047eu);
    const auto activeExec = Check(prefix, execContinuation, 0, execContinuation.size(), true);
    prefix.push_back(0xbefe0480u);
    const auto emptyExec = Check(prefix, execContinuation, 0, execContinuation.size(), true);
    CheckBranchColors(activeExec, emptyExec, "EXEC");
}

void VertexTailDraws(bool cullDistance) {
    std::vector<std::uint32_t> prefix{
        0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
        0x7e060280u, 0x7e0802f2u, 0xbe8c03ffu, 0x3f800000u, 0xbf060c0cu,
    };
    const auto original = Check(prefix, Continuation);
    const auto kept = std::count(original.begin(), original.end(), Kept);
    Require(kept != 0 && kept != static_cast<std::ptrdiff_t>(original.size()), "reference triangle did not cross the render target");
    const auto changedAddress = Check(prefix, OtherContinuation);
    Require(changedAddress != original, "changing the continuation did not change the reference pixels");
    Continuation[1] = OtherContinuation[1];
    Require(Check(prefix, Continuation) == changedAddress, "rewritten continuation kept the old pixels");
    Continuation[1] = 0x7e08020cu;
    Require(Check(prefix, Continuation) == original, "restored continuation did not restore the pixels");
    alignas(256) const std::array<std::uint32_t, 7> clipContinuation{0xbf840005u, 0x7e08020cu, 0xf80000cfu, 0x04030201u, 0xf80008dfu, 0x01010101u, 0xbf810000u};
    const auto clipped = Check(prefix, clipContinuation, 0x00400001u, clipContinuation.size());
    Require(clipped != original && std::count(clipped.begin(), clipped.end(), Kept) > kept, "a clip distance exported by the continuation did not clip the triangle");
    if (cullDistance) {
        const auto culled = Check(prefix, clipContinuation, 0x00400100u, clipContinuation.size());
        Require(culled == original, "a mixed-sign cull distance discarded the triangle");
    } else {
        std::puts("cull distance draw skipped, the device has no shaderCullDistance");
    }
    ControlStateDraws(prefix);
}

}

int main() {
    try {
        bool cullDistance = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            cullDistance = TargetHasCapability(device->Target(), spv::CapabilityCullDistance);
        }
        VertexTailDraws(cullDistance);
        AgcDriverShutdown_nid_postfix();
        std::puts("vertex user-data tail tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
