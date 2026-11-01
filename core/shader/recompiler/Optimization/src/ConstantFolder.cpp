#include "Optimization/ConstantFolder.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

IrValue& resolveArg(const IrValue& inst, std::size_t index) {
    return *inst.Argument(index)->Resolve();
}

bool isImmediate(const IrValue& value, IrType type) {
    return value.HasImmediate() && value.Type() == type;
}

void replaceWith(IrValue& inst, IrValue& value) {
    inst.ReplaceUsesWith(value.Resolve(), true);
}

template <typename TFunction> bool foldU32(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(lhs, IrType::U32) || !isImmediate(rhs, IrType::U32)) {
        return false;
    }
    replaceWith(inst, builder.Constant(static_cast<std::uint32_t>(function(lhs.ImmediateU32(), rhs.ImmediateU32()))));
    return true;
}

template <typename TFunction> bool foldU64(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(lhs, IrType::U64) || !isImmediate(rhs, IrType::U64)) {
        return false;
    }
    replaceWith(inst, builder.ConstantU64(static_cast<std::uint64_t>(function(lhs.ImmediateU64(), rhs.ImmediateU64()))));
    return true;
}

template <typename TFunction> bool foldU64Shift(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& value = resolveArg(inst, 0);
    auto& shift = resolveArg(inst, 1);
    if (!isImmediate(value, IrType::U64) || !isImmediate(shift, IrType::U32)) {
        return false;
    }
    replaceWith(inst, builder.ConstantU64(static_cast<std::uint64_t>(function(value.ImmediateU64(), shift.ImmediateU32()))));
    return true;
}

bool foldConstantBitTest(IrBuilder& builder, IrValue& inst) {
    auto& left = resolveArg(inst, 0);
    auto& right = resolveArg(inst, 1);
    if (!isImmediate(right, IrType::U32) || right.ImmediateU32() != 0u || left.Opcode() != IrOpcode::BitwiseAnd32) return false;
    auto& first = resolveArg(left, 0);
    auto& second = resolveArg(left, 1);
    IrValue* shifted = nullptr;
    if (isImmediate(second, IrType::U32) && second.ImmediateU32() == 1u) shifted = &first;
    else if (isImmediate(first, IrType::U32) && first.ImmediateU32() == 1u) shifted = &second;
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32) return false;
    auto& word = resolveArg(*shifted, 0);
    auto& amount = resolveArg(*shifted, 1);
    if (!isImmediate(word, IrType::U32) || (word.ImmediateU32() != 0u && word.ImmediateU32() != 0xffffffffu)) return false;
    if (amount.Opcode() != IrOpcode::BitwiseAnd32) return false;
    auto& amountLeft = resolveArg(amount, 0);
    auto& amountRight = resolveArg(amount, 1);
    const bool masked = (isImmediate(amountLeft, IrType::U32) && amountLeft.ImmediateU32() == 31u) || (isImmediate(amountRight, IrType::U32) && amountRight.ImmediateU32() == 31u);
    if (!masked) return false;
    replaceWith(inst, builder.ConstantBool(word.ImmediateU32() != 0u));
    return true;
}

bool isLaneIndex(const IrValue& value) {
    return value.Opcode() == IrOpcode::LaneId;
}

std::uint32_t possibleBits(const IrValue& value, unsigned depth = 0) {
    if (value.HasImmediate() && value.Type() == IrType::U32) return value.ImmediateU32();
    if (depth >= 8u || value.Type() != IrType::U32) return UINT32_MAX;
    const auto arg = [&](std::size_t index) -> const IrValue& { return *value.Argument(index)->Resolve(); };
    const auto immediate = [&](std::size_t index) { return arg(index).HasImmediate() && arg(index).Type() == IrType::U32; };
    switch (value.Opcode()) {
        case IrOpcode::BitwiseAnd32: return possibleBits(arg(0), depth + 1) & possibleBits(arg(1), depth + 1);
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32: return possibleBits(arg(0), depth + 1) | possibleBits(arg(1), depth + 1);
        case IrOpcode::BitFieldUExtract:
            if (immediate(2) && arg(2).ImmediateU32() < 32u) return (std::uint32_t{1} << arg(2).ImmediateU32()) - 1u;
            return UINT32_MAX;
        case IrOpcode::ShiftLeftLogical32:
            if (immediate(1) && arg(1).ImmediateU32() < 32u) return possibleBits(arg(0), depth + 1) << arg(1).ImmediateU32();
            return UINT32_MAX;
        case IrOpcode::ShiftRightLogical32:
            if (immediate(1) && arg(1).ImmediateU32() < 32u) return possibleBits(arg(0), depth + 1) >> arg(1).ImmediateU32();
            return UINT32_MAX;
        case IrOpcode::ConvertU32U16: return 0xffffu;
        case IrOpcode::ConvertU32U8: return 0xffu;
        case IrOpcode::SelectU32: return possibleBits(arg(1), depth + 1) | possibleBits(arg(2), depth + 1);
        default: return UINT32_MAX;
    }
}

bool readThroughUnusedBits(IrValue& value, std::size_t index, std::uint32_t demanded) {
    bool changed = false;
    for (unsigned step = 0; step < 16u; ++step) {
        auto& operand = resolveArg(value, index);
        if (operand.Type() != IrType::U32) break;
        IrValue* next = nullptr;
        for (std::size_t side = 0; side < 2u && next == nullptr; ++side) {
            if (operand.Opcode() == IrOpcode::BitwiseAnd32) {
                auto& mask = resolveArg(operand, side);
                if (isImmediate(mask, IrType::U32) && (demanded & ~mask.ImmediateU32()) == 0u) next = &resolveArg(operand, 1u - side);
            } else if (operand.Opcode() == IrOpcode::BitwiseOr32 || operand.Opcode() == IrOpcode::BitwiseXor32) {
                if ((possibleBits(resolveArg(operand, side)) & demanded) == 0u) next = &resolveArg(operand, 1u - side);
            }
        }
        if (next == nullptr) break;
        value.ReplaceArgument(index, next);
        changed = true;
    }
    return changed;
}

bool maskedPart(IrValue& value, IrValue*& source, std::uint32_t& mask) {
    const auto immediate = [&](IrValue& owner, std::size_t index, std::uint32_t& result) {
        auto& argument = resolveArg(owner, index);
        if (!isImmediate(argument, IrType::U32)) return false;
        result = argument.ImmediateU32();
        return true;
    };
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    switch (value.Opcode()) {
        case IrOpcode::BitwiseAnd32:
            for (std::size_t side = 0; side < 2u; ++side) {
                if (immediate(value, side, mask)) {
                    source = &resolveArg(value, 1u - side);
                    return !isImmediate(*source, IrType::U32);
                }
            }
            return false;
        case IrOpcode::BitFieldUExtract:
            if (!immediate(value, 1, first) || !immediate(value, 2, second) || first != 0u || second == 0u || second >= 32u) return false;
            source = &resolveArg(value, 0);
            mask = (std::uint32_t{1} << second) - 1u;
            return true;
        case IrOpcode::ShiftLeftLogical32: {
            if (!immediate(value, 1, first) || first == 0u || first >= 32u) return false;
            auto& inner = resolveArg(value, 0);
            std::uint32_t offset = 0;
            std::uint32_t count = 0;
            if (inner.Opcode() == IrOpcode::BitFieldUExtract && immediate(inner, 1, offset) && immediate(inner, 2, count) && offset == first && count > 0u && count <= 32u - offset) {
                source = &resolveArg(inner, 0);
                mask = (count == 32u - offset ? UINT32_MAX : (std::uint32_t{1} << (offset + count)) - 1u) & (UINT32_MAX << offset);
                return true;
            }
            if (inner.Opcode() == IrOpcode::ShiftRightLogical32 && immediate(inner, 1, offset) && offset == first) {
                source = &resolveArg(inner, 0);
                mask = UINT32_MAX << first;
                return true;
            }
            return false;
        }
        default: return false;
    }
}

bool mergeMaskedParts(IrProgram& program, IrValue& value) {
    IrValue* left = nullptr;
    IrValue* right = nullptr;
    std::uint32_t leftMask = 0;
    std::uint32_t rightMask = 0;
    if (!maskedPart(resolveArg(value, 0), left, leftMask) || !maskedPart(resolveArg(value, 1), right, rightMask) || left != right) return false;
    if ((leftMask | rightMask) == UINT32_MAX) {
        replaceWith(value, *left);
        return true;
    }
    if (value.Opcode() != IrOpcode::BitwiseOr32) return false;
    IrBuilder builder(program);
    value.ReplaceOpcode(IrOpcode::BitwiseAnd32);
    value.ReplaceArgument(0, left);
    value.ReplaceArgument(1, &builder.Constant(leftMask | rightMask));
    return true;
}

bool foldRedundantMask(IrValue& value) {
    for (std::size_t side = 0; side < 2u; ++side) {
        auto& mask = resolveArg(value, side);
        auto& operand = resolveArg(value, 1u - side);
        if (!isImmediate(mask, IrType::U32)) continue;
        const auto keep = mask.ImmediateU32();
        if ((possibleBits(operand) & ~keep) == 0u) {
            replaceWith(value, operand);
            return true;
        }
        if (operand.Opcode() == IrOpcode::BitwiseAnd32) {
            for (std::size_t part = 0; part < 2u; ++part) {
                auto& inner = resolveArg(operand, part);
                if (isImmediate(inner, IrType::U32) && (keep & ~inner.ImmediateU32()) == 0u) {
                    value.ReplaceArgument(1u - side, &resolveArg(operand, 1u - part));
                    foldRedundantMask(value);
                    return true;
                }
            }
        }
        if (operand.Opcode() == IrOpcode::BitwiseOr32) {
            for (std::size_t part = 0; part < 2u; ++part) {
                if ((possibleBits(resolveArg(operand, part)) & keep) == 0u) {
                    value.ReplaceArgument(1u - side, &resolveArg(operand, 1u - part));
                    foldRedundantMask(value);
                    return true;
                }
            }
        }
    }
    return false;
}

bool feedsMultiply(const IrValue& value) {
    const auto& uses = value.Uses();
    return std::any_of(uses.begin(), uses.end(), [](const IrValue* use) { return use->Opcode() == IrOpcode::IMul32; });
}

bool isImmediateU32(const IrValue& value, std::uint32_t immediate) {
    return isImmediate(value, IrType::U32) && value.ImmediateU32() == immediate;
}

bool runsAllStartingLanes(const IrProgram& program, const IrBlock* target) {
    if (program.BlockOrder().empty()) return false;
    const IrBlock* block = program.BlockOrder().front();
    for (std::size_t steps = 0; block != nullptr && steps < program.BlockOrder().size(); ++steps) {
        if (block == target) return true;
        if (block->Successors().size() != 1u) return false;
        block = block->Successors().front();
        if (block->Predecessors().size() != 1u) return false;
    }
    return false;
}

bool isRealLaneCondition(const IrValue& condition) {
    if (condition.Opcode() != IrOpcode::IEqual32) return false;
    const auto helper = [](const IrValue& value) {
        if (value.Opcode() != IrOpcode::GetBuiltin) return false;
        const auto& kind = resolveArg(value, 0);
        return isImmediate(kind, IrType::U32) && static_cast<StageInputKind>(kind.ImmediateU32()) == StageInputKind::HelperInvocation;
    };
    const auto& lhs = resolveArg(condition, 0);
    const auto& rhs = resolveArg(condition, 1);
    return (helper(lhs) && isImmediateU32(rhs, 0u)) || (helper(rhs) && isImmediateU32(lhs, 0u));
}

enum class LaneCoverage { Real, Quads };

constexpr std::uint32_t StartingLaneDepth = 8u;

bool holdsStartingLanes(const IrProgram& program, const IrValue& word, std::uint32_t index, LaneCoverage coverage, std::uint32_t depth = 0) {
    if (depth > StartingLaneDepth) return false;
    if (word.Opcode() == IrOpcode::BitwiseAnd32) {
        return holdsStartingLanes(program, resolveArg(word, 0), index, coverage, depth + 1u) && holdsStartingLanes(program, resolveArg(word, 1), index, coverage, depth + 1u);
    }
    if (word.Opcode() == IrOpcode::BitwiseOr32) {
        return holdsStartingLanes(program, resolveArg(word, 0), index, coverage, depth + 1u) || holdsStartingLanes(program, resolveArg(word, 1), index, coverage, depth + 1u);
    }
    if (word.Opcode() == IrOpcode::CompositeExtractU64 && isImmediateU32(resolveArg(word, 1), index)) {
        const auto& mask = resolveArg(word, 0);
        if (mask.Opcode() != IrOpcode::WqmU64) return false;
        const auto& lanes = resolveArg(mask, 0);
        return lanes.Opcode() == IrOpcode::CompositeConstructU64 && holdsStartingLanes(program, resolveArg(lanes, index), index, LaneCoverage::Real, depth + 1u);
    }
    if (coverage == LaneCoverage::Quads) return false;
    if (word.Opcode() != IrOpcode::CompositeExtractU32x4 || !isImmediateU32(resolveArg(word, 1), index)) return false;
    const auto& ballot = resolveArg(word, 0);
    if (ballot.Opcode() != IrOpcode::Ballot || !runsAllStartingLanes(program, ballot.Parent())) return false;
    const auto& condition = resolveArg(ballot, 0);
    return isImmediate(condition, IrType::U1) ? condition.ImmediateBool() : isRealLaneCondition(condition);
}

IrValue* startingRealLaneCondition(const IrProgram& program, const IrValue& word, std::uint32_t index, std::uint32_t depth = 0u) {
    if (depth > StartingLaneDepth) return nullptr;
    if (word.Opcode() == IrOpcode::BitwiseAnd32) {
        for (std::size_t side = 0u; side < 2u; ++side) {
            auto* condition = startingRealLaneCondition(program, resolveArg(word, side), index, depth + 1u);
            if (condition != nullptr && holdsStartingLanes(program, resolveArg(word, 1u - side), index, LaneCoverage::Real, depth + 1u)) return condition;
        }
        return nullptr;
    }
    if (word.Opcode() != IrOpcode::CompositeExtractU32x4 || !isImmediateU32(resolveArg(word, 1), index)) return nullptr;
    const auto& ballot = resolveArg(word, 0);
    if (ballot.Opcode() != IrOpcode::Ballot || !runsAllStartingLanes(program, ballot.Parent())) return nullptr;
    auto& condition = resolveArg(ballot, 0);
    return isRealLaneCondition(condition) ? &condition : nullptr;
}

bool foldStartingLaneBitTest(const IrProgram& program, IrBuilder& builder, IrValue& inst) {
    if (program.Resources().stage != IrShaderStage::Pixel) return false;
    auto& left = resolveArg(inst, 0);
    auto& right = resolveArg(inst, 1);
    if (!isImmediateU32(right, 0u) || left.Opcode() != IrOpcode::BitwiseAnd32) return false;
    IrValue* shifted = isImmediateU32(resolveArg(left, 1), 1u) ? &resolveArg(left, 0) : isImmediateU32(resolveArg(left, 0), 1u) ? &resolveArg(left, 1) : nullptr;
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32) return false;
    const auto& amount = resolveArg(*shifted, 1);
    if (amount.Opcode() != IrOpcode::BitwiseAnd32) return false;
    const bool laneAmount = (isLaneIndex(resolveArg(amount, 0)) && isImmediateU32(resolveArg(amount, 1), 31u)) || (isLaneIndex(resolveArg(amount, 1)) && isImmediateU32(resolveArg(amount, 0), 31u));
    if (!laneAmount) return false;
    const auto& word = resolveArg(*shifted, 0);
    const IrValue* low = &word;
    const IrValue* high = nullptr;
    if (word.Opcode() == IrOpcode::SelectU32) {
        const auto& half = resolveArg(word, 0);
        if (half.Opcode() != IrOpcode::ULessThan32 || !isLaneIndex(resolveArg(half, 0)) || !isImmediateU32(resolveArg(half, 1), 32u)) return false;
        low = &resolveArg(word, 1);
        high = &resolveArg(word, 2);
    } else if (program.WaveSize() != 32u) {
        return false;
    }
    if (holdsStartingLanes(program, *low, 0u, LaneCoverage::Quads) && (high == nullptr || holdsStartingLanes(program, *high, 1u, LaneCoverage::Quads))) {
        replaceWith(inst, builder.ConstantBool(true));
        return true;
    }
    auto* condition = startingRealLaneCondition(program, *low, 0u);
    if (condition == nullptr || (high != nullptr && startingRealLaneCondition(program, *high, 1u) != condition)) return false;
    replaceWith(inst, *condition);
    return true;
}

template <typename TFunction> bool foldU32Compare(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(lhs, IrType::U32) || !isImmediate(rhs, IrType::U32)) {
        return false;
    }
    replaceWith(inst, builder.ConstantBool(function(lhs.ImmediateU32(), rhs.ImmediateU32())));
    return true;
}

constexpr std::size_t ValueSetLimit = 64u;
constexpr std::uint32_t ValueSetDepth = 6u;

using ValueSet = std::optional<std::vector<std::uint32_t>>;

ValueSet normalizedSet(std::vector<std::uint32_t> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    if (values.size() > ValueSetLimit) return std::nullopt;
    return values;
}

ValueSet rangeSet(std::uint32_t count) {
    std::vector<std::uint32_t> values(count);
    for (std::uint32_t index = 0u; index < count; ++index) values[index] = index;
    return values;
}

template <typename TFunction> ValueSet mappedSet(const ValueSet& source, TFunction function) {
    if (!source) return std::nullopt;
    std::vector<std::uint32_t> values;
    values.reserve(source->size());
    for (const std::uint32_t value : *source) values.push_back(static_cast<std::uint32_t>(function(value)));
    return normalizedSet(std::move(values));
}

struct KnownBits {
    std::uint32_t zero = 0u;
    std::uint32_t one = 0u;
};

constexpr std::uint32_t KnownBitsDepth = 6u;
constexpr std::uint32_t FactBudget = 2048u;

std::uint32_t lowMask(std::uint32_t bits) {
    return bits >= 32u ? 0xffffffffu : (std::uint32_t{1} << bits) - 1u;
}

KnownBits commonBits(const KnownBits& lhs, const KnownBits& rhs) {
    return {lhs.zero & rhs.zero, lhs.one & rhs.one};
}

KnownBits knownBits(const IrValue& value, std::uint32_t depth, std::uint32_t& budget) {
    if (isImmediate(value, IrType::U32)) return {~value.ImmediateU32(), value.ImmediateU32()};
    if (value.Type() != IrType::U32 || depth == 0u || budget == 0u) return {};
    --budget;
    const auto argument = [&](std::size_t index) { return knownBits(resolveArg(value, index), depth - 1u, budget); };
    switch (value.Opcode()) {
        case IrOpcode::ShiftLeftLogical32: {
            auto& amount = resolveArg(value, 1);
            if (!isImmediate(amount, IrType::U32) || amount.ImmediateU32() >= 32u) return {};
            const std::uint32_t shift = amount.ImmediateU32();
            const KnownBits source = argument(0);
            return {(source.zero << shift) | lowMask(shift), source.one << shift};
        }
        case IrOpcode::ShiftRightLogical32: {
            auto& amount = resolveArg(value, 1);
            if (!isImmediate(amount, IrType::U32) || amount.ImmediateU32() >= 32u) return {};
            const std::uint32_t shift = amount.ImmediateU32();
            const KnownBits source = argument(0);
            return {(source.zero >> shift) | ~(0xffffffffu >> shift), source.one >> shift};
        }
        case IrOpcode::BitFieldUExtract: {
            auto& offset = resolveArg(value, 1);
            auto& count = resolveArg(value, 2);
            if (!isImmediate(offset, IrType::U32) || !isImmediate(count, IrType::U32) || count.ImmediateU32() > 32u || offset.ImmediateU32() > 32u - count.ImmediateU32()) return {};
            const std::uint32_t mask = lowMask(count.ImmediateU32());
            if (mask == 0u) return {0xffffffffu, 0u};
            const KnownBits source = argument(0);
            const std::uint32_t shift = offset.ImmediateU32();
            return {((source.zero >> shift) & mask) | ~mask, (source.one >> shift) & mask};
        }
        case IrOpcode::BitwiseAnd32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            return {lhs.zero | rhs.zero, lhs.one & rhs.one};
        }
        case IrOpcode::BitwiseOr32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            return {lhs.zero & rhs.zero, lhs.one | rhs.one};
        }
        case IrOpcode::BitwiseXor32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            return {(lhs.zero & rhs.zero) | (lhs.one & rhs.one), (lhs.zero & rhs.one) | (lhs.one & rhs.zero)};
        }
        case IrOpcode::IAdd32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            const std::uint32_t mask = lowMask(static_cast<std::uint32_t>(std::countr_one((lhs.zero | lhs.one) & (rhs.zero | rhs.one))));
            const std::uint32_t sum = lhs.one + rhs.one;
            return {~sum & mask, sum & mask};
        }
        case IrOpcode::IMul32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            return {lowMask(static_cast<std::uint32_t>(std::countr_one(lhs.zero) + std::countr_one(rhs.zero))), 0u};
        }
        case IrOpcode::UMin32: {
            const KnownBits lhs = argument(0);
            const KnownBits rhs = argument(1);
            KnownBits result = commonBits(lhs, rhs);
            result.zero |= ~lowMask(static_cast<std::uint32_t>(std::bit_width(std::min(~lhs.zero, ~rhs.zero))));
            return result;
        }
        case IrOpcode::SelectU32:
            return commonBits(argument(1), argument(2));
        case IrOpcode::ReadFirstLane:
        case IrOpcode::ReadLane:
            return argument(0);
        case IrOpcode::Phi: {
            std::optional<KnownBits> result;
            for (std::size_t index = 0; index < value.ArgumentCount(); index++) {
                const IrValue* incoming = value.Argument(index)->Resolve();
                if (incoming == &value) continue;
                if (budget == 0u) return {};
                --budget;
                const KnownBits bits = knownBits(*incoming, depth - 1u, budget);
                result = result ? commonBits(*result, bits) : bits;
                if (result->zero == 0u && result->one == 0u) return {};
            }
            return result.value_or(KnownBits{});
        }
        default:
            return {};
    }
}

ValueSet possibleValues(const IrValue& value, std::uint32_t depth, std::uint32_t& budget);

bool immediateOperand(const IrValue& inst, const IrValue*& other, std::uint32_t& immediate) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (isImmediate(rhs, IrType::U32)) {
        other = &lhs;
        immediate = rhs.ImmediateU32();
        return true;
    }
    if (isImmediate(lhs, IrType::U32)) {
        other = &rhs;
        immediate = lhs.ImmediateU32();
        return true;
    }
    return false;
}

ValueSet possibleValues(const IrValue& value, std::uint32_t depth, std::uint32_t& budget) {
    if (isImmediate(value, IrType::U32)) return std::vector<std::uint32_t>{value.ImmediateU32()};
    if (value.Type() != IrType::U32 || depth == 0u || budget == 0u) return std::nullopt;
    --budget;
    const IrValue* other = nullptr;
    std::uint32_t immediate = 0u;
    switch (value.Opcode()) {
        case IrOpcode::ShiftRightLogical32: {
            auto& amount = resolveArg(value, 1);
            if (!isImmediate(amount, IrType::U32) || amount.ImmediateU32() >= 32u) return std::nullopt;
            const std::uint32_t shift = amount.ImmediateU32();
            if (ValueSet source = possibleValues(resolveArg(value, 0), depth - 1u, budget)) return mappedSet(source, [shift](std::uint32_t v) { return v >> shift; });
            if (shift < 26u) return std::nullopt;
            return rangeSet(std::uint32_t{1} << (32u - shift));
        }
        case IrOpcode::ShiftLeftLogical32: {
            auto& amount = resolveArg(value, 1);
            if (!isImmediate(amount, IrType::U32) || amount.ImmediateU32() >= 32u) return std::nullopt;
            const std::uint32_t shift = amount.ImmediateU32();
            return mappedSet(possibleValues(resolveArg(value, 0), depth - 1u, budget), [shift](std::uint32_t v) { return v << shift; });
        }
        case IrOpcode::BitFieldUExtract: {
            auto& offset = resolveArg(value, 1);
            auto& count = resolveArg(value, 2);
            if (!isImmediate(offset, IrType::U32) || !isImmediate(count, IrType::U32) || count.ImmediateU32() == 0u || count.ImmediateU32() > 6u || offset.ImmediateU32() > 32u - count.ImmediateU32()) return std::nullopt;
            const std::uint32_t shift = offset.ImmediateU32();
            const std::uint32_t mask = (std::uint32_t{1} << count.ImmediateU32()) - 1u;
            if (ValueSet source = possibleValues(resolveArg(value, 0), depth - 1u, budget)) return mappedSet(source, [shift, mask](std::uint32_t v) { return (v >> shift) & mask; });
            return rangeSet(mask + 1u);
        }
        case IrOpcode::BitwiseAnd32: {
            if (!immediateOperand(value, other, immediate)) return std::nullopt;
            if (ValueSet source = possibleValues(*other, depth - 1u, budget)) return mappedSet(source, [immediate](std::uint32_t v) { return v & immediate; });
            const KnownBits bits = knownBits(*other, depth - 1u, budget);
            const std::uint32_t mask = immediate & ~(bits.zero | bits.one);
            const std::uint32_t fixed = immediate & bits.one;
            if (std::popcount(mask) > 6) return std::nullopt;
            std::vector<std::uint32_t> values;
            std::uint32_t subset = 0u;
            do {
                values.push_back(subset | fixed);
                subset = (subset - mask) & mask;
            } while (subset != 0u);
            return normalizedSet(std::move(values));
        }
        case IrOpcode::BitwiseOr32:
            if (!immediateOperand(value, other, immediate)) return std::nullopt;
            return mappedSet(possibleValues(*other, depth - 1u, budget), [immediate](std::uint32_t v) { return v | immediate; });
        case IrOpcode::BitwiseXor32:
            if (!immediateOperand(value, other, immediate)) return std::nullopt;
            return mappedSet(possibleValues(*other, depth - 1u, budget), [immediate](std::uint32_t v) { return v ^ immediate; });
        case IrOpcode::IMul32:
            if (!immediateOperand(value, other, immediate)) return std::nullopt;
            return mappedSet(possibleValues(*other, depth - 1u, budget), [immediate](std::uint32_t v) { return v * immediate; });
        case IrOpcode::IAdd32: {
            const ValueSet lhs = possibleValues(resolveArg(value, 0), depth - 1u, budget);
            const ValueSet rhs = lhs ? possibleValues(resolveArg(value, 1), depth - 1u, budget) : std::nullopt;
            if (!rhs || lhs->size() + rhs->size() - 1u > ValueSetLimit) return std::nullopt;
            std::vector<std::uint32_t> values;
            for (const std::uint32_t left : *lhs) {
                for (const std::uint32_t right : *rhs) values.push_back(left + right);
            }
            return normalizedSet(std::move(values));
        }
        case IrOpcode::UMin32: {
            if (!immediateOperand(value, other, immediate)) return std::nullopt;
            const std::uint32_t limit = immediate;
            if (ValueSet source = possibleValues(*other, depth - 1u, budget)) return mappedSet(source, [limit](std::uint32_t v) { return std::min(v, limit); });
            if (limit >= ValueSetLimit) return std::nullopt;
            return rangeSet(limit + 1u);
        }
        case IrOpcode::SelectU32: {
            ValueSet lhs = possibleValues(resolveArg(value, 1), depth - 1u, budget);
            const ValueSet rhs = lhs ? possibleValues(resolveArg(value, 2), depth - 1u, budget) : std::nullopt;
            if (!rhs) return std::nullopt;
            lhs->insert(lhs->end(), rhs->begin(), rhs->end());
            return normalizedSet(std::move(*lhs));
        }
        default:
            return std::nullopt;
    }
}

class OperandFacts {
public:
    KnownBits Bits(const IrValue& value) {
        if (const auto found = bits.find(&value); found != bits.end()) return found->second;
        std::uint32_t budget = FactBudget;
        return bits.emplace(&value, knownBits(value, KnownBitsDepth, budget)).first->second;
    }

    const ValueSet& Values(const IrValue& value) {
        if (const auto found = values.find(&value); found != values.end()) return found->second;
        std::uint32_t budget = FactBudget;
        return values.emplace(&value, possibleValues(value, ValueSetDepth, budget)).first->second;
    }

private:
    std::unordered_map<const IrValue*, KnownBits> bits;
    std::unordered_map<const IrValue*, ValueSet> values;
};

bool foldEqualityByValueSet(IrBuilder& builder, IrValue& inst, bool equal, OperandFacts& facts) {
    const IrValue* other = nullptr;
    std::uint32_t immediate = 0u;
    if (!immediateOperand(inst, other, immediate)) return false;
    const KnownBits bits = facts.Bits(*other);
    if (((immediate & bits.zero) | (~immediate & bits.one)) != 0u) {
        replaceWith(inst, builder.ConstantBool(!equal));
        return true;
    }
    const ValueSet& values = facts.Values(*other);
    if (!values) return false;
    const bool contains = std::binary_search(values->begin(), values->end(), immediate);
    if (contains && values->size() != 1u) return false;
    replaceWith(inst, builder.ConstantBool(contains == equal));
    return true;
}

bool foldULessThanByRange(IrBuilder& builder, IrValue& inst, OperandFacts& facts) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(rhs, IrType::U32) || isImmediate(lhs, IrType::U32)) return false;
    const std::uint32_t limit = rhs.ImmediateU32();
    const KnownBits bits = facts.Bits(lhs);
    std::uint32_t lowest = bits.one;
    std::uint32_t highest = ~bits.zero;
    if (const ValueSet& values = facts.Values(lhs); values && !values->empty()) {
        lowest = std::max(lowest, values->front());
        highest = std::min(highest, values->back());
    }
    if (highest < limit) {
        replaceWith(inst, builder.ConstantBool(true));
        return true;
    }
    if (lowest >= limit) {
        replaceWith(inst, builder.ConstantBool(false));
        return true;
    }
    return false;
}

template <typename TFunction> bool foldU64Compare(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(lhs, IrType::U64) || !isImmediate(rhs, IrType::U64)) {
        return false;
    }
    replaceWith(inst, builder.ConstantBool(function(lhs.ImmediateU64(), rhs.ImmediateU64())));
    return true;
}

template <typename TFunction> bool foldLogical(IrBuilder& builder, IrValue& inst, TFunction function) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (!isImmediate(lhs, IrType::U1) || !isImmediate(rhs, IrType::U1)) {
        return false;
    }
    replaceWith(inst, builder.ConstantBool(function(lhs.ImmediateBool(), rhs.ImmediateBool())));
    return true;
}

bool replaceBinaryIdentity(IrValue& inst, IrType type, std::uint64_t identity) {
    auto& lhs = resolveArg(inst, 0);
    auto& rhs = resolveArg(inst, 1);
    if (isImmediate(lhs, type) && (type == IrType::U32 ? lhs.ImmediateU32() == identity : lhs.ImmediateU64() == identity)) {
        replaceWith(inst, rhs);
        return true;
    }
    if (isImmediate(rhs, type) && (type == IrType::U32 ? rhs.ImmediateU32() == identity : rhs.ImmediateU64() == identity)) {
        replaceWith(inst, lhs);
        return true;
    }
    return false;
}

bool foldSelect(IrValue& inst) {
    auto& condition = resolveArg(inst, 0);
    if (isImmediate(condition, IrType::U1)) {
        replaceWith(inst, resolveArg(inst, condition.ImmediateBool() ? 1 : 2));
        return true;
    }
    bool folded = false;
    for (std::size_t arm = 1; arm <= 2; arm++) {
        auto& nested = resolveArg(inst, arm);
        if (nested.Opcode() == inst.Opcode() && nested.ArgumentCount() == 3u && &resolveArg(nested, 0) == &condition) {
            inst.ReplaceArgument(arm, &resolveArg(nested, arm));
            folded = true;
        }
    }
    auto& trueValue = resolveArg(inst, 1);
    auto& falseValue = resolveArg(inst, 2);
    if (trueValue == falseValue) {
        replaceWith(inst, trueValue);
        return true;
    }
    return folded;
}

bool foldPhi(IrValue& inst) {
    IrValue* same = nullptr;
    for (std::size_t index = 0; index < inst.ArgumentCount(); index++) {
        auto* value = inst.Argument(index)->Resolve();
        if (value == &inst) {
            continue;
        }
        if (same == nullptr) {
            same = value;
        } else if (same != value && !(same->HasImmediate() && value->HasImmediate() && *same == *value)) {
            return false;
        }
    }
    if (same == nullptr) {
        return false;
    }
    replaceWith(inst, *same);
    return true;
}

bool foldBitCast(IrBuilder& builder, IrValue& inst, IrOpcode reverse) {
    auto& value = resolveArg(inst, 0);
    if (isImmediate(value, IrType::F32) && inst.Opcode() == IrOpcode::BitCastU32F32) {
        replaceWith(inst, builder.Constant(std::bit_cast<std::uint32_t>(value.ImmediateF32())));
        return true;
    }
    if (isImmediate(value, IrType::U32) && inst.Opcode() == IrOpcode::BitCastF32U32) {
        replaceWith(inst, builder.ConstantF32(std::bit_cast<float>(value.ImmediateU32())));
        return true;
    }
    if (isImmediate(value, IrType::F16) && inst.Opcode() == IrOpcode::BitCastU16F16) {
        replaceWith(inst, builder.ConstantU16(value.ImmediateF16Bits()));
        return true;
    }
    if (isImmediate(value, IrType::U16) && inst.Opcode() == IrOpcode::BitCastF16U16) {
        replaceWith(inst, builder.ConstantF16(value.ImmediateU16()));
        return true;
    }
    if (value.Opcode() == reverse) {
        replaceWith(inst, *value.Argument(0));
        return true;
    }
    return false;
}

bool foldCompositeExtract(IrBuilder& builder, IrValue& inst, IrOpcode construct, std::size_t components) {
    auto& composite = resolveArg(inst, 0);
    auto& index = resolveArg(inst, 1);
    if (!isImmediate(index, IrType::U32) || index.ImmediateU32() >= components) {
        return false;
    }
    const auto component = index.ImmediateU32();
    if (isImmediate(composite, IrType::U64)) {
        replaceWith(inst, builder.Constant(static_cast<std::uint32_t>(composite.ImmediateU64() >> (component * 32u))));
        return true;
    }
    if (composite.Opcode() == construct) {
        replaceWith(inst, *composite.Argument(component));
        return true;
    }
    if (component < 2u && composite.Opcode() == IrOpcode::IAddCarry32) {
        auto& lhs = resolveArg(composite, 0);
        auto& rhs = resolveArg(composite, 1);
        if (isImmediate(lhs, IrType::U32) && isImmediate(rhs, IrType::U32)) {
            const auto sum = static_cast<std::uint64_t>(lhs.ImmediateU32()) + rhs.ImmediateU32();
            replaceWith(inst, builder.Constant(component == 0u ? static_cast<std::uint32_t>(sum) : static_cast<std::uint32_t>(sum >> 32u)));
            return true;
        }
    }
    return false;
}

bool forwardsWord(const IrUse& use) {
    return use.user->Opcode() == IrOpcode::Identity || use.user->Opcode() == IrOpcode::Phi || (use.user->Opcode() == IrOpcode::SelectU32 && use.operand != 0u);
}

void collectReaders(IrValue& value, std::vector<IrUse>& readers) {
    std::unordered_set<const IrValue*> visited{&value};
    std::vector<std::pair<IrUseList, std::size_t>> pending;
    pending.emplace_back(value.OperandUses(), 0u);
    while (!pending.empty()) {
        auto& [uses, next] = pending.back();
        if (next == uses.size()) {
            pending.pop_back();
            continue;
        }
        const IrUse use = uses[next++];
        if (!forwardsWord(use)) {
            readers.push_back(use);
        } else if (visited.insert(use.user).second) {
            pending.emplace_back(use.user->OperandUses(), 0u);
        }
    }
}

constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 2> packedFields {{{8u, 4u}, {16u, 13u}}};

std::optional<std::size_t> packedField(std::uint32_t offset, std::uint32_t count) {
    const auto field = std::ranges::find_if(packedFields, [&](const auto& candidate) {
        return offset >= candidate.first && count <= candidate.second && offset - candidate.first <= candidate.second - count;
    });
    if (field == packedFields.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(field - packedFields.begin());
}

bool isPackedAncillary(const IrValue& builtin) {
    auto& kind = resolveArg(builtin, 0);
    return isImmediate(kind, IrType::U32) && static_cast<StageInputKind>(kind.ImmediateU32()) == StageInputKind::PackedAncillary;
}

IrValue& insertBefore(IrProgram& program, IrValue& next, IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
    IrValue& created = program.CreateValue(opcode, IrType::U32);
    created.AddArgument(&lhs);
    created.AddArgument(&rhs);
    next.Parent()->InsertInstructionBefore(&next, &created);
    return created;
}

IrValue& insertBuiltin(IrProgram& program, IrBuilder& builder, IrValue& next, StageInputKind kind) {
    return insertBefore(program, next, IrOpcode::GetBuiltin, builder.Constant(static_cast<std::uint32_t>(kind)), builder.Constant(0u));
}

IrValue& sampleNumber(IrProgram& program, IrBuilder& builder, IrValue& next) {
    return program.Metadata().pixelIteratesSamples ? insertBuiltin(program, builder, next, StageInputKind::SampleId) : builder.Constant(0u);
}

void lowerPackedAncillary(IrProgram& program, IrBuilder& builder, IrValue& ancillary) {
    if (!isPackedAncillary(ancillary)) {
        return;
    }
    const IrUseList uses = ancillary.OperandUses();
    std::vector<IrUse> direct;
    std::vector<IrUse> forwarded;
    for (const IrUse& use : uses) {
        if (forwardsWord(use)) {
            forwarded.push_back(use);
        } else {
            direct.push_back(use);
        }
    }
    std::vector<IrUse> readers = direct;
    for (const IrUse& use : forwarded) {
        collectReaders(*use.user, readers);
    }
    const auto extractsField = [](const IrUse& use) {
        const IrValue& user = *use.user;
        if ((user.Opcode() != IrOpcode::BitFieldUExtract && user.Opcode() != IrOpcode::BitFieldSExtract) || use.operand != 0u) {
            return false;
        }
        auto& offset = resolveArg(user, 1);
        auto& count = resolveArg(user, 2);
        return isImmediate(offset, IrType::U32) && isImmediate(count, IrType::U32) && count.ImmediateU32() != 0u &&
               packedField(offset.ImmediateU32(), count.ImmediateU32()).has_value();
    };
    const bool whole = std::ranges::all_of(readers, extractsField);
    if (!whole) {
        std::vector<IrUse> extracts;
        const auto collectExtracts = [&](const auto& self, IrValue& value) -> void {
            for (const IrUse& use : value.OperandUses()) {
                if (use.user->Opcode() == IrOpcode::Identity) {
                    self(self, *use.user);
                } else if (extractsField(use)) {
                    extracts.push_back(use);
                }
            }
        };
        collectExtracts(collectExtracts, ancillary);
        direct = std::move(extracts);
        forwarded.clear();
    }
    std::array<IrValue*, 2> fields {};
    const auto field = [&](std::size_t index) -> IrValue& {
        if (fields[index] == nullptr && index == 1u) fields[index] = &insertBefore(program, ancillary, IrOpcode::IMul32, insertBuiltin(program, builder, ancillary, StageInputKind::Layer), insertBuiltin(program, builder, ancillary, StageInputKind::LayerExported));
        if (fields[index] == nullptr) {
            fields[index] = &sampleNumber(program, builder, ancillary);
        }
        return *fields[index];
    };
    const auto emit = [&](IrOpcode opcode, IrValue& lhs, IrValue& rhs) -> IrValue& {
        return insertBefore(program, ancillary, opcode, lhs, rhs);
    };
    if (!forwarded.empty()) {
        IrValue& sample = emit(IrOpcode::ShiftLeftLogical32, field(0u), builder.Constant(8u));
        IrValue& layer = emit(IrOpcode::ShiftLeftLogical32, field(1u), builder.Constant(16u));
        IrValue& packed = emit(IrOpcode::BitwiseOr32, sample, layer);
        for (const IrUse& use : forwarded) {
            use.user->ReplaceArgument(use.operand, &packed);
        }
    }
    for (const IrUse& use : direct) {
        IrValue& user = *use.user;
        auto& offset = resolveArg(user, 1);
        auto& count = resolveArg(user, 2);
        const auto index = *packedField(offset.ImmediateU32(), count.ImmediateU32());
        user.ReplaceArgument(0, &field(index));
        user.ReplaceArgument(1, &builder.Constant(offset.ImmediateU32() - packedFields[index].first));
    }
}

bool tryFoldValue(IrProgram& program, IrValue& value, OperandFacts& facts) {
    IrBuilder builder(program);
    switch (value.Opcode()) {
        case IrOpcode::Phi: return foldPhi(value);
        case IrOpcode::SelectU1:
        case IrOpcode::SelectF32:
        case IrOpcode::SelectU32: return foldSelect(value);
        case IrOpcode::BitFieldInsert: {
            auto& base = resolveArg(value, 0);
            auto& insert = resolveArg(value, 1);
            auto& offset = resolveArg(value, 2);
            auto& count = resolveArg(value, 3);
            if (!isImmediate(base, IrType::U32) || !isImmediate(insert, IrType::U32) || !isImmediate(offset, IrType::U32) || !isImmediate(count, IrType::U32) || offset.ImmediateU32() > 32u || count.ImmediateU32() > 32u - offset.ImmediateU32()) {
                return false;
            }
            if (count.ImmediateU32() == 0u) {
                replaceWith(value, base);
                return true;
            }
            const auto mask = count.ImmediateU32() == 32u ? UINT32_MAX : ((std::uint32_t{1} << count.ImmediateU32()) - 1u) << offset.ImmediateU32();
            replaceWith(value, builder.Constant((base.ImmediateU32() & ~mask) | ((insert.ImmediateU32() << offset.ImmediateU32()) & mask)));
            return true;
        }
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract: {
            if (isImmediate(resolveArg(value, 1), IrType::U32) && isImmediate(resolveArg(value, 2), IrType::U32)) {
                const auto fieldOffset = resolveArg(value, 1).ImmediateU32();
                const auto fieldCount = resolveArg(value, 2).ImmediateU32();
                if (fieldCount > 0u && fieldCount < 32u && fieldOffset < 32u && fieldCount <= 32u - fieldOffset) readThroughUnusedBits(value, 0, ((std::uint32_t{1} << fieldCount) - 1u) << fieldOffset);
            }
            if (resolveArg(value, 0).Opcode() == IrOpcode::ShiftLeftLogical32 && isImmediate(resolveArg(value, 1), IrType::U32) && isImmediate(resolveArg(value, 2), IrType::U32)) {
                auto& shifted = resolveArg(value, 0);
                auto& amount = resolveArg(shifted, 1);
                const auto fieldOffset = resolveArg(value, 1).ImmediateU32();
                if (isImmediate(amount, IrType::U32) && amount.ImmediateU32() > 0u && amount.ImmediateU32() < 32u && fieldOffset >= amount.ImmediateU32() && fieldOffset < 32u) {
                    value.ReplaceArgument(1, &builder.Constant(fieldOffset - amount.ImmediateU32()));
                    value.ReplaceArgument(0, &resolveArg(shifted, 0));
                }
            }
            auto& source = resolveArg(value, 0);
            auto& offset = resolveArg(value, 1);
            auto& count = resolveArg(value, 2);
            if (source.Opcode() == IrOpcode::ShiftLeftLogical32 && isImmediate(offset, IrType::U32) && isImmediate(count, IrType::U32)) {
                auto& shift = resolveArg(source, 1);
                if (isImmediate(shift, IrType::U32) && shift.ImmediateU32() < 32u && offset.ImmediateU32() <= shift.ImmediateU32() && count.ImmediateU32() <= shift.ImmediateU32() - offset.ImmediateU32()) {
                    replaceWith(value, builder.Constant(0u));
                    return true;
                }
            }
            if (value.Opcode() == IrOpcode::BitFieldUExtract && isImmediateU32(offset, 0u) && isImmediate(count, IrType::U32) && count.ImmediateU32() < 32u && !isImmediate(source, IrType::U32) && (possibleBits(source) >> count.ImmediateU32()) == 0u && !feedsMultiply(value)) {
                replaceWith(value, source);
                return true;
            }
            if (!isImmediate(source, IrType::U32) || !isImmediate(offset, IrType::U32) || !isImmediate(count, IrType::U32) || offset.ImmediateU32() > 32u || count.ImmediateU32() > 32u - offset.ImmediateU32()) {
                return false;
            }
            if (count.ImmediateU32() == 0u) {
                replaceWith(value, builder.Constant(0u));
                return true;
            }
            if (value.Opcode() == IrOpcode::BitFieldUExtract) {
                const auto mask = count.ImmediateU32() == 32u ? UINT32_MAX : (std::uint32_t{1} << count.ImmediateU32()) - 1u;
                replaceWith(value, builder.Constant((source.ImmediateU32() >> offset.ImmediateU32()) & mask));
                return true;
            }
            const auto left = 32u - offset.ImmediateU32() - count.ImmediateU32();
            const auto bits = source.ImmediateU32() << left;
            replaceWith(value, builder.Constant(static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(bits) >> (left + offset.ImmediateU32()))));
            return true;
        }
        case IrOpcode::BitCastU16F16: return foldBitCast(builder, value, IrOpcode::BitCastF16U16);
        case IrOpcode::BitCastF16U16: return foldBitCast(builder, value, IrOpcode::BitCastU16F16);
        case IrOpcode::BitCastU32F32: return foldBitCast(builder, value, IrOpcode::BitCastF32U32);
        case IrOpcode::BitCastF32U32: return foldBitCast(builder, value, IrOpcode::BitCastU32F32);
        case IrOpcode::ConvertU16U32: {
            readThroughUnusedBits(value, 0, 0xffffu);
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U32)) {
                return false;
            }
            replaceWith(value, builder.ConstantU16(static_cast<std::uint16_t>(operand.ImmediateU32())));
            return true;
        }
        case IrOpcode::ConvertU32U16: {
            auto& operand = resolveArg(value, 0);
            if (isImmediate(operand, IrType::U16)) {
                replaceWith(value, builder.Constant(static_cast<std::uint32_t>(operand.ImmediateU16())));
                return true;
            }
            return false;
        }
        case IrOpcode::ConvertU8U32: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U32)) {
                return false;
            }
            replaceWith(value, builder.ConstantU8(static_cast<std::uint8_t>(operand.ImmediateU32())));
            return true;
        }
        case IrOpcode::ConvertU32U8: {
            auto& operand = resolveArg(value, 0);
            if (isImmediate(operand, IrType::U8)) {
                replaceWith(value, builder.Constant(static_cast<std::uint32_t>(operand.ImmediateU8())));
                return true;
            }
            return false;
        }
        case IrOpcode::CompositeExtractU64: return foldCompositeExtract(builder, value, IrOpcode::CompositeConstructU64, 2);
        case IrOpcode::CompositeExtractU32x2: return foldCompositeExtract(builder, value, IrOpcode::CompositeConstructU32x2, 2);
        case IrOpcode::CompositeExtractU32x3: return foldCompositeExtract(builder, value, IrOpcode::CompositeConstructU32x3, 3);
        case IrOpcode::CompositeExtractU32x4: return foldCompositeExtract(builder, value, IrOpcode::CompositeConstructU32x4, 4);
        case IrOpcode::CompositeConstructU64: {
            auto& low = resolveArg(value, 0);
            auto& high = resolveArg(value, 1);
            if (!isImmediate(low, IrType::U32) || !isImmediate(high, IrType::U32)) {
                return false;
            }
            replaceWith(value, builder.ConstantU64(static_cast<std::uint64_t>(low.ImmediateU32()) | (static_cast<std::uint64_t>(high.ImmediateU32()) << 32u)));
            return true;
        }
        case IrOpcode::IAdd32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a + b; })) {
                return true;
            }
            return replaceBinaryIdentity(value, IrType::U32, 0u);
        }
        case IrOpcode::IAdd64: {
            if (foldU64(builder, value, [](std::uint64_t a, std::uint64_t b) { return a + b; })) {
                return true;
            }
            return replaceBinaryIdentity(value, IrType::U64, 0u);
        }
        case IrOpcode::ISub32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a - b; })) {
                return true;
            }
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(rhs, IrType::U32) && rhs.ImmediateU32() == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            return false;
        }
        case IrOpcode::ISub64: {
            if (foldU64(builder, value, [](std::uint64_t a, std::uint64_t b) { return a - b; })) {
                return true;
            }
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(rhs, IrType::U64) && rhs.ImmediateU64() == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            return false;
        }
        case IrOpcode::IMul32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a * b; })) {
                return true;
            }
            return replaceBinaryIdentity(value, IrType::U32, 1u);
        }
        case IrOpcode::IMul64: {
            if (foldU64(builder, value, [](std::uint64_t a, std::uint64_t b) { return a * b; })) {
                return true;
            }
            return replaceBinaryIdentity(value, IrType::U64, 1u);
        }
        case IrOpcode::WqmU64: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U64)) {
                return false;
            }
            const auto expand = [](std::uint32_t word) {
                auto quads = word | (word >> 1u);
                quads |= quads >> 2u;
                return (quads & 0x11111111u) * 0x0fu;
            };
            const auto low = expand(static_cast<std::uint32_t>(operand.ImmediateU64()));
            const auto high = expand(static_cast<std::uint32_t>(operand.ImmediateU64() >> 32u));
            replaceWith(value, builder.ConstantU64(static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u)));
            return true;
        }
        case IrOpcode::UDiv32: {
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(rhs, IrType::U32) && rhs.ImmediateU32() == 1u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            if (isImmediate(rhs, IrType::U32) && rhs.ImmediateU32() != 0u) {
                return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a / b; });
            }
            return false;
        }
        case IrOpcode::SMulHi: {
            return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) {
                const auto product = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(a)) * static_cast<std::int64_t>(std::bit_cast<std::int32_t>(b));
                return static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32u);
            });
        }
        case IrOpcode::UMulHi: {
            return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return static_cast<std::uint32_t>((static_cast<std::uint64_t>(a) * b) >> 32u); });
        }
        case IrOpcode::IAbs32: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U32)) {
                return false;
            }
            replaceWith(value, builder.Constant((operand.ImmediateU32() & 0x80000000u) != 0u ? 0u - operand.ImmediateU32() : operand.ImmediateU32()));
            return true;
        }
        case IrOpcode::ShiftLeftLogical32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a << (b & 31u); })) {
                return true;
            }
            if (isImmediate(resolveArg(value, 1), IrType::U32)) readThroughUnusedBits(value, 0, UINT32_MAX >> (resolveArg(value, 1).ImmediateU32() & 31u));
            auto& shift = resolveArg(value, 1);
            if (isImmediate(shift, IrType::U32) && (shift.ImmediateU32() & 31u) == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            return false;
        }
        case IrOpcode::ShiftRightLogical32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a >> (b & 31u); })) {
                return true;
            }
            auto& shift = resolveArg(value, 1);
            if (isImmediate(shift, IrType::U32) && (shift.ImmediateU32() & 31u) == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            if (!isImmediate(shift, IrType::U32)) return false;
            const auto amount = shift.ImmediateU32() & 31u;
            readThroughUnusedBits(value, 0, UINT32_MAX << amount);
            auto& shifted = resolveArg(value, 0);
            if (shifted.Opcode() == IrOpcode::ShiftLeftLogical32 && isImmediate(resolveArg(shifted, 1), IrType::U32) && (resolveArg(shifted, 1).ImmediateU32() & 31u) == amount) {
                auto& inner = resolveArg(shifted, 0);
                if ((possibleBits(inner) & ~(UINT32_MAX >> amount)) == 0u) {
                    replaceWith(value, inner);
                    return true;
                }
                value.ReplaceOpcode(IrOpcode::BitwiseAnd32);
                value.ReplaceArgument(0, &inner);
                value.ReplaceArgument(1, &builder.Constant(UINT32_MAX >> amount));
                foldRedundantMask(value);
                return true;
            }
            return false;
        }
        case IrOpcode::ShiftRightArithmetic32: {
            if (isImmediate(resolveArg(value, 1), IrType::U32)) readThroughUnusedBits(value, 0, UINT32_MAX << (resolveArg(value, 1).ImmediateU32() & 31u));
            return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(a) >> (b & 31u)); });
        }
        case IrOpcode::ShiftLeftLogical64: {
            if (foldU64Shift(builder, value, [](std::uint64_t a, std::uint32_t b) { return a << (b & 63u); })) {
                return true;
            }
            auto& shift = resolveArg(value, 1);
            if (isImmediate(shift, IrType::U32) && (shift.ImmediateU32() & 63u) == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            return false;
        }
        case IrOpcode::ShiftRightLogical64: {
            if (foldU64Shift(builder, value, [](std::uint64_t a, std::uint32_t b) { return a >> (b & 63u); })) {
                return true;
            }
            auto& shift = resolveArg(value, 1);
            if (isImmediate(shift, IrType::U32) && (shift.ImmediateU32() & 63u) == 0u) {
                replaceWith(value, resolveArg(value, 0));
                return true;
            }
            return false;
        }
        case IrOpcode::ShiftRightArithmetic64: {
            return foldU64Shift(builder, value, [](std::uint64_t a, std::uint32_t b) { return static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(a) >> (b & 63u)); });
        }
        case IrOpcode::BitwiseAnd32: {
            if (foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return a & b; })) {
                return true;
            }
            if (replaceBinaryIdentity(value, IrType::U32, 0xffffffffu)) return true;
            if (foldRedundantMask(value)) return true;
            bool changed = false;
            for (std::size_t side = 0; side < 2u; ++side) {
                if (isImmediate(resolveArg(value, side), IrType::U32)) changed |= readThroughUnusedBits(value, 1u - side, resolveArg(value, side).ImmediateU32());
            }
            if (changed) foldRedundantMask(value);
            return changed;
        }
        case IrOpcode::BitwiseAnd64: {
            if (foldU64(builder, value, [](std::uint64_t a, std::uint64_t b) { return a & b; })) {
                return true;
            }
            return replaceBinaryIdentity(value, IrType::U64, UINT64_MAX);
        }
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32: {
            const auto opcode = value.Opcode();
            if (foldU32(builder, value, [opcode](std::uint32_t a, std::uint32_t b) { return opcode == IrOpcode::BitwiseOr32 ? a | b : a ^ b; })) {
                return true;
            }
            if (replaceBinaryIdentity(value, IrType::U32, 0u)) return true;
            return opcode == IrOpcode::BitwiseOr32 && mergeMaskedParts(program, value);
        }
        case IrOpcode::BitwiseNot32: {
            auto& operand = resolveArg(value, 0);
            if (isImmediate(operand, IrType::U32)) {
                replaceWith(value, builder.Constant(~operand.ImmediateU32()));
                return true;
            }
            if (operand.Opcode() == IrOpcode::BitwiseNot32) {
                replaceWith(value, *operand.Argument(0));
                return true;
            }
            return false;
        }
        case IrOpcode::BitCount32: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U32)) {
                return false;
            }
            replaceWith(value, builder.Constant(static_cast<std::uint32_t>(std::popcount(operand.ImmediateU32()))));
            return true;
        }
        case IrOpcode::BitCount64: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U64)) {
                return false;
            }
            replaceWith(value, builder.Constant(static_cast<std::uint32_t>(std::popcount(operand.ImmediateU64()))));
            return true;
        }
        case IrOpcode::BitReverse32: {
            auto& operand = resolveArg(value, 0);
            if (!isImmediate(operand, IrType::U32)) {
                return false;
            }
            const std::uint32_t word = operand.ImmediateU32();
            std::uint32_t reversed = 0;
            for (std::uint32_t bit = 0; bit < 32; ++bit) {
                reversed |= ((word >> bit) & 1u) << (31u - bit);
            }
            replaceWith(value, builder.Constant(reversed));
            return true;
        }
        case IrOpcode::SMin32:
        case IrOpcode::SMax32: {
            const auto opcode = value.Opcode();
            return foldU32(builder, value, [opcode](std::uint32_t a, std::uint32_t b) {
                const auto lhs = std::bit_cast<std::int32_t>(a);
                const auto rhs = std::bit_cast<std::int32_t>(b);
                return opcode == IrOpcode::SMin32 ? (lhs < rhs ? a : b) : (lhs > rhs ? a : b);
            });
        }
        case IrOpcode::UMin32: {
            return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return std::min(a, b); });
        }
        case IrOpcode::UMax32: {
            return foldU32(builder, value, [](std::uint32_t a, std::uint32_t b) { return std::max(a, b); });
        }
        case IrOpcode::IEqual32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a == b; }) || foldEqualityByValueSet(builder, value, true, facts);
        case IrOpcode::INotEqual32: return foldConstantBitTest(builder, value) || foldStartingLaneBitTest(program, builder, value) || foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a != b; }) || foldEqualityByValueSet(builder, value, false, facts);
        case IrOpcode::ULessThan32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a < b; }) || foldULessThanByRange(builder, value, facts);
        case IrOpcode::ULessThanEqual32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a <= b; });
        case IrOpcode::UGreaterThan32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a > b; });
        case IrOpcode::UGreaterThanEqual32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a >= b; });
        case IrOpcode::SLessThan32:
        case IrOpcode::SLessThanEqual32:
        case IrOpcode::SGreaterThan32:
        case IrOpcode::SGreaterThanEqual32: {
            const auto opcode = value.Opcode();
            return foldU32Compare(builder, value, [opcode](std::uint32_t a, std::uint32_t b) {
                const auto lhs = std::bit_cast<std::int32_t>(a);
                const auto rhs = std::bit_cast<std::int32_t>(b);
                switch (opcode) {
                    case IrOpcode::SLessThan32: return lhs < rhs;
                    case IrOpcode::SLessThanEqual32: return lhs <= rhs;
                    case IrOpcode::SGreaterThan32: return lhs > rhs;
                    default: return lhs >= rhs;
                }
            });
        }
        case IrOpcode::IEqual64: return foldU64Compare(builder, value, [](std::uint64_t a, std::uint64_t b) { return a == b; });
        case IrOpcode::INotEqual64: return foldU64Compare(builder, value, [](std::uint64_t a, std::uint64_t b) { return a != b; });
        case IrOpcode::ULessThan64: return foldU64Compare(builder, value, [](std::uint64_t a, std::uint64_t b) { return a < b; });
        case IrOpcode::UGreaterThan64: return foldU64Compare(builder, value, [](std::uint64_t a, std::uint64_t b) { return a > b; });
        case IrOpcode::SLessThan64: {
            return foldU64Compare(builder, value, [](std::uint64_t a, std::uint64_t b) { return std::bit_cast<std::int64_t>(a) < std::bit_cast<std::int64_t>(b); });
        }
        case IrOpcode::LogicalAnd: {
            if (foldLogical(builder, value, [](bool a, bool b) { return a && b; })) {
                return true;
            }
            auto& lhs = resolveArg(value, 0);
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(lhs, IrType::U1)) {
                replaceWith(value, lhs.ImmediateBool() ? rhs : lhs);
                return true;
            }
            if (isImmediate(rhs, IrType::U1)) {
                replaceWith(value, rhs.ImmediateBool() ? lhs : rhs);
                return true;
            }
            return false;
        }
        case IrOpcode::LogicalOr: {
            if (foldLogical(builder, value, [](bool a, bool b) { return a || b; })) {
                return true;
            }
            auto& lhs = resolveArg(value, 0);
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(lhs, IrType::U1)) {
                replaceWith(value, lhs.ImmediateBool() ? lhs : rhs);
                return true;
            }
            if (isImmediate(rhs, IrType::U1)) {
                replaceWith(value, rhs.ImmediateBool() ? rhs : lhs);
                return true;
            }
            return false;
        }
        case IrOpcode::LogicalXor: {
            if (foldLogical(builder, value, [](bool a, bool b) { return a != b; })) {
                return true;
            }
            auto& lhs = resolveArg(value, 0);
            auto& rhs = resolveArg(value, 1);
            if (isImmediate(lhs, IrType::U1) && !lhs.ImmediateBool()) {
                replaceWith(value, rhs);
                return true;
            }
            if (isImmediate(rhs, IrType::U1) && !rhs.ImmediateBool()) {
                replaceWith(value, lhs);
                return true;
            }
            return false;
        }
        case IrOpcode::LogicalNot: {
            auto& operand = resolveArg(value, 0);
            if (isImmediate(operand, IrType::U1)) {
                replaceWith(value, builder.ConstantBool(!operand.ImmediateBool()));
                return true;
            }
            if (operand.Opcode() == IrOpcode::LogicalNot) {
                replaceWith(value, *operand.Argument(0));
                return true;
            }
            return false;
        }
        default: return false;
    }
}

} // namespace

void ConstantFolder::Fold(IrProgram& program, std::span<IrBlock* const> blocks) const {
    OperandFacts facts;
    for (IrBlock* block : blocks) {
        for (IrValue* inst : block->Instructions()) {
            const auto success = tryFoldValue(program, *inst, facts);
        }
    }
    std::vector<IrValue*> phis;
    for (IrBlock* block : blocks) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::Phi) {
                phis.push_back(inst);
            }
        }
    }
    bool folded = true;
    while (folded) {
        folded = false;
        for (IrValue* phi : phis) {
            if (phi->Opcode() == IrOpcode::Phi && foldPhi(*phi)) {
                folded = true;
            }
        }
    }
    std::vector<IrValue*> words;
    for (IrBlock* block : blocks) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetBuiltin) {
                words.push_back(inst);
            }
        }
    }
    IrBuilder builder(program);
    for (IrValue* word : words) {
        lowerPackedAncillary(program, builder, *word);
    }
}

void ConstantFolder::Fold(IrProgram& program) const {
    Fold(program, program.BlockOrder());
}

bool ConstantFolder::FoldVertexLaneBounds(IrProgram& program, std::uint32_t hostSubgroupSize, std::uint32_t spirvVersion) const {
    if (program.Resources().stage != IrShaderStage::Vertex || hostSubgroupSize != 32u || spirvVersion < 0x00010000u || spirvVersion >= 0x00010600u) return false;
    bool changed = false;
    for (IrBlock* block : program.BlockOrder()) {
        for (IrValue* value : block->Instructions()) {
            if (value->Opcode() != IrOpcode::SelectU32 || value->ArgumentCount() != 3u) continue;
            const auto& half = resolveArg(*value, 0);
            auto& low = resolveArg(*value, 1);
            if (half.Opcode() != IrOpcode::ULessThan32 || half.ArgumentCount() != 2u || !isLaneIndex(resolveArg(half, 0)) || !isImmediateU32(resolveArg(half, 1), 32u) || !isImmediate(low, IrType::U32) || !isImmediate(resolveArg(*value, 2), IrType::U32)) continue;
            replaceWith(*value, low);
            changed = true;
        }
    }
    return changed;
}

bool ConstantFolder::ComposePackedAncillary(IrProgram& program) const {
    std::vector<IrValue*> words;
    for (IrBlock* block : program.BlockOrder()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetBuiltin && inst->HasUses() && isPackedAncillary(*inst)) {
                words.push_back(inst);
            }
        }
    }
    IrBuilder builder(program);
    for (IrValue* word : words) {
        IrValue& primitive = insertBuiltin(program, builder, *word, StageInputKind::RasterPrimitive);
        IrValue& sample = insertBefore(program, *word, IrOpcode::ShiftLeftLogical32, sampleNumber(program, builder, *word), builder.Constant(8u));
        IrValue& exportedLayer = insertBefore(program, *word, IrOpcode::IMul32, insertBuiltin(program, builder, *word, StageInputKind::Layer), insertBuiltin(program, builder, *word, StageInputKind::LayerExported));
        IrValue& layer = insertBefore(program, *word, IrOpcode::ShiftLeftLogical32, exportedLayer, builder.Constant(16u));
        IrValue& fields = insertBefore(program, *word, IrOpcode::BitwiseOr32, sample, layer);
        word->ReplaceAllUsesWith(&insertBefore(program, *word, IrOpcode::BitwiseOr32, primitive, fields));
    }
    return !words.empty();
}

}
