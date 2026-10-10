#include "Optimization/HostInterpolationChecker.hpp"
#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/MaskedSelectEliminator.hpp"
#include <array>
#include <vector>

namespace ShaderRecompiler {
namespace {

bool isCopy(const IrValue& value) {
    return value.IsIdentity() || value.Opcode() == IrOpcode::BitCastF32U32 || value.Opcode() == IrOpcode::BitCastU32F32;
}

const IrValue* lookThrough(const IrValue* value) {
    value = value->Resolve();
    while (value->Opcode() == IrOpcode::BitCastF32U32 || value->Opcode() == IrOpcode::BitCastU32F32) value = value->Argument(0)->Resolve();
    return value;
}

const IrValue* partialOf(const IrValue& interpolation) {
    const auto* exec = interpolation.Argument(4)->Resolve();
    const auto* value = lookThrough(interpolation.Argument(3));
    while (value->Opcode() == IrOpcode::SelectU32) {
        if (value->Argument(0)->Resolve() != exec) return value;
        value = lookThrough(value->Argument(1));
    }
    return value;
}

bool isBarycentric(const IrValue& value) {
    if (value.Opcode() != IrOpcode::GetBuiltin) return false;
    const auto kind = static_cast<StageInputKind>(value.Argument(0)->Resolve()->ImmediateU32());
    return kind == StageInputKind::BaryCoordSmooth || kind == StageInputKind::BaryCoordNoPerspective;
}

bool isBarycentric(const IrValue& value, StageInputKind kind, std::uint32_t component) {
    return isBarycentric(value) && static_cast<StageInputKind>(value.Argument(0)->Resolve()->ImmediateU32()) == kind && value.Argument(1)->Resolve()->ImmediateU32() == component;
}

template<typename TAccept>
bool usesAll(const IrValue& value, const TAccept& accept) {
    for (const auto& use : value.OperandUses()) {
        if (isCopy(*use.user)) {
            if (!usesAll(*use.user, accept)) return false;
        } else if (!accept(*use.user, use.operand)) {
            return false;
        }
    }
    return true;
}

bool sameCondition(const IrValue& left, const IrValue& right) {
    return left.Argument(0)->Resolve() == right.Argument(0)->Resolve();
}

bool partialSelect(const IrValue& select) {
    return usesAll(select, [&](const IrValue& user, std::size_t operand) {
        return (user.Opcode() == IrOpcode::InterpolateHostP2 && operand == 3u) || (user.Opcode() == IrOpcode::SelectU32 && operand == 2u && sameCondition(user, select));
    });
}

bool sameParameter(const IrValue& left, const IrValue& right) {
    return left.Argument(0)->Resolve()->ImmediateU32() == right.Argument(0)->Resolve()->ImmediateU32() && left.Argument(1)->Resolve()->ImmediateU32() == right.Argument(1)->Resolve()->ImmediateU32();
}

bool exact(const IrValue& inst, const IrProgram& program, const ShaderPixelInputInfo& pixel, std::vector<IrValue*>& selects) {
    if (inst.Opcode() == IrOpcode::InterpolateHostP1) {
        return usesAll(inst, [&](const IrValue& user, std::size_t operand) {
            if (user.Opcode() == IrOpcode::InterpolateHostP2 && operand == 3u) return true;
            if (user.Opcode() != IrOpcode::SelectU32 || operand != 1u || !partialSelect(user)) return false;
            selects.push_back(const_cast<IrValue*>(&user));
            return true;
        });
    }
    if (inst.Opcode() == IrOpcode::GetInterpolationParameter) {
        const auto input = inst.Argument(0)->Resolve()->ImmediateU32();
        return pixel.InputIsDefault(input) || (inst.Argument(2)->Resolve()->ImmediateU32() == 2u && pixel.InputIsFlat(input));
    }
    if (inst.Opcode() != IrOpcode::InterpolateHostP2) return true;
    const auto* partial = partialOf(inst);
    if (partial->Opcode() != IrOpcode::InterpolateHostP1 || !sameParameter(inst, *partial)) return false;
    const auto& metadata = program.Metadata();
    const auto kind = pixel.InputIsLinear(inst.Argument(0)->Resolve()->ImmediateU32(), metadata.pixelLinearInputs, metadata.pixelPerspectiveInputs) ? StageInputKind::BaryCoordNoPerspective : StageInputKind::BaryCoordSmooth;
    return isBarycentric(*lookThrough(partial->Argument(2)), kind, 0u) && isBarycentric(*lookThrough(inst.Argument(2)), kind, 1u);
}

bool sharesSlots(const IrProgram& program, const ShaderPixelInputInfo& pixel) {
    constexpr std::uint32_t unassigned = 3u;
    std::array<std::uint32_t, 32> interpolation {};
    interpolation.fill(unassigned);
    const auto& metadata = program.Metadata();
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::GetAttribute && inst->Opcode() != IrOpcode::GetInterpolationParameter) continue;
            const auto input = inst->Argument(0)->Resolve()->ImmediateU32();
            if (input >= pixel.inputNum || pixel.InputIsDefault(input)) continue;
            const auto mode = pixel.InputIsFlat(input) ? 2u : pixel.InputIsLinear(input, metadata.pixelLinearInputs, metadata.pixelPerspectiveInputs) ? 1u : 0u;
            auto& slot = interpolation[pixel.InputSlot(input)];
            if (slot != unassigned && slot != mode) return true;
            slot = mode;
        }
    }
    return false;
}

}

bool HostInterpolationChecker::Lower(IrProgram& program, const ShaderPixelInputInfo& pixel) const {
    std::vector<IrValue*> selects;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (!exact(*inst, program, pixel, selects)) return false;
        }
    }
    if (sharesSlots(program, pixel)) return false;
    for (auto* select : selects) select->ReplaceAllUsesWith(select->Argument(2));
    for (const auto& block : program.Blocks()) {
        auto& instructions = block->Instructions();
        for (auto it = instructions.begin(); it != instructions.end();) {
            IrValue* inst = *it;
            if (inst->Opcode() != IrOpcode::InterpolateHostP2) {
                ++it;
                continue;
            }
            inst->Invalidate();
            inst->SetParent(nullptr);
            it = instructions.erase(it);
        }
    }
    DeadCodeEliminator{}.Eliminate(program);
    if (MaskedSelectEliminator{}.Eliminate(program).removedSelects != 0u) DeadCodeEliminator{}.Eliminate(program);
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (isBarycentric(*inst) || inst->Opcode() == IrOpcode::InterpolateHostP1) return false;
        }
    }
    return true;
}

}
