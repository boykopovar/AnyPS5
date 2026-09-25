#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTDESCRIPTOREVALUATION_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTDESCRIPTOREVALUATION_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <string>

namespace ShaderRecompiler::Detail {

// Why the last EvaluateRuntimeSourcesImpl call on this thread returned false.
const std::string& RuntimeSourceFailureReason();

bool EvaluateRuntimeSourcesImpl(const IrResourcePlan& program, std::span<const std::uint32_t> sources, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, bool evaluateFlat, std::span<const std::uint8_t> cleanFlatSlots, std::vector<std::uint8_t>& activeSources);

}

#endif
