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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> ZeroHighCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x36140cff, 0x0000ffff, 0xd705000a, 0x00020b04, 0xe0702000, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> ChainCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x7e140280, 0xd703000a, 0x00020906, 0xd705000a, 0x00020b0a, 0xe0702000, 0x80010a03,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 15> KeptHighCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x7e140306, 0xd705000a, 0x00020b04, 0xe0702000, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 20> MaskedFactorsCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x360808ff, 0x0000ffff, 0x360a0aff, 0x0000ffff, 0x36140cff, 0x0000ffff, 0xd705000a,
    0x00020b04, 0xe0702000, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> PackedLowLaneCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x360808ff, 0x0000ffff, 0x360a0aff, 0x0000ffff, 0xcc01400a, 0x18020b04, 0xe0702000,
    0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> PackedHighLaneCode{
    0x34020082, 0x34060080, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0x360808ff, 0xffff0000, 0x360a0aff, 0xffff0000, 0xcc01400a, 0x18020b04, 0xe0702000,
    0x80010a03, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 3>, 8> Edges{{
    {0xffffffffu, 0xffffffffu, 0x00000000u}, {0x0000ffffu, 0x0000ffffu, 0xabcd1234u}, {0xffff0000u, 0xffff0000u, 0xffffffffu},
    {0x8000ffffu, 0x7fff8000u, 0x12345678u}, {0x12345678u, 0x9abcdef0u, 0x80000000u}, {0x00010001u, 0x00010001u, 0x0000ffffu},
    {0x0000ff00u, 0x00000100u, 0xffff0000u}, {0x00000000u, 0xffffffffu, 0x7fff7fffu},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = tid < Edges.size() ? Edges[tid][j] : static_cast<std::uint32_t>(state >> 32u);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Low16Product(std::uint32_t a, std::uint32_t b) {
    return ((a & 0xffffu) * (b & 0xffffu)) & 0xffffu;
}

void Check(const char* name, const std::function<std::uint32_t(std::uint32_t, std::uint32_t, std::uint32_t)>& expected) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const auto want = expected(in[0], in[1], in[2]);
        Require(Output[tid] == want, std::string("mul lo u16: ") + name + ": thread " + std::to_string(tid) + " (" + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + ") wrote " + Hex(Output[tid]) + ", expected " + Hex(want));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device, ZeroHighCode);
        Check("v_mul_lo_u16 into a register whose high half is zero", [](std::uint32_t a, std::uint32_t b, std::uint32_t) { return Low16Product(a, b); });
        Run(*device, ChainCode);
        Check("v_add_nc_u16 then v_mul_lo_u16 in a cleared register", [](std::uint32_t a, std::uint32_t b, std::uint32_t c) { return Low16Product(c + a, b); });
        Run(*device, KeptHighCode);
        Check("v_mul_lo_u16 into a register holding another value", [](std::uint32_t a, std::uint32_t b, std::uint32_t c) { return (c & 0xffff0000u) | Low16Product(a, b); });
        Run(*device, MaskedFactorsCode);
        Check("v_mul_lo_u16 of factors masked to 16 bits into a register whose high half is zero", [](std::uint32_t a, std::uint32_t b, std::uint32_t) { return Low16Product(a, b); });
        Run(*device, PackedLowLaneCode);
        Check("v_pk_mul_lo_u16 of factors whose high halves are zero", [](std::uint32_t a, std::uint32_t b, std::uint32_t) { return Low16Product(a, b); });
        Run(*device, PackedHighLaneCode);
        Check("v_pk_mul_lo_u16 of factors whose low halves are zero", [](std::uint32_t a, std::uint32_t b, std::uint32_t) { return Low16Product(a >> 16u, b >> 16u) << 16u; });
        std::puts("mul lo u16 high half tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
