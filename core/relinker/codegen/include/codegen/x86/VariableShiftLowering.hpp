#ifndef CODEGEN_X86_VARIABLESHIFTLOWERING_HPP
#define CODEGEN_X86_VARIABLESHIFTLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

struct VariableShiftOperands {
    const char* Name;
    std::uint8_t UniformOpcode;
    bool Quadword;
    bool Vector256;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Counts;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<VariableShiftOperands> DecodeVariableShift(const std::uint8_t* data, std::size_t length);
void EmitVariableShift(StubBodyBuilder& body, const VariableShiftOperands& operands);

}

#endif
