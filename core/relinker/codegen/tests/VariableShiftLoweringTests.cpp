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

constexpr std::uint8_t kLeft = 0x47;
constexpr std::uint8_t kLogicalRight = 0x45;
constexpr std::uint8_t kArithmeticRight = 0x46;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes header(std::uint8_t opcode, bool wide, bool vector256, std::uint8_t dst, std::uint8_t src, bool countsExtended) {
    return {0xC4,
        static_cast<std::uint8_t>(((dst & 8) != 0 ? 0 : 0x80) | 0x40 | (countsExtended ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~src & 15) << 3) | (vector256 ? 4 : 0) | 1),
        opcode};
}

Bytes site(std::uint8_t opcode, bool wide, bool vector256, std::uint8_t dst, std::uint8_t src, std::uint8_t counts) {
    auto bytes = header(opcode, wide, vector256, dst, src, (counts & 8) != 0);
    bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (counts & 7)));
    return bytes;
}

Bytes memorySite(std::uint8_t opcode, bool wide, bool vector256, std::uint8_t dst, std::uint8_t src) {
    auto bytes = header(opcode, wide, vector256, dst, src, false);
    bytes.push_back(static_cast<std::uint8_t>(((dst & 7) << 3) | 0));
    return bytes;
}

void decoding() {
    struct Expected { std::uint8_t Opcode; bool Wide; const char* Name; VariableShiftOperation Operation; };
    for (const auto& expected : std::array<Expected, 5>{{
            {kLeft, false, "VPSLLVD", VariableShiftOperation::Left},
            {kLeft, true, "VPSLLVQ", VariableShiftOperation::Left},
            {kLogicalRight, false, "VPSRLVD", VariableShiftOperation::LogicalRight},
            {kLogicalRight, true, "VPSRLVQ", VariableShiftOperation::LogicalRight},
            {kArithmeticRight, false, "VPSRAVD", VariableShiftOperation::ArithmeticRight}}}) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = site(expected.Opcode, expected.Wide, vector256, 9, 3, 12);
            const auto operands = Codegen::DecodeVariableShift(bytes.data(), bytes.size());
            require(operands && std::string(operands->Name) == expected.Name, std::string(expected.Name) + " was not decoded");
            require(operands->Operation == expected.Operation && operands->Wide == expected.Wide && operands->Vector256 == vector256,
                std::string(expected.Name) + " decoded the wrong shape");
            require(operands->Destination == 9 && operands->Source == 3 && operands->Counts == 12 && !operands->Memory,
                std::string(expected.Name) + " decoded the wrong operands");
        }
    }
    const auto loaded = memorySite(kLeft, false, true, 8, 1);
    const auto loadedOperands = Codegen::DecodeVariableShift(loaded.data(), loaded.size());
    require(loadedOperands && loadedOperands->Memory && loadedOperands->Destination == 8 && loadedOperands->Source == 1,
        "a memory VPSLLVD lost its operands");
    const Bytes prefixed{0x65, 0xC4, 0xE2, 0x75, 0x47, 0x44, 0x24, 0x20};
    const auto prefixedOperands = Codegen::DecodeVariableShift(prefixed.data(), prefixed.size());
    require(prefixedOperands && prefixedOperands->Memory && prefixedOperands->Memory->Prefixes == Bytes{0x65},
        "a memory VPSLLVD lost its segment prefix");
    for (const bool vector256 : {false, true})
        require(!Codegen::DecodeVariableShift(site(kArithmeticRight, true, vector256, 1, 2, 3).data(), 5),
            "VPSRAVQ does not exist outside AVX-512 and must not be lowered");
    for (const Bytes bytes : {
            Bytes{0xC4, 0xE2, 0x6D, 0x44, 0xCB},
            Bytes{0xC4, 0xE2, 0x6D, 0x48, 0xCB},
            Bytes{0xC4, 0xE1, 0x6D, 0x47, 0xCB},
            Bytes{0xC4, 0xE3, 0x6D, 0x47, 0xCB},
            Bytes{0xC4, 0xE2, 0x6C, 0x47, 0xCB},
            Bytes{0xC5, 0x6D, 0x47, 0xCB},
            Bytes{0x90}})
        require(!Codegen::DecodeVariableShift(bytes.data(), bytes.size()), "an instruction that is not a variable shift was taken as one");
    const Bytes trailing{0xC4, 0xE2, 0x6D, 0x47, 0xCB, 0x90};
    bool rejected = false;
    try { (void)Codegen::DecodeVariableShift(trailing.data(), trailing.size()); }
    catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "a variable shift with trailing bytes was accepted");
}

void matching() {
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const std::uint8_t opcode : {kLeft, kLogicalRight, kArithmeticRight}) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = site(opcode, false, vector256, 0, 1, 2);
            require(!rosetta->Match(bytes.data(), bytes.size()), "Rosetta mode lowered a variable shift");
            const auto match = avx->Match(bytes.data(), bytes.size());
            require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow,
                "AVX mode did not lower a variable shift through a movable stub");
            require(match->InstructionName == Codegen::VariableShiftName(
                    opcode == kLeft ? VariableShiftOperation::Left : opcode == kLogicalRight ? VariableShiftOperation::LogicalRight : VariableShiftOperation::ArithmeticRight,
                    false),
                "a lowered variable shift was reported under the wrong name");
        }
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitVariableShift(body, *Codegen::DecodeVariableShift(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::uint64_t element(const std::array<std::uint64_t, 4>& value, bool wide, unsigned index) {
    if (wide) return value[index];
    return static_cast<std::uint32_t>(value[index / 2] >> ((index % 2) * 32));
}

void place(std::array<std::uint64_t, 4>& value, bool wide, unsigned index, std::uint64_t result) {
    if (wide) { value[index] = result; return; }
    value[index / 2] |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(result)) << ((index % 2) * 32);
}

std::uint64_t shifted(VariableShiftOperation operation, bool wide, std::uint64_t value, std::uint64_t count) {
    const std::uint64_t width = wide ? 64 : 32;
    if (operation == VariableShiftOperation::ArithmeticRight) {
        const auto signed32 = static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
        if (count >= width) return static_cast<std::uint64_t>(static_cast<std::int64_t>(signed32 < 0 ? -1 : 0));
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(signed32 >> count));
    }
    if (count >= width) return 0;
    if (operation == VariableShiftOperation::Left) return value << count;
    return value >> count;
}

std::array<std::uint64_t, 4> reference(VariableShiftOperation operation, bool wide, bool vector256,
        const std::array<std::uint64_t, 4>& source, const std::array<std::uint64_t, 4>& counts) {
    std::array<std::uint64_t, 4> out{};
    const unsigned elements = (vector256 ? 32u : 16u) / (wide ? 8u : 4u);
    for (unsigned index = 0; index < elements; ++index)
        place(out, wide, index, shifted(operation, wide, element(source, wide, index), element(counts, wide, index)));
    return out;
}

void verify(const Machine& start, const Machine& end, std::uint8_t dst, const std::array<std::uint64_t, 4>& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
    require(end.Memory == start.Memory, describe + " changed the memory operand");
}

const std::array<std::array<std::uint64_t, 4>, 6>& countPatterns() {
    static const std::array<std::array<std::uint64_t, 4>, 6> patterns{{
        {0, 0, 0, 0},
        {0x0000001F00000001ull, 0x0000002000000000ull, 0x000000FF0000001Eull, 0x0000000700000010ull},
        {0x00000020FFFFFFFFull, 0x8000000000000020ull, 0x0000004000000040ull, 0x0000003F0000003Full},
        {0xFFFFFFFFFFFFFFFFull, 0x0000000000000000ull, 0x7FFFFFFF80000000ull, 0x0000001000000020ull},
        {0x0000000100000002ull, 0x0000000300000004ull, 0x0000000500000006ull, 0x0000000700000008ull},
        {0x000000000000003Full, 0x0000000000000040ull, 0x0000000000000041ull, 0xFFFFFFFF00000000ull}}};
    return patterns;
}

void execution() {
    struct Form { std::uint8_t Opcode; VariableShiftOperation Operation; };
    for (const auto& form : std::array<Form, 3>{{{kLeft, VariableShiftOperation::Left},
            {kLogicalRight, VariableShiftOperation::LogicalRight},
            {kArithmeticRight, VariableShiftOperation::ArithmeticRight}}}) {
        for (const bool wide : {false, true}) {
            if (wide && form.Operation == VariableShiftOperation::ArithmeticRight) continue;
            for (const bool vector256 : {false, true}) {
                for (const auto triple : std::array<std::array<std::uint8_t, 3>, 7>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {2, 9, 2}}}) {
                    for (const auto& counts : countPatterns()) {
                        auto machine = VectorExecution::Initial();
                        machine.Ymm[triple[2]] = counts;
                        const auto start = machine;
                        const auto bytes = site(form.Opcode, wide, vector256, triple[0], triple[1], triple[2]);
                        Runner(lowered(bytes)).Run(machine);
                        const auto wanted = reference(form.Operation, wide, vector256, start.Ymm[triple[1]], start.Ymm[triple[2]]);
                        verify(start, machine, triple[0], wanted,
                            std::string(Codegen::VariableShiftName(form.Operation, wide)) + (vector256 ? " ymm " : " xmm ")
                                + std::to_string(triple[0]) + "," + std::to_string(triple[1]) + "," + std::to_string(triple[2]));
                    }
                }
                auto machine = VectorExecution::Initial();
                const auto start = machine;
                const auto bytes = memorySite(form.Opcode, wide, vector256, 3, 2);
                Runner(lowered(bytes)).Run(machine);
                verify(start, machine, 3, reference(form.Operation, wide, vector256, start.Ymm[2], start.Memory),
                    std::string(Codegen::VariableShiftName(form.Operation, wide)) + " from memory");
            }
        }
    }
}

bool hostSaturatesWideCounts() {
    static const bool saturates = [] {
        const auto bytes = site(kLeft, true, false, 1, 2, 3);
        auto machine = VectorExecution::Initial();
        machine.Ymm[3] = {0x0000000100000001ull, 0x0000000100000001ull, 0, 0};
        const auto start = machine;
        Runner(bytes).Run(machine);
        return machine.Ymm[1] == reference(VariableShiftOperation::Left, true, false, start.Ymm[2], start.Ymm[3]);
    }();
    return saturates;
}

bool comparableCount(bool wide, const std::array<std::uint64_t, 4>& counts) {
    if (!wide || hostSaturatesWideCounts()) return true;
    for (const auto count : counts)
        if ((count >> 32) != 0 && (count & 0xFFFFFFFFull) < 64) return false;
    return true;
}

void oracle() {
    const auto probe = site(kLeft, false, true, 1, 2, 3);
    if (!LoweringExecution::HostExecutes(probe)) {
        std::cout << "the host does not execute AVX2 variable shifts, skipping the hardware oracle\n";
        return;
    }
    if (!hostSaturatesWideCounts())
        std::cout << "the host disagrees with the SDM on 64-bit counts above 2^32, leaving those out of the comparison\n";
    for (const std::uint8_t opcode : {kLeft, kLogicalRight, kArithmeticRight}) {
        for (const bool wide : {false, true}) {
            if (wide && opcode == kArithmeticRight) continue;
            for (const bool vector256 : {false, true}) {
                for (const auto& counts : countPatterns()) {
                    if (!comparableCount(wide, counts)) continue;
                    const auto bytes = site(opcode, wide, vector256, 1, 2, 3);
                    auto native = VectorExecution::Initial();
                    native.Ymm[3] = counts;
                    auto stubbed = native;
                    Runner(bytes).Run(native);
                    Runner(lowered(bytes)).Run(stubbed);
                    for (std::uint8_t reg = 0; reg < 16; ++reg)
                        require(native.Ymm[reg] == stubbed.Ymm[reg],
                            "the lowered " + std::string(opcode == kLeft ? "VPSLLV" : opcode == kLogicalRight ? "VPSRLV" : "VPSRAV")
                                + " disagrees with the hardware in ymm" + std::to_string(reg));
                }
            }
        }
    }
}

void preservation() {
    const auto bytes = site(kLeft, false, true, 1, 2, 3);
    const LoweringExecution::Execution execution(bytes, 0, false, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitVariableShift(body, *Codegen::DecodeVariableShift(relocated.data(), relocated.size()));
    });
    for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
        auto state = LoweringExecution::InitialState();
        state.InputFlags = flags;
        execution.Run(state);
        require((state.Flags & LoweringExecution::kFlagMask) == (flags & LoweringExecution::kFlagMask),
            "the variable shift stub did not preserve the flags");
        for (std::size_t reg = 0; reg < state.Output.size(); ++reg)
            require(state.Output[reg] == state.Input[reg],
                "the variable shift stub clobbered r" + std::to_string(reg));
        for (const auto value : state.RedZone)
            require(value == LoweringExecution::kSentinel, "the variable shift stub overwrote the caller's red zone");
    }
}

#else

void execution() { std::cout << "variable shift execution checks require a Linux/macOS x86-64 build\n"; }
void oracle() {}
void preservation() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        oracle();
        preservation();
    } catch (const std::exception& error) {
        std::cout << error.what() << "\n";
        return 1;
    }
    std::cout << "Variable shift lowering checks passed\n";
    return 0;
}
