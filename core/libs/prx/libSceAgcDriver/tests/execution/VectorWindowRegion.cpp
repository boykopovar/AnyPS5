#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Waves = 3;
constexpr std::uint32_t MaxThreads = Waves * 64;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Stored = 13;
constexpr std::array<std::uint32_t, Stored> StoredRegisters{1, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16};
constexpr std::uint32_t MarkerV1 = 0x5a5a0000u;
constexpr std::uint32_t MarkerV5 = 0x5a5b0000u;
constexpr std::uint32_t MarkerV6 = 0x5a5c0000u;
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};
alignas(256) std::array<std::uint32_t, 96> Code{};

struct Variant {
    const char* name;
    bool entryRelative;
    bool setupRelative;
    bool afterLoop;
    bool entryInput;
};

constexpr std::array<Variant, 4> Variants{{
    {"loop", false, false, true, false},
    {"setup and loop left at a join", false, true, false, false},
    {"entry block and loop", true, false, true, false},
    {"entry block reads an input register", true, false, true, true},
}};

constexpr std::uint32_t Branch(std::uint32_t opcode, std::size_t from, std::size_t to) {
    return opcode | (static_cast<std::uint32_t>(static_cast<std::int32_t>(to) - static_cast<std::int32_t>(from) - 1) & 0xffffu);
}

std::uint32_t Threshold(std::uint32_t waveSize) {
    return waveSize + waveSize / 4u;
}

std::span<const std::uint32_t> Assemble(const Variant& variant, std::uint32_t waveSize) {
    std::size_t at = 0;
    const auto put = [&](std::initializer_list<std::uint32_t> words) {
        for (const auto word : words) Code.at(at++) = word;
    };
    put({0x34060086u, 0x34040084u});
    for (std::uint32_t slot = 0; slot < 8u; ++slot) put({0x4a000000u | ((8u + slot) << 17u) | (2u << 9u) | (0x80u + slot)});
    put({0x7e0202ffu, MarkerV1, 0x7e0a02ffu, MarkerV5, 0x7e0c02ffu, MarkerV6});
    if (variant.entryRelative) put({0xbefc0382u, 0x7e088708u});
    else put({0x4a080482u});
    if (variant.entryInput) put({0xbefc0380u, 0x7e208700u});
    put({waveSize == 64u ? 0xbe94047eu : 0xbe94037eu, 0x7da800ffu, Threshold(waveSize)});
    const auto skip = at;
    put({0u, 0xbefc0381u, 0xbe900383u, variant.setupRelative ? 0x7e0c8708u : 0x4a0c0481u});
    const auto loop = at;
    put({0x7e028708u, 0x7e128501u, 0x4a1c1c90u, 0x7e0a870du, 0x807c817cu, 0x80908110u, 0xbf078010u});
    put({Branch(0xbf850000u, at, loop)});
    if (variant.afterLoop) put({0x4a1a1aa0u});
    Code[skip] = Branch(0xbf880000u, skip, at);
    put({waveSize == 64u ? 0xbefe0414u : 0xbefe0314u});
    for (std::uint32_t index = 0; index < Stored; ++index) put({0xe0701000u | (index * 4u), 0x80010003u | (StoredRegisters[index] << 8u)});
    put({0xbf810000u});
    return {Code.data(), at};
}

std::array<std::uint32_t, Stored> ExpectedFor(const Variant& variant, std::uint32_t waveSize, std::uint32_t tid) {
    const std::uint32_t base = tid * 16u;
    if (tid >= Threshold(waveSize)) return {MarkerV1, base + 2u, MarkerV5, MarkerV6, base, base + 1u, base + 2u, base + 3u, base + 4u, base + 5u, base + 6u, base + 7u, variant.entryInput ? tid : 0u};
    return {base + 1u, base + 2u, variant.entryInput ? tid : 0u, base + 1u, base, base + 1u, base + 1u, base + 1u, base + 1u, base + (variant.afterLoop ? 37u : 5u), base + 54u, base + 7u, variant.entryInput ? tid : 0u};
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

void Run(AgcDriver::VulkanDevice& device, const Variant& variant, std::uint32_t waveSize) {
    const std::uint32_t threads = Waves * waveSize;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto code = Assemble(variant, waveSize);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        const auto expected = ExpectedFor(variant, waveSize, tid);
        for (std::uint32_t index = 0; index < Stored; ++index) {
            Require(out[index] == expected[index], std::string("vector window region: ") + variant.name + ", wave" + std::to_string(waveSize) + " lane " + std::to_string(tid) + " v" + std::to_string(StoredRegisters[index]) + " is " + Hex(out[index]) + ", expected " + Hex(expected[index]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const auto& variant : Variants) {
            for (const auto waveSize : {32u, 64u}) Run(*device, variant, waveSize);
        }
        std::puts("vector window region tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
