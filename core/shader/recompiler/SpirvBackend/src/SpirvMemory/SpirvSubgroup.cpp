#include "SpirvBackend/SpirvEmitter.hpp"
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <SpirvBackend/SpirvEmitterInstructions.hpp>
#include <SpirvBackend/SpirvEmitterState.hpp>
#include <SpirvBackend/SpirvMemory/SpirvConstants.hpp>
#include <SpirvBackend/SpirvMemory/SpirvTypes.hpp>

namespace ShaderRecompiler
{
    struct SpirvEmitterState;

    namespace {

        [[noreturn]] void FailEmit(const std::string& reason) {
            throw std::runtime_error("SPIR-V module emission failed: " + reason);
        }

        std::uint32_t HostInvocationId(SpirvEmitterState& state) {
            if (state.subgroupLocalInvocationIdVariable == 0) {
                FailEmit("SubgroupLocalInvocationId was not declared before function emission");
            }
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpLoad, TypeU32(state), value, state.subgroupLocalInvocationIdVariable);
            return value;
        }

    }

    std::uint32_t EmitSubgroupLocalInvocationId(SpirvEmitterState& state) {
        const auto value = HostInvocationId(state);
        if (state.splitSubgroup) return EmitBinaryU32(state, spv::OpBitwiseAnd, value, ConstantU32(state, 31u));
        return state.laneHalf == 0 ? value : EmitAddU32(state, value, ConstantU32(state, 32));
    }

    std::uint32_t EmitHostSubgroupLane(SpirvEmitterState& state, std::uint32_t lane) {
        if (!state.splitSubgroup) return lane;
        const auto base = EmitBinaryU32(state, spv::OpBitwiseAnd, HostInvocationId(state), ConstantU32(state, ~31u));
        return EmitAddU32(state, base, lane);
    }

    std::uint32_t EmitWaveBallot(SpirvEmitterState& state, std::uint32_t ballot) {
        if (!state.splitSubgroup) return ballot;
        const auto word = EmitBinaryU32(state, spv::OpShiftRightLogical, HostInvocationId(state), ConstantU32(state, 5u));
        const auto mask = state.module.AllocateId();
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpVectorExtractDynamic, TypeU32(state), mask, ballot, word);
        state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4u), result, mask, ConstantU32(state, 0u), ConstantU32(state, 0u), ConstantU32(state, 0u));
        return result;
    }

    DppTargetLane EmitDppGroupPermTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t control, std::uint32_t laneBits) {
        const std::uint32_t laneMask = (1u << laneBits) - 1u;
        const auto groupBase = state.module.AllocateId();
        const auto lane = state.module.AllocateId();
        const auto shift = state.module.AllocateId();
        const auto selectedRaw = state.module.AllocateId();
        const auto selected = state.module.AllocateId();
        const auto target = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), groupBase, subid, ConstantU32(state, ~laneMask));
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, laneMask));
        state.module.AddFunction(spv::OpIMul, TypeU32(state), shift, lane, ConstantU32(state, laneBits));
        state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), selectedRaw, ConstantU32(state, control), shift);
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), selected, selectedRaw, ConstantU32(state, laneMask));
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, groupBase, selected);
        return {target, ConstantBool(state, true)};
    }

    DppTargetLane EmitDppRowShiftTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount, bool left) {
        const auto row = state.module.AllocateId();
        const auto lane = state.module.AllocateId();
        const auto laneShifted = state.module.AllocateId();
        const auto target = state.module.AllocateId();
        const auto valid = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid, ConstantU32(state, 0xfffffff0u));
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, 15));
        if (left) {
            state.module.AddFunction(spv::OpIAdd, TypeU32(state), laneShifted, lane, ConstantU32(state, amount));
            state.module.AddFunction(spv::OpULessThan, TypeBool(state), valid, lane, ConstantU32(state, 16u - amount));
        } else {
            state.module.AddFunction(spv::OpISub, TypeU32(state), laneShifted, lane, ConstantU32(state, amount));
            state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), valid, lane, ConstantU32(state, amount));
        }
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row, laneShifted);
        return {target, valid};
    }

    DppTargetLane EmitDppRowRotateRightTargetLane(SpirvEmitterState& state, std::uint32_t subid, std::uint32_t amount) {
        const auto row = state.module.AllocateId();
        const auto lane = state.module.AllocateId();
        const auto inHigh = state.module.AllocateId();
        const auto minus = state.module.AllocateId();
        const auto plus = state.module.AllocateId();
        const auto selected = state.module.AllocateId();
        const auto target = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid, ConstantU32(state, 0xfffffff0u));
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, 15));
        state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), inHigh, lane, ConstantU32(state, amount));
        state.module.AddFunction(spv::OpISub, TypeU32(state), minus, lane, ConstantU32(state, amount));
        state.module.AddFunction(spv::OpIAdd, TypeU32(state), plus, lane, ConstantU32(state, 16u - amount));
        state.module.AddFunction(spv::OpSelect, TypeU32(state), selected, inHigh, minus, plus);
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row, selected);
        return {target, ConstantBool(state, true)};
    }

    DppTargetLane EmitDppMirrorTargetLane(SpirvEmitterState& state, std::uint32_t subid, bool halfRow) {
        const std::uint32_t baseMask = halfRow ? 0xfffffff8u : 0xfffffff0u;
        const std::uint32_t laneMask = halfRow ? 7u : 15u;
        const auto base = state.module.AllocateId();
        const auto lane = state.module.AllocateId();
        const auto mirrored = state.module.AllocateId();
        const auto target = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), base, subid, ConstantU32(state, baseMask));
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, laneMask));
        state.module.AddFunction(spv::OpISub, TypeU32(state), mirrored, ConstantU32(state, laneMask), lane);
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, base, mirrored);
        return {target, ConstantBool(state, true)};
    }

    DppTargetLane EmitDppTargetLane(SpirvEmitterState& state, std::uint32_t control) {
        const auto subid = EmitSubgroupLocalInvocationId(state);
        if ((control & DppMoveFlags::Lanes8) != 0u) {
            return EmitDppGroupPermTargetLane(state, subid, control & ~DppMoveFlags::Lanes8, 3u);
        }
        if (control <= 0xffu) {
            return EmitDppGroupPermTargetLane(state, subid, control, 2u);
        }
        if (control >= 0x101u && control <= 0x10fu) {
            return EmitDppRowShiftTargetLane(state, subid, control & 0xfu, true);
        }
        if (control >= 0x111u && control <= 0x11fu) {
            return EmitDppRowShiftTargetLane(state, subid, control & 0xfu, false);
        }
        if (control >= 0x121u && control <= 0x12fu) {
            return EmitDppRowRotateRightTargetLane(state, subid, control & 0xfu);
        }
        if (control == 0x140u) {
            return EmitDppMirrorTargetLane(state, subid, false);
        }
        if (control == 0x141u) {
            return EmitDppMirrorTargetLane(state, subid, true);
        }
        if (control >= 0x150u && control <= 0x15fu) {
            const auto row = state.module.AllocateId();
            const auto target = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid, ConstantU32(state, 0xfffffff0u));
            state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row, ConstantU32(state, control & 0xfu));
            return {target, ConstantBool(state, true)};
        }
        if (control >= 0x160u && control <= 0x16fu) {
            const auto target = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitwiseXor, TypeU32(state), target, subid, ConstantU32(state, control & 0xfu));
            return {target, ConstantBool(state, true)};
        }
        FailEmit("DPP control " + std::to_string(control) + " is not supported");
    }

    std::uint32_t EmitBallotLaneActiveBool(SpirvEmitterState& state, std::uint32_t ballot, std::uint32_t lane) {
        const auto waveSize = state.program.WaveSize();
        const auto low = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0u);
        std::uint32_t mask = low;
        if (waveSize == 64u) {
            const auto high = state.module.AllocateId();
            const auto inHigh = state.module.AllocateId();
            const auto selected = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1u);
            state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), inHigh, lane, ConstantU32(state, 32));
            state.module.AddFunction(spv::OpSelect, TypeU32(state), selected, inHigh, high, low);
            mask = selected;
        }
        const auto laneLow = state.module.AllocateId();
        const auto bit = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), laneLow, lane, ConstantU32(state, 31));
        state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), bit, ConstantU32(state, 1), laneLow);
        const auto hit = state.module.AllocateId();
        const auto active = state.module.AllocateId();
        const auto inRange = state.module.AllocateId();
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), hit, mask, bit);
        state.module.AddFunction(spv::OpINotEqual, TypeBool(state), active, hit, ConstantU32(state, 0));
        state.module.AddFunction(spv::OpULessThan, TypeBool(state), inRange, lane, ConstantU32(state, waveSize));
        state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), result, active, inRange);
        return result;
    }

    std::uint32_t EmitSubgroupLaneActiveBool(SpirvEmitterState& state, std::uint32_t lane) {
        const auto activeBallot = state.module.AllocateId();
        state.module.AddFunction(spv::OpGroupNonUniformBallot, TypeU32Vector(state, 4), activeBallot, ConstantU32(state, spv::ScopeSubgroup), ConstantBool(state, true));
        return EmitBallotLaneActiveBool(state, EmitWaveBallot(state, activeBallot), lane);
    }
}
