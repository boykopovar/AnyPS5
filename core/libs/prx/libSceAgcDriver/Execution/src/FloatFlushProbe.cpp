#include "prx/libSceAgcDriver/Execution/include/FloatFlushProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver {
namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 19;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 46> MixedCode{
    0x34020084u, 0x160600ffu, 0x0000004cu, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x06140b04u, 0x08160905u,
    0x10180b04u, 0x0e1a0b04u, 0xd54b000eu, 0x041a0b04u, 0x7e1e0306u, 0x561e0b04u, 0x1e200b04u, 0xd5540011u,
    0x041a0b04u, 0xd5570012u, 0x041a0b04u, 0x7e264904u, 0x7e284505u, 0x7e2a8104u, 0x7c020b04u, 0xd5010016u,
    0x01a90280u, 0x7e2e1b04u, 0x7e307f05u, 0xd5080119u, 0x42020b04u, 0xd55f001au, 0x041a0b04u, 0xd550001bu,
    0x041a0b04u, 0xd550001cu, 0x03ca0b04u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u,
    0x80011203u, 0xe0781030u, 0x80011603u, 0xe07c1040u, 0x80011a03u, 0xbf810000u,
};

constexpr std::uint32_t MixedRows[Threads][Inputs] = {
    {0x00000001u, 0x3f800000u, 0x00000000u, 0x00000000u},
    {0x80400000u, 0x3f800000u, 0x00800000u, 0x00000000u},
    {0x3f800000u, 0x00012345u, 0x80000001u, 0x00000000u},
    {0x007fffffu, 0x007fffffu, 0x3f800000u, 0x00000000u},
    {0x3f7fffffu, 0x00800000u, 0x00000000u, 0x00000000u},
    {0xbf7fffffu, 0x00800000u, 0x80000000u, 0x00000000u},
    {0x3f7fffffu, 0x00800001u, 0x00000001u, 0x00000000u},
    {0x3f000000u, 0x01000000u, 0x80000001u, 0x00000000u},
    {0x00800000u, 0x80800001u, 0x00800000u, 0x00000000u},
    {0x00800001u, 0x00800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x80000001u, 0x00000001u, 0x00000000u},
    {0x80000000u, 0x00000001u, 0x807fffffu, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x3f800000u, 0x00000000u},
    {0x00000001u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x7fc00000u, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x40000000u, 0x00400000u, 0x80800000u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x80000001u, 0x00000000u},
    {0xbf800000u, 0x00000001u, 0x3f000000u, 0x00000000u},
    {0x34000000u, 0x34000000u, 0x00000000u, 0x00000000u},
    {0x1f800000u, 0x1f800000u, 0x00000000u, 0x00000000u},
    {0x1fffffffu, 0x1fffffffu, 0x00800000u, 0x00000000u},
    {0x20000000u, 0x1f000000u, 0x00000000u, 0x00000000u},
    {0x20000000u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0x20000001u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0xa0000000u, 0x1f7fffffu, 0x80000000u, 0x00000000u},
    {0x3f800000u, 0xbf800000u, 0x00000001u, 0x00000000u},
    {0x00800000u, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000001u, 0x00000000u},
    {0x41200000u, 0x00000003u, 0x3dcccccdu, 0x00000000u},
    {0xc0490fdbu, 0x80000002u, 0x40490fdbu, 0x00000000u},
    {0x80000001u, 0x80000001u, 0x80000001u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u}
};

alignas(256) constexpr std::array<std::uint32_t, 30> TinyCode{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x10140b04u, 0x0e160b04u, 0xd54b000cu,
    0x041a0b04u, 0x7e1a0306u, 0x561a0b04u, 0x581c0d04u, 0x3f7ffffeu, 0x5a1e0b04u, 0x80000000u, 0xcc200010u,
    0x041a0b04u, 0xd5080011u, 0x22020b04u, 0xd5400012u, 0x041a0b04u, 0x7e260306u, 0x0c260b04u, 0xe0781000u,
    0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0741020u, 0x80011202u, 0xbf810000u,
};

constexpr std::uint32_t TinyRows[Threads][Inputs] = {
    {0x00800001u, 0x3f7ffffeu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000000u, 0x00000000u},
    {0x3f7ffffeu, 0x00800001u, 0x00000000u, 0x00000000u},
    {0x008003e6u, 0x3f7ff834u, 0x00000000u, 0x00000000u},
    {0x00f80000u, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80f80000u, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00800f95u, 0x3f7fe0d9u, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffdu, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x008005a8u, 0x3f7ff4b0u, 0x00000000u, 0x00000000u},
    {0x00f7ffffu, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x3effffffu, 0x00000000u, 0x00000000u},
    {0x008005a9u, 0x3f7ff4aeu, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffeu, 0x00000001u, 0x00000000u},
    {0x007fffffu, 0x3f800001u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffeu, 0x00000000u, 0x00000000u},
    {0xbf7ffffeu, 0x00800001u, 0x80000000u, 0x00000000u},
    {0x808003e6u, 0x3f7ff834u, 0x80000000u, 0x00000000u},
    {0x80f80000u, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00f80000u, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80800f95u, 0x3f7fe0d9u, 0x80000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffdu, 0x80000000u, 0x00000000u},
    {0x80ffffffu, 0x3f000000u, 0x80000000u, 0x00000000u},
    {0x808005a8u, 0x3f7ff4b0u, 0x80000000u, 0x00000000u},
    {0x80f7ffffu, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80ffffffu, 0x3effffffu, 0x80000000u, 0x00000000u},
    {0x808005a9u, 0x3f7ff4aeu, 0x80000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000001u, 0x00000000u},
    {0x807fffffu, 0x3f800001u, 0x80000000u, 0x00000000u},
    {0x80800000u, 0x3f7fffffu, 0x80000000u, 0x00000000u}
};

alignas(256) constexpr std::array<std::uint32_t, 28> MixedTinyCode{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xcc20000au, 0x141a0b04u, 0xcc20000bu,
    0x0c1a0905u, 0xcc20100cu, 0x141a0b04u, 0xcc20080du, 0x0c1a0905u, 0xcc20400eu, 0x141e0b04u, 0xcc20400fu,
    0x0c1e0905u, 0x7e1002ffu, 0x3f7ffffeu, 0xcc204010u, 0x041e1104u, 0xcc206011u, 0x041e1104u, 0xe0781000u,
    0x80010a02u, 0xe0781010u, 0x80010e02u, 0xbf810000u,
};

constexpr std::uint32_t MixedTinyRows[Threads][Inputs] = {
    {0x00801002u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x80801002u, 0x00003bffu, 0x80000000u, 0x80008000u},
    {0x00801000u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801001u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801002u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801003u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x80801000u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801001u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801002u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801003u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x00801000u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801001u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801002u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801003u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00800001u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
};

alignas(256) constexpr std::array<std::uint32_t, 18> TranscendentalCode{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e144f04u, 0x7e166704u, 0x7e185d04u,
    0x7e1a5504u, 0x7e1c4b05u, 0x7e1e4f05u, 0x7e206705u, 0x7e225505u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xbf810000u,
};

constexpr std::uint32_t TranscendentalRows[Threads][Inputs] = {
    {0x40000000u, 0x3f800000u, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0xbf800000u, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x40400000u, 0xbf000000u, 0x00000000u, 0x00000000u},
    {0x41200000u, 0x41200000u, 0x00000000u, 0x00000000u},
    {0x3dcccccdu, 0xc1200000u, 0x00000000u, 0x00000000u},
    {0x0da24260u, 0x42480000u, 0x00000000u, 0x00000000u},
    {0x7149f2cau, 0xc2480000u, 0x00000000u, 0x00000000u},
    {0x3fc00000u, 0x42c80000u, 0x00000000u, 0x00000000u},
    {0x40e00000u, 0xc2c80000u, 0x00000000u, 0x00000000u},
    {0x3f400000u, 0x40000000u, 0x00000000u, 0x00000000u},
    {0x42c80000u, 0x40400000u, 0x00000000u, 0x00000000u},
    {0x2edbe6ffu, 0x3dcccccdu, 0x00000000u, 0x00000000u},
    {0x501502f9u, 0xbdcccccdu, 0x00000000u, 0x00000000u},
    {0x40490fdbu, 0x40e00000u, 0x00000000u, 0x00000000u},
    {0x402df854u, 0xc0e00000u, 0x00000000u, 0x00000000u},
    {0x3f800001u, 0x3a83126fu, 0x00000000u, 0x00000000u},
    {0x3f7fffffu, 0x41a00000u, 0x00000000u, 0x00000000u},
    {0x0d800000u, 0xc1a00000u, 0x00000000u, 0x00000000u},
    {0x71800000u, 0x3fc00000u, 0x00000000u, 0x00000000u},
    {0x3e99999au, 0xbfc00000u, 0x00000000u, 0x00000000u},
    {0x40a00000u, 0x41f00000u, 0x00000000u, 0x00000000u},
    {0x42800000u, 0xc1f00000u, 0x00000000u, 0x00000000u},
    {0x3c800000u, 0x42800000u, 0x00000000u, 0x00000000u},
    {0x3f8ccccdu, 0x3e800000u, 0x00000000u, 0x00000000u},
    {0x3f666666u, 0x40800000u, 0x00000000u, 0x00000000u},
    {0x60ad78ecu, 0x40a00000u, 0x00000000u, 0x00000000u},
    {0x1e3ce508u, 0x40490fdbu, 0x00000000u, 0x00000000u},
    {0x4640e6b7u, 0x402df854u, 0x00000000u, 0x00000000u},
    {0x3a83126fu, 0x3e99999au, 0x00000000u, 0x00000000u},
    {0x40800000u, 0x3f666666u, 0x00000000u, 0x00000000u},
    {0x3e800000u, 0x3f8ccccdu, 0x00000000u, 0x00000000u}
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

std::vector<std::uint32_t> Run(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, std::span<const std::uint32_t> code, const std::uint32_t (*rows)[Inputs]) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(rows[tid]), std::end(rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return {Output.begin(), Output.end()};
}

void Compare(FloatFlushProbeResult& result, const std::vector<std::uint32_t>& expected, const std::vector<std::uint32_t>& actual) {
    for (std::size_t index = 0; index < expected.size(); ++index) {
        ++result.compared;
        if (expected[index] != actual[index] && !(IsNan(expected[index]) && IsNan(actual[index]))) ++result.mismatches;
    }
}

}

FloatFlushProbeResult ProbeFloat32FlushToZero(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    FloatFlushProbeResult result;
    const std::array<std::optional<ShaderRecompiler::ShaderFloatMode>, 3> modes{std::nullopt, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false}};
    const std::array<std::pair<std::span<const std::uint32_t>, const std::uint32_t (*)[Inputs]>, 3> programs{{{MixedCode, MixedRows}, {TinyCode, TinyRows}, {MixedTinyCode, MixedTinyRows}}};
    for (const auto& mode : modes) {
        for (const auto& [code, rows] : programs) {
            auto software = target;
            software.float32FlushToZero = false;
            auto hardware = target;
            hardware.float32FlushToZero = true;
            Compare(result, Run(device, software, mode, code, rows), Run(device, hardware, mode, code, rows));
        }
    }
    return result;
}

FloatFlushProbeResult ProbeFloat32DenormPreserve(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    FloatFlushProbeResult result;
    auto preserved = target;
    preserved.float32FlushToZero = false;
    preserved.float32DenormPreserve = true;
    auto unpreserved = preserved;
    unpreserved.float32DenormPreserve = false;
    Compare(result, Run(device, unpreserved, std::nullopt, TranscendentalCode, TranscendentalRows), Run(device, preserved, std::nullopt, TranscendentalCode, TranscendentalRows));
    return result;
}

}
