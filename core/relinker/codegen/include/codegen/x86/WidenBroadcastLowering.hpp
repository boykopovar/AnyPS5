#ifndef CODEGEN_X86_WIDENBROADCASTLOWERING_HPP
#define CODEGEN_X86_WIDENBROADCASTLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

enum class WidenBroadcastKind : std::uint8_t {
    Widen,
    BroadcastDword,
    BroadcastQword,
    BroadcastByte,
    BroadcastWord,
};

struct WidenBroadcastOperands {
    const char* Name;
    WidenBroadcastKind Kind;
    std::uint8_t Opcode;
    bool Vector256;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t SourceBytes;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<WidenBroadcastOperands> DecodeWidenBroadcast(const std::uint8_t* data, std::size_t length);
void EmitWidenBroadcast(StubBodyBuilder& body, const WidenBroadcastOperands& operands);

}

#endif
