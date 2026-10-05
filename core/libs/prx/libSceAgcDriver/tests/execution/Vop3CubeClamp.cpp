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
#include <span>
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

alignas(256) constexpr std::array<std::uint32_t, 46> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xd544000a, 0x041a0b04, 0xd545000b,
    0x041a0b04, 0xd546000c, 0x041a0b04, 0xd547000d, 0x041a0b04, 0xd544800e, 0x041a0b04, 0xd545800f,
    0x041a0b04, 0xd5468010, 0x041a0b04, 0xd5478011, 0x041a0b04, 0xd5440212, 0x241a0b04, 0xd5450113,
    0xc41a0b04, 0xd5460414, 0x641a0b04, 0xd5470115, 0x841a0b04, 0xd5448016, 0xe41a0b04, 0xd5458417,
    0x441a0b04, 0xd5468118, 0x841a0b04, 0xd5478219, 0x241a0b04, 0xe0781000, 0x80010a03, 0xe0781010,
    0x80010e03, 0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x3f800000u, 0xbf800000u, 0x3f800000u, 0x00000000u},
    {0x7f800001u, 0x3f000000u, 0x3f000000u, 0x00000000u},
    {0x40000000u, 0x3f000000u, 0xff812345u, 0x00000000u},
    {0x80000007u, 0x7fc00000u, 0x7fc00000u, 0x00000000u},
    {0x00000007u, 0xffc00000u, 0x7f800001u, 0x00000000u},
    {0xff800000u, 0x80000007u, 0x00000007u, 0x00000000u},
    {0x7fc00000u, 0x00000007u, 0xff800000u, 0x00000000u},
    {0xbf000000u, 0x00000000u, 0x80000007u, 0x00000000u},
    {0x00000007u, 0x00000007u, 0x7f800001u, 0x00000000u},
    {0x00000007u, 0x7f800000u, 0x80000007u, 0x00000000u},
    {0x00000007u, 0x00000007u, 0x80000007u, 0x00000000u},
    {0xff800000u, 0xff800000u, 0x40000000u, 0x00000000u},
    {0x40000000u, 0xc0200000u, 0xc0200000u, 0x00000000u},
    {0x7fc00000u, 0xff800000u, 0x80000000u, 0x00000000u},
    {0xc0200000u, 0x7f800000u, 0x7f800000u, 0x00000000u},
    {0xbf000000u, 0xbf000000u, 0x3f000000u, 0x00000000u},
    {0x7fc00000u, 0xff800000u, 0xc0200000u, 0x00000000u},
    {0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xff800000u, 0xbf000000u, 0x00000000u},
    {0xff800000u, 0x00000000u, 0xff800000u, 0x00000000u},
    {0xff800000u, 0x7f800000u, 0x00000000u, 0x00000000u},
    {0xff800000u, 0x7f800000u, 0xbf000000u, 0x00000000u},
    {0xff800000u, 0xff800000u, 0xff800000u, 0x00000000u},
    {0xff800000u, 0xff800000u, 0xbf000000u, 0x00000000u},
    {0xff800000u, 0xbf000000u, 0xbf000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x7fc00000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0xffc00000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x7f800001u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0xff812345u, 0x00000000u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x40800000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40800000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40800000u, 0x3f800000u, 0x3f800000u, 0x40000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x40800000u, 0xbf800000u, 0xbf800000u, 0xc0000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u},
    {0x00000000u, 0xbf000000u, 0xbf000000u, 0x7f800001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f000000u, 0x3f000000u, 0x7f800001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x7f812345u, 0xbf000000u, 0x40800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0xff812345u, 0x3f000000u, 0x40800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7fc00000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xff800001u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7f800001u, 0xffc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x00000007u, 0x00000007u, 0xff800000u, 0x3f800000u, 0x00000007u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000007u, 0x80000007u, 0x7f800000u, 0x00000000u, 0x00000007u, 0x00000007u, 0x3f800000u},
    {0x00000000u, 0x7f800000u, 0x80000007u, 0x7fc00000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xff800000u, 0x00000007u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x80000007u, 0x80000000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000007u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x3f800000u},
    {0x40000000u, 0x00000007u, 0x7f800001u, 0x00000000u, 0x3f800000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000007u, 0x7f800001u, 0x00000000u, 0x3f800000u, 0x00000007u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00000007u, 0x80000007u, 0x7f800000u, 0x3f800000u, 0x00000007u, 0x00000000u, 0x3f800000u, 0x40000000u, 0x00000007u, 0x80000007u, 0x7f800000u, 0x3f800000u, 0x00000007u, 0x00000007u, 0x3f800000u},
    {0x40800000u, 0x00000007u, 0x80000007u, 0x00000000u, 0x3f800000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x40800000u, 0x00000007u, 0x00000007u, 0x00000000u, 0x3f800000u, 0x00000007u, 0x00000000u, 0x00000000u},
    {0x40400000u, 0xff800000u, 0xc0000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x7f800000u, 0x40000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x3f800000u},
    {0x40a00000u, 0xc0000000u, 0x40200000u, 0xc0a00000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x40a00000u, 0x40000000u, 0xc0200000u, 0x40a00000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x7f800000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x80000000u, 0xff800000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u},
    {0x40800000u, 0xc0200000u, 0xff800000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x40800000u, 0xc0200000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u},
    {0x40800000u, 0xbf000000u, 0x3f000000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x3f800000u, 0x40800000u, 0xbf000000u, 0xbf000000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x3f800000u},
    {0x00000000u, 0x40200000u, 0x7f800000u, 0x7fc00000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0xc0200000u, 0xff800000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u},
    {0x40400000u, 0x00000000u, 0x80000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x00000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u},
    {0x40400000u, 0x00000000u, 0x3f000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x3f000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u},
    {0x40a00000u, 0x7f800000u, 0x80000000u, 0xff800000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x40a00000u, 0x7f800000u, 0x00000000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0xff800000u, 0x00000000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x40000000u, 0x7f800000u, 0x80000000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u},
    {0x40000000u, 0xff800000u, 0xbf000000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x40000000u, 0x7f800000u, 0xbf000000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x3f800000u},
    {0x40a00000u, 0x7f800000u, 0x7f800000u, 0xff800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x40a00000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u},
    {0x40400000u, 0xff800000u, 0x3f000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x40000000u, 0x7f800000u, 0x3f000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u},
    {0x3f800000u, 0xbf000000u, 0x3f000000u, 0xff800000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xbf000000u, 0xbf000000u, 0x7f800000u, 0x00000000u, 0x3f000000u, 0x3f000000u, 0x3f800000u},
    {0x40800000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40800000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00000000u, 0x7f800001u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x7f800001u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x40000000u, 0x00000000u, 0xff812345u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x7f812345u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u}
};
constexpr const char* Names[16] = {
    "v_cubeid_f32 v10, v4, v5, v6",
    "v_cubesc_f32 v11, v4, v5, v6",
    "v_cubetc_f32 v12, v4, v5, v6",
    "v_cubema_f32 v13, v4, v5, v6",
    "v_cubeid_f32 v14, v4, v5, v6 clamp",
    "v_cubesc_f32 v15, v4, v5, v6 clamp",
    "v_cubetc_f32 v16, v4, v5, v6 clamp",
    "v_cubema_f32 v17, v4, v5, v6 clamp",
    "v_cubeid_f32 v18, -v4, |v5|, v6",
    "v_cubesc_f32 v19, |v4|, -v5, -v6",
    "v_cubetc_f32 v20, -v4, -v5, |v6|",
    "v_cubema_f32 v21, |v4|, v5, -v6",
    "v_cubeid_f32 v22, -v4, -v5, -v6 clamp",
    "v_cubesc_f32 v23, v4, -v5, |v6| clamp",
    "v_cubetc_f32 v24, |v4|, v5, -v6 clamp",
    "v_cubema_f32 v25, -v4, |v5|, v6 clamp",
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
    Require(actual == expected, std::string("vop3 cube clamp: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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

void CheckOmodRefused(AgcDriver::VulkanDevice& device) {
    for (const std::uint32_t opcode : {0x144u, 0x145u, 0x146u, 0x147u}) {
        alignas(256) const std::array<std::uint32_t, 3> code{0xd400000au | (opcode << 16u), 0x0c1a0b04u, 0xbf810000u};
        std::string refusal;
        try {
            static_cast<void>(Compile(device, code));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find("VOP3 source modifiers are not implemented") != std::string::npos, "VOP3 opcode " + Hex(opcode) + " with omod was not refused");
    }
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 16; ++i) {
            Expect(tid, out[i], Expected[tid][i], Names[i]);
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
        CheckOmodRefused(*device);
        std::puts("vop3 cube clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
