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

alignas(256) constexpr std::array<std::uint32_t, 46> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e140307, 0xd75f000a, 0x041a0b04,
    0x7e160307, 0xd75f380b, 0x041a0b04, 0x7e180307, 0xd75f400c, 0x041a0b04, 0x7e1a0307, 0xd75f060d,
    0xa41a0b04, 0x7e1c0307, 0xd75f280e, 0x04160d04, 0x7e1e0307, 0xd75f100f, 0x44160906, 0x7e200307,
    0xd75f3010, 0x04120af2, 0x7e220307, 0xd75f0011, 0x0419e705, 0xe0701000, 0x80010a03, 0xe0701004,
    0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014,
    0x80010f03, 0xe0701018, 0x80011003, 0xe070101c, 0x80011103, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0xaa591773u, 0x0912d5a5u, 0x04005e85u, 0x1b9c7ec8u},
    {0x482c74f4u, 0x71a1f91au, 0xd69e0400u, 0x0cacc82cu},
    {0x7c018000u, 0x90086eaeu, 0x00007bffu, 0xce9da0ddu},
    {0x8000fc00u, 0x7c0183ffu, 0x7c010001u, 0x5a145979u},
    {0xba5f5640u, 0x4fc2fc00u, 0x4d3d82bcu, 0x9ffb2a93u},
    {0x7b327e00u, 0x0c5c7e00u, 0x00008000u, 0x4449e54eu},
    {0x7c001694u, 0x7c000000u, 0xa8617c00u, 0x3ebf7466u},
    {0x000149ddu, 0x80007e00u, 0xc0006dd1u, 0xea791f92u},
    {0x00015b7bu, 0x66bd3966u, 0x7e008000u, 0x0beb2994u},
    {0xe5c1fc00u, 0xc0003c00u, 0x56400001u, 0x8b2852b0u},
    {0xd82c3263u, 0x94600000u, 0x3c000000u, 0x384242e0u},
    {0x5b660400u, 0x5c84dea4u, 0x8000d995u, 0xea68a771u},
    {0x7c000001u, 0x48979303u, 0x00000979u, 0xbbf3011du},
    {0x0001f0fbu, 0x25117bffu, 0x329dad28u, 0x2a5a6250u},
    {0x2b697c01u, 0x463f9f63u, 0x80008000u, 0x5c0b4c22u},
    {0x83ff7bffu, 0xc000c000u, 0xfc007e00u, 0x95e109b3u},
    {0x3c000ecau, 0xc000f24eu, 0xc9823c00u, 0xa2d1d5ffu},
    {0x3c008000u, 0x7c007c01u, 0x7bff7c00u, 0x02165d7fu},
    {0x7bff3c00u, 0xec467c00u, 0x00009243u, 0x1424a54fu},
    {0xc0007bffu, 0x7e0096a1u, 0x0400fc00u, 0x48ce86a2u},
    {0x56407bffu, 0x9bbd7de0u, 0x0400c000u, 0xb7fc780fu},
    {0x56403956u, 0x1deca74eu, 0xfe558000u, 0xdd3cfee3u},
    {0x3c005cb0u, 0xd9133f9du, 0x24353c00u, 0x4aca4572u},
    {0xf932c000u, 0x7e008000u, 0x0d2bde75u, 0x0a2697e0u},
    {0x0a72fc00u, 0x41aad71fu, 0xa7bd0000u, 0xc33a46f2u},
    {0x67d70400u, 0x7c017c01u, 0x86ec7c01u, 0xd107ab16u},
    {0x40aaa7dfu, 0xd6653312u, 0xc0000400u, 0xabd58d2fu},
    {0xc60483ffu, 0xe2bec000u, 0x879b0400u, 0x1dc7aeecu},
    {0xc00083ffu, 0xea3bcb39u, 0x7e003c00u, 0x275116b8u},
    {0x005b7ebau, 0x5335775eu, 0xbe8b7c00u, 0x99f7867fu},
    {0x0001c000u, 0xfe55e3cdu, 0x0000fc00u, 0xbb3d5c51u},
    {0x55f94db8u, 0x38bd1e8eu, 0x0001fc00u, 0x7e4c716cu}
};
constexpr std::uint32_t Expected[32][8] = {
    {0x1b9c9773u, 0x1b9c2a59u, 0x97737ec8u, 0x1b9c9773u, 0x1b9c2a59u, 0x1b9cde85u, 0x1b9cbc00u, 0x1b9cd5a5u},
    {0x0cacf4f4u, 0x0cacc82cu, 0xf4f4c82cu, 0x0cacf4f4u, 0x0cac482cu, 0x0cac0400u, 0x0cac3c00u, 0x0cacf91au},
    {0xce9d0000u, 0xce9d8000u, 0x0000a0ddu, 0xce9d8000u, 0xce9dfc00u, 0xce9dfe01u, 0xce9d7e01u, 0xce9deeaeu},
    {0x5a14fc00u, 0x5a147e01u, 0xfc005979u, 0x5a14fc00u, 0x5a147e01u, 0x5a14fc00u, 0x5a147e01u, 0x5a1483ffu},
    {0x9ffb0000u, 0x9ffb3a5fu, 0x00002a93u, 0x9ffb8000u, 0x9ffbba5fu, 0x9ffbfc00u, 0x9ffbbc00u, 0x9ffb7c00u},
    {0x44497e00u, 0x44490000u, 0x7e00e54eu, 0x44497e00u, 0x4449fc00u, 0x44497e00u, 0x44493c00u, 0x44490000u},
    {0x3ebf7c00u, 0x3ebf8000u, 0x7c007466u, 0x3ebffc00u, 0x3ebffe00u, 0x3ebf8000u, 0x3ebffe00u, 0x3ebffc00u},
    {0xea797e00u, 0xea797c00u, 0x7e001f92u, 0xea797e00u, 0xea798000u, 0xea797e00u, 0xea79fc00u, 0xea79fc00u},
    {0x0beb8000u, 0x0beb7e00u, 0x80002994u, 0x0beb8000u, 0x0bebfc00u, 0x0beb8000u, 0x0beb3c00u, 0x0beb0000u},
    {0x8b287c00u, 0x8b28e5c1u, 0x7c0052b0u, 0x8b28fc00u, 0x8b28e5c1u, 0x8b280001u, 0x8b283c00u, 0x8b28bc00u},
    {0x3842fe00u, 0x3842d82cu, 0xfe0042e0u, 0x3842fe00u, 0x3842fc00u, 0x38420000u, 0x38423c00u, 0x38428000u},
    {0xea680400u, 0xea688000u, 0x0400a771u, 0xea688400u, 0xea68db66u, 0xea685995u, 0xea683c00u, 0xea685ea4u},
    {0xbbf38001u, 0xbbf30000u, 0x8001011du, 0xbbf38001u, 0xbbf37c00u, 0xbbf30000u, 0xbbf37c00u, 0xbbf39303u},
    {0x2a5af0fbu, 0x2a5a0001u, 0xf0fb6250u, 0x2a5af0fbu, 0x2a5a8001u, 0x2a5aad28u, 0x2a5a3c00u, 0x2a5a7bffu},
    {0x5c0b0000u, 0x5c0b8000u, 0x00004c22u, 0x5c0b8000u, 0x5c0bfc00u, 0x5c0b0000u, 0x5c0b3c00u, 0x5c0b0000u},
    {0x95e17e00u, 0x95e17c00u, 0x7e0009b3u, 0x95e1fe00u, 0x95e17e00u, 0x95e1fc00u, 0x95e13c00u, 0x95e17e00u},
    {0xa2d18ecau, 0xa2d13c00u, 0x8ecad5ffu, 0xa2d18ecau, 0xa2d1bc00u, 0xa2d13c00u, 0xa2d1bc00u, 0xa2d1f24eu},
    {0x02167e01u, 0x02160000u, 0x7e015d7fu, 0x02167e01u, 0x0216fe00u, 0x02167e01u, 0x02160000u, 0x0216fc00u},
    {0x14248000u, 0x14248000u, 0x8000a54fu, 0x14248000u, 0x14247bffu, 0x1424fc00u, 0x1424bc00u, 0x14247c00u},
    {0x48ce7c00u, 0x48ce7e00u, 0x7c0086a2u, 0x48cefc00u, 0x48ce7e00u, 0x48cefc00u, 0x48ce7e00u, 0x48ce7c00u},
    {0xb7fc7fe0u, 0xb7fcd640u, 0x7fe0780fu, 0xb7fc7fe0u, 0xb7fc5640u, 0xb7fc7fe0u, 0xb7fcbc00u, 0xb7fc7c00u},
    {0xdd3c0000u, 0xdd3cfe55u, 0x0000fee3u, 0xdd3c8000u, 0xdd3cfc00u, 0xdd3c0000u, 0xdd3c3c00u, 0xdd3c0000u},
    {0x4aca5cb0u, 0x4acabc00u, 0x5cb04572u, 0x4acadcb0u, 0x4acabc00u, 0x4acabc00u, 0x4acabc00u, 0x4acabf9du},
    {0x0a267c00u, 0x0a267e00u, 0x7c0097e0u, 0x0a26fc00u, 0x0a267e00u, 0x0a268000u, 0x0a267e00u, 0x0a260000u},
    {0xc33a8000u, 0xc33a8a72u, 0x800046f2u, 0xc33a8000u, 0xc33a7c00u, 0xc33a0000u, 0xc33a3c00u, 0xc33a8000u},
    {0xd1077e01u, 0xd1077e01u, 0x7e01ab16u, 0xd107fe01u, 0xd1077e01u, 0xd1077e01u, 0xd1077e01u, 0xd1077e01u},
    {0xabd527dfu, 0xabd540aau, 0x27df8d2fu, 0xabd5a7dfu, 0xabd5c0aau, 0xabd58400u, 0xabd5bc00u, 0xabd5b312u},
    {0x1dc783ffu, 0x1dc74604u, 0x83ffaeecu, 0x1dc783ffu, 0x1dc7c604u, 0x1dc78400u, 0x1dc73c00u, 0x1dc7c000u},
    {0x275183ffu, 0x27517e00u, 0x83ff16b8u, 0x275183ffu, 0x2751c000u, 0x2751bc00u, 0x27513c00u, 0x2751cb39u},
    {0x99f77c00u, 0x99f7805bu, 0x7c00867fu, 0x99f7fc00u, 0x99f70000u, 0x99f7fc00u, 0x99f73c00u, 0x99f7fc00u},
    {0xbb3d7c00u, 0xbb3dfe55u, 0x7c005c51u, 0xbb3dfc00u, 0xbb3dfe55u, 0xbb3d7c00u, 0xbb3dfe55u, 0xbb3d7c00u},
    {0x7e4cfc00u, 0x7e4c55f9u, 0xfc00716cu, 0x7e4cfc00u, 0x7e4c8000u, 0x7e4cfc00u, 0x7e4c3c00u, 0x7e4c7c00u}
};
constexpr const char* Names[8] = {"lo", "hi sources", "hi dst", "neg abs", "mixed halves", "neg denominator", "literal quotient", "literal denominator"};

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
    Require(actual == expected, std::string("div fixup f16: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 8; ++i) Expect(tid, out[i], Expected[tid][i], Names[i]);
    }
}

const Testing::Case operandForms{"DivFixupF16_OperandForms_MatchReferenceResults", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
