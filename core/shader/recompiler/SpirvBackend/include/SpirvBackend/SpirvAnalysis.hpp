#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVANALYSIS_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVANALYSIS_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {

struct SpirvRequirements {
    bool subgroupBallot = false;
    bool subgroupShuffle = false;
    bool subgroupLocalInvocationId = false;
    bool computeDerivatives = false;
    bool imageGatherExtended = false;
    bool functionLds = false;
    bool ldsLock = false;
    std::uint32_t functionLdsDwords = 0;
    std::unordered_map<const IrValue*, std::uint32_t> functionLdsAddresses;
    bool functionScratch = false;
    bool pixelValidMask = false;
    bool bufferInt64Atomics = false;
    bool imageInt64Atomics = false;
    bool sharedInt64Atomics = false;
    bool float64 = false;
    bool coherentBuffers = false;
    std::vector<std::uint32_t> capabilities;
    std::vector<std::string> extensions;
};

[[nodiscard]] SpirvRequirements AnalyzeProgramRequirements(const IrProgram& program);
inline constexpr std::uint32_t FunctionLdsDwordLimit = 8192u;
[[nodiscard]] std::uint32_t FunctionLdsDwords(const IrProgram& program);
[[nodiscard]] std::unordered_map<const IrValue*, std::uint32_t> FunctionLdsLaneAddresses(const IrProgram& program);
[[nodiscard]] std::unordered_set<const IrValue*> WaveUniformValues(const IrProgram& program);
[[nodiscard]] bool IsWaveMaskBranch(BranchCondition condition);

}

#endif
