#ifndef CODEGEN_X86_LANESPLITLOWERING_HPP
#define CODEGEN_X86_LANESPLITLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

enum class LaneSplitShape : std::uint8_t {
    Binary,
    BinaryImmediate,
    ShiftImmediate,
    ShiftCount,
    Unary,
    UnaryImmediate,
};

struct LaneSplitOperands {
    LaneSplitShape Shape;
    std::uint8_t Map;
    std::uint8_t Prefix;
    bool Wide;
    std::uint8_t Opcode;
    std::uint8_t Destination;
    std::uint8_t First;
    std::uint8_t Second;
    std::uint8_t Extension;
    std::optional<std::uint8_t> Immediate;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<LaneSplitOperands> DecodeLaneSplit(const std::uint8_t* data, std::size_t length);
void EmitLaneSplit(StubBodyBuilder& body, const LaneSplitOperands& operands);

}

#endif
