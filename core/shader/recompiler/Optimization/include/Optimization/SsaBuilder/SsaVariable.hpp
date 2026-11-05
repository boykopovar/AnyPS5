#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAVARIABLE_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAVARIABLE_HPP

#include "IntermediateRepresentation/GuestRegister.hpp"
#include "IntermediateRepresentation/IrType.hpp"
#include <compare>
#include <cstdint>
#include <variant>

namespace ShaderRecompiler::Detail {

struct SccTag { auto operator<=>(const SccTag&) const = default; };
struct ThreadBitScalarReg { ScalarReg reg{}; auto operator<=>(const ThreadBitScalarReg&) const = default; };
struct ScalarMaskTag { ScalarReg reg{}; auto operator<=>(const ScalarMaskTag&) const = default; };
struct ExecTag { auto operator<=>(const ExecTag&) const = default; };
struct ExecLoTag { auto operator<=>(const ExecLoTag&) const = default; };
struct ExecHiTag { auto operator<=>(const ExecHiTag&) const = default; };
struct VccTag { auto operator<=>(const VccTag&) const = default; };
struct VccLoTag { auto operator<=>(const VccLoTag&) const = default; };
struct VccHiTag { auto operator<=>(const VccHiTag&) const = default; };
struct M0Tag { auto operator<=>(const M0Tag&) const = default; };
struct GotoVariable { std::uint32_t index = 0; auto operator<=>(const GotoVariable&) const = default; };

using Variable = std::variant<ScalarReg, ThreadBitScalarReg, ScalarMaskTag, VectorReg,
    GotoVariable, SccTag, ExecTag, ExecLoTag, ExecHiTag, VccTag, VccLoTag, VccHiTag, M0Tag>;

[[nodiscard]] IrType VariableType(const Variable& variable);

}

#endif
