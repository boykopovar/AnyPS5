#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SINGLELANEANALYSIS_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SINGLELANEANALYSIS_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"

namespace ShaderRecompiler {

struct SingleLaneVerdict {
    bool eligible = false;
    const char* reason = nullptr;
    const IrValue* at = nullptr;
};

[[nodiscard]] SingleLaneVerdict AnalyzeSingleLane(const IrProgram& program, const ShaderWorkgroupInputInfo& workgroup);
void RecordSkippedRegions(IrProgram& program);

}

#endif
