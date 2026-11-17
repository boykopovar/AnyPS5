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
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 217> Code{
    0x34020084, 0xe0381000, 0x80000201, 0x340c0083, 0xbf8c3f70, 0x4a0e0cff, 0x00001000, 0x4a100c80,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb007c00, 0x00000007, 0xd9d80000, 0x0a000007,
    0x4a0e0cff, 0x00001100, 0x4a100cff, 0x00002100, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb040400, 0x00000007, 0xd9d80000, 0x0c000007, 0x4a0e0cff, 0x00001200, 0x4a100cff, 0x00000200,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb087c00, 0x00000007, 0xd9d80000, 0x0e000007,
    0x4a0e0cff, 0x00001300, 0x4a100cff, 0x00002300, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb0c0400, 0x00000007, 0xd9d80000, 0x10000007, 0x4a0e0cff, 0x00001400, 0x4a100cff, 0x00000400,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb107c00, 0x00000007, 0xd9d80000, 0x12000007,
    0x4a0e0cff, 0x00001500, 0x4a100cff, 0x00002500, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb140400, 0x00000007, 0xd9d80000, 0x14000007, 0x4a0e0cff, 0x00001600, 0x4a100cff, 0x00000600,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb187c00, 0x00000007, 0xd9d80000, 0x16000007,
    0x4a0e0cff, 0x00001700, 0x4a100cff, 0x00002700, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb1c0400, 0x00000007, 0xd9d80000, 0x18000007, 0x4a0e0cff, 0x00001800, 0x4a100cff, 0x00000800,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb207c00, 0x00000007, 0xd9d80000, 0x1a000007,
    0x4a0e0cff, 0x00001900, 0x4a100cff, 0x00002900, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb240400, 0x00000007, 0xd9d80000, 0x1c000007, 0x4a0e0cff, 0x00001a00, 0x4a100cff, 0x00000a00,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb287c00, 0x00000007, 0xd9d80000, 0x1e000007,
    0x4a0e0cff, 0x00001b00, 0x4a100cff, 0x00002b00, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb2c0400, 0x00000007, 0xd9d80000, 0x20000007, 0x4a0e0cff, 0x00001c00, 0x4a100cff, 0x00000c00,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb347c00, 0x00000007, 0xd9d80000, 0x22000007,
    0x4a0e0cff, 0x00001d00, 0x4a100cff, 0x00002d00, 0xd9340000, 0x00000207, 0xd9340000, 0x00000408,
    0xdb480400, 0x00000007, 0xd9d80000, 0x24000007, 0x4a0e0cff, 0x00001e00, 0x4a100cff, 0x00000e00,
    0xd9340000, 0x00000207, 0xd9340000, 0x00000408, 0xdb4c7c00, 0x00000007, 0xd9d80000, 0x26000007,
    0x34020087, 0xbf8cc07f, 0xe0741000, 0x80010a01, 0xe0741008, 0x80010c01, 0xe0741010, 0x80010e01,
    0xe0741018, 0x80011001, 0xe0741020, 0x80011201, 0xe0741028, 0x80011401, 0xe0741030, 0x80011601,
    0xe0741038, 0x80011801, 0xe0741040, 0x80011a01, 0xe0741048, 0x80011c01, 0xe0741050, 0x80011e01,
    0xe0741058, 0x80012001, 0xe0741060, 0x80012201, 0xe0741068, 0x80012401, 0xe0741070, 0x80012601,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> AddressOffsetCode{0xdb008000, 0x00000000, 0xbf810000};

constexpr std::uint64_t Sign = 0x8000000000000000ull;
constexpr std::uint64_t Quiet = 0x0008000000000000ull;

bool Nan(std::uint64_t bits) { return (bits & ~Sign) > 0x7ff0000000000000ull; }
std::uint64_t OrderKey(std::uint64_t bits) { return (bits & Sign) != 0u ? ~bits : bits | Sign; }
bool Signaling(std::uint64_t bits) { return Nan(bits) && (bits & Quiet) == 0u; }

std::uint64_t MinMax(std::uint64_t old, std::uint64_t data, bool max) {
    if (Signaling(data)) return data | Quiet;
    if (Signaling(old)) return old | Quiet;
    if (Nan(old)) return data;
    if (Nan(data)) return old;
    return (max ? OrderKey(data) > OrderKey(old) : OrderKey(data) < OrderKey(old)) ? data : old;
}

void Fill(std::uint32_t tid, std::uint32_t* words) {
    const std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    const auto mixed = seed * 0xbf58476d1ce4e5b9ull;
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    if (tid % 4u == 0u) {
        a = (tid % 7u) + 1u;
        b = ((tid * 3u) % 7u) + 2u;
    } else if (tid % 4u == 1u) {
        a = seed;
        b = a;
    } else if (tid % 4u == 2u) {
        a = seed;
        b = mixed;
    } else {
        const double fa = static_cast<double>(static_cast<std::int32_t>(tid % 9u) - 4) * 1.75 + 0.5;
        const double fb = static_cast<double>(static_cast<std::int32_t>((tid * 5u) % 11u) - 5) * 0.625 + 0.25;
        a = std::bit_cast<std::uint64_t>(fa);
        b = std::bit_cast<std::uint64_t>(fb);
    }
    words[0] = static_cast<std::uint32_t>(a);
    words[1] = static_cast<std::uint32_t>(a >> 32u);
    words[2] = static_cast<std::uint32_t>(b);
    words[3] = static_cast<std::uint32_t>(b >> 32u);
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

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, Code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, 15> names{
        "ds_add_src2_u64", "ds_sub_src2_u64", "ds_rsub_src2_u64", "ds_inc_src2_u64", "ds_dec_src2_u64",
        "ds_min_src2_i64", "ds_max_src2_i64", "ds_min_src2_u64", "ds_max_src2_u64",
        "ds_and_src2_b64", "ds_or_src2_b64", "ds_xor_src2_b64", "ds_write_src2_b64",
        "ds_min_src2_f64", "ds_max_src2_f64",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint64_t a = static_cast<std::uint64_t>(in[0]) | (static_cast<std::uint64_t>(in[1]) << 32u);
        const std::uint64_t b = static_cast<std::uint64_t>(in[2]) | (static_cast<std::uint64_t>(in[3]) << 32u);
        const auto sa = static_cast<std::int64_t>(a);
        const auto sb = static_cast<std::int64_t>(b);
        const std::array<std::uint64_t, 15> expected{
            a + b, a - b, b - a, a >= b ? 0ull : a + 1ull, (a == 0ull || a > b) ? b : a - 1ull,
            static_cast<std::uint64_t>(std::min(sa, sb)), static_cast<std::uint64_t>(std::max(sa, sb)), std::min(a, b), std::max(a, b),
            a & b, a | b, a ^ b, b,
            MinMax(a, b, false), MinMax(a, b, true),
        };
        for (std::uint32_t op = 0; op < expected.size(); ++op) {
            const std::uint32_t* out = &Output[tid * Results + op * 2u];
            const std::uint64_t actual = static_cast<std::uint64_t>(out[0]) | (static_cast<std::uint64_t>(out[1]) << 32u);
            Require(actual == expected[op], "lds src2 u64: lane " + std::to_string(tid) + " " + names[op] + " is " + Hex(actual) + ", expected " + Hex(expected[op]));
        }
    }
}

void CheckAddressOffsetRejected(AgcDriver::VulkanDevice& device) {
    try {
        (void)Compile(device, AddressOffsetCode);
    } catch (const std::exception&) {
        return;
    }
    Require(false, "lds src2 u64: an offset taken from the address was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        CheckLanes();
        CheckAddressOffsetRejected(*device);
        std::puts("lds src2 u64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
