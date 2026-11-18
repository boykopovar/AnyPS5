#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
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
constexpr std::uint32_t Results = 5;
constexpr std::uint32_t DwordsPerThread = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * DwordsPerThread> Output{};

void Mov(std::vector<std::uint32_t>& code, std::uint32_t vdst, std::uint32_t value) {
    code.push_back(0x4a000000u | (6u << 9u) | (vdst << 17u) | 0xffu);
    code.push_back(value);
}

void DsWrite64(std::vector<std::uint32_t>& code, std::uint32_t data1, std::uint32_t data0, std::uint32_t addr) {
    code.push_back(0xd8000000u | (0x4du << 18u));
    code.push_back((data1 << 16u) | (data0 << 8u) | addr);
}

void DsSrc2(std::vector<std::uint32_t>& code, std::uint32_t op, std::uint32_t offset, std::uint32_t addr) {
    code.push_back(0xd8000000u | (op << 18u) | offset);
    code.push_back(addr);
}

void DsRead64(std::vector<std::uint32_t>& code, std::uint32_t vdst, std::uint32_t addr) {
    code.push_back(0xd8000000u | (0x76u << 18u));
    code.push_back((vdst << 24u) | addr);
}

void Store4(std::vector<std::uint32_t>& code, std::uint32_t vdst, std::uint32_t offset) {
    code.push_back(0xe0781000u | offset);
    code.push_back(0x80010000u | (vdst << 8u) | 1u);
}

std::vector<std::uint32_t> OperationCode() {
    std::vector<std::uint32_t> code{0x34020084u, 0xe0381000u, 0x80000201u, 0x340c0082u, 0xbf8c3f70u};
    constexpr std::array<std::uint32_t, Results> ops{0xc0u, 0xc1u, 0xc2u, 0xc3u, 0xc4u};
    constexpr std::array<std::uint32_t, Results> addressesA{0x1000u, 0x1080u, 0x1100u, 0x1180u, 0x1200u};
    constexpr std::array<std::uint32_t, Results> addressesB{0x0000u, 0x2080u, 0x0100u, 0x2180u, 0x0200u};
    constexpr std::array<std::uint32_t, Results> offsets{0x7c00u, 0x0400u, 0x7c00u, 0x0400u, 0x7c00u};
    for (std::uint32_t index = 0u; index < Results; ++index) {
        Mov(code, 7u, addressesA[index]);
        Mov(code, 8u, addressesB[index]);
        DsWrite64(code, 3u, 2u, 7u);
        DsWrite64(code, 5u, 4u, 8u);
        DsSrc2(code, ops[index], offsets[index], 7u);
        DsRead64(code, 10u + index, 7u);
    }
    Store4(code, 10u, 0x00u);
    Store4(code, 14u, 0x10u);
    code.push_back(0xbf810000u);
    return code;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code) {
    for (std::uint32_t tid = 0u; tid < Threads; ++tid) {
        const std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
        const std::uint64_t a = seed ^ (seed >> 17u);
        const std::uint64_t b = (seed * 0xbf58476d1ce4e5b9ull) | 1ull;
        Input[tid * Inputs + 0u] = static_cast<std::uint32_t>(a);
        Input[tid * Inputs + 1u] = static_cast<std::uint32_t>(a >> 32u);
        Input[tid * Inputs + 2u] = static_cast<std::uint32_t>(b);
        Input[tid * Inputs + 3u] = static_cast<std::uint32_t>(b >> 32u);
    }
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, Results> names{"add", "sub", "rsub", "inc", "dec"};
    for (std::uint32_t tid = 0u; tid < Threads; ++tid) {
        const std::uint64_t a = static_cast<std::uint64_t>(Input[tid * Inputs + 1u]) << 32u | Input[tid * Inputs + 0u];
        const std::uint64_t b = static_cast<std::uint64_t>(Input[tid * Inputs + 3u]) << 32u | Input[tid * Inputs + 2u];
        const std::array<std::uint64_t, Results> expected{
            a + b,
            a - b,
            b - a,
            a >= b ? 0ull : a + 1ull,
            (a == 0ull || a > b) ? b : a - 1ull,
        };
        for (std::uint32_t op = 0u; op < Results; ++op) {
            const std::uint32_t base = tid * DwordsPerThread + op / 4u * 16u + op % 4u * 4u;
            const std::uint64_t actual = static_cast<std::uint64_t>(Output[base + 1u]) << 32u | Output[base];
            Require(actual == expected[op], "lds src2 u64: lane " + std::to_string(tid) + " " + names[op] + " is " + Hex(actual) + ", expected " + Hex(expected[op]));
        }
    }
}

void CheckRejected(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, const char* what) {
    try {
        (void)Compile(device, code);
    } catch (const std::exception&) {
        return;
    }
    Require(false, std::string("lds src2 u64: ") + what + " was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto code = OperationCode();
        Run(*device, code);
        CheckLanes();
        CheckRejected(*device, {0xdb008000u, 0x00000007u, 0xbf810000u}, "an offset taken from the address");
        CheckRejected(*device, {0xdb020400u, 0x00000007u, 0xbf810000u}, "a GDS operation");
        std::puts("lds src2 u64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
