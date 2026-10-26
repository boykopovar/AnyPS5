#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 7> First{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> Second{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Stored = 0x12345678u;
alignas(256) std::array<std::uint32_t, 64> FirstOutput{};
alignas(256) std::array<std::uint32_t, 64> SecondOutput{};

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

std::string Hex(const void* address) {
    char text[32];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address)));
    return text;
}

void List(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream(path, std::ios::trunc) << contents;
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
}

void Run(const char* phase, bool firstSkipped, bool secondSkipped) {
    FirstOutput.fill(Untouched);
    SecondOutput.fill(Untouched);
    Dispatch(First, FirstOutput);
    Dispatch(Second, SecondOutput);
    for (std::size_t i = 0; i < FirstOutput.size(); ++i) {
        Require(FirstOutput[i] == (firstSkipped ? Untouched : Stored), std::string(phase) + ": the first shader's dispatch " + (firstSkipped ? "stored" : "did not store") + " dword " + std::to_string(i));
        Require(SecondOutput[i] == (secondSkipped ? Untouched : Stored), std::string(phase) + ": the second shader's dispatch " + (secondSkipped ? "stored" : "did not store") + " dword " + std::to_string(i));
    }
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("aps5-skip-shader-file-" + Hex(&FirstOutput) + ".txt");
    try {
        List(path, "# listed\n" + Hex(First.data()) + "\n");
#ifdef _WIN32
        Require(_putenv_s("APS5_SKIP_SHADER_FILE", path.string().c_str()) == 0, "cannot set APS5_SKIP_SHADER_FILE");
#else
        Require(setenv("APS5_SKIP_SHADER_FILE", path.string().c_str(), 1) == 0, "cannot set APS5_SKIP_SHADER_FILE");
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) {
                std::filesystem::remove(path);
                return VulkanTestSkipped;
            }
        }
        Run("first listed", true, false);
        List(path, Hex(Second.data()) + " # the first is not listed any more\n");
        Run("second listed", false, true);
        List(path, Hex(First.data()) + "zz 1" + std::string(16, '0') + " 0x" + Hex(Second.data()) + "\n");
        Run("malformed and overflowing tokens", false, true);
        List(path, "");
        Run("none listed", false, false);
        AgcDriverShutdown_nid_postfix();
        std::filesystem::remove(path);
        std::puts("skip shader file tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return 1;
    }
}
