#include "LoweringExecution.hpp"
#include <codegen/x86/LaneSplitLowering.hpp>
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
using Codegen::LaneSplitShape;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes vex(std::uint8_t map, std::uint8_t prefix, bool wide256, std::uint8_t opcode, std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm, int immediate = -1) {
    Bytes bytes{0xC4, static_cast<std::uint8_t>(((reg & 8) ? 0 : 0x80) | 0x40 | ((rm & 8) ? 0 : 0x20) | map),
        static_cast<std::uint8_t>(((~vvvv & 15) << 3) | (wide256 ? 4 : 0) | prefix), opcode,
        static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))};
    if (immediate >= 0) bytes.push_back(static_cast<std::uint8_t>(immediate));
    return bytes;
}

struct Form {
    LaneSplitShape Shape;
    std::uint8_t Map;
    std::uint8_t Prefix;
    std::uint8_t Opcode;
    std::uint8_t Extension;
};

const std::vector<Form>& forms() {
    static const std::vector<Form> list{
        {LaneSplitShape::Binary, 1, 1, 0xFE, 0},
        {LaneSplitShape::Binary, 1, 1, 0xDB, 0},
        {LaneSplitShape::Binary, 1, 1, 0xEF, 0},
        {LaneSplitShape::Binary, 1, 1, 0x6B, 0},
        {LaneSplitShape::Binary, 1, 1, 0x76, 0},
        {LaneSplitShape::Binary, 1, 1, 0xF4, 0},
        {LaneSplitShape::Binary, 2, 1, 0x00, 0},
        {LaneSplitShape::Binary, 2, 1, 0x40, 0},
        {LaneSplitShape::Binary, 2, 1, 0x3F, 0},
        {LaneSplitShape::BinaryImmediate, 3, 1, 0x0E, 0},
        {LaneSplitShape::BinaryImmediate, 3, 1, 0x0F, 0},
        {LaneSplitShape::ShiftCount, 1, 1, 0xF2, 0},
        {LaneSplitShape::ShiftCount, 1, 1, 0xD3, 0},
        {LaneSplitShape::ShiftImmediate, 1, 1, 0x72, 6},
        {LaneSplitShape::ShiftImmediate, 1, 1, 0x71, 4},
        {LaneSplitShape::ShiftImmediate, 1, 1, 0x73, 3},
        {LaneSplitShape::Unary, 2, 1, 0x1E, 0},
        {LaneSplitShape::UnaryImmediate, 1, 1, 0x70, 0},
        {LaneSplitShape::UnaryImmediate, 1, 2, 0x70, 0},
        {LaneSplitShape::UnaryImmediate, 1, 3, 0x70, 0},
    };
    return list;
}

bool immediate(LaneSplitShape shape) {
    return shape == LaneSplitShape::BinaryImmediate || shape == LaneSplitShape::ShiftImmediate || shape == LaneSplitShape::UnaryImmediate;
}

Bytes site(const Form& form, bool wide256, std::uint8_t dst, std::uint8_t first, std::uint8_t second, int imm) {
    const int value = immediate(form.Shape) ? imm : -1;
    switch (form.Shape) {
    case LaneSplitShape::ShiftImmediate:
        return vex(form.Map, form.Prefix, wide256, form.Opcode, form.Extension, dst, first, value);
    case LaneSplitShape::Unary:
    case LaneSplitShape::UnaryImmediate:
        return vex(form.Map, form.Prefix, wide256, form.Opcode, dst, 0, second, value);
    default:
        return vex(form.Map, form.Prefix, wide256, form.Opcode, dst, first, second, value);
    }
}

void decoding() {
    for (const auto& form : forms()) {
        const auto bytes = site(form, true, 9, 3, 12, 0x21);
        const auto operands = Codegen::DecodeLaneSplit(bytes.data(), bytes.size());
        require(operands && operands->Shape == form.Shape && operands->Destination == 9 && operands->Opcode == form.Opcode, "a 256-bit integer form was not decoded");
        if (form.Shape == LaneSplitShape::ShiftImmediate) require(operands->First == 3, "a shift by immediate lost its source");
        if (form.Shape == LaneSplitShape::Binary) require(operands->First == 3 && operands->Second == 12, "a binary form lost an operand");
        const auto narrow = site(form, false, 9, 3, 12, 0x21);
        require(!Codegen::DecodeLaneSplit(narrow.data(), narrow.size()), "a 128-bit form was taken as a 256-bit one");
    }
    const Bytes twoByte{0xC5, 0xF5, 0xFE, 0xC2};
    const auto shortForm = Codegen::DecodeLaneSplit(twoByte.data(), twoByte.size());
    require(shortForm && shortForm->Destination == 0 && shortForm->First == 1 && shortForm->Second == 2, "a two-byte VEX VPADDD was not decoded");
    const Bytes memory{0x64, 0xC4, 0x41, 0x75, 0xFE, 0x44, 0x24, 0x10};
    const auto loaded = Codegen::DecodeLaneSplit(memory.data(), memory.size());
    require(loaded && loaded->Memory && loaded->Memory->Prefixes == Bytes{0x64} && loaded->Destination == 8, "a memory VPADDD lost its prefix or operand");
    for (const Bytes bytes : {Bytes{0xC5, 0xFD, 0x6F, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x36, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x47, 0xC1}, Bytes{0xC5, 0xFC, 0x58, 0xC1}, Bytes{0xC5, 0xFD, 0xD7, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x30, 0xC1}, Bytes{0x90}})
        require(!Codegen::DecodeLaneSplit(bytes.data(), bytes.size()), "an instruction that is not lane-local integer AVX2 was split");
    for (const Bytes bytes : {Bytes{0xC5, 0xF5, 0x72, 0x30, 0x01}, Bytes{0xC5, 0xF5, 0xFE, 0xC2, 0x90}, Bytes{0xC4, 0xE3, 0x75, 0x0E, 0xC2}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeLaneSplit(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "a malformed 256-bit integer instruction was accepted");
    }
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    const Bytes bytes{0xC5, 0xF5, 0xFE, 0xC2};
    require(!intel->Match(bytes.data(), bytes.size()) && !rosetta->Match(bytes.data(), bytes.size()), "Intel or Rosetta mode split a 256-bit integer instruction");
    const auto match = avx->Match(bytes.data(), bytes.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow && match->InstructionName == "256-bit integer", "AVX mode did not lower a 256-bit integer instruction through a movable stub");
}

#if LOWERING_EXECUTION_AVAILABLE

struct Machine {
    std::array<std::array<std::uint64_t, 4>, 16> Ymm;
    std::array<std::uint64_t, 4> Memory;
    std::array<std::uint64_t, 16> RedZone;
};

class Runner {
public:
    explicit Runner(const Bytes& body) {
        Bytes code;
        code.insert(code.end(), {0x48, 0x8D, 0x87});
        append32(code, 512);
        for (std::uint8_t reg = 0; reg < 16; ++reg) {
            code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, 0x6F, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
            append32(code, reg * 32);
        }
        for (int slot = 0; slot < 16; ++slot) {
            code.insert(code.end(), {0x48, 0x8B, 0x8F});
            append32(code, 544 + slot * 8);
            code.insert(code.end(), {0x48, 0x89, 0x8C, 0x24});
            append32(code, -128 + slot * 8);
        }
        code.insert(code.end(), body.begin(), body.end());
        for (std::uint8_t reg = 0; reg < 16; ++reg) {
            code.insert(code.end(), {0xC4, static_cast<std::uint8_t>((reg & 8 ? 0x60 : 0xE0) | 1), 0x7E, 0x7F, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
            append32(code, reg * 32);
        }
        for (int slot = 0; slot < 16; ++slot) {
            code.insert(code.end(), {0x48, 0x8B, 0x8C, 0x24});
            append32(code, -128 + slot * 8);
            code.insert(code.end(), {0x48, 0x89, 0x8F});
            append32(code, 544 + slot * 8);
        }
        code.insert(code.end(), {0xC5, 0xF8, 0x77, 0xC3});
        page = static_cast<std::uint8_t*>(mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (page == MAP_FAILED) throw std::runtime_error("Cannot allocate the lane-split execution page");
        if (code.size() > 8192) throw std::runtime_error("Lane-split execution code is too large");
        std::memcpy(page, code.data(), code.size());
        if (mprotect(page, 8192, PROT_READ | PROT_EXEC) != 0) throw std::runtime_error("Cannot make the lane-split execution page executable");
    }
    ~Runner() { munmap(page, 8192); }
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    void Run(Machine& machine) const { reinterpret_cast<void (*)(Machine*)>(page)(&machine); }

private:
    std::uint8_t* page = nullptr;
    static void append32(Bytes& bytes, std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
};

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitLaneSplit(body, *Codegen::DecodeLaneSplit(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

Machine initial() {
    Machine machine{};
    for (std::size_t reg = 0; reg < 16; ++reg)
        for (std::size_t q = 0; q < 4; ++q) machine.Ymm[reg][q] = 0x0123456789ABCDEFull * (reg + 1) ^ (0x1111111111111111ull * (q + 3)) ^ (q == 1 ? 0x8000800080008000ull : 0);
    for (std::size_t q = 0; q < 4; ++q) machine.Memory[q] = 0xFEDCBA9876543210ull ^ (0x0F0F0F0F0F0F0F0Full * q);
    for (auto& slot : machine.RedZone) slot = LoweringExecution::kSentinel;
    return machine;
}

std::array<std::uint64_t, 4> perHalf(const Form& form, const Machine& start, std::uint8_t first, std::uint8_t second, int imm, bool memorySecond) {
    std::array<std::uint64_t, 4> result{};
    for (int half = 0; half < 2; ++half) {
        Machine machine{};
        for (auto& slot : machine.RedZone) slot = LoweringExecution::kSentinel;
        const auto& a = start.Ymm[first];
        const auto& b = memorySecond ? start.Memory : start.Ymm[second];
        const bool count = form.Shape == LaneSplitShape::ShiftCount;
        machine.Ymm[1] = {a[half * 2], a[half * 2 + 1], 0, 0};
        machine.Ymm[2] = count ? std::array<std::uint64_t, 4>{b[0], b[1], 0, 0} : std::array<std::uint64_t, 4>{b[half * 2], b[half * 2 + 1], 0, 0};
        const Runner runner(site(form, false, 0, 1, 2, imm));
        runner.Run(machine);
        result[half * 2] = machine.Ymm[0][0];
        result[half * 2 + 1] = machine.Ymm[0][1];
    }
    return result;
}

void check(const Form& form, std::uint8_t dst, std::uint8_t first, std::uint8_t second, int imm) {
    const auto instruction = site(form, true, dst, first, second, imm);
    const Runner runner(lowered(instruction));
    auto machine = initial();
    const auto start = machine;
    runner.Run(machine);
    const bool unary = form.Shape == LaneSplitShape::Unary || form.Shape == LaneSplitShape::UnaryImmediate;
    const auto wanted = perHalf(form, start, unary ? second : first, second, imm, false);
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (machine.Ymm[reg] != expected)
            throw std::runtime_error("lane split of opcode " + std::to_string(form.Opcode) + " map " + std::to_string(form.Map) + " dst " + std::to_string(dst) + " first " + std::to_string(first) + " second " + std::to_string(second) + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : machine.RedZone) require(slot == LoweringExecution::kSentinel, "a lane split overwrote the red zone");
}

void execution() {
    for (const auto& form : forms()) {
        for (const auto& registers : std::array<std::array<std::uint8_t, 3>, 7>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {13, 15, 14}}}) {
            for (const int imm : {0x00, 0x1B, 0x05, 0x88}) {
                if (!immediate(form.Shape) && imm != 0x00) continue;
                check(form, registers[0], registers[1], registers[2], imm);
            }
        }
    }
}

#else

void execution() { std::cout << "lane split execution checks require a Linux/macOS x86-64 build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "lane split lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
