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

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 81> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280,
    0x7e1c0280, 0x7e1e0280, 0x7e200280, 0x7e220280, 0x7e240280, 0x7e260280, 0x7e280280, 0xbf8c3f70,
    0x7e3c0304, 0xd525001f, 0x00010304, 0xd5250020, 0x00010504, 0xd5250021, 0x00010704, 0xd5250022,
    0x00010904, 0xd5250023, 0x00010b04, 0xd5250024, 0x00010d04, 0xd5250025, 0x00010f04, 0xbefc0382,
    0x7e3e891e, 0x7e140321, 0xbefc03ff, 0xfc03fc02, 0x7e3e911e, 0x7e160322, 0x7e18031f, 0x4a4008a0,
    0x4a4208b0, 0xbefc03ff, 0x00010003, 0x7e3ed11e, 0x7e1a0320, 0x7e1c0321, 0x7e1e031e, 0x7e48cb25,
    0x7e200324, 0x7e220325, 0xbefc03ff, 0x00020002, 0x7e44d122, 0x7e240324, 0x7e260323, 0xd5c40023,
    0x0000011f, 0x7e280323, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03,
    0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014, 0x80010f03, 0xe0701018, 0x80011003,
    0xe070101c, 0x80011103, 0xe0701020, 0x80011203, 0xe0701024, 0x80011303, 0xe0701028, 0x80011403,
    0xbf810000,
};

constexpr std::uint32_t Expected[32][11] = {
    {0x00001002u, 0x00001002u, 0x00001001u, 0x00001030u, 0x00001020u, 0x00001000u, 0x00001007u, 0x00001006u, 0x00001007u, 0x00001005u, 0x00001005u},
    {0x00002002u, 0x00002002u, 0x00002001u, 0x00002030u, 0x00002020u, 0x00002000u, 0x00002007u, 0x00002006u, 0x00002007u, 0x00002005u, 0x00002005u},
    {0x00003002u, 0x00003002u, 0x00003001u, 0x00003030u, 0x00003020u, 0x00003000u, 0x00003007u, 0x00003006u, 0x00003007u, 0x00003005u, 0x00003005u},
    {0x00004002u, 0x00004002u, 0x00004001u, 0x00004030u, 0x00004020u, 0x00004000u, 0x00004007u, 0x00004006u, 0x00004007u, 0x00004005u, 0x00004005u},
    {0x00005002u, 0x00005002u, 0x00005001u, 0x00005030u, 0x00005020u, 0x00005000u, 0x00005007u, 0x00005006u, 0x00005007u, 0x00005005u, 0x00005005u},
    {0x00006002u, 0x00006002u, 0x00006001u, 0x00006030u, 0x00006020u, 0x00006000u, 0x00006007u, 0x00006006u, 0x00006007u, 0x00006005u, 0x00006005u},
    {0x00007002u, 0x00007002u, 0x00007001u, 0x00007030u, 0x00007020u, 0x00007000u, 0x00007007u, 0x00007006u, 0x00007007u, 0x00007005u, 0x00007005u},
    {0x00008002u, 0x00008002u, 0x00008001u, 0x00008030u, 0x00008020u, 0x00008000u, 0x00008007u, 0x00008006u, 0x00008007u, 0x00008005u, 0x00008005u},
    {0x00009002u, 0x00009002u, 0x00009001u, 0x00009030u, 0x00009020u, 0x00009000u, 0x00009007u, 0x00009006u, 0x00009007u, 0x00009005u, 0x00009005u},
    {0x0000a002u, 0x0000a002u, 0x0000a001u, 0x0000a030u, 0x0000a020u, 0x0000a000u, 0x0000a007u, 0x0000a006u, 0x0000a007u, 0x0000a005u, 0x0000a005u},
    {0x0000b002u, 0x0000b002u, 0x0000b001u, 0x0000b030u, 0x0000b020u, 0x0000b000u, 0x0000b007u, 0x0000b006u, 0x0000b007u, 0x0000b005u, 0x0000b005u},
    {0x0000c002u, 0x0000c002u, 0x0000c001u, 0x0000c030u, 0x0000c020u, 0x0000c000u, 0x0000c007u, 0x0000c006u, 0x0000c007u, 0x0000c005u, 0x0000c005u},
    {0x0000d002u, 0x0000d002u, 0x0000d001u, 0x0000d030u, 0x0000d020u, 0x0000d000u, 0x0000d007u, 0x0000d006u, 0x0000d007u, 0x0000d005u, 0x0000d005u},
    {0x0000e002u, 0x0000e002u, 0x0000e001u, 0x0000e030u, 0x0000e020u, 0x0000e000u, 0x0000e007u, 0x0000e006u, 0x0000e007u, 0x0000e005u, 0x0000e005u},
    {0x0000f002u, 0x0000f002u, 0x0000f001u, 0x0000f030u, 0x0000f020u, 0x0000f000u, 0x0000f007u, 0x0000f006u, 0x0000f007u, 0x0000f005u, 0x0000f005u},
    {0x00010002u, 0x00010002u, 0x00010001u, 0x00010030u, 0x00010020u, 0x00010000u, 0x00010007u, 0x00010006u, 0x00010007u, 0x00010005u, 0x00010005u},
    {0x00011002u, 0x00011002u, 0x00011001u, 0x00011030u, 0x00011020u, 0x00011000u, 0x00011007u, 0x00011006u, 0x00011007u, 0x00011005u, 0x00011005u},
    {0x00012002u, 0x00012002u, 0x00012001u, 0x00012030u, 0x00012020u, 0x00012000u, 0x00012007u, 0x00012006u, 0x00012007u, 0x00012005u, 0x00012005u},
    {0x00013002u, 0x00013002u, 0x00013001u, 0x00013030u, 0x00013020u, 0x00013000u, 0x00013007u, 0x00013006u, 0x00013007u, 0x00013005u, 0x00013005u},
    {0x00014002u, 0x00014002u, 0x00014001u, 0x00014030u, 0x00014020u, 0x00014000u, 0x00014007u, 0x00014006u, 0x00014007u, 0x00014005u, 0x00014005u},
    {0x00015002u, 0x00015002u, 0x00015001u, 0x00015030u, 0x00015020u, 0x00015000u, 0x00015007u, 0x00015006u, 0x00015007u, 0x00015005u, 0x00015005u},
    {0x00016002u, 0x00016002u, 0x00016001u, 0x00016030u, 0x00016020u, 0x00016000u, 0x00016007u, 0x00016006u, 0x00016007u, 0x00016005u, 0x00016005u},
    {0x00017002u, 0x00017002u, 0x00017001u, 0x00017030u, 0x00017020u, 0x00017000u, 0x00017007u, 0x00017006u, 0x00017007u, 0x00017005u, 0x00017005u},
    {0x00018002u, 0x00018002u, 0x00018001u, 0x00018030u, 0x00018020u, 0x00018000u, 0x00018007u, 0x00018006u, 0x00018007u, 0x00018005u, 0x00018005u},
    {0x00019002u, 0x00019002u, 0x00019001u, 0x00019030u, 0x00019020u, 0x00019000u, 0x00019007u, 0x00019006u, 0x00019007u, 0x00019005u, 0x00019005u},
    {0x0001a002u, 0x0001a002u, 0x0001a001u, 0x0001a030u, 0x0001a020u, 0x0001a000u, 0x0001a007u, 0x0001a006u, 0x0001a007u, 0x0001a005u, 0x0001a005u},
    {0x0001b002u, 0x0001b002u, 0x0001b001u, 0x0001b030u, 0x0001b020u, 0x0001b000u, 0x0001b007u, 0x0001b006u, 0x0001b007u, 0x0001b005u, 0x0001b005u},
    {0x0001c002u, 0x0001c002u, 0x0001c001u, 0x0001c030u, 0x0001c020u, 0x0001c000u, 0x0001c007u, 0x0001c006u, 0x0001c007u, 0x0001c005u, 0x0001c005u},
    {0x0001d002u, 0x0001d002u, 0x0001d001u, 0x0001d030u, 0x0001d020u, 0x0001d000u, 0x0001d007u, 0x0001d006u, 0x0001d007u, 0x0001d005u, 0x0001d005u},
    {0x0001e002u, 0x0001e002u, 0x0001e001u, 0x0001e030u, 0x0001e020u, 0x0001e000u, 0x0001e007u, 0x0001e006u, 0x0001e007u, 0x0001e005u, 0x0001e005u},
    {0x0001f002u, 0x0001f002u, 0x0001f001u, 0x0001f030u, 0x0001f020u, 0x0001f000u, 0x0001f007u, 0x0001f006u, 0x0001f007u, 0x0001f005u, 0x0001f005u},
    {0x00020002u, 0x00020002u, 0x00020001u, 0x00020030u, 0x00020020u, 0x00020000u, 0x00020007u, 0x00020006u, 0x00020007u, 0x00020005u, 0x00020005u},
};
constexpr const char* Names[11] = {"movrelsd", "swapped_2", "sd2_untouched", "swaprel_dst", "swaprel_src", "swaprel_other", "swap_dst", "swap_src", "swaprel_same", "swaprel_same_other", "movrelsd_e64_out_of_range"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    words[0] = 0x1000u * (tid + 1u);
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
    Testing::Require(actual == expected, std::string("relative indexing: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        for (std::uint32_t index = 0; index < 11; ++index) Expect(tid, out[index], Expected[tid][index], Names[index]);
    }
}

const Testing::Case relativeIndexing{"RelativeIndexing_MovrelAndSwaprel_MatchExpectedTable", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
