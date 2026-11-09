#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 7> Skipped{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> Kept{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Stored = 0x12345678u;
alignas(256) std::array<std::uint32_t, 64> SkippedOutput{};
alignas(256) std::array<std::uint32_t, 64> KeptOutput{};

void Dispatch(const std::array<std::uint32_t, 7>& code, std::array<std::uint32_t, 64>& output) {
    const auto address = reinterpret_cast<std::uintptr_t>(output.data());
    const std::array<std::uint32_t, 4> data{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(output.size()), 0x00014204u};
    const auto program = reinterpret_cast<std::uintptr_t>(code.data());
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
    AgcDriver::GuestMemory::FlushGpuWrites(address, sizeof(output));
}

}

int main() {
    try {
        char value[32];
        std::snprintf(value, sizeof(value), "%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(Skipped.data())));
#ifdef _WIN32
        Require(_putenv_s("APS5_SKIP_SHADER", value) == 0, "cannot set APS5_SKIP_SHADER");
#else
        Require(setenv("APS5_SKIP_SHADER", value, 1) == 0, "cannot set APS5_SKIP_SHADER");
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        SkippedOutput.fill(Untouched);
        KeptOutput.fill(Untouched);
        Dispatch(Skipped, SkippedOutput);
        Dispatch(Kept, KeptOutput);
        for (std::size_t i = 0; i < SkippedOutput.size(); ++i) {
            Require(SkippedOutput[i] == Untouched, "the dispatch of the skipped shader stored dword " + std::to_string(i));
            Require(KeptOutput[i] == Stored, "the dispatch of another shader did not store dword " + std::to_string(i));
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("skip shader tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
