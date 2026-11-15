#include "VectorExecution.hpp"
#include <codegen/x86/VariableShiftLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if LOWERING_EXECUTION_AVAILABLE
#include <unistd.h>
#endif

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes site(std::uint8_t opcode, bool quadword, bool wide256, std::uint8_t dst, std::uint8_t src, std::uint8_t count) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((count & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((quadword ? 0x80 : 0) | ((~src & 15) << 3) | (wide256 ? 4 : 0) | 1), opcode,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (count & 7))};
}

Bytes memorySite(std::uint8_t opcode, bool quadword, bool wide256, std::uint8_t dst, std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>((quadword ? 0x80 : 0) | ((~src & 15) << 3) | (wide256 ? 4 : 0) | 1), opcode, static_cast<std::uint8_t>((dst & 7) << 3)};
}

struct Variant {
    std::uint8_t Opcode;
    bool Quadword;
    const char* Name;
};

constexpr std::array<Variant, 5> kVariants{{{0x45, false, "VPSRLVD"}, {0x45, true, "VPSRLVQ"}, {0x46, false, "VPSRAVD"}, {0x47, false, "VPSLLVD"}, {0x47, true, "VPSLLVQ"}}};

void decoding() {
    for (const auto& variant : kVariants) {
        for (const bool wide : {false, true}) {
            const auto bytes = site(variant.Opcode, variant.Quadword, wide, 9, 3, 12);
            const auto operands = Codegen::DecodeVariableShift(bytes.data(), bytes.size());
            require(operands && std::string(operands->Name) == variant.Name && operands->Quadword == variant.Quadword && operands->Vector256 == wide, std::string(variant.Name) + " was not decoded");
            require(operands->Destination == 9 && operands->Source == 3 && operands->Count == 12 && !operands->Memory, std::string(variant.Name) + " operands were decoded wrong");
            const auto memory = memorySite(variant.Opcode, variant.Quadword, wide, 2, 7);
            const auto fromMemory = Codegen::DecodeVariableShift(memory.data(), memory.size());
            require(fromMemory && fromMemory->Memory && fromMemory->Destination == 2 && fromMemory->Source == 7, std::string(variant.Name) + " with a memory count was not decoded");
        }
    }
    for (const Bytes bytes : {site(0x46, true, true, 1, 2, 3), site(0x44, false, true, 1, 2, 3), site(0x48, false, true, 1, 2, 3), Bytes{0xC4, 0xE2, 0x7D, 0x40, 0xC1}, Bytes{0x90}})
        require(!Codegen::DecodeVariableShift(bytes.data(), bytes.size()), "a non-variable-shift instruction was taken as a variable shift");
    auto trailing = site(0x47, false, true, 0, 1, 2);
    trailing.push_back(0x90);
    bool rejected = false;
    try { (void)Codegen::DecodeVariableShift(trailing.data(), trailing.size()); }
    catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "trailing bytes were accepted");
}

void matching() {
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto& variant : kVariants) {
        for (const bool wide : {false, true}) {
            const auto bytes = site(variant.Opcode, variant.Quadword, wide, 0, 1, 2);
            require(!rosetta->Match(bytes.data(), bytes.size()), std::string("Rosetta mode lowered ") + variant.Name);
            const auto match = avx->Match(bytes.data(), bytes.size());
            require(match && match->InstructionName == variant.Name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, std::string("AVX mode did not lower ") + variant.Name + " through a movable stub");
        }
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;
using Vector = std::array<std::uint64_t, 4>;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitVariableShift(body, *Codegen::DecodeVariableShift(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

Vector dwords(const std::array<std::uint32_t, 8>& values) {
    Vector out{};
    std::memcpy(out.data(), values.data(), sizeof(values));
    return out;
}

const std::array<Vector, 4> kDwordCounts{
    dwords({0, 1, 7, 31, 32, 33, 0xFFFFFFFFu, 0x80000000u}),
    dwords({4, 31, 16, 3, 100, 0, 30, 8}),
    dwords({0xFFFFFF00u, 2, 0x20u, 0x1Fu, 12, 5, 0x7FFFFFFFu, 1}),
    dwords({15, 15, 15, 15, 28, 29, 30, 31}),
};

const std::array<Vector, 4> kQwordCounts{
    Vector{0, 1, 63, 64},
    Vector{32, 0xFFFFFFFFull, 0x100000000ull, 7},
    Vector{0x8000000000000000ull, 62, 13, 40},
    Vector{5, 48, 63, 1},
};

Vector shifted(const Variant& variant, const Vector& source, const Vector& counts, bool wide256) {
    Vector out{};
    const unsigned qwords = wide256 ? 4 : 2;
    if (variant.Quadword) {
        for (unsigned element = 0; element < qwords; ++element) {
            const auto count = counts[element];
            if (count > 63) out[element] = 0;
            else out[element] = variant.Opcode == 0x47 ? source[element] << count : source[element] >> count;
        }
        return out;
    }
    std::array<std::uint32_t, 8> in{};
    std::array<std::uint32_t, 8> by{};
    std::array<std::uint32_t, 8> result{};
    std::memcpy(in.data(), source.data(), sizeof(in));
    std::memcpy(by.data(), counts.data(), sizeof(by));
    for (unsigned element = 0; element < qwords * 2; ++element) {
        const auto count = by[element];
        if (variant.Opcode == 0x46) {
            const auto value = static_cast<std::int32_t>(in[element]);
            result[element] = static_cast<std::uint32_t>(count > 31 ? (value < 0 ? -1 : 0) : value >> count);
        } else if (count > 31) {
            result[element] = 0;
        } else {
            result[element] = variant.Opcode == 0x47 ? in[element] << count : in[element] >> count;
        }
    }
    std::memcpy(out.data(), result.data(), sizeof(result));
    return out;
}

void verify(const Machine& start, const Machine& end, std::uint8_t dst, const Vector& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    require(end.Memory == start.Memory, describe + " wrote the memory operand");
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

std::string describe(const Variant& variant, bool wide, const std::string& rest) {
    return std::string(variant.Name) + (wide ? " ymm " : " xmm ") + rest;
}

void execution() {
    const std::array<std::array<std::uint8_t, 3>, 7> triples{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 2, 15}, {12, 12, 9}}};
    for (const auto& variant : kVariants) {
        const auto& countSet = variant.Quadword ? kQwordCounts : kDwordCounts;
        for (const bool wide : {false, true}) {
            for (const auto& counts : countSet) {
                for (const auto triple : triples) {
                    auto machine = VectorExecution::Initial();
                    machine.Ymm[triple[2]] = counts;
                    const auto start = machine;
                    const auto wanted = shifted(variant, start.Ymm[triple[1]], start.Ymm[triple[2]], wide);
                    Runner(lowered(site(variant.Opcode, variant.Quadword, wide, triple[0], triple[1], triple[2]))).Run(machine);
                    verify(start, machine, triple[0], wanted, describe(variant, wide, std::to_string(triple[0]) + "," + std::to_string(triple[1]) + "," + std::to_string(triple[2])));
                }
                for (const std::uint8_t dst : {std::uint8_t{4}, std::uint8_t{11}}) {
                    auto machine = VectorExecution::Initial();
                    machine.Memory = counts;
                    const auto start = machine;
                    Runner(lowered(memorySite(variant.Opcode, variant.Quadword, wide, dst, 11))).Run(machine);
                    verify(start, machine, dst, shifted(variant, start.Ymm[11], counts, wide), describe(variant, wide, "from memory into " + std::to_string(dst)));
                }
            }
        }
    }
}

void pageEndMemory() {
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* area = static_cast<std::uint8_t*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(area != MAP_FAILED, "cannot allocate the guarded operand pages");
    require(mprotect(area + pageSize, pageSize, PROT_NONE) == 0, "cannot protect the page after the operand");
    for (const auto& variant : kVariants) {
        for (const bool wide : {false, true}) {
            const unsigned width = wide ? 32 : 16;
            const auto& counts = variant.Quadword ? kQwordCounts[0] : kDwordCounts[0];
            auto* operand = area + pageSize - width;
            std::memcpy(operand, counts.data(), width);
            Bytes body{0x48, 0xB8};
            const auto address = reinterpret_cast<std::uintptr_t>(operand);
            for (unsigned shift = 0; shift < 64; shift += 8) body.push_back(static_cast<std::uint8_t>(address >> shift));
            const auto stub = lowered(memorySite(variant.Opcode, variant.Quadword, wide, 3, 6));
            body.insert(body.end(), stub.begin(), stub.end());
            auto machine = VectorExecution::Initial();
            const auto start = machine;
            Runner(body).Run(machine);
            verify(start, machine, 3, shifted(variant, start.Ymm[6], counts, wide), describe(variant, wide, "at the end of a page"));
        }
    }
    munmap(area, pageSize * 2);
}

#else

void execution() { std::cout << "variable shift execution checks require a Linux/macOS x86-64 build\n"; }
void pageEndMemory() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        pageEndMemory();
        std::cout << "variable shift lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
