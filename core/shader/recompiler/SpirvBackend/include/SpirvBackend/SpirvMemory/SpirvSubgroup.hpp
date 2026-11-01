#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVSUBGROUP_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVSUBGROUP_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

bool HostSubgroupSingleLane(const SpirvEmitterState& state);
void EmitSubgroupBallot(SpirvEmitterState& state, std::uint32_t result, std::uint32_t predicate);
void EmitHostSubgroupBallot(SpirvEmitterState& state, std::uint32_t result, std::uint32_t predicate);
void EmitSubgroupBallotFindLsb(SpirvEmitterState& state, std::uint32_t result, std::uint32_t ballot);
void EmitSubgroupShuffle(SpirvEmitterState& state, std::uint32_t type, std::uint32_t result, std::uint32_t value, std::uint32_t lane);
void EmitSubgroupShuffleInHalf(SpirvEmitterState& state, std::uint32_t type, std::uint32_t result, std::uint32_t value, std::uint32_t lane);
void EmitSubgroupElect(SpirvEmitterState& state, std::uint32_t result);
std::uint32_t EmitSubgroupLocalInvocationId(SpirvEmitterState& state);
std::uint32_t EmitHostSubgroupLane(SpirvEmitterState& state, std::uint32_t lane);
std::uint32_t EmitWaveBallot(SpirvEmitterState& state, std::uint32_t ballot);
DppTargetLane EmitDppGroupPermTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control, std::uint32_t laneBits);
DppTargetLane EmitDppRowShiftTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount, bool left);
DppTargetLane EmitDppRowRotateRightTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount);
DppTargetLane EmitDppMirrorTargetLane(SpirvEmitterState& state, std::uint32_t subid, bool halfRow);
DppTargetLane EmitDppTargetLane(SpirvEmitterState& state, std::uint32_t control);
bool DppQuadRead(const SpirvEmitterState& state, std::uint32_t control);
std::uint32_t EmitDppQuadRead(SpirvEmitterState& state, std::uint32_t type, std::uint32_t value, std::uint32_t control);
std::uint32_t EmitBallotLaneActiveBool(SpirvEmitterState& state, std::uint32_t ballot, std::uint32_t lane);
std::uint32_t EmitSubgroupLaneActiveBool(SpirvEmitterState& state, std::uint32_t lane);

}

#endif
