#include "LoweringExecution.hpp"
#include <codegen/x86/ParallelBitsLowering.hpp>
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
using Codegen::ParallelBitsOperation;

constexpr std::array kOperations{ParallelBitsOperation::Deposit, ParallelBitsOperation::Extract};
constexpr std::uint64_t kArithmeticFlags = 0x8D5u;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t prefix(ParallelBitsOperation operation) {
    return operation == ParallelBitsOperation::Deposit ? 3 : 2;
}

Bytes instruction(ParallelBitsOperation operation, bool wide, std::uint8_t dst, std::uint8_t src, std::uint8_t mask) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((mask & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~src & 15) << 3) | prefix(operation)), 0xF5,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (mask & 7))};
}

Bytes memorySite(ParallelBitsOperation operation, bool wide, const Bytes& tail) {
    Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0x80 : 0) | 0x40 | prefix(operation)), 0xF5};
    site.insert(site.end(), tail.begin(), tail.end());
    return site;
}

void decoding() {
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    for (std::uint8_t mask = 0; mask < 16; ++mask) {
                        const auto bytes = instruction(operation, wide, dst, src, mask);
                        const auto operands = Codegen::DecodeParallelBits(bytes.data(), bytes.size());
                        require(operands && operands->Operation == operation && operands->Wide == wide && operands->Destination == dst && operands->Source == src && operands->Mask == mask && !operands->Memory, "parallel bits register decoding changed an operand");
                    }
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xAB, 0xF5, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeParallelBits(memory.data(), memory.size());
    require(operands && operands->Operation == ParallelBitsOperation::Deposit && operands->Destination == 9 && operands->Source == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "parallel bits memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF3}, Bytes{0xC4, 0xE1, 0xF3, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF3, 0xF6, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF7, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF7, 0xC0}})
        require(!Codegen::DecodeParallelBits(bytes.data(), bytes.size()), "Unrelated, truncated, BZHI, MULX, SARX or BEXTR opcode matched a parallel bits instruction");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF7, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF5, 0xC0, 0x90}, Bytes{0xC4, 0xE2, 0xF3, 0xF5, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeParallelBits(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed parallel bits instruction was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF3, 0xF5, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitParallelBits(body, *Codegen::DecodeParallelBits(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto operation : kOperations) {
        const auto bytes = instruction(operation, true, 0, 6, 2);
        const auto name = Codegen::ParallelBitsName(operation);
        require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode rewrote ") + name);
        for (const auto* matcher : {rosetta.get(), avx.get()}) {
            const auto match = matcher->Match(bytes.data(), bytes.size());
            require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, std::string("Rosetta or AVX mode did not require a trampoline for ") + name);
            const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
            require(matcher->MatchSequence(sequence, {}).has_value(), std::string("A sequence could not move out of line for ") + name);
        }
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

Execution executable(const Bytes& site, std::uint64_t memoryValue) {
    const auto operands = Codegen::DecodeParallelBits(site.data(), site.size());
    require(operands.has_value(), "Execution site was not a parallel bits instruction");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitParallelBits(body, *Codegen::DecodeParallelBits(relocated.data(), relocated.size()));
    });
}

std::uint64_t expected(const Codegen::ParallelBitsOperands& operands, std::uint64_t source, std::uint64_t mask) {
    const unsigned bits = operands.Wide ? 64 : 32;
    std::uint64_t result = 0;
    unsigned k = 0;
    for (unsigned m = 0; m < bits; ++m) {
        if (((mask >> m) & 1) == 0) continue;
        if (operands.Operation == ParallelBitsOperation::Deposit)
            result |= ((source >> k) & 1) << m;
        else
            result |= ((source >> m) & 1) << k;
        ++k;
    }
    return result;
}

void check(const State& state, const Codegen::ParallelBitsOperands& operands, std::uint64_t source, std::uint64_t mask) {
    const auto name = Codegen::ParallelBitsName(operands.Operation);
    const auto wanted = expected(operands, source, mask);
    const auto describe = [&] {
        return std::string(name) + (operands.Wide ? " 64" : " 32") + " dst " + std::to_string(operands.Destination) + " src " + std::to_string(operands.Source) + " mask " + std::to_string(operands.Mask) + (operands.Memory ? " memory" : "") + " (source " + std::to_string(source) + ", mask value " + std::to_string(mask) + ")";
    };
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto value = reg == operands.Destination ? wanted : state.Input[reg];
        if (state.Output[reg] != value)
            throw std::runtime_error(describe() + ": register " + std::to_string(reg) + " is " + std::to_string(state.Output[reg]) + ", wanted " + std::to_string(value));
    }
    if ((state.Flags & kArithmeticFlags) != (state.InputFlags & kArithmeticFlags))
        throw std::runtime_error(describe() + ": flags are " + std::to_string(state.Flags & kArithmeticFlags) + ", wanted " + std::to_string(state.InputFlags & kArithmeticFlags));
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, std::string(name) + " overwrote the caller's red zone");
}

void run(const Execution& execution, const Codegen::ParallelBitsOperands& operands, State state, std::uint64_t memoryValue) {
    for (const auto flags : {LoweringExecution::kBaseFlags | kArithmeticFlags, LoweringExecution::kBaseFlags, LoweringExecution::kBaseFlags | 0x41u, LoweringExecution::kBaseFlags | 0x894u}) {
        state.InputFlags = flags;
        auto trial = state;
        execution.Run(trial);
        const auto bits = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
        const auto source = trial.Input[operands.Source] & bits;
        const auto mask = (operands.Memory ? memoryValue : trial.Input[operands.Mask]) & bits;
        check(trial, operands, source, mask);
    }
}

void execution() {
    constexpr std::array<std::uint64_t, 8> values{0, 1, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull, 0x5555555555555555ull, 0x0123456789ABCDEFull};
    constexpr std::array<std::uint64_t, 10> masks{0, 1, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFull, 0xF0F0F0F00F0F0F0Full, 0xAAAAAAAAAAAAAAAAull, 0x00FF00000000FF01ull, 0x8000000180000001ull};
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const auto pair : std::array<std::pair<std::uint8_t, std::uint8_t>, 9>{{{6, 2}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}, {1, 6}, {6, 1}, {1, 1}}}) {
                    const auto site = instruction(operation, wide, dst, pair.first, pair.second);
                    const auto operands = *Codegen::DecodeParallelBits(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto value : values) {
                        for (const auto mask : masks) {
                            auto state = LoweringExecution::InitialState();
                            state.Input[pair.first] = value;
                            if (pair.second != pair.first) state.Input[pair.second] = mask;
                            run(execution, operands, state, 0);
                        }
                    }
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
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            const auto site = instruction(operation, wide, 3, 6, 2);
            const auto operands = *Codegen::DecodeParallelBits(site.data(), site.size());
            const auto execution = executable(site, 0);
            for (int trial = 0; trial < 2000; ++trial) {
                auto state = LoweringExecution::InitialState();
                state.Input[6] = next();
                state.Input[2] = next() & next();
                run(execution, operands, state, 0);
            }
        }
    }
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto value : std::array<std::uint64_t, 3>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull}) {
                    for (const auto source : std::array<std::uint64_t, 3>{0, 0x0123456789ABCDEFull, 0xFFFFFFFFFFFFFFFFull}) {
                        const auto site = memorySite(operation, wide, tail);
                        const auto operands = *Codegen::DecodeParallelBits(site.data(), site.size());
                        const auto execution = executable(site, value);
                        auto state = LoweringExecution::InitialState();
                        state.Input[6] = reinterpret_cast<std::uint64_t>(execution.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                        state.Input[2] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 3 * 8;
                        state.Input[1] = 3;
                        state.Input[5] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 16;
                        state.Input[7] = source;
                        state.Output[0] = value;
                        run(execution, operands, state, value);
                    }
                }
            }
            for (const std::uint8_t dst : std::array<std::uint8_t, 2>{0, 4}) {
                for (const std::int32_t displacement : {-216, -200, -192, -184, -176, -168, -160, -152, -144, -136}) {
                    Bytes tail{static_cast<std::uint8_t>(0x84 | (dst << 3)), 0x24};
                    for (unsigned s = 0; s < 32; s += 8)
                        tail.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> s));
                    constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
                    const auto site = memorySite(operation, wide, tail);
                    const auto operands = *Codegen::DecodeParallelBits(site.data(), site.size());
                    const Execution execution(site, value, false, [&](Codegen::StubBodyBuilder& body, const Bytes&) {
                        for (int part = 0; part < 2; ++part) {
                            Bytes store{0xC7, 0x84, 0x24};
                            const auto slot = static_cast<std::uint32_t>(displacement + part * 4);
                            const auto word = static_cast<std::uint32_t>(value >> (part * 32));
                            for (unsigned s = 0; s < 32; s += 8) store.push_back(static_cast<std::uint8_t>(slot >> s));
                            for (unsigned s = 0; s < 32; s += 8) store.push_back(static_cast<std::uint8_t>(word >> s));
                            body.Raw(store);
                        }
                        Codegen::EmitParallelBits(body, operands);
                    });
                    auto state = LoweringExecution::InitialState();
                    state.Input[7] = 0x0123456789ABCDEFull;
                    run(execution, operands, state, value);
                }
            }
        }
    }
}
#else
void execution() { std::cout << "parallel bits execution checks require an x86-64 Linux, macOS or Windows build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "parallel bits lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
