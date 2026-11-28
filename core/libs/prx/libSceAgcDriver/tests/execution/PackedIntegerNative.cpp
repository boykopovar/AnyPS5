#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t FirstResult = 16;
constexpr std::uint32_t LiteralSource = 255;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

enum Op : std::uint32_t { MadI = 0, MulLo = 1, AddI = 2, SubI = 3, Lshl = 4, Lshr = 5, Ashr = 6, MaxI = 7, MinI = 8, MadU = 9, AddU = 10, SubU = 11, MaxU = 12, MinU = 13 };

struct Case {
    Op op;
    std::array<std::uint32_t, 3> sources;
    std::uint32_t opSel;
    std::uint32_t opSelHi;
    std::uint32_t neg;
    std::uint32_t negHi;
    std::uint32_t literal;
};

bool Ternary(Op op) {
    return op == MadI || op == MadU;
}

std::vector<Case> Cases() {
    constexpr std::array<Op, 14> ops{MadI, MulLo, AddI, SubI, Lshl, Lshr, Ashr, MaxI, MinI, MadU, AddU, SubU, MaxU, MinU};
    constexpr std::array<std::array<std::uint32_t, 2>, 4> selections{{{0u, 7u}, {7u, 0u}, {0u, 0u}, {7u, 7u}}};
    std::vector<Case> cases;
    for (const auto op : ops) {
        for (const auto& [opSel, opSelHi] : selections) cases.push_back({op, {260u, 261u, 262u}, opSel, opSelHi, 0u, 0u, 0u});
    }
    cases.push_back({MadU, {260u, 261u, 262u}, 5u, 2u, 3u, 6u, 0u});
    cases.push_back({MadI, {261u, LiteralSource, 260u}, 2u, 5u, 4u, 1u, 0x8001fffeu});
    cases.push_back({Ashr, {136u, 260u, 0u}, 0u, 7u, 0u, 0u, 0u});
    cases.push_back({Lshl, {LiteralSource, 261u, 0u}, 0u, 7u, 2u, 0u, 0x000f0011u});
    cases.push_back({MaxI, {LiteralSource, 200u, 0u}, 0u, 7u, 0u, 0u, 0x7fff8000u});
    cases.push_back({SubU, {193u, LiteralSource, 0u}, 3u, 4u, 2u, 1u, 0x00010002u});
    cases.push_back({MulLo, {LiteralSource, 193u, 0u}, 1u, 6u, 3u, 3u, 0x8001fffeu});
    cases.push_back({Ashr, {LiteralSource, 200u, 0u}, 0u, 7u, 0u, 0u, 0x00030011u});
    return cases;
}

std::int64_t Signed(std::uint32_t half) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(half));
}

std::uint32_t Lane(const Case& c, const std::uint32_t* in, bool high) {
    std::array<std::uint32_t, 3> v{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto source = c.sources[i];
        std::uint32_t word = 0;
        if (source == LiteralSource) word = c.literal;
        else if (source >= 256u) word = in[source - 260u];
        else if (source >= 128u && source <= 192u) word = source - 128u;
        else if (source >= 193u && source <= 208u) word = static_cast<std::uint32_t>(192 - static_cast<std::int32_t>(source));
        const bool selected = ((high ? c.opSelHi : c.opSel) >> i) & 1u;
        const bool negated = ((high ? c.negHi : c.neg) >> i) & 1u;
        v[i] = ((selected ? word >> 16u : word) & 0xffffu) ^ (negated ? 0x8000u : 0u);
    }
    std::int64_t value = 0;
    switch (c.op) {
        case MadI: value = Signed(v[0]) * Signed(v[1]) + Signed(v[2]); break;
        case MadU: value = static_cast<std::int64_t>(v[0]) * v[1] + v[2]; break;
        case MulLo: value = static_cast<std::int64_t>(v[0]) * v[1]; break;
        case AddI: value = Signed(v[0]) + Signed(v[1]); break;
        case AddU: value = static_cast<std::int64_t>(v[0]) + v[1]; break;
        case SubI: value = Signed(v[0]) - Signed(v[1]); break;
        case SubU: value = static_cast<std::int64_t>(v[0]) - v[1]; break;
        case Lshl: value = static_cast<std::int64_t>(v[1]) << (v[0] & 15u); break;
        case Lshr: value = v[1] >> (v[0] & 15u); break;
        case Ashr: value = Signed(v[1]) >> (v[0] & 15u); break;
        case MaxI: value = std::max(Signed(v[0]), Signed(v[1])); break;
        case MinI: value = std::min(Signed(v[0]), Signed(v[1])); break;
        case MaxU: value = std::max(v[0], v[1]); break;
        case MinU: value = std::min(v[0], v[1]); break;
    }
    return static_cast<std::uint32_t>(value) & 0xffffu;
}

std::uint32_t Reference(const Case& c, const std::uint32_t* in) {
    return Lane(c, in, false) | (Lane(c, in, true) << 16u);
}

std::vector<std::uint32_t> Assemble(const std::vector<Case>& cases) {
    std::vector<std::uint32_t> code{0x34020084u, 0x34060088u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u};
    for (std::uint32_t index = 0; index < cases.size(); ++index) {
        const Case& c = cases[index];
        const auto sources = Ternary(c.op) ? c.sources : std::array<std::uint32_t, 3>{c.sources[0], c.sources[1], 0x80u};
        code.push_back(0xcc000000u | (c.op << 16u) | (((c.opSelHi >> 2u) & 1u) << 14u) | (c.opSel << 11u) | (c.negHi << 8u) | (FirstResult + index));
        code.push_back((c.neg << 29u) | ((c.opSelHi & 3u) << 27u) | (sources[2] << 18u) | (sources[1] << 9u) | sources[0]);
        if (std::find(sources.begin(), sources.end(), LiteralSource) != sources.end()) code.push_back(c.literal);
    }
    for (std::uint32_t quad = 0; quad < Results / 4u; ++quad) {
        code.push_back(0xe0781000u | (quad * 16u));
        code.push_back(0x80010003u | ((FirstResult + quad * 4u) << 8u));
    }
    code.push_back(0xbf810000u);
    return code;
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

std::uint32_t Count(std::span<const std::uint32_t> words, auto&& matches) {
    Require(words.size() >= 5u && words[0] == spv::MagicNumber, "packed integer native: the recompiler returned no SPIR-V module");
    std::uint32_t found = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Require(count != 0u && count <= words.size() - offset, "packed integer native: malformed SPIR-V instruction");
        if (matches(words.subspan(offset, count))) ++found;
        offset += count;
    }
    return found;
}

std::uint32_t Int16Declarations(std::span<const std::uint32_t> words) {
    return Count(words, [](std::span<const std::uint32_t> instruction) {
        const auto opcode = instruction[0] & spv::OpCodeMask;
        return (opcode == spv::OpCapability && instruction[1] == spv::CapabilityInt16) || (opcode == spv::OpTypeInt && instruction[2] == 16u);
    });
}

std::size_t Run(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, bool int16) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    std::vector<std::uint32_t> capabilities(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
    if (!int16) std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityInt16));
    request.target.supportedCapabilities = capabilities;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto words = std::span<const std::uint32_t>(result.spirv.Words());
    const auto declarations = Int16Declarations(words);
    Require(int16 ? declarations == 2u : declarations == 0u, std::string("packed integer native: the ") + (int16 ? "Int16" : "lane") + " build declares " + std::to_string(declarations) + " Int16 capabilities and 16-bit integer types");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return words.size();
}

void Check(const std::vector<Case>& cases, const char* build) {
    std::uint32_t failures = 0;
    std::string report;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        for (std::uint32_t index = 0; index < cases.size(); ++index) {
            const std::uint32_t expected = Reference(cases[index], in);
            const std::uint32_t actual = Output[tid * Results + index];
            if (actual != expected && failures++ < 24u) {
                report += std::string("packed integer native (") + build + "): lane " + std::to_string(tid) + " case " + std::to_string(index) + " op " + std::to_string(cases[index].op) + " is " + Hex(actual) + ", expected " + Hex(expected) + "\n";
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
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt16)) {
            std::puts("skipped, the device has no shaderInt16");
            return VulkanTestSkipped;
        }
        constexpr std::array<std::uint32_t, 8> edges{0x00000000u, 0xffffffffu, 0x80008000u, 0x7fff7fffu, 0x00018001u, 0xffff0000u, 0x0000ffffu, 0x8000fffeu};
        std::mt19937 random(0x5eed16u);
        for (std::uint32_t index = 0; index < Input.size(); ++index) Input[index] = index < Threads * Inputs / 2u ? edges[(index * 5u + index / 8u) % edges.size()] : random();
        const auto cases = Cases();
        Require(cases.size() == Results, "packed integer native: the case list does not fill the result registers");
        const auto code = Assemble(cases);
        const auto laneWords = Run(*device, code, false);
        Check(cases, "lane");
        const auto nativeWords = Run(*device, code, true);
        Check(cases, "Int16");
        Require(nativeWords < laneWords, "packed integer native: the Int16 module has " + std::to_string(nativeWords) + " words, not fewer than the lane module's " + std::to_string(laneWords));
        std::printf("packed integer native tests passed (%zu SPIR-V words with Int16, %zu lane by lane)\n", nativeWords, laneWords);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
