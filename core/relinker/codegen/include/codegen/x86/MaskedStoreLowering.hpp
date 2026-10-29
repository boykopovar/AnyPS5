#ifndef CODEGEN_X86_MASKEDSTORELOWERING_HPP
#define CODEGEN_X86_MASKEDSTORELOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Codegen {

struct MaskedStoreOperands {
    const char* Name;
    std::uint8_t ElementSize;
    bool Wide;
    std::uint8_t Mask;
    MemoryOperand Memory;
    std::vector<std::uint8_t> Instruction;
    std::size_t DisplacementOffset;
};

[[nodiscard]] std::optional<MaskedStoreOperands> DecodeMaskedStore(const std::uint8_t* data, std::size_t length);
void EmitMaskedStore(StubBodyBuilder& body, const MaskedStoreOperands& operands);

}

#endif
