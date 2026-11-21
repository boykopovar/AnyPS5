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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};
bool HardwareFlush = false;
bool FusedExactArithmetic = false;

alignas(256) constexpr std::array<std::uint32_t, 27> Code{
    0x34020084u, 0x34060085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd54b000au, 0x041a0b04u, 0x7e160306u,
    0x56160b04u, 0x58180d04u, 0x7f800000u, 0x581a0d05u, 0x00000001u, 0x5a1c0b04u, 0x7fc00001u, 0xd54b020fu,
    0x241a0b04u, 0x7e200306u, 0xd52b0010u, 0x00020b04u, 0xcc200011u, 0x041a0b04u, 0xe0781000u, 0x80010a03u,
    0xe0781010u, 0x80010e03u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][Inputs] = {
    {0x00000001u, 0x7f800000u, 0x7fc00001u, 0x00000000u},
    {0x807fffffu, 0x7f800000u, 0x7f812345u, 0x00000000u},
    {0x007fffffu, 0xff800000u, 0xffc12345u, 0x00000000u},
    {0x80000001u, 0xff800000u, 0xffa00001u, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x7fc00001u, 0x00000000u},
    {0xff800000u, 0x807fffffu, 0x7f812345u, 0x00000000u},
    {0x7f800000u, 0x007fffffu, 0xffc12345u, 0x00000000u},
    {0xff800000u, 0x80000001u, 0xffa00001u, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x3f800000u, 0x00000000u},
    {0x7f800000u, 0x807fffffu, 0xff800000u, 0x00000000u},
    {0xff800000u, 0x007fffffu, 0x00000000u, 0x00000000u},
    {0xff800000u, 0x00000001u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x7f800000u, 0x7fc00001u, 0x00000000u},
    {0xff800000u, 0x80000000u, 0x7f812345u, 0x00000000u},
    {0x00800000u, 0x7f800000u, 0x7fc00001u, 0x00000000u},
    {0x7f800000u, 0x80800000u, 0x7f812345u, 0x00000000u},
    {0x00000001u, 0x3f800000u, 0x7fc00001u, 0x00000000u},
    {0x3f800000u, 0x807fffffu, 0xffa00001u, 0x00000000u},
    {0x00000001u, 0x007fffffu, 0x7f812345u, 0x00000000u},
    {0x00000001u, 0x7fc00003u, 0x7f812345u, 0x00000000u},
    {0x7f812345u, 0x00000001u, 0x3f800000u, 0x00000000u},
    {0x7f800000u, 0xff800000u, 0x7fc00001u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x00000001u, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x3f800000u, 0x40000000u, 0x40400000u, 0x00000000u},
    {0x80000001u, 0x7f800000u, 0x7fc00001u, 0x00000000u},
    {0x00400000u, 0x7f800000u, 0xffa00001u, 0x00000000u},
    {0x7f800000u, 0x80400000u, 0x7fc00003u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x7f812345u, 0x00000000u},
    {0x3f800000u, 0x00000001u, 0x00800000u, 0x00000000u},
    {0xff800000u, 0x00000001u, 0xffc12345u, 0x00000000u},
    {0x7f800000u, 0x007fffffu, 0x7f812345u, 0x00000000u},
};

constexpr std::uint32_t Expected[2][Threads][Results] = {
    {
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7f812345u, 0x7f812345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc12345u, 0xffc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffa00001u, 0xffa00001u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7f800000u, 0x3f800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xff800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7f812345u, 0x7f812345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0x7f812345u, 0x7f812345u, 0x7f812345u, 0x7f812345u, 0x7fc00001u, 0x7f812345u, 0x7f812345u, 0x7f812345u},
        {0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0xffa00001u, 0xffa00001u, 0xffa00001u, 0xffa00001u, 0x7fc00001u, 0xffa00001u, 0xffa00001u, 0xffa00001u},
        {0x7f812345u, 0x7f812345u, 0xffc00000u, 0x7f812345u, 0x7fc00001u, 0x7f812345u, 0x7f812345u, 0x7f812345u},
        {0x7fc00003u, 0x7fc00003u, 0xffc00000u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u},
        {0x7f812345u, 0x7f812345u, 0x7f812345u, 0x3f800000u, 0x7f812345u, 0xff812345u, 0x7f812345u, 0x7f812345u},
        {0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0x3f800000u, 0x3f800000u, 0x7f800000u, 0x00000000u, 0x7fc00001u, 0xbf800000u, 0x3f800000u, 0x3f800000u},
        {0xffc00000u, 0xffc00000u, 0x7f800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x40a00000u, 0x40a00000u, 0x7f800000u, 0x40400000u, 0x7fc00001u, 0x3f800000u, 0x40a00000u, 0x40a00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc00003u, 0x7fc00003u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x7f812345u, 0x7f812345u, 0xffc00000u, 0x7f812345u, 0x7fc00001u, 0x7f812345u, 0x7f812345u, 0x7f812345u},
        {0x00800000u, 0x00800000u, 0x7f800000u, 0x00800000u, 0x7fc00001u, 0x00800000u, 0x00800000u, 0x00800000u},
        {0xffc00000u, 0xffc00000u, 0xffc12345u, 0xffc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7f812345u, 0x7f812345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
    },
    {
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc12345u, 0x7fc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc12345u, 0xffc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffe00001u, 0xffe00001u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7f800000u, 0x3f800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xff800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc12345u, 0x7fc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x7fc00001u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u},
        {0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0xffe00001u, 0xffe00001u, 0xffe00001u, 0xffe00001u, 0x7fc00001u, 0xffe00001u, 0xffe00001u, 0xffe00001u},
        {0x7fc12345u, 0x7fc12345u, 0xffc00000u, 0x7fc12345u, 0x7fc00001u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u},
        {0x7fc00003u, 0x7fc00003u, 0xffc00000u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u, 0x7fc00003u},
        {0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x3f800000u, 0x7fc12345u, 0xffc12345u, 0x7fc12345u, 0x7fc12345u},
        {0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0xffc00000u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u},
        {0x3f800000u, 0x3f800000u, 0x7f800000u, 0x00000000u, 0x7fc00001u, 0xbf800000u, 0x3f800000u, 0x3f800000u},
        {0xffc00000u, 0xffc00000u, 0x7f800000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x40a00000u, 0x40a00000u, 0x7f800000u, 0x40400000u, 0x7fc00001u, 0x3f800000u, 0x40a00000u, 0x40a00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc00003u, 0x7fc00003u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0x7fc12345u, 0x7fc12345u, 0xffc00000u, 0x7fc12345u, 0x7fc00001u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u},
        {0x00800000u, 0x00800000u, 0x7f800000u, 0x00800000u, 0x7fc00001u, 0x00800000u, 0x00800000u, 0x00800000u},
        {0xffc00000u, 0xffc00000u, 0xffc12345u, 0xffc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
        {0xffc00000u, 0xffc00000u, 0x7fc12345u, 0x7fc12345u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
    },
};

constexpr const char* Names[Results] = {
    "v_fma_f32 v10, v4, v5, v6",
    "v_fmac_f32 v11, v4, v5",
    "v_fmamk_f32 v12, v4, 0x7f800000, v6",
    "v_fmamk_f32 v13, v5, 0x00000001, v6",
    "v_fmaak_f32 v14, v4, v5, 0x7fc00001",
    "v_fma_f32 v15, -v4, |v5|, v6",
    "v_fmac_f32 v16, v4, v5 in VOP3",
    "v_fma_mix_f32 v17, v4, v5, v6",
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

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
    request.target.float32FlushToZero = HardwareFlush;
    request.target.fusedExactArithmetic = FusedExactArithmetic;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::uint32_t (&expected)[Threads][Results], const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < Results; ++index) {
            const std::uint32_t actual = Output[tid * Results + index];
            Require(actual == expected[tid][index], std::string("f32 fma flushed invalid product: ") + (HardwareFlush ? "hardware flush " : "emulated flush ") + (FusedExactArithmetic ? "fused " : "") + mode + " lane " +
                                                        std::to_string(tid) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected[tid][index]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t passes = 0u;
        for (const std::uint32_t variant : {0u, 1u, 2u, 3u}) {
            const bool flush = (variant & 1u) != 0u;
            if (flush && !device->Target().float32FlushToZero) continue;
            if ((variant & 2u) == 0u && device->Target().fusedExactArithmetic) continue;
            HardwareFlush = flush;
            FusedExactArithmetic = (variant & 2u) != 0u;
            ++passes;
            Run(*device, std::nullopt);
            Check(Expected[0], "no float mode");
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
            Check(Expected[0], "IEEE=0");
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false});
            Check(Expected[1], "IEEE=1");
        }
        std::printf("f32 fma flushed invalid product tests passed (%u of 4 passes run; the hardware flush passes need a device that uses the hardware f32 flush)\n", passes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
