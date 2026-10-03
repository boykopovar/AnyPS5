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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t ActiveLanes = 16;
constexpr std::uint32_t LdsDwords = 256;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr auto Code = std::to_array<std::uint32_t>({
    0x34020082, 0x34060084, 0x343c0082,
    0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601, 0xe030200c, 0x80000701,
    0xbf8c3f70,
    0xd8340000, 0x0000041e, 0xd8340080, 0x0000051e, 0xd8340100, 0x0000051e, 0xd8340300, 0x0000041e,
    0xd8340180, 0x0000051e, 0xd8340280, 0x0000041e, 0xd8340380, 0x0000041e, 0xd8340200, 0x0000051e,
    0xbf8cc07f,
    0x7e1c0306, 0x7e1e0307, 0x7e200306, 0x7e220307,
    0xd8b82000, 0x0a07061e,
    0xd8bc0103, 0x0c06071e,
    0xd8b8a060, 0x0e0f0e1e,
    0x7da80090,
    0xd8b880e0, 0x1007061e,
    0xbefe03c1,
    0xbf8cc07f,
    0xd8d80000, 0x1400001e, 0xd8d80080, 0x1500001e, 0xd8d80300, 0x1600001e, 0xd8d80100, 0x1700001e,
    0xd8d80180, 0x1800001e, 0xd8d80280, 0x1900001e, 0xd8d80380, 0x1a00001e, 0xd8d80200, 0x1b00001e,
    0xbf8cc07f,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03,
    0xe0702010, 0x80010e03, 0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c, 0x80011103,
    0xe0702020, 0x80011403, 0xe0702024, 0x80011503, 0xe0702028, 0x80011603, 0xe070202c, 0x80011703,
    0xe0702030, 0x80011803, 0xe0702034, 0x80011903, 0xe0702038, 0x80011a03, 0xe070203c, 0x80011b03,
    0xbf810000,
});

alignas(256) constexpr auto CoincidingOffsets = std::to_array<std::uint32_t>({0xd8b80505, 0x0a07061e, 0xbf810000});
alignas(256) constexpr auto CoincidingOffsetsSt64 = std::to_array<std::uint32_t>({0xd8bc0000, 0x0a07061e, 0xbf810000});
alignas(256) constexpr auto DestinationOverflow = std::to_array<std::uint32_t>({0xd8b82000, 0xff07061e, 0xbf810000});

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

template<typename TUse>
void Translate(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, TUse&& use) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    use(ShaderRecompiler::Recompile(request));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t word = 0; word < Inputs; ++word) {
            Input[tid * Inputs + word] = ((word + 1u) << 28u) | (tid * 0x00010203u + word);
        }
    }
    Output.fill(0xdeadbeefu);
    Translate(device, Code, [&](const auto& result) {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device.WaitIdle();
    });
}

void Reject(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    std::string error;
    try {
        Translate(device, code, [](const auto&) {});
    } catch (const std::exception& exception) {
        error = exception.what();
    }
    Require(error.find(reason) != std::string::npos, "ds wrxchg2: expected a rejection for \"" + reason + "\", got \"" + error + "\"");
}

void CheckRejections(AgcDriver::VulkanDevice& device) {
    Reject(device, CoincidingOffsets, "DS write exchange of one location through both offsets is not supported");
    Reject(device, CoincidingOffsetsSt64, "DS write exchange of one location through both offsets is not supported");
    Reject(device, DestinationOverflow, "DS write exchange destination register range overflow");
}

void Check() {
    constexpr std::array<const char*, Results> names{
        "ds_wrxchg2_rtn_b32 offset0 result", "ds_wrxchg2_rtn_b32 offset1 result",
        "ds_wrxchg2st64_rtn_b32 offset0 result", "ds_wrxchg2st64_rtn_b32 offset1 result",
        "aliased ds_wrxchg2_rtn_b32 offset0 result", "aliased ds_wrxchg2_rtn_b32 offset1 result",
        "masked ds_wrxchg2_rtn_b32 offset0 result", "masked ds_wrxchg2_rtn_b32 offset1 result",
        "ds_wrxchg2_rtn_b32 offset0 memory", "ds_wrxchg2_rtn_b32 offset1 memory",
        "ds_wrxchg2st64_rtn_b32 offset0 memory", "ds_wrxchg2st64_rtn_b32 offset1 memory",
        "aliased ds_wrxchg2_rtn_b32 offset0 memory", "aliased ds_wrxchg2_rtn_b32 offset1 memory",
        "masked ds_wrxchg2_rtn_b32 offset0 memory", "masked ds_wrxchg2_rtn_b32 offset1 memory",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t a = Input[tid * Inputs];
        const std::uint32_t b = Input[tid * Inputs + 1];
        const std::uint32_t c = Input[tid * Inputs + 2];
        const std::uint32_t d = Input[tid * Inputs + 3];
        const bool active = tid < ActiveLanes;
        const std::array<std::uint32_t, Results> expected{
            a, b, a, b, b, a, active ? a : c, active ? b : d,
            c, d, d, c, c, d, active ? c : a, active ? d : b,
        };
        for (std::uint32_t j = 0; j < Results; ++j) {
            const auto actual = Output[tid * Results + j];
            Require(actual == expected[j], std::string("ds wrxchg2: lane ") + std::to_string(tid) + " " + names[j] + " is " + Hex(actual) + ", expected " + Hex(expected[j]));
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
        CheckRejections(*device);
        std::puts("ds wrxchg2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
