#ifndef CODEGEN_X86_DEPOSITEXTRACTLOWERING_HPP
#define CODEGEN_X86_DEPOSITEXTRACTLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class DepositExtractOperation : std::uint8_t {
    Deposit,
    Extract,
};

struct DepositExtractOperands {
    DepositExtractOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Mask;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* DepositExtractName(DepositExtractOperation operation);
[[nodiscard]] std::optional<DepositExtractOperands> DecodeDepositExtract(const std::uint8_t* data, std::size_t length);
void EmitDepositExtract(StubBodyBuilder& body, const DepositExtractOperands& operands);

}

#endif
