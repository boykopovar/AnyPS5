#include "LoweringExecution.hpp"
#include <codegen/x86/ShiftLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using LoweringExecution::Bytes;
using Codegen::ShiftOperation;

constexpr std::array kShifts{ShiftOperation::Left, ShiftOperation::LogicalRight, ShiftOperation::ArithmeticRight};
constexpr std::array kOperations{ShiftOperation::Left, ShiftOperation::LogicalRight, ShiftOperation::ArithmeticRight, ShiftOperation::RotateRight};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t pp(ShiftOperation operation) {
    switch (operation) {
    case ShiftOperation::Left: return 1;
    case ShiftOperation::ArithmeticRight: return 2;
    case ShiftOperation::LogicalRight: return 3;
    case ShiftOperation::RotateRight: return 3;
    }
    return 0;
}

Bytes shift(ShiftOperation operation, bool wide, std::uint8_t dst, std::uint8_t src, std::uint8_t count) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((src & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~count & 15) << 3) | pp(operation)), 0xF7,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))};
}

Bytes rotate(bool wide, std::uint8_t dst, std::uint8_t src, std::uint8_t immediate) {
    return {0xC4, static_cast<std::uint8_t>(0xE3 ^ ((dst & 8) << 4) ^ ((src & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | 0x78 | 3), 0xF0,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7)), immediate};
}

Bytes memorySite(ShiftOperation operation, bool wide, const Bytes& tail, std::uint8_t immediate) {
    const bool rorx = operation == ShiftOperation::RotateRight;
    Bytes site{0xC4, static_cast<std::uint8_t>(rorx ? 0xE3 : 0xE2), static_cast<std::uint8_t>((wide ? 0x80 : 0) | (rorx ? 0x78 : 0x70) | pp(operation)), static_cast<std::uint8_t>(rorx ? 0xF0 : 0xF7)};
    site.insert(site.end(), tail.begin(), tail.end());
    if (rorx) site.push_back(immediate);
    return site;
}

void decoding() {
    for (const auto operation : kShifts) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    for (std::uint8_t count = 0; count < 16; ++count) {
                        const auto bytes = shift(operation, wide, dst, src, count);
                        const auto operands = Codegen::DecodeShift(bytes.data(), bytes.size());
                        require(operands && operands->Operation == operation && operands->Wide == wide && operands->Destination == dst && operands->Source == src && operands->Count == count && !operands->Memory, "shift register decoding changed an operand");
                    }
                }
            }
        }
    }
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (std::uint8_t src = 0; src < 16; ++src) {
                for (const std::uint8_t immediate : std::array<std::uint8_t, 4>{0, 1, 63, 255}) {
                    const auto bytes = rotate(wide, dst, src, immediate);
                    const auto operands = Codegen::DecodeShift(bytes.data(), bytes.size());
                    require(operands && operands->Operation == ShiftOperation::RotateRight && operands->Wide == wide && operands->Destination == dst && operands->Source == src && operands->Count == immediate && !operands->Memory, "RORX register decoding changed an operand");
                }
            }
        }
    }
    const Bytes shiftMemory{0x64, 0x67, 0xC4, 0x02, 0xA9, 0xF7, 0x8C, 0xAC, 0x10, 0, 0, 0};
    auto operands = Codegen::DecodeShift(shiftMemory.data(), shiftMemory.size());
    require(operands && operands->Operation == ShiftOperation::Left && operands->Destination == 9 && operands->Count == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "shift memory extensions or prefixes were lost");
    const Bytes rotateMemory{0xC4, 0x03, 0xFB, 0xF0, 0x8C, 0xAC, 0x10, 0, 0, 0, 0x21};
    operands = Codegen::DecodeShift(rotateMemory.data(), rotateMemory.size());
    require(operands && operands->Operation == ShiftOperation::RotateRight && operands->Destination == 9 && operands->Count == 0x21 && operands->Memory && operands->Memory->InstructionLength == rotateMemory.size(), "RORX memory operand or immediate was lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF1}, Bytes{0xC4, 0xE1, 0xF1, 0xF7, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF7, 0xC0}, Bytes{0xC4, 0xE3, 0xFA, 0xF0, 0xC0, 0}, Bytes{0xC4, 0xE2, 0xFB, 0xF0, 0xC0, 0}})
        require(!Codegen::DecodeShift(bytes.data(), bytes.size()), "Unrelated, truncated, BEXTR or wrong-map opcode matched a shift");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF5, 0xF7, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF7, 0xC0, 0x90}, Bytes{0xC4, 0xE2, 0xF1, 0xF7, 0x84}, Bytes{0xC4, 0xE3, 0xF3, 0xF0, 0xC0, 0}, Bytes{0xC4, 0xE3, 0xFB, 0xF0, 0xC0}, Bytes{0xC4, 0xE3, 0xFB, 0xF0, 0xC0, 0, 0}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeShift(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed shift was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF1, 0xF7, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitShift(body, *Codegen::DecodeShift(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto operation : kOperations) {
        const auto bytes = operation == ShiftOperation::RotateRight ? rotate(true, 0, 6, 7) : shift(operation, true, 0, 6, 2);
        const auto name = Codegen::ShiftName(operation);
        require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode rewrote ") + name);
        const auto match = rosetta->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, std::string("Rosetta mode did not require a trampoline for ") + name);
        const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
        require(rosetta->MatchSequence(sequence, {}).has_value(), std::string("A sequence could not move out of line for ") + name);
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

Execution executable(const Bytes& site, std::uint64_t memoryValue) {
    const auto operands = Codegen::DecodeShift(site.data(), site.size());
    require(operands.has_value(), "Execution site was not a shift");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitShift(body, *Codegen::DecodeShift(relocated.data(), relocated.size()));
    }, operands->Operation == ShiftOperation::RotateRight ? 1 : 0);
}

std::uint64_t expected(const Codegen::ShiftOperands& operands, std::uint64_t source, std::uint64_t count) {
    const unsigned bits = operands.Wide ? 64 : 32;
    const auto mask = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
    source &= mask;
    const unsigned amount = static_cast<unsigned>(count & (bits - 1));
    switch (operands.Operation) {
    case ShiftOperation::Left: return (source << amount) & mask;
    case ShiftOperation::LogicalRight: return source >> amount;
    case ShiftOperation::ArithmeticRight: {
        const bool negative = (source >> (bits - 1)) != 0;
        const auto shifted = source >> amount;
        return negative && amount != 0 ? (shifted | (mask << (bits - amount))) & mask : shifted;
    }
    case ShiftOperation::RotateRight: return amount == 0 ? source : ((source >> amount) | (source << (bits - amount))) & mask;
    }
    return 0;
}

void check(const State& state, const Codegen::ShiftOperands& operands, std::uint64_t source, std::uint64_t count) {
    const auto name = Codegen::ShiftName(operands.Operation);
    const auto result = expected(operands, source, count);
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto wanted = reg == operands.Destination ? result : state.Input[reg];
        if (state.Output[reg] != wanted)
            throw std::runtime_error(std::string(name) + (operands.Wide ? " 64" : " 32") + " dst " + std::to_string(operands.Destination) + " src " + std::to_string(operands.Source) + " count " + std::to_string(operands.Count) + (operands.Memory ? " memory" : "") + ": register " + std::to_string(reg) + " is " + std::to_string(state.Output[reg]) + ", wanted " + std::to_string(wanted) + " (source " + std::to_string(source) + ", count value " + std::to_string(count) + ")");
    }
    require((state.Flags & LoweringExecution::kFlagMask) == (state.InputFlags & LoweringExecution::kFlagMask), std::string(name) + " did not preserve the flags");
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, std::string(name) + " overwrote the caller's red zone");
}

void run(const Execution& execution, const Codegen::ShiftOperands& operands, State state, std::uint64_t memoryValue) {
    for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
        state.InputFlags = flags;
        auto trial = state;
        execution.Run(trial);
        const auto source = operands.Memory ? memoryValue : trial.Input[operands.Source];
        const auto count = operands.Operation == ShiftOperation::RotateRight ? operands.Count : trial.Input[operands.Count];
        check(trial, operands, source, count);
    }
}

void execution() {
    constexpr std::array<std::uint64_t, 6> values{0, 1, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull};
    constexpr std::array<std::uint64_t, 7> counts{0, 1, 31, 32, 63, 64, 0xFF};
    for (const auto operation : kShifts) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const auto pair : std::array<std::pair<std::uint8_t, std::uint8_t>, 9>{{{6, 2}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}, {1, 6}, {6, 1}, {1, 1}}}) {
                    const auto site = shift(operation, wide, dst, pair.first, pair.second);
                    const auto operands = *Codegen::DecodeShift(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto value : values) {
                        for (const auto count : counts) {
                            auto state = LoweringExecution::InitialState();
                            state.Input[pair.first] = value;
                            state.Input[pair.second] = count;
                            run(execution, operands, state, 0);
                        }
                    }
                }
            }
        }
    }
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (const std::uint8_t src : std::array<std::uint8_t, 4>{6, dst, 4, 10}) {
                const bool every = (dst == 0 && src == 6) || dst == src;
                for (int value = 0; value < 256; ++value) {
                    const auto immediate = static_cast<std::uint8_t>(value);
                    if (!every && immediate > 1 && immediate != 31 && immediate != 32 && immediate != 63 && immediate != 255) continue;
                    const auto site = rotate(wide, dst, src, immediate);
                    const auto operands = *Codegen::DecodeShift(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto value : values) {
                        auto state = LoweringExecution::InitialState();
                        state.Input[src] = value;
                        run(execution, operands, state, 0);
                    }
                }
            }
        }
    }
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto value : std::array<std::uint64_t, 3>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull}) {
                    const auto site = memorySite(operation, wide, tail, 13);
                    const auto operands = *Codegen::DecodeShift(site.data(), site.size());
                    const auto execution = executable(site, value);
                    auto state = LoweringExecution::InitialState();
                    state.Input[6] = reinterpret_cast<std::uint64_t>(execution.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                    state.Input[2] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 3 * 8;
                    state.Input[1] = 3;
                    state.Input[5] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 16;
                    state.Output[0] = value;
                    run(execution, operands, state, value);
                }
            }
        }
    }
}
#else
void execution() { std::cout << "shift execution checks require a Linux/macOS x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "shift lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
