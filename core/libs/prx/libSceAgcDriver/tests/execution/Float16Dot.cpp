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

alignas(256) constexpr std::array<std::uint32_t, 37> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e140306, 0x04140b04, 0xcc13400b,
    0x1c1a0b04, 0xcc13420c, 0xbc1a0b04, 0xcc13410d, 0x5c1a0b04, 0xcc13480e, 0x141a0b04, 0xcc13c00f,
    0x1c1a0b04, 0x7e200306, 0x04200af2, 0x7e220306, 0x04220aff, 0x40003c00, 0x7e240306, 0x04240afa,
    0xff00b104, 0x7e260306, 0xcc135013, 0x0c1a0b04, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03,
    0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x0001bc00u, 0x81bfacbau, 0xc541aa2fu, 0x00000000u},
    {0x01f61a79u, 0x3c000381u, 0xc1729fffu, 0x00000000u},
    {0x02c9be80u, 0xb252213du, 0x41880526u, 0x00000000u},
    {0x032f030cu, 0x0d88ca10u, 0x3f800000u, 0x00000000u},
    {0x04003848u, 0x2427b61cu, 0x3920d8c0u, 0x00000000u},
    {0x04008190u, 0x01c85a38u, 0xc1e5655cu, 0x00000000u},
    {0x00ba3faeu, 0x02bcc61fu, 0x439ff26au, 0x00000000u},
    {0x00bc0400u, 0xaed23800u, 0xaf78b10fu, 0x00000000u},
    {0x47a37c85u, 0x7ceb0000u, 0x80521c95u, 0x00000000u},
    {0x6bc64fd7u, 0x02f45f4du, 0xffc8ab86u, 0x00000000u},
    {0x034102d0u, 0xac427d00u, 0x7fa00000u, 0x00000000u},
    {0x00000000u, 0xf35dfc00u, 0x805c769au, 0x00000000u},
    {0x0000030cu, 0x8370c506u, 0x00000001u, 0x00000000u},
    {0x00002783u, 0x5f2c0896u, 0x7f800000u, 0x00000000u},
    {0x00010001u, 0xfc00004cu, 0xca63705au, 0x00000000u},
    {0x64ba817bu, 0x80000000u, 0x00000001u, 0x00000000u},
    {0x000083ffu, 0x7d00def6u, 0x95a99701u, 0x00000000u},
    {0x7c210c47u, 0x825c8000u, 0x45b66cc3u, 0x00000000u},
    {0x00003134u, 0x8000c266u, 0x8029d2f0u, 0x00000000u},
    {0x3b898000u, 0x0000e8b9u, 0x0034ccdcu, 0x00000000u},
    {0x00000000u, 0x04008000u, 0x5f834da7u, 0x00000000u},
    {0x00000001u, 0x366e51ddu, 0x7f7fffffu, 0x00000000u},
    {0x00000001u, 0xdc472c9du, 0x42750eeeu, 0x00000000u},
    {0x000000c8u, 0x83ff3c00u, 0x43380747u, 0x00000000u},
    {0x0000010cu, 0x7c00f56fu, 0x005df72bu, 0x00000000u},
    {0x00000377u, 0xdaabea88u, 0xf540cddbu, 0x00000000u},
    {0x000003cau, 0x5b9f7e00u, 0xcaba68d5u, 0x00000000u},
    {0x00000400u, 0x785b8ab8u, 0x7fa00000u, 0x00000000u},
    {0x0000051cu, 0x6f115c7fu, 0x0047beccu, 0x00000000u},
    {0x00000c55u, 0x3c00ec7fu, 0x263e4496u, 0x00000000u},
    {0x00000f28u, 0x9e2d0123u, 0x565f49ebu, 0x00000000u},
    {0x00001209u, 0x3fe3c10fu, 0x801b0c3eu, 0x00000000u}
};

constexpr std::uint32_t Expected[32][10] = {
    {0xc541a901u, 0xc541a901u, 0x4541a901u, 0xc541ab5du, 0xc541aa2fu, 0xc541a901u, 0xc541ab5eu, 0xc541ab5eu, 0xc541aa30u, 0xc541aa2fu},
    {0xc1729fdfu, 0xc1729fdfu, 0x41729fdfu, 0xc172a01fu, 0xc172930du, 0xc1729fdfu, 0xc1729fc7u, 0xc1529fc7u, 0xc172a037u, 0xc172930du},
    {0x4187e315u, 0x4187e315u, 0xc187e315u, 0x41882737u, 0x418a9676u, 0x4187e315u, 0x41881a1au, 0x4184f11au, 0x41880521u, 0x418a9676u},
    {0x3f7fdb0fu, 0x3f7fdb0fu, 0xbf7fdb0fu, 0x3f801279u, 0x3f7fd966u, 0x3f7fdb0fu, 0xc1320000u, 0xc131fd3cu, 0x41a5a000u, 0x3f7fd966u},
    {0xbe511687u, 0xbe511687u, 0x3e511687u, 0x3e5166f4u, 0x3c105963u, 0xbe511687u, 0xbec36be5u, 0xbeb2cfe5u, 0x392b6e40u, 0x3c105963u},
    {0xc1e56f13u, 0xc1e56f13u, 0x41e56f13u, 0xc1e55ba5u, 0xc1e54c7cu, 0xc1e56f13u, 0x432a5354u, 0x432a5358u, 0x429ba4a9u, 0xc1e54c7cu},
    {0x439a1228u, 0x439a1228u, 0xc39a1228u, 0x43a5d2acu, 0x439ff26au, 0x439a1228u, 0x439ce2eau, 0x439ce2edu, 0x439ff25eu, 0x439ff26au},
    {0x37f5fb14u, 0x37f5fb14u, 0xb7f5fb14u, 0xb7f5fc0cu, 0xb5720f8bu, 0x37f5fb14u, 0x3f000000u, 0x3e92e000u, 0x3f75bfecu, 0xb5720f8bu},
    {0x7fd0a000u, 0x7fd0a000u, 0xffd0a000u, 0x7fd0a000u, 0x7fd0a000u, 0x7fd0a000u, 0x7fdd6000u, 0x7fdd6000u, 0x7fdd6000u, 0x7fd0a000u},
    {0xffc8ab86u, 0xffc8ab86u, 0x7fc8ab86u, 0xffc8ab86u, 0xffc8ab86u, 0xffc8ab86u, 0xffc8ab86u, 0xffc8ab86u, 0x7fd0a000u, 0xffc8ab86u},
    {0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0xffe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xffc00000u},
    {0xb974e480u, 0xb974e480u, 0x3974e480u, 0x3974e480u, 0xb1279400u, 0xb974e480u, 0xc0a0c000u, 0xc0a0c0dcu, 0xbe16f048u, 0xb1279400u},
    {0x7f800000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u},
    {0xff800000u, 0xff800000u, 0x7f800000u, 0x7f800000u, 0xff800000u, 0xff800000u, 0xffc00000u, 0xff800000u, 0xff800000u, 0xff800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x7fe00000u, 0x7fe00000u, 0xffe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fc42000u, 0x7fe00000u},
    {0x7fc42000u, 0x7fc42000u, 0x7fc42000u, 0xffc42000u, 0x7fc42000u, 0x7fc42000u, 0x45b66cc3u, 0x45b66cc3u, 0x45b66cc3u, 0x7fc42000u},
    {0xbf052ae0u, 0xbf052ae0u, 0x3f052ae0u, 0x3f052ae0u, 0x80000000u, 0xbf052ae0u, 0xc04cc000u, 0xc04cc000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc50e5804u, 0x00000000u, 0xc5172000u, 0xc5172000u, 0xc3c494a0u, 0xc50e5804u},
    {0x5f834da7u, 0x5f834da7u, 0xdf834da7u, 0x5f834da7u, 0x5f834da7u, 0x5f834da7u, 0x5f834da7u, 0x5f834da7u, 0x5f834da7u, 0x5f834da7u},
    {0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu},
    {0x42750eeeu, 0x42750eeeu, 0xc2750eeeu, 0x42750eeeu, 0x42750eeau, 0x42750eeeu, 0x427558beu, 0xc3f314e8u, 0x42750eeeu, 0x42750eeau},
    {0x43380748u, 0x43380748u, 0xc3380748u, 0x43380746u, 0x43380747u, 0x43380748u, 0x43390747u, 0x4339073fu, 0x43380747u, 0x43380747u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, 0x7f800000u, 0xffc00000u, 0xffc00000u, 0x7f800000u, 0xffc00000u, 0x7f800000u},
    {0xf540cddbu, 0xf540cddbu, 0x7540cddbu, 0xf540cddbu, 0xf540cddbu, 0xf540cddbu, 0xf540cddbu, 0xf540cddbu, 0xf540cddbu, 0xf540cddbu},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0xffc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x7fe00000u, 0x7fe00000u, 0xffe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u, 0x7fe00000u},
    {0x3cb7c720u, 0x3cb7c720u, 0xbcb7c720u, 0xbcb7c720u, 0x3f106b70u, 0x3cb7c720u, 0x438fe000u, 0x46669f00u, 0x3d9bd158u, 0x3f106b70u},
    {0xbf9bd158u, 0xbf9bd158u, 0x3f9bd158u, 0x3f9bd158u, 0x398aa000u, 0xbf9bd158u, 0xc58fe000u, 0xc58fd000u, 0xbeb7c720u, 0x398aa000u},
    {0x565f49ebu, 0x565f49ebu, 0xd65f49ebu, 0x565f49ebu, 0x565f49ebu, 0x565f49ebu, 0x565f49ebu, 0x565f49ebu, 0x565f49ebu, 0x565f49ebu},
    {0xbaf43c38u, 0xbaf43c38u, 0x3af43c38u, 0x3af43c38u, 0x3abe63ecu, 0xbaf43c38u, 0xc021e000u, 0x3fb50000u, 0xba90cd60u, 0x3abe63ecu}
};

constexpr const char* Names[10] = {"v_dot2c", "v_dot2", "v_dot2 neg_lo src0/src2 neg_hi src1", "v_dot2 neg_lo src1 neg_hi src0", "v_dot2 op_sel src0", "v_dot2 clamp", "v_dot2c inline 1.0", "v_dot2c literal", "v_dot2c dpp quad_perm", "v_dot2 op_sel src1"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    for (std::uint32_t i = 0; i < 4; ++i) words[i] = Rows[tid][i];
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
    Require(actual == expected, std::string("v_dot2_f32_f16: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
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

void CheckRefused(AgcDriver::VulkanDevice& device, std::uint32_t word0, const std::string& what) {
    alignas(256) const std::array<std::uint32_t, 3> code{word0, 0x1c1a0b04u, 0xbf810000u};
    std::string refusal;
    try {
        static_cast<void>(Compile(device, code));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("v_dot2_f32_f16 accumulator op_sel, op_sel_hi and neg_hi are not implemented") != std::string::npos, what + " was not refused");
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    const std::span<const std::uint32_t> code(Code);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 10; ++i) Expect(tid, out[i], Expected[tid][i], Names[i]);
    }
}

const Testing::Case dotProducts{"Float16Dot_Dot2F32F16Variants_MatchReferenceLanes", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityFloat64, "v_dot2_f32_f16 is computed in f64 and the device has no shaderFloat64");
    Run(device);
    Check();
}};

const Testing::Case accumulatorModifiers{"Float16Dot_AccumulatorOpSelOrNegHi_IsRefused", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityFloat64, "v_dot2_f32_f16 is computed in f64 and the device has no shaderFloat64");
    CheckRefused(device, 0xcc13600au, "op_sel on the accumulator");
    CheckRefused(device, 0xcc13440au, "neg_hi on the accumulator");
    CheckRefused(device, 0xcc13000au, "op_sel_hi cleared on the accumulator");
}};

} // namespace
