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

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t RowCount = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e140306u, 0xd52b000au, 0x042a0b04u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

struct Row {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t accumulator;
};

constexpr Row Rows[RowCount] = {
    {0x3f800000u, 0x40000000u, 0x3f800000u},
    {0x3f800000u, 0x40000000u, 0xbf800000u},
    {0x40000000u, 0x40400000u, 0xc0c00000u},
    {0x40000000u, 0xc0400000u, 0xc0c00000u},
    {0x00000000u, 0xbf800000u, 0x00000000u},
    {0x00000000u, 0xbf800000u, 0x80000000u},
    {0x3f800000u, 0x3f800000u, 0x7f800000u},
    {0x3f800000u, 0x3f800000u, 0xff800000u},
    {0x7f800000u, 0x3f800000u, 0xff800000u},
    {0x3f800000u, 0x3f800000u, 0x7fc00001u},
    {0x3f800000u, 0x3f800000u, 0xffa00000u},
    {0x3f800000u, 0x3f800000u, 0x00000001u},
    {0x00800000u, 0x3f800000u, 0x80800001u},
    {0x40400000u, 0x3eaaaaabu, 0xbf800000u},
    {0x7f7fffffu, 0x40000000u, 0xff7fffffu},
    {0xbf800000u, 0x40000000u, 0x40000000u},
};

constexpr std::uint32_t ExpectedM1Plain[RowCount] = {
    0x40400000u, 0x3f800000u, 0x00000000u, 0xc1400000u,
    0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u,
    0xffc00000u, 0x7fc00001u, 0xffe00000u, 0x3f800000u,
    0x80000001u, 0x33000000u, 0x7f7fffffu, 0x00000000u,
};

constexpr std::uint32_t ExpectedM1Neg2[RowCount] = {
    0x3f800000u, 0x40400000u, 0x41400000u, 0x00000000u,
    0x80000000u, 0x00000000u, 0xff800000u, 0x7f800000u,
    0x7f800000u, 0xffc00001u, 0x7fe00000u, 0x3f800000u,
    0x01000000u, 0x40000000u, 0x7f800000u, 0xc0800000u,
};

constexpr std::uint32_t ExpectedM1Abs2[RowCount] = {
    0x40400000u, 0x40400000u, 0x41400000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x7f800000u, 0x7f800000u,
    0x7f800000u, 0x7fc00001u, 0x7fe00000u, 0x3f800000u,
    0x01000000u, 0x40000000u, 0x7f800000u, 0x00000000u,
};

constexpr std::uint32_t ExpectedM2Plain[RowCount] = {
    0x40400000u, 0x3f800000u, 0x00000000u, 0xc1400000u,
    0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u,
    0xffc00000u, 0x7fc00001u, 0xffa00000u, 0x3f800000u,
    0x80000000u, 0x33000000u, 0x7f7fffffu, 0x00000000u,
};

constexpr std::uint32_t ExpectedM2Neg2[RowCount] = {
    0x3f800000u, 0x40400000u, 0x41400000u, 0x00000000u,
    0x80000000u, 0x00000000u, 0xff800000u, 0x7f800000u,
    0x7f800000u, 0xffc00001u, 0x7fa00000u, 0x3f800000u,
    0x01000000u, 0x40000000u, 0x7f800000u, 0xc0800000u,
};

constexpr std::uint32_t ExpectedM2Abs2[RowCount] = {
    0x40400000u, 0x40400000u, 0x41400000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x7f800000u, 0x7f800000u,
    0x7f800000u, 0x7fc00001u, 0x7fa00000u, 0x3f800000u,
    0x01000000u, 0x40000000u, 0x7f800000u, 0x00000000u,
};

struct SplitRow {
    std::uint32_t row;
    std::uint32_t separate;
};

constexpr SplitRow SplitRows[] = {
    {13u, 0x00000000u},
    {14u, 0x7f800000u},
};

struct Variant {
    const char* name;
    std::uint32_t abs;
    std::uint32_t neg;
    const std::uint32_t* expectedM1;
    const std::uint32_t* expectedM2;
};

constexpr Variant Variants[] = {
    {"plain", 0u, 0u, ExpectedM1Plain, ExpectedM2Plain},
    {"-acc", 0u, 4u, ExpectedM1Neg2, ExpectedM2Neg2},
    {"|acc|", 4u, 0u, ExpectedM1Abs2, ExpectedM2Abs2},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsDenormal32(std::uint32_t bits) {
    return (bits & 0x7f800000u) == 0u && (bits & 0x007fffffu) != 0u;
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::ShaderFloatMode& floatMode, const Variant& variant) {
    Code[6] = 0xd52b000au | variant.abs << 8u;
    Code[7] = 0x042a0b04u | variant.neg << 29u;
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        Input[tid * Inputs] = Rows[tid].a;
        Input[tid * Inputs + 1u] = Rows[tid].b;
        Input[tid * Inputs + 2u] = Rows[tid].accumulator;
    }
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
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Check(const char* mode, const Variant& variant, const std::uint32_t* expected, bool skipDenormals, bool hostFuses) {
    std::uint32_t skipped = 0u;
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        const Row& row = Rows[tid];
        if (skipDenormals && (IsDenormal32(row.a) || IsDenormal32(row.b) || IsDenormal32(row.accumulator) || IsDenormal32(expected[tid]))) {
            ++skipped;
            continue;
        }
        std::uint32_t wanted = expected[tid];
        if (!hostFuses && variant.abs == 0u && variant.neg == 0u) {
            for (const SplitRow& split : SplitRows) {
                if (split.row == tid) wanted = split.separate;
            }
        }
        const std::uint32_t actual = Output[tid * Results];
        Require(actual == wanted, std::string("v_fmac_f32 ") + variant.name + " " + mode + ": lane " + std::to_string(tid) + " of " +
                                      Hex(Rows[tid].a) + " * " + Hex(Rows[tid].b) + " + " + Hex(Rows[tid].accumulator) + " is " + Hex(actual) +
                                      ", expected " + Hex(wanted));
    }
    return skipped;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool keepsDenormals = device->Target().float32DenormPreserve;
        std::uint32_t skipped = 0u;
        std::optional<bool> hostFuses;
        for (const Variant& variant : Variants) {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, variant);
            if (!hostFuses.has_value()) {
                hostFuses = Output[SplitRows[0].row * Results] == variant.expectedM1[SplitRows[0].row];
                if (!*hostFuses) std::puts("the device splits fma into a multiply and an add, so plain rows 13 and 14 are checked against the separately rounded results");
            }
            skipped += Check("M1", variant, variant.expectedM1, !keepsDenormals, *hostFuses);
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, variant);
            Check("M2", variant, variant.expectedM2, false, *hostFuses);
        }
        std::printf("v_fmac_f32 accumulator modifiers tests passed (%u M1 lanes with an f32 denormal not checked; they need DenormPreserve 32)\n", skipped);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
