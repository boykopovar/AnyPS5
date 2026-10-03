#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Stride = 4;
constexpr std::uint32_t Wave32 = 0x8000;
alignas(256) std::array<std::uint32_t, Threads * Stride> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};
alignas(256) std::array<std::uint32_t, 64> Label{};

alignas(256) constexpr std::array<std::uint32_t, 23> FirstCode{
    0x34020082, 0xbf930000, 0xbf940001, 0xe0302000, 0x80000401, 0xf4840000, 0x00000000, 0xf4800000,
    0x00000000, 0xf47c0000, 0x00000000, 0xe1c80000, 0x00000000, 0xe1c40000, 0x00000000, 0xbfa20000,
    0xbf8c3f70, 0xbfa80001, 0x4a080881, 0xbf950001, 0xe0702000, 0x80010401, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 24> SecondCode{
    0x34020082, 0xbf930000, 0xbf940001, 0xe0302000, 0x80000401, 0xf4840000, 0x00000000, 0xf4800000,
    0x00000000, 0xf47c0000, 0x00000000, 0xe1c80000, 0x00000000, 0xe1c40000, 0x00000000, 0xbfa20000,
    0xbf8c3f70, 0xbfa80001, 0x4a080881, 0xbf950001, 0xe0702000, 0x80010401, 0xbf800000, 0xbf810000,
};

const std::array<ShaderRegister, 4> Registers{{{0x207, Threads}, {0x208, 1}, {0x209, 1}, {0x213, 8u << 1u}}};
ShaderSpecialRegs Specials{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

template <std::size_t TWords>
void Register(const std::array<std::uint32_t, TWords>& code) {
    Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Shader);
    shader.shader_size = sizeof(code);
    shader.code = code.data();
    shader.type = 0;
    shader.sh_registers = const_cast<ShaderRegister*>(Registers.data());
    shader.num_sh_registers = static_cast<std::uint8_t>(Registers.size());
    shader.specials = &Specials;
    shader.special_sizes_bytes = sizeof(ShaderSpecialRegs);
    AgcDriverRegisterShader_nid_postfix(&shader);
}

template <std::size_t TWords>
void Dispatch(const std::array<std::uint32_t, TWords>& code, std::uint32_t modifier, const char* name) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * Stride] = tid * 0x01010101u + 7u;
    Output.fill(0xdeadbeefu);
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::vector<std::uint32_t> commands{
        0xc0027600, 0x20c, static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u),
        0xc0037600, 0x207, Threads, 1, 1,
        0xc0017600, 0x213, 8u << 1u,
        0xc0087600, 0x240,
    };
    commands.insert(commands.end(), input.begin(), input.end());
    commands.insert(commands.end(), output.begin(), output.end());
    const auto label = reinterpret_cast<std::uintptr_t>(Label.data());
    Label[0] = 0;
    commands.insert(commands.end(), {0xc0031500, 1, 1, 1, modifier | 0x41u});
    commands.insert(commands.end(), {0xc0033700, 0x100, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), 1});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    AgcDriverWaitIdle_nid_postfix();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (reinterpret_cast<volatile std::uint32_t*>(Label.data())[0] != 1) {
        if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error(std::string(name) + ": the dispatch did not complete");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        if (Output[tid * Stride] != Input[tid * Stride] + 1u) throw std::runtime_error(std::string(name) + ": thread " + std::to_string(tid) + " stored " + std::to_string(Output[tid * Stride]) + ", expected " + std::to_string(Input[tid * Stride] + 1u));
    }
}

void WaitForFrontEnds(std::uint64_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (ShaderRecompiler::FrontEndBuilds() < count) {
        if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("the registered compute shader was not prepared");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Run() {
    Specials.dispatch_modifier = Wave32;
    Register(FirstCode);
    Dispatch(FirstCode, Wave32, "first shader");

    const auto beforeRegistration = ShaderRecompiler::FrontEndBuilds();
    Register(SecondCode);
    WaitForFrontEnds(beforeRegistration + 1);
    const auto prepared = ShaderRecompiler::FrontEndBuilds();
    if (prepared != beforeRegistration + 1) throw std::runtime_error("registration ran the front end more than once");
    Dispatch(SecondCode, Wave32, "prepared shader");
    if (ShaderRecompiler::FrontEndBuilds() != prepared) throw std::runtime_error("the dispatch of a prepared shader ran the front end again");

    Dispatch(SecondCode, 0, "prepared shader at another wave size");
    if (ShaderRecompiler::FrontEndBuilds() != prepared + 1) throw std::runtime_error("a dispatch that differs from the prediction did not build its own front end");
}

}

int main() {
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        Run();
        LibcRunShutdown_nid_postfix();
        std::puts("shader preparation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (const std::exception& shutdown) { std::cerr << "shutdown: " << shutdown.what() << '\n'; }
        return 1;
    }
}
