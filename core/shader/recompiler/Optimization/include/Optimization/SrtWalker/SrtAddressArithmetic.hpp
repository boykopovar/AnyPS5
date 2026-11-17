#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTADDRESSARITHMETIC_HPP

#include <cstdint>

namespace ShaderRecompiler::Detail {

inline constexpr std::uint64_t AddressMask = 0x0000ffffffffffffull;

inline bool AddSignedAddress(std::uint64_t base, std::int64_t offset, std::uint64_t& result) {
    if (base > AddressMask) {
        return false;
    }
    if (offset < 0) {
        const auto magnitude = std::uint64_t {0} - static_cast<std::uint64_t>(offset);
        if (magnitude > base) {
            return false;
        }
        result = base - magnitude;
        return true;
    }
    const auto magnitude = static_cast<std::uint64_t>(offset);
    if (magnitude > AddressMask - base) {
        return false;
    }
    result = base + magnitude;
    return true;
}

}

#endif
