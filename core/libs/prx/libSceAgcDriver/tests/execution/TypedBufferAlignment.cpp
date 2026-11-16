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
#include <vector>

namespace {
using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 32u;
constexpr std::uint32_t Stride = 16u;
constexpr std::uint32_t Word3Base = 0x01016facu;
constexpr std::uint32_t RawRange = 3u << 28u;
constexpr std::uint32_t DefaultSelectors = 0xfacu;
constexpr std::uint32_t Guard = 0xdeadbeefu;
alignas(256) std::array<std::uint8_t, 256> Source{};
alignas(256) std::array<std::uint32_t, Threads * 4u> Output{};
alignas(256) std::array<std::uint32_t, 32> Code{};
std::uint32_t Failures = 0u;

struct Format {
    std::uint32_t number;
    std::uint32_t width;
    std::uint32_t components;
};

struct Case {
    Format format;
    bool structured;
    bool mubuf;
    std::uint32_t records;
    std::uint32_t soffset;
    std::uint32_t immediate;
    bool d16 = false;
    std::uint32_t selectors = DefaultSelectors;
};

struct Row {
    std::uint32_t format;
    std::uint32_t lane;
    std::array<std::uint32_t, 4> words;
};

constexpr std::array<Row, 14> Rows{{
    {5u, 3u, {0x96u, 0u, 0u, 0u}},
    {18u, 2u, {0x8fu, 0x96u, 0u, 0u}},
    {18u, 3u, {0u, 0u, 0u, 0u}},
    {60u, 0u, {0x81u, 0x88u, 0x8fu, 0x96u}},
    {60u, 1u, {0u, 0u, 0u, 0u}},
    {11u, 2u, {0x968fu, 0u, 0u, 0u}},
    {11u, 3u, {0u, 0u, 0u, 0u}},
    {27u, 4u, {0xa49du, 0xb2abu, 0u, 0u}},
    {27u, 6u, {0u, 0u, 0u, 0u}},
    {69u, 0u, {0x8881u, 0x968fu, 0xa49du, 0xb2abu}},
    {69u, 4u, {0xa49du, 0xb2abu, 0xc1b8u, 0xcfc6u}},
    {69u, 2u, {0u, 0u, 0u, 0u}},
    {20u, 4u, {0xb2aba49du, 0u, 0u, 0u}},
    {20u, 2u, {0u, 0u, 0u, 0u}},
}};

constexpr std::array<Format, 7> Formats{{{5u, 1u, 1u}, {18u, 1u, 2u}, {60u, 1u, 4u}, {11u, 2u, 1u}, {27u, 2u, 2u}, {69u, 2u, 4u}, {20u, 4u, 1u}}};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t records, std::uint32_t stride, std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    const auto high = static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u);
    return {static_cast<std::uint32_t>(address), high, records, word3};
}

std::uint32_t Word3(const Case& test) {
    const auto word3 = (Word3Base & ~(0x7fu << 12u) & ~RawRange & ~0xfffu) | (test.format.number << 12u) | test.selectors;
    return test.structured ? word3 : word3 | RawRange;
}

std::vector<std::uint32_t> Program(const Case& test) {
    std::vector<std::uint32_t> code{0x34020084u};
    for (std::uint32_t reg = 4u; reg < 8u; ++reg) code.insert(code.end(), {0x7e0002ffu | (reg << 17u), 0xdeadbeefu});
    if (test.soffset != 0u) code.push_back(0xbe8c0380u + test.soffset);
    if (test.structured) code.insert(code.end(), {0x7e040280u, 0x7e060300u});
    const std::uint32_t operation = (test.selectors != DefaultSelectors || test.format.components == 4u) ? 3u : test.format.components - 1u;
    const std::uint32_t modes = 0x1000u | (test.structured ? 0x2000u : 0u) | test.immediate;
    const std::uint32_t vaddr = test.structured ? 2u : 0u;
    const std::uint32_t soffset = (test.soffset != 0u ? 0x0cu : 0x80u) << 24u;
    const std::uint32_t head = test.mubuf ? 0xe0000000u | ((operation | (test.d16 ? 0x80u : 0u)) << 18u) : 0xe8000000u | (test.format.number << 19u) | (operation << 16u);
    const std::uint32_t d16Bit = test.d16 && !test.mubuf ? 1u << 21u : 0u;
    code.insert(code.end(), {head | modes, vaddr | (4u << 8u) | soffset | d16Bit, 0xbf8c3f70u});
    for (std::uint32_t word = 0u; word < 4u; ++word) code.insert(code.end(), {0xe0701000u | (word * 4u), 0x80010001u | ((4u + word) << 8u)});
    code.push_back(0xbf810000u);
    return code;
}

std::uint32_t Component(const Case& test, std::uint32_t lane, std::uint32_t component) {
    const auto offset = lane + test.immediate;
    const auto element = test.format.width * test.format.components;
    const auto alignment = std::min(element, 4u);
    const bool aligned = (test.soffset + offset) % alignment == 0u;
    const bool inRange = test.structured ? offset + element <= Stride : test.soffset <= test.records && offset + element <= test.records - test.soffset;
    if (!aligned || !inRange) return 0u;
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0u; byte < test.format.width; ++byte) value |= static_cast<std::uint32_t>(Source[test.soffset + offset + component * test.format.width + byte]) << (8u * byte);
    return value;
}

std::uint32_t Outputs(const Case& test) {
    return test.selectors != DefaultSelectors ? 4u : test.d16 ? (test.format.components + 1u) / 2u : test.format.components;
}

std::uint32_t Expected(const Case& test, std::uint32_t lane, std::uint32_t index) {
    if (test.d16) {
        const auto high = 2u * index + 1u < test.format.components ? Component(test, lane, 2u * index + 1u) : Guard >> 16u;
        return high << 16u | Component(test, lane, 2u * index);
    }
    const auto selector = (test.selectors >> (3u * index)) & 7u;
    if (selector < 2u) return selector;
    return Component(test, lane, (selector - 4u) % test.format.components);
}

void Dispatch(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    const auto program = Program(test);
    Code.fill(0u);
    std::copy(program.begin(), program.end(), Code.begin());
    std::array<std::uint32_t, 8> userData{};
    const auto input = Descriptor(Source.data(), test.records, test.structured ? Stride : 0u, Word3(test));
    const auto output = Descriptor(Output.data(), Threads * 16u, 0u, Word3Base | RawRange);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    const std::span<const std::uint32_t> words(Code.data(), program.size());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), words, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check(const Case& test, std::uint32_t wave, std::uint32_t lane, std::uint32_t index, std::uint32_t expected) {
    const auto actual = Output[lane * 4u + index];
    if (actual == expected) return;
    ++Failures;
    std::fprintf(stderr, "wave=%u format=%u mubuf=%d d16=%d selectors=%03x structured=%d records=%u soffset=%u immediate=%u lane=%u output=%u actual=%08x expected=%08x\n", wave, test.format.number, test.mubuf, test.d16, test.selectors, test.structured, test.records, test.soffset, test.immediate, lane, index, actual, expected);
}

bool IsPlain(const Case& test) {
    return !test.structured && !test.mubuf && !test.d16 && test.soffset == 0u && test.immediate == 0u && test.records == 64u;
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    for (std::uint32_t index = 0u; index < Source.size(); ++index) Source[index] = static_cast<std::uint8_t>((0x81u + index * 7u) ^ (index >> 3u));
    Output.fill(0u);
    Dispatch(device, test, wave);
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        for (std::uint32_t index = 0u; index < Outputs(test); ++index) Check(test, wave, lane, index, Expected(test, lane, index));
    }
    if (!IsPlain(test)) return;
    for (const auto& row : Rows) {
        if (row.format != test.format.number) continue;
        for (std::uint32_t index = 0u; index < test.format.components; ++index) Check(test, wave, row.lane, index, row.words[index]);
    }
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    for (const bool mubuf : {false, true}) {
        for (const auto& format : Formats) {
            cases.push_back({format, false, mubuf, 64u, 0u, 0u});
            cases.push_back({format, false, mubuf, 64u, 1u, 0u});
            cases.push_back({format, false, mubuf, 64u, 1u, 1u});
            cases.push_back({format, false, mubuf, 64u, 0u, 2u});
            cases.push_back({format, false, mubuf, 12u, 0u, 0u});
            cases.push_back({format, true, mubuf, 16u, 0u, 0u});
            if (format.width == 4u) continue;
            cases.push_back({format, false, mubuf, 64u, 0u, 0u, true});
            cases.push_back({format, false, mubuf, 64u, 1u, 1u, true});
            cases.push_back({format, true, mubuf, 16u, 0u, 0u, true});
        }
    }
    for (const auto& format : Formats) {
        for (const auto selectors : {0xf01u, 0x249u, 0x877u}) cases.push_back({format, false, true, 64u, 0u, 0u, false, selectors});
    }
    return cases;
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto cases = Cases();
        for (const auto wave : {32u, 64u}) {
            for (const auto& test : cases) Run(*device, test, wave);
        }
        Require(Failures == 0u, "typed buffer loads differ from RDNA2");
        std::puts("typed buffer loads return zero off their element alignment like RDNA2, wave32/wave64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
