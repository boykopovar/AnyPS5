#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t InputBytes = 16;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint8_t, Threads * InputBytes> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 107> D16Code{
    0x34020084, 0x34060086, 0xbe880300, 0x8709ff01, 0x0000ffff, 0x7e1402ff, 0xabcdabcd, 0x7e1602ff,
    0xabcdabcd, 0x7e1802ff, 0xabcdabcd, 0x7e1a02ff, 0xabcdabcd, 0x7e1c02ff, 0xabcdabcd, 0x7e1e02ff,
    0xabcdabcd, 0x7e2002ff, 0xabcdabcd, 0x7e2202ff, 0xabcdabcd, 0x7e2402ff, 0xabcdabcd, 0x7e2602ff,
    0xabcdabcd, 0x7e2802ff, 0xabcdabcd, 0x7e2a02ff, 0xabcdabcd, 0xe0801001, 0x80000a01, 0xe0841002,
    0x80000a01, 0xe0881003, 0x80000b01, 0xe08c1005, 0x80000b01, 0xe0901006, 0x80000c01, 0xe0941008,
    0x80000c01, 0xdc808001, 0x0d080001, 0xdc848002, 0x0d080001, 0xdc888003, 0x0e080001, 0xdc8c8005,
    0x0e080001, 0xdc908006, 0x0f080001, 0xdc948008, 0x0f080001, 0x34040082, 0xbf8c3f70, 0xe0301000,
    0x80000401, 0xbf8c3f70, 0xd8340000, 0x00000402, 0xbf8cc07f, 0xda880001, 0x10000002, 0xda8c0002,
    0x10000002, 0xda900003, 0x11000002, 0xda940001, 0x11000002, 0xda980002, 0x12000002, 0xda9c0000,
    0x12000002, 0xbf8cc07f, 0x7e2602ff, 0x80c1a2b3, 0xe0641028, 0x80011303, 0xe06c102c, 0x80011303,
    0xda800001, 0x00001302, 0xbf8cc07f, 0xd8d80000, 0x14000002, 0xbf8c0070, 0xe0701000, 0x80010a03,
    0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03,
    0xe0701014, 0x80010f03, 0xe0701018, 0x80011003, 0xe070101c, 0x80011103, 0xe0701020, 0x80011203,
    0xe0701024, 0x80011403, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t i = 0; i < Input.size(); ++i) Input[i] = static_cast<std::uint8_t>(i * 37u + 0x81u);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(D16Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 4u * Threads, {false, false, false}, false, 1};
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Check() {
    constexpr std::array<const char*, 12> names{
        "buffer_load_ubyte_d16(_hi)", "buffer_load_sbyte_d16(_hi)", "buffer_load_short_d16(_hi)",
        "global_load_ubyte_d16(_hi)", "global_load_sbyte_d16(_hi)", "global_load_short_d16(_hi)",
        "ds_read_u8_d16(_hi)", "ds_read_i8_d16(_hi)", "ds_read_u16_d16(_hi)", "ds_write_b8_d16_hi",
        "buffer_store_byte_d16_hi", "buffer_store_short_d16_hi",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint8_t* in = &Input[tid * InputBytes];
        const auto u8 = [&](std::uint32_t at) { return static_cast<std::uint32_t>(in[at]); };
        const auto i8 = [&](std::uint32_t at) { return static_cast<std::uint32_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(in[at]))) & 0xffffu; };
        const auto u16 = [&](std::uint32_t at) { return u8(at) | (u8(at + 1u) << 8u); };
        const std::uint32_t loads[3] = {u8(1) | (u8(2) << 16u), i8(3) | (i8(5) << 16u), u16(6) | (u16(8) << 16u)};
        std::uint32_t dword = 0;
        std::memcpy(&dword, in, 4);
        const std::uint32_t written = (dword & 0xffff00ffu) | (0xc1u << 8u);
        std::array<std::uint32_t, 12> expected{
            loads[0], loads[1], loads[2], loads[0], loads[1], loads[2],
            u8(1) | (u8(2) << 16u), i8(3) | (i8(1) << 16u), u16(2) | (u16(0) << 16u), written,
            0xdeadbec1u, 0xdead80c1u,
        };
        const auto* out = &Output[tid * Results];
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            Require(out[j] == expected[j], std::string("d16 memory: lane ") + std::to_string(tid) + " " + names[j] + " is " + Hex(out[j]) + ", expected " + Hex(expected[j]));
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
        std::puts("d16 memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
