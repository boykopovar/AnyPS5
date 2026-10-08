#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t Slots = 8;
constexpr std::uint32_t Checked = 8;
constexpr std::uint32_t InputBytes = 256;
constexpr std::uint32_t ViewBytes = 60;
constexpr std::uint32_t ResultBytes = Lanes * Slots * 4;
constexpr std::uint32_t StoredBytes = 8 * 28;
constexpr std::uint32_t Fill = 0xdeadbeefu;
alignas(256) std::array<std::uint8_t, InputBytes> Input{};
alignas(256) std::array<std::uint32_t, 384> Output{};

alignas(256) constexpr std::array<std::uint32_t, 56> Code{
    0x34020082, 0x34040085, 0x34100081, 0x4a101082, 0x34140081, 0x4a141500, 0x4a141481, 0x34180083,
    0x4a1a00ff, 0x5a5abe00, 0x4a1c00ff, 0x7f7f7f91, 0x4a1e00ff, 0xa1b2c300, 0x4a2000bc, 0xe0301000,
    0x80000301, 0xe0301000, 0x80010401, 0xe0341004, 0x80010501, 0xe0281000, 0x80010708, 0xe0201000,
    0x8001090a, 0xe8281002, 0x80011101, 0xf4200402, 0xfa000006, 0xbf8c0000, 0x7e160210, 0xe0701000,
    0x80020302, 0xe0701004, 0x80020402, 0xe0701008, 0x80020502, 0xe070100c, 0x80020602, 0xe0701010,
    0x80020702, 0xe0701014, 0x80020902, 0xe0701018, 0x80020b02, 0xe070101c, 0x80021102, 0xe0701000,
    0x80030f0c, 0xe0601004, 0x80030e0c, 0xe0681006, 0x80030d0c, 0xe82c1005, 0x8003100c, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PairCode{
    0x34040083, 0x7e0a0281, 0x7e0c0280, 0xe0345000, 0x80010300, 0xe1480100, 0x80020500, 0xbf8c0000,
    0xe0741000, 0x80020302, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> AtomicCode{0xe0c81000, 0x80000100, 0xbf810000};

constexpr std::array<std::uint32_t, 7> FormattedLoadCode(std::uint32_t format, std::uint32_t offset) {
    return {0x34020082, 0xe8001000u | (format << 19u) | offset, 0x80000201, 0xbf8c0000, 0xe0701000, 0x80010201, 0xbf810000};
}

constexpr std::array<std::uint32_t, 6> FormattedStoreCode(std::uint32_t format, std::uint32_t offset) {
    return {0x34020082, 0x4a0400ff, 0x0000c300, 0xe8041000u | (format << 19u) | offset, 0x80000201, 0xbf810000};
}

constexpr std::uint32_t Format16UInt = 11;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format16_16UInt = 27;
alignas(256) constexpr auto Load16Code = FormattedLoadCode(Format16UInt, 2);
alignas(256) constexpr auto Load32Code = FormattedLoadCode(Format32UInt, 0);
alignas(256) constexpr auto Load16_16Code = FormattedLoadCode(Format16_16UInt, 0);
alignas(256) constexpr auto Store16Code = FormattedStoreCode(Format16UInt, 0);

constexpr const char* Names[Checked] = {
    "aligned dword", "dword", "dwordx2[0]", "dwordx2[1]", "ushort", "ubyte", "s_buffer_load_dword", "tbuffer_load_format_x 8_UINT",
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t ViewDword(const std::uint8_t* view, std::uint32_t dword) {
    if (dword >= ViewBytes / 4) return 0;
    std::uint32_t value = 0;
    std::memcpy(&value, view + dword * 4u, sizeof(value));
    return value;
}

std::uint32_t ViewField(const std::uint8_t* view, std::uint32_t byte, std::uint32_t mask) {
    return (ViewDword(view, byte / 4u) >> ((byte % 4u) * 8u)) & mask;
}

std::uint8_t FillByte(std::size_t offset) {
    return static_cast<std::uint8_t>(Fill >> ((offset % 4u) * 8u));
}

ShaderRecompiler::RecompileRequest Request(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::SpirvTarget& target, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return request;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t viewOffset, std::uint32_t storeOffset) {
    for (std::uint32_t index = 0; index < InputBytes; ++index) Input[index] = static_cast<std::uint8_t>(index * 37u + 11u + (index >> 3u));
    Output.fill(Fill);
    auto* output = reinterpret_cast<std::uint8_t*>(Output.data());
    const auto* view = Input.data() + viewOffset;
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(Input.data(), InputBytes), BufferDescriptor(view, ViewBytes), BufferDescriptor(Output.data(), ResultBytes), BufferDescriptor(output + ResultBytes + storeOffset, StoredBytes)}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const auto result = ShaderRecompiler::Recompile(Request(Code, userData, device.Target(), memory));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    const auto run = "view +" + std::to_string(viewOffset) + ", stores +" + std::to_string(storeOffset);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        std::uint32_t aligned = 0;
        std::memcpy(&aligned, Input.data() + lane * 4u, sizeof(aligned));
        std::uint32_t scalar = 0;
        std::memcpy(&scalar, Input.data() + (viewOffset & ~3u) + 4u, sizeof(scalar));
        const std::uint32_t expected[Checked] = {aligned, ViewDword(view, lane), ViewDword(view, lane + 1u), ViewDword(view, lane + 2u), ViewField(view, lane * 2u + 2u, 0xffffu), ViewField(view, lane * 3u + 1u, 0xffu), scalar, ViewField(view, lane * 4u + 2u, 0xffu)};
        for (std::uint32_t slot = 0; slot < Checked; ++slot) {
            const auto actual = Output[lane * Slots + slot];
            Require(actual == expected[slot], "buffer unaligned base " + run + ": lane " + std::to_string(lane) + " " + Names[slot] + " is " + Hex(actual) + ", expected " + Hex(expected[slot]));
        }
    }
    for (std::size_t offset = ResultBytes; offset < Output.size() * 4u; ++offset) {
        const auto relative = static_cast<std::int64_t>(offset) - static_cast<std::int64_t>(ResultBytes + storeOffset);
        auto expected = FillByte(offset);
        if (relative >= 0 && relative < StoredBytes) {
            const auto lane = static_cast<std::uint32_t>(relative / 8);
            const auto byte = static_cast<std::uint32_t>(relative % 8);
            if (byte < 4u) expected = static_cast<std::uint8_t>((0xa1b2c300u + lane) >> (byte * 8u));
            else if (byte == 4u) expected = static_cast<std::uint8_t>(0x91u + lane);
            else if (byte == 5u) expected = static_cast<std::uint8_t>(0x3cu + lane);
            else if (byte >= 6u) expected = static_cast<std::uint8_t>((0xbe00u + lane) >> ((byte - 6u) * 8u));
        }
        Require(output[offset] == expected, "buffer unaligned base " + run + ": stored byte " + std::to_string(offset - ResultBytes) + " is " + Hex(output[offset]) + ", expected " + Hex(expected));
    }
}

void RunPair(AgcDriver::VulkanDevice& device, std::uint32_t viewOffset) {
    for (std::uint32_t index = 0; index < InputBytes; ++index) Input[index] = static_cast<std::uint8_t>(index * 53u + 7u + (index >> 2u));
    Output.fill(Fill);
    const auto* view = Input.data() + viewOffset;
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(Input.data(), InputBytes), BufferDescriptor(view, ViewBytes), BufferDescriptor(Output.data(), ResultBytes)}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(PairCode.data()), std::as_bytes(std::span(PairCode))}}};
    const auto result = ShaderRecompiler::Recompile(Request(PairCode, userData, device.Target(), memory));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(PairCode.data()));
    device.WaitIdle();
    const auto run = "buffer unaligned base glc dwordx2, view +" + std::to_string(viewOffset);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        for (std::uint32_t half = 0; half < 2u; ++half) {
            const auto actual = Output[lane * 2u + half];
            const auto expected = ViewDword(view, lane / 4u + half);
            Require(actual == expected, run + ": lane " + std::to_string(lane) + " dword " + std::to_string(half) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
    Require(Output[Lanes * 2u] == Fill + Lanes && Output[Lanes * 2u + 1u] == Fill, run + ": the 64-bit atomic is " + Hex(Output[Lanes * 2u + 1u]) + Hex(Output[Lanes * 2u]));
}

auto RecompileFormatted(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::uint8_t* view, std::uint32_t viewBytes) {
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(view, viewBytes), BufferDescriptor(Output.data(), ResultBytes)}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    return ShaderRecompiler::Recompile(Request(code, userData, device.Target(), memory));
}

void RunFormattedFault(AgcDriver::VulkanDevice& device, const std::string& name, std::span<const std::uint32_t> code, std::uint32_t viewOffset) {
    const auto run = "buffer unaligned base " + name + ", view +" + std::to_string(viewOffset);
    try {
        static_cast<void>(RecompileFormatted(device, code, Input.data() + viewOffset, ViewBytes));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("is a formatted buffer access through a V# whose base is not aligned to its element") != std::string::npos, run + ": unexpected error: " + error.what());
        return;
    }
    throw std::runtime_error(run + ": a formatted access the hardware faults on was recompiled");
}

void RunFormattedLoad(AgcDriver::VulkanDevice& device, std::uint32_t viewOffset) {
    for (std::uint32_t index = 0; index < InputBytes; ++index) Input[index] = static_cast<std::uint8_t>(index * 29u + 5u + (index >> 4u));
    Output.fill(Fill);
    const auto* view = Input.data() + viewOffset;
    const auto result = RecompileFormatted(device, Load16Code, view, ViewBytes);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Load16Code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = ViewField(view, lane * 4u + 2u, 0xffffu);
        Require(Output[lane] == expected, "buffer unaligned base tbuffer_load_format_x 16_UINT, view +" + std::to_string(viewOffset) + ": lane " + std::to_string(lane) + " is " + Hex(Output[lane]) + ", expected " + Hex(expected));
    }
}

void RunFormattedStore(AgcDriver::VulkanDevice& device, std::uint32_t viewOffset) {
    Output.fill(Fill);
    auto* output = reinterpret_cast<std::uint8_t*>(Output.data());
    const auto result = RecompileFormatted(device, Store16Code, output + viewOffset, Lanes * 4u);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Store16Code.data()));
    device.WaitIdle();
    for (std::size_t offset = 0; offset < Output.size() * 4u; ++offset) {
        const auto relative = static_cast<std::int64_t>(offset) - static_cast<std::int64_t>(viewOffset);
        auto expected = FillByte(offset);
        if (relative >= 0 && relative < Lanes * 4 && relative % 4 < 2) expected = static_cast<std::uint8_t>((0xc300u + static_cast<std::uint32_t>(relative / 4)) >> ((relative % 4) * 8));
        Require(output[offset] == expected, "buffer unaligned base tbuffer_store_format_x 16_UINT, view +" + std::to_string(viewOffset) + ": byte " + std::to_string(offset) + " is " + Hex(output[offset]) + ", expected " + Hex(expected));
    }
}

void RunAtomic(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData;
    const auto descriptor = BufferDescriptor(reinterpret_cast<std::uint8_t*>(Output.data()) + 2u, 64u);
    userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(AtomicCode.data()), std::as_bytes(std::span(AtomicCode))}}};
    try {
        const auto result = ShaderRecompiler::Recompile(Request(AtomicCode, userData, device.Target(), memory));
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AtomicCode.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("buffer atomic on a V# whose base is not DWORD aligned") != std::string::npos, std::string("buffer unaligned base atomic: unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("buffer unaligned base atomic: an atomic on a V# off a DWORD boundary ran");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 2, 1);
        Run(*device, 3, 3);
        Run(*device, 1, 2);
        RunPair(*device, 1);
        RunPair(*device, 2);
        RunPair(*device, 3);
        RunAtomic(*device);
        RunFormattedLoad(*device, 2);
        RunFormattedStore(*device, 2);
        for (const auto view : {1u, 3u}) {
            RunFormattedFault(*device, "tbuffer_load_format_x 16_UINT", Load16Code, view);
            RunFormattedFault(*device, "tbuffer_store_format_x 16_UINT", Store16Code, view);
        }
        for (const auto view : {1u, 2u, 3u}) {
            RunFormattedFault(*device, "tbuffer_load_format_x 32_UINT", Load32Code, view);
            RunFormattedFault(*device, "tbuffer_load_format_x 16_16_UINT", Load16_16Code, view);
        }
        std::puts("buffer unaligned base tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
