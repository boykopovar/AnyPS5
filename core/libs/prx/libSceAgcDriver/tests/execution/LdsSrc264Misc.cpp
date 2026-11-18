#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 10;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 134> Code{
    0x34020085, 0x3404008a, 0x3406008a, 0x4a040cff, 0x00000000, 0xe0301008, 0x80000601, 0xe030100c,
    0x80000701, 0xe0301010, 0x80000801, 0xe0301014, 0x80000901, 0xbf8c3f70, 0xd9340000, 0x00070602,
    0xd9340008, 0x00090802, 0xbf8cc07f, 0xdb280002, 0x00000002, 0xbf8cc07f, 0xd9d80000, 0x0a000002,
    0xbf8cc07f, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0x4a040cff, 0x00000000, 0xe0301008,
    0x80000601, 0xe030100c, 0x80000701, 0xe0301010, 0x80000801, 0xe0301014, 0x80000901, 0xbf8c3f70,
    0xd9340000, 0x00070602, 0xd9340008, 0x00090802, 0xbf8cc07f, 0xdb2c0002, 0x00000002, 0xbf8cc07f,
    0xd9d80000, 0x0a000002, 0xbf8cc07f, 0xe0701008, 0x80010a03, 0xe070100c, 0x80010b03, 0x4a040cff,
    0x00000000, 0xe0301008, 0x80000601, 0xe030100c, 0x80000701, 0xe0301010, 0x80000801, 0xe0301014,
    0x80000901, 0xbf8c3f70, 0xd9340000, 0x00070602, 0xd9340008, 0x00090802, 0xbf8cc07f, 0xdb340002,
    0x00000002, 0xbf8cc07f, 0xd9d80000, 0x0a000002, 0xbf8cc07f, 0xe0701010, 0x80010a03, 0xe0701014,
    0x80010b03, 0x4a040cff, 0x00000000, 0xe0301008, 0x80000601, 0xe030100c, 0x80000701, 0xe0301010,
    0x80000801, 0xe0301014, 0x80000901, 0xbf8c3f70, 0xd9340000, 0x00070602, 0xd9340008, 0x00090802,
    0xbf8cc07f, 0xdb480002, 0x00000002, 0xbf8cc07f, 0xd9d80000, 0x0a000002, 0xbf8cc07f, 0xe0701018,
    0x80010a03, 0xe070101c, 0x80010b03, 0x4a040cff, 0x00000000, 0xe0301008, 0x80000601, 0xe030100c,
    0x80000701, 0xe0301010, 0x80000801, 0xe0301014, 0x80000901, 0xbf8c3f70, 0xd9340000, 0x00070602,
    0xd9340008, 0x00090802, 0xbf8cc07f, 0xdb4c0002, 0x00000002, 0xbf8cc07f, 0xd9d80000, 0x0a000002,
    0xbf8cc07f, 0xe0701020, 0x80010a03, 0xe0701024, 0x80010b03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> AddressOffsetCode{0xdb288000u, 0x00000000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 3> GdsCode{0xdb2a0002u, 0x00000000u, 0xbf810000u};

std::uint64_t Word(const std::uint32_t* words) {
    return words[0] | (static_cast<std::uint64_t>(words[1]) << 32u);
}

void SetWord(std::uint32_t* words, std::uint64_t value) {
    words[0] = static_cast<std::uint32_t>(value);
    words[1] = static_cast<std::uint32_t>(value >> 32u);
}

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = Descriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = Descriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto available = device.Target().supportedCapabilities;
    std::vector<std::uint32_t> capabilities(available.begin(), available.end());
    if (!TargetHasCapability(device.Target(), spv::CapabilityInt64Atomics)) capabilities.push_back(spv::CapabilityInt64Atomics);
    request.target.supportedCapabilities = capabilities;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const double fa = static_cast<double>(static_cast<std::int32_t>(tid % 9u) - 4) * 1.75 + 0.5;
        const double fb = static_cast<double>(static_cast<std::int32_t>((tid * 5u) % 11u) - 5) * 0.625 + 0.25;
        SetWord(&Input[tid * Inputs], std::bit_cast<std::uint64_t>(fa));
        SetWord(&Input[tid * Inputs + 2u], std::bit_cast<std::uint64_t>(fb));
    }
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, Code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, 5> names{"or", "xor", "write", "min_f64", "max_f64"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t a = Word(&Input[tid * Inputs]);
        const std::uint64_t b = Word(&Input[tid * Inputs + 2u]);
        const double fa = std::bit_cast<double>(a);
        const double fb = std::bit_cast<double>(b);
        const std::array<std::uint64_t, 5> expected{
            a | b, a ^ b, b,
            std::bit_cast<std::uint64_t>(std::min(fa, fb)),
            std::bit_cast<std::uint64_t>(std::max(fa, fb)),
        };
        for (std::uint32_t op = 0; op < expected.size(); ++op) {
            const std::uint64_t actual = Word(&Output[tid * Results + op * 2u]);
            Require(actual == expected[op], "lds src2 64: lane " + std::to_string(tid) + " " + names[op] + " is " + Hex(actual) + ", expected " + Hex(expected[op]));
        }
    }
}

void CheckRejected(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const char* what) {
    bool threw = false;
    try {
        (void)Compile(device, code);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, std::string("lds src2 64: ") + what + " was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        CheckLanes();
        CheckRejected(*device, AddressOffsetCode, "an offset taken from the address");
        CheckRejected(*device, GdsCode, "a GDS src2 operation");
        std::puts("lds src2 64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
