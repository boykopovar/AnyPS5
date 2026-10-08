#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Words = std::vector<std::uint32_t>;

constexpr std::uint32_t SharedHigh = 0x80000000u;
constexpr std::uint32_t PrivateHigh = 0x70000000u;
constexpr std::uint32_t LdsDwords = 256u;
constexpr std::uint32_t ScratchDwords = 4u;
constexpr std::size_t GuestBytes = 65536u;
constexpr std::uint32_t GuestStoreSlot = 0x200u;
constexpr std::uint32_t GuestAtomicSlot = 0x800u;
constexpr std::uint32_t WaitCnt0 = 0xbf8c0070u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t ShiftLaneBy5 = 0x34020085u;
constexpr std::uint32_t StoreByteOp = 0x18u;
constexpr std::uint32_t StoreShortOp = 0x1au;
constexpr std::uint32_t StoreDwordOp = 0x1cu;
constexpr std::uint32_t StoreDwordx2Op = 0x1du;
constexpr std::uint32_t LoadUbyteOp = 0x08u;
constexpr std::uint32_t LoadUshortOp = 0x0au;
constexpr std::uint32_t LoadDwordOp = 0x0cu;
constexpr std::uint32_t LoadDwordx2Op = 0x0du;

enum class Target : std::uint8_t { Shared, Private, Guest, Mixed };
enum class AtomicOp : std::uint8_t { Swap, Cmpswap, Add, Sub, Smin, Umin, Smax, Umax, And, Or, Xor, Inc, Dec, Fcmpswap, Fmin, Fmax };

constexpr std::size_t Atomic32Ops = 13;
constexpr std::array<std::uint32_t, 13> AtomicOpcodes32{0x30u, 0x31u, 0x32u, 0x33u, 0x35u, 0x36u, 0x37u, 0x38u, 0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du};
constexpr std::array<std::uint32_t, 16> AtomicOpcodes64{0x50u, 0x51u, 0x52u, 0x53u, 0x55u, 0x56u, 0x57u, 0x58u, 0x59u, 0x5au, 0x5bu, 0x5cu, 0x5du, 0x5eu, 0x5fu, 0x60u};
constexpr std::array<const char*, 4> TargetNames{"shared", "private", "guest", "mixed"};
constexpr std::array<const char*, 16> OpNames{"swap", "cmpswap", "add", "sub", "smin", "umin", "smax", "umax", "and", "or", "xor", "inc", "dec", "fcmpswap", "fmin", "fmax"};

struct Row {
    Target target;
    bool atomic;
    AtomicOp op;
    std::uint32_t width;
    std::uint32_t offset;
    std::uint32_t waveSize;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestBytes, GuestBytes));
#endif
        Require(block != nullptr, "flat apertures: cannot allocate the guest block");
        std::memset(block, 0, GuestBytes);
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Hex64(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint32_t FlatWord0(std::uint32_t op, bool glc, std::uint32_t offset = 0u) {
    return 0xdc000000u | (op << 18u) | (glc ? 0x10000u : 0u) | offset;
}

std::uint32_t FlatWord1(std::uint32_t vdst, std::uint32_t data, std::uint32_t address) {
    return (vdst << 24u) | (0x7du << 16u) | (data << 8u) | address;
}

void EmitFlat(Words& code, std::uint32_t op, bool glc, std::uint32_t vdst, std::uint32_t data, std::uint32_t address) {
    code.push_back(FlatWord0(op, glc));
    code.push_back(FlatWord1(vdst, data, address));
}

void EmitTableLoad(Words& code, std::uint32_t vdst, std::uint32_t offset) {
    code.push_back(0xe0381000u | offset);
    code.push_back(0x80010001u | (vdst << 8u));
}

void EmitOutputStore(Words& code, std::uint32_t vdata, std::uint32_t offset) {
    code.push_back(0xe0701000u | offset);
    code.push_back(0x80000001u | (vdata << 8u));
}

std::uint32_t StoreOp(std::uint32_t width) {
    return width == 8u ? StoreByteOp : (width == 16u ? StoreShortOp : StoreDwordOp);
}

std::uint32_t LoadOp(std::uint32_t width) {
    return width == 8u ? LoadUbyteOp : (width == 16u ? LoadUshortOp : LoadDwordOp);
}

Words Kernel32(const Row& row) {
    Words code{ShiftLaneBy5};
    EmitTableLoad(code, 2u, 0u);
    EmitTableLoad(code, 6u, 16u);
    code.push_back(WaitCnt0);
    if (row.atomic) {
        EmitFlat(code, StoreDwordOp, false, 0u, 6u, 2u);
        code.push_back(WaitCnt0);
        EmitFlat(code, AtomicOpcodes32[static_cast<std::size_t>(row.op)], true, 10u, 4u, 2u);
        code.push_back(WaitCnt0);
        EmitFlat(code, LoadDwordOp, false, 11u, 0u, 2u);
        code.push_back(WaitCnt0);
        EmitOutputStore(code, 10u, 0u);
        EmitOutputStore(code, 11u, 4u);
    } else {
        EmitFlat(code, StoreOp(row.width), false, 0u, 4u, 2u);
        code.push_back(WaitCnt0);
        EmitFlat(code, LoadOp(row.width), false, 10u, 0u, 2u);
        EmitFlat(code, LoadUbyteOp, false, 11u, 0u, 2u);
        code.push_back(FlatWord0(LoadUbyteOp, false, row.width / 8u - 1u));
        code.push_back(FlatWord1(12u, 0u, 2u));
        code.push_back(WaitCnt0);
        EmitOutputStore(code, 10u, 0u);
        EmitOutputStore(code, 11u, 4u);
        EmitOutputStore(code, 12u, 8u);
    }
    code.push_back(EndProgram);
    return code;
}

Words Kernel64(const Row& row) {
    Words code{ShiftLaneBy5};
    EmitTableLoad(code, 2u, 0u);
    EmitTableLoad(code, 6u, 16u);
    code.push_back(WaitCnt0);
    EmitFlat(code, StoreDwordx2Op, false, 0u, 8u, 2u);
    code.push_back(WaitCnt0);
    EmitFlat(code, AtomicOpcodes64[static_cast<std::size_t>(row.op)], true, 10u, 4u, 2u);
    code.push_back(WaitCnt0);
    EmitFlat(code, LoadDwordx2Op, false, 12u, 0u, 2u);
    code.push_back(WaitCnt0);
    EmitOutputStore(code, 10u, 0u);
    EmitOutputStore(code, 11u, 4u);
    EmitOutputStore(code, 12u, 8u);
    EmitOutputStore(code, 13u, 12u);
    code.push_back(EndProgram);
    return code;
}

Words Kernel(const Row& row) {
    return row.atomic && row.width == 64u ? Kernel64(row) : Kernel32(row);
}

std::uint32_t OperandA(std::uint32_t lane) {
    return 0x9e3779b1u * (lane + 1u) ^ 0x5a5a0f0fu;
}

std::uint32_t InitValue(std::uint32_t lane) {
    return (lane % 2u) != 0u ? (0x80000000u | (lane * 0x101u)) : (lane * 0x35u + 7u);
}

std::uint32_t CmpValue(std::uint32_t lane) {
    return (lane % 2u) != 0u ? InitValue(lane) + 1u : InitValue(lane);
}

std::uint32_t Apply(AtomicOp op, std::uint32_t init, std::uint32_t a, std::uint32_t b) {
    const auto signedInit = static_cast<std::int32_t>(init);
    const auto signedA = static_cast<std::int32_t>(a);
    switch (op) {
    case AtomicOp::Swap: return a;
    case AtomicOp::Cmpswap: return init == b ? a : init;
    case AtomicOp::Add: return init + a;
    case AtomicOp::Sub: return init - a;
    case AtomicOp::Smin: return signedInit < signedA ? init : a;
    case AtomicOp::Umin: return init < a ? init : a;
    case AtomicOp::Smax: return signedInit > signedA ? init : a;
    case AtomicOp::Umax: return init > a ? init : a;
    case AtomicOp::And: return init & a;
    case AtomicOp::Or: return init | a;
    case AtomicOp::Xor: return init ^ a;
    case AtomicOp::Inc: return init >= a ? 0u : init + 1u;
    case AtomicOp::Dec: return (init == 0u || init > a) ? a : init - 1u;
    default: break;
    }
    return init;
}

std::uint64_t Pack(std::uint32_t high, std::uint32_t low) {
    return (static_cast<std::uint64_t>(high) << 32u) | low;
}

std::uint64_t Bits(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool IsFloatOp(AtomicOp op) {
    return op == AtomicOp::Fcmpswap || op == AtomicOp::Fmin || op == AtomicOp::Fmax;
}

std::uint64_t IntInit64(std::uint32_t lane) {
    return Pack((lane % 2u) != 0u ? (0x80000000u | (lane * 0x1357u)) : (lane * 0x0a0bu + 0x11u), InitValue(lane));
}

std::uint64_t IntA64(std::uint32_t lane) {
    return Pack(0x85ebca6bu * (lane + 1u) ^ 0xc2b2ae35u, OperandA(lane));
}

std::uint64_t IntCmp64(std::uint32_t lane) {
    return (lane % 2u) != 0u ? IntInit64(lane) + 1u : IntInit64(lane);
}

double FloatInit(std::uint32_t lane) {
    return (lane % 2u) == 0u ? static_cast<double>(lane + 1u) * 3.25 : -static_cast<double>(lane + 1u) * 7.5;
}

double FloatA(std::uint32_t lane) {
    return (lane % 2u) == 0u ? -static_cast<double>(lane + 2u) * 1.5 : static_cast<double>(lane + 3u) * 2.75 - 2.0;
}

double FloatCmp(std::uint32_t lane) {
    return (lane % 2u) == 0u ? FloatInit(lane) : FloatInit(lane) + 1.0;
}

std::uint64_t InitBits64(AtomicOp op, std::uint32_t lane) {
    return IsFloatOp(op) ? Bits(FloatInit(lane)) : IntInit64(lane);
}

std::uint64_t OperandBits64(AtomicOp op, std::uint32_t lane) {
    return IsFloatOp(op) ? Bits(FloatA(lane)) : IntA64(lane);
}

std::uint64_t CmpBits64(AtomicOp op, std::uint32_t lane) {
    return IsFloatOp(op) ? Bits(FloatCmp(lane)) : IntCmp64(lane);
}

std::uint64_t Apply64(AtomicOp op, std::uint64_t init, std::uint64_t a, std::uint64_t cmp) {
    const auto signedInit = static_cast<std::int64_t>(init);
    const auto signedA = static_cast<std::int64_t>(a);
    switch (op) {
    case AtomicOp::Swap: return a;
    case AtomicOp::Cmpswap: return init == cmp ? a : init;
    case AtomicOp::Add: return init + a;
    case AtomicOp::Sub: return init - a;
    case AtomicOp::Smin: return signedInit < signedA ? init : a;
    case AtomicOp::Umin: return init < a ? init : a;
    case AtomicOp::Smax: return signedInit > signedA ? init : a;
    case AtomicOp::Umax: return init > a ? init : a;
    case AtomicOp::And: return init & a;
    case AtomicOp::Or: return init | a;
    case AtomicOp::Xor: return init ^ a;
    case AtomicOp::Inc: return init >= a ? 0u : init + 1u;
    case AtomicOp::Dec: return (init == 0u || init > a) ? a : init - 1u;
    default: break;
    }
    return init;
}

std::uint64_t Expected64(AtomicOp op, std::uint32_t lane) {
    if (IsFloatOp(op)) {
        const auto init = FloatInit(lane);
        const auto a = FloatA(lane);
        switch (op) {
        case AtomicOp::Fcmpswap: return Bits(init == FloatCmp(lane) ? a : init);
        case AtomicOp::Fmin: return Bits(std::min(init, a));
        case AtomicOp::Fmax: return Bits(std::max(init, a));
        default: break;
        }
    }
    return Apply64(op, IntInit64(lane), IntA64(lane), IntCmp64(lane));
}

std::uint64_t LaneAddress(const Row& row, std::uint32_t lane, std::uint64_t guestBase) {
    Target target = row.target;
    if (target == Target::Mixed) {
        target = lane % 3u == 0u ? Target::Shared : (lane % 3u == 1u ? Target::Private : Target::Guest);
    }
    const std::uint32_t privateBase = row.width == 64u ? 8u : 4u;
    switch (target) {
    case Target::Shared: return (static_cast<std::uint64_t>(SharedHigh) << 32u) | (8u * lane + row.offset);
    case Target::Private: return (static_cast<std::uint64_t>(PrivateHigh) << 32u) | (privateBase + row.offset);
    case Target::Guest: return guestBase + 8u * lane + row.offset;
    default: break;
    }
    throw std::runtime_error("flat apertures: no address for the row target");
}

std::string Describe(const Row& row) {
    std::string access;
    if (row.atomic) {
        access = std::string(row.width == 64u ? "flat_atomic64_" : "flat_atomic_") + OpNames[static_cast<std::size_t>(row.op)];
    } else {
        access = "flat_store" + std::to_string(row.width);
    }
    return access + " " + TargetNames[static_cast<std::size_t>(row.target)] + " offset " + std::to_string(row.offset) + " wave" + std::to_string(row.waveSize);
}

void Check(const Row& row, const std::vector<std::uint32_t>& output) {
    const std::string name = Describe(row);
    for (std::uint32_t lane = 0; lane < row.waveSize; ++lane) {
        const auto a = OperandA(lane);
        if (row.atomic) {
            const auto init = InitValue(lane);
            const auto old = output[lane * 8u];
            Require(old == init, name + ": lane " + std::to_string(lane) + " returned " + Hex(old) + ", expected " + Hex(init));
            const auto expected = Apply(row.op, init, a, CmpValue(lane));
            const auto left = output[lane * 8u + 1u];
            Require(left == expected, name + ": lane " + std::to_string(lane) + " left " + Hex(left) + ", expected " + Hex(expected));
        } else {
            const auto expected = row.width == 8u ? (a & 0xffu) : (row.width == 16u ? (a & 0xffffu) : a);
            const auto read = output[lane * 8u];
            Require(read == expected, name + ": lane " + std::to_string(lane) + " read " + Hex(read) + " back, expected " + Hex(expected));
            const auto first = output[lane * 8u + 1u];
            Require(first == (a & 0xffu), name + ": lane " + std::to_string(lane) + " has first byte " + Hex(first) + " in memory, expected " + Hex(a & 0xffu));
            const auto last = output[lane * 8u + 2u];
            const auto lastExpected = (a >> (8u * (row.width / 8u - 1u))) & 0xffu;
            Require(last == lastExpected, name + ": lane " + std::to_string(lane) + " has last byte " + Hex(last) + " in memory, expected " + Hex(lastExpected));
        }
    }
}

void Check64(const Row& row, const std::vector<std::uint32_t>& output) {
    const std::string name = Describe(row);
    for (std::uint32_t lane = 0; lane < row.waveSize; ++lane) {
        const auto old = Pack(output[lane * 8u + 1u], output[lane * 8u]);
        const auto init = InitBits64(row.op, lane);
        Require(old == init, name + ": lane " + std::to_string(lane) + " returned " + Hex64(old) + ", expected " + Hex64(init));
        const auto left = Pack(output[lane * 8u + 3u], output[lane * 8u + 2u]);
        const auto expected = Expected64(row.op, lane);
        Require(left == expected, name + ": lane " + std::to_string(lane) + " left " + Hex64(left) + ", expected " + Hex64(expected));
    }
}

ShaderRecompiler::RecompileRequest MakeRequest(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<ShaderRecompiler::MemoryRegion, 1>& memory, std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::uint32_t waveSize) {
    return ShaderRecompiler::RecompileRequest{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
}

void Run(AgcDriver::VulkanDevice& device, const Row& row, const GuestBlock& guest) {
    const auto lanes = row.waveSize;
    const auto guestSlot = row.atomic ? GuestAtomicSlot : GuestStoreSlot;
    const auto guestBase = static_cast<std::uint64_t>(guest.Address()) + guestSlot;
    const bool wideAtomic = row.atomic && row.width == 64u;
    std::vector<std::uint32_t> table(lanes * 8u, 0u);
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const auto address = LaneAddress(row, lane, guestBase);
        table[lane * 8u] = static_cast<std::uint32_t>(address);
        table[lane * 8u + 1u] = static_cast<std::uint32_t>(address >> 32u);
        if (wideAtomic) {
            const auto operand = OperandBits64(row.op, lane);
            const auto cmp = CmpBits64(row.op, lane);
            const auto init = InitBits64(row.op, lane);
            table[lane * 8u + 2u] = static_cast<std::uint32_t>(operand);
            table[lane * 8u + 3u] = static_cast<std::uint32_t>(operand >> 32u);
            table[lane * 8u + 4u] = static_cast<std::uint32_t>(cmp);
            table[lane * 8u + 5u] = static_cast<std::uint32_t>(cmp >> 32u);
            table[lane * 8u + 6u] = static_cast<std::uint32_t>(init);
            table[lane * 8u + 7u] = static_cast<std::uint32_t>(init >> 32u);
        } else {
            table[lane * 8u + 2u] = OperandA(lane);
            table[lane * 8u + 3u] = row.atomic ? CmpValue(lane) : 0u;
            table[lane * 8u + 4u] = row.atomic ? InitValue(lane) : 0u;
        }
    }
    std::vector<std::uint32_t> output(lanes * 8u, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    const auto tableDescriptor = BufferDescriptor(table.data(), static_cast<std::uint32_t>(table.size() * 4u));
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin());
    std::copy(tableDescriptor.begin(), tableDescriptor.end(), userData.begin() + 4);
    const auto code = Kernel(row);
    const std::span<const std::uint32_t> codeSpan(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(codeSpan.data()), std::as_bytes(codeSpan)}}};
    ShaderRecompiler::ShaderComputeStageInfo compute{{lanes, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    compute.scratchDwords = ScratchDwords;
    auto request = MakeRequest(device, codeSpan, memory, userData, compute, row.waveSize);
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(codeSpan.data()));
    device.WaitIdle();
    if (wideAtomic) {
        Check64(row, output);
    } else {
        Check(row, output);
    }
}

std::vector<Row> Rows() {
    std::vector<Row> rows;
    const std::array<Target, 3> single{Target::Shared, Target::Private, Target::Guest};
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 7> stores{{{8u, 0u}, {8u, 3u}, {16u, 0u}, {16u, 1u}, {16u, 3u}, {32u, 0u}, {32u, 2u}}};
    for (const Target target : single) {
        for (const auto [width, offset] : stores) {
            rows.push_back(Row{target, false, AtomicOp::Swap, width, offset, 32u});
        }
    }
    rows.push_back(Row{Target::Mixed, false, AtomicOp::Swap, 16u, 3u, 32u});
    rows.push_back(Row{Target::Mixed, false, AtomicOp::Swap, 32u, 2u, 32u});
    for (const Target target : single) {
        for (std::uint32_t op = 0; op < Atomic32Ops; ++op) {
            rows.push_back(Row{target, true, static_cast<AtomicOp>(op), 32u, 0u, 32u});
        }
    }
    for (const Target target : single) {
        for (std::uint32_t op = 0; op < OpNames.size(); ++op) {
            rows.push_back(Row{target, true, static_cast<AtomicOp>(op), 64u, 0u, 32u});
        }
    }
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Add, 32u, 0u, 32u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Cmpswap, 32u, 0u, 32u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Add, 64u, 0u, 32u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Cmpswap, 64u, 0u, 32u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Fmax, 64u, 0u, 32u});
    rows.push_back(Row{Target::Mixed, false, AtomicOp::Swap, 16u, 3u, 64u});
    rows.push_back(Row{Target::Mixed, false, AtomicOp::Swap, 32u, 2u, 64u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Add, 32u, 0u, 64u});
    rows.push_back(Row{Target::Shared, true, AtomicOp::Cmpswap, 32u, 0u, 64u});
    rows.push_back(Row{Target::Mixed, true, AtomicOp::Add, 64u, 0u, 64u});
    rows.push_back(Row{Target::Shared, true, AtomicOp::Fmin, 64u, 0u, 64u});
    return rows;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool wave64 = device->Target().subgroupSize >= 32u;
        GuestBlock guest;
        std::size_t checked = 0;
        for (const Row& row : Rows()) {
            if (row.waveSize == 64u && !wave64) continue;
            Run(*device, row, guest);
            ++checked;
        }
        if (!wave64) std::printf("skipped the wave64 rows, subgroup size %u cannot hold a wave64 in two lanes\n", device->Target().subgroupSize);
        std::printf("flat aperture rows checked: %zu\n", checked);
        std::puts("flat aperture tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
