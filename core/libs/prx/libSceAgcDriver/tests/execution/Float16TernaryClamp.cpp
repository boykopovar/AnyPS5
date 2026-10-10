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

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 39> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e1c0307, 0x7e200307, 0x7e220307,
    0x7e140307, 0x7e160307, 0x7e180307, 0x7e1a0307, 0x7e1e0307, 0x7e240307, 0xd754800a, 0x041a0b04,
    0xd751800b, 0x041a0b04, 0xd757800c, 0x041a0b04, 0xd75f800d, 0x041a0b04, 0xd754fa0e, 0x24120d05,
    0xd751a80f, 0x44160906, 0xd757f410, 0x84120d05, 0xd75fd811, 0x04160906, 0xd75fa212, 0xa4120d05,
    0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000400u, 0x3bffb4a2u, 0x7bff7bffu, 0xfd976299u},
    {0x0001755du, 0x00007c00u, 0x07b7be05u, 0x32b8562au},
    {0x00017d23u, 0x42007c01u, 0x3c00fc01u, 0xfb47feb7u},
    {0xbfa8fd5au, 0x00008000u, 0x7d232fe0u, 0x6a55b5f1u},
    {0x1000d324u, 0xfc00f936u, 0xa88cfc00u, 0x455742adu},
    {0x87bb3c00u, 0x7c012000u, 0x00000000u, 0x517cded7u},
    {0xc2d6b555u, 0x2a2fbc00u, 0x3c00fae5u, 0x3726d2dfu},
    {0x000103ffu, 0x3555b555u, 0x3c14bc00u, 0xf06fead4u},
    {0x2000fc01u, 0x79817bffu, 0x7bff8001u, 0xe6607650u},
    {0x84003c00u, 0x5daf6841u, 0x3c00064eu, 0x3d4d4112u},
    {0x7e00fc00u, 0x7c007e00u, 0x7e00fc00u, 0x90b35b97u},
    {0x081d4286u, 0xbd763c00u, 0x84004200u, 0x510fb8c3u},
    {0x8a2b0000u, 0x05170400u, 0x03ffc7e5u, 0x6e83a7f0u},
    {0xc0007e00u, 0x7d237e00u, 0x7c017c00u, 0x45c0f96bu},
    {0xb95a7bffu, 0x007d8001u, 0x7c010ccbu, 0xb9c23f62u},
    {0x00017e00u, 0x1f9d8001u, 0x417698d1u, 0xe7b478feu},
    {0x048f7edbu, 0x5e8a4fd9u, 0x81b2fe00u, 0x9f542371u},
    {0x7c00c000u, 0xbc00fc00u, 0x3c000000u, 0x9004ee95u},
    {0x7e00f5ddu, 0x7e008000u, 0xfc01bc00u, 0x8fd0b012u},
    {0xb5558284u, 0x1000d1adu, 0xfe003460u, 0x76315f60u},
    {0x00003c00u, 0xc0008400u, 0x040039f3u, 0x9266fdd2u},
    {0x0000bc00u, 0x064de034u, 0x74c2fc01u, 0xcbe5dec7u},
    {0x3c01b555u, 0x42007c00u, 0x7c003555u, 0x36190caau},
    {0xc000fbffu, 0x3c004200u, 0x7d23c000u, 0xb63ec222u},
    {0x000000a1u, 0xf41e891eu, 0xbfb23436u, 0x35b2cc39u},
    {0x0000018fu, 0xb555844du, 0xcaf50000u, 0x1f46ce67u},
    {0x0000027cu, 0x7e00932bu, 0x35557c01u, 0xeedc6a0cu},
    {0x000003ffu, 0x0bc83555u, 0x076a4000u, 0x6dde7f88u},
    {0x000003ffu, 0x7bff2000u, 0xde652493u, 0xc36c95c2u},
    {0x000003ffu, 0xfbfffc01u, 0x40007d23u, 0x85da3cafu},
    {0x00000400u, 0x7789e844u, 0x80000001u, 0xa570949du},
    {0x0000053du, 0x12482000u, 0x8d328bb0u, 0x80525003u}
};
constexpr std::uint32_t Expected[32][12] = {
    {0xfd973c00u, 0xfd970000u, 0xfd970400u, 0xfd970000u, 0x3c006299u, 0xfd970000u, 0x00006299u, 0x00006299u, 0xfd970000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x32b83c00u, 0x32b80000u, 0x32b83c00u, 0x32b80000u, 0x07b7562au, 0x32b80000u, 0x07b7562au, 0x3c00562au, 0x32b80000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xfb470000u, 0xfb470000u, 0xfb470000u, 0xfb470000u, 0x3c00feb7u, 0xfb473c00u, 0x0000feb7u, 0x0000feb7u, 0xfb470000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x6a552fe0u, 0x6a550000u, 0x6a550000u, 0x6a550000u, 0x0000b5f1u, 0x6a550000u, 0x0000b5f1u, 0x0000b5f1u, 0x6a550000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x45570000u, 0x45570000u, 0x45570000u, 0x45573c00u, 0x3c0042adu, 0x45570000u, 0x000042adu, 0x000042adu, 0x45570000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x517c3c00u, 0x517c0000u, 0x517c2000u, 0x517c0000u, 0x0000ded7u, 0x517c0000u, 0x0000ded7u, 0x0000ded7u, 0x517c3c00u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x37260000u, 0x37260000u, 0x37260000u, 0x37263555u, 0x3c00d2dfu, 0x37262a2fu, 0x0000d2dfu, 0x3c00d2dfu, 0x37263c00u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xf06f03ffu, 0xf06f0000u, 0xf06f0000u, 0xf06f03ffu, 0x3c00ead4u, 0xf06f0000u, 0x0000ead4u, 0x0000ead4u, 0xf06f0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xe6603c00u, 0xe6600000u, 0xe6600000u, 0xe6600000u, 0x3c007650u, 0xe6603c00u, 0x3c007650u, 0x3c007650u, 0xe6600000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3d4d3c00u, 0x3d4d064eu, 0x3d4d3c00u, 0x3d4d3c00u, 0x3c004112u, 0x3d4d0000u, 0x3c004112u, 0x00004112u, 0x3d4d3c00u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x90b30000u, 0x90b30000u, 0x90b30000u, 0x90b30000u, 0x00005b97u, 0x90b33c00u, 0x00005b97u, 0x00005b97u, 0x90b30000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x510f3c00u, 0x510f3c00u, 0x510f3c00u, 0x510f3c00u, 0x3c00b8c3u, 0x510f0000u, 0x0000b8c3u, 0x0400b8c3u, 0x510f0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x6e830400u, 0x6e830000u, 0x6e830000u, 0x6e830000u, 0x03ffa7f0u, 0x6e830000u, 0x03ffa7f0u, 0x0000a7f0u, 0x6e830400u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x45c03c00u, 0x45c03c00u, 0x45c03c00u, 0x45c00000u, 0x0000f96bu, 0x45c00000u, 0x0000f96bu, 0x0000f96bu, 0x45c00000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xb9c23c00u, 0xb9c20000u, 0xb9c20ccbu, 0xb9c20000u, 0x00003f62u, 0xb9c20000u, 0x00003f62u, 0x3c003f62u, 0xb9c20001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xe7b40000u, 0xe7b40000u, 0xe7b40000u, 0xe7b43c00u, 0x3c0078feu, 0xe7b41f9du, 0x000078feu, 0x000078feu, 0xe7b40000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x9f543c00u, 0x9f543c00u, 0x9f543c00u, 0x9f540000u, 0x048f2371u, 0x9f540000u, 0x00002371u, 0x01b22371u, 0x9f540000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x90040000u, 0x90040000u, 0x90040000u, 0x90040000u, 0x3c00ee95u, 0x90040000u, 0x0000ee95u, 0x0000ee95u, 0x90040000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x8fd00000u, 0x8fd00000u, 0x8fd00000u, 0x8fd03c00u, 0x0000b012u, 0x8fd03c00u, 0x0000b012u, 0x0000b012u, 0x8fd00000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x76313460u, 0x76310000u, 0x76310000u, 0x76310000u, 0x00005f60u, 0x76310284u, 0x00005f60u, 0x3c005f60u, 0x76313c00u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x92663c00u, 0x92660000u, 0x926639f3u, 0x92660000u, 0x3c00fdd2u, 0x92660000u, 0x0000fdd2u, 0x0000fdd2u, 0x92660000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xcbe50000u, 0xcbe50000u, 0xcbe50000u, 0xcbe50000u, 0x3c00dec7u, 0xcbe5064du, 0x0000dec7u, 0x0000dec7u, 0xcbe50000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x36193c00u, 0x36190000u, 0x36193555u, 0x36190000u, 0x3c000caau, 0x36193555u, 0x3c000caau, 0x3c000caau, 0x36190000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xb63e3c00u, 0xb63e0000u, 0xb63e0000u, 0xb63e0000u, 0x0000c222u, 0xb63e3c00u, 0x0000c222u, 0x0000c222u, 0xb63e3c00u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x35b23436u, 0x35b20000u, 0x35b200a1u, 0x35b20000u, 0x3c00cc39u, 0x35b20000u, 0x0000cc39u, 0x0000cc39u, 0x35b20000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x1f46018fu, 0x1f460000u, 0x1f460000u, 0x1f460000u, 0x3c00ce67u, 0x1f460000u, 0x0000ce67u, 0x0000ce67u, 0x1f460000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xeedc027cu, 0xeedc0000u, 0xeedc0000u, 0xeedc0000u, 0x35556a0cu, 0xeedc0000u, 0x00006a0cu, 0x00006a0cu, 0xeedc0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x6dde3c00u, 0x6dde03ffu, 0x6dde3555u, 0x6dde03ffu, 0x076a7f88u, 0x6dde0000u, 0x076a7f88u, 0x3c007f88u, 0x6dde0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xc36c2493u, 0xc36c03ffu, 0xc36c2000u, 0xc36c03ffu, 0x3c0095c2u, 0xc36c0000u, 0x000095c2u, 0x3c0095c2u, 0xc36c0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x85da03ffu, 0x85da03ffu, 0x85da03ffu, 0x85da0000u, 0x3c003cafu, 0x85da0000u, 0x00003cafu, 0x00003cafu, 0x85da0000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xa5700400u, 0xa5700000u, 0xa5700001u, 0xa5700000u, 0x0000949du, 0xa5700000u, 0x0000949du, 0x0000949du, 0xa5700000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80522000u, 0x80520000u, 0x8052053du, 0x80520000u, 0x0d325003u, 0x80520000u, 0x00005003u, 0x3c005003u, 0x80520000u, 0x00000000u, 0x00000000u, 0x00000000u}
};
constexpr std::uint32_t Checked[32] = {0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1feu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1afu, 0x1ffu, 0x17fu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1dfu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1ffu, 0x1fbu, 0x1feu, 0x1feu, 0x1fdu, 0x1fdu, 0x1f8u, 0x1fbu, 0x1ffu};
constexpr const char* Names[9] = {
    "v_max3_f16 v10, v4, v5, v6 clamp",
    "v_min3_f16 v11, v4, v5, v6 clamp",
    "v_med3_f16 v12, v4, v5, v6 clamp",
    "v_div_fixup_f16 v13, v4, v5, v6 clamp",
    "v_max3_f16 v14, -v5, |v6|, v4 op_sel:[1,1,1,1] clamp",
    "v_min3_f16 v15, v6, -v4, v5 op_sel:[1,0,1,0] clamp",
    "v_med3_f16 v16, v5, v6, -|v4| op_sel:[0,1,1,1] clamp",
    "v_div_fixup_f16 v17, v6, v4, v5 op_sel:[1,1,0,1] clamp",
    "v_div_fixup_f16 v18, -v5, |v6|, -v4 op_sel:[0,0,1,0] clamp",
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
    Require(actual == expected, std::string("float16 ternary clamp: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 9; ++i) {
            if ((Checked[tid] >> i) & 1u) Expect(tid, out[i], Expected[tid][i], Names[i]);
        }
    }
}

const Testing::Case results{"Float16TernaryClamp_Max3Med3Min3WithClamp_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
