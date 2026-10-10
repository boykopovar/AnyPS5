#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Garbage = 0xabcd1234u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 63> SdwaCode{
    0x34020082, 0x34060083, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0x7e1402ff, 0xabcd1234, 0x7e1602ff, 0xabcd1234, 0x7e1802ff, 0xabcd1234, 0x7e1a02ff, 0xabcd1234,
    0x7e1c02ff, 0xabcd1234, 0x7e1e02ff, 0xabcd1234, 0x7e2002ff, 0xabcd1234, 0x7e2202ff, 0xabcd1234,
    0x7e2402ff, 0xabcd1234, 0xbf8c3f70, 0x7e1414f9, 0x00060604, 0x7e161504,
    0x7e1814f9, 0x00061505, 0x7e1a14f9, 0x00060604, 0x7e1c14f9, 0x00060605, 0xd746000d, 0x0435210e,
    0x641e0cf9, 0x05040606, 0x7e2002f9, 0x00061004, 0x7e2202f9, 0x00061204, 0x7e2402f9, 0x00060104,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03,
    0xe070200c, 0x80010d03, 0xe0702010, 0x80010f03, 0xe0702014, 0x80011003, 0xe0702018, 0x80011103,
    0xe070201c, 0x80011203, 0xbf810000,
};

float Quarter(std::uint32_t index) {
    return static_cast<float>(static_cast<int>(index % 17u) - 8) / 4.0f;
}

std::uint32_t HalfBits(float value) {
    if (value == 0.0f) return std::signbit(value) ? 0x8000u : 0u;
    int exponent = 0;
    const float mantissa = std::frexp(std::fabs(value), &exponent);
    const auto fraction = static_cast<std::uint32_t>(std::ldexp(mantissa, 11));
    Testing::Require(exponent >= -13 && exponent <= 16 && std::ldexp(static_cast<float>(fraction), exponent - 11) == std::fabs(value), "sdwa destination: a model value is not a normal f16");
    return (std::signbit(value) ? 0x8000u : 0u) | (static_cast<std::uint32_t>(exponent + 14) << 10u) | (fraction & 0x3ffu);
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        words[0] = std::bit_cast<std::uint32_t>(Quarter(tid * 3u + 1u));
        words[1] = std::bit_cast<std::uint32_t>(Quarter(tid * 5u + 2u));
        words[2] = HalfBits(Quarter(tid * 7u + 3u)) | (HalfBits(Quarter(tid * 11u + 4u)) << 16u);
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    FillInput();
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(SdwaCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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
        const auto* in = &Input[tid * Inputs];
        const auto* out = &Output[tid * Results];
        const auto a = HalfBits(std::bit_cast<float>(in[0]));
        const auto b = HalfBits(std::bit_cast<float>(in[1]));
        const auto sum = HalfBits(Quarter(tid * 7u + 3u) + Quarter(tid * 11u + 4u));
        const auto where = [&](std::uint32_t result) { return "sdwa destination: thread " + std::to_string(tid) + " result " + std::to_string(result) + " is " + Hex(out[result]); };
        Testing::Require(out[0] == a, where(0) + ", expected " + Hex(a) + ": v_cvt_f16_f32_sdwa dst_sel:DWORD dst_unused:UNUSED_PAD must zero the high half");
        Testing::Require(out[1] == ((Garbage & 0xffff0000u) | a), where(1) + ", expected " + Hex((Garbage & 0xffff0000u) | a) + ": v_cvt_f16_f32_e32 must keep the high half");
        Testing::Require(out[2] == ((b << 16u) | (Garbage & 0xffffu)), where(2) + ", expected " + Hex((b << 16u) | (Garbage & 0xffffu)) + ": dst_sel:WORD_1 dst_unused:UNUSED_PRESERVE must keep the low half");
        Testing::Require(out[3] == (a | (b << 16u)), where(3) + ", expected " + Hex(a | (b << 16u)) + ": two padded conversions packed with v_lshl_add_u32");
        Testing::Require(out[4] == sum, where(4) + ", expected " + Hex(sum) + ": v_add_f16_sdwa dst_sel:DWORD dst_unused:UNUSED_PAD must zero the high half");
        const auto low = in[0] & 0xffu;
        Testing::Require(out[5] == ((Garbage & 0xffffff00u) | low), where(5) + ", expected " + Hex((Garbage & 0xffffff00u) | low) + ": v_mov_b32_sdwa dst_sel:BYTE_0 dst_unused:UNUSED_PRESERVE must keep the other bytes");
        Testing::Require(out[6] == ((Garbage & 0xff00ffffu) | (low << 16u)), where(6) + ", expected " + Hex((Garbage & 0xff00ffffu) | (low << 16u)) + ": v_mov_b32_sdwa dst_sel:BYTE_2 dst_unused:UNUSED_PRESERVE must keep the other bytes");
        Testing::Require(out[7] == (low << 8u), where(7) + ", expected " + Hex(low << 8u) + ": v_mov_b32_sdwa dst_sel:BYTE_1 dst_unused:UNUSED_PAD must zero the other bytes");
    }
}

const Testing::Case sdwaDestinationSelects{"SdwaDestination_DstSelAndDstUnusedForms_WriteExpectedBits", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
