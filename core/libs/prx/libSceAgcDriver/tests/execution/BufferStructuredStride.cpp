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
constexpr std::uint8_t Fill = 0xa5u;
constexpr std::uint32_t Guard = 0xdeadbeefu;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t StructuredFormat = 0x01016facu;
constexpr std::uint32_t Ubyte = 8u;
constexpr std::uint32_t Dword = 12u;
constexpr std::uint32_t StoreByte = 24u;
constexpr std::uint32_t StoreShort = 26u;
alignas(256) std::array<std::uint8_t, 256> Source{};
alignas(256) std::array<std::uint8_t, 256> Target{};
alignas(256) std::array<std::uint32_t, Threads + 8u> Output{};
alignas(256) std::array<std::uint32_t, 16> Code{};
std::uint32_t Failures = 0u;

struct Case {
    std::uint32_t opcode;
    std::uint32_t size;
    bool store;
    bool sign;
    std::uint32_t mode;
    std::uint32_t stride;
    std::uint32_t records;
    std::uint32_t soffset;
    std::uint32_t immediate;
};

Case Load(std::uint32_t opcode, std::uint32_t mode, std::uint32_t stride, std::uint32_t records, std::uint32_t soffset = 0u, std::uint32_t immediate = 0u) {
    return {opcode, opcode == Dword ? 4u : 1u, false, false, mode, stride, records, soffset, immediate};
}

Case Store(std::uint32_t opcode, std::uint32_t mode, std::uint32_t stride, std::uint32_t records, std::uint32_t soffset, std::uint32_t immediate) {
    return {opcode, opcode == StoreShort ? 2u : 1u, true, false, mode, stride, records, soffset, immediate};
}

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t records, std::uint32_t stride, std::uint32_t mode) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    const auto high = static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u);
    return {static_cast<std::uint32_t>(address), high, records, mode == 3u ? RawFormat : StructuredFormat};
}

bool InRange(const Case& test, std::uint32_t offset) {
    if (test.mode == 0u) return offset + test.size <= test.stride;
    return test.soffset <= test.records && offset + test.size <= test.records - test.soffset;
}

std::uint32_t Shift(const Case& test) {
    return test.size == 1u ? 0u : test.size == 2u ? 1u : 2u;
}

std::uint32_t LaneOffset(const Case& test, std::uint32_t lane) {
    return (test.store || test.size == 4u ? lane << Shift(test) : lane) + test.immediate;
}

std::vector<std::uint32_t> Program(const Case& test) {
    const bool structured = test.mode == 0u;
    const std::uint32_t offsetRegister = test.store || test.size == 4u ? 1u : 0u;
    std::vector<std::uint32_t> code;
    if (test.store) code.insert(code.end(), {0x34020080u + Shift(test), 0x4a0400ffu, 0x00ab0100u});
    else code.push_back(0x34020082u);
    if (structured) code.insert(code.end(), {0x7e080280u, 0x7e0a0300u + offsetRegister});
    if (test.soffset != 0u) code.push_back(0xbe8c0380u + test.soffset);
    code.push_back(0xe0001000u | (structured ? 0x2000u : 0u) | (test.opcode << 18u) | test.immediate);
    code.push_back((structured ? 4u : offsetRegister) | (2u << 8u) | ((test.soffset != 0u ? 0x0cu : 0x80u) << 24u));
    if (!test.store) code.insert(code.end(), {0xbf8c3f70u, 0xe0701000u, 0x80010201u});
    code.push_back(0xbf810000u);
    return code;
}

std::uint32_t Expected(const Case& test, std::uint32_t offset) {
    if (!InRange(test, offset)) return 0u;
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0u; byte < test.size; ++byte) value |= static_cast<std::uint32_t>(Source[test.soffset + offset + byte]) << (8u * byte);
    const auto bits = test.size * 8u;
    if (test.sign && ((value >> (bits - 1u)) & 1u) != 0u) value |= ~0u << bits;
    return value;
}

void Dispatch(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    const auto program = Program(test);
    Require(program.size() <= Code.size(), "structured buffer stride program is too long");
    Code.fill(0u);
    std::copy(program.begin(), program.end(), Code.begin());
    std::array<std::uint32_t, 8> userData{};
    const void* memoryUnderTest = test.store ? static_cast<const void*>(Target.data()) : static_cast<const void*>(Source.data());
    const auto tested = Descriptor(memoryUnderTest, test.records, test.stride, test.mode);
    const auto output = Descriptor(Output.data(), Threads * 4u, 0u, 3u);
    std::copy(tested.begin(), tested.end(), userData.begin());
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

void Report(const Case& test, std::uint32_t wave, std::uint32_t position, std::uint32_t actual, std::uint32_t expected) {
    ++Failures;
    std::fprintf(stderr, "wave=%u store=%d opcode=%u mode=%u stride=%u records=%u soffset=%u immediate=%u at=%u actual=%08x expected=%08x\n",
                 wave, test.store, test.opcode, test.mode, test.stride, test.records, test.soffset, test.immediate, position, actual, expected);
}

void VerifyLoad(const Case& test, std::uint32_t wave) {
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        const auto expected = Expected(test, LaneOffset(test, lane));
        if (Output[lane] != expected) Report(test, wave, lane, Output[lane], expected);
    }
}

void VerifyStore(const Case& test, std::uint32_t wave) {
    std::array<std::uint8_t, 256> expected{};
    expected.fill(Fill);
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        const auto offset = LaneOffset(test, lane);
        if (!InRange(test, offset)) continue;
        const std::uint32_t value = 0x00ab0100u + lane;
        for (std::uint32_t byte = 0u; byte < test.size; ++byte) expected[test.soffset + offset + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
    }
    for (std::uint32_t index = 0u; index < Target.size(); ++index) {
        if (Target[index] != expected[index]) Report(test, wave, index, Target[index], expected[index]);
    }
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    for (std::uint32_t index = 0u; index < Source.size(); ++index) Source[index] = static_cast<std::uint8_t>(0x81u + index * 7u);
    Target.fill(Fill);
    Output.fill(Guard);
    Dispatch(device, test, wave);
    if (test.store) VerifyStore(test, wave);
    else VerifyLoad(test, wave);
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    for (const auto stride : {6u, 10u, 16u}) {
        cases.push_back(Load(Ubyte, 0u, stride, 4u));
        cases.push_back(Load(Dword, 0u, stride, 4u));
        for (const auto opcode : {StoreByte, StoreShort}) {
            for (const auto immediate : {0u, 1u}) cases.push_back(Store(opcode, 0u, stride, 4u, 0u, immediate));
        }
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
        Require(Failures == 0u, "structured buffer accesses differ from the RDNA2 stride rule");
        std::puts("structured buffer accesses stay inside their stride like RDNA2, wave32/wave64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
