#include "Optimization/DenormalFlushEliminator.hpp"
#include <cstdint>
#include <unordered_map>

namespace ShaderRecompiler {

namespace {

bool isImmediate(const IrValue* value, std::uint32_t bits) {
    return value->HasImmediate() && value->ImmediateU32() == bits;
}

const IrValue* maskedOperand(const IrValue* value, IrOpcode opcode, std::uint32_t bits) {
    if (value->Opcode() != opcode || value->ArgumentCount() != 2u) return nullptr;
    const IrValue* lhs = value->Argument(0)->Resolve();
    const IrValue* rhs = value->Argument(1)->Resolve();
    if (isImmediate(rhs, bits)) return lhs;
    if (isImmediate(lhs, bits)) return rhs;
    return nullptr;
}

IrValue* flushedSource(const IrValue& value) {
    if (value.Opcode() != IrOpcode::SelectU32 || value.ArgumentCount() != 3u) return nullptr;
    IrValue* source = value.Argument(2)->Resolve();
    const IrValue* exponent = maskedOperand(value.Argument(0)->Resolve(), IrOpcode::IEqual32, 0u);
    if (exponent == nullptr || maskedOperand(exponent, IrOpcode::BitwiseAnd32, 0x7f800000u) != source) return nullptr;
    return maskedOperand(value.Argument(1)->Resolve(), IrOpcode::BitwiseAnd32, 0x80000000u) == source ? source : nullptr;
}

class DenormalFreeValues {
public:
    bool Contains(const IrValue* value) {
        value = value->Resolve();
        if (const auto found = known.find(value); found != known.end()) return found->second;
        known.emplace(value, false);
        const bool result = compute(*value);
        known[value] = result;
        return result;
    }

private:
    bool compute(const IrValue& value) {
        if (value.HasImmediate()) {
            const std::uint32_t bits = value.ImmediateU32();
            return (bits & 0x7f800000u) != 0u || (bits & 0x7fffffffu) == 0u;
        }
        if (flushedSource(value) != nullptr) return true;
        switch (value.Opcode()) {
            case IrOpcode::ConvertIToF32:
            case IrOpcode::ConvertUToF32:
            case IrOpcode::ConvertF32S32:
            case IrOpcode::ConvertF32U32:
            case IrOpcode::ConvertF32F16:
                return true;
            case IrOpcode::BitCastU32F32:
            case IrOpcode::BitCastF32U32:
            case IrOpcode::FPAbs32:
            case IrOpcode::FPNeg32:
            case IrOpcode::FPSaturate32:
                return Contains(value.Argument(0));
            case IrOpcode::BitwiseXor32:
                if (const IrValue* operand = maskedOperand(&value, IrOpcode::BitwiseXor32, 0x80000000u)) return Contains(operand);
                return false;
            case IrOpcode::BitwiseAnd32:
                if (const IrValue* operand = maskedOperand(&value, IrOpcode::BitwiseAnd32, 0x7fffffffu)) return Contains(operand);
                return false;
            case IrOpcode::SelectU32:
            case IrOpcode::SelectF32:
                return Contains(value.Argument(1)) && Contains(value.Argument(2));
            case IrOpcode::Phi:
                for (std::size_t index = 0; index < value.ArgumentCount(); ++index) {
                    if (!Contains(value.Argument(index))) return false;
                }
                return true;
            default:
                return false;
        }
    }

    std::unordered_map<const IrValue*, bool> known;
};

}

DenormalFlushEliminationStats DenormalFlushEliminator::Eliminate(IrProgram& program) const {
    DenormalFlushEliminationStats stats;
    DenormalFreeValues denormalFree;
    for (auto* block : program.BlockOrder()) {
        for (auto* inst : block->Instructions()) {
            if (!inst->HasUses()) continue;
            IrValue* source = flushedSource(*inst);
            if (source == nullptr || !denormalFree.Contains(source)) continue;
            inst->ReplaceAllUsesWith(source);
            ++stats.removedFlushes;
        }
    }
    return stats;
}

}
