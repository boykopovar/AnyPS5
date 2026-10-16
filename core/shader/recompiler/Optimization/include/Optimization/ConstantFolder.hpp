#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_CONSTANTFOLDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_CONSTANTFOLDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/IrValue.hpp"

#include <span>

namespace ShaderRecompiler {

class ConstantFolder {
public:
    void Fold(IrProgram& program) const;

    void Fold(IrProgram& program, std::span<IrBlock* const> blocks) const;

    [[nodiscard]] bool FoldVertexLaneBounds(IrProgram& program, std::uint32_t hostSubgroupSize, std::uint32_t spirvVersion) const;

    [[nodiscard]] bool ComposePackedAncillary(IrProgram& program) const;

private:
    [[nodiscard]] bool tryFoldValue(IrProgram& program, IrValue& value) const;
};

}

#endif
