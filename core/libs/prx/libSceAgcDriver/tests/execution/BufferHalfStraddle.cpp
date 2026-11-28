#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <vector>

namespace {
using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 64u;
constexpr std::uint32_t Guard = 0xdeadbeefu;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t Ushort = 10u;
constexpr std::uint32_t Sshort = 11u;
alignas(256) std::array<std::uint8_t, 256> Source{};
alignas(256) std::array<std::uint32_t, Threads + 8u> Output{};
alignas(256) std::array<std::uint32_t, 16> Code{};
std::uint32_t Failures = 0u;

struct Case {
    std::uint32_t opcode;
    std::uint32_t records;
    std::uint32_t soffset;
    std::uint32_t immediate;
};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    const auto high = static_cast<std::uint32_t>((address >> 32u) & 0xffffu);
    return {static_cast<std::uint32_t>(address), high, records, RawFormat};
}

std::vector<std::uint32_t> Program(const Case& test) {
    std::vector<std::uint32_t> code{0x34020082u};
    if (test.soffset != 0u) code.push_back(0xbe8c0380u + test.soffset);
    code.push_back(0xe0001000u | (test.opcode << 18u) | test.immediate);
    code.push_back((2u << 8u) | ((test.soffset != 0u ? 0x0cu : 0x80u) << 24u));
    code.insert(code.end(), {0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u});
    return code;
}

std::uint32_t Expected(const Case& test, std::uint32_t offset) {
    if (test.soffset > test.records || offset + 2u > test.records - test.soffset) return 0u;
    std::uint32_t value = Source[test.soffset + offset] | (static_cast<std::uint32_t>(Source[test.soffset + offset + 1u]) << 8u);
    if (test.opcode == Sshort && (value & 0x8000u) != 0u) value |= 0xffff0000u;
    return value;
}

void Dispatch(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    const auto program = Program(test);
    Code.fill(0u);
    std::copy(program.begin(), program.end(), Code.begin());
    std::array<std::uint32_t, 8> userData{};
    const auto input = Descriptor(Source.data(), test.records);
    const auto output = Descriptor(Output.data(), Threads * 4u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    const std::span<const std::uint32_t> words(Code.data(), program.size());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), words, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    for (std::uint32_t index = 0u; index < Source.size(); ++index) Source[index] = static_cast<std::uint8_t>(0x81u + index * 7u);
    Output.fill(Guard);
    Dispatch(device, test, wave);
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        const auto expected = Expected(test, lane + test.immediate);
        if (Output[lane] == expected) continue;
        ++Failures;
        std::fprintf(stderr, "wave=%u opcode=%u records=%u soffset=%u immediate=%u lane=%u actual=%08x expected=%08x\n", wave, test.opcode, test.records, test.soffset, test.immediate, lane, Output[lane], expected);
    }
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    for (const auto opcode : {Ushort, Sshort}) {
        cases.push_back({opcode, 64u, 0u, 0u});
        cases.push_back({opcode, 64u, 3u, 0u});
        cases.push_back({opcode, 64u, 0u, 3u});
        cases.push_back({opcode, 8u, 0u, 0u});
    }
    return cases;
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto cases = Cases();
        for (const auto wave : {32u, 64u}) {
            for (const auto& test : cases) Run(*device, test, wave);
        }
        Require(Failures == 0u, "16-bit buffer loads differ from RDNA2");
        std::puts("16-bit buffer loads join their bytes across dwords like RDNA2, wave32/wave64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
