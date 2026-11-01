#include "LoweringExecution.hpp"
#include <codegen/x86/BitfieldLowering.hpp>
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
using Codegen::BitfieldOperation;

constexpr std::array kOperations{BitfieldOperation::ZeroHigh, BitfieldOperation::Extract};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t opcode(BitfieldOperation operation) {
    return operation == BitfieldOperation::ZeroHigh ? 0xF5 : 0xF7;
}

Bytes instruction(BitfieldOperation operation, bool wide, std::uint8_t dst, std::uint8_t src, std::uint8_t control) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((src & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~control & 15) << 3)), opcode(operation),
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))};
}

Bytes memorySite(BitfieldOperation operation, bool wide, const Bytes& tail) {
    Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0x80 : 0) | 0x40), opcode(operation)};
    site.insert(site.end(), tail.begin(), tail.end());
    return site;
}

void decoding() {
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    for (std::uint8_t control = 0; control < 16; ++control) {
                        const auto bytes = instruction(operation, wide, dst, src, control);
                        const auto operands = Codegen::DecodeBitfield(bytes.data(), bytes.size());
                        require(operands && operands->Operation == operation && operands->Wide == wide && operands->Destination == dst && operands->Source == src && operands->Control == control && !operands->Memory, "bitfield register decoding changed an operand");
                    }
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xA8, 0xF5, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeBitfield(memory.data(), memory.size());
    require(operands && operands->Operation == BitfieldOperation::ZeroHigh && operands->Destination == 9 && operands->Control == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "bitfield memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF0}, Bytes{0xC4, 0xE1, 0xF0, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF7, 0xC0}, Bytes{0xC4, 0xE2, 0xF3, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF3, 0xF6, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0xC8}})
        require(!Codegen::DecodeBitfield(bytes.data(), bytes.size()), "Unrelated, truncated, SHLX, PDEP, PEXT, MULX or BLSR opcode matched a bitfield instruction");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF4, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF7, 0xC0, 0x90}, Bytes{0xC4, 0xE2, 0xF0, 0xF5, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeBitfield(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed bitfield instruction was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF0, 0xF7, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitBitfield(body, *Codegen::DecodeBitfield(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto operation : kOperations) {
        const auto bytes = instruction(operation, true, 0, 6, 2);
        const auto name = Codegen::BitfieldName(operation);
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
    const auto operands = Codegen::DecodeBitfield(site.data(), site.size());
    require(operands.has_value(), "Execution site was not a bitfield instruction");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitBitfield(body, *Codegen::DecodeBitfield(relocated.data(), relocated.size()));
    });
}

struct Expected {
    std::uint64_t Result;
    std::uint64_t Flags;
    std::uint64_t FlagMask;
};

Expected expected(const Codegen::BitfieldOperands& operands, std::uint64_t source, std::uint64_t control) {
    const unsigned bits = operands.Wide ? 64 : 32;
    const auto mask = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
    source &= mask;
    const auto lowBits = [&](unsigned count) { return count >= bits ? mask : (std::uint64_t{1} << count) - 1; };
    if (operands.Operation == BitfieldOperation::ZeroHigh) {
        const auto index = static_cast<unsigned>(control & 0xFF);
        const auto result = index >= bits ? source : source & lowBits(index);
        const auto flags = (result == 0 ? LoweringExecution::kZero : 0u) | (index >= bits ? LoweringExecution::kCarry : 0u) | ((result >> (bits - 1)) != 0 ? LoweringExecution::kSign : 0u);
        return {result, flags, LoweringExecution::kFlagMask};
    }
    const auto start = static_cast<unsigned>(control & 0xFF);
    const auto length = static_cast<unsigned>((control >> 8) & 0xFF);
    const auto result = (start >= bits ? 0 : source >> start) & lowBits(length);
    return {result, result == 0 ? LoweringExecution::kZero : 0u, LoweringExecution::kFlagMask & ~LoweringExecution::kSign};
}

void check(const State& state, const Codegen::BitfieldOperands& operands, std::uint64_t source, std::uint64_t control) {
    const auto name = Codegen::BitfieldName(operands.Operation);
    const auto wanted = expected(operands, source, control);
    const auto describe = [&] {
        return std::string(name) + (operands.Wide ? " 64" : " 32") + " dst " + std::to_string(operands.Destination) + " src " + std::to_string(operands.Source) + " control " + std::to_string(operands.Control) + (operands.Memory ? " memory" : "") + " (source " + std::to_string(source) + ", control value " + std::to_string(control) + ")";
    };
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto value = reg == operands.Destination ? wanted.Result : state.Input[reg];
        if (state.Output[reg] != value)
            throw std::runtime_error(describe() + ": register " + std::to_string(reg) + " is " + std::to_string(state.Output[reg]) + ", wanted " + std::to_string(value));
    }
    if ((state.Flags & wanted.FlagMask) != wanted.Flags)
        throw std::runtime_error(describe() + ": flags are " + std::to_string(state.Flags & wanted.FlagMask) + ", wanted " + std::to_string(wanted.Flags));
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, std::string(name) + " overwrote the caller's red zone");
}

void run(const Execution& execution, const Codegen::BitfieldOperands& operands, State state, std::uint64_t memoryValue) {
    for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
        state.InputFlags = flags;
        auto trial = state;
        execution.Run(trial);
        const auto source = operands.Memory ? memoryValue : trial.Input[operands.Source];
        check(trial, operands, source, trial.Input[operands.Control]);
    }
}

void execution() {
    constexpr std::array<std::uint64_t, 6> values{0, 1, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull};
    constexpr std::array<std::uint64_t, 20> controls{0, 1, 31, 32, 63, 64, 0xFF, 0x100, 0x801, 0x1F05, 0x2000, 0x201F, 0x3F3F, 0x4000, 0x4020, 0xFF00, 0xFFFF, 0x0F10, 0x12345678, 0xFFFFFFFFFFFF0405ull};
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const auto pair : std::array<std::pair<std::uint8_t, std::uint8_t>, 9>{{{6, 2}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}, {1, 6}, {6, 1}, {1, 1}}}) {
                    const auto site = instruction(operation, wide, dst, pair.first, pair.second);
                    const auto operands = *Codegen::DecodeBitfield(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto value : values) {
                        for (const auto control : controls) {
                            auto state = LoweringExecution::InitialState();
                            state.Input[pair.first] = value;
                            state.Input[pair.second] = control;
                            run(execution, operands, state, 0);
                        }
                    }
                }
            }
        }
    }
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto value : std::array<std::uint64_t, 3>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull}) {
                    for (const auto control : std::array<std::uint64_t, 4>{0, 0x0C04, 0x3F, 0x40}) {
                        const auto site = memorySite(operation, wide, tail);
                        const auto operands = *Codegen::DecodeBitfield(site.data(), site.size());
                        const auto execution = executable(site, value);
                        auto state = LoweringExecution::InitialState();
                        state.Input[6] = reinterpret_cast<std::uint64_t>(execution.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                        state.Input[2] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 3 * 8;
                        state.Input[1] = 3;
                        state.Input[5] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 16;
                        state.Input[7] = control;
                        state.Output[0] = value;
                        run(execution, operands, state, value);
                    }
                }
            }
        }
    }
}
#else
void execution() { std::cout << "bitfield execution checks require a Linux/macOS x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "bitfield lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
