#include "Optimization/ConstantFolder.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
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

bool holdsStartingLanes(const IrProgram& program, const IrValue& word, std::uint32_t index, std::uint32_t depth = 0) {
    if (depth > 8u) return false;
    if (word.Opcode() == IrOpcode::BitwiseAnd32) {
        return holdsStartingLanes(program, resolveArg(word, 0), index, depth + 1u) && holdsStartingLanes(program, resolveArg(word, 1), index, depth + 1u);
    }
    if (word.Opcode() == IrOpcode::BitwiseOr32) {
        return holdsStartingLanes(program, resolveArg(word, 0), index, depth + 1u) || holdsStartingLanes(program, resolveArg(word, 1), index, depth + 1u);
    }
    if (word.Opcode() != IrOpcode::CompositeExtractU32x4 || !isImmediateU32(resolveArg(word, 1), index)) return false;
    const auto& ballot = resolveArg(word, 0);
    if (ballot.Opcode() != IrOpcode::Ballot || !runsAllStartingLanes(program, ballot.Parent())) return false;
    const auto& condition = resolveArg(ballot, 0);
    if (isImmediate(condition, IrType::U1)) return condition.ImmediateBool();
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

bool foldStartingQuadBitTest(const IrProgram& program, IrBuilder& builder, IrValue& inst) {
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
    if (low->Opcode() != IrOpcode::CompositeExtractU64 || !isImmediateU32(resolveArg(*low, 1), 0u)) return false;
    const auto& wqm = resolveArg(*low, 0);
    if (high != nullptr && (high->Opcode() != IrOpcode::CompositeExtractU64 || &resolveArg(*high, 0) != &wqm || !isImmediateU32(resolveArg(*high, 1), 1u))) return false;
    if (wqm.Opcode() != IrOpcode::WqmU64) return false;
    const auto& lanes = resolveArg(wqm, 0);
    if (lanes.Opcode() != IrOpcode::CompositeConstructU64 || !holdsStartingLanes(program, resolveArg(lanes, 0), 0u)) return false;
    if (high != nullptr && !holdsStartingLanes(program, resolveArg(lanes, 1), 1u)) return false;
    replaceWith(inst, builder.ConstantBool(true));
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
    auto& trueValue = resolveArg(inst, 1);
    auto& falseValue = resolveArg(inst, 2);
    if (isImmediate(condition, IrType::U1)) {
        replaceWith(inst, condition.ImmediateBool() ? trueValue : falseValue);
        return true;
    }
    if (trueValue == falseValue) {
        replaceWith(inst, trueValue);
        return true;
    }
    return false;
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

void collectReaders(IrValue& value, std::vector<IrUse>& readers, std::vector<const IrValue*>& visited) {
    if (std::ranges::find(visited, &value) != visited.end()) {
        return;
    }
    visited.push_back(&value);
    const std::vector<IrUse> uses = value.OperandUses();
    for (const IrUse& use : uses) {
        if (forwardsWord(use)) {
            collectReaders(*use.user, readers, visited);
        } else {
            readers.push_back(use);
        }
    }
}

void collectReaders(IrValue& value, std::vector<IrUse>& readers) {
    std::vector<const IrValue*> visited;
    collectReaders(value, readers, visited);
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

void lowerPackedAncillary(IrProgram& program, IrBuilder& builder, IrValue& ancillary) {
    if (!isPackedAncillary(ancillary)) {
        return;
    }
    const std::vector<IrUse> uses = ancillary.OperandUses();
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
            fields[index] = &insertBuiltin(program, builder, ancillary, StageInputKind::SampleId);
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

} // namespace

bool ConstantFolder::tryFoldValue(IrProgram& program, IrValue& value) const {
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
            return false;
        }
        case IrOpcode::ShiftRightArithmetic32: {
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
            return replaceBinaryIdentity(value, IrType::U32, 0xffffffffu);
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
            return replaceBinaryIdentity(value, IrType::U32, 0u);
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
        case IrOpcode::IEqual32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a == b; });
        case IrOpcode::INotEqual32: return foldConstantBitTest(builder, value) || foldStartingQuadBitTest(program, builder, value) || foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a != b; });
        case IrOpcode::ULessThan32: return foldU32Compare(builder, value, [](std::uint32_t a, std::uint32_t b) { return a < b; });
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

void ConstantFolder::Fold(IrProgram& program, std::span<IrBlock* const> blocks) const {
    for (IrBlock* block : blocks) {
        for (IrValue* inst : block->Instructions()) {
            const auto success = tryFoldValue(program, *inst);
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
        IrValue& sample = insertBefore(program, *word, IrOpcode::ShiftLeftLogical32, insertBuiltin(program, builder, *word, StageInputKind::SampleId), builder.Constant(8u));
        IrValue& exportedLayer = insertBefore(program, *word, IrOpcode::IMul32, insertBuiltin(program, builder, *word, StageInputKind::Layer), insertBuiltin(program, builder, *word, StageInputKind::LayerExported));
        IrValue& layer = insertBefore(program, *word, IrOpcode::ShiftLeftLogical32, exportedLayer, builder.Constant(16u));
        IrValue& fields = insertBefore(program, *word, IrOpcode::BitwiseOr32, sample, layer);
        word->ReplaceAllUsesWith(&insertBefore(program, *word, IrOpcode::BitwiseOr32, primitive, fields));
    }
    return !words.empty();
}

}
