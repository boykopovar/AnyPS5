#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 128;
constexpr std::uint32_t Operations = 35;
constexpr std::uint32_t ReturningOperations = 18;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 475> Code{
    0x34020085, 0x34060089, 0x34040083, 0x7ef80280, 0x7efa0280, 0x7efc0280, 0xe0301000, 0x80000401,
    0xe0301004, 0x80000501, 0xe0301008, 0x80000601, 0xe030100c, 0x80000701, 0xe0301010, 0x80000801,
    0xe0301014, 0x80000901, 0xbf8c3f70, 0xd9340000, 0x00000402, 0xd9340100, 0x00000402, 0xd9340200,
    0x00000402, 0xd9340300, 0x00000402, 0xd9340400, 0x00000402, 0xd9340500, 0x00000402, 0xd9340600,
    0x00000402, 0xd9340700, 0x00000402, 0xd9340800, 0x00000402, 0xd9340900, 0x00000402, 0xd9340a00,
    0x00000402, 0xd9340b00, 0x00000402, 0xd9340c00, 0x00000402, 0xd9340d00, 0x00000402, 0xd9340e00,
    0x00000402, 0xd9340f00, 0x00000402, 0xd9341000, 0x00000402, 0xd9341100, 0x00000402, 0xd9341200,
    0x00000402, 0xd9341300, 0x00000402, 0xd9341400, 0x00000402, 0xd9341500, 0x00000402, 0xd9341600,
    0x00000402, 0xd9341700, 0x00000402, 0xd9341800, 0x00000402, 0xd9341900, 0x00000402, 0xd9341a00,
    0x00000402, 0xd9341b00, 0x00000402, 0xd9341c00, 0x00000402, 0xd9341d00, 0x00000402, 0xd9341e00,
    0x00000402, 0xd9341f00, 0x00000402, 0xd9342000, 0x00000402, 0xd9342100, 0x00000402, 0xd9342200,
    0x00000402, 0xd9342300, 0x00007c7e, 0xd9342308, 0x00007c7e, 0xbf8cc07f, 0xbf8a0000, 0xd9000000,
    0x00000602, 0xd9040100, 0x00000602, 0xd9080200, 0x00000602, 0xd90c0300, 0x00000602, 0xd9100400,
    0x00000602, 0xd9140500, 0x00000602, 0xd9180600, 0x00000602, 0xd91c0700, 0x00000602, 0xd9200800,
    0x00000602, 0xd9240900, 0x00000602, 0xd9280a00, 0x00000602, 0xd92c0b00, 0x00000602, 0xd9300c00,
    0x00080602, 0xd9400d00, 0x00080602, 0xd9440e00, 0x00080602, 0xd9480f00, 0x00000602, 0xd94c1000,
    0x00000602, 0xd9801100, 0x0a000602, 0xd9841200, 0x0c000602, 0xd9881300, 0x0e000602, 0xd98c1400,
    0x10000602, 0xd9901500, 0x12000602, 0xd9941600, 0x14000602, 0xd9981700, 0x16000602, 0xd99c1800,
    0x18000602, 0xd9a01900, 0x1a000602, 0xd9a41a00, 0x1c000602, 0xd9a81b00, 0x1e000602, 0xd9ac1c00,
    0x20000602, 0xd9b01d00, 0x22080602, 0xd9b41e00, 0x24000602, 0xd9c01f00, 0x26080602, 0xd9c42000,
    0x28080602, 0xd9c82100, 0x2a000602, 0xd9cc2200, 0x2c000602, 0xd9802300, 0x2e00067e, 0xd9a02308,
    0x3000087e, 0xbf8cc07f, 0xbf8a0000, 0xd9d80000, 0x32000002, 0xd9d80100, 0x34000002, 0xd9d80200,
    0x36000002, 0xd9d80300, 0x38000002, 0xd9d80400, 0x3a000002, 0xd9d80500, 0x3c000002, 0xd9d80600,
    0x3e000002, 0xd9d80700, 0x40000002, 0xd9d80800, 0x42000002, 0xd9d80900, 0x44000002, 0xd9d80a00,
    0x46000002, 0xd9d80b00, 0x48000002, 0xd9d80c00, 0x4a000002, 0xd9d80d00, 0x4c000002, 0xd9d80e00,
    0x4e000002, 0xd9d80f00, 0x50000002, 0xd9d81000, 0x52000002, 0xd9d81100, 0x54000002, 0xd9d81200,
    0x56000002, 0xd9d81300, 0x58000002, 0xd9d81400, 0x5a000002, 0xd9d81500, 0x5c000002, 0xd9d81600,
    0x5e000002, 0xd9d81700, 0x60000002, 0xd9d81800, 0x62000002, 0xd9d81900, 0x64000002, 0xd9d81a00,
    0x66000002, 0xd9d81b00, 0x68000002, 0xd9d81c00, 0x6a000002, 0xd9d81d00, 0x6c000002, 0xd9d81e00,
    0x6e000002, 0xd9d81f00, 0x70000002, 0xd9d82000, 0x72000002, 0xd9d82100, 0x74000002, 0xd9d82200,
    0x76000002, 0xd9d82300, 0x7800007e, 0xd9d82308, 0x7a00007e, 0xbf8cc07f, 0xe0701000, 0x80013203,
    0xe0701004, 0x80013303, 0xe0701008, 0x80013403, 0xe070100c, 0x80013503, 0xe0701010, 0x80013603,
    0xe0701014, 0x80013703, 0xe0701018, 0x80013803, 0xe070101c, 0x80013903, 0xe0701020, 0x80013a03,
    0xe0701024, 0x80013b03, 0xe0701028, 0x80013c03, 0xe070102c, 0x80013d03, 0xe0701030, 0x80013e03,
    0xe0701034, 0x80013f03, 0xe0701038, 0x80014003, 0xe070103c, 0x80014103, 0xe0701040, 0x80014203,
    0xe0701044, 0x80014303, 0xe0701048, 0x80014403, 0xe070104c, 0x80014503, 0xe0701050, 0x80014603,
    0xe0701054, 0x80014703, 0xe0701058, 0x80014803, 0xe070105c, 0x80014903, 0xe0701060, 0x80014a03,
    0xe0701064, 0x80014b03, 0xe0701068, 0x80014c03, 0xe070106c, 0x80014d03, 0xe0701070, 0x80014e03,
    0xe0701074, 0x80014f03, 0xe0701078, 0x80015003, 0xe070107c, 0x80015103, 0xe0701080, 0x80015203,
    0xe0701084, 0x80015303, 0xe0701088, 0x80015403, 0xe070108c, 0x80015503, 0xe0701090, 0x80015603,
    0xe0701094, 0x80015703, 0xe0701098, 0x80015803, 0xe070109c, 0x80015903, 0xe07010a0, 0x80015a03,
    0xe07010a4, 0x80015b03, 0xe07010a8, 0x80015c03, 0xe07010ac, 0x80015d03, 0xe07010b0, 0x80015e03,
    0xe07010b4, 0x80015f03, 0xe07010b8, 0x80016003, 0xe07010bc, 0x80016103, 0xe07010c0, 0x80016203,
    0xe07010c4, 0x80016303, 0xe07010c8, 0x80016403, 0xe07010cc, 0x80016503, 0xe07010d0, 0x80016603,
    0xe07010d4, 0x80016703, 0xe07010d8, 0x80016803, 0xe07010dc, 0x80016903, 0xe07010e0, 0x80016a03,
    0xe07010e4, 0x80016b03, 0xe07010e8, 0x80016c03, 0xe07010ec, 0x80016d03, 0xe07010f0, 0x80016e03,
    0xe07010f4, 0x80016f03, 0xe07010f8, 0x80017003, 0xe07010fc, 0x80017103, 0xe0701100, 0x80017203,
    0xe0701104, 0x80017303, 0xe0701108, 0x80017403, 0xe070110c, 0x80017503, 0xe0701110, 0x80017603,
    0xe0701114, 0x80017703, 0xe0701118, 0x80017803, 0xe070111c, 0x80017903, 0xe0701120, 0x80017a03,
    0xe0701124, 0x80017b03, 0xe0701128, 0x80010a03, 0xe070112c, 0x80010b03, 0xe0701130, 0x80010c03,
    0xe0701134, 0x80010d03, 0xe0701138, 0x80010e03, 0xe070113c, 0x80010f03, 0xe0701140, 0x80011003,
    0xe0701144, 0x80011103, 0xe0701148, 0x80011203, 0xe070114c, 0x80011303, 0xe0701150, 0x80011403,
    0xe0701154, 0x80011503, 0xe0701158, 0x80011603, 0xe070115c, 0x80011703, 0xe0701160, 0x80011803,
    0xe0701164, 0x80011903, 0xe0701168, 0x80011a03, 0xe070116c, 0x80011b03, 0xe0701170, 0x80011c03,
    0xe0701174, 0x80011d03, 0xe0701178, 0x80011e03, 0xe070117c, 0x80011f03, 0xe0701180, 0x80012003,
    0xe0701184, 0x80012103, 0xe0701188, 0x80012203, 0xe070118c, 0x80012303, 0xe0701190, 0x80012403,
    0xe0701194, 0x80012503, 0xe0701198, 0x80012603, 0xe070119c, 0x80012703, 0xe07011a0, 0x80012803,
    0xe07011a4, 0x80012903, 0xe07011a8, 0x80012a03, 0xe07011ac, 0x80012b03, 0xe07011b0, 0x80012c03,
    0xe07011b4, 0x80012d03, 0xe07011b8, 0x80012e03, 0xe07011bc, 0x80012f03, 0xe07011c0, 0x80013003,
    0xe07011c4, 0x80013103, 0xbf810000,
};

constexpr std::uint64_t Sign = 0x8000000000000000ull;

bool Nan(std::uint64_t bits) { return (bits & ~Sign) > 0x7ff0000000000000ull; }
std::uint64_t OrderKey(std::uint64_t bits) { return (bits & Sign) != 0u ? ~bits : bits | Sign; }
std::uint64_t MinMax(std::uint64_t old, std::uint64_t data, bool max) {
    if (Nan(old)) return data;
    if (Nan(data)) return old;
    return (max ? OrderKey(data) > OrderKey(old) : OrderKey(data) < OrderKey(old)) ? data : old;
}
bool FloatEqual(std::uint64_t a, std::uint64_t b) {
    return (a == b && !Nan(a)) || ((a | b) & ~Sign) == 0u;
}

std::uint64_t Word(const std::uint32_t* words, std::uint32_t index) {
    return words[index * 2u] | (static_cast<std::uint64_t>(words[index * 2u + 1u]) << 32u);
}

void Fill(std::uint32_t tid, std::uint32_t* words) {
    constexpr std::array<std::uint64_t, 12> floats{
        0x0000000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
        0x7ff8000000000000ull, 0x7ff0000000000000ull, 0x4004000000000000ull, 0xc008000000000000ull,
        0x0000000000000001ull, 0x7ff0000000000001ull, 0xfff0000000000000ull, 0x800fffffffffffffull,
    };
    const std::uint32_t kind = tid % 4u;
    const std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    std::array<std::uint64_t, 3> values{};
    if (kind == 0u) values = {seed, seed * 0xbf58476d1ce4e5b9ull, seed ^ 0x94d049bb133111ebull};
    if (kind == 1u) values = {tid % 7u, (tid * 3u) % 7u, tid % 5u};
    if (kind == 2u) values = {floats[tid % 12u], floats[(tid / 2u + 3u) % 12u], floats[(tid + 5u) % 12u]};
    if (kind == 3u) values = {seed, (tid & 4u) != 0u ? seed : seed + 0x100000000ull, seed * 3u};
    for (std::uint32_t i = 0; i < values.size(); ++i) {
        words[i * 2u] = static_cast<std::uint32_t>(values[i]);
        words[i * 2u + 1u] = static_cast<std::uint32_t>(values[i] >> 32u);
    }
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

void Expect(std::uint32_t tid, std::uint64_t actual, std::uint64_t expected, const std::string& name) {
    Require(actual == expected, "lds 64-bit atomics: lane " + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 2244u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckLanes() {
    constexpr std::array<const char*, Operations> names{
        "ds_add_u64", "ds_sub_u64", "ds_rsub_u64", "ds_inc_u64", "ds_dec_u64", "ds_min_i64", "ds_max_i64", "ds_min_u64", "ds_max_u64",
        "ds_and_b64", "ds_or_b64", "ds_xor_b64", "ds_mskor_b64", "ds_cmpst_b64", "ds_cmpst_f64", "ds_min_f64", "ds_max_f64",
        "ds_add_rtn_u64", "ds_sub_rtn_u64", "ds_rsub_rtn_u64", "ds_inc_rtn_u64", "ds_dec_rtn_u64", "ds_min_rtn_i64", "ds_max_rtn_i64",
        "ds_min_rtn_u64", "ds_max_rtn_u64", "ds_and_rtn_b64", "ds_or_rtn_b64", "ds_xor_rtn_b64", "ds_mskor_rtn_b64", "ds_wrxchg_rtn_b64",
        "ds_cmpst_rtn_b64", "ds_cmpst_rtn_f64", "ds_min_rtn_f64", "ds_max_rtn_f64",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        const std::uint64_t a = Word(in, 0);
        const std::uint64_t b = Word(in, 1);
        const std::uint64_t c = Word(in, 2);
        const auto sa = static_cast<std::int64_t>(a);
        const auto sb = static_cast<std::int64_t>(b);
        const std::array<std::uint64_t, 17> common{
            a + b, a - b, b - a, a >= b ? 0u : a + 1u, (a == 0u || a > b) ? b : a - 1u,
            sb < sa ? b : a, sb > sa ? b : a, b < a ? b : a, b > a ? b : a,
            a & b, a | b, a ^ b, (a & ~b) | c, a == b ? c : a, FloatEqual(a, b) ? c : a,
            MinMax(a, b, false), MinMax(a, b, true),
        };
        std::array<std::uint64_t, Operations> memory{};
        std::copy(common.begin(), common.end(), memory.begin());
        std::copy(common.begin(), common.begin() + 13, memory.begin() + 17);
        memory[30] = b;
        std::copy(common.begin() + 13, common.end(), memory.begin() + 31);
        for (std::uint32_t j = 0; j < Operations; ++j) Expect(tid, Word(out, j), memory[j], names[j]);
        for (std::uint32_t j = 0; j < ReturningOperations; ++j) Expect(tid, Word(out, Operations + 2u + j), a, std::string(names[17u + j]) + " returned value");
    }
}

void CheckContention() {
    std::uint64_t total = 0;
    std::uint64_t maximum = 0;
    std::vector<std::uint32_t> pending;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        total += Word(&Input[tid * Inputs], 1);
        maximum = std::max(maximum, Word(&Input[tid * Inputs], 2));
        pending.push_back(tid);
    }
    Expect(0, Word(Output.data(), Operations), total, "contended ds_add_rtn_u64 total");
    Expect(0, Word(Output.data(), Operations + 1u), maximum, "contended ds_max_rtn_u64 result");
    std::uint64_t current = 0;
    while (!pending.empty()) {
        auto next = pending.end();
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            if (Word(&Output[*it * Results], Operations + 2u + ReturningOperations) != current) continue;
            if (next == pending.end() || Word(&Input[*it * Inputs], 1) == 0u) next = it;
        }
        Require(next != pending.end(), "lds 64-bit atomics: contended ds_add_rtn_u64 returned values do not form one serial order at " + Hex(current));
        current += Word(&Input[*next * Inputs], 1);
        pending.erase(next);
    }
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t old = Word(&Output[tid * Results], Operations + 3u + ReturningOperations);
        bool seen = old == 0u;
        for (std::uint32_t lane = 0; lane < Threads; ++lane) seen |= Word(&Input[lane * Inputs], 2) == old;
        Require(seen && old <= maximum, "lds 64-bit atomics: contended ds_max_rtn_u64 returned " + Hex(old));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        CheckLanes();
        CheckContention();
        std::puts("lds 64-bit atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
