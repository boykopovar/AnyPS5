#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 8> Code{
    0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e020280u, 0xe0702000u, 0x80020100u, 0xbf810000u, 0u,
};
constexpr std::uint32_t Untouched = 0xcafebabeu;
alignas(256) std::array<std::uint32_t, 64> Output{};
alignas(16) std::array<std::uint32_t, 4> Valid{};
alignas(16) std::array<std::uint32_t, 4> Unsupported{};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Output.size()), word3};
}

std::array<std::uint32_t, 2> Pointer(const void* descriptor) {
    const auto address = reinterpret_cast<std::uintptr_t>(descriptor);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
}

void Dispatch(const std::array<std::uint32_t, 2>& userData) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "shader request " + path.string() + " was not written");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string Replay(const std::string& text) {
    const auto replay = ShaderRecompiler::RequestSerializer{}.Deserialize(text);
    try {
        static_cast<void>(ShaderRecompiler::Recompile(replay.request));
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

std::vector<std::uint32_t> UserData(const std::string& text) {
    const auto replay = ShaderRecompiler::RequestSerializer{}.Deserialize(text);
    return {replay.request.context.userData.begin(), replay.request.context.userData.end()};
}

void Run() {
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "shader_%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(Code.data())));
    const std::string first = std::string(prefix) + ".req";
    const std::string failed = std::string(prefix) + "_failed.req";
    Valid = BufferDescriptor(0x00014204u);
    Unsupported = BufferDescriptor(0xa1b00facu);
    Output.fill(Untouched);
    const auto valid = Pointer(Valid.data());
    Dispatch(valid);
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    for (const auto word : Output) Require(word == 0u, "the dispatch through the valid descriptor stored nothing");
    const auto unsupported = Pointer(Unsupported.data());
    std::string message;
    try {
        Dispatch(unsupported);
    } catch (const std::exception& error) {
        message = error.what();
    }
    Require(message.find("unsupported type") != std::string::npos, "the dispatch through a type 2 descriptor failed for another reason: " + message);
    Require(message.find("(" + failed + ")") != std::string::npos, "the failure does not name the failing dispatch's request: " + message);
    const auto firstText = Read(first);
    const auto failedText = Read(failed);
    Require(UserData(firstText) == std::vector<std::uint32_t>(valid.begin(), valid.end()), "the first dispatch's request lost its user data");
    Require(UserData(failedText) == std::vector<std::uint32_t>(unsupported.begin(), unsupported.end()), "the failed request holds another dispatch's user data");
    Require(Replay(firstText).empty(), "the first dispatch's request does not replay: " + Replay(firstText));
    const auto replayed = Replay(failedText);
    Require(replayed.find("unsupported type") != std::string::npos, "the failed request does not replay its failure: " + replayed);
}

}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("agc_failed_capture_dump-" + std::to_string(std::random_device{}()));
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        std::filesystem::current_path(directory);
#ifdef _WIN32
        Require(_putenv_s("APS5_DUMP_SHADERS", "1") == 0, "cannot enable shader capture");
#else
        Require(setenv("APS5_DUMP_SHADERS", "1", 1) == 0, "cannot enable shader capture");
#endif
        Run();
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        std::filesystem::current_path(directory.parent_path());
        std::filesystem::remove_all(directory);
        std::puts("failed capture dump tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
