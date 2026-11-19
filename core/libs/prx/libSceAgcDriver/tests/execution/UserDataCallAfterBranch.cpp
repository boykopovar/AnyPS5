#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Untouched = 0xcafebabeu;

alignas(256) constexpr std::array<std::uint32_t, 64> Caller{
    0x7e020280u, 0xbe8e0300u, 0xbe8f0301u, 0xbf880001u, 0x7e020280u, 0xbe8e210eu, 0x34040082u, 0xe0701000u,
    0x80010102u, 0xbf810000u, 0xbf9f0000u, 0xbf9f0000u, 0xbf9f0000u, 0xbf9f0000u, 0xbf9f0000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 2> AddFive{0x4a020285u, 0xbe80200eu};
alignas(256) constexpr std::array<std::uint32_t, 2> AddOne{0x4a020281u, 0xbe80200eu};
alignas(256) std::array<std::uint32_t, Threads> Output{};

std::array<std::uint32_t, 8> UserData(const void* first, const void* second) {
    const auto low = reinterpret_cast<std::uintptr_t>(first);
    const auto high = reinterpret_cast<std::uintptr_t>(second);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(low >> 32u), static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(high >> 32u),
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

std::vector<std::uint32_t> Commands(const std::array<std::uint32_t, 8>& data, bool wave32) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Caller.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(data.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u | (wave32 ? 0x8000u : 0u)});
    return words;
}

void Dispatch(const void* first, const void* second, bool wave32) {
    Output.fill(Untouched);
    const auto words = Commands(UserData(first, second), wave32);
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
}

void Check(const void* first, const void* second, bool wave32, std::uint32_t expected, const std::string& what) {
    Dispatch(first, second, wave32);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(Output[lane] == expected, what + (wave32 ? " (wave32)" : " (wave64)") + ": lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        for (const bool wave32 : {false, true}) {
            Check(AddFive.data(), AddOne.data(), wave32, 5u, "a user-data callee copied before a forward branch");
            Check(AddOne.data(), AddFive.data(), wave32, 1u, "the callee replaced");
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("user-data call after branch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception&) {
        }
        return 1;
    }
}
