#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
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
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32x4UInt = 75;
constexpr std::uint32_t Format2_10_10_10UInt = 48;
constexpr std::uint32_t Format8x4SInt = 61;
constexpr std::uint32_t Format16x4SInt = 70;
constexpr std::uint32_t Format16x4Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Prefill = 0xababababu;
constexpr std::uint32_t Zero = 0u;
alignas(256) std::array<std::uint32_t, Threads * (Inputs + Results)> Buffer{};
alignas(4096) std::array<std::uint32_t, 4096> Texels{};

constexpr std::array<std::uint32_t, 19> Prologue{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xbe93030f, 0xbe92030e, 0xbe91030d,
    0xbe90030c, 0xbe8f030b, 0xbe8e030a, 0xbe8d0309, 0xbe8c0308, 0xbe8b0307, 0xbe8a0306, 0xbe890305,
    0xbe880304, 0x7e100300, 0x7e120280,
};

enum class Data { Marker, Negative, Half, Small };

struct Case {
    const char* name;
    std::uint32_t format;
    std::uint32_t channels;
    Data data;
    std::uint32_t prefill;
};

std::vector<std::uint32_t> Program(std::uint32_t channels) {
    const std::uint32_t dmask = channels == 1u ? 0x100u : 0xf00u;
    std::vector<std::uint32_t> code(Prologue.begin(), Prologue.end());
    code.insert(code.end(), {0xf0200088u | dmask, 0x00020408u, 0xbbfd0000u});
    code.insert(code.end(), {0xf0000088u | dmask, 0x00020a08u, 0x7e120281u});
    code.insert(code.end(), {0xf0000088u | dmask, 0x00020008u | ((10u + channels) << 8u), 0xbf8c3f70u});
    for (std::uint32_t j = 0; j < channels * 2u; ++j) {
        code.insert(code.end(), {0xe0701200u + j * 4u, 0x80000003u | ((10u + j) << 8u)});
    }
    code.push_back(0xbf810000u);
    return code;
}

std::uint32_t Stored(Data data, std::uint32_t tid, std::uint32_t channel) {
    switch (data) {
        case Data::Negative: return static_cast<std::uint32_t>(-static_cast<std::int32_t>(tid * 4u + channel + 1u));
        case Data::Half: return std::bit_cast<std::uint32_t>((static_cast<float>(tid) - 15.5f) * 0.25f * static_cast<float>(channel + 1u));
        case Data::Small: return 1u + (tid + channel) % 3u;
        default: return 0x5a000000u | (tid << 8u) | (channel << 4u) | 0x3u;
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, const Case& test) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t channel = 0; channel < Inputs; ++channel) Buffer[tid * Inputs + channel] = Stored(test.data, tid, channel);
    }
    Texels.fill(test.prefill);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data(), test.format);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const auto code = Program(test.channels);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
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
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t channel = 0; channel < test.channels; ++channel) {
            const auto loaded = Buffer[Threads * Inputs + tid * Results + channel];
            const auto untouched = Buffer[Threads * Inputs + tid * Results + test.channels + channel];
            char message[200];
            std::snprintf(message, sizeof(message), "%s: thread %u channel %u loaded 0x%08x after storing 0x%08x", test.name, tid, channel, loaded, Stored(test.data, tid, channel));
            Require(loaded == Stored(test.data, tid, channel), message);
            std::snprintf(message, sizeof(message), "%s: thread %u channel %u read 0x%08x from a texel the dispatch did not store, expected 0x%08x", test.name, tid, channel, untouched, test.prefill);
            Require(untouched == test.prefill, message);
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        constexpr std::array<Case, 6> cases{{
            {"32_UINT", Format32UInt, 1u, Data::Marker, Prefill},
            {"32_32_32_32_UINT", Format32x4UInt, 4u, Data::Marker, Prefill},
            {"8_8_8_8_SINT", Format8x4SInt, 4u, Data::Negative, Zero},
            {"16_16_16_16_SINT", Format16x4SInt, 4u, Data::Negative, Zero},
            {"16_16_16_16_FLOAT", Format16x4Float, 4u, Data::Half, Zero},
            {"2_10_10_10_UINT", Format2_10_10_10UInt, 4u, Data::Small, Zero},
        }};
        for (const auto& test : cases) Run(*device, test);
        std::puts("image load after store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
