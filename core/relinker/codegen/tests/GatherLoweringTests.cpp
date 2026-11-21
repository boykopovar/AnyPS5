#include "LoweringExecution.hpp"
#include <codegen/x86/GatherLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Site {
    std::uint8_t Opcode;
    bool Wide;
    bool Vector256;
    std::uint8_t Destination;
    std::uint8_t Mask;
    std::uint8_t Index;
    std::optional<std::uint8_t> Base;
    std::uint8_t Scale;
    std::int32_t Displacement;
    bool LongDisplacement;
    Bytes Prefixes;
};

Bytes encode(const Site& site) {
    Bytes bytes = site.Prefixes;
    const std::uint8_t base = site.Base ? *site.Base : 5;
    bytes.insert(bytes.end(), {0xC4,
        static_cast<std::uint8_t>(((site.Destination & 8) ? 0 : 0x80) | ((site.Index & 8) ? 0 : 0x40) | ((site.Base && (base & 8)) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((site.Wide ? 0x80 : 0) | ((~site.Mask & 15) << 3) | (site.Vector256 ? 4 : 0) | 1),
        site.Opcode});
    std::uint8_t mod = 0;
    if (site.Base) mod = site.LongDisplacement ? 2 : (site.Displacement != 0 || (base & 7) == 5) ? 1 : 0;
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((site.Destination & 7) << 3) | 4));
    bytes.push_back(static_cast<std::uint8_t>((site.Scale << 6) | ((site.Index & 7) << 3) | (base & 7)));
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(site.Displacement));
    if (mod == 2 || !site.Base)
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(site.Displacement) >> shift));
    return bytes;
}

void decoding() {
    const auto dd = encode({0x90, false, true, 1, 2, 3, 3, 2, 0, false, {}});
    const auto operands = Codegen::DecodeGather(dd.data(), dd.size());
    require(operands && std::string(operands->Name) == "VPGATHERDD" && operands->Vector256 && !operands->QwordIndex && !operands->QwordElement
        && operands->Destination == 1 && operands->Mask == 2 && operands->Index == 3, "VPGATHERDD was not decoded");
    const auto names = std::array<const char*, 8>{"VPGATHERDD", "VPGATHERDQ", "VPGATHERQD", "VPGATHERQQ", "VGATHERDPS", "VGATHERDPD", "VGATHERQPS", "VGATHERQPD"};
    for (std::uint8_t form = 0; form < 8; ++form) {
        const auto bytes = encode({static_cast<std::uint8_t>(0x90 + form / 2), (form & 1) != 0, false, 9, 14, 12, 13, 3, -8, false, {0x3E}});
        const auto decoded = Codegen::DecodeGather(bytes.data(), bytes.size());
        require(decoded && std::string(decoded->Name) == names[form] && decoded->Destination == 9 && decoded->Mask == 14 && decoded->Index == 12
            && decoded->Memory.Prefixes == Bytes{0x3E} && decoded->Memory.Displacement == -8, std::string(names[form]) + " lost an operand");
    }
    for (const auto& alias : {Site{0x90, false, true, 1, 1, 3, 3, 0, 0, false, {}}, Site{0x91, true, true, 4, 2, 4, 3, 0, 0, false, {}}, Site{0x92, false, false, 5, 6, 6, 3, 0, 0, false, {}}}) {
        const auto bytes = encode(alias);
        require(!Codegen::DecodeGather(bytes.data(), bytes.size()), "a gather that repeats a vector register was lowered");
    }
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0x6D, 0x90, 0xC3}, Bytes{0xC4, 0xE2, 0x6D, 0x90, 0x03}, Bytes{0xC4, 0xE2, 0x6D, 0x94, 0x0C, 0x83}, Bytes{0xC4, 0xE2, 0x6C, 0x90, 0x0C, 0x83}, Bytes{0xC4, 0xE3, 0x6D, 0x90, 0x0C, 0x83}, Bytes{0x90}})
        require(!Codegen::DecodeGather(bytes.data(), bytes.size()), "an instruction that is not a gather was taken as one");
    for (std::uint8_t opcode = 0x90; opcode <= 0x93; ++opcode) {
        const auto narrow = encode({opcode, opcode == 0x91, true, 1, 2, 3, 3, 2, 8, false, {0x67}});
        require(!Codegen::DecodeGather(narrow.data(), narrow.size()), "a gather with 32-bit addressing was lowered");
        const auto segmented = encode({opcode, opcode == 0x91, true, 1, 2, 3, 3, 2, 8, false, {0x3E, 0x67}});
        require(!Codegen::DecodeGather(segmented.data(), segmented.size()), "a gather with 32-bit addressing behind a segment prefix was lowered");
    }
    const Bytes noBase{0xC4, 0xE2, 0x6D, 0x90, 0x0C, 0x1D, 0x40, 0x00, 0x00, 0x00};
    const auto absolute = Codegen::DecodeGather(noBase.data(), noBase.size());
    require(absolute && !absolute->Memory.RipRelative && absolute->Memory.Mod == 0 && (absolute->Memory.Sib & 7) == 5 && absolute->Memory.Displacement == 0x40,
        "a VSIB operand without a base was not decoded as an absolute displacement");
    auto trailing = dd;
    trailing.push_back(0x90);
    bool rejected = false;
    try { (void)Codegen::DecodeGather(trailing.data(), trailing.size()); }
    catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "a gather with trailing bytes was accepted");
}

void matching() {
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (std::uint8_t opcode = 0x90; opcode <= 0x93; ++opcode) {
        const auto bytes = encode({opcode, opcode == 0x93, true, 0, 1, 2, 3, 1, 16, false, {}});
        require(!rosetta->Match(bytes.data(), bytes.size()), "Rosetta mode lowered a gather");
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, "AVX mode did not lower a gather through a movable stub");
        const auto narrow = encode({opcode, opcode == 0x93, true, 0, 1, 2, 3, 1, 16, false, {0x67}});
        require(!avx->Match(narrow.data(), narrow.size()), "AVX mode lowered a gather with 32-bit addressing");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

struct Machine {
    std::array<std::array<std::uint64_t, 4>, 16> Ymm;
    std::array<std::uint64_t, 16> Gpr;
    std::uint64_t Flags;
    std::uint64_t HostStack;
    std::uint64_t Stack;
    std::array<std::uint64_t, 16> RedZone;
};

static_assert(offsetof(Machine, Gpr) == 512 && offsetof(Machine, Flags) == 640 && offsetof(Machine, Stack) == 656 && offsetof(Machine, RedZone) == 664);

class Runner {
public:
    explicit Runner(const Bytes& body) {
        for (const std::uint8_t reg : {3, 5, 12, 13, 14, 15}) push(reg);
        state(0x89, 4, 648);
        state(0x8B, 4, 656);
        code.insert(code.end(), {0xFF, 0xB7});
        append32(640);
        code.push_back(0x9D);
        for (int slot = 0; slot < 16; ++slot) {
            state(0x8B, 0, 664 + slot * 8);
            stack(0x89, 0, -128 + slot * 8);
        }
        for (std::uint8_t reg = 0; reg < 16; ++reg) vector(0x6F, reg);
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) state(0x8B, reg, 512 + reg * 8);
        state(0x8B, 7, 512 + 7 * 8);
        code.insert(code.end(), body.begin(), body.end());
        stack(0x89, 7, 16);
        stack(0x8B, 7, 8);
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) state(0x89, reg, 512 + reg * 8);
        stack(0x8B, 0, 16);
        state(0x89, 0, 512 + 7 * 8);
        for (int slot = 0; slot < 16; ++slot) {
            stack(0x8B, 0, -128 + slot * 8);
            state(0x89, 0, 664 + slot * 8);
        }
        code.insert(code.end(), {0x9C, 0x58});
        state(0x89, 0, 640);
        for (std::uint8_t reg = 0; reg < 16; ++reg) vector(0x7F, reg);
        state(0x8B, 4, 648);
        code.insert(code.end(), {0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5D, 0x5B, 0xC5, 0xF8, 0x77, 0xC3});
        entry = LoweringExecution::AppendHostEntry(code);
        if (code.size() > kPage) throw std::runtime_error("gather execution code is too large");
        page = LoweringExecution::AllocatePages(kPage);
        if (!page) throw std::runtime_error("Cannot allocate the gather execution page");
        std::memcpy(page, code.data(), code.size());
        if (!LoweringExecution::ProtectPages(page, kPage, true, false)) {
            LoweringExecution::FreePages(page, kPage);
            throw std::runtime_error("Cannot make the gather execution page executable");
        }
    }
    ~Runner() { LoweringExecution::FreePages(page, kPage); }
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    void Run(Machine& machine, std::array<std::uint64_t, 1024>& stackMemory) const {
        auto* top = stackMemory.data() + stackMemory.size() - 8;
        top[1] = reinterpret_cast<std::uint64_t>(&machine);
        machine.Stack = reinterpret_cast<std::uint64_t>(top);
        LoweringExecution::Call(page, entry, &machine);
    }

private:
    static constexpr std::size_t kPage = 8192;
    Bytes code;
    std::uint8_t* page = nullptr;
    std::size_t entry = 0;

    void append32(std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) code.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
    void push(std::uint8_t reg) {
        if (reg & 8) code.push_back(0x41);
        code.push_back(static_cast<std::uint8_t>(0x50 | (reg & 7)));
    }
    void state(std::uint8_t opcode, std::uint8_t reg, std::int32_t displacement) {
        code.insert(code.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
        append32(displacement);
    }
    void stack(std::uint8_t opcode, std::uint8_t reg, std::int32_t displacement) {
        code.insert(code.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
        append32(displacement);
    }
    void vector(std::uint8_t opcode, std::uint8_t reg) {
        code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
        append32(reg * 32);
    }
};

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitGather(body, *Codegen::DecodeGather(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::uint64_t element(const std::array<std::uint64_t, 4>& value, unsigned index, unsigned size) {
    std::uint64_t out = 0;
    std::memcpy(&out, reinterpret_cast<const std::uint8_t*>(value.data()) + index * size, size);
    return out;
}

void setElement(std::array<std::uint64_t, 4>& value, unsigned index, unsigned size, std::uint64_t data) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(value.data()) + index * size, &data, size);
}

struct Shape {
    unsigned Count;
    unsigned IndexSize;
    unsigned ElementSize;
};

Shape shape(const Site& site) {
    const unsigned indexSize = (site.Opcode & 1) ? 8 : 4;
    const unsigned elementSize = site.Wide ? 8 : 4;
    return {(site.Vector256 ? 32u : 16u) / std::max(indexSize, elementSize), indexSize, elementSize};
}

Machine expected(const Machine& start, const Site& site, const std::uint8_t* stackTop) {
    auto out = start;
    const auto form = shape(site);
    std::array<std::uint64_t, 4> destination{};
    const auto& mask = start.Ymm[site.Mask];
    for (unsigned i = 0; i < form.Count; ++i) {
        auto value = element(start.Ymm[site.Destination], i, form.ElementSize);
        if (element(mask, i, form.ElementSize) >> (form.ElementSize * 8 - 1)) {
            auto index = element(start.Ymm[site.Index], i, form.IndexSize);
            if (form.IndexSize == 4) index = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(index)));
            const std::uint64_t base = !site.Base ? 0 : *site.Base == 4 ? reinterpret_cast<std::uint64_t>(stackTop) : start.Gpr[*site.Base];
            const auto address = base + (index << site.Scale) + static_cast<std::uint64_t>(static_cast<std::int64_t>(site.Displacement));
            value = 0;
            if (site.Base && *site.Base == 4) {
                const auto offset = static_cast<std::int64_t>(address - reinterpret_cast<std::uint64_t>(stackTop)) + 128;
                require(offset >= 0 && offset + form.ElementSize <= 128, "a stack-based gather test reads outside the red zone");
                std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(start.RedZone.data()) + offset, form.ElementSize);
            } else {
                std::memcpy(&value, reinterpret_cast<const void*>(address), form.ElementSize);
            }
        }
        setElement(destination, i, form.ElementSize, value);
    }
    out.Ymm[site.Destination] = destination;
    out.Ymm[site.Mask] = {};
    return out;
}

void compare(const Machine& got, const Machine& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        require(got.Ymm[reg] == wanted.Ymm[reg], describe + ": ymm" + std::to_string(reg) + " is wrong");
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        require(reg == 4 || got.Gpr[reg] == wanted.Gpr[reg], describe + ": general register " + std::to_string(reg) + " changed");
    require((got.Flags & 0xCD5) == (wanted.Flags & 0xCD5), describe + ": RFLAGS changed");
    require(got.RedZone == wanted.RedZone, describe + ": the red zone changed");
}

std::uint64_t mix(std::uint64_t value) {
    value ^= value >> 33;
    value *= 0xFF51AFD7ED558CCDull;
    value ^= value >> 29;
    return value;
}

struct Fixture {
    Machine Start;
    std::array<std::uint64_t, 1024> Stack{};
};

Fixture fixture(const Site& site, const std::array<std::uint64_t, 96>& table, std::uint64_t seed, unsigned maskPattern, std::uint64_t flags) {
    Fixture out{};
    auto& machine = out.Start;
    for (std::size_t reg = 0; reg < 16; ++reg) {
        for (std::size_t q = 0; q < 4; ++q) machine.Ymm[reg][q] = mix(seed * 131 + reg * 4 + q + 1);
        machine.Gpr[reg] = mix(seed * 977 + reg + 77);
    }
    for (auto& slot : machine.RedZone) slot = mix(seed + reinterpret_cast<std::uintptr_t>(&slot));
    machine.Flags = flags;
    const auto form = shape(site);
    const auto middle = reinterpret_cast<std::uint64_t>(table.data() + 48);
    if (site.Base && *site.Base != 4) machine.Gpr[*site.Base] = middle;
    for (unsigned i = 0; i < form.Count; ++i) {
        const bool on = maskPattern == 0 ? true : maskPattern == 1 ? false : maskPattern == 2 ? (i & 1) == 0 : ((mix(seed + i) >> 7) & 1) != 0;
        auto maskValue = mix(seed ^ (i * 0x9E37ull));
        const auto signBit = std::uint64_t{1} << (form.ElementSize * 8 - 1);
        maskValue = on ? (maskValue | signBit) : (maskValue & ~signBit);
        setElement(machine.Ymm[site.Mask], i, form.ElementSize, maskValue);
        std::int64_t offset;
        if (site.Base && *site.Base == 4) {
            const unsigned step = std::max(form.ElementSize, 1u << site.Scale);
            offset = -128 + static_cast<std::int64_t>((mix(seed + i * 3) % (128 / step)) * step) - site.Displacement;
        } else {
            offset = static_cast<std::int64_t>(mix(seed * 7 + i) % 81) * 4 - 160 - site.Displacement;
        }
        std::int64_t index = offset >> site.Scale;
        if (!site.Base) index = static_cast<std::int64_t>((middle + static_cast<std::uint64_t>(offset)) >> site.Scale);
        if (!on) index = form.IndexSize == 4 ? 0x7FFFFFF0 : 0x3FFFFFFFFFFFFFF0ll;
        setElement(machine.Ymm[site.Index], i, form.IndexSize, static_cast<std::uint64_t>(index));
    }
    return out;
}

bool hostHasAvx2() {
    return __builtin_cpu_supports("avx2") != 0;
}

void run(const Site& site, const std::array<std::uint64_t, 96>& table, std::uint64_t seed, unsigned maskPattern, std::uint64_t flags, bool native) {
    const auto bytes = encode(site);
    const std::string describe = std::string(Codegen::DecodeGather(bytes.data(), bytes.size())->Name) + (site.Vector256 ? " ymm" : " xmm")
        + " dst " + std::to_string(site.Destination) + " mask " + std::to_string(site.Mask) + " index " + std::to_string(site.Index)
        + " base " + (site.Base ? std::to_string(*site.Base) : std::string("none")) + " scale " + std::to_string(1 << site.Scale)
        + " disp " + std::to_string(site.Displacement) + " masks " + std::to_string(maskPattern) + " seed " + std::to_string(seed);
    auto stub = fixture(site, table, seed, maskPattern, flags);
    const auto* top = reinterpret_cast<const std::uint8_t*>(stub.Stack.data() + stub.Stack.size() - 8);
    const auto wanted = expected(stub.Start, site, top);
    auto machine = stub.Start;
    Runner(lowered(bytes)).Run(machine, stub.Stack);
    compare(machine, wanted, describe + " (lowered)");
    if (!native) return;
    auto reference = fixture(site, table, seed, maskPattern, flags);
    auto hardware = reference.Start;
    Runner(bytes).Run(hardware, reference.Stack);
    compare(hardware, expected(reference.Start, site, reinterpret_cast<const std::uint8_t*>(reference.Stack.data() + reference.Stack.size() - 8)), describe + " (native)");
}

void execution() {
    std::array<std::uint64_t, 96> table{};
    for (std::size_t q = 0; q < table.size(); ++q) table[q] = mix(q + 0x5EED);
    const bool native = hostHasAvx2();
    if (!native) std::cout << "the host has no AVX2; gathers are checked against the SDM model only\n";
    const std::array<std::array<std::uint8_t, 4>, 5> registers{{{1, 2, 3, 3}, {9, 14, 7, 13}, {0, 15, 8, 0}, {12, 4, 11, 7}, {6, 10, 5, 5}}};
    std::uint64_t seed = 1;
    for (std::uint8_t opcode = 0x90; opcode <= 0x93; ++opcode)
        for (const bool wide : {false, true})
            for (const bool vector256 : {false, true})
                for (const auto& regs : registers)
                    for (std::uint8_t scale = 0; scale < 4; ++scale)
                        for (unsigned maskPattern = 0; maskPattern < 4; ++maskPattern) {
                            const std::int32_t displacement = (scale & 1) ? -8 : (scale == 2 ? 0x40 : 0);
                            const Site site{opcode, wide, vector256, regs[0], regs[1], regs[2], regs[3], scale, displacement, scale == 2, {}};
                            run(site, table, seed++, maskPattern, LoweringExecution::kBaseFlags | (maskPattern & 1 ? LoweringExecution::kFlagMask : 0), native);
                        }
    for (std::uint8_t opcode = 0x90; opcode <= 0x93; ++opcode)
        for (const bool wide : {false, true}) {
            run({opcode, wide, true, 2, 3, 1, 4, 0, -16, false, {}}, table, seed++, 3, LoweringExecution::kBaseFlags, native);
            run({opcode, wide, true, 7, 6, 9, 4, 3, 0x20, true, {0x3E}}, table, seed++, 0, LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, native);
            if (opcode & 1) run({opcode, wide, false, 13, 1, 0, std::nullopt, 0, 0x10, true, {}}, table, seed++, 2, LoweringExecution::kBaseFlags, native);
        }
}

#else

void execution() { std::cout << "gather execution checks require an x86-64 Linux, macOS or Windows build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "gather lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
