#ifndef CODEGEN_X86_GATHERLOWERING_HPP
#define CODEGEN_X86_GATHERLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

struct GatherOperands {
    const char* Name;
    std::uint8_t DataBytes;
    std::uint8_t IndexBytes;
    bool Vector256;
    std::uint8_t Destination;
    std::uint8_t Mask;
    std::uint8_t Index;
    std::optional<std::uint8_t> Base;
    std::uint8_t Scale;
    MemoryOperand Memory;
};

[[nodiscard]] std::optional<GatherOperands> DecodeGather(const std::uint8_t* data, std::size_t length);
void EmitGather(StubBodyBuilder& body, const GatherOperands& operands);

}

#endif
