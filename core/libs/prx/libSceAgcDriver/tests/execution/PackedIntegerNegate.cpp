#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 47> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xcc07410a, 0x300208ff, 0x00007fff,
    0xcc0a410b, 0x58020b04, 0xcc03420c, 0x38020b04, 0xcc08430d, 0x78020b04, 0xcc0c420e, 0x58020b04,
    0xcc0d410f, 0x38020b04, 0xcc014210, 0x38020b04, 0xcc004511, 0x9c1a0b04, 0xcc094612, 0x7c1a0b04,
    0xcc044213, 0x58020b04, 0xcc064314, 0x58020b04, 0xcc054215, 0x78020b04, 0xcc024a16, 0x30020b04,
    0xcc0b5317, 0x68020b04, 0xcc02c218, 0x58020b04, 0xcc074919, 0x30020906, 0xe0781000, 0x80010a03,
    0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x1234021bu, 0xffff7fffu, 0x8001edccu, 0x00000000u},
    {0xc000021bu, 0x0002021bu, 0x000fedccu, 0x00000000u},
    {0xedcc0001u, 0x00ff0218u, 0x0002fffeu, 0x00000000u},
    {0x000f00ffu, 0x000f0218u, 0x80100218u, 0x00000000u},
    {0x000f8000u, 0x000f021bu, 0x00ff021bu, 0x00000000u},
    {0x7fff0218u, 0x4000000fu, 0x80018000u, 0x00000000u},
    {0xffff4000u, 0x00ff8000u, 0x00021234u, 0x00000000u},
    {0x7ffeffffu, 0x021bffffu, 0x8010c000u, 0x00000000u},
    {0x80107fffu, 0x80010010u, 0xedcc8001u, 0x00000000u},
    {0xc0000000u, 0x80008000u, 0x0218021bu, 0x00000000u},
    {0x00020001u, 0x00027fffu, 0x40000218u, 0x00000000u},
    {0x00ffc000u, 0xffff0002u, 0xfffefffeu, 0x00000000u},
    {0x00100000u, 0x00ff000fu, 0xfffe4000u, 0x00000000u},
    {0x021bfffeu, 0x00ff8001u, 0x00fffffeu, 0x00000000u},
    {0xedccedccu, 0x021b021bu, 0x7ffe8001u, 0x00000000u},
    {0x00007ffeu, 0x12340218u, 0xfffe0001u, 0x00000000u},
    {0xc0000218u, 0x0010ffffu, 0xffff8001u, 0x00000000u},
    {0x400000ffu, 0x7ffec000u, 0x80008000u, 0x00000000u},
    {0x000f4000u, 0xedcc0001u, 0x80008000u, 0x00000000u},
    {0x7fff0218u, 0x8010c000u, 0x80108010u, 0x00000000u},
    {0x80008000u, 0x80000218u, 0x00007ffeu, 0x00000000u},
    {0x021bffffu, 0x000fc000u, 0x7fffedccu, 0x00000000u},
    {0x02180002u, 0x000f8000u, 0x0001edccu, 0x00000000u},
    {0x021b4000u, 0x021b0001u, 0x02188001u, 0x00000000u},
    {0x7fff8000u, 0x40008010u, 0xffff1234u, 0x00000000u},
    {0x80100000u, 0x00010001u, 0x80000218u, 0x00000000u},
    {0x4000c000u, 0xffff021bu, 0x40008010u, 0x00000000u},
    {0x7fff4000u, 0x7ffe0002u, 0x000f00ffu, 0x00000000u},
    {0x000fedccu, 0x8000fffeu, 0x021b4000u, 0x00000000u},
    {0xedcc0001u, 0x8000021bu, 0xedcc8000u, 0x00000000u},
    {0x02180000u, 0x00ffedccu, 0x12344000u, 0x00000000u},
    {0xc000ffffu, 0x80010218u, 0x7fffc000u, 0x00000000u},
};

enum class Kind { MaxI, MinI, MaxU, MinU, AddU, AddI, SubU, SubI, MulLo, MadI, MadU, Lshl, Ashr, Lshr };

constexpr std::int32_t Literal = -1;

struct Case {
    const char* name;
    Kind kind;
    std::array<std::int32_t, 3> sources;
    std::array<bool, 3> opSel;
    std::array<bool, 3> opSelHi;
    std::array<bool, 3> neg;
    std::array<bool, 3> negHi;
    bool clamp;
};

constexpr std::uint32_t LiteralValue = 0x00007fffu;

constexpr std::array<Case, Results> Cases{{
    {"v_pk_max_i16 v10, lit 0x7fff, v4 op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[1,0]", Kind::MaxI, {Literal, 0, 0}, {false, false, false}, {false, true, true}, {true, false, false}, {true, false, false}, false},
    {"v_pk_add_u16 v11, v4, v5 neg_lo:[0,1] neg_hi:[1,0]", Kind::AddU, {0, 1, 0}, {}, {true, true, true}, {false, true, false}, {true, false, false}, false},
    {"v_pk_sub_i16 v12, v4, v5 neg_lo:[1,0] neg_hi:[0,1]", Kind::SubI, {0, 1, 0}, {}, {true, true, true}, {true, false, false}, {false, true, false}, false},
    {"v_pk_min_i16 v13, v4, v5 neg_lo:[1,1] neg_hi:[1,1]", Kind::MinI, {0, 1, 0}, {}, {true, true, true}, {true, true, false}, {true, true, false}, false},
    {"v_pk_max_u16 v14, v4, v5 neg_lo:[0,1] neg_hi:[0,1]", Kind::MaxU, {0, 1, 0}, {}, {true, true, true}, {false, true, false}, {false, true, false}, false},
    {"v_pk_min_u16 v15, v4, v5 neg_lo:[1,0] neg_hi:[1,0]", Kind::MinU, {0, 1, 0}, {}, {true, true, true}, {true, false, false}, {true, false, false}, false},
    {"v_pk_mul_lo_u16 v16, v4, v5 neg_lo:[1,0] neg_hi:[0,1]", Kind::MulLo, {0, 1, 0}, {}, {true, true, true}, {true, false, false}, {false, true, false}, false},
    {"v_pk_mad_i16 v17, v4, v5, v6 neg_lo:[0,0,1] neg_hi:[1,0,1]", Kind::MadI, {0, 1, 2}, {}, {true, true, true}, {false, false, true}, {true, false, true}, false},
    {"v_pk_mad_u16 v18, v4, v5, v6 neg_lo:[1,1,0] neg_hi:[0,1,1]", Kind::MadU, {0, 1, 2}, {}, {true, true, true}, {true, true, false}, {false, true, true}, false},
    {"v_pk_lshlrev_b16 v19, v4, v5 neg_lo:[0,1] neg_hi:[0,1]", Kind::Lshl, {0, 1, 0}, {}, {true, true, true}, {false, true, false}, {false, true, false}, false},
    {"v_pk_ashrrev_i16 v20, v4, v5 neg_lo:[0,1] neg_hi:[1,1]", Kind::Ashr, {0, 1, 0}, {}, {true, true, true}, {false, true, false}, {true, true, false}, false},
    {"v_pk_lshrrev_b16 v21, v4, v5 neg_lo:[1,1] neg_hi:[0,1]", Kind::Lshr, {0, 1, 0}, {}, {true, true, true}, {true, true, false}, {false, true, false}, false},
    {"v_pk_add_i16 v22, v4, v5 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1]", Kind::AddI, {0, 1, 0}, {true, false, false}, {false, true, true}, {true, false, false}, {false, true, false}, false},
    {"v_pk_sub_u16 v23, v4, v5 op_sel:[0,1] op_sel_hi:[1,0] neg_lo:[1,1] neg_hi:[1,1]", Kind::SubU, {0, 1, 0}, {false, true, false}, {true, false, true}, {true, true, false}, {true, true, false}, false},
    {"v_pk_add_i16 v24, v4, v5 neg_lo:[0,1] neg_hi:[0,1] clamp", Kind::AddI, {0, 1, 0}, {}, {true, true, true}, {false, true, false}, {false, true, false}, true},
    {"v_pk_max_i16 v25, v6, v4 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[1,0]", Kind::MaxI, {2, 0, 0}, {true, false, false}, {false, true, true}, {true, false, false}, {true, false, false}, false},
}};

std::int32_t Signed(std::uint32_t half) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(half));
}

std::uint32_t Source(const Case& c, const std::uint32_t* in, std::size_t i, bool highLane) {
    const std::uint32_t word = c.sources[i] == Literal ? LiteralValue : in[c.sources[i]];
    const bool high = highLane ? c.opSelHi[i] : c.opSel[i];
    const bool negate = highLane ? c.negHi[i] : c.neg[i];
    return ((high ? word >> 16u : word) & 0xffffu) ^ (negate ? 0x8000u : 0u);
}

std::uint32_t Lane(const Case& c, const std::uint32_t* in, bool highLane) {
    const std::uint32_t a = Source(c, in, 0, highLane);
    const std::uint32_t b = Source(c, in, 1, highLane);
    const std::uint32_t s = Source(c, in, 2, highLane);
    std::int64_t value = 0;
    switch (c.kind) {
        case Kind::MaxI: value = std::max(Signed(a), Signed(b)); break;
        case Kind::MinI: value = std::min(Signed(a), Signed(b)); break;
        case Kind::MaxU: value = std::max(a, b); break;
        case Kind::MinU: value = std::min(a, b); break;
        case Kind::AddU: value = a + b; break;
        case Kind::AddI: value = static_cast<std::int64_t>(Signed(a)) + Signed(b); break;
        case Kind::SubU: value = static_cast<std::int64_t>(a) - b; break;
        case Kind::SubI: value = static_cast<std::int64_t>(Signed(a)) - Signed(b); break;
        case Kind::MulLo: value = static_cast<std::int64_t>(a) * b; break;
        case Kind::MadI: value = static_cast<std::int64_t>(Signed(a)) * Signed(b) + Signed(s); break;
        case Kind::MadU: value = static_cast<std::int64_t>(a) * b + s; break;
        case Kind::Lshl: value = static_cast<std::int64_t>(b) << (a & 15u); break;
        case Kind::Ashr: value = Signed(b) >> (a & 15u); break;
        case Kind::Lshr: value = b >> (a & 15u); break;
    }
    if (c.clamp) value = std::clamp<std::int64_t>(value, -32768, 32767);
    return static_cast<std::uint32_t>(value) & 0xffffu;
}

std::uint32_t Reference(const Case& c, const std::uint32_t* in) {
    return Lane(c, in, false) | (Lane(c, in, true) << 16u);
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

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
    std::uint32_t failures = 0;
    std::string report;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        const auto clampLane = [&](std::uint32_t x) { return static_cast<std::uint32_t>(std::max<std::int32_t>(-1, Signed(x))) & 0xffffu; };
        const std::uint32_t tsr = clampLane(in[0] & 0xffffu) | (clampLane(in[0] >> 16u) << 16u);
        Require(Reference(Cases[0], in) == tsr, "packed integer negate: reference disagrees with max(-1, x)");
        for (std::uint32_t i = 0; i < Results; ++i) {
            const std::uint32_t expected = Reference(Cases[i], in);
            if (out[i] != expected && failures++ < 24u) {
                report += std::string("packed integer negate: lane ") + std::to_string(tid) + " " + Cases[i].name + " is " + Hex(out[i]) + ", expected " + Hex(expected) + "\n";
            }
        }
    }
    Require(failures == 0u, report + std::to_string(failures) + " mismatches");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("packed integer negate tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
