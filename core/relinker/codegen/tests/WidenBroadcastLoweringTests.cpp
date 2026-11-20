#include "VectorExecution.hpp"
#include <codegen/x86/WidenBroadcastLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
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

Bytes site(std::uint8_t opcode, bool wide256, std::uint8_t dst, std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((src & 8) ? 0 : 0x20) | 2), static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), opcode,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))};
}

Bytes memorySite(std::uint8_t opcode, bool wide256, std::uint8_t dst) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2), static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), opcode, static_cast<std::uint8_t>((dst & 7) << 3)};
}

constexpr std::array<std::uint8_t, 12> kWidenOpcodes{0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

void decoding() {
    for (const auto opcode : kWidenOpcodes) {
        const auto bytes = site(opcode, true, 9, 3);
        const auto operands = Codegen::DecodeWidenBroadcast(bytes.data(), bytes.size());
        require(operands && operands->Kind == Codegen::WidenBroadcastKind::Widen && operands->Destination == 9 && operands->Source == 3, "a 256-bit widening move was not decoded");
        const auto narrow = site(opcode, false, 9, 3);
        require(!Codegen::DecodeWidenBroadcast(narrow.data(), narrow.size()), "a 128-bit widening move, which AVX has, was lowered");
    }
    for (const auto& [opcode, wide] : std::array<std::pair<std::uint8_t, bool>, 9>{{{0x18, false}, {0x18, true}, {0x58, false}, {0x58, true}, {0x19, true}, {0x59, false}, {0x59, true}, {0x78, true}, {0x79, false}}}) {
        const auto bytes = site(opcode, wide, 1, 14);
        require(Codegen::DecodeWidenBroadcast(bytes.data(), bytes.size()).has_value(), "a register-source broadcast was not decoded");
    }
    for (const Bytes bytes : {memorySite(0x18, true, 0), memorySite(0x58, true, 0), memorySite(0x59, true, 0), site(0x19, false, 0, 1), Bytes{0xC4, 0xE2, 0xFD, 0x31, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x36, 0xC1}, Bytes{0x90}})
        require(!Codegen::DecodeWidenBroadcast(bytes.data(), bytes.size()), "an AVX broadcast, a twin or an unrelated instruction was taken as widen/broadcast");
    const Bytes trailing{0xC4, 0xE2, 0x7D, 0x31, 0xC1, 0x90};
    bool rejected = false;
    try { (void)Codegen::DecodeWidenBroadcast(trailing.data(), trailing.size()); }
    catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "trailing bytes were accepted");
}

void matching() {
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto bytes = site(0x31, true, 0, 1);
    require(!rosetta->Match(bytes.data(), bytes.size()), "Rosetta mode lowered a widening move");
    const auto match = avx->Match(bytes.data(), bytes.size());
    require(match && match->InstructionName == "VPMOVZXBD" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, "AVX mode did not lower VPMOVZXBD through a movable stub");
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;
using Vector = std::array<std::uint64_t, 4>;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitWidenBroadcast(body, *Codegen::DecodeWidenBroadcast(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::array<std::uint8_t, 32> bytesOf(const Vector& value) {
    std::array<std::uint8_t, 32> out{};
    std::memcpy(out.data(), value.data(), out.size());
    return out;
}

Vector vectorOf(const std::array<std::uint8_t, 32>& bytes) {
    Vector out{};
    std::memcpy(out.data(), bytes.data(), bytes.size());
    return out;
}

Vector widened(std::uint8_t opcode, const Vector& source) {
    constexpr std::array<std::pair<unsigned, unsigned>, 6> sizes{{{1, 2}, {1, 4}, {1, 8}, {2, 4}, {2, 8}, {4, 8}}};
    const auto [in, out] = sizes[opcode & 7];
    const bool sign = (opcode & 0x10) == 0;
    const auto input = bytesOf(source);
    std::array<std::uint8_t, 32> result{};
    for (unsigned element = 0; element < 32 / out; ++element) {
        std::uint64_t value = 0;
        std::memcpy(&value, input.data() + element * in, in);
        if (sign && (value >> (in * 8 - 1)) != 0) value |= ~std::uint64_t{0} << (in * 8);
        std::memcpy(result.data() + element * out, &value, out);
    }
    return vectorOf(result);
}

Vector broadcast(const Vector& source, unsigned size, bool wide256) {
    const auto input = bytesOf(source);
    std::array<std::uint8_t, 32> result{};
    for (unsigned offset = 0; offset < (wide256 ? 32u : 16u); offset += size) std::memcpy(result.data() + offset, input.data(), size);
    return vectorOf(result);
}

unsigned broadcastSize(std::uint8_t opcode) {
    if (opcode == 0x78) return 1;
    if (opcode == 0x79) return 2;
    if (opcode == 0x18 || opcode == 0x58) return 4;
    return 8;
}

void verify(const Machine& start, const Machine& end, std::uint8_t dst, const Vector& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

void execution() {
    const std::array<std::array<std::uint8_t, 2>, 5> pairs{{{0, 1}, {3, 3}, {15, 14}, {8, 2}, {13, 15}}};
    for (const auto opcode : kWidenOpcodes) {
        for (const auto pair : pairs) {
            auto machine = VectorExecution::Initial();
            const auto start = machine;
            Runner(lowered(site(opcode, true, pair[0], pair[1]))).Run(machine);
            verify(start, machine, pair[0], widened(opcode, start.Ymm[pair[1]]), "widen " + std::to_string(opcode) + " " + std::to_string(pair[0]) + "," + std::to_string(pair[1]));
        }
        auto machine = VectorExecution::Initial();
        const auto start = machine;
        Runner(lowered(memorySite(opcode, true, 5))).Run(machine);
        verify(start, machine, 5, widened(opcode, start.Memory), "widen " + std::to_string(opcode) + " from memory");
    }
    for (const auto& [opcode, wide] : std::array<std::pair<std::uint8_t, bool>, 11>{{{0x18, false}, {0x18, true}, {0x58, false}, {0x58, true}, {0x19, true}, {0x59, false}, {0x59, true}, {0x78, false}, {0x78, true}, {0x79, false}, {0x79, true}}}) {
        for (const auto pair : pairs) {
            auto machine = VectorExecution::Initial();
            const auto start = machine;
            Runner(lowered(site(opcode, wide, pair[0], pair[1]))).Run(machine);
            verify(start, machine, pair[0], broadcast(start.Ymm[pair[1]], broadcastSize(opcode), wide), "broadcast " + std::to_string(opcode) + (wide ? " ymm " : " xmm ") + std::to_string(pair[0]) + "," + std::to_string(pair[1]));
        }
        if (opcode == 0x78 || opcode == 0x79) {
            auto machine = VectorExecution::Initial();
            const auto start = machine;
            Runner(lowered(memorySite(opcode, wide, 6))).Run(machine);
            verify(start, machine, 6, broadcast(start.Memory, broadcastSize(opcode), wide), "broadcast " + std::to_string(opcode) + " from memory");
        }
    }
}

void pageEndMemory() {
    const auto pageSize = LoweringExecution::PageSize();
    auto* area = LoweringExecution::AllocatePages(pageSize * 2);
    require(area != nullptr, "cannot allocate the guarded operand pages");
    require(LoweringExecution::ProtectPages(area + pageSize, pageSize, false, false), "cannot protect the page after the operand");
    const auto memory = VectorExecution::Initial().Memory;
    const auto runAtPageEnd = [&](const Bytes& instruction, unsigned width, std::uint8_t dst, const Vector& wanted, const std::string& describe) {
        auto* operand = area + pageSize - width;
        std::memcpy(operand, memory.data(), width);
        Bytes body{0x48, 0xB8};
        const auto address = reinterpret_cast<std::uintptr_t>(operand);
        for (unsigned shift = 0; shift < 64; shift += 8) body.push_back(static_cast<std::uint8_t>(address >> shift));
        const auto stub = lowered(instruction);
        body.insert(body.end(), stub.begin(), stub.end());
        auto machine = VectorExecution::Initial();
        const auto start = machine;
        Runner(body).Run(machine);
        verify(start, machine, dst, wanted, describe + " at the end of a page");
    };
    constexpr std::array<std::pair<unsigned, unsigned>, 6> sizes{{{1, 2}, {1, 4}, {1, 8}, {2, 4}, {2, 8}, {4, 8}}};
    for (const auto opcode : kWidenOpcodes) {
        const auto [in, out] = sizes[opcode & 7];
        runAtPageEnd(memorySite(opcode, true, 5), 32 / out * in, 5, widened(opcode, memory), "widen " + std::to_string(opcode) + " from memory");
    }
    for (const auto& [opcode, wide] : std::array<std::pair<std::uint8_t, bool>, 4>{{{0x78, false}, {0x78, true}, {0x79, false}, {0x79, true}}})
        runAtPageEnd(memorySite(opcode, wide, 6), broadcastSize(opcode), 6, broadcast(memory, broadcastSize(opcode), wide), "broadcast " + std::to_string(opcode) + " from memory");
    LoweringExecution::FreePages(area, pageSize * 2);
}

#else

void execution() { std::cout << "widen/broadcast execution checks require an x86-64 Linux, macOS or Windows build\n"; }
void pageEndMemory() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        pageEndMemory();
        std::cout << "widen/broadcast lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
