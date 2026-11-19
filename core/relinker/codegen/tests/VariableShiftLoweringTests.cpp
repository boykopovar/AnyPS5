#include "VectorExecution.hpp"
#include <codegen/x86/VariableShiftLowering.hpp>
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

struct Form {
    const char* Name;
    std::uint8_t Opcode;
    bool Quadword;
};

constexpr std::array<Form, 5> kForms{{{"VPSRLVD", 0x45, false}, {"VPSRLVQ", 0x45, true}, {"VPSRAVD", 0x46, false}, {"VPSLLVD", 0x47, false}, {"VPSLLVQ", 0x47, true}}};

Bytes site(const Form& form, bool vector256, std::uint8_t destination, std::uint8_t source, std::uint8_t counts) {
    return {0xC4, static_cast<std::uint8_t>(((destination & 8) ? 0 : 0x80) | 0x40 | ((counts & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((form.Quadword ? 0x80 : 0) | ((~source & 15) << 3) | (vector256 ? 4 : 0) | 1), form.Opcode,
        static_cast<std::uint8_t>(0xC0 | ((destination & 7) << 3) | (counts & 7))};
}

Bytes memorySite(const Form& form, bool vector256, std::uint8_t destination, std::uint8_t source) {
    return {0xC4, static_cast<std::uint8_t>(((destination & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>((form.Quadword ? 0x80 : 0) | ((~source & 15) << 3) | (vector256 ? 4 : 0) | 1), form.Opcode,
        static_cast<std::uint8_t>((destination & 7) << 3)};
}

void decoding() {
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = site(form, vector256, 9, 3, 12);
            const auto operands = Codegen::DecodeVariableShift(bytes.data(), bytes.size());
            require(operands && std::string(operands->Name) == form.Name && operands->Quadword == form.Quadword && operands->Vector256 == vector256 && operands->Destination == 9 && operands->Source == 3 && operands->Counts == 12 && !operands->Memory,
                std::string(form.Name) + " was not decoded");
            const auto loaded = memorySite(form, vector256, 2, 5);
            const auto memory = Codegen::DecodeVariableShift(loaded.data(), loaded.size());
            require(memory && memory->Memory && memory->Destination == 2 && memory->Source == 5, std::string(form.Name) + " with a memory count was not decoded");
        }
    }
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF5, 0x46, 0xC2}, Bytes{0xC4, 0xE2, 0x75, 0x48, 0xC2}, Bytes{0xC4, 0xE3, 0x75, 0x45, 0xC2}, Bytes{0xC4, 0xE2, 0x74, 0x47, 0xC2}, Bytes{0x90}})
        require(!Codegen::DecodeVariableShift(bytes.data(), bytes.size()), "an instruction that is not a VEX variable shift was decoded as one");
    const Bytes trailing{0xC4, 0xE2, 0x75, 0x47, 0xC2, 0x90};
    bool rejected = false;
    try { (void)Codegen::DecodeVariableShift(trailing.data(), trailing.size()); }
    catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "trailing bytes were accepted");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = site(form, vector256, 0, 1, 2);
            require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode lowered ") + form.Name);
            const auto match = avx->Match(bytes.data(), bytes.size());
            require(match && match->InstructionName == form.Name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, std::string("AVX mode did not lower ") + form.Name + " through a movable stub");
        }
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;
using Vector = std::array<std::uint64_t, 4>;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitVariableShift(body, *Codegen::DecodeVariableShift(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

Vector shifted(const Form& form, bool vector256, const Vector& source, const Vector& counts) {
    Vector result{};
    const unsigned quads = vector256 ? 4u : 2u;
    for (unsigned quad = 0; quad < quads; ++quad) {
        if (form.Quadword) {
            const auto count = counts[quad];
            const auto value = source[quad];
            result[quad] = count > 63 ? 0 : form.Opcode == 0x47 ? value << count : value >> count;
            continue;
        }
        for (unsigned part = 0; part < 2; ++part) {
            const auto count = static_cast<std::uint32_t>(counts[quad] >> (part * 32));
            const auto value = static_cast<std::uint32_t>(source[quad] >> (part * 32));
            std::uint32_t out = 0;
            if (form.Opcode == 0x46)
                out = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> (count > 31 ? 31 : count));
            else if (count <= 31)
                out = form.Opcode == 0x47 ? value << count : value >> count;
            result[quad] |= static_cast<std::uint64_t>(out) << (part * 32);
        }
    }
    return result;
}

Vector countPattern(bool quadword, unsigned round) {
    if (quadword) {
        const std::array<Vector, 4> quads{{{0, 1, 63, 64}, {7, 32, 0x8000000000000000ull, 33}, {0xFFFFFFFF00000001ull, 62, 31, 0x100000003ull}, {5, 5, 65, 2}}};
        return quads[round % quads.size()];
    }
    const std::array<std::array<std::uint32_t, 8>, 4> dwords{{{0, 1, 31, 32, 7, 0x80000000u, 30, 33}, {16, 0xFFFFFFFFu, 3, 0, 31, 1, 0x100u, 2}, {9, 9, 9, 9, 40, 0, 5, 31}, {1, 2, 3, 4, 5, 6, 7, 8}}};
    Vector out{};
    std::memcpy(out.data(), dwords[round % dwords.size()].data(), sizeof(out));
    return out;
}

void verify(const Machine& start, const Machine& end, std::uint8_t destination, const Vector& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == destination ? wanted : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

void execution() {
    const bool native = LoweringExecution::HostExecutes(Bytes{0xC4, 0xE2, 0x7D, 0x47, 0xC0});
    const std::array<std::array<std::uint8_t, 3>, 8> registers{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {13, 15, 13}, {10, 10, 11}}};
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const auto name = std::string(form.Name) + (vector256 ? " ymm" : " xmm");
            for (unsigned round = 0; round < 4; ++round) {
                for (const auto& [destination, source, counts] : registers) {
                    const auto instruction = site(form, vector256, destination, source, counts);
                    auto machine = VectorExecution::Initial();
                    if (source != counts) machine.Ymm[counts] = countPattern(form.Quadword, round);
                    const auto start = machine;
                    Runner(lowered(instruction)).Run(machine);
                    const auto describe = name + " " + std::to_string(destination) + "," + std::to_string(source) + "," + std::to_string(counts) + " round " + std::to_string(round);
                    verify(start, machine, destination, shifted(form, vector256, start.Ymm[source], start.Ymm[counts]), describe);
                    if (native) {
                        auto hardware = start;
                        Runner(instruction).Run(hardware);
                        require(hardware.Ymm == machine.Ymm, describe + " differs from the host AVX2 instruction");
                    }
                }
                auto machine = VectorExecution::Initial();
                machine.Memory = countPattern(form.Quadword, round);
                const auto start = machine;
                Runner(lowered(memorySite(form, vector256, 6, 9))).Run(machine);
                verify(start, machine, 6, shifted(form, vector256, start.Ymm[9], start.Memory), name + " with memory counts round " + std::to_string(round));
            }
        }
    }
}

void pageEndMemory() {
    const auto pageSize = LoweringExecution::PageSize();
    auto* area = LoweringExecution::AllocatePages(pageSize * 2);
    require(area != nullptr, "cannot allocate the guarded operand pages");
    require(LoweringExecution::ProtectPages(area + pageSize, pageSize, false, false), "cannot protect the page after the operand");
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const unsigned width = vector256 ? 32u : 16u;
            auto* operand = area + pageSize - width;
            const auto counts = countPattern(form.Quadword, 1);
            std::memcpy(operand, counts.data(), width);
            Bytes body{0x48, 0xB8};
            const auto address = reinterpret_cast<std::uintptr_t>(operand);
            for (unsigned shift = 0; shift < 64; shift += 8) body.push_back(static_cast<std::uint8_t>(address >> shift));
            const auto stub = lowered(memorySite(form, vector256, 4, 11));
            body.insert(body.end(), stub.begin(), stub.end());
            auto machine = VectorExecution::Initial();
            const auto start = machine;
            Runner(body).Run(machine);
            Vector read{};
            std::memcpy(read.data(), counts.data(), width);
            verify(start, machine, 4, shifted(form, vector256, start.Ymm[11], read), std::string(form.Name) + " with memory counts at the end of a page");
        }
    }
    LoweringExecution::FreePages(area, pageSize * 2);
}

#else

void execution() { std::cout << "variable shift execution checks require an x86-64 Linux, macOS or Windows build\n"; }
void pageEndMemory() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        pageEndMemory();
        std::cout << "variable shift lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
