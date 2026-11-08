#include "Optimization/NanPayloadEliminator.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::size_t ExplorationBudget = std::size_t{1} << 24u;
constexpr std::size_t TieLimit = 256u;
constexpr std::size_t MaxIterations = 16u;
constexpr std::uint32_t LowestNanMagnitude = 0x7f800001u;
constexpr std::uint32_t HighestNanMagnitude = 0x7fffffffu;
constexpr int ConditionDepth = 16;
constexpr int AlwaysNanDepth = 24;

enum class Difference : std::uint8_t { Equal, NanPayload, Arbitrary };

using ValueSet = std::unordered_set<const IrValue*>;

bool isImmediate(const IrValue* value, std::uint32_t bits) {
    return value->HasImmediate() && value->ImmediateU32() == bits;
}

const IrValue* otherImmediate(const IrValue& value, std::size_t operand) {
    if (value.ArgumentCount() != 2u || operand > 1u) return nullptr;
    const IrValue* other = value.Argument(1u - operand)->Resolve();
    return other->HasImmediate() ? other : nullptr;
}

bool otherOperandIs(const IrValue& value, std::size_t operand, std::uint32_t bits) {
    const IrValue* other = otherImmediate(value, operand);
    return other != nullptr && other->ImmediateU32() == bits;
}

std::optional<std::uint32_t> anyImmediate(const IrValue& value) {
    if (value.ArgumentCount() != 2u) return std::nullopt;
    for (std::size_t index = 0; index < 2u; ++index) {
        const IrValue* argument = value.Argument(index)->Resolve();
        if (argument->HasImmediate()) return argument->ImmediateU32();
    }
    return std::nullopt;
}

bool isBitCast(const IrValue* value) {
    return (value->Opcode() == IrOpcode::BitCastU32F32 || value->Opcode() == IrOpcode::BitCastF32U32) && value->ArgumentCount() == 1u;
}

const IrValue* withoutBitCasts(const IrValue* value) {
    value = value->Resolve();
    while (isBitCast(value)) value = value->Argument(0)->Resolve();
    return value;
}

const IrValue* maskedOperand(const IrValue* value, IrOpcode opcode, std::uint32_t bits) {
    if (value->Opcode() != opcode || value->ArgumentCount() != 2u) return nullptr;
    const IrValue* lhs = value->Argument(0)->Resolve();
    const IrValue* rhs = value->Argument(1)->Resolve();
    if (isImmediate(rhs, bits)) return lhs;
    if (isImmediate(lhs, bits)) return rhs;
    return nullptr;
}

const IrValue* nanTested(const IrValue* condition) {
    condition = condition->Resolve();
    if (condition->Opcode() == IrOpcode::FPIsNan32 && condition->ArgumentCount() == 1u) return withoutBitCasts(condition->Argument(0));
    if (condition->Opcode() != IrOpcode::UGreaterThan32 || condition->ArgumentCount() != 2u || !isImmediate(condition->Argument(1)->Resolve(), 0x7f800000u)) return nullptr;
    const IrValue* magnitude = maskedOperand(condition->Argument(0)->Resolve(), IrOpcode::BitwiseAnd32, 0x7fffffffu);
    return magnitude != nullptr ? withoutBitCasts(magnitude) : nullptr;
}

bool isSelect(const IrValue& value) {
    return (value.Opcode() == IrOpcode::SelectU32 || value.Opcode() == IrOpcode::SelectF32) && value.ArgumentCount() == 3u;
}

bool isNanImmediate(const IrValue* value) {
    return value->HasImmediate() && (value->Type() == IrType::U32 || value->Type() == IrType::F32) && (value->ImmediateU32() & 0x7fffffffu) > 0x7f800000u;
}

bool isOrderedCompare(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::FPOrdEqual32: case IrOpcode::FPOrdNotEqual32: case IrOpcode::FPOrdLessThan32:
        case IrOpcode::FPOrdGreaterThan32: case IrOpcode::FPOrdLessThanEqual32: case IrOpcode::FPOrdGreaterThanEqual32:
            return true;
        default:
            return false;
    }
}

bool isUnorderedCompare(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::FPUnordEqual32: case IrOpcode::FPUnordNotEqual32: case IrOpcode::FPUnordLessThan32:
        case IrOpcode::FPUnordGreaterThan32: case IrOpcode::FPUnordLessThanEqual32: case IrOpcode::FPUnordGreaterThanEqual32:
            return true;
        default:
            return false;
    }
}

bool isUnsignedCompare(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::ULessThan32: case IrOpcode::ULessThanEqual32: case IrOpcode::UGreaterThan32:
        case IrOpcode::UGreaterThanEqual32: case IrOpcode::IEqual32: case IrOpcode::INotEqual32:
            return true;
        default:
            return false;
    }
}

bool compareAt(IrOpcode opcode, std::uint32_t lhs, std::uint32_t rhs) {
    switch (opcode) {
        case IrOpcode::ULessThan32: return lhs < rhs;
        case IrOpcode::ULessThanEqual32: return lhs <= rhs;
        case IrOpcode::UGreaterThan32: return lhs > rhs;
        case IrOpcode::UGreaterThanEqual32: return lhs >= rhs;
        case IrOpcode::IEqual32: return lhs == rhs;
        case IrOpcode::INotEqual32: return lhs != rhs;
        default: return false;
    }
}

std::optional<bool> compareOverNanMagnitudes(IrOpcode opcode, std::size_t operand, std::uint32_t constant) {
    const auto at = [&](std::uint32_t magnitude) { return operand == 0u ? compareAt(opcode, magnitude, constant) : compareAt(opcode, constant, magnitude); };
    if (opcode == IrOpcode::IEqual32 || opcode == IrOpcode::INotEqual32) {
        if (constant >= LowestNanMagnitude && constant <= HighestNanMagnitude) return std::nullopt;
        return at(LowestNanMagnitude);
    }
    if (at(LowestNanMagnitude) != at(HighestNanMagnitude)) return std::nullopt;
    return at(LowestNanMagnitude);
}

bool propagatesNan(const IrValue& user, std::size_t operand) {
    switch (user.Opcode()) {
        case IrOpcode::Identity:
        case IrOpcode::BitCastU32F32:
        case IrOpcode::BitCastF32U32:
        case IrOpcode::FPAbs32:
        case IrOpcode::FPNeg32:
        case IrOpcode::FPAdd32:
        case IrOpcode::FPSub32:
        case IrOpcode::FPMul32:
        case IrOpcode::FPFract32:
        case IrOpcode::FPFloor32:
        case IrOpcode::FPCeil32:
        case IrOpcode::FPTrunc32:
        case IrOpcode::FPRoundEven32:
        case IrOpcode::FPSqrt:
        case IrOpcode::FPExp2:
            return true;
        case IrOpcode::BitwiseAnd32:
            return otherOperandIs(user, operand, 0x7fffffffu);
        case IrOpcode::BitwiseXor32:
            return otherOperandIs(user, operand, 0x80000000u);
        case IrOpcode::BitwiseOr32:
            return otherOperandIs(user, operand, 0x00400000u) || otherOperandIs(user, operand, 0x80000000u);
        default:
            return false;
    }
}

bool keepsNanPayloadDifference(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::Identity: case IrOpcode::Phi: case IrOpcode::BitCastU32F32: case IrOpcode::BitCastF32U32:
        case IrOpcode::FPAbs32: case IrOpcode::FPNeg32: case IrOpcode::FPAdd32: case IrOpcode::FPSub32: case IrOpcode::FPMul32:
        case IrOpcode::FPFract32: case IrOpcode::FPFloor32: case IrOpcode::FPCeil32: case IrOpcode::FPTrunc32: case IrOpcode::FPRoundEven32:
        case IrOpcode::FPSqrt: case IrOpcode::FPExp2:
        case IrOpcode::CompositeConstructU32x2: case IrOpcode::CompositeConstructU32x3: case IrOpcode::CompositeConstructU32x4: case IrOpcode::CompositeConstructF32x2:
            return true;
        default:
            return false;
    }
}

bool pureOperation(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::Identity: case IrOpcode::Phi: case IrOpcode::Select: case IrOpcode::SelectU1: case IrOpcode::SelectU32: case IrOpcode::SelectF32:
        case IrOpcode::BitCastU32F32: case IrOpcode::BitCastF32U32: case IrOpcode::BitCastU16F16: case IrOpcode::BitCastF16U16:
        case IrOpcode::IAdd32: case IrOpcode::ISub32: case IrOpcode::IMul32: case IrOpcode::INegate32: case IrOpcode::IAbs32:
        case IrOpcode::IAdd64: case IrOpcode::IAddCarry32: case IrOpcode::ISub64: case IrOpcode::IMul64:
        case IrOpcode::ShiftLeftLogical32: case IrOpcode::ShiftLeftLogical64: case IrOpcode::ShiftRightLogical32: case IrOpcode::ShiftRightLogical64:
        case IrOpcode::ShiftRightArithmetic32: case IrOpcode::ShiftRightArithmetic64:
        case IrOpcode::BitwiseAnd32: case IrOpcode::BitwiseAnd64: case IrOpcode::BitwiseOr32: case IrOpcode::BitwiseXor32: case IrOpcode::BitwiseNot32:
        case IrOpcode::BitReverse32: case IrOpcode::BitCount32: case IrOpcode::BitCount64: case IrOpcode::FindUMsb32: case IrOpcode::FindUMsb64: case IrOpcode::FindILsb32:
        case IrOpcode::BitFieldInsert: case IrOpcode::BitFieldUExtract: case IrOpcode::BitFieldSExtract:
        case IrOpcode::SMin32: case IrOpcode::UMin32: case IrOpcode::SMax32: case IrOpcode::UMax32:
        case IrOpcode::SMinTri32: case IrOpcode::UMinTri32: case IrOpcode::SMaxTri32: case IrOpcode::UMaxTri32:
        case IrOpcode::SLessThan32: case IrOpcode::SLessThan64: case IrOpcode::ULessThan32: case IrOpcode::ULessThan64: case IrOpcode::IEqual32: case IrOpcode::IEqual64:
        case IrOpcode::SLessThanEqual32: case IrOpcode::ULessThanEqual32: case IrOpcode::SGreaterThan32: case IrOpcode::UGreaterThan32: case IrOpcode::UGreaterThan64:
        case IrOpcode::INotEqual32: case IrOpcode::INotEqual64: case IrOpcode::SGreaterThanEqual32: case IrOpcode::UGreaterThanEqual32:
        case IrOpcode::LogicalOr: case IrOpcode::LogicalAnd: case IrOpcode::LogicalXor: case IrOpcode::LogicalNot:
        case IrOpcode::ConvertFToI32: case IrOpcode::ConvertIToF32: case IrOpcode::ConvertFToU32: case IrOpcode::ConvertUToF32:
        case IrOpcode::ConvertU16U32: case IrOpcode::ConvertU32U16: case IrOpcode::ConvertU8U32: case IrOpcode::ConvertU32U8:
        case IrOpcode::ConvertF32F16: case IrOpcode::ConvertF16F32: case IrOpcode::ConvertS32F32: case IrOpcode::ConvertU32F32: case IrOpcode::ConvertF32S32: case IrOpcode::ConvertF32U32:
        case IrOpcode::CompositeConstructU64: case IrOpcode::CompositeConstructU32x2: case IrOpcode::CompositeConstructU32x3: case IrOpcode::CompositeConstructF32x2: case IrOpcode::CompositeConstructU32x4:
        case IrOpcode::CompositeExtractU64: case IrOpcode::CompositeExtractU32x2: case IrOpcode::CompositeExtractU32x3: case IrOpcode::CompositeExtractU32x4:
        case IrOpcode::PackHalf2x16: case IrOpcode::PackFloat2x16Rtz:
        case IrOpcode::FPAbs32: case IrOpcode::FPNeg32: case IrOpcode::FPSaturate32:
        case IrOpcode::FPOrdEqual32: case IrOpcode::FPUnordEqual32: case IrOpcode::FPOrdNotEqual32: case IrOpcode::FPUnordNotEqual32:
        case IrOpcode::FPOrdLessThan32: case IrOpcode::FPUnordLessThan32: case IrOpcode::FPOrdGreaterThan32: case IrOpcode::FPUnordGreaterThan32:
        case IrOpcode::FPOrdLessThanEqual32: case IrOpcode::FPUnordLessThanEqual32: case IrOpcode::FPOrdGreaterThanEqual32: case IrOpcode::FPUnordGreaterThanEqual32:
        case IrOpcode::FPIsNan32: case IrOpcode::FPCmpClass32:
        case IrOpcode::FPAdd32: case IrOpcode::FPSub32: case IrOpcode::FPFma32: case IrOpcode::FPMad32: case IrOpcode::FPMul32:
        case IrOpcode::FPMin32: case IrOpcode::FPMax32: case IrOpcode::FPMinTri32: case IrOpcode::FPMaxTri32: case IrOpcode::FPMedTri32:
        case IrOpcode::FPRecip32: case IrOpcode::FPRecipIFlag32: case IrOpcode::FPRecipSqrt32: case IrOpcode::FPSqrt: case IrOpcode::FPSin: case IrOpcode::FPCos:
        case IrOpcode::FPExp2: case IrOpcode::FPLog2: case IrOpcode::FPRoundEven32: case IrOpcode::FPFloor32: case IrOpcode::FPCeil32: case IrOpcode::FPTrunc32: case IrOpcode::FPFract32:
            return true;
        default:
            return false;
    }
}

class PayloadAnalysis {
public:
    [[nodiscard]] bool AlwaysNan(const IrValue* value, int depth = 0) {
        value = value->Resolve();
        if (value->HasImmediate()) return isNanImmediate(value);
        if (depth > AlwaysNanDepth) return false;
        if (const auto found = alwaysNan.find(value); found != alwaysNan.end()) return found->second;
        alwaysNan.emplace(value, false);
        bool result = false;
        if (isBitCast(value) || value->Opcode() == IrOpcode::Identity) {
            result = AlwaysNan(value->Argument(0), depth + 1);
        } else if (value->Opcode() == IrOpcode::BitwiseOr32 && value->ArgumentCount() == 2u) {
            const IrValue* quieted = maskedOperand(value, IrOpcode::BitwiseOr32, 0x00400000u);
            const IrValue* negated = maskedOperand(value, IrOpcode::BitwiseOr32, 0x80000000u);
            const IrValue* source = quieted != nullptr ? quieted : negated;
            result = source != nullptr && AlwaysNan(source, depth + 1);
        } else if (isSelect(*value)) {
            const IrValue* tested = nanTested(value->Argument(0));
            result = TakenIsNan(value->Argument(1), tested, empty, depth + 1) && AlwaysNan(value->Argument(2), depth + 1);
        }
        alwaysNan[value] = result;
        return result;
    }

    [[nodiscard]] bool TakenIsNan(const IrValue* arm, const IrValue* tested, const ValueSet& tied, int depth = 0) {
        if (contains(tied, arm)) return true;
        if (tested != nullptr) {
            const IrValue* bits = withoutBitCasts(arm);
            if (bits == tested) return true;
            const IrValue* quieted = maskedOperand(bits, IrOpcode::BitwiseOr32, 0x00400000u);
            if (quieted != nullptr && withoutBitCasts(quieted) == tested) return true;
        }
        return AlwaysNan(arm, depth);
    }

    [[nodiscard]] const ValueSet& TiesOf(const IrValue* cause) {
        cause = withoutBitCasts(cause);
        if (const auto found = tieCache.find(cause); found != tieCache.end()) return found->second;
        ValueSet computed = ties(cause);
        return tieCache.emplace(cause, std::move(computed)).first->second;
    }

    [[nodiscard]] static bool Contains(const ValueSet& set, const IrValue* value) {
        return contains(set, value);
    }

    [[nodiscard]] std::vector<IrValue*> Removable(const std::vector<IrValue*>& candidates, bool& exhausted) {
        std::unordered_set<const IrValue*> sources(candidates.begin(), candidates.end());
        std::size_t steps = 0;
        for (std::size_t iteration = 0; iteration < MaxIterations && !sources.empty(); ++iteration) {
            states.clear();
            causes.clear();
            std::vector<const IrValue*> observed;
            std::vector<const IrValue*> pending;
            for (const IrValue* source : sources) {
                states[source] = Difference::NanPayload;
                causes[source] = source;
                pending.push_back(source);
            }
            while (!pending.empty()) {
                const IrValue* current = pending.back();
                pending.pop_back();
                for (const IrUse& use : current->OperandUses()) {
                    if (++steps > ExplorationBudget) {
                        exhausted = true;
                        return {};
                    }
                    const IrValue* user = use.user;
                    if (user->Type() == IrType::Void || user->MayHaveSideEffects() || !pureOperation(user->Opcode())) {
                        if (stateOf(user->Argument(use.operand)) != Difference::Equal) observed.push_back(current);
                        continue;
                    }
                    auto [state, cause] = sources.contains(user) ? sourceTransfer(*user) : transfer(*user);
                    const Difference old = stateOf(user);
                    bool changed = state > old;
                    if (!changed && state == old && state != Difference::Equal) {
                        const auto stored = causes.find(user);
                        if (stored != causes.end() && stored->second != nullptr && stored->second != cause) {
                            cause = nullptr;
                            changed = true;
                        }
                    }
                    if (!changed) continue;
                    states[user] = state;
                    causes[user] = cause;
                    pending.push_back(user);
                }
            }
            if (observed.empty()) {
                std::vector<IrValue*> result;
                for (IrValue* candidate : candidates) {
                    if (sources.contains(candidate)) result.push_back(candidate);
                }
                return result;
            }
            const auto blamed = blame(observed, sources);
            if (blamed.empty()) return {};
            for (const IrValue* value : blamed) sources.erase(value);
        }
        return {};
    }

private:
    [[nodiscard]] static bool contains(const ValueSet& set, const IrValue* value) {
        return set.contains(value->Resolve()) || set.contains(withoutBitCasts(value));
    }

    [[nodiscard]] std::optional<bool> conditionWhenNan(const IrValue* condition, const ValueSet& tied, int depth = 0) const {
        condition = condition->Resolve();
        if (depth > ConditionDepth) return std::nullopt;
        const IrOpcode opcode = condition->Opcode();
        if (opcode == IrOpcode::LogicalNot && condition->ArgumentCount() == 1u) {
            const auto inner = conditionWhenNan(condition->Argument(0), tied, depth + 1);
            return inner.has_value() ? std::optional<bool>(!*inner) : std::nullopt;
        }
        if ((opcode == IrOpcode::LogicalAnd || opcode == IrOpcode::LogicalOr) && condition->ArgumentCount() == 2u) {
            const auto left = conditionWhenNan(condition->Argument(0), tied, depth + 1);
            const auto right = conditionWhenNan(condition->Argument(1), tied, depth + 1);
            if (opcode == IrOpcode::LogicalAnd) {
                if ((left.has_value() && !*left) || (right.has_value() && !*right)) return false;
                if (left.has_value() && right.has_value()) return true;
                return std::nullopt;
            }
            if ((left.has_value() && *left) || (right.has_value() && *right)) return true;
            if (left.has_value() && right.has_value()) return false;
            return std::nullopt;
        }
        if (opcode == IrOpcode::FPIsNan32 && condition->ArgumentCount() == 1u) {
            return contains(tied, condition->Argument(0)) ? std::optional<bool>(true) : std::nullopt;
        }
        if (isOrderedCompare(opcode) || isUnorderedCompare(opcode)) {
            for (std::size_t index = 0; index < condition->ArgumentCount(); ++index) {
                if (contains(tied, condition->Argument(index))) return isUnorderedCompare(opcode);
            }
            return std::nullopt;
        }
        if (isUnsignedCompare(opcode) && condition->ArgumentCount() == 2u) {
            for (std::size_t operand = 0; operand < 2u; ++operand) {
                const IrValue* other = otherImmediate(*condition, operand);
                if (other == nullptr) continue;
                const IrValue* side = condition->Argument(operand)->Resolve();
                const IrValue* magnitude = maskedOperand(side, IrOpcode::BitwiseAnd32, 0x7fffffffu);
                if (magnitude != nullptr && contains(tied, magnitude)) {
                    const auto fixed = compareOverNanMagnitudes(opcode, operand, other->ImmediateU32());
                    if (fixed.has_value()) return fixed;
                }
                const IrValue* exponent = maskedOperand(side, IrOpcode::BitwiseAnd32, 0x7f800000u);
                if (exponent != nullptr && contains(tied, exponent) && (opcode == IrOpcode::IEqual32 || opcode == IrOpcode::INotEqual32)) {
                    return (other->ImmediateU32() == 0x7f800000u) == (opcode == IrOpcode::IEqual32);
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] ValueSet ties(const IrValue* start) {
        ValueSet tied{start};
        std::vector<const IrValue*> pending{start};
        std::vector<const IrValue*> selects;
        while (!pending.empty() && tied.size() < TieLimit) {
            const IrValue* current = pending.back();
            pending.pop_back();
            selects.clear();
            for (const IrUse& use : current->OperandUses()) {
                const IrValue* user = use.user;
                if (isSelect(*user)) {
                    if (use.operand != 0u) selects.push_back(user);
                    continue;
                }
                if (!tied.contains(user) && propagatesNan(*user, use.operand)) {
                    tied.insert(user);
                    pending.push_back(user);
                }
            }
            conditionSelects(current, selects, 0);
            for (const IrValue* select : selects) {
                if (tied.contains(select) || !selectJoins(*select, tied)) continue;
                tied.insert(select);
                pending.push_back(select);
            }
        }
        return tied;
    }

    [[nodiscard]] bool selectJoins(const IrValue& select, const ValueSet& tied) {
        const auto known = conditionWhenNan(select.Argument(0), tied);
        const IrValue* tested = nanTested(select.Argument(0));
        const auto falseNan = [&] { return contains(tied, select.Argument(2)) || AlwaysNan(select.Argument(2)); };
        if (known.has_value()) return *known ? TakenIsNan(select.Argument(1), tested, tied) : falseNan();
        return TakenIsNan(select.Argument(1), tested, tied) && falseNan();
    }

    void conditionSelects(const IrValue* value, std::vector<const IrValue*>& selects, int depth) const {
        if (depth > 4) return;
        for (const IrUse& use : value->OperandUses()) {
            const IrValue* user = use.user;
            if (isSelect(*user) && use.operand == 0u) selects.push_back(user);
            else if (user->Type() == IrType::Bool || user->Opcode() == IrOpcode::BitwiseAnd32) conditionSelects(user, selects, depth + 1);
        }
    }

    [[nodiscard]] Difference stateOf(const IrValue* value) const {
        const auto found = states.find(value->Resolve());
        return found != states.end() ? found->second : Difference::Equal;
    }

    [[nodiscard]] const IrValue* causeOf(const IrValue* value) const {
        value = value->Resolve();
        const Difference state = stateOf(value);
        if (state == Difference::Equal) return nullptr;
        if (state == Difference::NanPayload) return withoutBitCasts(value);
        const auto found = causes.find(value);
        return found != causes.end() ? found->second : nullptr;
    }

    [[nodiscard]] std::pair<Difference, const IrValue*> sourceTransfer(const IrValue& select) {
        const IrValue* tested = nanTested(select.Argument(0));
        if (tested == nullptr || stateOf(tested) == Difference::Arbitrary || stateOf(select.Argument(0)) == Difference::Arbitrary) return {Difference::Arbitrary, nullptr};
        if (stateOf(select.Argument(2)) == Difference::Arbitrary) {
            const IrValue* cause = causeOf(select.Argument(2));
            if (cause == nullptr || !contains(TiesOf(cause), tested)) return {Difference::Arbitrary, nullptr};
        }
        return {Difference::NanPayload, &select};
    }

    [[nodiscard]] std::pair<Difference, const IrValue*> transfer(const IrValue& user) {
        Difference worst = Difference::Equal;
        const IrValue* cause = nullptr;
        bool singleCause = true;
        for (std::size_t index = 0; index < user.ArgumentCount(); ++index) {
            const Difference state = stateOf(user.Argument(index));
            if (state == Difference::Equal) continue;
            worst = std::max(worst, state);
            const IrValue* operandCause = causeOf(user.Argument(index));
            if (operandCause == nullptr || (cause != nullptr && cause != operandCause)) singleCause = false;
            cause = operandCause;
        }
        if (worst == Difference::Equal) return {Difference::Equal, nullptr};
        const IrValue* single = singleCause ? cause : nullptr;
        const IrOpcode opcode = user.Opcode();
        if (isSelect(user)) {
            if (stateOf(user.Argument(0)) != Difference::Equal) return {Difference::Arbitrary, single};
            Difference result = Difference::Equal;
            const IrValue* resultCause = nullptr;
            bool resultSingle = true;
            for (std::size_t arm = 1u; arm <= 2u; ++arm) {
                const Difference armState = stateOf(user.Argument(arm));
                if (armState == Difference::Equal) continue;
                const IrValue* armCause = causeOf(user.Argument(arm));
                if (armCause != nullptr) {
                    const auto known = conditionWhenNan(user.Argument(0), TiesOf(armCause));
                    if (known.has_value() && *known != (arm == 1u)) continue;
                }
                result = std::max(result, armState);
                if (armCause == nullptr || (resultCause != nullptr && resultCause != armCause)) resultSingle = false;
                resultCause = armCause;
            }
            return {result, result == Difference::Equal || !resultSingle ? nullptr : resultCause};
        }
        if (opcode == IrOpcode::Phi) return {worst, single};
        if (opcode == IrOpcode::FPIsNan32 || opcode == IrOpcode::FPSaturate32 || isOrderedCompare(opcode) || isUnorderedCompare(opcode)) {
            if (worst == Difference::NanPayload) return {Difference::Equal, nullptr};
            return {Difference::Arbitrary, single};
        }
        if (worst == Difference::Arbitrary) return {Difference::Arbitrary, single};
        if (isUnsignedCompare(opcode) && user.ArgumentCount() == 2u) {
            for (std::size_t operand = 0; operand < 2u; ++operand) {
                const IrValue* other = otherImmediate(user, operand);
                const IrValue* side = user.Argument(operand)->Resolve();
                if (other == nullptr || stateOf(side) != Difference::NanPayload || maskedOperand(side, IrOpcode::BitwiseAnd32, 0x7fffffffu) == nullptr) continue;
                if (compareOverNanMagnitudes(opcode, operand, other->ImmediateU32()).has_value()) return {Difference::Equal, nullptr};
            }
            return {Difference::Arbitrary, single};
        }
        if (opcode == IrOpcode::BitwiseAnd32) {
            const auto mask = anyImmediate(user);
            if (mask == 0x7f800000u) return {Difference::Equal, nullptr};
            if (mask == 0x7fffffffu) return {Difference::NanPayload, single};
            return {Difference::Arbitrary, single};
        }
        if (opcode == IrOpcode::BitwiseXor32 || opcode == IrOpcode::BitwiseOr32) {
            const auto mask = anyImmediate(user);
            return {mask == 0x80000000u || mask == 0x00400000u ? Difference::NanPayload : Difference::Arbitrary, single};
        }
        if (keepsNanPayloadDifference(opcode)) return {Difference::NanPayload, single};
        return {Difference::Arbitrary, single};
    }

    [[nodiscard]] std::unordered_set<const IrValue*> blame(const std::vector<const IrValue*>& observed, const std::unordered_set<const IrValue*>& sources) const {
        std::unordered_set<const IrValue*> blamed;
        std::unordered_set<const IrValue*> seen;
        std::vector<const IrValue*> pending(observed.begin(), observed.end());
        while (!pending.empty()) {
            const IrValue* value = pending.back()->Resolve();
            pending.pop_back();
            if (!seen.insert(value).second) continue;
            if (sources.contains(value)) blamed.insert(value);
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
                const IrValue* argument = value->Argument(index)->Resolve();
                if (stateOf(argument) != Difference::Equal) pending.push_back(argument);
            }
        }
        return blamed;
    }

    ValueSet empty;
    std::unordered_map<const IrValue*, bool> alwaysNan;
    std::unordered_map<const IrValue*, ValueSet> tieCache;
    std::unordered_map<const IrValue*, Difference> states;
    std::unordered_map<const IrValue*, const IrValue*> causes;
};

}

NanPayloadEliminationStats NanPayloadEliminator::Eliminate(IrProgram& program) const {
    NanPayloadEliminationStats stats;
    PayloadAnalysis analysis;
    std::vector<IrValue*> candidates;
    for (auto* block : program.BlockOrder()) {
        for (auto* inst : block->Instructions()) {
            if (!inst->HasUses() || !isSelect(*inst)) continue;
            const IrValue* tested = nanTested(inst->Argument(0));
            if (tested == nullptr) continue;
            ++stats.candidates;
            const ValueSet& tied = analysis.TiesOf(tested);
            const IrValue* falseArm = inst->Argument(2);
            const bool falseNan = PayloadAnalysis::Contains(tied, falseArm) || analysis.AlwaysNan(falseArm);
            if (falseNan && analysis.TakenIsNan(inst->Argument(1), tested, tied)) candidates.push_back(inst);
        }
    }
    bool exhausted = false;
    const std::vector<IrValue*> removable = analysis.Removable(candidates, exhausted);
    if (exhausted) ++stats.budgetExhausted;
    for (IrValue* select : removable) {
        select->ReplaceAllUsesWith(select->Argument(2)->Resolve());
        ++stats.removedSelects;
    }
    return stats;
}

}
