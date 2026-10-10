#ifndef CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_CONTROLFLOWHELPERS_HPP
#define CORE_SHADER_RECOMPILIER_CONTROLFLOW_INCLUDE_CONTROLFLOW_CONTROLFLOWHELPERS_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ShaderRecompiler {

std::string toHexString(std::uint32_t value);
bool contains(const std::vector<std::uint32_t>& values, std::uint32_t value);
std::vector<std::uint32_t> intersectSorted(const std::vector<std::uint32_t>& first, const std::vector<std::uint32_t>& second);
void addUnique(std::vector<std::uint32_t>& values, std::uint32_t value);
void sortUnique(std::vector<std::uint32_t>& values);
std::uint32_t remapId(std::uint32_t id, const std::vector<std::uint32_t>& idMap);
void remapIds(std::vector<std::uint32_t>& values, const std::vector<std::uint32_t>& idMap);
void rebuildPredecessors(ControlFlowGraph& graph);

}

#endif
