#include "IntermediateRepresentation/GuestRegister.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

bool operator==(const GuestRegister& lhs, const GuestRegister& rhs) {
    return lhs.bank == rhs.bank && lhs.index == rhs.index;
}

bool operator<(const GuestRegister& lhs, const GuestRegister& rhs) {
    if (lhs.bank != rhs.bank) {
        return lhs.bank < rhs.bank;
    }
    return lhs.index < rhs.index;
}

std::uint32_t RegIndex(ScalarReg reg) {
    const auto index = static_cast<std::uint32_t>(reg);
    // if (index >= NumScalarRegs) {
    //     throw std::out_of_range("ScalarReg index is out of range");
    // }
    return index;
}

std::uint32_t RegIndex(VectorReg reg) {
    const auto index = static_cast<std::uint32_t>(reg);
    if (index >= NumVectorRegs) {
        throw std::out_of_range("VectorReg index is out of range");
    }
    return index;
}

}
