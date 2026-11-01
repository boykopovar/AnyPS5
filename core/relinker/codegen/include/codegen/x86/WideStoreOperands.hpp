#ifndef CODEGEN_X86_WIDESTOREOPERANDS_HPP
#define CODEGEN_X86_WIDESTOREOPERANDS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Codegen {

struct WideStoreHalf {
    std::vector<std::uint8_t> Bytes;
    std::optional<std::size_t> RipDisplacementOffset;
    std::int64_t RipTarget = 0;
};

struct WideStoreOperands {
    WideStoreHalf Low;
    WideStoreHalf High;
};

[[nodiscard]] std::optional<WideStoreOperands> DecodeVexWideStore(const std::uint8_t* data, std::size_t length);

}

#endif
