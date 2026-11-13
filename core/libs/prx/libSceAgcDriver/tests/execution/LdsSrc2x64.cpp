#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 32;
constexpr std::uint32_t Operations = 15;
constexpr std::uint32_t RegionBytes = Threads * 8u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::array<std::uint32_t, Operations> Opcodes{0xc0u, 0xc1u, 0xc2u, 0xc3u, 0xc4u, 0xc5u, 0xc6u, 0xc7u, 0xc8u, 0xc9u, 0xcau, 0xcbu, 0xcdu, 0xd2u, 0xd3u};

constexpr std::uint32_t DsWord0(std::uint32_t opcode, std::uint32_t offset) {
    return 0xd8000000u | (opcode << 18u) | (((offset >> 8u) & 0x7fu) << 8u) | (offset & 0xffu);
}

constexpr std::uint32_t DsWord1(std::uint32_t address, std::uint32_t data, std::uint32_t destination) {
    return address | (data << 8u) | (destination << 24u);
}

constexpr auto BuildCode() {
    std::array<std::uint32_t, 5 + Operations * 12 + 19> code{};
    std::size_t at = 0;
    for (const std::uint32_t word : {0x34020084u, 0xe0381000u, 0x80000201u, 0x340c0083u, 0xbf8c3f70u}) code[at++] = word;
    for (std::uint32_t op = 0; op < Operations; ++op) {
        const bool sourceBelow = op % 2u == 0u;
        const std::uint32_t target = RegionBytes * (2u * op + (sourceBelow ? 1u : 0u));
        const std::uint32_t source = RegionBytes * (2u * op + (sourceBelow ? 0u : 1u));
        const std::uint32_t dwords = sourceBelow ? 0x8000u - (RegionBytes / 4u - 1u) : RegionBytes / 4u + 1u;
        for (const std::uint32_t word : {0x4a0e0cffu, target, 0x4a100cffu, source,
                 DsWord0(0x4du, 0u), DsWord1(7u, 2u, 0u), DsWord0(0x4du, 0u), DsWord1(8u, 4u, 0u),
                 DsWord0(Opcodes[op], dwords & 0x7fffu), DsWord1(7u, 0u, 0u),
                 DsWord0(0x76u, 0u), DsWord1(7u, 0u, 10u + 2u * op)}) {
            code[at++] = word;
        }
    }
    code[at++] = 0x34020087u;
    code[at++] = 0xbf8cc07fu;
    for (std::uint32_t store = 0; store < 8u; ++store) {
        code[at++] = 0xe0781000u | (store * 16u);
        code[at++] = 0x80010001u | ((10u + 4u * store) << 8u);
    }
    code[at++] = 0xbf810000u;
    return code;
}

alignas(256) constexpr auto Code = BuildCode();

alignas(256) constexpr std::array<std::uint32_t, 3> AddressOffsetCode{0xdb008000u, 0x00000000u, 0xbf810000u};

std::uint64_t ValueA(std::uint32_t tid) {
    if (tid % 8u == 0u) return 0u;
    return std::bit_cast<std::uint64_t>(static_cast<double>(static_cast<std::int32_t>(tid % 9u) - 4) * 1.75 + 0.5);
}

std::uint64_t ValueB(std::uint32_t tid) {
    if (tid % 4u == 1u) return ValueA(tid);
    return std::bit_cast<std::uint64_t>(static_cast<double>(static_cast<std::int32_t>((tid * 5u) % 11u) - 5) * 0.625 + 0.25);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
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

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t a = ValueA(tid);
        const std::uint64_t b = ValueB(tid);
        Input[tid * Inputs] = static_cast<std::uint32_t>(a);
        Input[tid * Inputs + 1] = static_cast<std::uint32_t>(a >> 32u);
        Input[tid * Inputs + 2] = static_cast<std::uint32_t>(b);
        Input[tid * Inputs + 3] = static_cast<std::uint32_t>(b >> 32u);
    }
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, Code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, Operations> names{
        "ds_add_src2_u64", "ds_sub_src2_u64", "ds_rsub_src2_u64", "ds_inc_src2_u64", "ds_dec_src2_u64", "ds_min_src2_i64", "ds_max_src2_i64", "ds_min_src2_u64",
        "ds_max_src2_u64", "ds_and_src2_b64", "ds_or_src2_b64", "ds_xor_src2_b64", "ds_write_src2_b64", "ds_min_src2_f64", "ds_max_src2_f64",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t a = ValueA(tid);
        const std::uint64_t b = ValueB(tid);
        const auto sa = static_cast<std::int64_t>(a);
        const auto sb = static_cast<std::int64_t>(b);
        const auto fa = std::bit_cast<double>(a);
        const auto fb = std::bit_cast<double>(b);
        const std::array<std::uint64_t, Operations> expected{
            a + b, a - b, b - a, a >= b ? 0u : a + 1u, (a == 0u || a > b) ? b : a - 1u,
            static_cast<std::uint64_t>(std::min(sa, sb)), static_cast<std::uint64_t>(std::max(sa, sb)), std::min(a, b), std::max(a, b),
            a & b, a | b, a ^ b, b, std::bit_cast<std::uint64_t>(std::min(fa, fb)), std::bit_cast<std::uint64_t>(std::max(fa, fb)),
        };
        for (std::uint32_t op = 0; op < Operations; ++op) {
            const std::uint64_t actual = Output[tid * Results + 2u * op] | (static_cast<std::uint64_t>(Output[tid * Results + 2u * op + 1u]) << 32u);
            Require(actual == expected[op], "lds src2 64: lane " + std::to_string(tid) + " " + names[op] + " is " + Hex(actual) + ", expected " + Hex(expected[op]));
        }
    }
}

void CheckAddressOffsetRejected(AgcDriver::VulkanDevice& device) {
    try {
        (void)Compile(device, AddressOffsetCode);
    } catch (const std::exception&) {
        return;
    }
    Require(false, "lds src2 64: an offset taken from the address was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        CheckLanes();
        CheckAddressOffsetRejected(*device);
        std::puts("lds src2 64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
