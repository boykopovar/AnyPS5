#ifndef CODEGEN_X86_BLSLOWERING_HPP
#define CODEGEN_X86_BLSLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class BlsOperation : std::uint8_t {
    Reset = 1,
    Mask = 2,
    Isolate = 3,
};

struct BlsOperands {
    BlsOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* BlsName(BlsOperation operation);
[[nodiscard]] std::optional<BlsOperands> DecodeBls(const std::uint8_t* data, std::size_t length);
void EmitBls(StubBodyBuilder& body, const BlsOperands& operands);

}

#endif
