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
bool FusedExactArithmetic = false;

alignas(256) constexpr std::array<std::uint32_t, 18> Code{
    0x34020084u, 0x34060085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x10140b04u, 0x06160d0au, 0x08180d0au,
    0x0a1a0d0au, 0x061c1506u, 0x081e1506u, 0x06200d04u, 0x10220b10u, 0xe0781000u, 0x80010a03u, 0xe0781010u,
    0x80010e03u, 0xbf810000u,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x3f800800u, 0x3f800c00u, 0xbf801401u, 0x00000000u},
    {0x3f801800u, 0x3f801400u, 0xbf802c04u, 0x00000000u},
    {0x3f802800u, 0x3f801c00u, 0xbf804409u, 0x00000000u},
    {0x3f803800u, 0x3f802400u, 0xbf805c10u, 0x00000000u},
    {0x3f804800u, 0x3f802c00u, 0xbf807419u, 0x00000000u},
    {0x3f805800u, 0x3f803400u, 0xbf808c24u, 0x00000000u},
    {0x3f806800u, 0x3f803c00u, 0xbf80a431u, 0x00000000u},
    {0x3f807800u, 0x3f804400u, 0xbf80bc40u, 0x00000000u},
    {0x3f808800u, 0x3f804c00u, 0xbf80d451u, 0x00000000u},
    {0x3f809800u, 0x3f805400u, 0xbf80ec64u, 0x00000000u},
    {0x3f80a800u, 0x3f805c00u, 0xbf810479u, 0x00000000u},
    {0x3f80b800u, 0x3f806400u, 0xbf811c90u, 0x00000000u},
    {0x3d801000u, 0xc1001800u, 0x3f002804u, 0x00000000u},
    {0x3e005000u, 0xc0803800u, 0x3f008822u, 0x00000000u},
    {0x3e809000u, 0xc0005800u, 0x3f00e864u, 0x00000000u},
    {0x3f00d000u, 0xbf807800u, 0x3f0148c2u, 0x00000000u},
    {0x3f811000u, 0xbf009800u, 0x3f01a944u, 0x00000000u},
    {0x40015000u, 0xbe80b800u, 0x3f0209e2u, 0x00000000u},
    {0x40819000u, 0xbe00d800u, 0x3f026aa4u, 0x00000000u},
    {0x4101d000u, 0xbd80f800u, 0x3f02cb82u, 0x00000000u},
    {0x40400000u, 0x3eaaaaabu, 0xbf800000u, 0x00000000u},
    {0x40540000u, 0x3eabaaabu, 0xc0000000u, 0x00000000u},
    {0x40680000u, 0x3eacaaabu, 0xc0400000u, 0x00000000u},
    {0x407c0000u, 0x3eadaaabu, 0xc0800000u, 0x00000000u},
    {0x40880000u, 0x3eaeaaabu, 0xc0a00000u, 0x00000000u},
    {0x40920000u, 0x3eafaaabu, 0xc0c00000u, 0x00000000u},
    {0x00000000u, 0xbf800000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x3f800000u, 0x80000000u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0xbf800000u, 0x00000000u},
    {0x4e800001u, 0x4e7ffffeu, 0xdd800000u, 0x00000000u},
    {0x3f800001u, 0x3f7fffffu, 0xbf800000u, 0x00000000u},
    {0xbfc00000u, 0x35800000u, 0x35000000u, 0x00000000u},
};
constexpr std::uint32_t Expected[32][8] = {
    {0x3f801401u, 0x00000000u, 0x40001401u, 0xc0001401u, 0x00000000u, 0xc0001401u, 0xb9c01000u, 0xb9c02202u},
    {0x3f802c04u, 0x00000000u, 0x40002c04u, 0xc0002c04u, 0x00000000u, 0xc0002c04u, 0xba202000u, 0xba203905u},
    {0x3f804409u, 0x00000000u, 0x40004409u, 0xc0004409u, 0x00000000u, 0xc0004409u, 0xba604800u, 0xba607910u},
    {0x3f805c10u, 0x00000000u, 0x40005c10u, 0xc0005c10u, 0x00000000u, 0xc0005c10u, 0xba904000u, 0xba906892u},
    {0x3f807419u, 0x00000000u, 0x40007419u, 0xc0007419u, 0x00000000u, 0xc0007419u, 0xbab06400u, 0xbab0a0a2u},
    {0x3f808c24u, 0x00000000u, 0x40008c24u, 0xc0008c24u, 0x00000000u, 0xc0008c24u, 0xbad09000u, 0xbad0e4bau},
    {0x3f80a431u, 0x00000000u, 0x4000a431u, 0xc000a431u, 0x00000000u, 0xc000a431u, 0xbaf0c400u, 0xbaf134dcu},
    {0x3f80bc40u, 0x00000000u, 0x4000bc40u, 0xc000bc40u, 0x00000000u, 0xc000bc40u, 0xbb088000u, 0xbb08c884u},
    {0x3f80d451u, 0x00000000u, 0x4000d451u, 0xc000d451u, 0x00000000u, 0xc000d451u, 0xbb18a200u, 0xbb18fca0u},
    {0x3f80ec64u, 0x00000000u, 0x4000ec64u, 0xc000ec64u, 0x00000000u, 0xc000ec64u, 0xbb28c800u, 0xbb2936c3u},
    {0x3f810479u, 0x00000000u, 0x40010479u, 0xc0010479u, 0x00000000u, 0xc0010479u, 0xbb38f200u, 0xbb3976eeu},
    {0x3f811c90u, 0x00000000u, 0x40011c90u, 0xc0011c90u, 0x00000000u, 0xc0011c90u, 0xbb492000u, 0xbb49bd21u},
    {0xbf002803u, 0x33800000u, 0xbf802804u, 0x3f802804u, 0x33800000u, 0x3f802804u, 0x3f102a04u, 0xc090450cu},
    {0xbf008823u, 0xb3800000u, 0xbf808822u, 0x3f808822u, 0xb3800000u, 0x3f808822u, 0x3f209c22u, 0xc020e266u},
    {0xbf00e863u, 0x33800000u, 0xbf80e864u, 0x3f80e864u, 0x33800000u, 0x3f80e864u, 0x3f413064u, 0xbfc1b535u},
    {0xbf0148c3u, 0xb3800000u, 0xbf8148c2u, 0x3f8148c2u, 0xb3800000u, 0x3f8148c2u, 0x3f810c61u, 0xbf81855du},
    {0xbf01a943u, 0x33800000u, 0xbf81a944u, 0x3f81a944u, 0x33800000u, 0x3f81a944u, 0x3fc1e4a2u, 0xbf42cae2u},
    {0xbf0209e3u, 0xb3800000u, 0xbf8209e2u, 0x3f8209e2u, 0xb3800000u, 0x3f8209e2u, 0x4021d278u, 0xbf22bb17u},
    {0xbf026aa3u, 0x33800000u, 0xbf826aa4u, 0x3f826aa4u, 0x33800000u, 0x3f826aa4u, 0x4091dd54u, 0xbf12d379u},
    {0xbf02cb83u, 0xb3800000u, 0xbf82cb82u, 0x3f82cb82u, 0xb3800000u, 0x3f82cb82u, 0x4109fcb8u, 0xbf0b0812u},
    {0x3f800000u, 0x00000000u, 0x40000000u, 0xc0000000u, 0x00000000u, 0xc0000000u, 0x40000000u, 0x3f2aaaabu},
    {0x3f8e2956u, 0xbf63ad54u, 0x404714abu, 0xc04714abu, 0xbf63ad54u, 0xc04714abu, 0x3fa80000u, 0x3ee15000u},
    {0x3f9c7aabu, 0xbfe38555u, 0x40871eabu, 0xc0871eabu, 0xbfe38555u, 0xc0871eabu, 0x3f200000u, 0x3e57d556u},
    {0x3faaf400u, 0xc02a8600u, 0x40aabd00u, 0xc0aabd00u, 0xc02a8600u, 0xc0aabd00u, 0xbd800000u, 0xbcadaaabu},
    {0x3fb99556u, 0xc0633555u, 0x40ce6556u, 0xc0ce6556u, 0xc0633555u, 0xc0ce6556u, 0xbf400000u, 0xbe830000u},
    {0x3fc85eabu, 0xc08de855u, 0x40f217abu, 0xc0f217abu, 0xc08de855u, 0xc0f217abu, 0xbfb80000u, 0xbefc8556u},
    {0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u},
    {0x3f800000u, 0x00000000u, 0x40000000u, 0xc0000000u, 0x00000000u, 0xc0000000u, 0x00000000u, 0x00000000u},
    {0x5d800000u, 0x00000000u, 0x5e000000u, 0xde000000u, 0x00000000u, 0xde000000u, 0xdd800000u, 0xec7ffffeu},
    {0x3f800000u, 0x00000000u, 0x40000000u, 0xc0000000u, 0x00000000u, 0xc0000000u, 0x34000000u, 0x33ffffffu},
    {0xb5c00000u, 0xb5800000u, 0xb6000000u, 0x36000000u, 0xb5800000u, 0x36000000u, 0xbfbffffcu, 0xb5bffffcu},
};

constexpr const char* Names[Results] = {
    "v_mul_f32 v10, v4, v5",
    "v_add_f32 v11, v10, v6",
    "v_sub_f32 v12, v10, v6",
    "v_subrev_f32 v13, v10, v6",
    "v_add_f32 v14, v6, v10",
    "v_sub_f32 v15, v6, v10",
    "v_add_f32 v16, v4, v6",
    "v_mul_f32 v17, v16, v5",
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
    request.target.fusedExactArithmetic = FusedExactArithmetic;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < Results; ++i) {
            const auto actual = Output[tid * Results + i];
            Require(actual == Expected[tid][i], std::string("f32 separate rounding: ") + (FusedExactArithmetic ? "fused " : "") + "lane " + std::to_string(tid) + " " + mode + " " + Names[i] + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][i]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool fused : {false, true}) {
            if (!fused && device->Target().fusedExactArithmetic) continue;
            FusedExactArithmetic = fused;
            Run(*device, std::nullopt);
            Check("no float mode");
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false});
            Check("IEEE=1");
        }
        std::puts("f32 separate rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
