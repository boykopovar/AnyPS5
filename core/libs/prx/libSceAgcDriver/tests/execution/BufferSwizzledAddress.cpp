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
constexpr std::uint32_t Threads = 64u;
constexpr std::uint32_t Records = 4096u;
constexpr std::uint32_t Guard = 0xdeadbeefu;
constexpr std::uint8_t Fill = 0xa5u;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t StructuredFormat = 0x01016facu;
constexpr std::uint32_t AddTid = 1u << 23u;
constexpr std::uint32_t SourceBytes = 8192u;
alignas(256) std::array<std::uint8_t, SourceBytes> Source{};
alignas(256) std::array<std::uint8_t, SourceBytes> Target{};
alignas(256) std::array<std::uint32_t, Threads * 4u> Output{};
alignas(256) std::array<std::uint32_t, 40> Code{};
std::uint32_t Failures = 0u;

struct Op {
    std::uint32_t load;
    std::uint32_t store;
    std::uint32_t bytes;
    std::uint32_t components;
};

constexpr Op Ubyte{8u, 24u, 1u, 1u};
constexpr Op Ushort{10u, 26u, 2u, 1u};
constexpr Op Dword{12u, 28u, 4u, 1u};
constexpr Op Dwordx2{13u, 29u, 4u, 2u};
constexpr Op Dwordx4{14u, 30u, 4u, 4u};

struct Case {
    Op op;
    bool store;
    bool idxen;
    bool offen;
    std::uint32_t offset;
    std::uint32_t immediate;
    std::uint32_t stride;
    std::uint32_t indexStride;
    bool addTid;
    bool swizzle;
};

Case Load(Op op, bool idxen, bool offen, std::uint32_t offset, std::uint32_t stride, std::uint32_t indexStride, bool addTid = false, std::uint32_t immediate = 0u, bool swizzle = true) {
    return {op, false, idxen, offen, offset, immediate, stride, indexStride, addTid, swizzle};
}

Case Store(Op op, std::uint32_t offset, std::uint32_t stride, std::uint32_t indexStride) {
    return {op, true, true, true, offset, 0u, stride, indexStride, false, true};
}

std::uint32_t SourceWord(std::uint32_t index) {
    return index * 0x9e3779b1u + 0x1234567u;
}

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t records, std::uint32_t stride, std::uint32_t word1Flags, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    const auto high = static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u) | word1Flags;
    return {static_cast<std::uint32_t>(address), high, records, format};
}

std::uint32_t Spread(const Case& test) {
    if (!test.store) return 0u;
    return test.op.components == 4u ? 2u : test.op.components == 2u ? 1u : 0u;
}

std::uint32_t Index(const Case& test, std::uint32_t thread, std::uint32_t wave) {
    if (test.idxen) return thread << Spread(test);
    return test.addTid ? thread % wave : 0u;
}

std::uint32_t Swizzled(const Case& test, std::uint32_t index, std::uint32_t offset) {
    const auto step = 8u << test.indexStride;
    return ((index / step) * test.stride + (offset & ~3u)) * step + (index % step) * 4u + (offset & 3u);
}

std::uint32_t Address(const Case& test, std::uint32_t thread, std::uint32_t wave, std::uint32_t component) {
    const auto index = Index(test, thread, wave);
    const auto offset = (test.offen ? test.offset : 0u) + test.immediate;
    if (!test.swizzle || !(test.idxen || test.addTid)) return index * test.stride + offset + component * 4u;
    if (test.idxen) return Swizzled(test, index, offset) + component * 4u;
    return Swizzled(test, index, offset + component * 4u);
}

std::uint32_t Mov(std::uint32_t reg, std::uint32_t value) {
    return 0x7e000280u + (reg << 17u) + value;
}

std::vector<std::uint32_t> Addressing(const Case& test) {
    std::vector<std::uint32_t> code;
    if (test.idxen) code.push_back(Spread(test) == 0u ? 0x7e080300u : 0x34080080u + Spread(test));
    if (test.offen) code.push_back(Mov(test.idxen ? 5u : 4u, test.offset));
    return code;
}

std::uint32_t Mubuf(std::uint32_t opcode, const Case& test) {
    return 0xe0000000u | (opcode << 18u) | (test.offen ? 0x1000u : 0u) | (test.idxen ? 0x2000u : 0u) | test.immediate;
}

std::vector<std::uint32_t> Program(const Case& test) {
    std::vector<std::uint32_t> code{0x34180084u};
    const auto addressing = Addressing(test);
    code.insert(code.end(), addressing.begin(), addressing.end());
    if (test.store) {
        for (std::uint32_t component = 0u; component < 4u; ++component) code.insert(code.end(), {0x4a1000ffu + (component << 17u), 0x70000000u + component * 0x01000000u});
        code.insert(code.end(), {Mubuf(test.op.store, test), 0x80000804u, 0xbf810000u});
        return code;
    }
    for (std::uint32_t component = 0u; component < 4u; ++component) code.insert(code.end(), {0x7e1002ffu + (component << 17u), Guard});
    code.insert(code.end(), {Mubuf(test.op.load, test), 0x80000804u, 0xbf8c3f70u, 0xe0781000u, 0x8001080cu, 0xbf810000u});
    return code;
}

std::uint32_t Wide(std::uint32_t address, std::uint32_t bytes) {
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0u; byte < bytes; ++byte) value |= static_cast<std::uint32_t>(Source[address + byte]) << (8u * byte);
    return value;
}

void Dispatch(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    const auto program = Program(test);
    Require(program.size() <= Code.size(), "swizzled buffer program is too long");
    Code.fill(0u);
    std::copy(program.begin(), program.end(), Code.begin());
    std::array<std::uint32_t, 8> userData{};
    const void* memoryUnderTest = test.store ? static_cast<const void*>(Target.data()) : static_cast<const void*>(Source.data());
    const auto tested = Descriptor(memoryUnderTest, Records, test.stride, test.swizzle ? 0x80000000u : 0u, StructuredFormat | (test.indexStride << 21u) | (test.addTid ? AddTid : 0u));
    const auto output = Descriptor(Output.data(), Threads * 16u, 0u, 0u, RawFormat);
    std::copy(tested.begin(), tested.end(), userData.begin());
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

void Report(const Case& test, std::uint32_t wave, std::uint32_t position, std::uint32_t actual, std::uint32_t expected) {
    if (++Failures > 24u) return;
    std::fprintf(stderr, "wave=%u store=%d opcode=%u idxen=%d offen=%d offset=%u immediate=%u stride=%u indexStride=%u addTid=%d swizzle=%d at=%u actual=%08x expected=%08x\n",
                 wave, test.store, test.store ? test.op.store : test.op.load, test.idxen, test.offen, test.offset, test.immediate, test.stride, test.indexStride, test.addTid, test.swizzle, position, actual, expected);
}

void VerifyLoad(const Case& test, std::uint32_t wave) {
    for (std::uint32_t thread = 0u; thread < Threads; ++thread) {
        for (std::uint32_t component = 0u; component < test.op.components; ++component) {
            const auto expected = Wide(Address(test, thread, wave, component), test.op.bytes);
            const auto actual = Output[thread * 4u + component];
            if (actual != expected) Report(test, wave, thread * 4u + component, actual, expected);
        }
        for (std::uint32_t component = test.op.components; component < 4u; ++component) {
            if (Output[thread * 4u + component] != Guard) Report(test, wave, thread * 4u + component, Output[thread * 4u + component], Guard);
        }
    }
}

void VerifyStore(const Case& test, std::uint32_t wave) {
    std::array<std::uint8_t, SourceBytes> expected{};
    expected.fill(Fill);
    for (std::uint32_t thread = 0u; thread < Threads; ++thread) {
        for (std::uint32_t component = 0u; component < test.op.components; ++component) {
            const std::uint32_t value = 0x70000000u + component * 0x01000000u + thread;
            const auto address = Address(test, thread, wave, component);
            for (std::uint32_t byte = 0u; byte < test.op.bytes; ++byte) expected[address + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
        }
    }
    for (std::uint32_t index = 0u; index < Target.size(); ++index) {
        if (Target[index] != expected[index]) Report(test, wave, index, Target[index], expected[index]);
    }
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t wave) {
    for (std::uint32_t index = 0u; index < SourceBytes / 4u; ++index) {
        const auto word = SourceWord(index);
        for (std::uint32_t byte = 0u; byte < 4u; ++byte) Source[index * 4u + byte] = static_cast<std::uint8_t>(word >> (8u * byte));
    }
    Target.fill(Fill);
    Output.fill(Guard);
    Dispatch(device, test, wave);
    if (test.store) VerifyStore(test, wave);
    else VerifyLoad(test, wave);
}

void AddLoads(std::vector<Case>& cases) {
    for (const auto stride : {16u, 32u}) {
        for (std::uint32_t indexStride = 0u; indexStride < 4u; ++indexStride) {
            for (const auto offset : {0u, 4u, 8u}) {
                cases.push_back(Load(Dword, true, true, offset, stride, indexStride));
                cases.push_back(Load(Dwordx2, true, true, offset, stride, indexStride));
            }
            cases.push_back(Load(Dwordx4, true, true, 0u, stride, indexStride));
            cases.push_back(Load(Dwordx4, true, true, 8u, 32u, indexStride));
        }
    }
    for (const auto offset : {1u, 5u, 15u}) cases.push_back(Load(Ubyte, true, true, offset, 16u, 0u));
    for (const auto offset : {2u, 6u, 14u}) cases.push_back(Load(Ushort, true, true, offset, 16u, 1u));
    cases.push_back(Load(Dword, true, true, 4u, 16u, 0u, false, 4u));
    cases.push_back(Load(Dword, true, false, 0u, 16u, 1u, false, 8u));
    cases.push_back(Load(Dword, true, false, 0u, 16u, 0u));
}

void AddWithoutIndex(std::vector<Case>& cases) {
    for (const auto offset : {0u, 4u, 12u}) {
        cases.push_back(Load(Dword, false, true, offset, 16u, 0u));
        cases.push_back(Load(Dword, false, true, offset, 16u, 2u, true));
        cases.push_back(Load(Dword, true, true, offset, 16u, 0u, true));
    }
    cases.push_back(Load(Dword, false, false, 0u, 16u, 0u, false, 4u));
    cases.push_back(Load(Dword, false, false, 0u, 16u, 0u, true, 4u));
    cases.push_back(Load(Dword, false, false, 0u, 16u, 1u, true, 8u));
    cases.push_back(Load(Dwordx4, false, false, 0u, 16u, 0u, true));
    cases.push_back(Load(Dword, false, false, 0u, 16u, 0u, true, 0u, false));
    cases.push_back(Load(Dword, true, true, 4u, 16u, 0u, true, 0u, false));
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    AddLoads(cases);
    AddWithoutIndex(cases);
    for (const auto indexStride : {0u, 1u, 3u}) {
        cases.push_back(Store(Dword, 4u, 16u, indexStride));
        cases.push_back(Store(Dwordx2, 8u, 16u, indexStride));
        cases.push_back(Store(Dwordx4, 0u, 16u, indexStride));
        cases.push_back(Store(Ubyte, 5u, 16u, indexStride));
        cases.push_back(Store(Ushort, 6u, 16u, indexStride));
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
        Require(Failures == 0u, "swizzled buffer addresses differ from the RDNA2 layout");
        std::puts("swizzled buffer addresses follow the RDNA2 layout, wave32/wave64");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
