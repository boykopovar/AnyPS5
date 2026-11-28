#include "LoweringExecution.hpp"
#include <codegen/x86/DepositExtractLowering.hpp>
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
using Codegen::DepositExtractOperation;

constexpr std::array kOperations{DepositExtractOperation::Deposit, DepositExtractOperation::Extract};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t prefix(DepositExtractOperation operation) {
    return operation == DepositExtractOperation::Deposit ? 3 : 2;
}

Bytes instruction(DepositExtractOperation operation, bool wide, std::uint8_t dst, std::uint8_t source, std::uint8_t mask) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((mask & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~source & 15) << 3) | prefix(operation)), 0xF5,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (mask & 7))};
}

Bytes memorySite(DepositExtractOperation operation, bool wide, const Bytes& tail) {
    Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0x80 : 0) | 0x40 | prefix(operation)), 0xF5};
    site.insert(site.end(), tail.begin(), tail.end());
    return site;
}

void decoding() {
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t source = 0; source < 16; ++source) {
                    for (std::uint8_t mask = 0; mask < 16; ++mask) {
                        const auto bytes = instruction(operation, wide, dst, source, mask);
                        const auto operands = Codegen::DecodeDepositExtract(bytes.data(), bytes.size());
                        require(operands && operands->Operation == operation && operands->Wide == wide && operands->Destination == dst && operands->Source == source && operands->Mask == mask && !operands->Memory, "deposit or extract register decoding changed an operand");
                    }
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xAB, 0xF5, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeDepositExtract(memory.data(), memory.size());
    require(operands && operands->Operation == DepositExtractOperation::Deposit && operands->Destination == 9 && operands->Source == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "deposit memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF3}, Bytes{0xC4, 0xE1, 0xF3, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF3, 0xF6, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF7, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0xC8}})
        require(!Codegen::DecodeDepositExtract(bytes.data(), bytes.size()), "Unrelated, truncated, wrong-map, BZHI, MULX, SARX or BLSR opcode matched a deposit or extract instruction");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF7, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF6, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF5, 0xC0, 0x90}, Bytes{0xC4, 0xE2, 0xF3, 0xF5, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeDepositExtract(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed deposit or extract instruction was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF2, 0xF5, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitDepositExtract(body, *Codegen::DecodeDepositExtract(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    for (const auto target : {Codegen::Amd64OnlyTarget::Rosetta, Codegen::Amd64OnlyTarget::Avx}) {
        const auto lowering = Codegen::MakeAmd64OnlyInstructionMatcher(target);
        for (const auto operation : kOperations) {
            const auto bytes = instruction(operation, true, 0, 6, 2);
            const auto name = Codegen::DepositExtractName(operation);
            require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode rewrote ") + name);
            const auto match = lowering->Match(bytes.data(), bytes.size());
            require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, std::string("A lowering mode did not require a trampoline for ") + name);
            const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
            require(lowering->MatchSequence(sequence, {}).has_value(), std::string("A sequence could not move out of line for ") + name);
        }
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

Execution executable(const Bytes& site, std::uint64_t memoryValue) {
    const auto operands = Codegen::DecodeDepositExtract(site.data(), site.size());
    require(operands.has_value(), "Execution site was not a deposit or extract instruction");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitDepositExtract(body, *Codegen::DecodeDepositExtract(relocated.data(), relocated.size()));
    });
}

std::uint64_t expected(const Codegen::DepositExtractOperands& operands, std::uint64_t source, std::uint64_t mask) {
    if (!operands.Wide) {
        source &= 0xFFFFFFFFull;
        mask &= 0xFFFFFFFFull;
    }
    std::uint64_t result = 0;
    std::uint64_t bit = 1;
    for (; mask != 0; mask &= mask - 1, bit <<= 1) {
        const auto lowest = mask & (~mask + 1);
        if (operands.Operation == DepositExtractOperation::Deposit) {
            if ((source & bit) != 0) result |= lowest;
        } else if ((source & lowest) != 0) {
            result |= bit;
        }
    }
    return result;
}

std::string describe(const Codegen::DepositExtractOperands& operands, std::uint64_t source, std::uint64_t mask) {
    return std::string(Codegen::DepositExtractName(operands.Operation)) + (operands.Wide ? " 64" : " 32") + " dst " + std::to_string(operands.Destination) + " source " + std::to_string(operands.Source) + " mask " + std::to_string(operands.Mask) + (operands.Memory ? " memory" : "") + " (source " + std::to_string(source) + ", mask value " + std::to_string(mask) + ")";
}

void check(const State& state, const Codegen::DepositExtractOperands& operands, std::uint64_t source, std::uint64_t mask) {
    const auto wanted = expected(operands, source, mask);
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto value = reg == operands.Destination ? wanted : state.Input[reg];
        if (state.Output[reg] != value)
            throw std::runtime_error(describe(operands, source, mask) + ": register " + std::to_string(reg) + " is " + std::to_string(state.Output[reg]) + ", wanted " + std::to_string(value));
    }
    if ((state.Flags & LoweringExecution::kFlagMask) != (state.InputFlags & LoweringExecution::kFlagMask))
        throw std::runtime_error(describe(operands, source, mask) + ": flags are " + std::to_string(state.Flags & LoweringExecution::kFlagMask) + ", wanted " + std::to_string(state.InputFlags & LoweringExecution::kFlagMask));
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, describe(operands, source, mask) + " overwrote the caller's red zone");
}

void run(const Execution& execution, const Codegen::DepositExtractOperands& operands, State state, std::uint64_t memoryValue) {
    for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
        state.InputFlags = flags;
        auto trial = state;
        execution.Run(trial);
        check(trial, operands, trial.Input[operands.Source], operands.Memory ? memoryValue : trial.Input[operands.Mask]);
    }
}

void registers() {
    constexpr std::array<std::uint64_t, 6> sources{0, 1, 0x80000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull, 0x5555555555555555ull};
    constexpr std::array<std::uint64_t, 10> masks{0, 1, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF00000000ull, 0xAAAAAAAAAAAAAAAAull, 0x00FF00F00F0000F1ull, 0x8000000180000001ull};
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const auto pair : std::array<std::pair<std::uint8_t, std::uint8_t>, 10>{{{6, 2}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}, {4, 4}, {8, 0}, {3, 3}, {11, 10}}}) {
                    const auto site = instruction(operation, wide, dst, pair.first, pair.second);
                    const auto operands = *Codegen::DecodeDepositExtract(site.data(), site.size());
                    const auto execution = executable(site, 0);
                    for (const auto source : sources) {
                        for (const auto mask : masks) {
                            auto state = LoweringExecution::InitialState();
                            state.Input[pair.second] = mask;
                            state.Input[pair.first] = pair.first == pair.second ? mask : source;
                            run(execution, operands, state, 0);
                        }
                    }
                }
            }
        }
    }
}

void memory() {
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto mask : std::array<std::uint64_t, 4>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull, 0x8000000180000001ull}) {
                    for (const auto source : std::array<std::uint64_t, 3>{0, 0xFFFFFFFFFFFFFFFFull, 0x123456789ABCDEF0ull}) {
                        const auto site = memorySite(operation, wide, tail);
                        const auto operands = *Codegen::DecodeDepositExtract(site.data(), site.size());
                        const auto execution = executable(site, mask);
                        auto state = LoweringExecution::InitialState();
                        state.Input[6] = reinterpret_cast<std::uint64_t>(execution.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                        state.Input[2] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 3 * 8;
                        state.Input[1] = 3;
                        state.Input[5] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 16;
                        state.Input[7] = source;
                        state.Output[0] = mask;
                        run(execution, operands, state, mask);
                    }
                }
            }
            for (const std::uint8_t dst : std::array<std::uint8_t, 2>{0, 4}) {
                for (const std::int32_t displacement : {-208, -200, -199, -192, -191, -184, -168, -152, -137, -136}) {
                    Bytes tail{static_cast<std::uint8_t>(0x84 | (dst << 3)), 0x24};
                    for (unsigned s = 0; s < 32; s += 8)
                        tail.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> s));
                    constexpr std::uint64_t mask = 0xFEDCBA9876543210ull;
                    const auto site = memorySite(operation, wide, tail);
                    const auto operands = *Codegen::DecodeDepositExtract(site.data(), site.size());
                    const Execution execution(site, mask, false, [&](Codegen::StubBodyBuilder& body, const Bytes&) {
                        for (int part = 0; part < 2; ++part) {
                            Bytes store{0xC7, 0x84, 0x24};
                            const auto slot = static_cast<std::uint32_t>(displacement + part * 4);
                            const auto word = static_cast<std::uint32_t>(mask >> (part * 32));
                            for (unsigned s = 0; s < 32; s += 8) store.push_back(static_cast<std::uint8_t>(slot >> s));
                            for (unsigned s = 0; s < 32; s += 8) store.push_back(static_cast<std::uint8_t>(word >> s));
                            body.Raw(store);
                        }
                        Codegen::EmitDepositExtract(body, operands);
                    });
                    auto state = LoweringExecution::InitialState();
                    state.Input[7] = 0x0F0F0F0FF0F0F0F0ull;
                    run(execution, operands, state, mask);
                }
            }
        }
    }
}

std::uint64_t next(std::uint64_t& seed) {
    seed += 0x9E3779B97F4A7C15ull;
    auto value = seed;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

void hardware() {
    constexpr std::size_t kRounds = 50000;
    if (!LoweringExecution::HostExecutes(instruction(DepositExtractOperation::Deposit, true, 0, 0, 0))) {
        std::cout << "this host has no BMI2: PDEP and PEXT were checked against the SDM model only\n";
        return;
    }
    std::uint64_t seed = 0x243F6A8885A308D3ull;
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            const auto site = instruction(operation, wide, 0, 6, 2);
            const auto operands = *Codegen::DecodeDepositExtract(site.data(), site.size());
            const auto lowered = executable(site, 0);
            const Execution native(site, 0, false, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) { body.Raw(relocated); });
            for (std::size_t round = 0; round < kRounds; ++round) {
                auto state = LoweringExecution::InitialState();
                const auto density = round % 4;
                state.Input[6] = next(seed);
                state.Input[2] = density == 0 ? next(seed) : density == 1 ? next(seed) & next(seed) & next(seed) : density == 2 ? next(seed) | next(seed) | next(seed) : next(seed) << (next(seed) & 63);
                state.InputFlags = LoweringExecution::kBaseFlags | (next(seed) & LoweringExecution::kFlagMask);
                auto first = state;
                auto second = state;
                lowered.Run(first);
                native.Run(second);
                if (first.Output != second.Output || first.Flags != second.Flags || first.RedZone != second.RedZone)
                    throw std::runtime_error(describe(operands, state.Input[6], state.Input[2]) + ": the stub and the host instruction disagree");
                check(first, operands, state.Input[6], state.Input[2]);
            }
        }
    }
}

void execution() {
    registers();
    memory();
    hardware();
}
#else
void execution() { std::cout << "deposit and extract execution checks require an x86-64 Linux, macOS or Windows build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "deposit and extract lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
