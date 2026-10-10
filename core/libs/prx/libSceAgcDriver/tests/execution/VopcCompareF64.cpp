#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Compares = 41;
constexpr std::uint32_t RandomRounds = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 271> CompareCode{
    0x34020083, 0x34060082, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xe030200c, 0x80000701, 0xe0302010, 0x80000801, 0xe0302014, 0x80000901, 0xe0302018, 0x80000e01,
    0xe030201c, 0x80000f01, 0x7e140280, 0x7e160280, 0xbf8c3f70, 0x7c400d04, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04290114, 0x7c420d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290314, 0x7c440d04,
    0xd5010014, 0x01a90280, 0xd76f000a, 0x04290514, 0x7c460d04, 0xd5010014, 0x01a90280, 0xd76f000a,
    0x04290714, 0x7c480d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290914, 0x7c4a0d04, 0xd5010014,
    0x01a90280, 0xd76f000a, 0x04290b14, 0x7c4c0d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290d14,
    0x7c4e0d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290f14, 0x7c500d04, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04291114, 0x7c520d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291314, 0x7c540d04,
    0xd5010014, 0x01a90280, 0xd76f000a, 0x04291514, 0x7c560d04, 0xd5010014, 0x01a90280, 0xd76f000a,
    0x04291714, 0x7c580d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291914, 0x7c5a0d04, 0xd5010014,
    0x01a90280, 0xd76f000a, 0x04291b14, 0x7c5c0d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291d14,
    0x7c5e0d04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291f14, 0x7d501104, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04292114, 0xbe9e037e, 0x7e280280, 0x7c600d04, 0x7e280281, 0xbefe031e, 0xd76f000a,
    0x04292314, 0xbe9e037e, 0x7e280280, 0x7c620d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292514,
    0xbe9e037e, 0x7e280280, 0x7c640d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292714, 0xbe9e037e,
    0x7e280280, 0x7c660d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292914, 0xbe9e037e, 0x7e280280,
    0x7c680d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292b14, 0xbe9e037e, 0x7e280280, 0x7c6a0d04,
    0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292d14, 0xbe9e037e, 0x7e280280, 0x7c6c0d04, 0x7e280281,
    0xbefe031e, 0xd76f000a, 0x04292f14, 0xbe9e037e, 0x7e280280, 0x7c6e0d04, 0x7e280281, 0xbefe031e,
    0xd76f000a, 0x04293114, 0xbe9e037e, 0x7e280280, 0x7c700d04, 0x7e280281, 0xbefe031e, 0xd76f000a,
    0x04293314, 0xbe9e037e, 0x7e280280, 0x7c720d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293514,
    0xbe9e037e, 0x7e280280, 0x7c740d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293714, 0xbe9e037e,
    0x7e280280, 0x7c760d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293914, 0xbe9e037e, 0x7e280280,
    0x7c780d04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293b14, 0xbe9e037e, 0x7e280280, 0x7c7a0d04,
    0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293d14, 0xbe9e037e, 0x7e280280, 0x7c7c0d04, 0x7e280281,
    0xbefe031e, 0xd76f000a, 0x04293f14, 0xbe9e037e, 0x7e280280, 0x7c7e0d04, 0x7e280281, 0xbefe031e,
    0xd76f000b, 0x042d0114, 0xbe9e037e, 0x7e280280, 0x7d701104, 0x7e280281, 0xbefe031e, 0xd76f000b,
    0x042d0314, 0x7c4208ff, 0x40080000, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0514, 0x7c4c08f2,
    0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0714, 0x7c4408f8, 0xd5010014, 0x01a90280, 0xd76f000b,
    0x042d0914, 0x7c420881, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0b14, 0xd421016a, 0x00020d04,
    0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0d14, 0xd4a8016a, 0x00021104, 0xd5010014, 0x01a90280,
    0xd76f000b, 0x042d0f14, 0xbe9e037e, 0x7e280280, 0xd43b037e, 0x00020d04, 0x7e280281, 0xbefe031e,
    0xd76f000b, 0x042d1114, 0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xbf810000,
};

constexpr std::array<std::uint64_t, 20> Edges{
    0x7ff8000000000000ull, 0x7ff0000000000001ull, 0xfff8000000000001ull, 0x7ff0000100000000ull, 0x7ff0000000000000ull,
    0xfff0000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000001ull, 0x0000000000000002ull,
    0x800fffffffffffffull, 0x0010000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000001ull, 0xbff0000000000000ull,
    0x4008000000000000ull, 0xc008000000000000ull, 0x3fc45f306dc9c882ull, 0x3fc45f3060000000ull, 0x7fefffffffffffffull,
};

constexpr std::array<std::uint32_t, 16> Masks{
    0x001u, 0x002u, 0x004u, 0x008u, 0x010u, 0x020u, 0x040u, 0x080u, 0x100u, 0x200u, 0x3ffu, 0x000u, 0x155u, 0x2aau, 0xfffffc00u, 0x207u,
};

constexpr std::uint32_t EdgeRounds = (Edges.size() * Edges.size() + Threads - 1u) / Threads;

void FillInput(std::uint32_t round) {
    std::uint64_t state = 0x853c49e6748fea9bull + round;
    const auto next = [&] {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        const auto high = static_cast<std::uint32_t>(state >> 32u);
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return (static_cast<std::uint64_t>(high) << 32u) | static_cast<std::uint32_t>(state >> 32u);
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::uint64_t a = 0u;
        std::uint64_t b = 0u;
        std::uint32_t mask = 0u;
        if (round < EdgeRounds) {
            const std::uint32_t pair = round * Threads + tid;
            const std::uint32_t first = pair % Edges.size();
            const std::uint32_t second = (pair / Edges.size()) % Edges.size();
            a = Edges[first];
            b = Edges[second];
            mask = Masks[(first + second) % Masks.size()];
        } else {
            a = next();
            b = tid % 5u == 0u ? a : (tid % 7u == 0u ? a ^ 0x8000000000000000ull : next());
            mask = static_cast<std::uint32_t>(next());
        }
        auto* words = &Input[tid * Inputs];
        words[0] = static_cast<std::uint32_t>(a);
        words[1] = static_cast<std::uint32_t>(a >> 32u);
        words[2] = static_cast<std::uint32_t>(b);
        words[3] = static_cast<std::uint32_t>(b >> 32u);
        words[4] = mask;
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(CompareCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

std::uint32_t ClassBit(std::uint64_t bits) {
    const bool negative = (bits >> 63u) != 0u;
    switch (std::fpclassify(std::bit_cast<double>(bits))) {
        case FP_NAN: return (bits & 0x0008000000000000ull) != 0u ? 1u : 0u;
        case FP_INFINITE: return negative ? 2u : 9u;
        case FP_NORMAL: return negative ? 3u : 8u;
        case FP_SUBNORMAL: return negative ? 4u : 7u;
        default: return negative ? 5u : 6u;
    }
}

bool ClassMatch(std::uint64_t bits, std::uint32_t mask) {
    return ((mask >> ClassBit(bits)) & 1u) != 0u;
}

void Check(std::uint32_t round) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint64_t abits = in[0] | (static_cast<std::uint64_t>(in[1]) << 32u);
        const std::uint64_t bbits = in[2] | (static_cast<std::uint64_t>(in[3]) << 32u);
        const std::uint32_t mask = in[4];
        const double a = std::bit_cast<double>(abits);
        const double b = std::bit_cast<double>(bbits);
        const bool unordered = std::isnan(a) || std::isnan(b);
        const std::array<bool, 17> base{
            false,
            a < b,
            a == b,
            a <= b,
            a > b,
            a < b || a > b,
            a >= b,
            !unordered,
            unordered,
            !(a >= b),
            !(a < b || a > b),
            !(a > b),
            !(a <= b),
            !(a == b),
            !(a < b),
            true,
            ClassMatch(abits, mask),
        };
        std::array<bool, Compares> expected{};
        for (std::uint32_t k = 0; k < base.size(); ++k) {
            expected[k] = base[k];
            expected[base.size() + k] = base[k];
        }
        expected[34] = 3.0 < a;
        expected[35] = 1.0 >= a;
        expected[36] = std::bit_cast<double>(0x3fc45f306dc9c882ull) == a;
        expected[37] = std::numeric_limits<double>::denorm_min() < a;
        expected[38] = std::fabs(a) < b;
        expected[39] = ClassMatch(abits & 0x7fffffffffffffffull, mask);
        expected[40] = !(std::fabs(a) > std::fabs(b));
        for (std::uint32_t k = 0; k < expected.size(); ++k) {
            const bool actual = ((Output[tid * Results + k / 32u] >> (k % 32u)) & 1u) != 0u;
            Testing::Require(actual == expected[k], "vopc f64 compares: round " + std::to_string(round) + " thread " + std::to_string(tid) + " compare " + std::to_string(k) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[k]));
        }
    }
}

const Testing::Case edgePairs{"VopcCompareF64_EveryEdgePair_MatchesReference", [] {
    auto& device = SharedVulkanTestDevice();
    for (std::uint32_t round = 0; round < EdgeRounds; ++round) {
        FillInput(round);
        Run(device);
        Check(round);
    }
}};

const Testing::Case randomInputs{"VopcCompareF64_RandomInputs_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    for (std::uint32_t round = EdgeRounds; round < EdgeRounds + RandomRounds; ++round) {
        FillInput(round);
        Run(device);
        Check(round);
    }
}};

} // namespace
