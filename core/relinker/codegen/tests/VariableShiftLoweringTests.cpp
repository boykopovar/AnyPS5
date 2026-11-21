#include "VectorExecution.hpp"
#include <codegen/x86/VariableShiftLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;
using Codegen::VariableShiftOperation;

struct Kind {
    const char* Name;
    std::uint8_t Opcode;
    bool Quadwords;
    VariableShiftOperation Operation;
};

constexpr std::array<Kind, 5> kKinds{{
    {"VPSLLVD", 0x47, false, VariableShiftOperation::LeftLogical},
    {"VPSLLVQ", 0x47, true, VariableShiftOperation::LeftLogical},
    {"VPSRLVD", 0x45, false, VariableShiftOperation::RightLogical},
    {"VPSRLVQ", 0x45, true, VariableShiftOperation::RightLogical},
    {"VPSRAVD", 0x46, false, VariableShiftOperation::RightArithmetic},
}};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes site(const Kind& kind, bool vector256, std::uint8_t dst, std::uint8_t src, std::uint8_t count) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((count & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((kind.Quadwords ? 0x80 : 0) | ((~src & 15) << 3) | (vector256 ? 4 : 0) | 1), kind.Opcode,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (count & 7))};
}

Bytes memorySite(const Kind& kind, bool vector256, std::uint8_t dst, std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>((kind.Quadwords ? 0x80 : 0) | ((~src & 15) << 3) | (vector256 ? 4 : 0) | 1), kind.Opcode,
        static_cast<std::uint8_t>((dst & 7) << 3)};
}

void decoding() {
    for (const auto& kind : kKinds) {
        for (const bool vector256 : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    for (std::uint8_t count = 0; count < 16; ++count) {
                        const auto bytes = site(kind, vector256, dst, src, count);
                        const auto operands = Codegen::DecodeVariableShift(bytes.data(), bytes.size());
                        require(operands && std::string(operands->Name) == kind.Name && operands->Operation == kind.Operation && operands->Quadwords == kind.Quadwords && operands->Vector256 == vector256 && operands->Destination == dst && operands->Source == src && operands->Count == count && !operands->Memory, std::string(kind.Name) + " register decoding changed an operand");
                    }
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0x2D, 0x47, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeVariableShift(memory.data(), memory.size());
    require(operands && std::string(operands->Name) == "VPSLLVD" && operands->Vector256 && operands->Destination == 9 && operands->Source == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "variable shift memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF5, 0x46, 0xC2}, Bytes{0xC4, 0xE2, 0x74, 0x47, 0xC2}, Bytes{0xC4, 0xE2, 0x76, 0x45, 0xC2}, Bytes{0xC4, 0xE2, 0x75, 0x40, 0xC2}, Bytes{0xC4, 0xE2, 0x75, 0x36, 0xC2}, Bytes{0xC4, 0xE1, 0x75, 0x47, 0xC2}, Bytes{0xC4, 0xE2, 0x75}})
        require(!Codegen::DecodeVariableShift(bytes.data(), bytes.size()), "an instruction that is not a variable shift was taken as one");
    bool rejected = false;
    try {
        const Bytes trailing{0xC4, 0xE2, 0x75, 0x47, 0xC2, 0x90};
        (void)Codegen::DecodeVariableShift(trailing.data(), trailing.size());
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "a variable shift with trailing bytes was accepted");
    rejected = false;
    try {
        const Bytes overflow{0xC4, 0xE2, 0x75, 0x47, 0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
        Codegen::StubBodyBuilder body;
        Codegen::EmitVariableShift(body, *Codegen::DecodeVariableShift(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "an overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& kind : kKinds) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = site(kind, vector256, 0, 1, 2);
            require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode lowered ") + kind.Name);
            require(!rosetta->Match(bytes.data(), bytes.size()), std::string("Rosetta mode lowered ") + kind.Name);
            const auto match = avx->Match(bytes.data(), bytes.size());
            require(match && match->InstructionName == kind.Name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, std::string("AVX mode did not lower ") + kind.Name + " through a movable stub");
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

Vector expected(const Kind& kind, bool vector256, const Vector& source, const Vector& count) {
    Vector out{};
    const unsigned qwords = vector256 ? 4 : 2;
    for (unsigned q = 0; q < qwords; ++q) {
        if (kind.Quadwords) {
            const auto shift = count[q];
            out[q] = shift > 63 ? 0 : kind.Operation == VariableShiftOperation::LeftLogical ? source[q] << shift : source[q] >> shift;
            continue;
        }
        for (unsigned half = 0; half < 2; ++half) {
            const auto value = static_cast<std::uint32_t>(source[q] >> (32 * half));
            const auto shift = static_cast<std::uint32_t>(count[q] >> (32 * half));
            std::uint32_t result;
            if (kind.Operation == VariableShiftOperation::RightArithmetic)
                result = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> (shift > 31 ? 31 : shift));
            else if (shift > 31)
                result = 0;
            else
                result = kind.Operation == VariableShiftOperation::LeftLogical ? value << shift : value >> shift;
            out[q] |= static_cast<std::uint64_t>(result) << (32 * half);
        }
    }
    return out;
}

void verify(const Machine& start, const Machine& end, std::uint8_t dst, const Vector& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto value = reg == dst ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != value) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    require(end.Memory == start.Memory, describe + " changed its memory operand");
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

std::string describe(const Kind& kind, bool vector256, const std::string& operands) {
    return std::string(kind.Name) + (vector256 ? " ymm " : " xmm ") + operands;
}

void execution() {
    const std::array<Vector, 6> counts{{
        {0x0000000100000000ull, 0x0000001F00000002ull, 0x0000002000000021ull, 0xFFFFFFFF0000003Full},
        {0, 1, 63, 64},
        {0x8000000000000000ull, 0x0000000100000000ull, 31, 0x0000000700000010ull},
        {0x0000000300000005ull, 0x0000000B0000001Dull, 0x0000000400000008ull, 0x0000001100000001ull},
        {65, 0xFFFFFFFFFFFFFFFFull, 0x7FFFFFFF00000000ull, 32},
        {0x0000001E0000001Full, 0x0000000C00000018ull, 0x0000002300000000ull, 0x0000000100000040ull},
    }};
    for (const auto& kind : kKinds) {
        for (const bool vector256 : {false, true}) {
            for (const auto triple : std::array<std::array<std::uint8_t, 3>, 9>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {2, 9, 2}, {11, 0, 0}, {12, 12, 10}}}) {
                const Runner runner(lowered(site(kind, vector256, triple[0], triple[1], triple[2])));
                for (const auto& count : counts) {
                    auto machine = VectorExecution::Initial();
                    machine.Ymm[triple[2]] = count;
                    const auto start = machine;
                    runner.Run(machine);
                    verify(start, machine, triple[0], expected(kind, vector256, start.Ymm[triple[1]], start.Ymm[triple[2]]),
                        describe(kind, vector256, std::to_string(triple[0]) + "," + std::to_string(triple[1]) + "," + std::to_string(triple[2])));
                }
            }
            for (const auto pair : std::array<std::array<std::uint8_t, 2>, 4>{{{1, 2}, {3, 3}, {15, 0}, {8, 14}}}) {
                const Runner runner(lowered(memorySite(kind, vector256, pair[0], pair[1])));
                for (const auto& count : counts) {
                    auto machine = VectorExecution::Initial();
                    machine.Memory = count;
                    const auto start = machine;
                    runner.Run(machine);
                    verify(start, machine, pair[0], expected(kind, vector256, start.Ymm[pair[1]], start.Memory),
                        describe(kind, vector256, std::to_string(pair[0]) + "," + std::to_string(pair[1]) + ",memory"));
                }
            }
        }
    }
    std::uint64_t random = 0x9E3779B97F4A7C15ull;
    const auto next = [&random] {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        return random;
    };
    for (const auto& kind : kKinds) {
        for (const bool vector256 : {false, true}) {
            const Runner runner(lowered(site(kind, vector256, 4, 6, 9)));
            for (int trial = 0; trial < 500; ++trial) {
                auto machine = VectorExecution::Initial();
                for (auto& q : machine.Ymm[6]) q = next();
                for (auto& q : machine.Ymm[9]) q = next() & (trial % 2 == 0 ? 0x0000003F0000003Full : 0x0000007F0000007Full);
                const auto start = machine;
                runner.Run(machine);
                verify(start, machine, 4, expected(kind, vector256, start.Ymm[6], start.Ymm[9]), describe(kind, vector256, "random"));
            }
        }
    }
}

#else

void execution() { std::cout << "variable shift execution checks require an x86-64 Linux, macOS or Windows build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "variable shift lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
