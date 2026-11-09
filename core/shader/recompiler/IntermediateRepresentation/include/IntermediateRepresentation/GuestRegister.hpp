#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_GUESTREGISTER_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_GUESTREGISTER_HPP

#include <cstdint>

namespace ShaderRecompiler {

enum class ScalarReg : std::uint16_t {};
enum class VectorReg : std::uint16_t {};

inline constexpr std::uint32_t NumScalarRegs = 106;
inline constexpr std::uint32_t NumVectorRegs = 256;

enum class RegisterBank {
    Scalar,
    Vector,
    VectorConditionCode,
    ScalarConditionCode,
    ExecutionMask,
    Memory,
    ThreadBitScalar,
    ScalarMaskTag,
    GotoVariable,
    M0,
    UserData
};

struct GuestRegister {
    RegisterBank bank;
    std::uint32_t index;
};

[[nodiscard]] bool operator==(const GuestRegister& lhs, const GuestRegister& rhs);
[[nodiscard]] bool operator<(const GuestRegister& lhs, const GuestRegister& rhs);

[[nodiscard]] std::uint32_t RegIndex(ScalarReg reg);
[[nodiscard]] std::uint32_t RegIndex(VectorReg reg);

}

#endif
