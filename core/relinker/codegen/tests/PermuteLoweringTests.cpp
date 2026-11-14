#include "VectorExecution.hpp"
#include <codegen/x86/PermuteLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes immediateSite(bool doubles, std::uint8_t dst, std::uint8_t src, std::uint8_t imm) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((src & 8) ? 0 : 0x20) | 3), 0xFD, static_cast<std::uint8_t>(doubles ? 0x01 : 0x00),
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7)), imm};
}

Bytes variableSite(bool floats, std::uint8_t dst, std::uint8_t index, std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((src & 8) ? 0 : 0x20) | 2), static_cast<std::uint8_t>(((~index & 15) << 3) | 5), static_cast<std::uint8_t>(floats ? 0x16 : 0x36),
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))};
}

void decoding() {
    const auto q = immediateSite(false, 9, 3, 0x1B);
    const auto qOperands = Codegen::DecodePermute(q.data(), q.size());
    require(qOperands && std::string(qOperands->Name) == "VPERMQ" && !qOperands->Variable && qOperands->Destination == 9 && qOperands->Source == 3 && qOperands->Immediate == 0x1B, "VPERMQ was not decoded");
    const auto pd = immediateSite(true, 1, 12, 0x4E);
    require(Codegen::DecodePermute(pd.data(), pd.size()) && std::string(Codegen::DecodePermute(pd.data(), pd.size())->Name) == "VPERMPD", "VPERMPD was not decoded");
    const auto d = variableSite(false, 2, 11, 7);
    const auto dOperands = Codegen::DecodePermute(d.data(), d.size());
    require(dOperands && std::string(dOperands->Name) == "VPERMD" && dOperands->Variable && dOperands->Destination == 2 && dOperands->Index == 11 && dOperands->Source == 7, "VPERMD was not decoded");
    const auto ps = variableSite(true, 14, 0, 5);
    require(Codegen::DecodePermute(ps.data(), ps.size()) && std::string(Codegen::DecodePermute(ps.data(), ps.size())->Name) == "VPERMPS", "VPERMPS was not decoded");
    const Bytes memory{0x65, 0xC4, 0x62, 0x75, 0x36, 0x44, 0x24, 0x20};
    const auto loaded = Codegen::DecodePermute(memory.data(), memory.size());
    require(loaded && loaded->Memory && loaded->Memory->Prefixes == Bytes{0x65} && loaded->Destination == 8 && loaded->Index == 1, "a memory VPERMD lost its prefix or operands");
    for (const Bytes bytes : {Bytes{0xC4, 0xE3, 0x7D, 0x00, 0xC1, 0x1B}, Bytes{0xC4, 0xE2, 0xF5, 0x36, 0xC2}, Bytes{0xC4, 0xE2, 0x71, 0x36, 0xC2}, Bytes{0xC4, 0xE3, 0xF9, 0x00, 0xC1, 0x1B}, Bytes{0xC4, 0xE3, 0x7D, 0x06, 0xC1, 0x01}, Bytes{0xC4, 0xE2, 0x75, 0x0C, 0xC2}, Bytes{0x90}})
        require(!Codegen::DecodePermute(bytes.data(), bytes.size()), "an instruction that is not a cross-lane permute was taken as one");
    for (const Bytes bytes : {Bytes{0xC4, 0xE3, 0xFD, 0x00, 0xC1}, Bytes{0xC4, 0xE2, 0x75, 0x36, 0xC2, 0x90}}) {
        bool rejected = false;
        try { (void)Codegen::DecodePermute(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "a malformed permute was accepted");
    }
}

void matching() {
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& bytes : {immediateSite(false, 0, 1, 0x1B), variableSite(false, 0, 1, 2)}) {
        require(!rosetta->Match(bytes.data(), bytes.size()), "Rosetta mode lowered a permute");
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, "AVX mode did not lower a permute through a movable stub");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitPermute(body, *Codegen::DecodePermute(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::array<std::uint64_t, 4> permuteQwords(const std::array<std::uint64_t, 4>& source, std::uint8_t imm) {
    std::array<std::uint64_t, 4> out{};
    for (unsigned i = 0; i < 4; ++i) out[i] = source[(imm >> (2 * i)) & 3];
    return out;
}

std::uint32_t dword(const std::array<std::uint64_t, 4>& value, unsigned index) {
    return static_cast<std::uint32_t>(value[index / 2] >> ((index % 2) * 32));
}

std::array<std::uint64_t, 4> permuteDwords(const std::array<std::uint64_t, 4>& source, const std::array<std::uint64_t, 4>& index) {
    std::array<std::uint64_t, 4> out{};
    for (unsigned i = 0; i < 8; ++i)
        out[i / 2] |= static_cast<std::uint64_t>(dword(source, dword(index, i) & 7)) << ((i % 2) * 32);
    return out;
}

void verify(const Machine& start, const Machine& end, std::uint8_t dst, const std::array<std::uint64_t, 4>& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

void execution() {
    for (const bool doubles : {false, true}) {
        for (const auto pair : std::array<std::array<std::uint8_t, 2>, 5>{{{0, 1}, {3, 3}, {15, 14}, {8, 2}, {6, 13}}}) {
            for (const int imm : {0x00, 0x1B, 0x4E, 0xE4, 0x93, 0xFF}) {
                const auto site = immediateSite(doubles, pair[0], pair[1], static_cast<std::uint8_t>(imm));
                const Runner runner(lowered(site));
                auto machine = VectorExecution::Initial();
                const auto start = machine;
                runner.Run(machine);
                verify(start, machine, pair[0], permuteQwords(start.Ymm[pair[1]], static_cast<std::uint8_t>(imm)), std::string(doubles ? "VPERMPD" : "VPERMQ") + " imm " + std::to_string(imm));
            }
        }
    }
    for (const bool floats : {false, true}) {
        for (const auto triple : std::array<std::array<std::uint8_t, 3>, 7>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {2, 9, 2}}}) {
            for (const std::uint64_t pattern : {0x0000000100000000ull, 0x0000000600000007ull, 0xFFFFFFF3FFFFFFFCull, 0x0000000500000002ull}) {
                auto machine = VectorExecution::Initial();
                machine.Ymm[triple[1]] = {pattern, pattern ^ 0x0000000500000003ull, pattern + 0x0000000200000001ull, ~pattern};
                const auto start = machine;
                const Runner runner(lowered(variableSite(floats, triple[0], triple[1], triple[2])));
                runner.Run(machine);
                verify(start, machine, triple[0], permuteDwords(start.Ymm[triple[2]], start.Ymm[triple[1]]), std::string(floats ? "VPERMPS" : "VPERMD") + " registers " + std::to_string(triple[0]) + "," + std::to_string(triple[1]) + "," + std::to_string(triple[2]));
            }
        }
    }
    const Bytes memoryQ{0xC4, 0xE3, 0xFD, 0x00, 0x08, 0x93};
    auto machine = VectorExecution::Initial();
    auto start = machine;
    Runner(lowered(memoryQ)).Run(machine);
    verify(start, machine, 1, permuteQwords(start.Memory, 0x93), "VPERMQ from memory");
    const Bytes memoryD{0xC4, 0xE2, 0x6D, 0x36, 0x18};
    machine = VectorExecution::Initial();
    machine.Ymm[2] = {0x0000000300000006ull, 0x0000000100000004ull, 0x0000000700000000ull, 0x0000000200000005ull};
    start = machine;
    Runner(lowered(memoryD)).Run(machine);
    verify(start, machine, 3, permuteDwords(start.Memory, start.Ymm[2]), "VPERMD from memory");
}

#else

void execution() { std::cout << "permute execution checks require a Linux/macOS x86-64 build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "permute lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
