#include <codegen/x86/VariableShiftLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr Form kShuffleDwords{Map0F, Prefix66, false, false, 0x70};
constexpr Form kZeroExtendDwords{Map0F38, Prefix66, false, false, 0x35};
constexpr Form kUnpackHighQwords{Map0F, Prefix66, false, false, 0x6D};
constexpr Form kBlendDwords{Map0F3A, Prefix66, false, false, 0x0C};
constexpr Form kBlendQwords{Map0F3A, Prefix66, false, false, 0x0D};
constexpr std::int32_t kRedZone = 128;
constexpr std::size_t kMaxScratch = 7;

struct VectorScratch {
    std::array<std::uint8_t, kMaxScratch> Registers{};
    std::size_t Count = 0;

    std::uint8_t Take(const std::array<bool, 16>& used) {
        for (std::uint8_t reg = 15;; --reg) {
            bool taken = used[reg];
            for (std::size_t index = 0; index < Count; ++index) taken = taken || Registers[index] == reg;
            if (!taken) {
                Registers[Count++] = reg;
                return reg;
            }
            if (reg == 0) throw CodegenException("variable shift has no scratch vector register");
        }
    }
};

std::uint8_t _uniformShift(const VariableShiftOperands& operands) {
    switch (operands.Operation) {
    case VariableShiftOperation::LeftLogical: return operands.Quadwords ? 0xF3 : 0xF2;
    case VariableShiftOperation::RightLogical: return operands.Quadwords ? 0xD3 : 0xD2;
    case VariableShiftOperation::RightArithmetic: return 0xE2;
    }
    throw CodegenException("unknown variable shift operation");
}

std::int32_t _frame(const VariableShiftOperands& operands, const std::int32_t spill) {
    const auto standard = kRedZone + spill;
    if (!operands.Memory || !UnindexedStack(*operands.Memory)) return standard;
    const auto displacement = static_cast<std::int64_t>(operands.Memory->Displacement);
    const std::int64_t size = operands.Vector256 ? 32 : 16;
    if (displacement >= spill - standard || displacement + size <= -standard) return standard;
    const auto needed = (-displacement + spill + 31) & ~std::int64_t{31};
    if (needed > std::numeric_limits<std::int32_t>::max())
        throw CodegenException(std::string(operands.Name) + " stack spill frame exceeds 32-bit displacement");
    return static_cast<std::int32_t>(needed);
}

void _half(StubBodyBuilder& body, const VariableShiftOperands& operands, const std::uint8_t result, const std::uint8_t source, const std::uint8_t count, const std::uint8_t lane, const std::uint8_t shifted) {
    const Form shift{Map0F, Prefix66, false, false, _uniformShift(operands)};
    if (operands.Quadwords) {
        Register(body, shift, result, source, count);
        Register(body, kUnpackHighQwords, lane, count, count);
        Register(body, shift, shifted, source, lane);
        Register(body, kBlendQwords, result, result, shifted, std::uint8_t{2});
        return;
    }
    Register(body, kZeroExtendDwords, lane, 0, count);
    Register(body, shift, result, source, lane);
    for (std::uint8_t element = 1; element < 4; ++element) {
        Register(body, kShuffleDwords, lane, 0, count, element);
        Register(body, kZeroExtendDwords, lane, 0, lane);
        Register(body, shift, shifted, source, lane);
        Register(body, kBlendDwords, result, result, shifted, static_cast<std::uint8_t>(1u << element));
    }
}

}

std::optional<VariableShiftOperands> DecodeVariableShift(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != Map0F38 || (data[vex + 2] & 3) != Prefix66) return std::nullopt;
    const bool quadwords = (data[vex + 2] & 0x80) != 0;
    const char* name = nullptr;
    VariableShiftOperation operation;
    switch (data[vex + 3]) {
    case 0x45:
        name = quadwords ? "VPSRLVQ" : "VPSRLVD";
        operation = VariableShiftOperation::RightLogical;
        break;
    case 0x46:
        if (quadwords) return std::nullopt;
        name = "VPSRAVD";
        operation = VariableShiftOperation::RightArithmetic;
        break;
    case 0x47:
        name = quadwords ? "VPSLLVQ" : "VPSLLVD";
        operation = VariableShiftOperation::LeftLogical;
        break;
    default:
        return std::nullopt;
    }
    const auto instruction = Decode(data, length);
    if (!instruction) return std::nullopt;
    if (instruction->End != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return VariableShiftOperands{name, operation, quadwords, instruction->Vector256, instruction->Reg, instruction->Vvvv, instruction->Rm, instruction->Memory};
}

void EmitVariableShift(StubBodyBuilder& body, const VariableShiftOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Source] = true;
    if (!operands.Memory) used[operands.Count] = true;
    VectorScratch scratch;
    const auto count = operands.Memory ? scratch.Take(used) : operands.Count;
    const auto low = scratch.Take(used);
    const auto lane = scratch.Take(used);
    const auto shifted = scratch.Take(used);
    std::uint8_t high = 0;
    std::uint8_t upperSource = 0;
    std::uint8_t upperCount = 0;
    if (operands.Vector256) {
        high = scratch.Take(used);
        upperSource = scratch.Take(used);
        upperCount = scratch.Take(used);
    }
    const auto spill = static_cast<std::int32_t>(scratch.Count * 32);
    const auto frame = _frame(operands, spill);
    StackSlot(body, 0x8D, true, Stack, -frame);
    for (std::size_t index = 0; index < scratch.Count; ++index)
        StackOperand(body, MoveUnaligned256Store, scratch.Registers[index], 0, static_cast<std::int32_t>(index * 32));
    if (operands.Memory)
        Memory(body, operands.Vector256 ? MoveUnaligned256Load : MoveUnaligned128Load, count, 0, *operands.Memory, frame, operands.Name);
    if (operands.Vector256) {
        ExtractUpper(body, upperSource, operands.Source);
        ExtractUpper(body, upperCount, count);
    }
    _half(body, operands, low, operands.Source, count, lane, shifted);
    if (operands.Vector256) {
        _half(body, operands, high, upperSource, upperCount, lane, shifted);
        InsertUpper(body, operands.Destination, low, high);
    } else {
        Register(body, MoveUnaligned128Load, operands.Destination, 0, low);
    }
    for (std::size_t index = 0; index < scratch.Count; ++index)
        StackOperand(body, MoveUnaligned256Load, scratch.Registers[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, frame);
}

}
