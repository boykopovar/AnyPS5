#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"

namespace ShaderRecompiler::Detail {

bool AddSignedAddress(std::uint64_t base, std::int64_t offset, std::uint64_t& result) {
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
