#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
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

alignas(256) constexpr std::array<std::uint32_t, 7> Refused{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> Kept{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t StoredFormat = 0x00014204u;
constexpr std::uint32_t UnsupportedFormat = 0x61b00facu;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Stored = 0x12345678u;
alignas(256) std::array<std::uint32_t, 64> RefusedOutput{};
alignas(256) std::array<std::uint32_t, 64> KeptOutput{};

void Dispatch(const std::array<std::uint32_t, 7>& code, std::array<std::uint32_t, 64>& output, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(output.data());
    const std::array<std::uint32_t, 4> data{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(output.size()), format};
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

int main(int argc, char** argv) {
    try {
        const bool maxWords = argc == 2 && std::string(argv[1]) == "max-words";
#ifdef _WIN32
        Require(_putenv_s("APS5_SKIP_REFUSED_DISPATCHES", "1") == 0, "cannot set APS5_SKIP_REFUSED_DISPATCHES");
        if (maxWords) Require(_putenv_s("APS5_MAX_COMPUTE_SPIRV_WORDS", "1") == 0, "cannot set APS5_MAX_COMPUTE_SPIRV_WORDS");
#else
        Require(setenv("APS5_SKIP_REFUSED_DISPATCHES", "1", 1) == 0, "cannot set APS5_SKIP_REFUSED_DISPATCHES");
        if (maxWords) Require(setenv("APS5_MAX_COMPUTE_SPIRV_WORDS", "1", 1) == 0, "cannot set APS5_MAX_COMPUTE_SPIRV_WORDS");
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (maxWords) {
            KeptOutput.fill(Untouched);
            Dispatch(Kept, KeptOutput, StoredFormat);
            Require(AgcDriver::DriverDetail::RefusedDispatch(reinterpret_cast<std::uintptr_t>(Kept.data())), "a compute shader above APS5_MAX_COMPUTE_SPIRV_WORDS was not refused");
            for (std::size_t i = 0; i < KeptOutput.size(); ++i) Require(KeptOutput[i] == Untouched, "the dispatch of a shader above APS5_MAX_COMPUTE_SPIRV_WORDS stored dword " + std::to_string(i));
            AgcDriverShutdown_nid_postfix();
            std::puts("max compute SPIR-V words tests passed");
            return 0;
        }
        RefusedOutput.fill(Untouched);
        KeptOutput.fill(Untouched);
        Dispatch(Refused, RefusedOutput, UnsupportedFormat);
        Dispatch(Refused, RefusedOutput, UnsupportedFormat);
        Dispatch(Kept, KeptOutput, StoredFormat);
        Require(AgcDriver::DriverDetail::RefusedDispatch(reinterpret_cast<std::uintptr_t>(Refused.data())), "the refused shader is not remembered");
        Require(!AgcDriver::DriverDetail::RefusedDispatch(reinterpret_cast<std::uintptr_t>(Kept.data())), "the kept shader is remembered as refused");
        Require(AgcDriver::DriverDetail::RefusedShaderCount() == std::optional<std::size_t>(1), "the refused shader is not counted once");
        for (std::size_t i = 0; i < RefusedOutput.size(); ++i) {
            Require(RefusedOutput[i] == Untouched, "the dispatch of the refused shader stored dword " + std::to_string(i));
            Require(KeptOutput[i] == Stored, "the dispatch of another shader did not store dword " + std::to_string(i));
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("skip refused dispatches tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
