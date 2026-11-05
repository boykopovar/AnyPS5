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
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 8;
alignas(256) std::array<std::uint32_t, Threads> Input{
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu, 0x00800000u, 0x80800000u,
    0x00800001u, 0x80800001u, 0x3f000000u, 0xbf000000u, 0x3effffffu, 0x3f000001u, 0xc0800000u, 0xc07fffffu,
    0xc0800001u, 0x3f800000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00001u, 0x7f800001u, 0x7f7fffffu,
    0xff7fffffu, 0x00400000u, 0x80400000u, 0x0000ffffu, 0x3e800000u, 0xbe800000u, 0x80000000u, 0x01000000u,
};
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

struct Constant {
    std::uint32_t bits;
    std::uint32_t inlineSource;
};

constexpr std::array<Constant, 9> Constants{{
    {0x00000000u, 0x80u}, {0x80000000u, 0xffu}, {0x00000001u, 0xffu}, {0x807fffffu, 0xffu}, {0x3f000000u, 0xf0u},
    {0xc0800000u, 0xf7u}, {0x00800000u, 0xffu}, {0x7f800000u, 0xffu}, {0x7fc00000u, 0xffu},
}};

constexpr std::array<std::uint32_t, 12> Predicates{0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu};
constexpr std::array<const char*, 12> PredicateNames{"lt", "eq", "le", "gt", "lg", "ge", "nge", "nlg", "ngt", "nle", "neq", "nlt"};

constexpr std::uint32_t Compares = static_cast<std::uint32_t>(Constants.size() * Predicates.size() * 2u);

std::vector<std::uint32_t> Build(bool registerOperand) {
    std::vector<std::uint32_t> code{0x34020082u, 0x34060085u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u, 0x7e2c0281u};
    for (std::uint32_t word = 0; word < Words; ++word) code.push_back(0x7e000280u | ((30u + word) << 17));
    std::uint32_t bit = 0;
    for (const auto& constant : Constants) {
        if (registerOperand) code.insert(code.end(), {0x7e2a02ffu, constant.bits});
        const std::uint32_t source = registerOperand ? 256u + 21u : constant.inlineSource;
        const bool literal = !registerOperand && source == 0xffu;
        for (const std::uint32_t predicate : Predicates) {
            for (const bool constantFirst : {true, false}) {
                if (constantFirst) {
                    code.push_back(0x7c000000u | (predicate << 17) | (4u << 9) | source);
                } else {
                    code.push_back(0xd4000000u | (predicate << 16) | 106u);
                    code.push_back((source << 9) | (256u + 4u));
                }
                if (literal) code.push_back(constant.bits);
                const std::uint32_t accumulator = 30u + bit / 32u;
                code.push_back((0x01u << 25) | (20u << 17) | (22u << 9) | 0x80u);
                code.push_back(0xd4000000u | (0x36fu << 16) | accumulator);
                code.push_back(((256u + accumulator) << 18) | ((0x80u + bit % 32u) << 9) | (256u + 20u));
                ++bit;
            }
        }
    }
    for (std::uint32_t word = 0; word < Words; ++word) code.insert(code.end(), {0xe0701000u | (word * 4u), 0x80010003u | ((30u + word) << 8)});
    code.push_back(0xbf810000u);
    return code;
}

std::uint32_t Flush(std::uint32_t bits, bool flush) {
    return flush && (bits & 0x7f800000u) == 0u ? bits & 0x80000000u : bits;
}

bool Compare(std::uint32_t predicate, std::uint32_t lhsBits, std::uint32_t rhsBits) {
    const float lhs = std::bit_cast<float>(lhsBits);
    const float rhs = std::bit_cast<float>(rhsBits);
    switch (predicate) {
    case 0x01u: return lhs < rhs;
    case 0x02u: return lhs == rhs;
    case 0x03u: return lhs <= rhs;
    case 0x04u: return lhs > rhs;
    case 0x05u: return lhs < rhs || lhs > rhs;
    case 0x06u: return lhs >= rhs;
    case 0x09u: return !(lhs >= rhs);
    case 0x0au: return !(lhs < rhs || lhs > rhs);
    case 0x0bu: return !(lhs > rhs);
    case 0x0cu: return !(lhs <= rhs);
    case 0x0du: return !(lhs == rhs);
    case 0x0eu: return !(lhs < rhs);
    default: throw std::runtime_error("f32 compare constant: unknown predicate");
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, bool registerOperand) {
    Output.fill(0xdeadbeefu);
    const auto code = Build(registerOperand);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const std::string& mode, bool flush) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t value = Flush(Input[tid], flush);
        std::uint32_t bit = 0;
        for (const auto& constant : Constants) {
            const std::uint32_t flushedConstant = Flush(constant.bits, flush);
            for (std::size_t predicate = 0; predicate < Predicates.size(); ++predicate) {
                for (const bool constantFirst : {true, false}) {
                    const bool expected = constantFirst ? Compare(Predicates[predicate], flushedConstant, value) : Compare(Predicates[predicate], value, flushedConstant);
                    const bool actual = ((Output[tid * Words + bit / 32u] >> (bit % 32u)) & 1u) != 0u;
                    Require(actual == expected, "f32 compare constant: " + mode + " lane " + std::to_string(tid) + " v_cmp_" + PredicateNames[predicate] + "_f32 " +
                        (constantFirst ? Hex(constant.bits) + ", " + Hex(Input[tid]) : Hex(Input[tid]) + ", " + Hex(constant.bits)) + " is " + (actual ? "1" : "0") + ", expected " + (expected ? "1" : "0"));
                    ++bit;
                }
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        static_assert(Compares <= Words * 32u);
        struct Mode {
            const char* name;
            std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
            bool flush;
        };
        const std::array<Mode, 3> modes{{
            {"no float mode", std::nullopt, true},
            {"IEEE=0 f32 denormals flushed", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, true},
            {"IEEE=1 all denormals flushed", ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false}, true},
        }};
        for (const auto& mode : modes) {
            for (const bool registerOperand : {true, false}) {
                Run(*device, mode.floatMode, registerOperand);
                Check(std::string(mode.name) + (registerOperand ? ", register operand" : ", constant operand"), mode.flush);
            }
        }
        std::puts("f32 compare constant tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
