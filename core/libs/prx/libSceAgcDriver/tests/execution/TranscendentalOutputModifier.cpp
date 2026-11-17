#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/FloatProbeTolerance.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Float32ProbeOp;
using AgcDriver::Float32ResultsAgree;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 24;
constexpr std::uint32_t Ops = 7;
constexpr std::uint32_t Results = 8;

using Table = std::array<std::array<std::uint32_t, Ops + 1u>, Threads>;

constexpr Table ModifierApplied{{
    {0x3f800000u, 0x40000000u, 0x40800000u, 0x3f000000u, 0x40800000u, 0x00000000u, 0x3f800000u, 0x40000000u},
    {0x40000000u, 0x3f800000u, 0x40000000u, 0x3eb504f3u, 0x40b504f3u, 0x40000000u, 0x40000000u, 0x40000000u},
    {0x40800000u, 0x3f000000u, 0x3f800000u, 0x3e800000u, 0x41000000u, 0x40800000u, 0x41000000u, 0x40000000u},
    {0x3e800000u, 0x41000000u, 0x41800000u, 0x3f800000u, 0x40000000u, 0xc0800000u, 0x3f1837f0u, 0x00000000u},
    {0x41000000u, 0x3e800000u, 0x3f000000u, 0x3e3504f3u, 0x413504f3u, 0x40c00000u, 0x43000000u, 0x40000000u},
    {0x41800000u, 0x3e000000u, 0x3e800000u, 0x3e000000u, 0x41800000u, 0x41000000u, 0x47000000u, 0x40000000u},
    {0x43800000u, 0x3c000000u, 0x3c800000u, 0x3d000000u, 0x42800000u, 0x41800000u, 0x7f800000u, 0x40000000u},
    {0x00000000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x00000000u, 0xff800000u, 0x3f000000u, 0x40000000u},
    {0x80000000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u, 0xff800000u, 0x3f000000u, 0x40000000u},
    {0xbf800000u, 0xc0000000u, 0xc0800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3e800000u, 0x40000000u},
    {0xc0800000u, 0xbf000000u, 0xbf800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3d000000u, 0x40000000u},
    {0xc0000000u, 0xbf800000u, 0xc0000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3e000000u, 0x40000000u},
    {0xc2fe0000u, 0xbc810204u, 0xbd010204u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x40000000u},
    {0x42800000u, 0x3d000000u, 0x3d800000u, 0x3d800000u, 0x42000000u, 0x41400000u, 0x5f000000u, 0x40000000u},
    {0x44800000u, 0x3b000000u, 0x3b800000u, 0x3c800000u, 0x43000000u, 0x41a00000u, 0x7f800000u, 0x40000000u},
    {0x41200000u, 0x3e4ccccdu, 0x3ecccccdu, 0x3e21e89bu, 0x414a62c2u, 0x40d49a78u, 0x44000000u, 0x40000000u},
    {0x42c80000u, 0x3ca3d70au, 0x3d23d70au, 0x3d4ccccdu, 0x42200000u, 0x41549a78u, 0x71000000u, 0x40000000u},
    {0x42000000u, 0x3d800000u, 0x3e000000u, 0x3db504f3u, 0x41b504f3u, 0x41200000u, 0x4f000000u, 0x40000000u},
    {0x7f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0xffc00000u},
    {0xff800000u, 0x00000000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x0d800000u, 0x72000000u, 0x72800000u, 0x58000000u, 0x27800000u, 0xc3480000u, 0x3f000000u, 0x40000000u},
    {0x71800000u, 0x0e000000u, 0x0e800000u, 0x26000000u, 0x59800000u, 0x43480000u, 0x7f800000u, 0x40000000u},
    {0x00000001u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x00000000u, 0xff800000u, 0x3f000000u, 0x40000000u},
}};

constexpr Table ModifierIgnored{{
    {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x40000000u, 0x3f800000u},
    {0x40000000u, 0x3f000000u, 0x3f000000u, 0x3f3504f3u, 0x3fb504f3u, 0x3f800000u, 0x40800000u, 0x3f800000u},
    {0x40800000u, 0x3e800000u, 0x3e800000u, 0x3f000000u, 0x40000000u, 0x40000000u, 0x41800000u, 0x3f800000u},
    {0x3e800000u, 0x40800000u, 0x40800000u, 0x40000000u, 0x3f000000u, 0xc0000000u, 0x3f9837f0u, 0x00000000u},
    {0x41000000u, 0x3e000000u, 0x3e000000u, 0x3eb504f3u, 0x403504f3u, 0x40400000u, 0x43800000u, 0x3f800000u},
    {0x41800000u, 0x3d800000u, 0x3d800000u, 0x3e800000u, 0x40800000u, 0x40800000u, 0x47800000u, 0x3f800000u},
    {0x43800000u, 0x3b800000u, 0x3b800000u, 0x3d800000u, 0x41800000u, 0x41000000u, 0x7f800000u, 0x3f800000u},
    {0x00000000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x00000000u, 0xff800000u, 0x3f800000u, 0x3f800000u},
    {0x80000000u, 0xff800000u, 0xff800000u, 0xff800000u, 0x80000000u, 0xff800000u, 0x3f800000u, 0x3f800000u},
    {0xbf800000u, 0xbf800000u, 0xbf800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3f000000u, 0x3f800000u},
    {0xc0800000u, 0xbe800000u, 0xbe800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3d800000u, 0x3f800000u},
    {0xc0000000u, 0xbf000000u, 0xbf000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x3e800000u, 0x3f800000u},
    {0xc2fe0000u, 0xbc010204u, 0xbc010204u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x3f800000u},
    {0x42800000u, 0x3c800000u, 0x3c800000u, 0x3e000000u, 0x41000000u, 0x40c00000u, 0x5f800000u, 0x3f800000u},
    {0x44800000u, 0x3a800000u, 0x3a800000u, 0x3d000000u, 0x42000000u, 0x41200000u, 0x7f800000u, 0x3f800000u},
    {0x41200000u, 0x3dcccccdu, 0x3dcccccdu, 0x3ea1e89bu, 0x404a62c2u, 0x40549a78u, 0x44800000u, 0x3f800000u},
    {0x42c80000u, 0x3c23d70au, 0x3c23d70au, 0x3dcccccdu, 0x41200000u, 0x40d49a78u, 0x71800000u, 0x3f800000u},
    {0x42000000u, 0x3d000000u, 0x3d000000u, 0x3e3504f3u, 0x40b504f3u, 0x40a00000u, 0x4f800000u, 0x3f800000u},
    {0x7f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0xffc00000u},
    {0xff800000u, 0x80000000u, 0x80000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x0d800000u, 0x71800000u, 0x71800000u, 0x58800000u, 0x26800000u, 0xc2c80000u, 0x3f800000u, 0x3f800000u},
    {0x71800000u, 0x0d800000u, 0x0d800000u, 0x26800000u, 0x58800000u, 0x42c80000u, 0x7f800000u, 0x3f800000u},
    {0x00000001u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x00000000u, 0xff800000u, 0x3f800000u, 0x3f800000u},
}};

constexpr const char* Names[Ops] = {"v_rcp_f32 mul:2", "v_rcp_iflag_f32 mul:4", "v_rsq_f32 div:2", "v_sqrt_f32 mul:4", "v_log_f32 mul:2", "v_exp_f32 div:2", "v_cos_f32 mul:2"};
constexpr std::array<double, Ops> Scales{2.0, 4.0, 0.5, 4.0, 2.0, 0.5, 2.0};
constexpr std::array<Float32ProbeOp, Ops - 1u> Probes{Float32ProbeOp::Rcp, Float32ProbeOp::Rcp, Float32ProbeOp::Rsq, Float32ProbeOp::Sqrt, Float32ProbeOp::Log2, Float32ProbeOp::Exp2};
constexpr std::uint32_t Cos = Ops - 1u;

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

bool Agrees(std::uint32_t op, std::uint32_t input, std::uint32_t wanted, std::uint32_t got, bool applied) {
    if (got == wanted) return true;
    if (IsNan(wanted) || IsNan(got)) return false;
    if (((wanted | got) & 0x7fffffffu) == 0u) return false;
    const double scale = applied ? Scales[op] : 1.0;
    const float expected = static_cast<float>(static_cast<double>(std::bit_cast<float>(wanted)) / scale);
    const float actual = static_cast<float>(static_cast<double>(std::bit_cast<float>(got)) / scale);
    if (!std::isfinite(expected) || !std::isfinite(actual)) return false;
    if (op == Cos) return std::fabs(static_cast<double>(expected) - static_cast<double>(actual)) <= std::ldexp(1.0, -11);
    return Float32ResultsAgree(Probes[op], input, std::bit_cast<std::uint32_t>(expected), std::bit_cast<std::uint32_t>(actual));
}

alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xd5aa0003u, 0x08000102u, 0xd5ab0004u, 0x10000102u,
    0xd5ae0005u, 0x18000102u, 0xd5b30006u, 0x10000102u, 0xd5a70007u, 0x08000102u, 0xd5a50008u, 0x18000102u,
    0xd5b60009u, 0x08000102u, 0x34140085u, 0xe0781000u, 0x8001030au, 0xe07c1010u, 0x8001070au, 0xbf8c3f70u,
    0xbf810000u,
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

void Run(AgcDriver::VulkanDevice& device, std::uint32_t floatMode, bool ieeeMode, const Table& measured, bool applied) {
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, !ieeeMode, ieeeMode, false};
    const auto result = ShaderRecompiler::Recompile(request);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = measured[tid][0];
    Output.fill(0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t op = 0; op < Ops; ++op) {
            const auto got = Output[tid * Results + op];
            const auto wanted = measured[tid][op + 1u];
            Require(Agrees(op, measured[tid][0], wanted, got, applied), std::string(Names[op]) + " of " + Hex(measured[tid][0]) + " with FLOAT_MODE " + Hex(floatMode) + " and IEEE " +
                                       (ieeeMode ? "on" : "off") + " is " + Hex(got) + ", expected the RDNA 2 value " + Hex(wanted) + " within the device's allowed error");
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 0xf0u, false, ModifierApplied, true);
        Run(*device, 0xc0u, false, ModifierApplied, true);
        Run(*device, 0xf0u, true, ModifierIgnored, false);
        std::puts("transcendental output modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
