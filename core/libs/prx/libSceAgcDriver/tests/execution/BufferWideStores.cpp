#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t OutputBytes = 2048;
alignas(256) std::array<std::uint8_t, OutputBytes> Output{};
std::deque<std::vector<std::uint32_t>> Programs;

enum class Wide { None, Pair, Quad };

struct Case {
    const char* name;
    bool formatted;
    std::uint32_t components;
    std::uint32_t format;
    std::uint32_t stride;
    std::uint32_t offset;
    std::uint32_t base;
    std::uint32_t records;
    bool modeled;
    Wide wide;
    std::array<std::uint32_t, 4> literals;
    std::uint32_t swizzleStride = 0;
};

constexpr std::array<std::uint32_t, 4> Small{0x20, 0x31, 0x42, 0x53};
constexpr std::array<std::uint32_t, 4> Large{0x1230, 0x2341, 0x3452, 0x4563};
constexpr std::array<std::uint32_t, 4> Floats{0x3f000000u, 0x3e800000u, 0x3f400000u, 0x3f7fff00u};

constexpr std::uint32_t Format16_16UInt = 27;
constexpr std::uint32_t Format8_8_8_8UInt = 60;
constexpr std::uint32_t Format32_32UInt = 62;
constexpr std::uint32_t Format16_16_16_16UNorm = 65;
constexpr std::uint32_t Format16_16_16_16UInt = 69;
constexpr std::uint32_t Format32_32_32_32UInt = 75;

const Case Cases[] = {
    {"format 16_16_16_16_UINT xyzw", true, 4, Format16_16_16_16UInt, 8, 0, 0, 256, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xyzw, offset 2", true, 4, Format16_16_16_16UInt, 8, 2, 0, 272, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xyzw, offset 4", true, 4, Format16_16_16_16UInt, 8, 4, 0, 272, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xyzw, base +4", true, 4, Format16_16_16_16UInt, 8, 0, 4, 256, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xyzw, stride 12", true, 4, Format16_16_16_16UInt, 12, 0, 0, 12 * Lanes, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xyzw, last lane past records", true, 4, Format16_16_16_16UInt, 8, 0, 0, 8 * (Lanes - 1) + 4, true, Wide::Pair, Large},
    {"format 16_16_16_16_UINT xy", true, 2, Format16_16_16_16UInt, 8, 0, 0, 256, true, Wide::Pair, Large},
    {"format 16_16_16_16_UNORM xyzw", true, 4, Format16_16_16_16UNorm, 8, 0, 0, 256, false, Wide::Pair, Floats},
    {"format 32_32_UINT xy", true, 2, Format32_32UInt, 8, 0, 0, 256, true, Wide::Pair, Large},
    {"format 32_32_UINT xy, offset 4", true, 2, Format32_32UInt, 8, 4, 0, 264, true, Wide::Pair, Large},
    {"format 32_32_32_32_UINT xyzw", true, 4, Format32_32_32_32UInt, 16, 0, 0, 512, true, Wide::Quad, Large},
    {"format 32_32_32_32_UINT xyzw, offset 8", true, 4, Format32_32_32_32UInt, 16, 8, 0, 528, true, Wide::Quad, Large},
    {"format 32_32_32_32_UINT xyzw, stride 20", true, 4, Format32_32_32_32UInt, 20, 0, 0, 20 * Lanes, true, Wide::Quad, Large},
    {"format 32_32_32_32_UINT xyzw, last lane past records", true, 4, Format32_32_32_32UInt, 16, 0, 0, 16 * (Lanes - 1) + 8, true, Wide::Quad, Large},
    {"format 32_32_32_32_UINT xyzw, base +8", true, 4, Format32_32_32_32UInt, 16, 0, 8, 512, true, Wide::Quad, Large},
    {"format 8_8_8_8_UINT xyzw", true, 4, Format8_8_8_8UInt, 4, 0, 0, 128, true, Wide::None, Small},
    {"format 8_8_8_8_UINT xyzw, offset 2", true, 4, Format8_8_8_8UInt, 4, 2, 0, 132, true, Wide::None, Small},
    {"format 16_16_UINT xy", true, 2, Format16_16UInt, 4, 0, 0, 128, true, Wide::None, Large},
    {"format 16_16_UINT xy, offset 2", true, 2, Format16_16UInt, 4, 2, 0, 132, true, Wide::None, Large},
    {"dwordx2", false, 2, 0, 8, 0, 0, 256, true, Wide::Pair, Large},
    {"dwordx2, offset 4", false, 2, 0, 8, 4, 0, 264, true, Wide::Pair, Large},
    {"dwordx2, stride 12", false, 2, 0, 12, 0, 0, 12 * Lanes, true, Wide::Pair, Large},
    {"dwordx2, base +4", false, 2, 0, 8, 0, 4, 256, true, Wide::Pair, Large},
    {"dwordx2, base +1", false, 2, 0, 8, 0, 1, 256, false, Wide::Pair, Large},
    {"dwordx2, base +2", false, 2, 0, 8, 0, 2, 256, false, Wide::Pair, Large},
    {"dwordx2, base +3", false, 2, 0, 8, 0, 3, 256, false, Wide::Pair, Large},
    {"dwordx2, last lane half in records", false, 2, 0, 8, 0, 0, 8 * (Lanes - 1) + 4, true, Wide::Pair, Large},
    {"dwordx2, last lane past records", false, 2, 0, 8, 0, 0, 8 * (Lanes - 1), true, Wide::Pair, Large},
    {"dwordx4", false, 4, 0, 16, 0, 0, 512, true, Wide::Quad, Large},
    {"dwordx4, offset 4", false, 4, 0, 16, 4, 0, 528, true, Wide::Quad, Large},
    {"dwordx4, offset 8", false, 4, 0, 16, 8, 0, 528, true, Wide::Quad, Large},
    {"dwordx4, stride 20", false, 4, 0, 20, 0, 0, 20 * Lanes, true, Wide::Quad, Large},
    {"dwordx4, base +4", false, 4, 0, 16, 0, 4, 512, true, Wide::Quad, Large},
    {"dwordx4, base +8", false, 4, 0, 16, 0, 8, 512, true, Wide::Quad, Large},
    {"dwordx4, base +12", false, 4, 0, 16, 0, 12, 512, true, Wide::Quad, Large},
    {"dwordx4, base +1", false, 4, 0, 16, 0, 1, 512, false, Wide::Quad, Large},
    {"dwordx4, base +3", false, 4, 0, 16, 0, 3, 512, false, Wide::Quad, Large},
    {"dwordx4, last lane two dwords in records", false, 4, 0, 16, 0, 0, 16 * (Lanes - 1) + 8, true, Wide::Quad, Large},
    {"dwordx4, last lane three dwords in records", false, 4, 0, 16, 0, 0, 16 * (Lanes - 1) + 12, true, Wide::Quad, Large},
    {"dwordx2, records not a dword multiple", false, 2, 0, 8, 0, 0, 8 * (Lanes - 1) + 6, true, Wide::Pair, Large},
    {"dwordx4, records not a dword multiple", false, 4, 0, 16, 0, 0, 16 * (Lanes - 1) + 13, true, Wide::Quad, Large},
    {"format 16_16_16_16_UINT xyzw, records not a dword multiple", true, 4, Format16_16_16_16UInt, 8, 0, 0, 8 * (Lanes - 1) + 6, true, Wide::Pair, Large},
    {"dwordx3", false, 3, 0, 12, 0, 0, 12 * Lanes, true, Wide::None, Large},
    {"dwordx2, swizzled V#", false, 2, 0, 8, 0, 0, 256, false, Wide::Pair, Large, 16},
    {"dwordx4, swizzled V#", false, 4, 0, 16, 0, 0, 512, false, Wide::Quad, Large, 32},
};

std::uint32_t FormatBits(std::uint32_t format) {
    switch (format) {
        case Format16_16_16_16UInt: return 16;
        case Format32_32UInt: return 32;
        case Format32_32_32_32UInt: return 32;
        case Format8_8_8_8UInt: return 8;
        case Format16_16UInt: return 16;
        default: return 0;
    }
}

std::uint32_t FormatComponents(std::uint32_t format) {
    switch (format) {
        case Format16_16_16_16UInt: return 4;
        case Format32_32UInt: return 2;
        case Format32_32_32_32UInt: return 4;
        case Format8_8_8_8UInt: return 4;
        case Format16_16UInt: return 2;
        default: return 0;
    }
}

std::uint32_t StoreOpcode(const Case& test) {
    if (test.formatted) return 3u + test.components;
    static constexpr std::array<std::uint32_t, 5> dwords{0u, 0x1cu, 0x1du, 0x1fu, 0x1eu};
    return dwords[test.components];
}

std::vector<std::uint32_t> Code(const Case& test) {
    std::vector<std::uint32_t> code;
    code.push_back(0x16000000u | (1u << 17u) | (0x80u + test.stride));
    for (std::uint32_t component = 0; component < 4u; ++component) {
        code.push_back(0x4a0000ffu | ((2u + component) << 17u));
        code.push_back(test.literals[component]);
    }
    code.push_back(0xe0001000u | (StoreOpcode(test) << 18u) | test.offset);
    code.push_back(0x80000201u);
    code.push_back(0xbf810000u);
    return code;
}

std::array<std::uint32_t, 4> Descriptor(const Case& test) {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data() + test.base);
    const std::uint32_t word3 = 0x31000facu | ((test.formatted ? test.format : 22u) << 12u);
    const std::uint32_t swizzle = test.swizzleStride != 0u ? (1u << 31u) | (test.swizzleStride << 16u) : 0u;
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | swizzle, test.records, word3};
}

void Fill() {
    for (std::size_t index = 0; index < Output.size(); ++index) Output[index] = static_cast<std::uint8_t>(index * 7u + 0x5au);
}

std::vector<std::uint8_t> Expected(const Case& test) {
    Fill();
    std::vector<std::uint8_t> bytes(Output.begin(), Output.end());
    const auto write = [&](std::uint32_t at, std::uint32_t size, std::uint32_t value) {
        for (std::uint32_t byte = 0; byte < size; ++byte) bytes[test.base + at + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
    };
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto address = lane * test.stride + test.offset;
        std::array<std::uint32_t, 4> values{};
        for (std::uint32_t component = 0; component < 4u; ++component) values[component] = test.literals[component] + lane;
        if (!test.formatted) {
            for (std::uint32_t component = 0; component < test.components; ++component) {
                if (address + component * 4u + 4u <= test.records) write(address + component * 4u, 4u, values[component]);
            }
            continue;
        }
        const auto bits = FormatBits(test.format);
        const auto count = FormatComponents(test.format);
        const auto size = bits / 8u;
        if (address + count * size > test.records) continue;
        const std::uint32_t mask = bits == 32u ? 0xffffffffu : (1u << bits) - 1u;
        for (std::uint32_t component = 0; component < count; ++component) write(address + component * size, size, component < test.components ? std::min(values[component], mask) : 0u);
    }
    return bytes;
}

std::size_t StoresThrough(std::span<const std::uint32_t> words, std::string_view variable) {
    std::unordered_map<std::uint32_t, std::string> names;
    std::unordered_map<std::uint32_t, std::uint32_t> bases;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> 16u;
        Require(count != 0 && count <= words.size() - offset, "buffer wide stores: malformed SPIR-V instruction");
        const auto opcode = words[offset] & 0xffffu;
        if (opcode == 5u) names[words[offset + 1]] = std::string(reinterpret_cast<const char*>(&words[offset + 2]));
        if (opcode == 65u) bases[words[offset + 2]] = words[offset + 3];
        offset += count;
    }
    std::uint32_t id = 0;
    for (const auto& [value, name] : names) {
        if (name == variable) id = value;
    }
    if (id == 0) return 0;
    std::size_t stores = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> 16u;
        if ((words[offset] & 0xffffu) == 62u) {
            const auto base = bases.find(words[offset + 1]);
            if (base != bases.end() && base->second == id) ++stores;
        }
        offset += count;
    }
    return stores;
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

struct Outcome {
    std::vector<std::uint8_t> bytes;
    std::size_t pairStores = 0;
    std::size_t quadStores = 0;
};

Outcome Run(AgcDriver::VulkanDevice& device, const Case& test, bool wide) {
    Fill();
    auto& program = Programs.emplace_back(Code(test));
    const auto descriptor = Descriptor(test);
    const std::vector<std::uint32_t> userData(descriptor.begin(), descriptor.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.data()), std::as_bytes(std::span(program))}}};
    auto target = device.Target();
    target.wideBufferStores = wide && device.Target().wideBufferStores;
    const auto result = ShaderRecompiler::Recompile(Request(program, userData, target, memory));
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(program.data()));
    device.WaitIdle();
    return {std::vector<std::uint8_t>(Output.begin(), Output.end()), StoresThrough(result.spirv.Words(), "buffers_u64"), StoresThrough(result.spirv.Words(), "buffers_u32x4")};
}

std::string FirstDifference(const std::vector<std::uint8_t>& left, const std::vector<std::uint8_t>& right) {
    for (std::size_t index = 0; index < left.size() && index < right.size(); ++index) {
        if (left[index] != right[index]) {
            char text[96];
            std::snprintf(text, sizeof(text), "byte %zu is 0x%02x, expected 0x%02x", index, left[index], right[index]);
            return text;
        }
    }
    return {};
}

void Check(AgcDriver::VulkanDevice& device, const Case& test, bool deviceWide) {
    const auto label = std::string("buffer wide stores: ") + test.name;
    const auto narrow = Run(device, test, false);
    const auto wide = Run(device, test, true);
    Require(narrow.pairStores == 0 && narrow.quadStores == 0, label + ": the narrow module stores through a wide view");
    if (deviceWide) {
        const bool pair = test.wide == Wide::Pair && TargetHasCapability(device.Target(), spv::CapabilityInt64);
        Require(pair == (wide.pairStores != 0) && (test.wide == Wide::Quad) == (wide.quadStores != 0), label + ": the wide module has " + std::to_string(wide.pairStores) + " u64 and " + std::to_string(wide.quadStores) + " uvec4 stores");
    } else {
        Require(wide.pairStores == 0 && wide.quadStores == 0, label + ": a device without wide stores emitted a wide view");
    }
    const auto difference = FirstDifference(wide.bytes, narrow.bytes);
    Require(difference.empty(), label + ": the wide module's memory differs from the narrow module's: " + difference);
    if (!test.modeled) return;
    const auto expected = Expected(test);
    const auto host = FirstDifference(narrow.bytes, expected);
    Require(host.empty(), label + ": the narrow module's memory differs from the host model: " + host);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool optOut = std::getenv("APS5_NO_WIDE_BUFFER_STORES") != nullptr;
        const bool deviceWide = device->Target().wideBufferStores;
        Require(!optOut || !deviceWide, "buffer wide stores: APS5_NO_WIDE_BUFFER_STORES did not turn the device target's wide stores off");
        for (const auto& test : Cases) Check(*device, test, deviceWide);
        std::printf("buffer wide stores tests passed (%zu cases, device wide stores %s)\n", std::size(Cases), deviceWide ? "on" : "off");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
