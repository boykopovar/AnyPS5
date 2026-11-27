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
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t Words = 16;
constexpr std::uint32_t Masks = 11;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 157> Wave32Code{
    0x34020082, 0x34060086, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xe030200c, 0x80000701, 0xbf8c3f70, 0x7d080b04, 0xbe96036a, 0xbe943c16, 0xbeea03c1, 0xd70f6a0a,
    0x02020f06, 0xbe98036a, 0xbefe0314, 0xd501001e, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0xd7106a0a,
    0x02020f06, 0xbe9a036a, 0xbefe0314, 0xd501001f, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0xd7196a0a,
    0x02020f06, 0xbe9c036a, 0xbefe0314, 0xd5010020, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0x50140f06,
    0xbe9e036a, 0xbefe0314, 0xd5010021, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0x52140f06, 0xbea0036a,
    0xbefe0314, 0xd5010022, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0x54140f06, 0xbea2036a, 0xbefe0314,
    0xd5010023, 0x01a90280, 0xbe943c16, 0xbe8e03c1, 0xd70f0e0a, 0x02020f06, 0xbea4030e, 0xbefe0314,
    0xd5010024, 0x00390280, 0xbe943c16, 0xbeea03c1, 0xd5766a0a, 0x04120f06, 0xbea6036a, 0xbefe0314,
    0xd5010025, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0xd5776a0a, 0x04120f06, 0xbea8036a, 0xbefe0314,
    0xd5010026, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0xd56d6a0a, 0x041a0f06, 0xbeaa036a, 0xbefe0314,
    0xd5010027, 0x01a90280, 0xbe943c16, 0xbeea03c1, 0xd56e6a0a, 0x041a0906, 0xbeac036a, 0xbefe0314,
    0xd5010028, 0x01a90280, 0xe0702000, 0x80011e03, 0xe0702004, 0x80011f03, 0xe0702008, 0x80012003,
    0xe070200c, 0x80012103, 0xe0702010, 0x80012203, 0xe0702014, 0x80012303, 0xe0702018, 0x80012403,
    0xe070201c, 0x80012503, 0xe0702020, 0x80012603, 0xe0702024, 0x80012703, 0xe0702028, 0x80012803,
    0x7e140216, 0xe0702040, 0x80010a03, 0x7e140218, 0xe0702044, 0x80010a03, 0x7e14021a, 0xe0702048,
    0x80010a03, 0x7e14021c, 0xe070204c, 0x80010a03, 0x7e14021e, 0xe0702050, 0x80010a03, 0x7e140220,
    0xe0702054, 0x80010a03, 0x7e140222, 0xe0702058, 0x80010a03, 0x7e140224, 0xe070205c, 0x80010a03,
    0x7e140226, 0xe0702060, 0x80010a03, 0x7e140228, 0xe0702064, 0x80010a03, 0x7e14022a, 0xe0702068,
    0x80010a03, 0x7e14022c, 0xe070206c, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 193> Wave64Code{
    0x34020082, 0x34060086, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xe030200c, 0x80000701, 0xbf8c3f70, 0x7d080b04, 0xbe96046a, 0xbe942416, 0xbeea04c1, 0xd70f6a0a,
    0x02020f06, 0xbe98046a, 0xbefe0414, 0xd501001e, 0x01a90280, 0xbe942416, 0xbeea04c1, 0xd7106a0a,
    0x02020f06, 0xbe9a046a, 0xbefe0414, 0xd501001f, 0x01a90280, 0xbe942416, 0xbeea04c1, 0xd7196a0a,
    0x02020f06, 0xbe9c046a, 0xbefe0414, 0xd5010020, 0x01a90280, 0xbe942416, 0xbeea04c1, 0x50140f06,
    0xbe9e046a, 0xbefe0414, 0xd5010021, 0x01a90280, 0xbe942416, 0xbeea04c1, 0x52140f06, 0xbea0046a,
    0xbefe0414, 0xd5010022, 0x01a90280, 0xbe942416, 0xbeea04c1, 0x54140f06, 0xbea2046a, 0xbefe0414,
    0xd5010023, 0x01a90280, 0xbe942416, 0xbe8e04c1, 0xd70f0e0a, 0x02020f06, 0xbea4040e, 0xbefe0414,
    0xd5010024, 0x00390280, 0xbe942416, 0xbeea04c1, 0xd5766a0a, 0x04120f06, 0xbea6046a, 0xbefe0414,
    0xd5010025, 0x01a90280, 0xbe942416, 0xbeea04c1, 0xd5776a0a, 0x04120f06, 0xbea8046a, 0xbefe0414,
    0xd5010026, 0x01a90280, 0xbe942416, 0xbeea04c1, 0xd56d6a0a, 0x041a0f06, 0xbeaa046a, 0xbefe0414,
    0xd5010027, 0x01a90280, 0xbe942416, 0xbeea04c1, 0xd56e6a0a, 0x041a0906, 0xbeac046a, 0xbefe0414,
    0xd5010028, 0x01a90280, 0xe0702000, 0x80011e03, 0xe0702004, 0x80011f03, 0xe0702008, 0x80012003,
    0xe070200c, 0x80012103, 0xe0702010, 0x80012203, 0xe0702014, 0x80012303, 0xe0702018, 0x80012403,
    0xe070201c, 0x80012503, 0xe0702020, 0x80012603, 0xe0702024, 0x80012703, 0xe0702028, 0x80012803,
    0x7e140216, 0xe0702040, 0x80010a03, 0x7e140217, 0xe0702044, 0x80010a03, 0x7e140218, 0xe0702048,
    0x80010a03, 0x7e140219, 0xe070204c, 0x80010a03, 0x7e14021a, 0xe0702050, 0x80010a03, 0x7e14021b,
    0xe0702054, 0x80010a03, 0x7e14021c, 0xe0702058, 0x80010a03, 0x7e14021d, 0xe070205c, 0x80010a03,
    0x7e14021e, 0xe0702060, 0x80010a03, 0x7e14021f, 0xe0702064, 0x80010a03, 0x7e140220, 0xe0702068,
    0x80010a03, 0x7e140221, 0xe070206c, 0x80010a03, 0x7e140222, 0xe0702070, 0x80010a03, 0x7e140223,
    0xe0702074, 0x80010a03, 0x7e140224, 0xe0702078, 0x80010a03, 0x7e140225, 0xe070207c, 0x80010a03,
    0x7e140226, 0xe0702080, 0x80010a03, 0x7e140227, 0xe0702084, 0x80010a03, 0x7e140228, 0xe0702088,
    0x80010a03, 0x7e140229, 0xe070208c, 0x80010a03, 0x7e14022a, 0xe0702090, 0x80010a03, 0x7e14022b,
    0xe0702094, 0x80010a03, 0x7e14022c, 0xe0702098, 0x80010a03, 0x7e14022d, 0xe070209c, 0x80010a03,
    0xbf810000,
};

constexpr std::array<const char*, Masks> Names{
    "v_add_co_u32", "v_sub_co_u32", "v_subrev_co_u32", "v_add_co_ci_u32", "v_sub_co_ci_u32", "v_subrev_co_ci_u32", "v_add_co_u32 into an sgpr", "v_mad_u64_u32", "v_mad_i64_i32", "v_div_scale_f32", "v_div_scale_f64",
};

constexpr std::array<std::array<std::uint32_t, Masks + 1u>, 2> Measured{{
    {0x10bb69edu, 0x00ba6165u, 0x00936061u, 0x1028098cu, 0x00ba6165u, 0x00936061u, 0x1028098cu, 0x00ba6165u, 0x00906041u, 0x10b368a9u, 0x101208c0u, 0x001b4820u},
    {0xc7dc5fecu, 0x800c4e08u, 0x42904280u, 0x854c1d6cu, 0x800c4e08u, 0x42904280u, 0x854c1d6cu, 0x800c4e08u, 0x40080808u, 0x47584c8cu, 0xc74c08ecu, 0x41405008u},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t& word : Input) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        word = static_cast<std::uint32_t>(state >> 32u);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = waveSize == 64u ? std::span<const std::uint32_t>(Wave64Code) : std::span<const std::uint32_t>(Wave32Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t waveSize, bool words) {
    const std::uint32_t halves = waveSize / 32u;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::string thread = "wave" + std::to_string(waveSize) + " thread " + std::to_string(tid);
        for (std::uint32_t j = 0; j < Masks; ++j) {
            const std::uint32_t bit = Output[tid * Results + j];
            const std::uint32_t expected = (Measured[tid / 32u][j + 1u] >> (tid % 32u)) & 1u;
            Require(bit == expected, thread + ": the mask of " + Names[j] + " under EXEC " + Hex(Measured[tid / 32u][0]) + " reads " + std::to_string(bit) + " in this lane once EXEC is restored, expected " + std::to_string(expected) +
                ": RDNA2 (gfx1036) clears the mask bits of the lanes EXEC leaves out");
        }
        for (std::uint32_t half = 0; words && half < halves; ++half) {
            const auto& measured = Measured[halves == 2u ? half : tid / 32u];
            const std::string lanes = thread + " lanes " + std::to_string(half * 32u) + " to " + std::to_string(half * 32u + 31u);
            const std::uint32_t exec = Output[tid * Results + Words + half];
            Require(exec == measured[0], lanes + ": EXEC is " + Hex(exec) + ", expected " + Hex(measured[0]));
            for (std::uint32_t j = 0; j < Masks; ++j) {
                const std::uint32_t mask = Output[tid * Results + Words + (j + 1u) * halves + half];
                Require(mask == measured[j + 1u], lanes + ": the mask of " + Names[j] + " under EXEC " + Hex(exec) + " is " + Hex(mask) + ", expected " + Hex(measured[j + 1u]) +
                    ": RDNA2 (gfx1036) clears the mask bits of the lanes EXEC leaves out");
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool words = device->Target().subgroupSize >= 32u;
        if (!words) std::printf("whole mask words not checked, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
        FillInput();
        for (const std::uint32_t waveSize : {32u, 64u}) {
            Run(*device, waveSize);
            Check(waveSize, words);
        }
        std::puts("vector mask result exec tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
