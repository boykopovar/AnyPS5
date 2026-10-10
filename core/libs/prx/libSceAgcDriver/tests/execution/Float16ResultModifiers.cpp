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
#include <iostream>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 92> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xe030100c, 0x80000701, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280, 0x7e1c0280, 0x7e1e0280,
    0x7e200280, 0x7e220280, 0x7e240280, 0x7e260280, 0x7e280280, 0x7e2a0280, 0x7e2c0280, 0x7e2e0280,
    0x7e300280, 0x7e320280, 0xbf8c3f70, 0xd5d4800a, 0x00000104, 0xd5d4000b, 0x08000104, 0xd5d5800c,
    0x08000104, 0xd5e0000d, 0x08000104, 0xd5e0800e, 0x00000105, 0xd58a800f, 0x00000107, 0xd58a0010,
    0x18000107, 0xd5d08011, 0x00000106, 0xd74b8012, 0x041a0b04, 0xd74b0013, 0x0c1a0b04, 0x64280af9,
    0x04042404, 0x6a2a0af9, 0x04044404, 0xd52f8016, 0x00020d07, 0xd52f0017, 0x18020d07, 0xd5db8018,
    0x00000105, 0x64320af9, 0x04043504, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008,
    0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014, 0x80010f03, 0xe0701018,
    0x80011003, 0xe070101c, 0x80011103, 0xe0701020, 0x80011203, 0xe0701024, 0x80011303, 0xe0701028,
    0x80011403, 0xe070102c, 0x80011503, 0xe0701030, 0x80011603, 0xe0701034, 0x80011703, 0xe0701038,
    0x80011803, 0xe070103c, 0x80011903, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00003400u, 0x00003800u, 0x00003000u, 0x3e99999au},
    {0x00003e00u, 0x0000b400u, 0x00004000u, 0xbfd9999au},
    {0x00007b53u, 0x000070e2u, 0x00007bffu, 0x47ffdc00u},
    {0x00000400u, 0x00003800u, 0x00000000u, 0x33000000u},
    {0x00007e00u, 0x00003c00u, 0x00003c00u, 0x7fc00000u},
    {0x00007c00u, 0x00003c00u, 0x0000bc00u, 0x7f800000u},
    {0x0000c000u, 0x0000c200u, 0x00004400u, 0x80000000u},
    {0x00000001u, 0x00003a00u, 0x00008001u, 0x477ff000u},
    {0x00002265u, 0x000091b7u, 0x0000d8f1u, 0xcd613e30u},
    {0x0000c386u, 0x00001027u, 0x0000414cu, 0xc2ce6f44u},
    {0x00007311u, 0x000078e5u, 0x0000a6ceu, 0xc9e9c616u},
    {0x000035bfu, 0x00001807u, 0x00000741u, 0xc324c985u},
    {0x0000c464u, 0x0000b222u, 0x00007204u, 0x442e3d43u},
    {0x0000b8b6u, 0x0000cd44u, 0x00003a90u, 0x37730edfu},
    {0x0000f813u, 0x00006c0fu, 0x0000b9d1u, 0x38c0c8fdu},
    {0x0000c381u, 0x00007019u, 0x0000f06du, 0x3bab6c39u},
    {0x0000587fu, 0x00003b1au, 0x0000ad45u, 0x380208a9u},
    {0x0000c2cdu, 0x000075a8u, 0x0000f3c6u, 0x4a2f20aau},
    {0x0000ed2fu, 0x00000580u, 0x00006a8au, 0xb94067edu},
    {0x0000dc25u, 0x00004be0u, 0x00001ef2u, 0xbe3edc0au},
    {0x0000552bu, 0x0000e544u, 0x0000b8b3u, 0xb610a9f7u},
    {0x0000efbau, 0x0000f79bu, 0x00006c0fu, 0x4da98f1du},
    {0x000048beu, 0x0000966bu, 0x0000f934u, 0x3e2434e3u},
    {0x0000be65u, 0x0000cc22u, 0x0000677fu, 0xb3fa7aa7u},
    {0x0000c69du, 0x0000acabu, 0x0000bcfbu, 0xc74803e3u},
    {0x000029e8u, 0x0000855cu, 0x0000d707u, 0xbb968a43u},
    {0x00000792u, 0x00007825u, 0x00000b21u, 0x4efbc8d6u},
    {0x0000b410u, 0x0000d92au, 0x0000fbb2u, 0x3a1890c7u},
    {0x0000fb69u, 0x0000c541u, 0x00003313u, 0x3b6fe507u},
    {0x0000678au, 0x00005804u, 0x0000f3d4u, 0x44ef7febu},
    {0x0000a8c2u, 0x00008c49u, 0x00009be3u, 0xbab9f87fu},
    {0x00006239u, 0x0000c89du, 0x0000db61u, 0xbd91a1b7u},
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00003c00u, 0x00004400u, 0x00003800u, 0x00003c00u, 0x00000000u, 0x000034cdu, 0x000034cdu, 0x00003c00u, 0x00003400u, 0x00003400u, 0x00003a00u, 0x00003000u, 0x000034ccu, 0x000034ccu, 0x00000000u, 0x3a000000u},
    {0x00003955u, 0x00003955u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000becdu, 0x00003c00u, 0x00003c00u, 0x00003e80u, 0x00003c00u, 0x0000b600u, 0x0000beccu, 0x0000beccu, 0x00000000u, 0x3c000000u},
    {0x00000118u, 0x00000118u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00007c00u, 0x00007bffu, 0x00007bffu, 0x00003c00u, 0x3c000000u},
    {0x00003c00u, 0x00007400u, 0x00002000u, 0x00000e48u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000200u, 0x00000200u, 0x00003800u, 0x00000200u, 0x00000000u, 0x00000000u, 0x00000000u, 0x38000000u},
    {0x00000000u, 0x00007e00u, 0x00000000u, 0x00007e00u, 0x00000000u, 0x00000000u, 0x00007e00u, 0x00003c00u, 0x00000000u, 0x00007e00u, 0x00000000u, 0x00007e00u, 0x00007e00u, 0x00007e00u, 0x00003c00u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00003c00u, 0x0000fe00u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00007c00u, 0x00007c00u, 0x00007c00u, 0x00003c00u, 0x3c000000u},
    {0x00000000u, 0x0000b800u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00003c00u, 0x00003c00u, 0x00004900u, 0x00000000u, 0x00004600u, 0x00008000u, 0x00008000u, 0x00000000u, 0x00000000u},
    {0x00003c00u, 0x00007c00u, 0x00000c00u, 0x00000006u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00000000u, 0x00008000u, 0x00003a00u, 0x00000001u, 0x00007bffu, 0x00007bffu, 0x00000000u, 0x3a000000u},
    {0x00003c00u, 0x00005501u, 0x00002f27u, 0x00002d04u, 0x00000000u, 0x00000000u, 0x0000fc00u, 0x00003c00u, 0x00000000u, 0x0000d8f1u, 0x0000220au, 0x00008092u, 0x0000fbffu, 0x0000fbffu, 0x00000000u, 0x220a0000u},
    {0x00000000u, 0x0000b441u, 0x00000000u, 0x00003bfau, 0x00001a86u, 0x00000000u, 0x0000d673u, 0x00003c00u, 0x00003c00u, 0x0000414bu, 0x00000000u, 0x000097cfu, 0x0000d673u, 0x0000d673u, 0x00000000u, 0x00000000u},
    {0x00000487u, 0x00000487u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000fc00u, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00007c00u, 0x0000fbffu, 0x0000fbffu, 0x00003c00u, 0x3c000000u},
    {0x00003c00u, 0x00004192u, 0x000038cbu, 0x00003a31u, 0x00002253u, 0x00000000u, 0x0000d926u, 0x00003c00u, 0x000012b1u, 0x000012b1u, 0x000035c7u, 0x000011c9u, 0x0000d926u, 0x0000d926u, 0x00000000u, 0x35c70000u},
    {0x00000000u, 0x0000b34au, 0x00000000u, 0x0000b913u, 0x00000000u, 0x00003c00u, 0x00006172u, 0x00003c00u, 0x00003c00u, 0x00007204u, 0x00000000u, 0x00003abbu, 0x00006171u, 0x00006171u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x0000becbu, 0x00000000u, 0x0000383du, 0x00000000u, 0x000000f3u, 0x000000f3u, 0x00003c00u, 0x00003c00u, 0x00004a9du, 0x00000000u, 0x00004a34u, 0x000000f3u, 0x000000f3u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x000081f7u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000606u, 0x00000606u, 0x00003c00u, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x0000fc00u, 0x00000606u, 0x00000606u, 0x00003c00u, 0x00000000u},
    {0x00000000u, 0x0000b444u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00001d5bu, 0x00001d5bu, 0x00003c00u, 0x00000000u, 0x0000f8f3u, 0x00003c00u, 0x0000f7b0u, 0x00001d5bu, 0x00001d5bu, 0x00003c00u, 0x3c000000u},
    {0x00001f1eu, 0x00001f1eu, 0x00003c00u, 0x0000b9a8u, 0x00000000u, 0x00000208u, 0x00000208u, 0x00003c00u, 0x00003c00u, 0x000057fau, 0x00003c00u, 0x000057fbu, 0x00000208u, 0x00000208u, 0x00000000u, 0x3c000000u},
    {0x00000000u, 0x0000b4b5u, 0x00000000u, 0x0000b8b0u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00000000u, 0x0000fc00u, 0x00003c00u, 0x0000fc00u, 0x00007bffu, 0x00007bffu, 0x00003c00u, 0x3c000000u},
    {0x00000000u, 0x00008a2cu, 0x00000000u, 0x00000000u, 0x00001052u, 0x00000000u, 0x00008a03u, 0x00003c00u, 0x00003c00u, 0x00006a8au, 0x00000000u, 0x0000b721u, 0x00008a03u, 0x00008a03u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00009bb9u, 0x00000000u, 0x0000bc00u, 0x00000000u, 0x00000000u, 0x0000b1f7u, 0x00003c00u, 0x00000000u, 0x0000ec14u, 0x00000000u, 0x0000ec14u, 0x0000b1f6u, 0x0000b1f6u, 0x00003c00u, 0x00000000u},
    {0x00002231u, 0x00002231u, 0x00003c00u, 0x0000bb64u, 0x00000000u, 0x00000000u, 0x00008024u, 0x00003c00u, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x0000fc00u, 0x00008024u, 0x00008024u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00008824u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00000000u, 0x00007c00u, 0x00007bffu, 0x00007bffu, 0x00000000u, 0x00000000u},
    {0x00002ebfu, 0x00002ebfu, 0x00003c00u, 0x00002e46u, 0x00000000u, 0x00003122u, 0x00003122u, 0x00003c00u, 0x00000000u, 0x0000f934u, 0x00003c00u, 0x0000a39cu, 0x00003121u, 0x00003121u, 0x00000000u, 0x3c000000u},
    {0x00000000u, 0x0000b901u, 0x00000000u, 0x000038a6u, 0x0000323eu, 0x00000000u, 0x00008002u, 0x00003c00u, 0x00003c00u, 0x00006799u, 0x00000000u, 0x00004e9bu, 0x00008001u, 0x00008001u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x0000b0d7u, 0x00000000u, 0x0000393au, 0x00000000u, 0x00000000u, 0x0000fa40u, 0x00003c00u, 0x00000000u, 0x0000ba1au, 0x00000000u, 0x000037b8u, 0x0000fa40u, 0x0000fa40u, 0x00000000u, 0x00000000u},
    {0x00003c00u, 0x00004d6bu, 0x000032e0u, 0x00003493u, 0x00000000u, 0x00000000u, 0x00009cb4u, 0x00003c00u, 0x00000000u, 0x0000d707u, 0x000029e5u, 0x0000803fu, 0x00009cb4u, 0x00009cb4u, 0x00000000u, 0x29e50000u},
    {0x00003c00u, 0x0000703au, 0x00002181u, 0x000011f2u, 0x00000000u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00003c00u, 0x000043d8u, 0x00003c00u, 0x000043d8u, 0x00007bffu, 0x00007bffu, 0x00003c00u, 0x3c000000u},
    {0x00000000u, 0x0000c3e0u, 0x00000000u, 0x0000bbffu, 0x00000000u, 0x000010c5u, 0x000010c5u, 0x00003c00u, 0x00000000u, 0x0000fbb1u, 0x00000000u, 0x0000513fu, 0x000010c4u, 0x000010c4u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00008114u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00001b7fu, 0x00001b7fu, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00000000u, 0x00007c00u, 0x00001b7fu, 0x00001b7fu, 0x00000000u, 0x00000000u},
    {0x0000103fu, 0x0000103fu, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x0000677cu, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00007c00u, 0x0000677bu, 0x0000677bu, 0x00003c00u, 0x3c000000u},
    {0x00000000u, 0x0000cebau, 0x00000000u, 0x0000b368u, 0x00000000u, 0x00000000u, 0x000095d0u, 0x00003c00u, 0x00000000u, 0x00009bdeu, 0x00000000u, 0x000000a3u, 0x000095cfu, 0x000095cfu, 0x00000000u, 0x00000000u},
    {0x00001524u, 0x00001524u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000ac8du, 0x00003c00u, 0x00000000u, 0x0000ef68u, 0x00003c00u, 0x0000ef2du, 0x0000ac8du, 0x0000ac8du, 0x00000000u, 0x3c000000u},
};
constexpr const char* Names[16] = {"rcp_clamp", "rcp_mul2", "sqrt_clamp_mul2", "sin_mul2", "sin_clamp", "cvt_f16_f32_clamp", "cvt_f16_f32_div2", "cvt_f16_u16_clamp", "fma_clamp", "fma_mul2", "add_sdwa_clamp", "mul_sdwa_mul2", "pkrtz_clamp", "pkrtz_div2", "floor_clamp", "add_sdwa_hi_clamp"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
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

bool IsNan16(std::uint32_t bits) {
    return (bits & 0x7fffu) > 0x7c00u;
}

bool SameHalf(std::uint32_t actual, std::uint32_t expected) {
    return actual == expected || (IsNan16(actual) && IsNan16(expected));
}

double HalfValue(std::uint32_t bits) {
    const double magnitude = (bits & 0x7c00u) == 0u ? std::ldexp(static_cast<double>(bits & 0x3ffu), -24) : std::ldexp(static_cast<double>((bits & 0x3ffu) | 0x400u), static_cast<int>((bits >> 10u) & 0x1fu) - 25);
    return (bits & 0x8000u) != 0u ? -magnitude : magnitude;
}

bool NearHalf(std::uint32_t actual, std::uint32_t expected, bool clamped) {
    if (clamped && (actual & 0x8000u) != 0u) return false;
    if (!clamped && (expected & 0x7fffu) == 0u) return actual == expected;
    if (IsNan16(actual) || IsNan16(expected) || (actual & 0x7fffu) == 0x7c00u || (expected & 0x7fffu) == 0x7c00u) return SameHalf(actual, expected);
    return std::abs(HalfValue(actual) - HalfValue(expected)) <= 0x1p-10;
}

void ExpectNear(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name, bool clamped) {
    Require((actual >> 16u) == (expected >> 16u) && NearHalf(actual & 0xffffu, expected & 0xffffu, clamped), std::string("float16 result modifiers: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected near " + Hex(expected));
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(SameHalf(actual & 0xffffu, expected & 0xffffu) && SameHalf(actual >> 16u, expected >> 16u), std::string("float16 result modifiers: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
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
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < 16; ++index) {
            if (index == 3u || index == 4u) ExpectNear(tid, out[index], Expected[tid][index], Names[index], index == 4u);
            else Expect(tid, out[index], Expected[tid][index], Names[index]);
        }
    }
}

const Testing::Case results{"Float16ResultModifiers_TranscendentalConversionAndSdwaModifiers_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
