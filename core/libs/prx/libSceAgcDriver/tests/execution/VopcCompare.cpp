#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 364> CompareCode{
    0x34020083, 0x34060082, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xe030200c, 0x80000701, 0xe0302010, 0x80000801, 0xe0302014, 0x80000901, 0xe0302018, 0x80000e01,
    0xe030201c, 0x80000f01, 0x7e140280, 0x7e160280, 0xbf8c3f70, 0xbe9e037e, 0x7e280280, 0x7c200b04,
    0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290114, 0xbe9e037e, 0x7e280280, 0x7c2e0b04, 0x7e280281,
    0xbefe031e, 0xd76f000a, 0x04290314, 0xbe9e037e, 0x7e280280, 0x7c300b04, 0x7e280281, 0xbefe031e,
    0xd76f000a, 0x04290514, 0xbe9e037e, 0x7e280280, 0x7c3e0b04, 0x7e280281, 0xbefe031e, 0xd76f000a,
    0x04290714, 0xbe9e037e, 0x7e280280, 0x7d200b04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290914,
    0xbe9e037e, 0x7e280280, 0x7d2e0b04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290b14, 0xbe9e037e,
    0x7e280280, 0x7da00b04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290d14, 0xbe9e037e, 0x7e280280,
    0x7dae0b04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290f14, 0x7d401106, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04291114, 0x7d421106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291314, 0x7d461106,
    0xd5010014, 0x01a90280, 0xd76f000a, 0x04291514, 0x7d481106, 0xd5010014, 0x01a90280, 0xd76f000a,
    0x04291714, 0x7d4a1106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291914, 0x7d4c1106, 0xd5010014,
    0x01a90280, 0xd76f000a, 0x04291b14, 0x7d4e1106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04291d14,
    0xbe9e037e, 0x7e280280, 0x7d601106, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04291f14, 0xbe9e037e,
    0x7e280280, 0x7d621106, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292114, 0xbe9e037e, 0x7e280280,
    0x7d641106, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292314, 0xbe9e037e, 0x7e280280, 0x7d661106,
    0x7e280281, 0xbefe031e, 0xd76f000a, 0x04292514, 0xbe9e037e, 0x7e280280, 0x7d681106, 0x7e280281,
    0xbefe031e, 0xd76f000a, 0x04292714, 0xbe9e037e, 0x7e280280, 0x7d6c1106, 0x7e280281, 0xbefe031e,
    0xd76f000a, 0x04292914, 0xbe9e037e, 0x7e280280, 0x7d6e1106, 0x7e280281, 0xbefe031e, 0xd76f000a,
    0x04292b14, 0x7dc01106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04292d14, 0x7dc61106, 0xd5010014,
    0x01a90280, 0xd76f000a, 0x04292f14, 0x7dcc1106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04293114,
    0x7dce1106, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04293314, 0xbe9e037e, 0x7e280280, 0x7de01106,
    0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293514, 0xbe9e037e, 0x7e280280, 0x7de21106, 0x7e280281,
    0xbefe031e, 0xd76f000a, 0x04293714, 0xbe9e037e, 0x7e280280, 0x7de41106, 0x7e280281, 0xbefe031e,
    0xd76f000a, 0x04293914, 0xbe9e037e, 0x7e280280, 0x7de61106, 0x7e280281, 0xbefe031e, 0xd76f000a,
    0x04293b14, 0xbe9e037e, 0x7e280280, 0x7de81106, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293d14,
    0xbe9e037e, 0x7e280280, 0x7dec1106, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04293f14, 0xbe9e037e,
    0x7e280280, 0x7dee1106, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d0114, 0x7d901f0e, 0xd5010014,
    0x01a90280, 0xd76f000b, 0x042d0314, 0x7d9e1f0e, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0514,
    0x7dd01f0e, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0714, 0x7dd21f0e, 0xd5010014, 0x01a90280,
    0xd76f000b, 0x042d0914, 0x7dd41f0e, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0b14, 0x7dd61f0e,
    0xd5010014, 0x01a90280, 0xd76f000b, 0x042d0d14, 0x7dd81f0e, 0xd5010014, 0x01a90280, 0xd76f000b,
    0x042d0f14, 0x7ddc1f0e, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d1114, 0x7dde1f0e, 0xd5010014,
    0x01a90280, 0xd76f000b, 0x042d1314, 0xbe9e037e, 0x7e280280, 0x7db01f0e, 0x7e280281, 0xbefe031e,
    0xd76f000b, 0x042d1514, 0xbe9e037e, 0x7e280280, 0x7dba1f0e, 0x7e280281, 0xbefe031e, 0xd76f000b,
    0x042d1714, 0xbe9e037e, 0x7e280280, 0x7dbe1f0e, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d1914,
    0xbe9e037e, 0x7e280280, 0x7df01f0e, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d1b14, 0xbe9e037e,
    0x7e280280, 0x7df21f0e, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d1d14, 0xbe9e037e, 0x7e280280,
    0x7df41f0e, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d1f14, 0xbe9e037e, 0x7e280280, 0x7df81f0e,
    0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d2114, 0xbe9e037e, 0x7e280280, 0x7dfe1f0e, 0x7e280281,
    0xbefe031e, 0xd76f000b, 0x042d2314, 0xd4e9026a, 0x00021f0e, 0xd5010014, 0x01a90280, 0xd76f000b,
    0x042d2514, 0xd4a3006a, 0x00020d08, 0xd5010014, 0x01a90280, 0xd76f000b, 0x042d2714, 0xbe9e037e,
    0x7e280280, 0xd4f6007e, 0x00020d08, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d2914, 0xbe9e037e,
    0x7e280280, 0xd417017e, 0x00020b04, 0x7e280281, 0xbefe031e, 0xd76f000b, 0x042d2b14, 0xe0702000,
    0x80010a03, 0xe0702004, 0x80010b03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 12> FloatEdges{
    0x7fc00000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x7f800000u, 0xff800000u,
    0xbf800000u, 0x3f800000u, 0x00000001u, 0x40490fdbu, 0xffc00001u, 0xc0000000u,
};

constexpr std::array<std::uint16_t, 12> HalfEdges{
    0x7e00u, 0x3c00u, 0x8000u, 0x0000u, 0x7c00u, 0xfc00u, 0xbc00u, 0x3c00u, 0x0001u, 0x4248u, 0xfe01u, 0xc000u,
};

constexpr std::array<std::uint64_t, 8> IntegerEdges{
    0u, ~0ull, 0x8000000000000000ull, 0x7fffffffffffffffull, 1u, 0x00000001ffffffffull, 0x0000000100000000ull, 0xffffffff00000000ull,
};

float HalfValue(std::uint32_t bits) {
    const auto exponent = (bits >> 10u) & 0x1fu;
    const auto fraction = bits & 0x3ffu;
    float magnitude = 0.0f;
    if (exponent == 0x1fu) {
        magnitude = fraction != 0u ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
    } else {
        magnitude = exponent == 0u ? std::ldexp(static_cast<float>(fraction), -24) : std::ldexp(static_cast<float>(fraction | 0x400u), static_cast<int>(exponent) - 25);
    }
    return (bits & 0x8000u) != 0u ? -magnitude : magnitude;
}

void FillInput() {
    std::uint64_t state = 0x853c49e6748fea9bull;
    const auto next = [&] {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(state >> 33u);
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        words[0] = FloatEdges[tid % FloatEdges.size()];
        words[1] = FloatEdges[(tid / FloatEdges.size() + tid) % FloatEdges.size()];
        const std::uint64_t a = tid < 16u ? IntegerEdges[tid % IntegerEdges.size()] : (static_cast<std::uint64_t>(next()) << 32u) | next();
        const std::uint64_t b = tid < 16u ? IntegerEdges[(tid / IntegerEdges.size() + tid * 3u) % IntegerEdges.size()] : (tid % 5u == 0u ? a : (static_cast<std::uint64_t>(next()) << 32u) | next());
        words[2] = static_cast<std::uint32_t>(a);
        words[3] = static_cast<std::uint32_t>(a >> 32u);
        words[4] = static_cast<std::uint32_t>(b);
        words[5] = static_cast<std::uint32_t>(b >> 32u);
        words[6] = 0xabcd0000u | HalfEdges[tid % HalfEdges.size()];
        words[7] = 0x12340000u | HalfEdges[(tid / HalfEdges.size() + tid * 5u) % HalfEdges.size()];
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

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const float fa = std::bit_cast<float>(in[0]);
        const float fb = std::bit_cast<float>(in[1]);
        const std::uint64_t ua = in[2] | (static_cast<std::uint64_t>(in[3]) << 32u);
        const std::uint64_t ub = in[4] | (static_cast<std::uint64_t>(in[5]) << 32u);
        const auto sa = static_cast<std::int64_t>(ua);
        const auto sb = static_cast<std::int64_t>(ub);
        const float ha = HalfValue(in[6] & 0xffffu);
        const float hb = HalfValue(in[7] & 0xffffu);
        const std::array<bool, 54> expected{
            false,
            !std::isnan(fa) && !std::isnan(fb),
            std::isnan(fa) || std::isnan(fb),
            true,
            false,
            true,
            false,
            true,
            false,
            sa < sb,
            sa <= sb,
            sa > sb,
            sa != sb,
            sa >= sb,
            true,
            false,
            sa < sb,
            sa == sb,
            sa <= sb,
            sa > sb,
            sa >= sb,
            true,
            false,
            ua <= ub,
            ua >= ub,
            true,
            false,
            ua < ub,
            ua == ub,
            ua <= ub,
            ua > ub,
            ua >= ub,
            true,
            false,
            !std::isnan(ha) && !std::isnan(hb),
            std::isnan(ha) || std::isnan(hb),
            !(ha >= hb),
            !(ha < hb || ha > hb),
            !(ha > hb),
            !(ha <= hb),
            !(ha < hb),
            true,
            false,
            ha < hb || ha > hb,
            !std::isnan(ha) && !std::isnan(hb),
            std::isnan(ha) || std::isnan(hb),
            !(ha >= hb),
            !(ha < hb || ha > hb),
            !(ha <= hb),
            true,
            !(ha >= std::fabs(hb)),
            sb <= sa,
            ub >= ua,
            !std::isnan(fa) && !std::isnan(fb),
        };
        for (std::uint32_t k = 0; k < expected.size(); ++k) {
            const bool actual = ((Output[tid * Results + k / 32u] >> (k % 32u)) & 1u) != 0u;
            Require(actual == expected[k], "vopc compares: thread " + std::to_string(tid) + " compare " + std::to_string(k) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[k]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        for (std::uint32_t pairBase = 0; pairBase < HalfEdges.size() * HalfEdges.size(); pairBase += Threads) {
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                const auto pair = (pairBase + tid) % (HalfEdges.size() * HalfEdges.size());
                Input[tid * Inputs + 6u] = 0xabcd0000u | HalfEdges[pair / HalfEdges.size()];
                Input[tid * Inputs + 7u] = 0x12340000u | HalfEdges[pair % HalfEdges.size()];
            }
            Run(*device);
            Check();
        }
        std::puts("vopc compare tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
