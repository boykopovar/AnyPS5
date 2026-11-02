#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 6> Code{0xd7460000u, 0x04010c04u, 0x7e020280u, 0xe0102000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
alignas(256) std::array<std::uint32_t, 64> Output{};
alignas(256) std::array<std::uint32_t, 4> Predicate{};

void Registers(std::vector<std::uint32_t>& words, std::uint32_t first, std::span<const std::uint32_t> values) {
    words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
    words.push_back(first);
    words.insert(words.end(), values.begin(), values.end());
}

void SetPredication(std::vector<std::uint32_t>& words, const void* address, bool executeWhenSet) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(address);
    words.insert(words.end(), {0xc0022000u, (4u << 16u) | (executeWhenSet ? 0x100u : 0u), static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u)});
}

void ClearPredication(std::vector<std::uint32_t>& words) {
    words.insert(words.end(), {0xc0022000u, 0, 0, 0});
}

void WritePredicate(std::vector<std::uint32_t>& words, std::uint32_t value) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(Predicate.data());
    words.insert(words.end(), {0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), value});
}

void PredicatedFill(std::vector<std::uint32_t>& words) {
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    Registers(words, 0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    Registers(words, 0x20c, program);
    const std::array<std::uint32_t, 1> resource{(4u << 1u) | (1u << 7u)};
    Registers(words, 0x213, resource);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Output.size()), 0x00014204u};
    Registers(words, 0x240, descriptor);
    words.insert(words.end(), {0xc0031501u, 1, 1, 1, 0x41u});
}

void SubmitBehindCommits(std::vector<std::uint32_t>& words) {
    std::unique_lock gpuLock(AgcDriver::GuestMemory::GpuMutex());
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void Run(std::uint32_t before, std::uint32_t written, bool expectFill) {
    Output.fill(Untouched);
    Predicate = {before, 0, 0, 0};
    std::vector<std::uint32_t> words;
    SetPredication(words, Predicate.data(), true);
    WritePredicate(words, written);
    PredicatedFill(words);
    ClearPredication(words);
    SubmitBehindCommits(words);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    Require(Predicate[0] == written, "the WRITE_DATA to the predicate did not land");
    for (std::size_t i = 0; i < Output.size(); ++i) {
        const auto expected = expectFill ? 0u : Untouched;
        Require(Output[i] == expected, std::string("a predicated dispatch read a predicate that a queued WRITE_DATA had not yet stored (predicate ") + std::to_string(before) + " -> " + std::to_string(written) + ", word " + std::to_string(i) + ")");
    }
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Run(0, 1, true);
        Run(1, 0, false);
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined predication read tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
