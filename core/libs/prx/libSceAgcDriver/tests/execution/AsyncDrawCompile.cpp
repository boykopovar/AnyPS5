#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 7> Code{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t StoredFormat = 0x00014204u;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Stored = 0x12345678u;
alignas(256) std::array<std::uint32_t, 64> Output{};

void Dispatch() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> data{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Output.size()), StoredFormat};
    const auto program = reinterpret_cast<std::uintptr_t>(Code.data());
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207, 64, 1, 1,
        0xc0027600u, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600u, 0x213, (4u << 1u) | (1u << 7u),
        0xc0047600u, 0x240, data[0], data[1], data[2], data[3],
        0xc0031500u, 1, 1, 1, 0x41u,
    };
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(address, sizeof(Output));
}

bool Done() {
    for (const auto word : Output) {
        if (word != Stored) return false;
    }
    return true;
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Output.fill(Untouched);
        const bool async = AgcDriver::DriverDetail::AsyncDrawCompile();
        std::size_t attempts = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        do {
            Dispatch();
            ++attempts;
            if (Done()) break;
            for (const auto word : Output) Require(word == Untouched || word == Stored, "a skipped dispatch stored a partial result");
            Require(async, "a dispatch without APS5_ASYNC_DRAW_COMPILE was skipped");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
        Require(Done(), "the dispatch never stored after its shader was prepared and specialized in the background");
        Require(async || attempts == 1, "a dispatch without APS5_ASYNC_DRAW_COMPILE needed more than one attempt");
        AgcDriverShutdown_nid_postfix();
        std::printf("async draw compile tests passed (%s, %zu dispatches)\n", async ? "enabled" : "disabled", attempts);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
