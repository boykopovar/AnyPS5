#ifndef CODEGEN_X86_SHAREDSTUBDATA_HPP
#define CODEGEN_X86_SHAREDSTUBDATA_HPP

#include <codegen/CodegenTypes.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Codegen {

enum class SharedConstant : std::uint8_t {
    Index12,
    Index13,
    Bit12,
    ExponentMask,
    ReciprocalBias,
    SquareRootBias,
    Sign,
    Magnitude,
    ReciprocalOverflow,
    SmallestNormal,
    Infinity,
    QuietBit,
    NegativeSmallestNormal,
    DefaultNan,
    NegativeInfinity
};

inline constexpr std::size_t kReciprocalTableOffset = 0;
inline constexpr std::size_t kReciprocalSquareRootTableOffset = 4096 * 2;

[[nodiscard]] std::size_t SharedConstantOffset(SharedConstant constant);
[[nodiscard]] std::span<const std::uint8_t> SharedStubData();
[[nodiscard]] bool UsesSharedStubData(const std::vector<TrampolineSite>& sites);

}

#endif
