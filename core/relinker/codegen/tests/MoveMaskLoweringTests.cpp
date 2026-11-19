#include "GprVectorExecution.hpp"
#include <codegen/x86/MoveMaskLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes site(std::uint8_t destination, std::uint8_t source, bool wide = false) {
    return {0xC4, static_cast<std::uint8_t>(((destination & 8) ? 0 : 0x80) | 0x40 | ((source & 8) ? 0 : 0x20) | 1), static_cast<std::uint8_t>((wide ? 0x80 : 0) | 0x7D), 0xD7,
        static_cast<std::uint8_t>(0xC0 | ((destination & 7) << 3) | (source & 7))};
}

bool throws(const Bytes& bytes) {
    try {
        (void)Codegen::DecodeMoveMask(bytes.data(), bytes.size());
    } catch (const Codegen::CodegenException&) {
        return true;
    }
    return false;
}

void decoding() {
    for (std::uint8_t destination = 0; destination < 16; ++destination) {
        if (destination == 4) continue;
        for (std::uint8_t source = 0; source < 16; ++source) {
            for (const bool wide : {false, true}) {
                const auto bytes = site(destination, source, wide);
                const auto operands = Codegen::DecodeMoveMask(bytes.data(), bytes.size());
                require(operands && operands->Destination == destination && operands->Source == source, "a 256-bit VPMOVMSKB was not decoded");
            }
        }
    }
    const Bytes twoByte{0xC5, 0xFD, 0xD7, 0xC1};
    const auto operands = Codegen::DecodeMoveMask(twoByte.data(), twoByte.size());
    require(operands && operands->Destination == 0 && operands->Source == 1, "the two-byte VEX form was not decoded");
    for (const Bytes bytes : {Bytes{0xC5, 0xF9, 0xD7, 0xC1}, Bytes{0xC5, 0xF5, 0xD7, 0xC1}, Bytes{0xC5, 0xFD, 0xD7, 0x01}, Bytes{0xC5, 0xFC, 0xD7, 0xC1}, Bytes{0xC5, 0xFD, 0xD6, 0xC1}, Bytes{0x66, 0x0F, 0xD7, 0xC1}, Bytes{0x90}})
        require(!Codegen::DecodeMoveMask(bytes.data(), bytes.size()), "an AVX form, an invalid form or an unrelated instruction was taken as 256-bit VPMOVMSKB");
    require(throws(Bytes{0xC5, 0xFD, 0xD7, 0xC1, 0x90}), "trailing bytes were accepted");
    require(throws(Bytes{0xC5, 0xFD, 0xD7, 0xE1}), "an RSP destination was accepted");
}

void matching() {
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Intel);
    const auto bytes = site(9, 3);
    require(!intel->Match(bytes.data(), bytes.size()), "Intel mode lowered VPMOVMSKB");
    const auto match = avx->Match(bytes.data(), bytes.size());
    require(match && match->InstructionName == "VPMOVMSKB" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, "AVX mode did not lower VPMOVMSKB through a movable stub");
    const Bytes narrow{0xC5, 0xF9, 0xD7, 0xC1};
    require(!avx->Match(narrow.data(), narrow.size()), "AVX mode lowered the 128-bit VPMOVMSKB, which AVX has");
}

#if LOWERING_EXECUTION_AVAILABLE

using GprVectorExecution::Machine;
using GprVectorExecution::Runner;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitMoveMask(body, *Codegen::DecodeMoveMask(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

Machine initial(std::uint64_t seed, std::uint64_t flags) { return GprVectorExecution::Initial(seed, flags); }

std::uint64_t mask(const std::array<std::uint64_t, 4>& value) {
    std::array<std::uint8_t, 32> bytes{};
    std::memcpy(bytes.data(), value.data(), bytes.size());
    std::uint64_t result = 0;
    for (unsigned index = 0; index < bytes.size(); ++index)
        if ((bytes[index] & 0x80) != 0) result |= std::uint64_t{1} << index;
    return result;
}

void verify(const Machine& start, const Machine& end, std::uint8_t destination, std::uint64_t wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        if (end.Ymm[reg] != start.Ymm[reg]) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " changed");
        if (reg == 4) continue;
        const auto expected = reg == destination ? wanted : start.Gpr[reg];
        if (end.Gpr[reg] != expected) throw std::runtime_error(describe + ": r" + std::to_string(reg) + " is wrong");
    }
    require((end.Flags & 0xFFF) == (start.Flags & 0xFFF), describe + " changed RFLAGS");
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

void execution() {
    constexpr std::uint64_t kClear = LoweringExecution::kBaseFlags;
    constexpr std::uint64_t kSet = LoweringExecution::kBaseFlags | 0x8D5u;
    std::uint64_t seed = 0x9E3779B97F4A7C15ull;
    for (std::uint8_t destination = 0; destination < 16; ++destination) {
        if (destination == 4) continue;
        for (std::uint8_t source = 0; source < 16; ++source) {
            for (const auto flags : {kClear, kSet}) {
                const auto instruction = site(destination, source, (source & 1) != 0);
                const Runner runner(lowered(instruction));
                for (int round = 0; round < 3; ++round) {
                    auto machine = initial(seed += 0x2545F4914F6CDD1Dull, flags);
                    if (round == 1) machine.Ymm[source] = {~0ull, ~0ull, ~0ull, ~0ull};
                    if (round == 2) machine.Ymm[source] = {0x7F7F7F7F7F7F7F7Full, 0x8000000000000080ull, 0, 0x8080808080808080ull};
                    const auto start = machine;
                    runner.Run(machine);
                    verify(start, machine, destination, mask(start.Ymm[source]), "VPMOVMSKB r" + std::to_string(destination) + ", ymm" + std::to_string(source));
                }
            }
        }
    }
}

void native() {
    const auto probe = site(0, 1);
    if (!LoweringExecution::HostExecutes(probe)) {
        std::cout << "the host has no AVX2; the native comparison is skipped\n";
        return;
    }
    for (const auto& [destination, source] : std::array<std::pair<std::uint8_t, std::uint8_t>, 6>{{{0, 1}, {3, 15}, {7, 0}, {8, 14}, {15, 15}, {13, 8}}}) {
        const auto instruction = site(destination, source);
        const Runner lowering(lowered(instruction));
        const Runner hardware(instruction);
        auto first = initial(0xD1B54A32D192ED03ull + destination * 31u + source, LoweringExecution::kBaseFlags | 0x8D5u);
        auto second = first;
        lowering.Run(first);
        hardware.Run(second);
        require(first.Ymm == second.Ymm && first.Gpr == second.Gpr && first.RedZone == second.RedZone && (first.Flags & 0xFFF) == (second.Flags & 0xFFF),
            "the lowering differs from the host VPMOVMSKB for r" + std::to_string(destination) + ", ymm" + std::to_string(source));
    }
}

#else

void execution() { std::cout << "move mask execution checks require an x86-64 Linux, macOS or Windows build\n"; }
void native() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        native();
        std::cout << "move mask lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
