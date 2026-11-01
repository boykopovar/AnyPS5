#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "SceShaders.hpp"
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
constexpr std::uint32_t Marker = 0x55u;
constexpr std::uint32_t BufferFormat = 0x31016facu;

alignas(256) constexpr std::array<std::uint32_t, 24> Caller{
    0x34040082u, 0x7e0202ffu, Marker, 0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0xf4281204u, 0xfa000040u, 0xbf8cc07fu, 0xbe880381u, 0x8f6a8408u, 0xf4280224u,
    0xd4000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u};
alignas(256) constexpr std::array<std::uint32_t, 2> Routine{0xbe80200eu, 0xbf9f0000u};
alignas(256) std::array<std::uint32_t, Threads> Output{};
alignas(16) std::array<std::uint64_t, 16> Records{};
alignas(16) std::array<std::uint32_t, 20> OuterBuffer{};
alignas(16) std::array<std::uint32_t, 4> OuterDescriptor{};

std::array<std::uint32_t, 4> BufferDescriptor(std::uintptr_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, BufferFormat};
}

std::vector<std::uint32_t> Commands() {
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
    const std::array<std::uint32_t, 1> resource{8u << 1u};
    registers(0x213, resource);
    const auto pointer = reinterpret_cast<std::uintptr_t>(OuterDescriptor.data());
    const auto output = BufferDescriptor(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    const std::array<std::uint32_t, 8> data{static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), 0u, 0u, output[0], output[1], output[2], output[3]};
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    return words;
}

std::string Dispatch() {
    Output.fill(Untouched);
    const auto words = Commands();
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    std::string failure;
    try {
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    return failure;
}

void Fill() {
    for (std::size_t record = 0; record < 8u; ++record) Records[record * 2u] = reinterpret_cast<std::uintptr_t>(Routine.data());
    const auto inner = BufferDescriptor(reinterpret_cast<std::uintptr_t>(Records.data()), sizeof(Records));
    for (std::size_t word = 0; word < inner.size(); ++word) OuterBuffer[16u + word] = inner[word];
    OuterDescriptor = BufferDescriptor(reinterpret_cast<std::uintptr_t>(OuterBuffer.data()), sizeof(OuterBuffer));
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Fill();
        const auto failure = Dispatch();
        Require(failure.empty(), "the dispatch through the nested call table failed: " + failure);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Require(Output[lane] == Marker, "lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + " after the call through the nested table, expected " + std::to_string(Marker));
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("compute nested table call tests passed");
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
