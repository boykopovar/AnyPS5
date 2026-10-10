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

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 78> Code{
    0x34020085, 0x34060087, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xe030100c, 0x80000701, 0xe0301010, 0x80000801, 0xe0301014, 0x80000901, 0xbf8c3f70, 0xe0701000,
    0x80010403, 0xe0701008, 0x80010403, 0xe0741010, 0x80010403, 0xe0741018, 0x80010403, 0xe0741020,
    0x80010403, 0xe0741028, 0x80010403, 0xe0741030, 0x80010403, 0xe0741038, 0x80010403, 0xe0741040,
    0x80010403, 0xe0741048, 0x80010403, 0xe0741050, 0x80010403, 0xbf8c3f70, 0x7e280306, 0xe0f05000,
    0x80011403, 0x7e2a0306, 0xe0f45008, 0x80011503, 0x7e2c0306, 0x7e2e0307, 0xe1485010, 0x80011603,
    0xe14c1018, 0x80010603, 0xe1541020, 0x80010603, 0xe1581028, 0x80010603, 0xe15c1030, 0x80010603,
    0xe1601038, 0x80010603, 0xe1641040, 0x80010603, 0xe16c1048, 0x80010603, 0x7e300306, 0x7e320307,
    0x7e340308, 0x7e360309, 0xe1445050, 0x80011803, 0xbf8c3f70, 0xe0701058, 0x80011403, 0xe070105c,
    0x80011503, 0xe0741060, 0x80011603, 0xe0741068, 0x80011803, 0xbf810000,
};

std::uint64_t Word64(const std::uint32_t* words) { return words[0] | (static_cast<std::uint64_t>(words[1]) << 32u); }
void Fill(std::uint32_t tid, std::uint32_t* words) {
    const std::uint64_t seed = (tid + 1u) * 0x9e3779b97f4a7c15ull;
    constexpr std::array<std::uint64_t, 4> edges{0u, ~0ull, 0x8000000000000000ull, 0x7fffffffffffffffull};
    const std::uint64_t a = tid < 8u ? edges[tid % 4u] : seed;
    const std::uint64_t b = tid < 8u ? edges[(tid / 4u + tid + 1u) % 4u] : seed * 0xc2b2ae3d27d4eb4full;
    const std::uint64_t c = (tid & 1u) != 0u ? a : seed ^ 1u;
    const std::array<std::uint64_t, 3> values{a, b, c};
    for (std::uint32_t i = 0; i < 3u; ++i) { words[2u * i] = static_cast<std::uint32_t>(values[i]); words[2u * i + 1u] = static_cast<std::uint32_t>(values[i] >> 32u); }
    if (tid % 3u == 0u) words[2] = words[0] % 7u, words[0] %= 9u;
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
    Require(actual == expected, std::string("buffer atomics: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        const std::uint64_t a = Word64(in);
        const std::uint64_t b = Word64(in + 2);
        const std::uint64_t c = Word64(in + 4);
        const std::uint32_t a32 = in[0];
        const std::uint32_t b32 = in[2];
        Expect(tid, out[0], a32 >= b32 ? 0u : a32 + 1u, "buffer_atomic_inc");
        Expect(tid, out[2], (a32 == 0u || a32 > b32) ? b32 : a32 - 1u, "buffer_atomic_dec");
        const auto sa = static_cast<std::int64_t>(a);
        const auto sb = static_cast<std::int64_t>(b);
        const std::array<std::uint64_t, 9> expected{a + b, a - b, static_cast<std::uint64_t>(std::min(sa, sb)), std::min(a, b), static_cast<std::uint64_t>(std::max(sa, sb)), std::max(a, b), a & b, a ^ b, a == c ? b : a};
        constexpr std::array<const char*, 9> names{"add_x2", "sub_x2", "smin_x2", "umin_x2", "smax_x2", "umax_x2", "and_x2", "xor_x2", "cmpswap_x2"};
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            Expect(tid, out[4u + 2u * j], static_cast<std::uint32_t>(expected[j]), names[j]);
            Expect(tid, out[5u + 2u * j], static_cast<std::uint32_t>(expected[j] >> 32u), names[j]);
        }
        Expect(tid, out[22], a32, "inc returned value");
        Expect(tid, out[23], a32, "dec returned value");
        Expect(tid, out[24], in[0], "add_x2 returned low");
        Expect(tid, out[25], in[1], "add_x2 returned high");
        Expect(tid, out[26], in[0], "cmpswap_x2 returned low");
        Expect(tid, out[27], in[1], "cmpswap_x2 returned high");
    }
}

const Testing::Case wrapAnd64BitOps{"BufferAtomics_IncDecAnd64BitOps_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityInt64Atomics, "the device has no shaderBufferInt64Atomics");
    Run(device);
    Check();
}};

} // namespace
