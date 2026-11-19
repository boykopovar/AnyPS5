#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "tests/TemporaryFile.hpp"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 256;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint64_t Untouched = 0xdeadbeefdeadbeefull;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};
alignas(256) std::array<std::uint64_t, 8192> Gds{};
alignas(256) std::array<std::uint64_t, 8192> Readback{};

struct Operation {
    std::uint32_t encoding;
    const char* name;
    bool returnsValue;
};

constexpr auto Operations = std::to_array<Operation>({
    {0x40, "add", false}, {0x41, "sub", false}, {0x42, "rsub", false},
    {0x43, "inc", false}, {0x44, "dec", false}, {0x45, "smin", false},
    {0x46, "smax", false}, {0x47, "umin", false}, {0x48, "umax", false},
    {0x49, "and", false}, {0x4a, "or", false}, {0x4b, "xor", false},
    {0x4c, "mskor", false}, {0x50, "cmpst", false},
    {0x60, "add_rtn", true}, {0x61, "sub_rtn", true}, {0x62, "rsub_rtn", true},
    {0x63, "inc_rtn", true}, {0x64, "dec_rtn", true}, {0x65, "smin_rtn", true},
    {0x66, "smax_rtn", true}, {0x67, "umin_rtn", true}, {0x68, "umax_rtn", true},
    {0x69, "and_rtn", true}, {0x6a, "or_rtn", true}, {0x6b, "xor_rtn", true},
    {0x6c, "mskor_rtn", true}, {0x6d, "wrxchg", true}, {0x70, "cmpst_rtn", true},
});

struct Row {
    std::uint64_t old;
    std::uint64_t data0;
    std::uint64_t data1;
};

constexpr auto Rows = std::to_array<Row>({
    {0x00000000ffffffffull, 1, 0xfedcba9876543210ull},
    {0x0000000100000000ull, 1, 0x123456789abcdef0ull},
    {0xffffffffffffffffull, 1, 0x8000000000000001ull},
    {0, 1, 0xabcdef0123456789ull},
    {0x8000000000000000ull, 0x7fffffffffffffffull, 0x0123456789abcdefull},
    {0x7fffffffffffffffull, 0x8000000000000000ull, 0xfedcba9876543210ull},
    {7, 7, 0x123456789abcdef0ull},
    {8, 7, 0xfedcba9876543210ull},
    {6, 7, 0x8000000000000001ull},
    {0, 0, 0x123456789abcdef0ull},
    {0x123456789abcdef0ull, 0x123456789abcdef0ull, 0xfedcba9876543210ull},
    {0x123456789abcdef0ull, 0x123456799abcdef0ull, 0xfedcba9876543210ull},
    {0xaaaaaaaa55555555ull, 0xff00ff0000ff00ffull, 0x0123456789abcdefull},
    {0x0000000100000000ull, 0x0000000100000000ull, 0x76543210fedcba98ull},
    {0xffffffffffffffffull, 0xffffffffffffffffull, 0x1020304050607080ull},
    {0x8000000000000001ull, 0xfffffffffffffffeull, 0xabcdef0123456789ull},
});

std::uint64_t Reference(std::uint32_t opcode, const Row& row) {
    const auto a = row.old;
    const auto b = row.data0;
    const auto c = row.data1;
    switch (opcode) {
    case 0x40: case 0x60: return a + b;
    case 0x41: case 0x61: return a - b;
    case 0x42: case 0x62: return b - a;
    case 0x43: case 0x63: return a >= b ? 0 : a + 1;
    case 0x44: case 0x64: return a == 0 || a > b ? b : a - 1;
    case 0x45: case 0x65: return std::bit_cast<std::int64_t>(b) < std::bit_cast<std::int64_t>(a) ? b : a;
    case 0x46: case 0x66: return std::bit_cast<std::int64_t>(b) > std::bit_cast<std::int64_t>(a) ? b : a;
    case 0x47: case 0x67: return std::min(a, b);
    case 0x48: case 0x68: return std::max(a, b);
    case 0x49: case 0x69: return a & b;
    case 0x4a: case 0x6a: return a | b;
    case 0x4b: case 0x6b: return a ^ b;
    case 0x4c: case 0x6c: return (a & ~b) | c;
    case 0x50: case 0x70: return a == b ? c : a;
    case 0x6d: return b;
    default: throw std::runtime_error("unknown reference operation");
    }
}

std::uint64_t Word(const std::uint32_t* words) {
    return words[0] | (static_cast<std::uint64_t>(words[1]) << 32u);
}

void SetWord(std::uint32_t* words, std::uint64_t value) {
    words[0] = static_cast<std::uint32_t>(value);
    words[1] = static_cast<std::uint32_t>(value >> 32u);
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Expect(std::uint64_t actual, std::uint64_t expected, const std::string& what) {
    Require(actual == expected, what + ": got " + Hex(actual) + ", expected " + Hex(expected));
}

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dma(AgcDriver::QueueState& queue, bool upload) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(upload ? Gds.data() : Readback.data());
    const auto low = static_cast<std::uint32_t>(pointer);
    const auto high = static_cast<std::uint32_t>(pointer >> 32u);
    const std::array<std::uint32_t, 7> packet = upload
        ? std::array<std::uint32_t, 7>{0xc0055000, 0x00100000, low, high, 0, 0, 65536}
        : std::array<std::uint32_t, 7>{0xc0055000, 0x20000000, 0, 0, low, high, 65536};
    AgcDriver::Pm4::Validate(packet, 0);
    AgcDriver::Pm4::Execute(packet, queue);
}

void Ds(std::vector<std::uint32_t>& code, std::uint32_t op, std::uint32_t offset, std::uint32_t dest = 10, std::uint32_t addr = 2, std::uint32_t data = 6) {
    code.push_back(0xd8020000u | (op << 18u) | offset);
    code.push_back((dest << 24u) | (8u << 16u) | (data << 8u) | addr);
}

void Store(std::vector<std::uint32_t>& code, std::uint32_t reg, std::uint32_t offset) {
    code.push_back(0xe0701000u | offset);
    code.push_back(0x80010003u | (reg << 8u));
}

void LoadData(std::vector<std::uint32_t>& code) {
    for (std::uint32_t i = 0; i < 4; ++i) {
        code.push_back(0xe0301008u + i * 4u);
        code.push_back(0x80000001u | ((6u + i) << 8u));
    }
    code.push_back(0xbf8c3f70u);
}

void Mask(std::vector<std::uint32_t>& code, std::uint32_t active) {
    code.insert(code.end(), {0x7da800ffu, active});
}

void Unmask(std::vector<std::uint32_t>& code) {
    code.push_back(0xbefe04c1u);
}

std::vector<std::uint32_t> OperationCode(std::uint32_t active, bool overlap) {
    std::vector<std::uint32_t> code{0x34020085, 0x3404008a, 0x3406008a};
    for (std::uint32_t i = 0; i < Operations.size(); ++i) {
        LoadData(code);
        const auto dest = overlap ? 6u : 10u;
        if (!overlap) code.insert(code.end(), {0x7e1402ffu, Sentinel, 0x7e1602ffu, Sentinel});
        Mask(code, active);
        Ds(code, Operations[i].encoding, i * 8u, dest);
        code.push_back(0xbf8cc07fu);
        Unmask(code);
        Store(code, dest, i * 8u);
        Store(code, dest + 1u, i * 8u + 4u);
    }
    code.push_back(0xbf810000u);
    return code;
}

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t wave, std::uint32_t threads = Threads, bool groupId = false, bool atomics = true) {
    Output.fill(Sentinel);
    std::vector<std::uint32_t> userData(8);
    const auto input = Descriptor(Input.data(), sizeof(Input));
    const auto output = Descriptor(Output.data(), sizeof(Output));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0, {groupId, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {wave, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto available = device.Target().supportedCapabilities;
    std::vector<std::uint32_t> capabilities(available.begin(), available.end());
    if (!atomics) std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityInt64Atomics));
    else if (!TargetHasCapability(device.Target(), spv::CapabilityInt64Atomics)) capabilities.push_back(spv::CapabilityInt64Atomics);
    request.target.supportedCapabilities = capabilities;
    return ShaderRecompiler::Recompile(request);
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t wave, std::uint32_t threads = Threads, std::uint32_t groups = 1, bool groupId = false) {
    const auto result = Compile(device, code, wave, threads, groupId);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckOperations(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, std::uint32_t active, bool overlap) {
    Gds.fill(Untouched);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        SetWord(&Input[lane * Inputs], row.old);
        SetWord(&Input[lane * Inputs + 2], row.data0);
        SetWord(&Input[lane * Inputs + 4], row.data1);
        for (std::uint32_t i = 0; i < Operations.size(); ++i) Gds[lane * 128u + i] = row.old;
    }
    Dma(queue, true);
    const auto code = OperationCode(active, overlap);
    Dispatch(device, code, wave);
    Dma(queue, false);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        for (std::uint32_t i = 0; i < Operations.size(); ++i) {
            const auto& op = Operations[i];
            const auto what = "wave" + std::to_string(wave) + " lane " + std::to_string(lane) + " " + op.name + (overlap ? " overlap" : "");
            Expect(Readback[lane * 128u + i], lane < active ? Reference(op.encoding, row) : row.old, what + " memory");
            const auto untouched = overlap ? row.data0 : Untouched;
            Expect(Word(&Output[lane * Results + i * 2u]), lane < active && op.returnsValue ? row.old : untouched, what + " return");
        }
        for (std::uint32_t i = Operations.size(); i < 128; ++i) Expect(Readback[lane * 128u + i], Untouched, "operation guard");
    }
}

void CheckExchanges(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, bool stride, bool overlap) {
    Gds.fill(Untouched);
    const auto first = stride ? 0u : 3u;
    const auto second = stride ? 64u : 9u;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        SetWord(&Input[lane * Inputs + 2], row.data0);
        SetWord(&Input[lane * Inputs + 4], row.data1);
        Gds[lane * 128u + first] = row.old;
        Gds[lane * 128u + second] = ~row.old;
    }
    Dma(queue, true);
    std::vector<std::uint32_t> code{0x34020085, 0x3404008a, 0x3406008a};
    LoadData(code);
    const auto dest = overlap ? 6u : 10u;
    Mask(code, 47);
    Ds(code, stride ? 0x6f : 0x6e, stride ? 0x100u : 0x903u, dest);
    code.push_back(0xbf8cc07fu);
    Unmask(code);
    for (std::uint32_t i = 0; i < 4; ++i) Store(code, dest + i, i * 4u);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave);
    Dma(queue, false);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        const auto what = std::string(stride ? "wrxchg2st64" : "wrxchg2") + " lane " + std::to_string(lane);
        Expect(Readback[lane * 128u + first], lane < 47 ? row.data0 : row.old, what + " first memory");
        Expect(Readback[lane * 128u + second], lane < 47 ? row.data1 : ~row.old, what + " second memory");
        if (lane < 47) {
            Expect(Word(&Output[lane * Results]), row.old, what + " first old");
            Expect(Word(&Output[lane * Results + 2]), ~row.old, what + " second old");
        } else if (overlap) {
            Expect(Word(&Output[lane * Results]), row.data0, what + " inactive first");
            Expect(Word(&Output[lane * Results + 2]), row.data1, what + " inactive second");
        }
        for (std::uint32_t i = 0; i < 128; ++i) {
            if (i != first && i != second) Expect(Readback[lane * 128u + i], Untouched, what + " guard");
        }
    }
}

void CheckContention(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, std::uint32_t opcode) {
    constexpr std::uint32_t groups = 32;
    constexpr std::uint32_t repetitions = 8;
    constexpr std::uint32_t count = Threads * groups * repetitions;
    constexpr std::uint64_t start = 0x12345678fffffff0ull;
    constexpr std::uint64_t step = 0x100000001ull;
    Gds.fill(Untouched);
    Gds[0] = start;
    Dma(queue, true);
    std::vector<std::uint32_t> code{0x34060083, 0x7e040280, 0x7e0c0281, 0x7e0e0281};
    for (std::uint32_t i = 0; i < repetitions; ++i) Ds(code, opcode, 0);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave, Threads, groups);
    Dma(queue, false);
    if (opcode == 0x60) Expect(Readback[0], start + step * count, "cross-workgroup add conservation");
    else Expect(Readback[0], count - 1u, "cross-workgroup inc conservation");
    for (std::uint32_t i = 1; i < Gds.size(); ++i) Expect(Readback[i], Untouched, "contention guard");
}

void CheckBounds(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave) {
    Gds.fill(Untouched);
    Gds.back() = 0x01234567ffffffffull;
    Dma(queue, true);
    std::vector<std::uint32_t> code{0x3406008a, 0x7e0402ff, 0xfff8u, 0x7e0c0281, 0x7e0e0280};
    Mask(code, 1);
    Ds(code, 0x60, 0);
    code.push_back(0xbf8cc07f);
    Unmask(code);
    Store(code, 10, 0);
    Store(code, 11, 4);
    code.insert(code.end(), {0x7e0402ff, 0xfffcu});
    Mask(code, 1);
    Ds(code, 0x60, 0);
    code.push_back(0xbf8cc07f);
    Unmask(code);
    Store(code, 10, 8);
    Store(code, 11, 12);
    code.insert(code.end(), {0x7e0402ff, 0x10000u});
    Mask(code, 1);
    Ds(code, 0x60, 0);
    code.push_back(0xbf8cc07f);
    Unmask(code);
    Store(code, 10, 16);
    Store(code, 11, 20);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave);
    Dma(queue, false);
    Expect(Readback.back(), Gds.back() + 1, "last valid qword memory");
    Expect(Word(Output.data()), Gds.back(), "last valid qword return");
    Expect(Word(Output.data() + 2), 0, "partial qword out of bounds");
    Expect(Word(Output.data() + 4), 0, "past GDS out of bounds");
    for (std::uint32_t i = 0; i + 1 < Gds.size(); ++i) Expect(Readback[i], Untouched, "bounds guard");
}

void CheckDwordView(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, bool cas) {
    Gds.fill(Untouched);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        SetWord(&Input[lane * Inputs + 2], row.old);
        SetWord(&Input[lane * Inputs + 4], row.data0);
    }
    Dma(queue, true);
    std::vector<std::uint32_t> code{0x34020085, 0x34040084, 0x3406008a};
    LoadData(code);
    Ds(code, 0x0d, 0, 0, 2, 6);
    Ds(code, 0x0d, 4, 0, 2, 7);
    code.insert(code.end(), {0xbf8cc07f, 0x7e1402ff, Sentinel, 0x7e1602ff, Sentinel});
    Mask(code, 43);
    Ds(code, cas ? 0x63 : 0x60, 0, 10, 2, 8);
    code.push_back(0xbf8cc07f);
    Unmask(code);
    Ds(code, 0x76, 0, 12);
    code.push_back(0xbf8cc07f);
    Store(code, 10, 0);
    Store(code, 11, 4);
    Store(code, 12, 8);
    Store(code, 13, 12);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave);
    Dma(queue, false);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& row = Rows[lane % Rows.size()];
        const auto expected = lane < 43 ? Reference(cas ? 0x63 : 0x60, row) : row.old;
        const auto what = "dword/qword alias lane " + std::to_string(lane) + (cas ? " CAS" : " native");
        Expect(Word(&Output[lane * Results]), lane < 43 ? row.old : Untouched, what + " old");
        Expect(Word(&Output[lane * Results + 2]), expected, what + " DS read");
        Expect(Readback[lane * 2u], expected, what + " host read");
        Expect(Readback[lane * 2u + 1u], Untouched, what + " neighbouring qword");
    }
    std::puts("GDS dword stores and reads share the qword atomic storage");
}

void CheckInactive(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave) {
    Gds.fill(Untouched);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        SetWord(&Input[lane * Inputs + 2], 0x123456789abcdef0ull);
        SetWord(&Input[lane * Inputs + 4], 0xfedcba9876543210ull);
    }
    Dma(queue, true);
    const auto code = OperationCode(0, false);
    Dispatch(device, code, wave);
    Dma(queue, false);
    Require(Readback == Gds, "EXEC zero changed GDS through a qword atomic");
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t i = 0; i < Operations.size(); ++i) {
            Expect(Word(&Output[lane * Results + i * 2u]), Untouched, "EXEC zero changed the destination");
        }
    }
    std::puts("GDS qword atomics with EXEC zero preserve storage and destinations");
}

void CheckExchangeContention(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, bool stride) {
    constexpr std::uint32_t groups = 8;
    constexpr std::uint32_t count = groups * Threads;
    constexpr std::uint64_t seed0 = 0xabcdef0123456789ull;
    constexpr std::uint64_t seed1 = 0x9876543210abcdefull;
    const auto second = stride ? 64u : 1u;
    Gds.fill(Untouched);
    Gds[0] = seed0;
    Gds[second] = seed1;
    Dma(queue, true);
    std::vector<std::uint32_t> code{
        0x7e080208, 0x34080886, 0x4a080900, 0x34060884,
        0x7e040280, 0x7e0c0304, 0x7e0e02ff, 0x12345678,
        0x7e100304, 0x7e1202ff, 0x87654321,
    };
    Ds(code, stride ? 0x6f : 0x6e, 0x100);
    code.push_back(0xbf8cc07fu);
    for (std::uint32_t i = 0; i < 4; ++i) Store(code, 10 + i, i * 4u);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave, Threads, groups, true);
    Dma(queue, false);
    std::vector<std::uint64_t> firstReturns{Readback[0]};
    std::vector<std::uint64_t> secondReturns{Readback[second]};
    std::vector<std::uint64_t> firstInputs{seed0};
    std::vector<std::uint64_t> secondInputs{seed1};
    for (std::uint32_t i = 0; i < count; ++i) {
        firstReturns.push_back(Word(&Output[i * 4u]));
        secondReturns.push_back(Word(&Output[i * 4u + 2u]));
        firstInputs.push_back(0x1234567800000000ull | i);
        secondInputs.push_back(0x8765432100000000ull | i);
    }
    std::sort(firstReturns.begin(), firstReturns.end());
    std::sort(secondReturns.begin(), secondReturns.end());
    std::sort(firstInputs.begin(), firstInputs.end());
    std::sort(secondInputs.begin(), secondInputs.end());
    Require(firstReturns == firstInputs, "contended wrxchg2 first qword lost or tore a value");
    Require(secondReturns == secondInputs, "contended wrxchg2 second qword lost or tore a value");
    for (std::uint32_t i = 0; i < Gds.size(); ++i) {
        if (i != 0 && i != second) Expect(Readback[i], Untouched, "contended wrxchg2 guard");
    }
    std::puts(stride ? "GDS wrxchg2st64 cross-workgroup permutations verified" : "GDS wrxchg2 cross-workgroup permutations verified");
}

void CheckExchangeBounds(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, bool stride) {
    constexpr std::uint64_t old = 0x01234567ffffffffull;
    constexpr std::uint64_t first = 0xfedcba9876543210ull;
    constexpr std::uint64_t second = 0x76543210abcdef01ull;
    Gds.fill(Untouched);
    Gds.back() = old;
    Dma(queue, true);
    std::vector<std::uint32_t> code{
        0x3406008a,
        0x7e0402ff, stride ? 0xfff8u : 0xfff0u,
        0x7e0c02ff, static_cast<std::uint32_t>(first),
        0x7e0e02ff, static_cast<std::uint32_t>(first >> 32u),
        0x7e1002ff, static_cast<std::uint32_t>(second),
        0x7e1202ff, static_cast<std::uint32_t>(second >> 32u),
    };
    Mask(code, 1);
    Ds(code, stride ? 0x6f : 0x6e, stride ? 0x100u : 0x201u, 6);
    code.push_back(0xbf8cc07fu);
    Unmask(code);
    for (std::uint32_t i = 0; i < 4; ++i) Store(code, 6 + i, i * 4u);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave);
    Dma(queue, false);
    Expect(Readback.back(), first, "wrxchg2 last valid qword memory");
    Expect(Word(Output.data()), old, "wrxchg2 last valid qword return");
    Expect(Word(Output.data() + 2), 0, "wrxchg2 out-of-bounds second qword return");
    for (std::uint32_t i = 0; i + 1 < Gds.size(); ++i) Expect(Readback[i], Untouched, "wrxchg2 boundary guard");
    std::puts(stride ? "GDS wrxchg2st64 bounds apply independently to both qwords" : "GDS wrxchg2 bounds apply independently to both qwords");
}

void CheckAliasedView(std::span<const std::uint32_t> words) {
    struct Declaration {
        std::uint32_t width = 0;
        std::uint32_t element = 0;
        std::uint32_t member = 0;
        std::uint32_t pointee = 0;
        std::uint32_t variableType = 0;
        std::uint32_t stride = 0;
        std::uint32_t binding = 0xffffffffu;
        std::uint32_t set = 0xffffffffu;
        bool coherent = false;
        bool aliased = false;
        bool storageBuffer = false;
    };
    std::vector<Declaration> declarations(words[3]);
    for (std::size_t index = 5; index < words.size();) {
        const auto size = words[index] >> 16u;
        Require(size != 0 && size <= words.size() - index, "invalid SPIR-V declaration length");
        const auto instruction = words.subspan(index, size);
        const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffffu);
        switch (opcode) {
        case spv::OpTypeInt:
            declarations.at(instruction[1]).width = instruction[2];
            break;
        case spv::OpTypeRuntimeArray:
            declarations.at(instruction[1]).element = instruction[2];
            break;
        case spv::OpTypeStruct:
            if (size == 3) declarations.at(instruction[1]).member = instruction[2];
            break;
        case spv::OpTypePointer:
            if (instruction[2] == spv::StorageClassStorageBuffer) declarations.at(instruction[1]).pointee = instruction[3];
            break;
        case spv::OpVariable:
            declarations.at(instruction[2]).variableType = instruction[1];
            declarations.at(instruction[2]).storageBuffer = instruction[3] == spv::StorageClassStorageBuffer;
            break;
        case spv::OpDecorate: {
            auto& declaration = declarations.at(instruction[1]);
            switch (instruction[2]) {
            case spv::DecorationCoherent: declaration.coherent = true; break;
            case spv::DecorationAliased: declaration.aliased = true; break;
            case spv::DecorationBinding: declaration.binding = instruction[3]; break;
            case spv::DecorationDescriptorSet: declaration.set = instruction[3]; break;
            case spv::DecorationArrayStride: declaration.stride = instruction[3]; break;
            default: break;
            }
            break;
        }
        default: break;
        }
        index += size;
    }
    const auto arrayOf = [&](const Declaration& variable) -> const Declaration& {
        const auto& pointer = declarations.at(variable.variableType);
        const auto& block = declarations.at(pointer.pointee);
        return declarations.at(block.member);
    };
    bool found = false;
    for (const auto& wide : declarations) {
        if (!wide.storageBuffer) continue;
        const auto& array = arrayOf(wide);
        if (declarations.at(array.element).width != 64) continue;
        Require(array.stride == 8, "GDS qword view has an incorrect byte stride");
        Require(wide.coherent && wide.aliased, "GDS qword view omitted coherent alias decorations");
        for (const auto& narrow : declarations) {
            if (!narrow.storageBuffer || narrow.binding != wide.binding || narrow.set != wide.set) continue;
            const auto& narrowArray = arrayOf(narrow);
            if (declarations.at(narrowArray.element).width != 32) continue;
            Require(narrowArray.stride == 4 && narrow.coherent && narrow.aliased, "GDS dword view is not a compatible coherent alias");
            found = true;
        }
    }
    Require(found, "GDS qword view does not share the dword view's descriptor binding");
}

void CheckRequirements(AgcDriver::VulkanDevice& device) {
    auto code = OperationCode(Threads, false);
    const auto result = Compile(device, code, 32);
    const std::span<const std::uint32_t> words(result.spirv.Words());
    CheckAliasedView(words);
    std::vector<std::uint32_t> widths(words[3]);
    std::vector<std::uint32_t> constants(words[3]);
    std::array<bool, 11> seen{};
    constexpr std::array<spv::Op, 11> expected{
        spv::OpAtomicIAdd, spv::OpAtomicISub, spv::OpAtomicSMin, spv::OpAtomicSMax,
        spv::OpAtomicUMin, spv::OpAtomicUMax, spv::OpAtomicAnd, spv::OpAtomicOr,
        spv::OpAtomicXor, spv::OpAtomicExchange, spv::OpAtomicCompareExchange,
    };
    bool capability = false;
    for (std::size_t index = 5; index < words.size();) {
        const auto count = words[index] >> 16u;
        const auto opcode = static_cast<spv::Op>(words[index] & 0xffffu);
        Require(count != 0 && count <= words.size() - index, "invalid SPIR-V instruction size");
        const auto inst = words.subspan(index, count);
        if (opcode == spv::OpCapability && inst[1] == spv::CapabilityInt64Atomics) capability = true;
        if (opcode == spv::OpTypeInt) widths.at(inst[1]) = inst[2];
        if (opcode == spv::OpConstant && widths.at(inst[1]) == 32) constants.at(inst[2]) = inst[3];
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (opcode != expected[i] || widths.at(inst[1]) != 64) continue;
            seen[i] = true;
            Require(constants.at(inst[4]) == spv::ScopeDevice, "GDS qword atomic does not use device scope");
        }
        index += count;
    }
    Require(capability, "GDS qword atomics omitted Int64Atomics");
    Require(std::ranges::all_of(seen, [](bool value) { return value; }), "GDS qword atomics omitted a native opcode or CAS loop");
    bool refused = false;
    try {
        static_cast<void>(Compile(device, code, 32, Threads, false, false));
    } catch (const std::exception& error) {
        refused = std::string(error.what()).find("64-bit GDS atomics need shaderBufferInt64Atomics") != std::string::npos;
    }
    Require(refused, "GDS qword atomics did not reject a target without shaderBufferInt64Atomics");
    for (const auto opcode : {0x51u, 0x52u, 0x53u, 0x71u, 0x72u, 0x73u}) {
        std::vector<std::uint32_t> floating{0x7e040280, 0x7e0c0280, 0x7e0e0280, 0x7e100280, 0x7e120280};
        Ds(floating, opcode, 0);
        floating.push_back(0xbf810000u);
        refused = false;
        try {
            static_cast<void>(Compile(device, floating, 32));
        } catch (const std::exception& error) {
            refused = std::string(error.what()).find("floating-point GDS atomics are not supported") != std::string::npos;
        }
        Require(refused, "out-of-scope GDS float atomic was accepted");
    }
    std::puts("GDS qword capabilities, native opcodes, scope and unsupported forms verified");
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = TemporaryFile::Open();
        Require(file != nullptr, "cannot open stderr capture");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Require(saved >= 0 && _dup2(_fileno(file), _fileno(stderr)) == 0, "cannot redirect stderr");
#else
        saved = dup(fileno(stderr));
        Require(saved >= 0 && dup2(fileno(file), fileno(stderr)) >= 0, "cannot redirect stderr");
#endif
    }

    ~StderrCapture() {
        if (file != nullptr) static_cast<void>(Finish());
    }

    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        std::array<char, 4096> buffer{};
        for (std::size_t count; (count = std::fread(buffer.data(), 1, buffer.size(), file)) != 0;) text.append(buffer.data(), count);
        std::fclose(file);
        file = nullptr;
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

void CheckAlignment(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave) {
    for (const auto address : {1u, 2u, 3u, 4u, 5u, 6u, 7u}) {
        Gds.fill(Untouched);
        Dma(queue, true);
        std::vector<std::uint32_t> code{0x3406008a, 0x7e0402ff, address, 0x7e0c0281, 0x7e0e0280};
        if (wave == 64) code.insert(code.begin(), 0xbf800000u);
        Mask(code, 1);
        Ds(code, 0x60, 0);
        code.push_back(0xbf8cc07fu);
        Unmask(code);
        Store(code, 10, 0);
        Store(code, 11, 4);
        code.push_back(0xbf810000u);
        StderrCapture capture;
        Dispatch(device, code, wave);
        const auto log = capture.Finish();
        Require(log.find("BDA access failed: address=0x" + std::to_string(address) + " ") != std::string::npos && log.find("bytes=8") != std::string::npos && log.find("reason=7") != std::string::npos, "misaligned GDS qword did not record an Unaligned shader fault");
        Dma(queue, false);
        Expect(Word(Output.data()), 0, "misaligned GDS return");
        Require(Readback == Gds, "misaligned GDS qword changed memory");
    }
    std::puts("GDS qword byte alignments 1..7 recorded faults without modifying memory");
}

void CheckPermutation(AgcDriver::VulkanDevice& device, AgcDriver::QueueState& queue, std::uint32_t wave, bool exchange) {
    constexpr std::uint32_t groups = 8;
    constexpr std::uint32_t count = groups * Threads;
    constexpr std::uint64_t seed = 0xfedcba98fffffff0ull;
    Gds.fill(Untouched);
    Gds[0] = exchange ? Untouched : seed;
    Dma(queue, true);
    std::vector<std::uint32_t> code{
        0x7e080208, 0x34080886, 0x4a080900, 0x34060883,
        0x7e040280, 0x7e0c0281, 0x7e0e0280,
    };
    if (exchange) code.insert(code.end(), {0x7e0c0304, 0x7e0e02ff, 0x12345678});
    Ds(code, exchange ? 0x6d : 0x60, 0);
    code.push_back(0xbf8cc07fu);
    Store(code, 10, 0);
    Store(code, 11, 4);
    code.push_back(0xbf810000u);
    Dispatch(device, code, wave, Threads, groups, true);
    Dma(queue, false);
    std::vector<std::uint64_t> actual;
    std::vector<std::uint64_t> expected;
    for (std::uint32_t i = 0; i < count; ++i) {
        actual.push_back(Word(&Output[i * 2u]));
        expected.push_back(exchange ? (0x1234567800000000ull | i) : seed + i);
    }
    if (exchange) {
        actual.push_back(Readback[0]);
        expected.push_back(Untouched);
    } else {
        Expect(Readback[0], seed + count, "cross-workgroup returned add final value");
    }
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    Require(actual == expected, exchange ? "cross-workgroup exchange returns and final word are not a permutation of inputs" : "cross-workgroup add returns are not a permutation of serial counter values");
    std::puts(exchange ? "GDS exchange cross-workgroup permutation verified" : "GDS add cross-workgroup return permutation verified");
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        AgcDriver::QueueState queue;
        const std::string mode = argc > 1 ? argv[1] : "operations";
        if (mode != "requirements" && !TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics)) {
            std::puts("skipped, the device has no shaderBufferInt64Atomics");
            return VulkanTestSkipped;
        }
        for (const auto wave : {32u, 64u}) {
            if (mode == "operations") {
                CheckOperations(*device, queue, wave, Threads, false);
                CheckOperations(*device, queue, wave, 43, true);
            }
            if (mode == "exchanges") {
                for (const bool stride : {false, true}) {
                    for (const bool overlap : {false, true}) CheckExchanges(*device, queue, wave, stride, overlap);
                }
            }
            if (mode == "contention") {
                CheckContention(*device, queue, wave, 0x60);
                CheckContention(*device, queue, wave, 0x63);
                CheckPermutation(*device, queue, wave, false);
                CheckPermutation(*device, queue, wave, true);
            }
            if (mode == "bounds") {
                CheckBounds(*device, queue, wave);
                CheckExchangeBounds(*device, queue, wave, false);
                CheckExchangeBounds(*device, queue, wave, true);
            }
            if (mode == "coherence") {
                CheckDwordView(*device, queue, wave, false);
                CheckDwordView(*device, queue, wave, true);
                CheckInactive(*device, queue, wave);
            }
            if (mode == "alignment") CheckAlignment(*device, queue, wave);
            if (mode == "exchange-contention") {
                CheckExchangeContention(*device, queue, wave, false);
                CheckExchangeContention(*device, queue, wave, true);
            }
        }
        if (mode == "requirements") CheckRequirements(*device);
        std::puts("gds 64-bit integer atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
