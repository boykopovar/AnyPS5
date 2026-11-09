#ifndef CODEGEN_X86_SCALARCOMPAREOPERANDS_HPP
#define CODEGEN_X86_SCALARCOMPAREOPERANDS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Codegen {

struct ScalarCompareOperands {
    std::optional<std::size_t> RipDisplacementOffset;
    std::int64_t RipTarget = 0;
};

[[nodiscard]] std::optional<ScalarCompareOperands> DecodeScalarCompare(const std::uint8_t* data, std::size_t length);

}

#endif
