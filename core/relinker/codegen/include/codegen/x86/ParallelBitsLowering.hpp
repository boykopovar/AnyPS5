#ifndef CODEGEN_X86_PARALLELBITSLOWERING_HPP
#define CODEGEN_X86_PARALLELBITSLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class ParallelBitsOperation : std::uint8_t {
    Deposit,
    Extract,
};

struct ParallelBitsOperands {
    ParallelBitsOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Mask;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* ParallelBitsName(ParallelBitsOperation operation);
[[nodiscard]] std::optional<ParallelBitsOperands> DecodeParallelBits(const std::uint8_t* data, std::size_t length);
void EmitParallelBits(StubBodyBuilder& body, const ParallelBitsOperands& operands);

}

#endif
