#include "LoweringExecution.hpp"
#include <codegen/x86/CountLowering.hpp>
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
using Codegen::CountOperation;

constexpr std::array kOperations{CountOperation::TrailingZeros, CountOperation::LeadingZeros};
constexpr std::array<std::uint8_t, 3> kWidths{16, 32, 64};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t opcode(CountOperation operation) {
    return operation == CountOperation::TrailingZeros ? 0xBC : 0xBD;
}

Bytes prefixes(std::uint8_t width, std::uint8_t rex) {
    Bytes bytes;
    if (width == 16) bytes.push_back(0x66);
    bytes.push_back(0xF3);
    rex = static_cast<std::uint8_t>(rex | (width == 64 ? 8 : 0));
    if (rex != 0) bytes.push_back(static_cast<std::uint8_t>(0x40 | rex));
    return bytes;
}

Bytes instruction(CountOperation operation, std::uint8_t width, std::uint8_t dst, std::uint8_t src) {
    auto bytes = prefixes(width, static_cast<std::uint8_t>(((dst & 8) >> 1) | ((src & 8) >> 3)));
    bytes.insert(bytes.end(), {0x0F, opcode(operation), static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))});
    return bytes;
}

Bytes memorySite(CountOperation operation, std::uint8_t width, const Bytes& tail) {
    auto bytes = prefixes(width, 0);
    bytes.insert(bytes.end(), {0x0F, opcode(operation)});
    bytes.insert(bytes.end(), tail.begin(), tail.end());
    return bytes;
}

void decoding() {
    for (const auto operation : kOperations) {
        for (const auto width : kWidths) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    const auto bytes = instruction(operation, width, dst, src);
                    const auto operands = Codegen::DecodeCount(bytes.data(), bytes.size());
                    require(operands && operands->Operation == operation && operands->Width == width && operands->Destination == dst && operands->Source == src && !operands->Memory, "count register decoding changed an operand");
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xF3, 0x4F, 0x0F, 0xBD, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeCount(memory.data(), memory.size());
    require(operands && operands->Operation == CountOperation::LeadingZeros && operands->Width == 64 && operands->Destination == 9 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "count memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0x0F, 0xBC, 0xC0}, Bytes{0x0F, 0xBD, 0xC0}, Bytes{0x48, 0x0F, 0xBC, 0xC0}, Bytes{0xF2, 0x0F, 0xBC, 0xC0}, Bytes{0xF3, 0x0F, 0xB8, 0xC0}, Bytes{0xF3, 0x0F}, Bytes{0xF3, 0x0F, 0xBE, 0xC0}})
        require(!Codegen::DecodeCount(bytes.data(), bytes.size()), "BSF, BSR, POPCNT, a truncated or an unrelated opcode matched a count instruction");
    for (const Bytes bytes : {Bytes{0xF2, 0xF3, 0x0F, 0xBC, 0xC0}, Bytes{0xF3, 0x0F, 0xBC, 0xC0, 0x90}, Bytes{0xF3, 0x0F, 0xBD, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeCount(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed count instruction was accepted");
    }
    const Bytes overflow{0xF3, 0x48, 0x0F, 0xBC, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitCount(body, *Codegen::DecodeCount(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto operation : kOperations) {
        const auto bytes = instruction(operation, 64, 0, 6);
        const auto name = Codegen::CountName(operation);
        require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode rewrote ") + name);
        require(!rosetta->Match(bytes.data(), bytes.size()), std::string("Rosetta mode rewrote ") + name);
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, std::string("AVX mode did not require a trampoline for ") + name);
        const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
        require(avx->MatchSequence(sequence, {}).has_value(), std::string("A sequence could not move out of line for ") + name);
    }
    const Bytes bsf{0x48, 0x0F, 0xBC, 0xC6};
    require(!avx->Match(bsf.data(), bsf.size()), "AVX mode rewrote BSF");
    const Bytes andn{0xC4, 0xE2, 0xF8, 0xF2, 0xC6};
    const auto inherited = avx->Match(andn.data(), andn.size());
    require(inherited && inherited->InstructionName == "ANDN", "AVX mode dropped the Rosetta lowerings");
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

Execution executable(const Bytes& site, std::uint64_t memoryValue) {
    const auto operands = Codegen::DecodeCount(site.data(), site.size());
    require(operands.has_value(), "Execution site was not a count instruction");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitCount(body, *Codegen::DecodeCount(relocated.data(), relocated.size()));
    });
}

std::uint64_t counted(const Codegen::CountOperands& operands, std::uint64_t source) {
    const unsigned bits = operands.Width;
    source &= bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
    if (source == 0) return bits;
    unsigned count = 0;
    if (operands.Operation == CountOperation::TrailingZeros) {
        while ((source >> count & 1) == 0) ++count;
    } else {
        while ((source >> (bits - 1 - count) & 1) == 0) ++count;
    }
    return count;
}

std::uint64_t written(std::uint8_t width, std::uint64_t previous, std::uint64_t result) {
    return width == 16 ? (previous & ~0xFFFFull) | result : result;
}

void check(const State& state, const Codegen::CountOperands& operands, std::uint64_t source) {
    const auto name = Codegen::CountName(operands.Operation);
    const auto result = counted(operands, source);
    const auto describe = [&] {
        return std::string(name) + " " + std::to_string(operands.Width) + " dst " + std::to_string(operands.Destination) + " src " + std::to_string(operands.Source) + (operands.Memory ? " memory" : "") + " (source " + std::to_string(source) + ")";
    };
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto value = reg == operands.Destination ? written(operands.Width, state.Input[reg], result) : state.Input[reg];
        if (state.Output[reg] != value)
            throw std::runtime_error(describe() + ": register " + std::to_string(reg) + " is " + std::to_string(state.Output[reg]) + ", wanted " + std::to_string(value));
    }
    const auto mask = LoweringExecution::kCarry | LoweringExecution::kZero;
    const auto flags = (result == operands.Width ? LoweringExecution::kCarry : 0u) | (result == 0 ? LoweringExecution::kZero : 0u);
    if ((state.Flags & mask) != flags)
        throw std::runtime_error(describe() + ": flags are " + std::to_string(state.Flags & mask) + ", wanted " + std::to_string(flags));
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, std::string(name) + " overwrote the caller's red zone");
}

void run(const Execution& execution, const Codegen::CountOperands& operands, State state, std::uint64_t memoryValue) {
    for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
        state.InputFlags = flags;
        auto trial = state;
        execution.Run(trial);
        check(trial, operands, operands.Memory ? memoryValue : trial.Input[operands.Source]);
    }
}

void execution() {
    constexpr std::array<std::uint64_t, 12> values{0, 1, 2, 0x8000, 0x10000, 0x80000000ull, 0x100000000ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull, 0xFFFF0000FFFF0000ull, 0x0000000000F00F00ull};
    for (const auto operation : kOperations) {
        for (const auto width : kWidths) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const auto src : std::array<std::uint8_t, 6>{6, dst, 11, 4, 0, 15}) {
                    if (dst == 4 && width != 64) continue;
                    const auto site = instruction(operation, width, dst, src);
                    const auto operands = *Codegen::DecodeCount(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto value : values) {
                        if (dst == 4 && value != 0x8000000000000000ull) continue;
                        auto state = LoweringExecution::InitialState();
                        state.Input[src] = value;
                        run(execution, operands, state, 0);
                    }
                }
            }
        }
    }
    for (const auto operation : kOperations) {
        for (const auto width : kWidths) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto value : std::array<std::uint64_t, 4>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull, 0x0000000000010000ull}) {
                    const auto site = memorySite(operation, width, tail);
                    const auto operands = *Codegen::DecodeCount(site.data(), site.size());
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

void execution() { std::cout << "count execution checks require a Linux/macOS x86-64 build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "count lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
