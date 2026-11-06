#include "LoweringExecution.hpp"
#include <codegen/x86/BlsLowering.hpp>
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
using Codegen::BlsOperation;

constexpr std::array kOperations{BlsOperation::Reset, BlsOperation::Mask, BlsOperation::Isolate};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t extension(BlsOperation operation) {
    return static_cast<std::uint8_t>(operation);
}

Bytes instruction(BlsOperation operation, bool wide, std::uint8_t dst, std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((src & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~dst & 15) << 3)), 0xF3,
        static_cast<std::uint8_t>(0xC0 | (extension(operation) << 3) | (src & 7))};
}

void decoding() {
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (std::uint8_t src = 0; src < 16; ++src) {
                    const auto bytes = instruction(operation, wide, dst, src);
                    const auto operands = Codegen::DecodeBls(bytes.data(), bytes.size());
                    require(operands && operands->Operation == operation && operands->Wide == wide && operands->Destination == dst && operands->Source == src && !operands->Memory, "BLS register decoding changed an operand");
                }
            }
        }
        const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xA8, 0xF3, static_cast<std::uint8_t>(0x84 | (extension(operation) << 3)), 0xAC, 0x10, 0, 0, 0};
        const auto operands = Codegen::DecodeBls(memory.data(), memory.size());
        require(operands && operands->Operation == operation && operands->Destination == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "BLS memory extensions or prefixes were lost");
    }
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF0}, Bytes{0xC4, 0xE1, 0xF0, 0xF3, 0xC8}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0xE0}, Bytes{0xC4, 0xE2, 0xF0, 0xF2, 0xC8}})
        require(!Codegen::DecodeBls(bytes.data(), bytes.size()), "Unrelated, truncated or non-BLS opcode matched BLS");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF4, 0xF3, 0xC8}, Bytes{0xC4, 0xE2, 0xF1, 0xF3, 0xD0}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0xD8, 0x90}, Bytes{0xC4, 0xE2, 0xF0, 0xF3, 0x8C}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeBls(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed BLS was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF0, 0xF3, 0x8C, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitBls(body, *Codegen::DecodeBls(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto operation : kOperations) {
        const auto bytes = instruction(operation, true, 0, 6);
        const auto name = Codegen::BlsName(operation);
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
    const auto operands = Codegen::DecodeBls(site.data(), site.size());
    require(operands.has_value(), "Execution site was not BLS");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitBls(body, *Codegen::DecodeBls(relocated.data(), relocated.size()));
    });
}

void check(const State& state, const Codegen::BlsOperands& operands, std::uint64_t source) {
    const auto name = Codegen::BlsName(operands.Operation);
    const auto mask = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
    source &= mask;
    std::uint64_t expected = 0;
    std::uint64_t flags = 0;
    switch (operands.Operation) {
    case BlsOperation::Reset:
        expected = (source & (source - 1)) & mask;
        flags = (expected == 0 ? LoweringExecution::kZero : 0u) | (source == 0 ? LoweringExecution::kCarry : 0u);
        break;
    case BlsOperation::Mask:
        expected = (source ^ (source - 1)) & mask;
        flags = source == 0 ? LoweringExecution::kCarry : 0u;
        break;
    case BlsOperation::Isolate:
        expected = (source & (~source + 1)) & mask;
        flags = (expected == 0 ? LoweringExecution::kZero : 0u) | (source != 0 ? LoweringExecution::kCarry : 0u);
        break;
    }
    flags |= (expected >> (operands.Wide ? 63 : 31)) != 0 ? LoweringExecution::kSign : 0u;
    for (std::size_t reg = 0; reg < 16; ++reg)
        require(state.Output[reg] == (reg == operands.Destination ? expected : state.Input[reg]), std::string(name) + " changed a source/scratch register or produced an incorrect result");
    require((state.Flags & LoweringExecution::kFlagMask) == flags, std::string(name) + " did not produce its defined flags");
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, std::string(name) + " overwrote the caller's red zone");
}

void execution() {
    constexpr std::array<std::uint64_t, 8> values{0, 1, 0x6, 0x80000000ull, 0x8000000000000000ull, 0xFFFFFFFF00000000ull, 0xFFFFFFFFFFFFFFFFull, 0xABCD123456789000ull};
    for (const auto operation : kOperations) {
        for (bool wide : {false, true}) {
            for (std::uint8_t dst = 0; dst < 16; ++dst) {
                for (const std::uint8_t src : std::array<std::uint8_t, 4>{6, dst, 4, 10}) {
                    const auto site = instruction(operation, wide, dst, src);
                    const auto run = executable(site, 0);
                    for (const auto value : values) {
                        auto state = LoweringExecution::InitialState();
                        state.Input[src] = value;
                        run.Run(state);
                        check(state, *Codegen::DecodeBls(site.data(), site.size()), state.Input[src]);
                    }
                }
            }
            for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
                for (const auto value : std::array<std::uint64_t, 3>{0, 0xFEDCBA9876543210ull, 0xFFFFFFFF00000000ull}) {
                    Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>(wide ? 0xF8 : 0x78), 0xF3, static_cast<std::uint8_t>(tail[0] | (extension(operation) << 3))};
                    site.insert(site.end(), tail.begin() + 1, tail.end());
                    const auto run = executable(site, value);
                    auto state = LoweringExecution::InitialState();
                    state.Input[6] = reinterpret_cast<std::uint64_t>(run.Memory()) - (tail[0] == 0x46 ? 16 : 0);
                    state.Input[2] = reinterpret_cast<std::uint64_t>(run.Memory()) - 3 * 8;
                    state.Input[1] = 3;
                    state.Input[5] = reinterpret_cast<std::uint64_t>(run.Memory()) - 16;
                    state.Output[0] = value;
                    run.Run(state);
                    check(state, *Codegen::DecodeBls(site.data(), site.size()), value);
                }
            }
        }
    }
}
#else
void execution() { std::cout << "BLS execution checks require a Linux/macOS x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "BLS lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
