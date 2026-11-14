#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Untouched = 0xcafebabeu;

alignas(256) constexpr std::array<std::uint32_t, 64> Caller{
    0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
    0x80948114u, 0xbf078014u, 0xbf85fff7u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 2> AddFive{0x4a020285u, 0xbe80200eu};
alignas(256) constexpr std::array<std::uint32_t, 2> AddSixtyFour{0x4a0202c0u, 0xbe80200eu};
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

void Register(bool wave32) {
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    };
    alignas(8) static Header header{};
    const auto address = reinterpret_cast<std::uintptr_t>(Caller.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = sizeof(Caller);
    header.shader.code = Caller.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = wave32 ? 0x8000u : 0u;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, Threads}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 8u << 1u}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

const void* LongCallee() {
    constexpr std::size_t page = 4096u;
    constexpr std::size_t words = 20000u;
    constexpr std::size_t committed = (words * 4u + page - 1u) / page * page;
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, committed + page, MEM_RESERVE, PAGE_NOACCESS));
    Require(block != nullptr && VirtualAlloc(block, committed, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot reserve the long callee");
#else
    void* mapped = mmap(nullptr, committed + page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED && mprotect(mapped, committed, PROT_READ | PROT_WRITE) == 0, "cannot reserve the long callee");
    auto* block = static_cast<std::uint32_t*>(mapped);
#endif
    block[0] = 0x4a020281u;
    for (std::size_t word = 1; word + 1u < words; ++word) block[word] = 0xbf800000u;
    block[words - 1u] = 0xbe80200eu;
    for (std::size_t word = words; word < committed / 4u; ++word) block[word] = 0xbf810000u;
    return block;
}

template <typename TAction>
void ExpectFailure(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, std::string("unexpected failure: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected a failure containing ") + expected);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "--registered"), "usage: [--registered]");
        const bool registered = argc == 2;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (registered) Register(true);
        const auto* longCallee = LongCallee();
        for (const bool wave32 : registered ? std::vector<bool>{true} : std::vector<bool>{false, true}) {
            Check(AddFive.data(), AddSixtyFour.data(), wave32, 3u * (5u + 64u), "two user-data callees called from a loop");
            Check(AddOne.data(), AddSixtyFour.data(), wave32, 3u * (1u + 64u), "the first callee replaced");
            Check(AddSixtyFour.data(), AddSixtyFour.data(), wave32, 3u * (64u + 64u), "one callee behind both pointers");
            Check(longCallee, AddSixtyFour.data(), wave32, 3u * (1u + 64u), "a callee of 20,000 dwords whose last page is followed by unreadable memory");
        }
        const auto* unmapped = reinterpret_cast<const void*>(std::uintptr_t{0x1000u});
        ExpectFailure([&] { Dispatch(AddFive.data(), unmapped, registered); }, "is not readable");
        ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "is not readable");
        std::puts("compute user-data call tests passed");
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
