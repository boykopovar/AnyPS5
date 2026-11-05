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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 171> Code{
    0x34020084, 0xe0381000, 0x80000201, 0x340c0082, 0xbf8c3f70, 0x4a0e0cff, 0x00001000, 0x4a100c80,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda007c00, 0x00000007, 0xd8d80000, 0x0a000007,
    0x4a0e0cff, 0x00001080, 0x4a100cff, 0x00002080, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda040400, 0x00000007, 0xd8d80000, 0x0b000007, 0x4a0e0cff, 0x00001100, 0x4a100cff, 0x00000100,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda087c00, 0x00000007, 0xd8d80000, 0x0c000007,
    0x4a0e0cff, 0x00001180, 0x4a100cff, 0x00002180, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda0c0400, 0x00000007, 0xd8d80000, 0x0d000007, 0x4a0e0cff, 0x00001200, 0x4a100cff, 0x00000200,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda107c00, 0x00000007, 0xd8d80000, 0x0e000007,
    0x4a0e0cff, 0x00001280, 0x4a100cff, 0x00002280, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda140400, 0x00000007, 0xd8d80000, 0x0f000007, 0x4a0e0cff, 0x00001300, 0x4a100cff, 0x00000300,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda187c00, 0x00000007, 0xd8d80000, 0x10000007,
    0x4a0e0cff, 0x00001380, 0x4a100cff, 0x00002380, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda1c0400, 0x00000007, 0xd8d80000, 0x11000007, 0x4a0e0cff, 0x00001400, 0x4a100cff, 0x00000400,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda207c00, 0x00000007, 0xd8d80000, 0x12000007,
    0x4a0e0cff, 0x00001480, 0x4a100cff, 0x00002480, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda240400, 0x00000007, 0xd8d80000, 0x13000007, 0x4a0e0cff, 0x00001500, 0x4a100cff, 0x00000500,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda287c00, 0x00000007, 0xd8d80000, 0x14000007,
    0x4a0e0cff, 0x00001580, 0x4a100cff, 0x00002580, 0xd8340000, 0x00000207, 0xd8340000, 0x00000308,
    0xda2c0400, 0x00000007, 0xd8d80000, 0x15000007, 0x4a0e0cff, 0x00001600, 0x4a100cff, 0x00000600,
    0xd8340000, 0x00000207, 0xd8340000, 0x00000308, 0xda347c00, 0x00000007, 0xd8d80000, 0x16000007,
    0x34020086, 0xbf8cc07f, 0xe0781000, 0x80010a01, 0xe0781010, 0x80010e01, 0xe0781020, 0x80011201,
    0xe0781030, 0x80011601, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> AddressOffsetCode{0xda008000, 0x00000000, 0xbf810000};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    const std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    const auto mixed = seed * 0xbf58476d1ce4e5b9ull;
    words[0] = tid % 4u == 0u ? tid % 7u : static_cast<std::uint32_t>(seed);
    words[1] = tid % 4u == 0u ? (tid * 3u) % 7u : tid % 4u == 1u ? words[0] : static_cast<std::uint32_t>(mixed >> 32u);
    words[2] = std::bit_cast<std::uint32_t>(static_cast<float>(static_cast<std::int32_t>(tid % 9u) - 4) * 1.75f + 0.5f);
    words[3] = std::bit_cast<std::uint32_t>(static_cast<float>(static_cast<std::int32_t>((tid * 5u) % 11u) - 5) * 0.625f + 0.25f);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
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
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, Code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, 13> names{
        "ds_add_src2_u32", "ds_sub_src2_u32", "ds_rsub_src2_u32", "ds_inc_src2_u32", "ds_dec_src2_u32", "ds_min_src2_i32", "ds_max_src2_i32", "ds_min_src2_u32",
        "ds_max_src2_u32", "ds_and_src2_b32", "ds_or_src2_b32", "ds_xor_src2_b32", "ds_write_src2_b32",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const auto sa = static_cast<std::int32_t>(a);
        const auto sb = static_cast<std::int32_t>(b);
        const std::array<std::uint32_t, 13> expected{
            a + b, a - b, b - a, a >= b ? 0u : a + 1u, (a == 0u || a > b) ? b : a - 1u,
            static_cast<std::uint32_t>(std::min(sa, sb)), static_cast<std::uint32_t>(std::max(sa, sb)), std::min(a, b), std::max(a, b),
            a & b, a | b, a ^ b, b,
        };
        for (std::uint32_t op = 0; op < expected.size(); ++op) {
            const std::uint32_t actual = Output[tid * Results + op];
            Require(actual == expected[op], "lds src2: lane " + std::to_string(tid) + " " + names[op] + " is " + Hex(actual) + ", expected " + Hex(expected[op]));
        }
    }
}

void CheckFloatRejected(AgcDriver::VulkanDevice& device) {
    for (const std::uint32_t opcode : {0x92u, 0x93u, 0x95u}) {
        alignas(256) static std::array<std::uint32_t, 3> code{};
        code = {0xd8000000u | (opcode << 18u) | 1u, 0x00000000u, 0xbf810000u};
        bool refused = false;
        try {
            (void)Compile(device, code);
        } catch (const std::exception& error) {
            refused = std::string(error.what()).find("f32 denormal mode") != std::string::npos;
        }
        Require(refused, "lds src2: f32 opcode " + Hex(opcode) + " was not refused");
    }
}

void CheckAddressOffsetRejected(AgcDriver::VulkanDevice& device) {
    try {
        (void)Compile(device, AddressOffsetCode);
    } catch (const std::exception&) {
        return;
    }
    Require(false, "lds src2: an offset taken from the address was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        CheckLanes();
        CheckAddressOffsetRejected(*device);
        CheckFloatRejected(*device);
        std::puts("lds src2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
