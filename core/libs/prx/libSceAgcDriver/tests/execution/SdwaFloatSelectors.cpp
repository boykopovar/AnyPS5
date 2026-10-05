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

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 50> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x06140af9, 0x06000604, 0x7e160307,
    0x08160af9, 0x05061504, 0x0a180af9, 0x06142a04, 0x101a0af9, 0x03260404, 0x0e1c0af9, 0x06010604,
    0x1e1e0af9, 0x160a0604, 0x7e200307, 0x20200af9, 0x08061104, 0x20220af9, 0x060d2604, 0x64240af9,
    0x05010004, 0x66260af9, 0x02150d04, 0x7e280307, 0x68280af9, 0x24031304, 0x6a2a0af9, 0x00040604,
    0x7e2c0307, 0x742c0af9, 0x05081404, 0x722e0af9, 0x19050204, 0x06300af9, 0x06052b04, 0x6a320af9,
    0x06020904, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030,
    0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x00000000u, 0x40404040u, 0x8b2af49fu},
    {0x00000000u, 0x10663f9du, 0x8b35e743u, 0x7d7d9db4u},
    {0x0008dbf2u, 0x47004700u, 0x3c000000u, 0x80000000u},
    {0x05c21fabu, 0x00470047u, 0x80000000u, 0x47004700u},
    {0x1a0e7cdeu, 0x00000000u, 0xefc2742bu, 0xcd8d3db8u},
    {0xc4de998au, 0xaf1fba34u, 0x0000bc00u, 0xbc00bc00u},
    {0x00000000u, 0x00470047u, 0xc2c2c2c2u, 0xfda0095du},
    {0x3c000000u, 0x80808080u, 0x5960b098u, 0xadd5eff7u},
    {0xbc00bc00u, 0x00000000u, 0x42a502c0u, 0x3c000000u},
    {0x80808080u, 0x1a64a58eu, 0xfe242714u, 0x3c003c00u},
    {0x00000000u, 0x7c007c00u, 0x7f7f7f7fu, 0x3c003c00u},
    {0x00470047u, 0x00000000u, 0x3468787du, 0x7e7103e1u},
    {0x00470047u, 0x80000000u, 0x702bd672u, 0x47004700u},
    {0x00000000u, 0x00800080u, 0x4659ff82u, 0xc2c2c2c2u},
    {0x7f800000u, 0x00000000u, 0x01000100u, 0x1b1c3f27u},
    {0x80000000u, 0x00800080u, 0xbf800000u, 0x829ea49cu},
    {0x00000000u, 0x0000bc00u, 0x3343e632u, 0x13e60ef5u},
    {0x00000000u, 0x00470047u, 0xb5a54b0bu, 0x6769ec45u},
    {0x00000000u, 0x00800080u, 0xa2e7068bu, 0x871c3c9cu},
    {0x00000000u, 0x00800080u, 0xafe5c4b5u, 0x3c003c00u},
    {0x00000000u, 0x00ff00ffu, 0x0000bc00u, 0x40404040u},
    {0x00000000u, 0x00ff00ffu, 0x7f800000u, 0x00ff00ffu},
    {0x00000000u, 0x00ff00ffu, 0xb4ad5866u, 0xe4ea7accu},
    {0x00000000u, 0x00ff00ffu, 0xd03c7dbeu, 0x5de25876u},
    {0x00000000u, 0x01000100u, 0x52eff50fu, 0x00000000u},
    {0x00000000u, 0x182f8e4au, 0xe117e1fau, 0x0000bc00u},
    {0x00000000u, 0x20d8e2bcu, 0xe73074b1u, 0xbf800000u},
    {0x00000000u, 0x2333ad9cu, 0x00800080u, 0x3f800000u},
    {0x00000000u, 0x24f70eabu, 0xfdb88e95u, 0x17e6487au},
    {0x00000000u, 0x2a778095u, 0x00470047u, 0x7c007c00u},
    {0x00000000u, 0x2b119b05u, 0x0000bc00u, 0x4ce5144fu},
    {0x00000000u, 0x2b5d55fdu, 0x03f55439u, 0x7f7f7f7fu}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00000000u, 0x0000f49fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x8b2a009fu, 0x00000000u, 0x00000000u, 0x80000000u, 0x002af49fu, 0x00000000u, 0x8b2a0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x10663f9du, 0x10669db4u, 0xff9d0000u, 0x00000000u, 0x00000000u, 0x90663f9du, 0x7d7d00b4u, 0x10663f9du, 0x00000066u, 0x80660000u, 0x9d7d9db4u, 0x00000000u, 0x7d7d0000u, 0x00000000u, 0x9d000000u, 0x00000000u},
    {0x47004700u, 0x94f20000u, 0x00000000u, 0x00000000u, 0x006dbcbdu, 0xc7004700u, 0x8000f200u, 0x3f800000u, 0x00000000u, 0x80080000u, 0x00000000u, 0x00008000u, 0x80004700u, 0x00080000u, 0x00000000u, 0x00003800u},
    {0x004700f2u, 0x1fab4700u, 0xfff20000u, 0x00000000u, 0x00000000u, 0x80470047u, 0x4700ab00u, 0x00470047u, 0x00000066u, 0x86090000u, 0x42004700u, 0x00000001u, 0x47000047u, 0x00c20000u, 0x09000000u, 0x00000000u},
    {0x000000deu, 0x7cde3db8u, 0xffde0000u, 0x00000000u, 0x00000000u, 0x80000000u, 0xcd8ddeb8u, 0x00001a0eu, 0x0000007cu, 0x9a0e0000u, 0x1a8d3db8u, 0x00007edeu, 0xcd8d0000u, 0x000e0000u, 0x0e000000u, 0x00000000u},
    {0xaf1fba34u, 0x998abc00u, 0x00000000u, 0x0000c158u, 0x80000000u, 0x2f1fba34u, 0xbc003400u, 0x00000000u, 0x0000001fu, 0x44de0000u, 0x3400bc00u, 0x00008000u, 0xbc00af1fu, 0x00de0000u, 0x00000000u, 0xffffac00u},
    {0x00470047u, 0x0047095du, 0x00470000u, 0x00000000u, 0x00000000u, 0x80470047u, 0xfda0475du, 0x00470047u, 0x00000047u, 0x80470000u, 0x47a0095du, 0x00000000u, 0xfda00000u, 0x00000000u, 0x47000000u, 0x00000000u},
    {0x80808080u, 0x0000eff7u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0xadd500f7u, 0x00003c00u, 0x00000080u, 0xbc000000u, 0x44d5eff7u, 0x00000000u, 0xadd58080u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xbc000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x3c000000u, 0x00000000u, 0x000000bcu, 0x3c000000u, 0xbc000000u, 0x00008000u, 0x3c000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x1a64a58eu, 0x9ae43c00u, 0xff8e0000u, 0x00000000u, 0x00000000u, 0x9a64a58eu, 0x3c008000u, 0x1a64a58eu, 0x00000068u, 0x001c0000u, 0x8e003c00u, 0x00008000u, 0x3c001a64u, 0x00800000u, 0x8e000000u, 0x00000300u},
    {0x7c007c00u, 0x7c003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfc007c00u, 0x3c000000u, 0x3f800000u, 0x00000000u, 0x80000000u, 0x00003c00u, 0x00000000u, 0x3c000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000047u, 0x004703e1u, 0x00470000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x7e7147e1u, 0x00000047u, 0x00000000u, 0x80470000u, 0x007103e1u, 0x00000000u, 0x7e710000u, 0x00470000u, 0x47000000u, 0x00000000u},
    {0x00000047u, 0x80474700u, 0x00470000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x47004700u, 0x00000047u, 0x00000000u, 0x80470000u, 0x00004700u, 0x00000000u, 0x47008000u, 0x00470000u, 0x47000000u, 0x00000000u},
    {0x00800080u, 0x0080c2c2u, 0xff800000u, 0x00000000u, 0x00000000u, 0x80800080u, 0xc2c200c2u, 0x00800080u, 0x00000080u, 0x80800000u, 0x80c2c2c2u, 0x00000000u, 0xc2c20000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x00003f27u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x1b1c0027u, 0x00007f80u, 0x00000000u, 0xff800000u, 0x7f1c3f27u, 0x00000000u, 0x1b1c0000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00800080u, 0x0080a49cu, 0xff800000u, 0x00000000u, 0x00000000u, 0x80800080u, 0x829e009cu, 0x00800080u, 0x00000080u, 0x80800000u, 0x009ea49cu, 0x00000000u, 0x829e0000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x0000bc00u, 0x00000ef5u, 0x00000000u, 0x00000000u, 0x00000000u, 0x8000bc00u, 0x13e600f5u, 0x0000bc00u, 0x00000000u, 0x80000000u, 0x00e60ef5u, 0x00000000u, 0x13e60000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00470047u, 0x0047ec45u, 0x00470000u, 0x00000000u, 0x00000000u, 0x80470047u, 0x67694745u, 0x00470047u, 0x00000047u, 0x80470000u, 0x4769ec45u, 0x00000000u, 0x67690000u, 0x00000000u, 0x47000000u, 0x00000000u},
    {0x00800080u, 0x00803c9cu, 0xff800000u, 0x00000000u, 0x00000000u, 0x80800080u, 0x871c009cu, 0x00800080u, 0x00000080u, 0x80800000u, 0x801c3c9cu, 0x00000000u, 0x871c0000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00800080u, 0x00803c00u, 0xff800000u, 0x00000000u, 0x00000000u, 0x80800080u, 0x3c000000u, 0x00800080u, 0x00000080u, 0x80800000u, 0x80003c00u, 0x00000000u, 0x3c000000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00ff00ffu, 0x00ff4040u, 0xffff0000u, 0x00000000u, 0x00000000u, 0x80ff00ffu, 0x40400040u, 0x00ff00ffu, 0x000000ffu, 0x80ff0000u, 0xff404040u, 0x00000000u, 0x40400000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x00ff00ffu, 0x00ff00ffu, 0xffff0000u, 0x00000000u, 0x00000000u, 0x80ff00ffu, 0x00ff00ffu, 0x00ff00ffu, 0x000000ffu, 0x80ff0000u, 0xffff00ffu, 0x00000000u, 0x00ff0000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x00ff00ffu, 0x00ff7accu, 0xffff0000u, 0x00000000u, 0x00000000u, 0x80ff00ffu, 0xe4ea00ccu, 0x00ff00ffu, 0x000000ffu, 0x80ff0000u, 0xffea7accu, 0x00000000u, 0xe4ea0000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x00ff00ffu, 0x00ff5876u, 0xffff0000u, 0x00000000u, 0x00000000u, 0x80ff00ffu, 0x5de20076u, 0x00ff00ffu, 0x000000ffu, 0x80ff0000u, 0xffe25876u, 0x00000000u, 0x5de20000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x01000100u, 0x01000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x81000100u, 0x00000000u, 0x01000100u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x182f8e4au, 0x182fbc00u, 0x004a0000u, 0x00000000u, 0x00000000u, 0x982f8e4au, 0x00004a00u, 0x182f8e4au, 0x0000002fu, 0x802f0000u, 0x4a00bc00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x4a000000u, 0x00000000u},
    {0x20d8e2bcu, 0x20d80000u, 0xffbc0000u, 0x00000000u, 0x00000000u, 0xa0d8e2bcu, 0xbf800000u, 0x20d8e2bcu, 0x000000d8u, 0x80d80000u, 0xbc800000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0xbc000000u, 0x00000000u},
    {0x2333ad9cu, 0x23330000u, 0xff9c0000u, 0x00000000u, 0x00000000u, 0xa333ad9cu, 0x3f800000u, 0x2333ad9cu, 0x00000033u, 0x80330000u, 0x9c800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x9c000000u, 0x00000000u},
    {0x24f70eabu, 0x24f7487au, 0xffab0000u, 0x00000000u, 0x00000000u, 0xa4f70eabu, 0x17e6007au, 0x24f70eabu, 0x000000f7u, 0x80f70000u, 0xabe6487au, 0x00000000u, 0x17e60000u, 0x00000000u, 0xab000000u, 0x00000000u},
    {0x2a778095u, 0x2a777c00u, 0xff950000u, 0x00000000u, 0x00000000u, 0xaa778095u, 0x7c000000u, 0x2a778095u, 0x00000077u, 0x80770000u, 0x95007c00u, 0x00000000u, 0x7c000000u, 0x00000000u, 0x95000000u, 0x00000000u},
    {0x2b119b05u, 0x2b11144fu, 0x00050000u, 0x00000000u, 0x00000000u, 0xab119b05u, 0x4ce5054fu, 0x2b119b05u, 0x00000011u, 0x80110000u, 0x05e5144fu, 0x00000000u, 0x4ce50000u, 0x00000000u, 0x05000000u, 0x00000000u},
    {0x2b5d55fdu, 0x2b5d7f7fu, 0xfffd0000u, 0x00000000u, 0x00000000u, 0xab5d55fdu, 0x7f7f007fu, 0x2b5d55fdu, 0x0000005du, 0x805d0000u, 0xfd7f7f7fu, 0x00000000u, 0x7f7f0000u, 0x00000000u, 0xfd000000u, 0x00000000u}
};
constexpr std::uint32_t Checked[32] = {0xffffu, 0xfdfdu, 0x5dadu, 0xa05au, 0xb27au, 0x7fa7u, 0xb818u, 0xea77u, 0xfaffu, 0x5cfdu, 0x7ffdu, 0x9d38u, 0x9d38u, 0xf8fdu, 0xb977u, 0xfcfdu, 0xff5eu, 0xb818u, 0xf8fdu, 0xf8fdu, 0xf8fdu, 0xf8fdu, 0xf8fdu, 0xf8fdu, 0xfffdu, 0xfdbdu, 0xfdfdu, 0xfdfdu, 0xfdfdu, 0xf9fdu, 0xfdbdu, 0xfdfdu};
constexpr const char* Names[16] = {
    "add_f32 BYTE_0/DWORD -> DWORD",
    "sub_f32 DWORD/WORD_1 -> WORD_1",
    "subrev_f32 WORD_0/DWORD -> BYTE_2",
    "mul_f32 DWORD/BYTE_3 -> WORD_0",
    "mul_legacy_f32 BYTE_1/DWORD -> DWORD",
    "min_f32 BYTE_2/DWORD -> DWORD",
    "max_f32 DWORD/BYTE_0 -> BYTE_1",
    "max_f32 WORD_1/DWORD -> DWORD",
    "add_f16 BYTE_1/WORD_1 -> BYTE_0",
    "sub_f16 WORD_1/BYTE_2 -> WORD_1",
    "subrev_f16 BYTE_3/WORD_0 -> BYTE_3",
    "mul_f16 WORD_0/BYTE_0 -> DWORD",
    "min_f16 BYTE_0/WORD_1 -> WORD_0",
    "max_f16 WORD_1/BYTE_1 -> BYTE_2",
    "add_f32 WORD_1/DWORD -> BYTE_3",
    "mul_f16 BYTE_2/DWORD -> BYTE_1",
};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("sdwa float selectors: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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

void CheckByteSelectorClampRefused(AgcDriver::VulkanDevice& device) {
    constexpr std::array<std::array<std::uint32_t, 2>, 6> forms{{
        {0x640a0cf9u, 0x04052004u},
        {0x660a0cf9u, 0x04022504u},
        {0x680a0cf9u, 0x03042604u},
        {0x6a0a0cf9u, 0x05043304u},
        {0x740a0cf9u, 0x06012404u},
        {0x720a0cf9u, 0x00062604u},
    }};
    for (const auto& form : forms) {
        alignas(256) const std::array<std::uint32_t, 3> code{form[0], form[1], 0xbf810000u};
        std::string refusal;
        try {
            static_cast<void>(Compile(device, code));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find("VOP2 SDWA clamp with byte selectors is not implemented") != std::string::npos, "f16 SDWA form " + Hex(form[0]) + " " + Hex(form[1]) + " with clamp was not refused");
    }
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

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        CheckByteSelectorClampRefused(*device);
        std::puts("sdwa float selectors tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
