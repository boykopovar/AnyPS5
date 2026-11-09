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

alignas(256) constexpr std::array<std::uint32_t, 66> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e1402ff, 0xdeadbeef, 0x7e1602ff,
    0xdeadbeef, 0x7e1802ff, 0xdeadbeef, 0x7e1a02ff, 0xdeadbeef, 0x7e1c02ff, 0xdeadbeef, 0x7e1e02ff,
    0xdeadbeef, 0x7e2002ff, 0xdeadbeef, 0x7e2202ff, 0xdeadbeef, 0x7e2402ff, 0xdeadbeef, 0x7e2602ff,
    0xdeadbeef, 0x7e2802ff, 0xdeadbeef, 0x7e2a02ff, 0xdeadbeef, 0x7e2c02ff, 0xdeadbeef, 0x7e2e02ff,
    0xdeadbeef, 0x7e3002ff, 0xdeadbeef, 0x7e3202ff, 0xdeadbeef, 0xd550000a, 0x041a0b04, 0xd550000b,
    0x241a0b04, 0xd550000c, 0x441a0b04, 0xd550020d, 0x841a0b04, 0xd550070e, 0x641a0b04, 0xd550050f,
    0x841a0b04, 0xd55e0010, 0x041a0f04, 0xd55e0011, 0x241a0f04, 0xd55e0112, 0x041a0f04, 0xd55e0113,
    0x241a0f04, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030,
    0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x3fc00000u, 0x00000000u, 0xbf800000u, 0x7e318ad6u},
    {0xc0000000u, 0x3fc00000u, 0x3fc00000u, 0x1b3a953cu},
    {0xff7ffffeu, 0x437e8000u, 0x7f800000u, 0x8511fd5bu},
    {0x3f19999au, 0x00000000u, 0x7f800000u, 0x4607d625u},
    {0x7fc00001u, 0x3fc00000u, 0x3f000000u, 0xe4fead80u},
    {0x40200000u, 0x7f800000u, 0x3f000002u, 0xb68c914bu},
    {0x437f8000u, 0x437e8000u, 0x7f7fffffu, 0x400224c0u},
    {0xffc00000u, 0xc0000000u, 0x7f7fffffu, 0xd9c578ddu},
    {0x7f7fffffu, 0xbf800000u, 0x7f61b1e6u, 0xc7c63fe1u},
    {0x7f800000u, 0x3fc00000u, 0xbf400000u, 0x00b09f63u},
    {0x7f800000u, 0x3f19999au, 0x7f800000u, 0x054367bau},
    {0xff800000u, 0x7f800000u, 0xff7fffffu, 0xdee7b644u},
    {0x7fc00001u, 0x7f800000u, 0xc9a27dd4u, 0xeac29dbfu},
    {0xffc00000u, 0x7f7fffffu, 0xb587728cu, 0x929cedc6u},
    {0x7f7fffffu, 0x3f800000u, 0x7f7fffffu, 0x034bd1bau},
    {0x7f7fffffu, 0xbf800000u, 0xff800000u, 0x88c035d3u},
    {0x0da24260u, 0x7f7fffffu, 0xd4ab4b77u, 0xc51bdf32u},
    {0x00000000u, 0x00000000u, 0xf149f2cau, 0x9531985du},
    {0x00000000u, 0x00000000u, 0x7f800000u, 0x36f675ccu},
    {0x00000000u, 0x3fc00000u, 0xbf000002u, 0xdbc496cbu},
    {0x00000000u, 0x7f800000u, 0x95e761d1u, 0x2e05319au},
    {0x7f800000u, 0x00000000u, 0x7f61b1e6u, 0x3173b8d9u},
    {0x7f7fffffu, 0x00000000u, 0xbf000000u, 0x933de2fcu},
    {0xbf400000u, 0xff7fffffu, 0xff800000u, 0x79f35099u},
    {0xbf800000u, 0x00000000u, 0xff7fffffu, 0x03213676u},
    {0x7f61b1e6u, 0x7fc00001u, 0xff7ffffeu, 0x7ace7351u},
    {0x3f800001u, 0x7fc00001u, 0xff800000u, 0xe3153518u},
    {0x80000000u, 0x00000000u, 0x80000000u, 0x110e2cb6u},
    {0x80000000u, 0x80000000u, 0x7f800001u, 0xf81e54ddu},
    {0x80000000u, 0xc0000000u, 0x80000000u, 0x8d118e37u},
    {0x80000000u, 0x7fc00001u, 0x437e8000u, 0x28541424u},
    {0x7f800000u, 0x00000000u, 0x7f800001u, 0xa4672c0cu}
};
constexpr std::uint32_t Expected[32][10] = {
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xbf020000u, 0xbf000000u, 0xbf020000u, 0xbf000000u},
    {0xc0400000u, 0x40400000u, 0x40400000u, 0xff7fffffu, 0x40400000u, 0xff7fffffu, 0x3fc00000u, 0x3fc00002u, 0x3fc00002u, 0x3fc00000u},
    {0xff800000u, 0x7f800000u, 0x7f800000u, 0xff7fffffu, 0x7f800000u, 0xff7fffffu, 0x00800000u, 0xff800000u, 0xff800000u, 0x00800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xff7fffffu, 0x00000000u, 0xff7fffffu, 0x7f800100u, 0x7f800000u, 0x7f800100u, 0x7f800000u},
    {0x7fc00001u, 0xffc00001u, 0x7fc00001u, 0xff7fffffu, 0xffc00001u, 0xff7fffffu, 0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u},
    {0x7f800000u, 0xff800000u, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x02000002u, 0x00000002u, 0x02000002u, 0x00000002u},
    {0x477e00c0u, 0xc77e00c0u, 0xc77e00c0u, 0xff7fffffu, 0x477e00c0u, 0xff7fffffu, 0x7f7fffffu, 0x7f7fff00u, 0x7f7fffffu, 0x7f7fff00u},
    {0xffc00000u, 0x7fc00000u, 0xffc00000u, 0xff7fffffu, 0xffc00000u, 0xff7fffffu, 0x7f7f00ffu, 0x7f7f00ffu, 0x7f7f00ffu, 0x7f7f00ffu},
    {0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f61ffe6u, 0x7f6100e6u, 0x7f61ffe6u, 0x7f6100e6u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800000u, 0x7f800000u, 0xff7fffffu, 0xff400000u, 0x00400000u, 0xff400000u, 0x00400000u},
    {0x7f800000u, 0xff800000u, 0xff800000u, 0xff7fffffu, 0x7f800000u, 0xff7fffffu, 0x7fff0000u, 0x7f000000u, 0x7fff0000u, 0x7f000000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff7fffffu, 0xff7fff00u, 0xff7fffffu, 0xff7fffffu, 0xff7fff00u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7fc00001u, 0xff7fffffu, 0xff7fffffu, 0x00a27dd4u, 0x00a27dd4u, 0x00a27dd4u, 0x00a27dd4u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xffc00000u, 0xff7fffffu, 0xff7fffffu, 0xb500728cu, 0xb500728cu, 0xb500728cu, 0xb500728cu},
    {0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7fffffffu, 0x7f00ffffu, 0x7fffffffu, 0x7f00ffffu},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0xff800000u, 0x00800000u, 0xff800000u, 0x00800000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x4da2425fu, 0xff7fffffu, 0xff7fffffu, 0xd4004b77u, 0xd4004b77u, 0xd4004b77u, 0xd4004b77u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xf14900cau, 0xf14900cau, 0xf14900cau, 0xf14900cau},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xff7fffffu, 0x00000000u, 0xff7fffffu, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0x00000002u, 0x00000002u, 0x00000002u, 0x00000002u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x950061d1u, 0x950061d1u, 0x950061d1u, 0x950061d1u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xff7fffffu, 0x00000000u, 0xff7fffffu, 0x7f61ffe6u, 0x7f6100e6u, 0x7f61ffe6u, 0x7f6100e6u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xbf0000ffu, 0xbf000000u, 0xbf0000ffu, 0xbf000000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff3fffffu, 0xff7fffffu, 0xff7fffffu, 0xff800000u, 0xff800100u, 0xff800100u, 0xff800000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff00ffffu, 0xff01ffffu, 0xff01ffffu, 0xff00ffffu},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7ffffeu, 0xff7f00feu, 0xff7ffffeu, 0xff7f00feu},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff800001u, 0xff800000u, 0xff800001u, 0xff800000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7f800001u, 0x7f800001u, 0x7f800001u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x437e8000u, 0x437e8000u, 0x437e8000u, 0x437e8000u},
    {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f8000ffu, 0x7f800000u, 0x7f8000ffu, 0x7f800000u}
};
constexpr std::uint32_t Checked[32] = {0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu, 0x03ffu};
constexpr const char* Names[10] = {
    "v_mullit_f32 v10, v4, v5, v6",
    "v_mullit_f32 v11, -v4, v5, v6",
    "v_mullit_f32 v12, v4, -v5, v6",
    "v_mullit_f32 v13, v4, |v5|, -v6",
    "v_mullit_f32 v14, -|v4|, -|v5|, |v6|",
    "v_mullit_f32 v15, |v4|, v5, -|v6|",
    "v_cvt_pk_u8_f32 v16, v4, v7, v6",
    "v_cvt_pk_u8_f32 v17, -v4, v7, v6",
    "v_cvt_pk_u8_f32 v18, |v4|, v7, v6",
    "v_cvt_pk_u8_f32 v19, -|v4|, v7, v6",
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
    Require(actual == expected, std::string("vop3 legacy source modifiers: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 10; ++i) {
            if ((Checked[tid] >> i) & 1u) Expect(tid, out[i], Expected[tid][i], Names[i]);
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
        std::puts("vop3 legacy source modifiers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
