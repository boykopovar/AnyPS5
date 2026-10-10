#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 168> Code{
    0x34020084, 0x34060087, 0x343c0082, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008,
    0x80000601, 0xbf8c3f70, 0xd8340000, 0x0000041e, 0xd8340080, 0x0000041e, 0xd8340100, 0x0000041e,
    0xd8340180, 0x0000041e, 0xd8340200, 0x0000041e, 0xd8340280, 0x0000041e, 0xd8340300, 0x0000041e,
    0xd8340380, 0x0000041e, 0xd8340400, 0x0000041e, 0xd8340480, 0x0000041e, 0xd8340500, 0x0000041e,
    0xd8340580, 0x0000041e, 0xd8340600, 0x0000041e, 0xd8340680, 0x0000041e, 0xd8340700, 0x0000041e,
    0xd8340780, 0x0000041e, 0xd8340800, 0x0000041e, 0xbf8cc07f, 0xd8500000, 0x00000000, 0xd8080000,
    0x0000051e, 0xd8880080, 0x1400051e, 0xd80c0100, 0x0000051e, 0xd8100180, 0x0000051e, 0xd8300200,
    0x0006051e, 0xd8400280, 0x0006051e, 0xd8440300, 0x0006051e, 0xd8540380, 0x0000051e, 0xd8b00400,
    0x1506051e, 0xd8c00480, 0x1606051e, 0xd8c40500, 0x1806051e, 0xd8c80580, 0x1700051e, 0xd8cc0600,
    0x1900051e, 0xd8d00680, 0x1a06051e, 0xd9540700, 0x1b00051e, 0xd8480780, 0x0000051e, 0xd84c0800,
    0x0000051e, 0xbf8cc07f, 0xd8d80000, 0x2800001e, 0xd8d80080, 0x2900001e, 0xd8d80100, 0x2a00001e,
    0xd8d80180, 0x2b00001e, 0xd8d80200, 0x2c00001e, 0xd8d80280, 0x2d00001e, 0xd8d80300, 0x2e00001e,
    0xd8d80380, 0x2f00001e, 0xd8d80400, 0x3000001e, 0xd8d80480, 0x3100001e, 0xd8d80500, 0x3200001e,
    0xd8d80580, 0x3300001e, 0xd8d80600, 0x3400001e, 0xd8d80680, 0x3500001e, 0xd8d80700, 0x3600001e,
    0xd8d80780, 0x3700001e, 0xd8d80800, 0x3800001e, 0xbf8cc07f, 0xe0701000, 0x80012803, 0xe0701004,
    0x80012903, 0xe0701008, 0x80012a03, 0xe070100c, 0x80012b03, 0xe0701010, 0x80012c03, 0xe0701014,
    0x80012d03, 0xe0701018, 0x80012e03, 0xe070101c, 0x80012f03, 0xe0701020, 0x80013003, 0xe0701024,
    0x80013103, 0xe0701028, 0x80013203, 0xe070102c, 0x80013303, 0xe0701030, 0x80013403, 0xe0701034,
    0x80013503, 0xe0701038, 0x80013603, 0xe070103c, 0x80013703, 0xe0701040, 0x80013803, 0xe0701044,
    0x80011403, 0xe0701048, 0x80011503, 0xe070104c, 0x80011603, 0xe0701050, 0x80011803, 0xe0701054,
    0x80011703, 0xe0701058, 0x80011903, 0xe070105c, 0x80011a03, 0xe0701060, 0x80011b03, 0xbf810000,
};

std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
float Value(std::uint32_t bits) { return std::bit_cast<float>(bits); }
bool Nan(std::uint32_t bits) { return (bits & 0x7fffffffu) > 0x7f800000u; }
bool Denormal(std::uint32_t bits) { return (bits & 0x7f800000u) == 0u && (bits & 0x007fffffu) != 0u; }
std::uint32_t MinMax(std::uint32_t old, std::uint32_t data, bool max) {
    if (Nan(old)) return data;
    if (Nan(data)) return old;
    if (((old | data) & 0x7fffffffu) == 0u) return max ? (old & data) : (old | data);
    return (max ? Value(data) > Value(old) : Value(data) < Value(old)) ? data : old;
}
void Fill(std::uint32_t tid, std::uint32_t* words) {
    constexpr std::array<std::uint32_t, 8> floats{0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7fc00000u, 0x7f800000u, 0x40200000u, 0xc0400000u};
    const std::uint32_t kind = tid % 4u;
    const std::uint32_t seed = tid * 0x9e3779b9u + 0x7f4a7c15u;
    if (kind == 0u) { words[0] = seed; words[1] = seed * 0x85ebca6bu; words[2] = seed ^ 0xc2b2ae35u; }
    if (kind == 1u) { words[0] = tid % 7u; words[1] = (tid * 3u) % 7u; words[2] = tid % 5u; }
    if (kind == 2u) { words[0] = floats[tid % 8u]; words[1] = floats[(tid / 2u + 3u) % 8u]; words[2] = floats[(tid + 5u) % 8u]; }
    if (kind == 3u) { words[0] = seed; words[1] = (tid & 4u) != 0u ? seed : seed + 1u; words[2] = seed * 3u; }
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

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("lds atomics: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 4096u, {false, false, false}, false, 1};
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

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const std::uint32_t c = in[2];
        const std::uint32_t sum = Nan(Bits(Value(a) + Value(b))) ? 0x7fc00000u : Bits(Value(a) + Value(b));
        const std::array<std::uint32_t, 17> memory{
            b - a, b - a, a >= b ? 0u : a + 1u, (a == 0u || a > b) ? b : a - 1u,
            (a & ~b) | c, a == b ? c : a, Value(a) == Value(b) ? c : a, sum,
            (a & ~b) | c, a == b ? c : a, Value(a) == Value(b) ? c : a, MinMax(a, b, false),
            MinMax(a, b, true), a >= b ? a - b : a + c, sum, MinMax(a, b, false), MinMax(a, b, true),
        };
        constexpr std::array<const char*, 17> names{"ds_rsub_u32", "ds_rsub_rtn_u32", "ds_inc_u32", "ds_dec_u32", "ds_mskor_b32", "ds_cmpst_b32", "ds_cmpst_f32", "ds_add_f32",
            "ds_mskor_rtn_b32", "ds_cmpst_rtn_b32", "ds_cmpst_rtn_f32", "ds_min_rtn_f32", "ds_max_rtn_f32", "ds_wrap_rtn_b32", "ds_add_rtn_f32", "ds_min_f32", "ds_max_f32"};
        constexpr std::uint32_t floatOps = (1u << 6u) | (1u << 7u) | (1u << 10u) | (1u << 11u) | (1u << 12u) | (1u << 14u) | (1u << 15u) | (1u << 16u);
        const bool denormal = Denormal(a) || Denormal(b) || Denormal(sum);
        for (std::uint32_t j = 0; j < memory.size(); ++j) {
            if (denormal && ((floatOps >> j) & 1u) != 0u) continue;
            const bool bothNan = Nan(out[j]) && Nan(memory[j]);
            Expect(tid, bothNan ? memory[j] : out[j], memory[j], names[j]);
        }
        for (std::uint32_t j = 17; j < 25; ++j) Expect(tid, out[j], a, "returned value");
    }
}


const Testing::Case floatAndIntegerAtomics{"LdsAtomics_IntegerAndFloatOperations_MatchReferenceResults", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
