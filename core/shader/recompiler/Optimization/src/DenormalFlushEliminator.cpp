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

const IrValue* rightOperand(const IrValue* value, IrOpcode opcode, std::uint32_t bits) {
    if (value->Opcode() != opcode || value->ArgumentCount() != 2u) return nullptr;
    return isImmediate(value->Argument(1)->Resolve(), bits) ? value->Argument(0)->Resolve() : nullptr;
}

const IrValue* bitsOf(const IrValue* value) {
    value = value->Resolve();
    while ((value->Opcode() == IrOpcode::BitCastU32F32 || value->Opcode() == IrOpcode::BitCastF32U32) && value->ArgumentCount() == 1u) value = value->Argument(0)->Resolve();
    return value;
}

bool testsForNan(const IrValue* condition, const IrValue* bits) {
    condition = condition->Resolve();
    if (condition->Opcode() == IrOpcode::FPIsNan32 && condition->ArgumentCount() == 1u) return bitsOf(condition->Argument(0)) == bits;
    const IrValue* magnitude = rightOperand(condition, IrOpcode::UGreaterThan32, 0x7f800000u);
    if (magnitude != nullptr) magnitude = maskedOperand(magnitude, IrOpcode::BitwiseAnd32, 0x7fffffffu);
    return magnitude != nullptr && bitsOf(magnitude) == bits;
}

bool takenOnlyAsNan(const IrValue* condition, const IrValue* arm) {
    const IrValue* bits = bitsOf(arm);
    if (const IrValue* quieted = maskedOperand(bits, IrOpcode::BitwiseOr32, 0x00400000u)) bits = bitsOf(quieted);
    return testsForNan(condition, bits);
}

const IrValue* tinyGuardedProduct(const IrValue* condition, const IrValue* arm) {
    const IrValue* product = arm->Resolve();
    if (product->Opcode() != IrOpcode::FPMul32 || product->ArgumentCount() != 2u) return nullptr;
    const IrValue* offset = rightOperand(condition->Resolve(), IrOpcode::ULessThan32, 0x207fffffu);
    const IrValue* magnitude = offset != nullptr ? rightOperand(offset, IrOpcode::ISub32, 1u) : nullptr;
    const IrValue* scaled = magnitude != nullptr ? maskedOperand(magnitude, IrOpcode::BitwiseAnd32, 0x7fffffffu) : nullptr;
    if (scaled == nullptr) return nullptr;
    scaled = bitsOf(scaled);
    if (scaled->Opcode() != IrOpcode::FPMul32 || scaled->ArgumentCount() != 2u) return nullptr;
    const IrValue* lhs = product->Argument(0)->Resolve();
    const IrValue* rhs = product->Argument(1)->Resolve();
    for (std::size_t index = 0; index < 2u; ++index) {
        const IrValue* factor = maskedOperand(scaled->Argument(index)->Resolve(), IrOpcode::FPMul32, 0x5f800000u);
        const IrValue* other = scaled->Argument(1u - index)->Resolve();
        if (factor != nullptr && ((factor == lhs && other == rhs) || (factor == rhs && other == lhs))) return product;
    }
    return nullptr;
}

bool clearsSignificand(const IrValue& value) {
    for (std::size_t index = 0; index < value.ArgumentCount(); ++index) {
        const IrValue* mask = value.Argument(index)->Resolve();
        if (mask->HasImmediate() && (mask->ImmediateU32() & 0x007fffffu) == 0u) return true;
    }
    return false;
}

bool setsExponentBit(const IrValue& value) {
    if (value.ArgumentCount() != 2u) return false;
    for (std::size_t index = 0; index < 2u; ++index) {
        const IrValue* operand = value.Argument(index)->Resolve();
        if (operand->HasImmediate() && (operand->ImmediateU32() & 0x7f800000u) != 0u) return true;
    }
    return false;
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
            case IrOpcode::FPFloor32:
            case IrOpcode::FPCeil32:
            case IrOpcode::FPTrunc32:
            case IrOpcode::FPRoundEven32:
            case IrOpcode::FPSqrt:
            case IrOpcode::FPRecipSqrt32:
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
                return value.ArgumentCount() == 2u && clearsSignificand(value);
            case IrOpcode::BitwiseOr32:
                return setsExponentBit(value);
            case IrOpcode::SelectU32:
            case IrOpcode::SelectF32: {
                const IrValue* condition = value.Argument(0);
                const IrValue* product = tinyGuardedProduct(condition, value.Argument(2));
                const bool taken = takenOnlyAsNan(condition, value.Argument(1)) || Contains(value.Argument(1));
                return taken && (product != nullptr ? Contains(product->Argument(0)) && Contains(product->Argument(1)) : Contains(value.Argument(2)));
            }
            case IrOpcode::FPMin32:
            case IrOpcode::FPMax32:
            case IrOpcode::FPMinTri32:
            case IrOpcode::FPMaxTri32:
            case IrOpcode::FPMedTri32:
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
