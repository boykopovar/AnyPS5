#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_TESSELLATIONLOWERING_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_TESSELLATIONLOWERING_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Recompiler.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

class TessellationConstantsRequired : public std::runtime_error {
public:
    TessellationConstantsRequired(std::uint32_t userDataWord, const std::string& reason) : std::runtime_error(reason), word(userDataWord) {}
    [[nodiscard]] std::uint32_t UserDataWord() const noexcept { return word; }

private:
    std::uint32_t word;
};

class TessellationLowering {
public:
    void Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation, const TessellationTargetLimits* limits = nullptr) const;
};

}

#endif
