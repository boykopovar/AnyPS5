#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_MASKEDSELECTELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_MASKEDSELECTELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <span>

namespace ShaderRecompiler {

struct MaskedSelectEliminationStats {
    std::uint32_t removedSelects = 0;
    std::uint64_t steps = 0;
};

class MaskedSelectEliminator {
public:
    [[nodiscard]] MaskedSelectEliminationStats Eliminate(IrProgram& program) const;
};

[[nodiscard]] bool GuardImplies(const IrProgram& program, const IrValue* condition, const IrValue* consequence);
[[nodiscard]] bool GuardImpliesMaskOnPath(const IrProgram& program, const IrValue* condition, const IrValue* low, const IrValue* high, std::span<const IrBlock* const> path);

}

#endif
