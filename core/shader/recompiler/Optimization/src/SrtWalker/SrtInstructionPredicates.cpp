#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"

#include <algorithm>
#include <vector>

namespace ShaderRecompiler::Detail {

bool IsRawRead(const IrResourcePlan& program, const IrValue& inst) {
    const auto op = inst.Opcode();
    if (op != IrOpcode::LoadAddressU32 && op != IrOpcode::ReadConstBuffer) {
        return false;
    }
    const auto index = inst.Flags<MemoryFlags>().index;
    if (index >= program.memoryInfo.size()) {
        return false;
    }
    const auto kind = program.memoryInfo[index].kind;
    return (op == IrOpcode::LoadAddressU32 && kind == ResourceKind::ScalarAddress) || (op == IrOpcode::ReadConstBuffer && kind == ResourceKind::ScalarBuffer);
}

bool LoadedFromMemory(const IrValue& handle) {
    std::vector<const IrValue*> pending;
    std::vector<const IrValue*> visited;
    for (std::size_t index = 0; index < handle.ArgumentCount(); ++index) pending.push_back(handle.Argument(index));
    while (!pending.empty()) {
        const IrValue* value = pending.back();
        pending.pop_back();
        if (value == nullptr) continue;
        value = value->Resolve();
        if (value->HasImmediate() || std::find(visited.begin(), visited.end(), value) != visited.end()) continue;
        visited.push_back(value);
        const auto opcode = value->Opcode();
        if (opcode == IrOpcode::ReadConst || opcode == IrOpcode::LoadAddressU32 || opcode == IrOpcode::ReadConstBuffer) return true;
        for (std::size_t index = 0; index < value->ArgumentCount(); ++index) pending.push_back(value->Argument(index));
    }
    return false;
}

bool IsDescriptorHandle(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::GetBufferResource:
        case IrOpcode::GetAddressResource:
        case IrOpcode::GetImageResource:
        case IrOpcode::GetSamplerResource: return true;
        default: return false;
    }
}

bool IsRuntimeSelect(IrOpcode opcode) {
    return opcode == IrOpcode::SelectU1 || opcode == IrOpcode::SelectU32 || opcode == IrOpcode::SelectF32;
}

bool IsRuntimeUniformOp(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32:
        case IrOpcode::ConvertU32F32:
        case IrOpcode::ConvertF32U32:
        case IrOpcode::CompositeConstructU64:
        case IrOpcode::CompositeExtractU64:
        case IrOpcode::CompositeConstructU32x2:
        case IrOpcode::CompositeExtractU32x2:
        case IrOpcode::BitFieldInsert:
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract:
        case IrOpcode::IAdd32:
        case IrOpcode::IAdd64:
        case IrOpcode::IAddCarry32:
        case IrOpcode::ISub32:
        case IrOpcode::ISub64:
        case IrOpcode::IMul32:
        case IrOpcode::IMul64:
        case IrOpcode::UMin32:
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::ShiftLeftLogical64:
        case IrOpcode::ShiftRightLogical32:
        case IrOpcode::ShiftRightLogical64:
        case IrOpcode::ShiftRightArithmetic32:
        case IrOpcode::ShiftRightArithmetic64:
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseAnd64:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
        case IrOpcode::BitwiseNot32:
        case IrOpcode::SelectU1:
        case IrOpcode::SelectU32:
        case IrOpcode::SelectF32:
        case IrOpcode::ULessThan32:
        case IrOpcode::IEqual32:
        case IrOpcode::UGreaterThan32:
        case IrOpcode::INotEqual32:
        case IrOpcode::LogicalOr:
        case IrOpcode::LogicalAnd:
        case IrOpcode::LogicalXor:
        case IrOpcode::LogicalNot:
        case IrOpcode::FPOrdLessThanEqual32:
        case IrOpcode::FPOrdGreaterThanEqual32:
        case IrOpcode::FPIsNan32:
        case IrOpcode::FPMul32:
        case IrOpcode::FPTrunc32: return true;
        default: return false;
    }
}

}
