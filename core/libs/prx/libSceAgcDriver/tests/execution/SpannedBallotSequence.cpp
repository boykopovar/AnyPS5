#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Stride = 16;
alignas(256) std::array<std::uint32_t, Lanes * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 77> BallotCode{
    0x36020081, 0x7d8a0280, 0x7e14026a, 0x7e16026b, 0xe0702000, 0x80010a00, 0xe0702004, 0x80010b00,
    0x360200a0, 0x7d8a0280, 0x7e14026a, 0x7e16026b, 0xe0702008, 0x80010a00, 0xe070200c, 0x80010b00,
    0x7d8800a8, 0x7e14026a, 0x7e16026b, 0xe0702010, 0x80010a00, 0xe0702014, 0x80010b00, 0x2c040083,
    0x3a040500, 0x36040481, 0x7d8a0480, 0x7e14026a, 0x7e16026b, 0xe0702018, 0x80010a00, 0xe070201c,
    0x80010b00, 0x7e020280, 0x7e180280, 0x7e1a0280, 0xbe880380, 0x34020281, 0x3a020300, 0x3a020208,
    0x36040284, 0x7d8a0480, 0x7e14026a, 0x7e16026b, 0x34181881, 0x3a18190a, 0x341a1a81, 0x3a1a1b0b,
    0x80088108, 0xbf078608, 0xbf85fff2, 0xe0702020, 0x80010c00, 0xe0702024, 0x80010d00, 0x34060081,
    0xbe8b03a5, 0xd7600009, 0x00001703, 0x3a080009, 0x360a08bf, 0x7d880a94, 0x7e14026a, 0x7e16026b,
    0xe0702028, 0x80010a00, 0xe070202c, 0x80010b00, 0x360c0882, 0x7d8a0c80, 0x7e14026a, 0x7e16026b,
    0xe0702030, 0x80010a00, 0xe0702034, 0x80010b00, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | ((Stride * 4u) << 16u), records, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, bool spanned) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), Lanes);
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto address = reinterpret_cast<std::uintptr_t>(BallotCode.data());
    const std::span<const std::uint32_t> code{BallotCode};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address, std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, Lanes * 2u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, address, code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.target.spannedWaves = spanned;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(result.fullSubgroups == spanned, spanned ? "the program did not span two host subgroups" : "the program spans host subgroups without APS5_SPANNED_WAVE");
    device.Dispatch(result, 1, 1, 1, {}, address);
    device.WaitIdle();
}

std::uint64_t Ballot(const std::function<bool(std::uint32_t)>& predicate) {
    std::uint64_t mask = 0;
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        if (predicate(lane)) mask |= std::uint64_t{1} << lane;
    }
    return mask;
}

std::array<std::uint32_t, 14> Expected() {
    std::array<std::uint32_t, 14> words{};
    const auto put = [&](std::uint32_t index, std::uint64_t mask) {
        words[index] = static_cast<std::uint32_t>(mask);
        words[index + 1u] = static_cast<std::uint32_t>(mask >> 32u);
    };
    put(0, Ballot([](std::uint32_t lane) { return (lane & 1u) != 0; }));
    put(2, Ballot([](std::uint32_t lane) { return (lane & 32u) != 0; }));
    put(4, Ballot([](std::uint32_t lane) { return 40u > lane; }));
    put(6, Ballot([](std::uint32_t lane) { return (((lane >> 3u) ^ lane) & 1u) != 0; }));
    std::array<std::uint32_t, Lanes> mix{};
    std::uint32_t low = 0;
    std::uint32_t high = 0;
    for (std::uint32_t iteration = 0; iteration < 6; ++iteration) {
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) mix[lane] = (mix[lane] << 1u) ^ lane ^ iteration;
        const auto mask = Ballot([&](std::uint32_t lane) { return (mix[lane] & 4u) != 0; });
        low = (low << 1u) ^ static_cast<std::uint32_t>(mask);
        high = (high << 1u) ^ static_cast<std::uint32_t>(mask >> 32u);
    }
    words[8] = low;
    words[9] = high;
    const std::uint32_t shuffled = 37u << 1u;
    put(10, Ballot([&](std::uint32_t lane) { return 20u > ((lane ^ shuffled) & 63u); }));
    put(12, Ballot([&](std::uint32_t lane) { return (((lane ^ shuffled) & 2u) != 0); }));
    return words;
}

void Check(bool spanned) {
    const auto expected = Expected();
    const std::array<const char*, 7> names{"odd lanes", "upper half", "lanes below 40", "bit mix", "six-iteration loop", "ballot after shuffle", "second ballot after shuffle"};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        for (std::uint32_t word = 0; word < expected.size(); ++word) {
            const auto actual = Output[lane * Stride + word];
            Require(actual == expected[word], std::string(spanned ? "spanned" : "two-lane") + ": lane " + std::to_string(lane) + " " + names[word / 2u] + (word % 2u == 0 ? " low" : " high") + " word is " + std::to_string(actual) + ", expected " + std::to_string(expected[word]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        const bool spannable = device->Target().subgroupSize == 32u && device->ComputeFullSubgroups();
        if (!spannable) std::puts("spanned mode skipped: the device has no full 32-lane compute subgroups");
        for (const bool spanned : {false, true}) {
            if (spanned && !spannable) continue;
            for (std::uint32_t repeat = 0; repeat < 4; ++repeat) {
                Run(*device, spanned);
                Check(spanned);
            }
        }
        std::printf("spanned ballot sequence tests passed (two-lane%s)\n", spannable ? " and spanned" : "");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
