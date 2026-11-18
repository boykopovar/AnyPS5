#ifndef CODEGEN_X86_SHIFTLOWERING_HPP
#define CODEGEN_X86_SHIFTLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class ShiftOperation : std::uint8_t {
    Left,
    LogicalRight,
    ArithmeticRight,
    RotateRight,
};

struct ShiftOperands {
    ShiftOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Count;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* ShiftName(ShiftOperation operation);
[[nodiscard]] std::optional<ShiftOperands> DecodeShift(const std::uint8_t* data, std::size_t length);
void EmitShift(StubBodyBuilder& body, const ShiftOperands& operands);

}

#endif
