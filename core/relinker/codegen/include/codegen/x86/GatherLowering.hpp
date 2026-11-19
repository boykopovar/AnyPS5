#ifndef CODEGEN_X86_GATHERLOWERING_HPP
#define CODEGEN_X86_GATHERLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

struct GatherOperands {
    const char* Name;
    bool Vector256;
    bool QwordIndex;
    bool QwordElement;
    std::uint8_t Destination;
    std::uint8_t Mask;
    std::uint8_t Index;
    MemoryOperand Memory;
};

[[nodiscard]] std::optional<GatherOperands> DecodeGather(const std::uint8_t* data, std::size_t length);
void EmitGather(StubBodyBuilder& body, const GatherOperands& operands);

}

#endif
