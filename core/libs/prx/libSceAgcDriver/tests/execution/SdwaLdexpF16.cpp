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
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 50> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x76140af9, 0x06000604, 0x7e160307,
    0x76160af9, 0x00091504, 0x76180af9, 0x09120c04, 0x761a0af9, 0x02230004, 0x7e1c0307, 0x761c0af9,
    0x0b041104, 0x761e0af9, 0x04050a04, 0x76200af9, 0x05360304, 0x76220af9, 0x0d0d0604, 0x76240af9,
    0x0c060d04, 0x76260af9, 0x06042504, 0x7e280307, 0x76280af9, 0x05153404, 0x762a0af9, 0x0c262604,
    0x762c0af9, 0x06062c04, 0x762e0af9, 0x06044604, 0x7e300307, 0x76300af9, 0x0c15f504, 0x76320af9,
    0x050b0404, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030,
    0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x684e9968u, 0xff007c00u, 0x6f931b8au},
    {0x808003ffu, 0x0004fffcu, 0x00ff8080u, 0x7bff0004u},
    {0xfff0fff0u, 0x00108000u, 0x7bffbc00u, 0xa45c7559u},
    {0x7d22fe4du, 0x056329d8u, 0xd7096658u, 0x38008080u},
    {0x8bb3fddeu, 0x00ff0004u, 0x80800040u, 0xbc0003ffu},
    {0xfc7a7ca0u, 0x7c007f7fu, 0xef4b2407u, 0xcc04a29cu},
    {0x7d007d00u, 0x7b6150bfu, 0x6dcc8926u, 0x8000fffcu},
    {0x7e007e00u, 0x00103c00u, 0x3c00bc00u, 0xb9cb4fcfu},
    {0x7e097c3cu, 0x7c007d00u, 0x0080fc00u, 0x80808080u},
    {0x7eedf7dbu, 0x00000080u, 0x7bff3c00u, 0x8000fffcu},
    {0x7efc7d70u, 0xc2404efau, 0x03ff0000u, 0xf8e9b3fau},
    {0x7f7f7f7fu, 0x03ff3c00u, 0x0a0a0ef4u, 0x00043800u},
    {0x925cfc30u, 0xc2c739c1u, 0x7c00ffc0u, 0x3c00fff0u},
    {0xfcb6fe11u, 0x7bfffffcu, 0xfc000004u, 0x00ff0000u},
    {0xffc0ffc0u, 0x00400004u, 0x935ed482u, 0xfff07e00u},
    {0xfffcfffcu, 0x80807bffu, 0x7d000080u, 0x42e04aebu},
    {0x9308fc92u, 0x800003ffu, 0x8080fffcu, 0x00107d00u},
    {0x9759fcb4u, 0xea4692e5u, 0xffc04400u, 0xd16a9b24u},
    {0x9b437c27u, 0x00040001u, 0x00803800u, 0xf26a25eau},
    {0x9d757cb2u, 0x7d00fc00u, 0xfffc0080u, 0x03ff8000u},
    {0xa2207fd4u, 0x77acf39bu, 0x00000040u, 0x4321d11au},
    {0xa9957e8au, 0x784edcbcu, 0xc1986687u, 0x1673526du},
    {0xab057dadu, 0x03ff0000u, 0xfc00fff0u, 0xa912eb3au},
    {0xacdffc34u, 0x01c450c4u, 0x00007c00u, 0xff00fff0u},
    {0xb5317eabu, 0x6d3223e0u, 0xb2c8089au, 0x001000ffu},
    {0xb8987d0au, 0x3800fff0u, 0x28bc3cd4u, 0x7bfffff0u},
    {0xbca4fe29u, 0x6af4ea82u, 0x800000ffu, 0x409ee072u},
    {0xc242ffedu, 0x00107d00u, 0x45a37ba4u, 0x7f7f4400u},
    {0xc31cfca9u, 0x25115884u, 0xa6568fceu, 0x7d0003ffu},
    {0xc492fd96u, 0x330fbd35u, 0x00ff7d00u, 0x8248e011u},
    {0xcabaff3cu, 0x0000fc00u, 0x000400ffu, 0xbc007bffu},
    {0xcd717c3bu, 0x808000ffu, 0xffc003ffu, 0x6af2aa83u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00000000u, 0x00001b8au, 0xffff8000u, 0x00000000u, 0x6f93008au, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6f930000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00001b8au, 0x00000000u},
    {0x00000010u, 0x7c000004u, 0xffff8040u, 0x00000000u, 0x7bffff04u, 0x00080000u, 0xfe000000u, 0x00008800u, 0x00400000u, 0x00400000u, 0x7bff0800u, 0x00000040u, 0x00000040u, 0x00000040u, 0x00080004u, 0x0000ff80u},
    {0x00000000u, 0xffff7559u, 0xffff8000u, 0x000000f8u, 0xa45cf059u, 0xfff00000u, 0xf0000000u, 0x0000fff0u, 0xfff00000u, 0x00000000u, 0xa45c0000u, 0x00000000u, 0x00000000u, 0x0000fff0u, 0x00007559u, 0x0000ffffu},
    {0x00007c00u, 0xfffe8080u, 0xfffffc00u, 0x00000000u, 0x38004d80u, 0x00220000u, 0x4d000000u, 0x00007f22u, 0xfe4d0000u, 0x00000000u, 0x38000000u, 0x00000000u, 0x00000000u, 0x0000fe4du, 0x00008080u, 0x00007c00u},
    {0x00000af0u, 0xfffd03ffu, 0xffff80b3u, 0x00000000u, 0xbc00deffu, 0xffb30000u, 0xde000000u, 0x0000fc00u, 0xffde0000u, 0x00000000u, 0xbc003c00u, 0x00000000u, 0x00000000u, 0x0000ffdeu, 0x1bb303ffu, 0x0000ff8bu},
    {0x00007c00u, 0x7c00a29cu, 0xfffffc00u, 0x000000fcu, 0xcc04a09cu, 0x007a0000u, 0xa0000000u, 0x0000fe7au, 0x7ea00000u, 0x00000000u, 0xcc040000u, 0x00000000u, 0x00000000u, 0x00007ea0u, 0x0000a29cu, 0x0000fffcu},
    {0x00000000u, 0x7c00fffcu, 0xffff8000u, 0x00000000u, 0x800000fcu, 0x00000000u, 0x00000000u, 0x00007f00u, 0x7f000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00007f00u, 0x0000fffcu, 0x00007c00u},
    {0x00000000u, 0x007e4fcfu, 0xffff8000u, 0x000000e0u, 0xb9cb00cfu, 0x00000000u, 0x00000000u, 0x00007e00u, 0x7e000000u, 0x00000000u, 0xb9cb0000u, 0x00000000u, 0x00000000u, 0x00007e00u, 0x00004fcfu, 0x000037e0u},
    {0x00007c00u, 0x007c8080u, 0xfffffc00u, 0x0000007eu, 0x80803c80u, 0x00090000u, 0x3c000000u, 0x00007e09u, 0x7e3c0000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x00000000u, 0x00007e3cu, 0x00008080u, 0x00007c00u},
    {0x00007c00u, 0xfff7fffcu, 0xffff80edu, 0x0000007eu, 0x8000dbfcu, 0xffed0000u, 0xdb000000u, 0x00007eedu, 0xfc000000u, 0x00000000u, 0x80000000u, 0x00003c00u, 0x00000000u, 0x0000fc00u, 0x0000fffcu, 0x0000007eu},
    {0x00007c00u, 0x7c00b3fau, 0xfffffc00u, 0x00000000u, 0xf8e970fau, 0xfffc0000u, 0x70000000u, 0x00007efcu, 0x7f700000u, 0x00000000u, 0xf8e90000u, 0x00000000u, 0x00000000u, 0x00007f70u, 0x0000b3fau, 0x00000000u},
    {0x00007c00u, 0x007f3800u, 0xfffffc00u, 0x00000000u, 0x00047f00u, 0x007f0000u, 0x7f000000u, 0x00007f7fu, 0x7f7f0000u, 0x00000000u, 0x00040000u, 0x00000000u, 0x00000000u, 0x00007f7fu, 0x00003800u, 0x00007c00u},
    {0x00007c00u, 0xfffcfff0u, 0xfffffc00u, 0x00000000u, 0x3c0030f0u, 0x00000000u, 0x30000000u, 0x00008000u, 0xfe300000u, 0x00000000u, 0x3c000000u, 0x00000000u, 0x00000000u, 0x0000fe30u, 0x3c00fff0u, 0x0000ff92u},
    {0x00000001u, 0xfffe0000u, 0xffff805bu, 0x00000000u, 0x00ff1100u, 0xffb60000u, 0x11000000u, 0x0000feb6u, 0xfe110000u, 0x00000000u, 0x00ff0000u, 0x00000000u, 0x00000000u, 0x0000fe11u, 0x00000000u, 0x0000fffcu},
    {0x00000a00u, 0xffff7e00u, 0xffff80c0u, 0x00000000u, 0xfff0c000u, 0xffc00000u, 0xc0000000u, 0x0000ffc0u, 0xffc00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0x00000000u, 0x0000ffc0u, 0x00007e00u, 0x0000ffffu},
    {0x00007c00u, 0xffff4aebu, 0xfffffc00u, 0x00000000u, 0x42e0fcebu, 0xfffc0000u, 0xfc000000u, 0x0000fffcu, 0xfffc0000u, 0x00000000u, 0x42e00000u, 0x00000000u, 0x00000000u, 0x0000fffcu, 0x00004aebu, 0x0000ffffu},
    {0x00007c00u, 0xfffc7d00u, 0xffff8040u, 0x00000093u, 0x00109200u, 0x00000000u, 0x92000000u, 0x00008000u, 0xfe920000u, 0x00000000u, 0x00100000u, 0x00000000u, 0x00000000u, 0x0000fe92u, 0x3c007d00u, 0x0000ff93u},
    {0x00000000u, 0xfffc9b24u, 0xffff8000u, 0x00000000u, 0xd16ab424u, 0x00000000u, 0xb4000000u, 0x00008000u, 0xfeb40000u, 0x00000000u, 0xd16a0000u, 0x00000000u, 0x00000000u, 0x0000feb4u, 0x00009b24u, 0x0000ff97u},
    {0x0000004eu, 0x00f825eau, 0xffff8043u, 0x000000d8u, 0xf26a27eau, 0x00430000u, 0x27000000u, 0x0000ab43u, 0x7e270000u, 0x00000000u, 0xf26a2b43u, 0x00000000u, 0x00000000u, 0x00007e27u, 0x1f4325eau, 0x0000ff9bu},
    {0x00000000u, 0x007c8000u, 0xffff8007u, 0x0000009du, 0x03ffb200u, 0x00000000u, 0xb2000000u, 0x0000fc00u, 0x7eb20000u, 0x00000000u, 0x03ff3c00u, 0x00000000u, 0x00000000u, 0x00007eb2u, 0x00008000u, 0x0000ff9du},
    {0x00000000u, 0x7c00d11au, 0xffff8000u, 0x00000000u, 0x4321d41au, 0x00000000u, 0xd4000000u, 0x0000fc00u, 0x7fd40000u, 0x00000000u, 0x43213c00u, 0x00000000u, 0x00000000u, 0x00007fd4u, 0x0000d11au, 0x0000ffa2u},
    {0x00000000u, 0x7c00526du, 0xffff8000u, 0x00000000u, 0x16738a6du, 0x00000000u, 0x8a000000u, 0x0000fc00u, 0x7e8a0000u, 0x00000000u, 0x16733c00u, 0x00000000u, 0x00000000u, 0x00007e8au, 0x0000526du, 0x0000ffa9u},
    {0x000000adu, 0x007deb3au, 0xffff8005u, 0x00000000u, 0xa912ad3au, 0x00050000u, 0xad000000u, 0x0000fc00u, 0x7fad0000u, 0x00000000u, 0xa9123c00u, 0x00000000u, 0x00000000u, 0x00007fadu, 0x2b05eb3au, 0x0000ffabu},
    {0x00007c00u, 0xfffcfff0u, 0xfffffc00u, 0x00000000u, 0xff0034f0u, 0x00000000u, 0x34000000u, 0x0000fc00u, 0xfe340000u, 0x00000000u, 0xff003c00u, 0x00000000u, 0x00000000u, 0x0000fe34u, 0x3c00fff0u, 0x0000ffacu},
    {0x00007c00u, 0x7c0000ffu, 0xfffffc00u, 0x00000000u, 0x0010abffu, 0x00000000u, 0xab000000u, 0x0000fc00u, 0x7eab0000u, 0x00000000u, 0x00103c00u, 0x00000000u, 0x00000000u, 0x00007eabu, 0x3c0000ffu, 0x0000ffb5u},
    {0x00000000u, 0x7c00fff0u, 0xffff804cu, 0x000000b8u, 0x7bff0af0u, 0xff930000u, 0x0a000000u, 0x0000fc00u, 0x7f0a0000u, 0x00000000u, 0x7bff3c00u, 0x00000000u, 0x00000000u, 0x00007f0au, 0x0093fff0u, 0x0000ffb8u},
    {0x00000000u, 0xfffee072u, 0xffff8000u, 0x00000000u, 0x409e2972u, 0x00000000u, 0x29000000u, 0x0000fc00u, 0xfe290000u, 0x00000000u, 0x409e3c00u, 0x00000000u, 0x00000000u, 0x0000fe29u, 0x0000e072u, 0x0000ffbcu},
    {0x00007c00u, 0xffff4400u, 0xfffffc00u, 0x00000010u, 0x7f7fed00u, 0x00000000u, 0xed000000u, 0x0000fc00u, 0xffed0000u, 0x00000000u, 0x7f7f3c00u, 0x00000000u, 0x00000000u, 0x0000ffedu, 0x3c004400u, 0x0000ffc2u},
    {0x00007c00u, 0xfffc03ffu, 0xfffffc00u, 0x00000018u, 0x7d00a9ffu, 0x00000000u, 0xa9000000u, 0x0000fc00u, 0xfea90000u, 0x00000000u, 0x7d003c00u, 0x00000000u, 0x00000000u, 0x0000fea9u, 0x3c0003ffu, 0x0000ffc3u},
    {0x00000000u, 0xfffde011u, 0xffff8000u, 0x00000020u, 0x82489611u, 0x00000000u, 0x96000000u, 0x0000fc00u, 0xff960000u, 0x00000000u, 0x82483c00u, 0x00000000u, 0x00000000u, 0x0000ff96u, 0x0000e011u, 0x0000ffc4u},
    {0x00000000u, 0xffff7bffu, 0xffff800cu, 0x000000cau, 0xbc003cffu, 0x00000000u, 0x3c000000u, 0x0000cabau, 0xff3c0000u, 0x00000000u, 0xbc003c00u, 0x00000000u, 0x00000000u, 0x0000ff3cu, 0x00007bffu, 0x0000ffcau},
    {0x00007c00u, 0x7c00aa83u, 0xffff8071u, 0x00000000u, 0x6af23b83u, 0x00000000u, 0x3b000000u, 0x00008000u, 0x7e3b0000u, 0x00000000u, 0x6af20000u, 0x00000000u, 0x00000000u, 0x00007e3bu, 0x3c00aa83u, 0x0000ffcdu}
};
constexpr std::uint32_t Checked[32] = {0xffffu, 0x8008u, 0xfff7u, 0x7ffau, 0xfffau, 0xfff0u, 0x7ffdu, 0x7ff5u, 0x7ff0u, 0x7ff2u, 0xfff8u, 0x7ff8u, 0xfffau, 0xfffau, 0xfffau, 0xfffau, 0xfff2u, 0xffffu, 0xfff0u, 0xfff1u, 0xfffdu, 0xfffdu, 0xfff8u, 0xfffau, 0xfff8u, 0xbfd1u, 0xffffu, 0xfff2u, 0xfff2u, 0xfff7u, 0xfff3u, 0xfff8u};
constexpr const char* Names[16] = {
    "BYTE_0/DWORD -> DWORD",
    "BYTE_1/BYTE_0 -> WORD_1",
    "BYTE_2/BYTE_1 -> WORD_0",
    "BYTE_3/BYTE_2 -> BYTE_0",
    "WORD_0/BYTE_3 -> BYTE_1",
    "WORD_1/WORD_0 -> BYTE_2",
    "DWORD/WORD_1 -> BYTE_3",
    "WORD_1/WORD_1 -> DWORD",
    "DWORD/WORD_0 -> WORD_1",
    "WORD_0/DWORD -> WORD_1 clamp",
    "WORD_1/WORD_1 -> WORD_0 clamp",
    "DWORD/WORD_0 -> DWORD clamp",
    "DWORD/DWORD -> WORD_0 clamp",
    "WORD_0/DWORD -> DWORD mul:2",
    "WORD_1/WORD_0 -> WORD_1 clamp div:2",
    "BYTE_3/WORD_1 -> WORD_0",
};

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

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Testing::Require(actual == expected, std::string("sdwa ldexp f16: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
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
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false};
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    const std::span<const std::uint32_t> code(Code);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

template<typename TAction>
std::string RequireRefusal(TAction action, const std::string& message) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    Testing::Fail(message);
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::uint32_t modifier, std::string_view expected) {
    alignas(256) const std::array<std::uint32_t, 3> code{0x76140af9u, modifier, 0xbf810000u};
    const auto refusal = RequireRefusal([&] { static_cast<void>(Compile(device, code)); }, "v_ldexp_f16 SDWA form " + Hex(modifier) + " was not refused");
    Testing::Require(refusal.find(expected) != std::string::npos, "v_ldexp_f16 SDWA form " + Hex(modifier) + " was refused for another reason: " + refusal);
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 16; ++i) {
            if ((Checked[tid] >> i) & 1u) {
                Expect(tid, out[i], Expected[tid][i], Names[i]);
            }
        }
    }
}

const Testing::Case ldexpDispatch{"SdwaLdexpF16_SelectorClampAndOmodForms_MatchExpectedTable", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

const Testing::Case refuseClampByteDestination{"SdwaLdexpF16_ClampWithByteDestination_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0x04042004u, "VOP2 SDWA clamp with byte selectors is not implemented");
}};

const Testing::Case refuseClampByteSource0{"SdwaLdexpF16_ClampWithByteSource0_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0x06022504u, "VOP2 SDWA clamp with byte selectors is not implemented");
}};

const Testing::Case refuseClampByteSource1{"SdwaLdexpF16_ClampWithByteSource1_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0x01062604u, "VOP2 SDWA clamp with byte selectors is not implemented");
}};

const Testing::Case refuseSource1Negate{"SdwaLdexpF16_Source1Negate_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0x10050404u, "VOP2 SDWA source modifiers are not supported");
}};

const Testing::Case refuseSource1Absolute{"SdwaLdexpF16_Source1Absolute_IsRefused", [] {
    RequireRefused(SharedVulkanTestDevice(), 0x26060604u, "VOP2 SDWA source modifiers are not supported");
}};

} // namespace
