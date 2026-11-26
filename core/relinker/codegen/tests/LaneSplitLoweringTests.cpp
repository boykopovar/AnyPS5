#include "VectorExecution.hpp"
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
        {LaneSplitShape::LaneImmediate, 3, 1, 0x42, 0},
        {LaneSplitShape::Blend, 3, 1, 0x4C, 0},
    };
    return list;
}

bool immediate(LaneSplitShape shape) {
    return shape == LaneSplitShape::BinaryImmediate || shape == LaneSplitShape::ShiftImmediate || shape == LaneSplitShape::UnaryImmediate || shape == LaneSplitShape::LaneImmediate || shape == LaneSplitShape::Blend;
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

Bytes memorySite(const Form& form, std::uint8_t dst, std::uint8_t first, int imm) {
    const bool unary = form.Shape == LaneSplitShape::Unary || form.Shape == LaneSplitShape::UnaryImmediate;
    Bytes bytes{0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | form.Map),
        static_cast<std::uint8_t>(((~(unary ? 0 : first) & 15) << 3) | 4 | form.Prefix), form.Opcode,
        static_cast<std::uint8_t>(0x80 | ((dst & 7) << 3) | 7), 0, 2, 0, 0};
    if (immediate(form.Shape)) bytes.push_back(static_cast<std::uint8_t>(imm));
    return bytes;
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
    for (const Bytes bytes : {Bytes{0xC5, 0xFD, 0x6F, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x36, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x47, 0xC1}, Bytes{0xC5, 0xFC, 0x58, 0xC1}, Bytes{0xC4, 0xE2, 0x7D, 0x30, 0xC1}, Bytes{0x90}})
        require(!Codegen::DecodeLaneSplit(bytes.data(), bytes.size()), "an instruction that is not lane-local integer AVX2 was split");
    const Bytes mask{0xC5, 0xFD, 0xD7, 0xC1};
    const auto low = Codegen::DecodeLaneSplit(mask.data(), mask.size());
    require(low && low->Shape == LaneSplitShape::MoveMask && low->Destination == 0 && low->Second == 1, "a VPMOVMSKB was not decoded");
    const Bytes extendedMask{0xC4, 0x41, 0x7D, 0xD7, 0xEF};
    const auto extended = Codegen::DecodeLaneSplit(extendedMask.data(), extendedMask.size());
    require(extended && extended->Shape == LaneSplitShape::MoveMask && extended->Destination == 13 && extended->Second == 15, "a VPMOVMSKB lost an extended register");
    const Bytes blend{0xC4, 0xE3, 0x75, 0x4C, 0xC2, 0x3A};
    const auto blended = Codegen::DecodeLaneSplit(blend.data(), blend.size());
    require(blended && blended->Shape == LaneSplitShape::Blend && blended->First == 1 && blended->Second == 2 && blended->Immediate == 0x3A, "a VPBLENDVB lost its mask register");
    for (const Bytes bytes : {Bytes{0xC5, 0xFD, 0xD7, 0x00}, Bytes{0xC5, 0xF5, 0xD7, 0xC1}, Bytes{0xC5, 0xF9, 0xD7, 0xC1}, Bytes{0xC4, 0xE3, 0xF5, 0x4C, 0xC2, 0x30}})
        require(!Codegen::DecodeLaneSplit(bytes.data(), bytes.size()), "an encoding the processor rejects was split");
    for (const Bytes bytes : {Bytes{0xC5, 0xF5, 0x72, 0x30, 0x01}, Bytes{0xC5, 0xF5, 0xFE, 0xC2, 0x90}, Bytes{0xC4, 0xE3, 0x75, 0x0E, 0xC2}, Bytes{0xC4, 0xE3, 0x75, 0x42, 0xC2}, Bytes{0xC4, 0xE3, 0x75, 0x4C, 0xC2}, Bytes{0xC5, 0xFD, 0xD7, 0xC1, 0x90}}) {
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
    for (const Bytes added : {Bytes{0xC5, 0xFD, 0xD7, 0xC1}, Bytes{0xC4, 0xE3, 0x75, 0x4C, 0xC2, 0x30}, Bytes{0xC4, 0xE3, 0x75, 0x42, 0xC2, 0x2A}}) {
        require(!intel->Match(added.data(), added.size()) && !rosetta->Match(added.data(), added.size()), "Intel or Rosetta mode lowered a byte mask, byte blend or block difference");
        const auto lowered = avx->Match(added.data(), added.size());
        require(lowered && lowered->Lowering == Codegen::Amd64OnlyLowering::Trampoline && lowered->InstructionName == "256-bit integer", "AVX mode did not lower a byte mask, byte blend or block difference");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitLaneSplit(body, *Codegen::DecodeLaneSplit(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
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

using Value = std::array<std::uint64_t, 4>;
using Octets = std::array<std::uint8_t, 32>;

Octets octets(const Value& value) {
    Octets bytes{};
    std::memcpy(bytes.data(), value.data(), bytes.size());
    return bytes;
}

Value value(const Octets& bytes) {
    Value result{};
    std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

Value blendResult(const Value& first, const Value& second, const Value& mask) {
    const auto a = octets(first);
    const auto b = octets(second);
    const auto m = octets(mask);
    Octets out{};
    for (std::size_t index = 0; index < out.size(); ++index) out[index] = (m[index] & 0x80) != 0 ? b[index] : a[index];
    return value(out);
}

Value differenceResult(const Value& first, const Value& second, int imm) {
    const auto a = octets(first);
    const auto b = octets(second);
    Octets out{};
    for (std::size_t lane = 0; lane < 2; ++lane) {
        const auto control = static_cast<std::size_t>(lane == 0 ? imm & 7 : (imm >> 3) & 7);
        const auto firstBlock = lane * 16 + ((control >> 2) & 1) * 4;
        const auto secondBlock = lane * 16 + (control & 3) * 4;
        for (std::size_t word = 0; word < 8; ++word) {
            unsigned sum = 0;
            for (std::size_t byte = 0; byte < 4; ++byte) {
                const int difference = static_cast<int>(a[firstBlock + word + byte]) - static_cast<int>(b[secondBlock + byte]);
                sum += static_cast<unsigned>(difference < 0 ? -difference : difference);
            }
            out[lane * 16 + word * 2] = static_cast<std::uint8_t>(sum);
            out[lane * 16 + word * 2 + 1] = static_cast<std::uint8_t>(sum >> 8);
        }
    }
    return value(out);
}

Value wantedResult(const Form& form, const Machine& start, std::uint8_t first, const Value& second, int imm) {
    if (form.Shape == LaneSplitShape::Blend) return blendResult(start.Ymm[first], second, start.Ymm[static_cast<std::uint8_t>(imm) >> 4]);
    return differenceResult(start.Ymm[first], second, imm);
}

bool described(LaneSplitShape shape) {
    return shape == LaneSplitShape::Blend || shape == LaneSplitShape::LaneImmediate;
}

void checkStackSource(const Form& form, std::uint8_t dst, std::uint8_t first, int imm) {
    Bytes instruction{0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | form.Map),
        static_cast<std::uint8_t>(((~first & 15) << 3) | 4 | form.Prefix), form.Opcode,
        static_cast<std::uint8_t>(0x44 | ((dst & 7) << 3)), 0x24, 0xC0, static_cast<std::uint8_t>(imm)};
    const Runner runner(lowered(instruction));
    auto machine = VectorExecution::Initial();
    const auto start = machine;
    runner.Run(machine);
    Value source{};
    source.fill(LoweringExecution::kSentinel);
    const auto wanted = wantedResult(form, start, first, source, imm);
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        require(machine.Ymm[reg] == (reg == dst ? wanted : start.Ymm[reg]), "a lane split with a source in the red zone left ymm" + std::to_string(reg) + " wrong");
    for (const auto slot : machine.RedZone) require(slot == LoweringExecution::kSentinel, "a lane split with a source in the red zone overwrote it");
}

void machineSlot(Bytes& code, std::uint8_t opcode, std::uint8_t reg, int index) {
    code.insert(code.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x87 | ((reg & 7) << 3))});
    const auto displacement = static_cast<std::uint32_t>(512 + index * 8);
    for (unsigned shift = 0; shift < 32; shift += 8) code.push_back(static_cast<std::uint8_t>(displacement >> shift));
}

void checkMask(std::uint8_t dst, std::uint8_t source, const Value& input, std::uint32_t flags) {
    const Bytes below{0x48, 0x8D, 0x64, 0x24, 0x80};
    const Bytes above{0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00};
    const auto instruction = vex(1, 1, true, 0xD7, dst, 0, source);
    const auto stub = lowered(instruction);
    Bytes code;
    machineSlot(code, 0x89, dst, 1);
    code.insert(code.end(), below.begin(), below.end());
    code.push_back(0x68);
    for (unsigned shift = 0; shift < 32; shift += 8) code.push_back(static_cast<std::uint8_t>(flags >> shift));
    code.push_back(0x9D);
    code.insert(code.end(), above.begin(), above.end());
    code.insert(code.end(), stub.begin(), stub.end());
    if (dst == 4) {
        machineSlot(code, 0x89, dst, 0);
        machineSlot(code, 0x89, 0, 3);
        machineSlot(code, 0x8B, dst, 1);
    } else {
        code.insert(code.end(), below.begin(), below.end());
        if (dst & 8) code.push_back(0x41);
        code.push_back(static_cast<std::uint8_t>(0x50 | (dst & 7)));
        code.insert(code.end(), {0x41, 0x5B, 0x48, 0x8B, 0xBC, 0x24, 0x80, 0, 0, 0});
        machineSlot(code, 0x89, 11, 0);
        machineSlot(code, 0x89, 0, 3);
        machineSlot(code, 0x8B, dst, 1);
        code.insert(code.end(), above.begin(), above.end());
    }
    code.insert(code.end(), below.begin(), below.end());
    code.push_back(0x9C);
    machineSlot(code, 0x8F, 0, 2);
    code.insert(code.end(), above.begin(), above.end());
    const Runner runner(code);
    auto machine = VectorExecution::Initial();
    machine.Ymm[source] = input;
    const auto start = machine;
    runner.Run(machine);
    std::uint64_t wanted = 0;
    const auto bytes = octets(input);
    for (std::size_t index = 0; index < bytes.size(); ++index) wanted |= static_cast<std::uint64_t>(bytes[index] >> 7) << index;
    const std::string where = "byte mask dst " + std::to_string(dst) + " source " + std::to_string(source);
    require(machine.Memory[0] == wanted, where + ": the mask is wrong");
    require((machine.Memory[2] & LoweringExecution::kFlagMask) == (flags & LoweringExecution::kFlagMask), where + ": RFLAGS changed");
    require(machine.Memory[3] == (dst == 0 ? wanted : reinterpret_cast<std::uint64_t>(machine.Memory.data())), where + ": rax was not restored");
    for (std::uint8_t reg = 0; reg < 16; ++reg) require(machine.Ymm[reg] == start.Ymm[reg], where + ": ymm" + std::to_string(reg) + " changed");
    for (const auto entry : machine.RedZone) require(entry == LoweringExecution::kSentinel, where + ": the red zone was overwritten");
}

void check(const Form& form, std::uint8_t dst, std::uint8_t first, std::uint8_t second, int imm) {
    const auto instruction = site(form, true, dst, first, second, imm);
    const Runner runner(lowered(instruction));
    auto machine = VectorExecution::Initial();
    const auto start = machine;
    runner.Run(machine);
    const bool unary = form.Shape == LaneSplitShape::Unary || form.Shape == LaneSplitShape::UnaryImmediate;
    const auto wanted = described(form.Shape) ? wantedResult(form, start, first, start.Ymm[second], imm) : perHalf(form, start, unary ? second : first, second, imm, false);
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        if (machine.Ymm[reg] != expected)
            throw std::runtime_error("lane split of opcode " + std::to_string(form.Opcode) + " map " + std::to_string(form.Map) + " dst " + std::to_string(dst) + " first " + std::to_string(first) + " second " + std::to_string(second) + ": ymm" + std::to_string(reg) + " is wrong");
    }
    for (const auto slot : machine.RedZone) require(slot == LoweringExecution::kSentinel, "a lane split overwrote the red zone");
}

void checkMemory(const Form& form, std::uint8_t dst, std::uint8_t first, int imm) {
    const Runner runner(lowered(memorySite(form, dst, first, imm)));
    auto machine = VectorExecution::Initial();
    const auto start = machine;
    runner.Run(machine);
    const bool unary = form.Shape == LaneSplitShape::Unary || form.Shape == LaneSplitShape::UnaryImmediate;
    const auto wanted = described(form.Shape) ? wantedResult(form, start, first, start.Memory, imm) : perHalf(form, start, unary ? 0 : first, 0, imm, true);
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == dst ? wanted : start.Ymm[reg];
        require(machine.Ymm[reg] == expected, "a lane split with a memory source left ymm" + std::to_string(reg) + " wrong");
    }
    require(machine.Memory == start.Memory, "a lane split changed its memory source");
    for (const auto slot : machine.RedZone) require(slot == LoweringExecution::kSentinel, "a lane split with a memory source overwrote the red zone");
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
    for (const auto& form : forms()) {
        if (form.Shape == LaneSplitShape::ShiftImmediate) continue;
        for (const auto& registers : std::array<std::array<std::uint8_t, 3>, 7>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}, {8, 12, 9}, {13, 15, 14}}}) {
            for (const int imm : {0x00, 0x1B, 0x05, 0x88}) {
                if (!immediate(form.Shape) && imm != 0x00) continue;
                checkMemory(form, registers[0], registers[1], imm);
            }
        }
    }
    const Form difference{LaneSplitShape::LaneImmediate, 3, 1, 0x42, 0};
    const Form blend{LaneSplitShape::Blend, 3, 1, 0x4C, 0};
    for (int imm = 0; imm < 256; ++imm) {
        check(difference, 0, 1, 2, imm);
        check(difference, 9, 9, 12, imm);
        check(difference, 15, 3, 15, imm);
    }
    for (const auto& registers : std::array<std::array<std::uint8_t, 3>, 5>{{{0, 1, 2}, {3, 3, 4}, {5, 6, 5}, {7, 7, 7}, {15, 14, 13}}})
        for (std::uint8_t mask = 0; mask < 16; ++mask)
            check(blend, registers[0], registers[1], registers[2], (mask << 4) | (mask & 5));
    for (const int imm : {0x00, 0x2A, 0x15, 0x3F}) checkStackSource(difference, 2, 5, imm);
    for (const int mask : {0x10, 0x20, 0x50, 0xF0}) checkStackSource(blend, 1, 2, mask);
    const auto initial = VectorExecution::Initial();
    const std::array<Value, 6> inputs{{initial.Ymm[3], initial.Ymm[11],
        {0x8080808080808080ull, 0x8080808080808080ull, 0, 0}, {0, 0, 0x8080808080808080ull, 0x8080808080808080ull},
        {0x7F7F7F7F7F7F7F7Full, 0x7F7F7F7F7F7F7F7Full, 0x7F7F7F7F7F7F7F7Full, 0x7F7F7F7F7F7F7F7Full}, {0x80ull, 0, 0, 0x8000000000000000ull}}};
    for (const int dst : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15})
        for (const int source : {0, 7, 14, 15})
            for (const auto& input : inputs)
                for (const std::uint32_t flags : {0x202u, 0xAD7u})
                    checkMask(static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(source), input, flags);
}

#else

void execution() { std::cout << "lane split execution checks require an x86-64 Linux, macOS or Windows build\n"; }

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
