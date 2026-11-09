#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * 2> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 24> Code{
    0x34020083, 0xe0341000, 0x80000201, 0x34080082, 0xbf8c3f70, 0xd8340000, 0x00000204, 0xd8340400,
    0x00000204, 0xbf8cc07f, 0xd9540000, 0x05000304, 0xd8540400, 0x00000304, 0xbf8cc07f, 0xd8d80000,
    0x06000004, 0xd8d80400, 0x07000004, 0x34020084, 0xbf8cc07f, 0xe07c1000, 0x80010501, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 2>, Threads> Operands{{
    {0x7f800000, 0xff800000}, {0xff800000, 0x7f800000}, {0x3f800000, 0xbf800000}, {0x80000000, 0x80000000},
    {0x00000000, 0x80000000}, {0x7f7fffff, 0x7f7fffff}, {0x7fc00000, 0xffc00001}, {0x7f800001, 0x3f800000},
    {0x3f800000, 0xff812345}, {0xffc00001, 0x7f800001}, {0x00000001, 0x80000001}, {0x00800000, 0x80000001},
    {0x8110c65e, 0x8074b225}, {0x00d42774, 0x80d0391a}, {0x80800000, 0x00800001}, {0x80431a89, 0x01bcb83b},
    {0x01bec9b9, 0x81bab7f2}, {0x007fffff, 0x00400000}, {0x80000000, 0x80000001}, {0x80f9f2ca, 0x00cd2d23},
    {0x005921e2, 0x8187bc63}, {0x014e8a46, 0x0060b9ff}, {0x81cf89be, 0x805706c5}, {0x00e62d65, 0x80e62d68},
    {0x00a66021, 0x800b6944}, {0x802976a1, 0x0156a967}, {0x0197eca0, 0x803c270f}, {0x8027bc33, 0x00f88e02},
    {0x0104a791, 0x80c6fa02}, {0x001f9fb3, 0x01e94ee7}, {0x80000000, 0x007fffff}, {0x007fffff, 0x007fffff},
    {0x010d380e, 0x0076291d}, {0x80000001, 0x80000001}, {0x81dd408a, 0x8023bed7}, {0x00000001, 0x00400000},
    {0x8033deaa, 0x001a010b}, {0x003da089, 0x01a8d6f4}, {0x002eafb3, 0x01f74c53}, {0x01fd86b4, 0x00195d7b},
    {0x80476063, 0x00b49438}, {0x00800001, 0x80000001}, {0xe34217a6, 0x5b100f9a}, {0xff7fffff, 0x00800000},
    {0x00400000, 0xcb000001}, {0x4b000000, 0x807fffff}, {0xa63b9449, 0x2d91f2c3}, {0x819c564f, 0x802c240f},
    {0x81b5b8a2, 0x81a1681d}, {0xe640b291, 0x5766d41e}, {0x334bd7d6, 0x3d01a33b}, {0x01eeda5d, 0x80e7c8f6},
    {0x7f800001, 0x80000001}, {0xff812345, 0x80000000}, {0xffc00001, 0x7f7fffff}, {0xff812345, 0x7f7fffff},
    {0xffc00001, 0x00800001}, {0xff812345, 0x80800000}, {0x6ef2f0aa, 0xeef2feaa}, {0x019a87f3, 0x80efc545},
    {0x71223030, 0xf122303c}, {0xff7fffff, 0xff7fffff}, {0x80ec8133, 0x005f7c8c}, {0xe914c8da, 0x6914c8db},
}};

constexpr std::array<std::array<std::uint32_t, Threads>, 5> Measured{{
    {
        0xffc00000, 0xffc00000, 0x00000000, 0x80000000, 0x00000000, 0x7f800000, 0xffc00001, 0x7fc00001,
        0xffc12345, 0x7fc00001, 0x00000000, 0x00800000, 0x8110c65e, 0x00000000, 0x00000000, 0x01bcb83b,
        0x00000000, 0x00000000, 0x80000000, 0x80000000, 0x8187bc63, 0x014e8a46, 0x81cf89be, 0x80000000,
        0x00a66021, 0x0156a967, 0x0197eca0, 0x00f88e02, 0x00000000, 0x01e94ee7, 0x00000000, 0x00000000,
        0x010d380e, 0x80000000, 0x81dd408a, 0x00000000, 0x00000000, 0x01a8d6f4, 0x01f74c53, 0x01fd86b4,
        0x00b49438, 0x00800001, 0xe3421716, 0xff7fffff, 0xcb000001, 0x4b000000, 0x2d91f14c, 0x819c564f,
        0x822b9060, 0xe640b291, 0x3d01a348, 0x01b4e820, 0x7fc00001, 0xffc12345, 0xffc00001, 0xffc12345,
        0xffc00001, 0xffc12345, 0xe8e00000, 0x013d2d44, 0xe7400000, 0xff800000, 0x80ec8133, 0x5d800000,
    },
    {
        0xffc00000, 0xffc00000, 0x00000000, 0x80000000, 0x00000000, 0x7f800000, 0xffc00001, 0x7fc00001,
        0xffc12345, 0x7fc00001, 0x00000000, 0x00000000, 0x814b1f70, 0x00000000, 0x00000000, 0x01abf199,
        0x00000000, 0x00bfffff, 0x80000000, 0x80000000, 0x8162e7d5, 0x017ee746, 0x81e54b6f, 0x80000000,
        0x009af6dd, 0x0141ee16, 0x0188e2dc, 0x00d0d1cf, 0x00000000, 0x01f136d4, 0x00000000, 0x00fffffe,
        0x01484c9c, 0x80000000, 0x81e63040, 0x00000000, 0x80000000, 0x01b83f16, 0x02017c20, 0x0201ef09,
        0x00000000, 0x00800000, 0xe3421716, 0xff7fffff, 0xcb000001, 0x4b000000, 0x2d91f14c, 0x81a75f53,
        0x822b9060, 0xe640b291, 0x3d01a348, 0x01b4e820, 0x7fc00001, 0xffc12345, 0xffc00001, 0xffc12345,
        0xffc00001, 0xffc12345, 0xe8e00000, 0x013d2d44, 0xe7400000, 0xff800000, 0x808d04a7, 0x5d800000,
    },
    {
        0xffc00000, 0xffc00000, 0x00000000, 0x80000000, 0x00000000, 0x7f800000, 0xffc00001, 0x7fc00001,
        0xffc12345, 0x7fc00001, 0x00000000, 0x00800000, 0x8110c65e, 0x0003ee5a, 0x00000001, 0x01bcb83b,
        0x0010471c, 0x00000000, 0x80000000, 0x802cc5a7, 0x8187bc63, 0x014e8a46, 0x81cf89be, 0x80000003,
        0x00a66021, 0x0156a967, 0x0197eca0, 0x00f88e02, 0x00425520, 0x01e94ee7, 0x00000000, 0x00000000,
        0x010d380e, 0x80000000, 0x81dd408a, 0x00000000, 0x00000000, 0x01a8d6f4, 0x01f74c53, 0x01fd86b4,
        0x00b49438, 0x00800001, 0xe3421716, 0xff7fffff, 0xcb000001, 0x4b000000, 0x2d91f14c, 0x819c564f,
        0x822b9060, 0xe640b291, 0x3d01a348, 0x01b4e820, 0x7fc00001, 0xffc12345, 0xffc00001, 0xffc12345,
        0xffc00001, 0xffc12345, 0xe8e00000, 0x013d2d44, 0xe7400000, 0xff800000, 0x80ec8133, 0x5d800000,
    },
    {
        0xffc00000, 0xffc00000, 0x00000000, 0x80000000, 0x00000000, 0x7f800000, 0xffc00001, 0x7fc00001,
        0xffc12345, 0x7fc00001, 0x00000000, 0x007fffff, 0x814b1f70, 0x0003ee5a, 0x00000001, 0x01abf199,
        0x0010471c, 0x00bfffff, 0x80000001, 0x802cc5a7, 0x8162e7d5, 0x017ee746, 0x81e54b6f, 0x80000003,
        0x009af6dd, 0x0141ee16, 0x0188e2dc, 0x00d0d1cf, 0x00425520, 0x01f136d4, 0x007fffff, 0x00fffffe,
        0x01484c9c, 0x80000002, 0x81e63040, 0x00400001, 0x8019dd9f, 0x01b83f16, 0x02017c20, 0x0201ef09,
        0x006d33d5, 0x00800000, 0xe3421716, 0xff7fffff, 0xcb000001, 0x4b000000, 0x2d91f14c, 0x81a75f53,
        0x822b9060, 0xe640b291, 0x3d01a348, 0x01b4e820, 0x7fc00001, 0xffc12345, 0xffc00001, 0xffc12345,
        0xffc00001, 0xffc12345, 0xe8e00000, 0x013d2d44, 0xe7400000, 0xff800000, 0x808d04a7, 0x5d800000,
    },
    {
        0xffc00000, 0xffc00000, 0x00000000, 0x80000000, 0x00000000, 0x7f800000, 0xffc00001, 0x7fc00001,
        0xffc12345, 0x7fc00001, 0x00000000, 0x007fffff, 0x814b1f70, 0x0003ee5a, 0x00000001, 0x01abf199,
        0x0010471c, 0x00bfffff, 0x80000001, 0x802cc5a7, 0x8162e7d5, 0x017ee746, 0x81e54b6f, 0x80000003,
        0x009af6dd, 0x0141ee16, 0x0188e2dc, 0x00d0d1cf, 0x00425520, 0x01f136d4, 0x007fffff, 0x00fffffe,
        0x01484c9c, 0x80000002, 0x81e63040, 0x00400001, 0x8019dd9f, 0x01b83f16, 0x02017c20, 0x0201ef09,
        0x006d33d5, 0x00800000, 0xe3421716, 0xff7fffff, 0xcb000001, 0x4b000000, 0x2d91f14c, 0x81a75f53,
        0x822b9060, 0xe640b291, 0x3d01a348, 0x01b4e820, 0x7fc00001, 0xffc12345, 0xffc00001, 0xffc12345,
        0xffc00001, 0xffc12345, 0xe8e00000, 0x013d2d44, 0xe7400000, 0xff800000, 0x808d04a7, 0x5d800000,
    },
}};

constexpr std::array<std::uint32_t, 5> FloatModes{0x00u | (3u << 6u), 0x10u | (3u << 6u), 0x20u | (3u << 6u), 0x30u | (3u << 6u), 0x33u | (3u << 6u)};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 2u] = Operands[tid][0];
        Input[tid * 2u + 1u] = Operands[tid][1];
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 2048u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

bool Denormal(std::uint32_t value) {
    return (value & 0x7f800000u) == 0u && (value & 0x007fffffu) != 0u;
}

bool HostDependent(std::uint32_t mode, std::uint32_t tid) {
    const bool keepInputs = ((FloatModes[mode] >> 4u) & 1u) != 0u;
    const bool keepResult = ((FloatModes[mode] >> 5u) & 1u) != 0u;
    return (keepInputs && (Denormal(Operands[tid][0]) || Denormal(Operands[tid][1]))) || (keepResult && Denormal(Measured[mode][tid]));
}

std::uint32_t Check(std::uint32_t mode) {
    std::uint32_t checked = 0;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        if (HostDependent(mode, tid)) continue;
        ++checked;
        const std::uint32_t* out = &Output[tid * Results];
        const std::string row = "FLOAT_MODE " + Hex(FloatModes[mode]) + " lane " + std::to_string(tid) + " (" + Hex(Operands[tid][0]) + " + " + Hex(Operands[tid][1]) + ")";
        Require(out[0] == Operands[tid][0], "lds add f32: ds_add_rtn_f32 " + row + " returned " + Hex(out[0]));
        Require(out[1] == Measured[mode][tid], "lds add f32: ds_add_rtn_f32 " + row + " stored " + Hex(out[1]) + ", expected " + Hex(Measured[mode][tid]));
        Require(out[2] == Measured[mode][tid], "lds add f32: ds_add_f32 " + row + " stored " + Hex(out[2]) + ", expected " + Hex(Measured[mode][tid]));
    }
    return checked;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t checked = 0;
        for (std::uint32_t mode = 0; mode < FloatModes.size(); ++mode) {
            Run(*device, FloatModes[mode]);
            checked += Check(mode);
        }
        Require(checked >= 200u, "lds add f32: only " + std::to_string(checked) + " rows were independent of host denormal preservation");
        std::puts("lds add f32 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
