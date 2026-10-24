#ifndef CODEGEN_X86_ADXLOWERING_HPP
#define CODEGEN_X86_ADXLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>
#include <vector>

namespace Codegen {

struct AdxOperands {
    bool Overflow;
    bool Wide;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Rex;
    std::size_t OpcodeOffset;
    std::size_t ModRmOffset;
    std::vector<std::uint8_t> Instruction;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<AdxOperands> DecodeAdx(const std::uint8_t* data, std::size_t length);
void EmitAdx(StubBodyBuilder& body, const AdxOperands& operands);

}

#endif
