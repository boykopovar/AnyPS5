#include "LoweringExecution.hpp"
#include <codegen/x86/MovbeLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

constexpr std::array<std::uint8_t, 3> kWidths{16, 32, 64};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes site(bool store, std::uint8_t width, std::uint8_t reg, const Bytes& tail) {
    Bytes bytes;
    if (width == 16) bytes.push_back(0x66);
    const auto rex = static_cast<std::uint8_t>((width == 64 ? 8 : 0) | ((reg & 8) >> 1));
    if (rex != 0) bytes.push_back(static_cast<std::uint8_t>(0x40 | rex));
    bytes.insert(bytes.end(), {0x0F, 0x38, static_cast<std::uint8_t>(store ? 0xF1 : 0xF0), static_cast<std::uint8_t>(tail[0] | ((reg & 7) << 3))});
    bytes.insert(bytes.end(), tail.begin() + 1, tail.end());
    return bytes;
}

void decoding() {
    for (const bool store : {false, true}) {
        for (const auto width : kWidths) {
            for (std::uint8_t reg = 0; reg < 16; ++reg) {
                const auto bytes = site(store, width, reg, {0x06});
                const auto operands = Codegen::DecodeMovbe(bytes.data(), bytes.size());
                require(operands && operands->Store == store && operands->Width == width && operands->Register == reg && !operands->Memory.RipRelative, "MOVBE decoding changed an operand");
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0x4F, 0x0F, 0x38, 0xF1, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeMovbe(memory.data(), memory.size());
    require(operands && operands->Store && operands->Width == 64 && operands->Register == 9 && operands->Memory.RexIndexBase == 3 && operands->Memory.Prefixes == Bytes{0x64, 0x67}, "MOVBE memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xF2, 0x0F, 0x38, 0xF0, 0x06}, Bytes{0xF2, 0x0F, 0x38, 0xF1, 0x06}, Bytes{0x0F, 0x38, 0xF2, 0x06}, Bytes{0x0F, 0x38}, Bytes{0x0F, 0x38, 0x00, 0x06}})
        require(!Codegen::DecodeMovbe(bytes.data(), bytes.size()), "CRC32, a truncated or an unrelated opcode matched MOVBE");
    for (const Bytes bytes : {Bytes{0x0F, 0x38, 0xF0, 0xC0}, Bytes{0x0F, 0x38, 0xF0, 0x06, 0x90}, Bytes{0x0F, 0x38, 0xF1, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeMovbe(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed MOVBE was accepted");
    }
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const bool store : {false, true}) {
        const auto bytes = site(store, 64, 0, {0x06});
        require(!intel->Match(bytes.data(), bytes.size()) && !rosetta->Match(bytes.data(), bytes.size()), "Intel or Rosetta mode rewrote MOVBE");
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == "MOVBE" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "AVX mode did not require a trampoline for MOVBE");
        const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
        require(avx->MatchSequence(sequence, {}).has_value(), "A MOVBE sequence could not move out of line");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

std::uint64_t swapped(std::uint64_t value, std::uint8_t width) {
    std::uint64_t result = 0;
    for (unsigned byte = 0; byte < width / 8u; ++byte)
        result |= ((value >> (byte * 8)) & 0xFF) << ((width / 8u - 1 - byte) * 8);
    return result;
}

std::uint64_t merged(std::uint8_t width, std::uint64_t previous, std::uint64_t value) {
    if (width == 16) return (previous & ~0xFFFFull) | value;
    return value;
}

void execution() {
    constexpr std::array<std::uint64_t, 4> values{0, 0x0102030405060708ull, 0xFEDCBA9876543210ull, 0x00000000000080FFull};
    for (const bool store : {false, true}) {
        for (const auto width : kWidths) {
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (std::uint8_t reg = 0; reg < 16; ++reg) {
                    if (reg == 4 || reg == 6 || reg == 1 || reg == 2 || reg == 5) continue;
                    for (const auto value : values) {
                        const auto bytes = site(store, width, reg, tail);
                        const auto operands = *Codegen::DecodeMovbe(bytes.data(), bytes.size());
                        const auto stackMemory = tail[0] == 0x84;
                        const auto initial = stackMemory ? 0x1111111111111111ull : 0x2222222222222222ull;
                        Execution execution(bytes, initial, operands.Memory.RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
                            Codegen::EmitMovbe(body, *Codegen::DecodeMovbe(relocated.data(), relocated.size()));
                        });
                        auto state = LoweringExecution::InitialState();
                        state.Input[6] = reinterpret_cast<std::uint64_t>(execution.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                        state.Input[2] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 3 * 8;
                        state.Input[1] = 3;
                        state.Input[5] = reinterpret_cast<std::uint64_t>(execution.Memory()) - 16;
                        state.Input[reg] = value;
                        state.Output[0] = initial;
                        for (const auto flags : {LoweringExecution::kBaseFlags | LoweringExecution::kFlagMask, LoweringExecution::kBaseFlags}) {
                            state.InputFlags = flags;
                            if (!stackMemory) std::memcpy(const_cast<void*>(execution.Memory()), &initial, sizeof(initial));
                            auto trial = state;
                            execution.Run(trial);
                            const auto describe = std::string(store ? "MOVBE store " : "MOVBE load ") + std::to_string(width) + " reg " + std::to_string(reg) + " tail " + std::to_string(tail[0]) + " value " + std::to_string(value);
                            for (std::size_t other = 0; other < 16; ++other) {
                                auto wanted = trial.Input[other];
                                if (!store && other == reg)
                                    wanted = merged(width, trial.Input[reg], swapped(initial, width));
                                if (trial.Output[other] != wanted)
                                    throw std::runtime_error(describe + ": register " + std::to_string(other) + " is " + std::to_string(trial.Output[other]) + ", wanted " + std::to_string(wanted));
                            }
                            if ((trial.Flags & (LoweringExecution::kFlagMask | 0x14u)) != (flags & (LoweringExecution::kFlagMask | 0x14u)))
                                throw std::runtime_error(describe + ": flags changed");
                            if (store && !stackMemory) {
                                std::uint64_t memory = 0;
                                std::memcpy(&memory, execution.Memory(), sizeof(memory));
                                const auto mask = width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1;
                                const auto wanted = (initial & ~mask) | swapped(value, width);
                                if (memory != wanted)
                                    throw std::runtime_error(describe + ": memory is " + std::to_string(memory) + ", wanted " + std::to_string(wanted));
                            }
                            for (const auto red : trial.RedZone) require(red == LoweringExecution::kSentinel, "MOVBE overwrote the caller's red zone");
                        }
                    }
                }
            }
        }
    }
}

#else

void execution() { std::cout << "MOVBE execution checks require a Linux/macOS x86-64 build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "MOVBE lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
