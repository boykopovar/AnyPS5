#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {
namespace {

struct Pair {
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

std::uint32_t Exact(SpirvEmitterState& state, std::uint32_t result) {
    state.module.AddAnnotation(spv::OpDecorate, result, spv::DecorationNoContraction);
    return result;
}

Pair ExtractPair(SpirvEmitterState& state, std::uint32_t value) {
    Pair result{state.module.AllocateId(), state.module.AllocateId()};
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result.low, value, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result.high, value, 1u);
    return result;
}

std::uint32_t MakePair(SpirvEmitterState& state, std::uint32_t low, std::uint32_t high) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU64(state), result, low, high);
    return result;
}

std::uint32_t CompareEqual64(SpirvEmitterState& state, std::uint32_t lhsValue, std::uint32_t rhsValue, bool notEqual) {
    const auto compare = Binary(state, notEqual ? spv::OpINotEqual : spv::OpIEqual, TypeBoolVector(state, 2), lhsValue, rhsValue);
    return Unary(state, notEqual ? spv::OpAny : spv::OpAll, TypeBool(state), compare);
}

std::uint32_t CompareOrdered64(SpirvEmitterState& state, std::uint32_t lhsValue, std::uint32_t rhsValue, spv::Op highCompare, spv::Op lowCompare) {
    const auto lhs = ExtractPair(state, lhsValue);
    const auto rhs = ExtractPair(state, rhsValue);
    const auto highEqual = Binary(state, spv::OpIEqual, TypeBool(state), lhs.high, rhs.high);
    const auto highResult = Binary(state, highCompare, TypeBool(state), lhs.high, rhs.high);
    const auto lowResult = Binary(state, lowCompare, TypeBool(state), lhs.low, rhs.low);
    const auto lowPath = Binary(state, spv::OpLogicalAnd, TypeBool(state), highEqual, lowResult);
    return Binary(state, spv::OpLogicalOr, TypeBool(state), highResult, lowPath);
}

std::uint32_t EmitMulHigh(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool signedValue) {
    const auto operandType = signedValue ? TypeI32(state) : TypeU32(state);
    const auto pairType = signedValue ? TypeI32Pair(state) : TypeU32Pair(state);
    auto lhsOperand = lhs;
    auto rhsOperand = rhs;
    if (signedValue) {
        lhsOperand = Unary(state, spv::OpBitcast, TypeI32(state), lhs);
        rhsOperand = Unary(state, spv::OpBitcast, TypeI32(state), rhs);
    }
    const auto extended = state.module.AllocateId();
    state.module.AddFunction(signedValue ? spv::OpSMulExtended : spv::OpUMulExtended, pairType, extended, lhsOperand, rhsOperand);
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, operandType, high, extended, 1u);
    return signedValue ? Unary(state, spv::OpBitcast, TypeU32(state), high) : high;
}

std::uint32_t EmitShift64(SpirvEmitterState& state, spv::Op opcode, std::uint32_t value, std::uint32_t shift) {
    const auto pair = ExtractPair(state, value);
    const auto amount = EmitAndConstant(state, shift, 63u);
    const auto wordShift = EmitAndConstant(state, amount, 31u);
    const auto atLeast32 = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), amount, ConstantU32(state, 32u));
    const auto nonzero = Binary(state, spv::OpINotEqual, TypeBool(state), amount, ConstantU32(state, 0u));
    const auto carryCount = EmitAndConstant(state, Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), wordShift), 31u);
    if (opcode == spv::OpShiftLeftLogical) {
        const auto low = Binary(state, opcode, TypeU32(state), pair.low, wordShift);
        const auto carry = Select(state, TypeU32(state), nonzero, Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, carryCount), ConstantU32(state, 0u));
        const auto high = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, opcode, TypeU32(state), pair.high, wordShift), carry);
        return MakePair(state, Select(state, TypeU32(state), atLeast32, ConstantU32(state, 0u), low), Select(state, TypeU32(state), atLeast32, low, high));
    }
    const auto high = Binary(state, opcode, TypeU32(state), pair.high, wordShift);
    const auto carry = Select(state, TypeU32(state), nonzero, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, carryCount), ConstantU32(state, 0u));
    const auto low = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, wordShift), carry);
    const auto fill = opcode == spv::OpShiftRightArithmetic ? Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, 31u)) : ConstantU32(state, 0u);
    return MakePair(state, Select(state, TypeU32(state), atLeast32, high, low), Select(state, TypeU32(state), atLeast32, fill, high));
}

std::uint32_t EmitConstantShift64(SpirvEmitterState& state, spv::Op opcode, std::uint32_t value, std::uint32_t shift) {
    shift &= 63u;
    if (shift == 0u) {
        return value;
    }
    const auto pair = ExtractPair(state, value);
    if (opcode == spv::OpShiftLeftLogical) {
        if (shift < 32u) {
            const auto low = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.low, ConstantU32(state, shift));
            const auto highShifted = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, ConstantU32(state, shift));
            const auto lowCarry = Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, ConstantU32(state, 32u - shift));
            return MakePair(state, low, Binary(state, spv::OpBitwiseOr, TypeU32(state), highShifted, lowCarry));
        }
        const auto high = shift == 32u ? pair.low : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.low, ConstantU32(state, shift - 32u));
        return MakePair(state, ConstantU32(state, 0u), high);
    }
    if (shift < 32u) {
        const auto lowShifted = Binary(state, spv::OpShiftRightLogical, TypeU32(state), pair.low, ConstantU32(state, shift));
        const auto highCarry = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pair.high, ConstantU32(state, 32u - shift));
        const auto low = Binary(state, spv::OpBitwiseOr, TypeU32(state), lowShifted, highCarry);
        return MakePair(state, low, Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, shift)));
    }
    const auto high = opcode == spv::OpShiftRightArithmetic ? Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), pair.high, ConstantU32(state, 31u)) : ConstantU32(state, 0u);
    const auto low = shift == 32u ? pair.high : Binary(state, opcode, TypeU32(state), pair.high, ConstantU32(state, shift - 32u));
    return MakePair(state, low, high);
}

std::uint32_t EmitMinMax3(SpirvEmitterState& state, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool signedValue, bool maxValue) {
    const auto ab = signedValue ? EmitMinMaxI32Value(state, a, b, maxValue) : EmitMinMaxU32Value(state, a, b, maxValue);
    return signedValue ? EmitMinMaxI32Value(state, ab, c, maxValue) : EmitMinMaxU32Value(state, ab, c, maxValue);
}

std::uint32_t EmitMed3(SpirvEmitterState& state, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool signedValue) {
    const auto minimum = EmitMinMax3(state, a, b, c, signedValue, false);
    const auto maximum = EmitMinMax3(state, a, b, c, signedValue, true);
    const auto ab = Binary(state, spv::OpIAdd, TypeU32(state), a, b);
    const auto abc = Binary(state, spv::OpIAdd, TypeU32(state), ab, c);
    return Binary(state, spv::OpISub, TypeU32(state), Binary(state, spv::OpISub, TypeU32(state), abc, minimum), maximum);
}

std::uint32_t EmitFMinMax3(SpirvEmitterState& state, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool maxValue) {
    return EmitMinMaxF32Value(state, EmitMinMaxF32Value(state, a, b, maxValue), c, maxValue);
}

std::uint32_t EmitExt(SpirvEmitterState& state, std::uint32_t type, std::uint32_t opcode, std::initializer_list<std::uint32_t> args) {
    const auto result = state.module.AllocateId();
    std::vector<std::uint32_t> words{spv::OpExtInst, type, result, GlslStd450(state), opcode};
    words.insert(words.end(), args.begin(), args.end());
    state.module.AddFunction(words);
    return result;
}

std::uint32_t EmitF32ToU32(SpirvEmitterState& state, std::uint32_t src, bool signedValue) {
    const auto truncated = EmitTruncF32Value(state, src);
    const auto convertedRaw = state.module.AllocateId();
    if (signedValue) {
        const auto convertedSigned = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertFToS, TypeI32(state), convertedSigned, truncated);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), convertedRaw, convertedSigned);
    } else {
        state.module.AddFunction(spv::OpConvertFToU, TypeU32(state), convertedRaw, truncated);
    }
    const auto nan = EmitClassifyF32(state, src).nan;
    if (signedValue) {
        const auto below = Binary(state, spv::OpFOrdLessThanEqual, TypeBool(state), src, ConstantF32(state, 0xcf000000u));
        const auto above = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), src, ConstantF32(state, 0x4f000000u));
        const auto high = Select(state, TypeU32(state), above, ConstantU32(state, 0x7fffffffu), convertedRaw);
        const auto low = Select(state, TypeU32(state), below, ConstantU32(state, 0x80000000u), high);
        return Select(state, TypeU32(state), nan, ConstantU32(state, 0u), low);
    }
    const auto below = Binary(state, spv::OpFOrdLessThanEqual, TypeBool(state), src, ConstantF32(state, 0u));
    const auto above = Binary(state, spv::OpFOrdGreaterThanEqual, TypeBool(state), src, ConstantF32(state, 0x4f800000u));
    const auto zero = Binary(state, spv::OpLogicalOr, TypeBool(state), nan, below);
    const auto high = Select(state, TypeU32(state), above, ConstantU32(state, 0xffffffffu), convertedRaw);
    return Select(state, TypeU32(state), zero, ConstantU32(state, 0u), high);
}

std::uint32_t EmitDppWriteCondition(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto flags = inst.Flags<DppMoveFlags>();
    const auto lane = EmitSubgroupLocalInvocationId(state);
    const auto bankShift = state.module.AllocateId();
    const auto rowShift = state.module.AllocateId();
    const auto bank = state.module.AllocateId();
    const auto row = state.module.AllocateId();
    const auto bankBit = state.module.AllocateId();
    const auto rowBit = state.module.AllocateId();
    const auto bankHit = state.module.AllocateId();
    const auto rowHit = state.module.AllocateId();
    const auto bankOk = state.module.AllocateId();
    const auto rowOk = state.module.AllocateId();
    const auto masksOk = state.module.AllocateId();
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), bankShift, lane, ConstantU32(state, 2u));
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), rowShift, lane, ConstantU32(state, 4u));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), bank, bankShift, ConstantU32(state, 3u));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, rowShift, ConstantU32(state, 3u));
    state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), bankBit, ConstantU32(state, 1u), bank);
    state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), rowBit, ConstantU32(state, 1u), row);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), bankHit, ConstantU32(state, flags.bankMask), bankBit);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), rowHit, ConstantU32(state, flags.rowMask), rowBit);
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), bankOk, bankHit, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), rowOk, rowHit, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), masksOk, bankOk, rowOk);
    auto writable = masksOk;
    if (!flags.boundControl) {
        const auto target = EmitDppTargetLane(state, flags.control);
        const auto bounded = state.module.AllocateId();
        state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), bounded, writable, target.valid);
        writable = bounded;
        if (!flags.fetchInactive && (flags.control & DppMoveFlags::Lanes8) == 0u) {
            const auto sourceActive = EmitBallotLaneActiveBool(state, ctx.Ballot(inst.Argument(2)), target.lane);
            writable = Binary(state, spv::OpLogicalAnd, TypeBool(state), writable, sourceActive);
        }
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), result, ctx.Arg(inst, 2), writable);
    return result;
}

std::uint32_t EmitDsMaskedLaneRead(SpirvEmitterState& state, std::uint32_t source, std::uint32_t target, std::uint32_t exec) {
    auto lane = target;
    if (state.laneCount == 2) {
        lane = Binary(state, spv::OpBitwiseAnd, TypeU32(state), lane, ConstantU32(state, 31u));
    }
    const auto physicalLane = EmitHostSubgroupLane(state, lane);
    const auto shuffled = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, TypeU32(state), shuffled, ConstantU32(state, spv::ScopeSubgroup), source, physicalLane);
    const auto sourceExec = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, TypeBool(state), sourceExec, ConstantU32(state, spv::ScopeSubgroup), exec, physicalLane);
    const auto sourceActive = Binary(state, spv::OpLogicalAnd, TypeBool(state), sourceExec, EmitSubgroupLaneActiveBool(state, lane));
    return Select(state, TypeU32(state), sourceActive, shuffled, ConstantU32(state, 0u));
}

bool HostSubgroupNarrowerThanWave(const SpirvEmitterState& state) {
    return state.program.Resources().stage != IrShaderStage::Compute && state.laneCount == 1u && state.hostSubgroupSize < state.program.WaveSize();
}

[[noreturn]] void FailOutsideHostSubgroup(const SpirvValueEmitContext& ctx, const IrValue& inst, const std::string& access) {
    const auto& state = ctx.state;
    ctx.Fail(inst, (access + " is outside the " + std::to_string(state.hostSubgroupSize) + "-lane host subgroup that runs this wave" + std::to_string(state.program.WaveSize()) + " program at one lane per invocation").c_str());
}

struct WaveReduction {
    IrOpcode opcode;
    std::uint32_t identity;
    spv::Op reduce;
};

constexpr std::array<WaveReduction, 7> WaveReductions{{
    {IrOpcode::UMin32, 0xffffffffu, spv::OpGroupNonUniformUMin},
    {IrOpcode::UMax32, 0u, spv::OpGroupNonUniformUMax},
    {IrOpcode::SMin32, 0x7fffffffu, spv::OpGroupNonUniformSMin},
    {IrOpcode::SMax32, 0x80000000u, spv::OpGroupNonUniformSMax},
    {IrOpcode::IAdd32, 0u, spv::OpGroupNonUniformIAdd},
    {IrOpcode::BitwiseAnd32, 0xffffffffu, spv::OpGroupNonUniformBitwiseAnd},
    {IrOpcode::BitwiseOr32, 0u, spv::OpGroupNonUniformBitwiseOr},
}};

struct HalfWaveScan {
    const WaveReduction* reduction = nullptr;
    const IrValue* source = nullptr;
};

const IrValue* Resolved(const IrValue* value) {
    return value != nullptr ? value->Resolve() : nullptr;
}

bool Is(const IrValue* value, IrOpcode opcode) {
    return value != nullptr && !value->HasImmediate() && value->Opcode() == opcode;
}

bool IsU32(const IrValue* value, std::uint32_t expected) {
    const auto* resolved = Resolved(value);
    return resolved != nullptr && resolved->HasImmediate() && resolved->Type() == IrType::U32 && resolved->ImmediateU32() == expected;
}

template<typename TMatch>
bool EitherOrder(const IrValue* value, TMatch&& match) {
    return match(Resolved(value->Argument(0)), Resolved(value->Argument(1))) || match(Resolved(value->Argument(1)), Resolved(value->Argument(0)));
}

std::optional<std::array<const IrValue*, 2>> LaneBitWords(const IrValue* bit) {
    const IrValue* masked = nullptr;
    const IrValue* shifted = nullptr;
    if (!Is(bit, IrOpcode::INotEqual32) || !EitherOrder(bit, [&](const IrValue* value, const IrValue* zero) { masked = value; return IsU32(zero, 0u) && Is(value, IrOpcode::BitwiseAnd32); })) return std::nullopt;
    if (!EitherOrder(masked, [&](const IrValue* value, const IrValue* one) { shifted = value; return IsU32(one, 1u) && Is(value, IrOpcode::ShiftRightLogical32); })) return std::nullopt;
    const auto* index = Resolved(shifted->Argument(1));
    if (!Is(index, IrOpcode::BitwiseAnd32) || !EitherOrder(index, [](const IrValue* lane, const IrValue* mask) { return Is(lane, IrOpcode::LaneId) && IsU32(mask, 31u); })) return std::nullopt;
    const auto* word = Resolved(shifted->Argument(0));
    if (!Is(word, IrOpcode::SelectU32)) return std::array<const IrValue*, 2>{word, word};
    const auto* low = Resolved(word->Argument(0));
    if (!Is(low, IrOpcode::ULessThan32) || !Is(Resolved(low->Argument(0)), IrOpcode::LaneId) || !IsU32(low->Argument(1), 32u)) return std::nullopt;
    return std::array<const IrValue*, 2>{Resolved(word->Argument(1)), Resolved(word->Argument(2))};
}

bool AllOnesWord(const IrValue* word) {
    if (IsU32(word, 0xffffffffu)) return true;
    return Is(word, IrOpcode::BitwiseOr32) && EitherOrder(word, [](const IrValue* inverted, const IrValue* other) {
        return AllOnesWord(inverted) || (Is(inverted, IrOpcode::BitwiseNot32) && Resolved(inverted->Argument(0)) == other);
    });
}

bool AllLanesBit(const IrValue* bit) {
    const auto words = LaneBitWords(Resolved(bit));
    return words && AllOnesWord((*words)[0]) && AllOnesWord((*words)[1]);
}

class EntryLaneWalk {
public:
    bool ZeroBit(const IrValue* bit) {
        bit = Resolved(bit);
        if (bit == nullptr || !Spend()) return false;
        if (bit->HasImmediate()) return bit->Type() == IrType::Bool && !bit->ImmediateBool();
        if (Is(bit, IrOpcode::LogicalAnd)) return ZeroBit(bit->Argument(0)) || ZeroBit(bit->Argument(1));
        if (Is(bit, IrOpcode::SelectU1)) return ZeroBit(bit->Argument(1)) && ZeroBit(bit->Argument(2));
        if (Is(bit, IrOpcode::Phi)) return Incoming(bit, [this](const IrValue* incoming) { return ZeroBit(incoming); });
        const auto words = LaneBitWords(bit);
        return words && Word((*words)[0], 0u) && Word((*words)[1], 1u);
    }

private:
    bool Spend() {
        if (budget == 0u) return false;
        --budget;
        return true;
    }

    bool NotHelper(const IrValue* predicate) {
        predicate = Resolved(predicate);
        if (predicate == nullptr || predicate->HasImmediate() || !Spend()) return false;
        switch (predicate->Opcode()) {
            case IrOpcode::LogicalAnd: return NotHelper(predicate->Argument(0)) || NotHelper(predicate->Argument(1));
            case IrOpcode::IEqual32: return EitherOrder(predicate, [](const IrValue* builtin, const IrValue* zero) { return IsU32(zero, 0u) && Is(builtin, IrOpcode::GetBuiltin) && IsU32(builtin->Argument(0), static_cast<std::uint32_t>(StageInputKind::HelperInvocation)) && IsU32(builtin->Argument(1), 0u); });
            default: return ZeroBit(predicate);
        }
    }

    bool Word(const IrValue* word, std::uint32_t half) {
        word = Resolved(word);
        if (word == nullptr || !Spend()) return false;
        if (word->HasImmediate()) return IsU32(word, 0u);
        switch (word->Opcode()) {
            case IrOpcode::CompositeExtractU32x4: {
                const auto* ballot = Resolved(word->Argument(0));
                return IsU32(word->Argument(1), half) && Is(ballot, IrOpcode::Ballot) && NotHelper(ballot->Argument(0));
            }
            case IrOpcode::CompositeExtractU64: return IsU32(word->Argument(1), half) && Word64(word->Argument(0), half);
            case IrOpcode::BitwiseAnd32: return Word(word->Argument(0), half) || Word(word->Argument(1), half);
            case IrOpcode::BitwiseOr32: return Word(word->Argument(0), half) && Word(word->Argument(1), half);
            case IrOpcode::Phi: return Incoming(word, [this, half](const IrValue* incoming) { return Word(incoming, half); });
            default: return false;
        }
    }

    bool Word64(const IrValue* mask, std::uint32_t half) {
        mask = Resolved(mask);
        if (mask == nullptr || !Spend()) return false;
        if (mask->HasImmediate()) return mask->Type() == IrType::U64 && static_cast<std::uint32_t>(mask->ImmediateU64() >> (32u * half)) == 0u;
        switch (mask->Opcode()) {
            case IrOpcode::CompositeConstructU64: return Word(mask->Argument(half), half);
            case IrOpcode::Phi: return Incoming(mask, [this, half](const IrValue* incoming) { return Word64(incoming, half); });
            default: return false;
        }
    }

    template<typename TCheck>
    bool Incoming(const IrValue* phi, TCheck&& check) {
        if (!phis.insert(phi).second) return true;
        bool result = phi->ArgumentCount() != 0u;
        for (std::size_t index = 0; result && index < phi->ArgumentCount(); ++index) result = check(phi->Argument(index));
        phis.erase(phi);
        return result;
    }

    std::unordered_set<const IrValue*> phis;
    std::uint32_t budget = 512u;
};

bool RowShiftFlags(const IrValue* value, std::uint32_t control) {
    const auto flags = value->Flags<DppMoveFlags>();
    return flags.control == control && flags.rowMask == 0xfu && flags.bankMask == 0xfu && !flags.boundControl && !flags.fetchInactive;
}

const IrValue* RowScanStepInput(const IrValue* step, const WaveReduction& reduction, std::uint32_t control) {
    if (!Is(step, IrOpcode::DppUpdateU32) || !RowShiftFlags(step, control) || !AllLanesBit(step->Argument(2))) return nullptr;
    const auto* previous = Resolved(step->Argument(1));
    const auto* combined = Resolved(step->Argument(0));
    const bool matched = Is(combined, reduction.opcode) && EitherOrder(combined, [&](const IrValue* moved, const IrValue* own) {
        return own == previous && Is(moved, IrOpcode::DppMoveU32) && RowShiftFlags(moved, control) && Resolved(moved->Argument(0)) == previous && AllLanesBit(moved->Argument(1));
    });
    return matched ? previous : nullptr;
}

const IrValue* Unmasked(const IrValue* value) {
    while (Is(value, IrOpcode::SelectU32) && AllLanesBit(value->Argument(0))) value = Resolved(value->Argument(1));
    return value;
}

bool CrossesRowsOf(const IrValue* value, const IrValue* scan) {
    const auto* permlane = Unmasked(value);
    if (!Is(permlane, IrOpcode::Permlane16U32)) return false;
    const auto flags = permlane->Flags<PermlaneFlags>();
    return flags.x16 && !flags.fetchInactive && Resolved(permlane->Argument(0)) == scan && IsU32(permlane->Argument(1), 0xffffffffu) && IsU32(permlane->Argument(2), 0xffffffffu) && AllLanesBit(permlane->Argument(3));
}

std::optional<HalfWaveScan> MatchHalfWaveScan(const IrValue* value) {
    const auto* combined = Unmasked(Resolved(value));
    const auto reduction = std::find_if(WaveReductions.begin(), WaveReductions.end(), [&](const WaveReduction& candidate) { return Is(combined, candidate.opcode); });
    const IrValue* scan = nullptr;
    if (reduction == WaveReductions.end() || !EitherOrder(combined, [&](const IrValue* own, const IrValue* crossed) { scan = own; return CrossesRowsOf(crossed, own); })) return std::nullopt;
    for (const auto control : {0x118u, 0x114u, 0x112u, 0x111u}) {
        scan = RowScanStepInput(scan, *reduction, control);
        if (scan == nullptr) return std::nullopt;
    }
    const auto* source = Unmasked(scan);
    if (!Is(source, IrOpcode::SelectU32) || !IsU32(source->Argument(2), reduction->identity) || !EntryLaneWalk{}.ZeroBit(source->Argument(0))) return std::nullopt;
    return HalfWaveScan{&*reduction, source};
}

std::optional<std::uint32_t> EmitHalfWaveReduction(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t lane) {
    auto& state = ctx.state;
    if (state.program.WaveSize() != 64u || (lane & 31u) != 31u) return std::nullopt;
    const auto scan = MatchHalfWaveScan(inst.Argument(0));
    if (!scan) return std::nullopt;
    const auto read = "v_readlane_b32 of lane " + std::to_string(lane) + " of a wave64 half-wave reduction scan";
    const auto& capabilities = state.supportedCapabilities;
    if (std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityGroupNonUniformArithmetic)) == capabilities.end()) ctx.Fail(inst, (read + " needs subgroup arithmetic, which the device lacks").c_str());
    if (state.hostSubgroupSize > 64u) ctx.Fail(inst, (read + " on a " + std::to_string(state.hostSubgroupSize) + "-lane host subgroup, which is wider than the wave").c_str());
    const auto identity = ConstantU32(state, scan->reduction->identity);
    if (lane >= 32u && state.hostSubgroupSize <= 32u) return identity;
    auto keys = ctx.Def(scan->source);
    if (state.hostSubgroupSize > 32u) {
        const auto upper = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), EmitSubgroupLocalInvocationId(state), ConstantU32(state, 32u));
        keys = lane >= 32u ? Select(state, TypeU32(state), upper, keys, identity) : Select(state, TypeU32(state), upper, identity, keys);
    }
    state.module.EmitCapability(spv::CapabilityGroupNonUniformArithmetic);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(scan->reduction->reduce, TypeU32(state), result, ConstantU32(state, spv::ScopeSubgroup), static_cast<std::uint32_t>(spv::GroupOperationReduce), keys);
    return result;
}

}

std::uint32_t EmitConvertU16U32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitwiseAnd, IrType::U16>(state, arg0, ConstantU32(state, 0xffffu));
}

std::uint32_t EmitConvertU8U32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitwiseAnd, IrType::U8>(state, arg0, ConstantU32(state, 0xffu));
}

std::uint32_t EmitConvertF16F32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 2), pair, arg0, ConstantF32(state, 0u));
    return EmitPackHalf2x16(state, pair);
}

std::uint32_t EmitConvertS32F32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitF32ToU32(state, arg0, true);
}

std::uint32_t EmitConvertU32F32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitF32ToU32(state, arg0, false);
}

std::uint32_t EmitConvertF32S32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto signedValue = Unary(state, spv::OpBitcast, TypeI32(state), arg0);
    return EmitNative<spv::OpConvertSToF, IrType::F32>(state, signedValue);
}

std::uint32_t EmitCompositeExtractU64(SpirvEmitterState& state, std::uint32_t arg0, const IrValue* arg1) {
    return EmitNative<spv::OpCompositeExtract, IrType::U32>(state, arg0, arg1->ImmediateU32());
}

std::uint32_t EmitPackFloat2x16Rtz(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto low = EmitF32ToF16RtzBits(state, arg0);
    const auto high = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), EmitF32ToF16RtzBits(state, arg1), ConstantU32(state, 16u));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), low, high);
}

std::uint32_t EmitFPSaturate32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto bits = Unary(state, spv::OpBitcast, TypeU32(state), arg0);
    const auto positive = Binary(state, spv::OpULessThan, TypeBool(state), Binary(state, spv::OpISub, TypeU32(state), bits, ConstantU32(state, 1u)), ConstantU32(state, 0x7f800000u));
    const auto upper = EmitExt(state, TypeU32(state), GLSLstd450UMin, {bits, ConstantU32(state, 0x3f800000u)});
    return Unary(state, spv::OpBitcast, TypeF32(state), Select(state, TypeU32(state), positive, upper, ConstantU32(state, 0u)));
}

std::uint32_t EmitIAdd64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto lhs = ExtractPair(state, arg0);
    const auto rhs = ExtractPair(state, arg1);
    const auto lowPair = state.module.AllocateId();
    const auto low = state.module.AllocateId();
    const auto carry = state.module.AllocateId();
    state.module.AddFunction(spv::OpIAddCarry, TypeU32Pair(state), lowPair, lhs.low, rhs.low);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, lowPair, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), carry, lowPair, 1u);
    const auto high0 = Binary(state, spv::OpIAdd, TypeU32(state), lhs.high, rhs.high);
    return MakePair(state, low, Binary(state, spv::OpIAdd, TypeU32(state), high0, carry));
}

std::uint32_t EmitISub64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto lhs = ExtractPair(state, arg0);
    const auto rhs = ExtractPair(state, arg1);
    const auto low = Binary(state, spv::OpISub, TypeU32(state), lhs.low, rhs.low);
    const auto borrow = Binary(state, spv::OpULessThan, TypeBool(state), lhs.low, rhs.low);
    const auto borrowValue = Select(state, TypeU32(state), borrow, ConstantU32(state, 1u), ConstantU32(state, 0u));
    const auto high0 = Binary(state, spv::OpISub, TypeU32(state), lhs.high, rhs.high);
    return MakePair(state, low, Binary(state, spv::OpISub, TypeU32(state), high0, borrowValue));
}

std::uint32_t EmitIMul64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto lhs = ExtractPair(state, arg0);
    const auto rhs = ExtractPair(state, arg1);
    const auto low = Binary(state, spv::OpIMul, TypeU32(state), lhs.low, rhs.low);
    const auto high0 = EmitMulHigh(state, lhs.low, rhs.low, false);
    const auto high1 = Binary(state, spv::OpIMul, TypeU32(state), lhs.low, rhs.high);
    const auto high2 = Binary(state, spv::OpIMul, TypeU32(state), lhs.high, rhs.low);
    const auto high = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpIAdd, TypeU32(state), high0, high1), high2);
    return MakePair(state, low, high);
}

std::uint32_t EmitSMulHi(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMulHigh(state, arg0, arg1, true);
}

std::uint32_t EmitUMulHi(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMulHigh(state, arg0, arg1, false);
}

std::uint32_t EmitIAbs32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto negated = Unary(state, spv::OpSNegate, TypeU32(state), arg0);
    const auto negative = Binary(state, spv::OpSLessThan, TypeBool(state), arg0, ConstantU32(state, 0u));
    return Select(state, TypeU32(state), negative, negated, arg0);
}

std::uint32_t EmitShiftLeftLogical64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftLeftLogical, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftLeftLogical, arg0, ctx.Def(arg1));
}

std::uint32_t EmitShiftRightLogical64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftRightLogical, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftRightLogical, arg0, ctx.Def(arg1));
}

std::uint32_t EmitShiftRightArithmetic64(SpirvValueEmitContext& ctx, std::uint32_t arg0, const IrValue* arg1) {
    const IrValue* resolved = arg1->Resolve();
    return resolved->HasImmediate() ? EmitConstantShift64(ctx.state, spv::OpShiftRightArithmetic, arg0, resolved->ImmediateU32()) : EmitShift64(ctx.state, spv::OpShiftRightArithmetic, arg0, ctx.Def(arg1));
}

std::uint32_t EmitBitwiseAnd64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return Binary(state, spv::OpBitwiseAnd, TypeU64(state), arg0, arg1);
}

std::uint32_t EmitBitCount64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, Unary(state, spv::OpBitCount, TypeU64(state), arg0));
    return Binary(state, spv::OpIAdd, TypeU32(state), pair.low, pair.high);
}

std::uint32_t EmitFindILsb32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = EmitExt(state, TypeI32(state), GLSLstd450FindILsb, {arg0});
    return Unary(state, spv::OpBitcast, TypeU32(state), value);
}

std::uint32_t EmitFindUMsb32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto value = EmitExt(state, TypeI32(state), GLSLstd450FindUMsb, {arg0});
    return Unary(state, spv::OpBitcast, TypeU32(state), value);
}

std::uint32_t EmitFindUMsb64(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto pair = ExtractPair(state, arg0);
    const auto high = Unary(state, spv::OpBitcast, TypeU32(state), EmitExt(state, TypeI32(state), GLSLstd450FindUMsb, {pair.high}));
    const auto low = Unary(state, spv::OpBitcast, TypeU32(state), EmitExt(state, TypeI32(state), GLSLstd450FindUMsb, {pair.low}));
    const auto highNonzero = Binary(state, spv::OpINotEqual, TypeBool(state), pair.high, ConstantU32(state, 0u));
    return Select(state, TypeU32(state), highNonzero, Binary(state, spv::OpIAdd, TypeU32(state), high, ConstantU32(state, 32u)), low);
}

std::uint32_t EmitSMin32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxI32Value(state, arg0, arg1, false);
}

std::uint32_t EmitSMax32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxI32Value(state, arg0, arg1, true);
}

std::uint32_t EmitUMin32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxU32Value(state, arg0, arg1, false);
}

std::uint32_t EmitUMax32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxU32Value(state, arg0, arg1, true);
}

std::uint32_t EmitSMinTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMinMax3(state, arg0, arg1, arg2, true, false);
}

std::uint32_t EmitSMaxTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMinMax3(state, arg0, arg1, arg2, true, true);
}

std::uint32_t EmitUMinTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMinMax3(state, arg0, arg1, arg2, false, false);
}

std::uint32_t EmitUMaxTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMinMax3(state, arg0, arg1, arg2, false, true);
}

std::uint32_t EmitSMedTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMed3(state, arg0, arg1, arg2, true);
}

std::uint32_t EmitUMedTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitMed3(state, arg0, arg1, arg2, false);
}

std::uint32_t EmitIEqual64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return CompareEqual64(state, arg0, arg1, false);
}

std::uint32_t EmitINotEqual64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return CompareEqual64(state, arg0, arg1, true);
}

std::uint32_t EmitULessThan64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return CompareOrdered64(state, arg0, arg1, spv::OpULessThan, spv::OpULessThan);
}

std::uint32_t EmitSLessThan64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return CompareOrdered64(state, arg0, arg1, spv::OpSLessThan, spv::OpULessThan);
}

std::uint32_t EmitUGreaterThan64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return CompareOrdered64(state, arg0, arg1, spv::OpUGreaterThan, spv::OpUGreaterThan);
}

std::uint32_t EmitFPIsNan32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpFUnordNotEqual, IrType::U1>(state, arg0, arg0);
}

std::uint32_t EmitFPMin32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxF32Value(state, arg0, arg1, false);
}

std::uint32_t EmitFPMax32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitMinMaxF32Value(state, arg0, arg1, true);
}

std::uint32_t EmitFPMinTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitFMinMax3(state, arg0, arg1, arg2, false);
}

std::uint32_t EmitFPMaxTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitFMinMax3(state, arg0, arg1, arg2, true);
}

std::uint32_t EmitFPMedTri32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    const auto minAb = EmitMinMaxF32Value(state, arg0, arg1, false);
    const auto min3 = EmitMinMaxF32Value(state, minAb, arg2, false);
    const auto maxAb = EmitMinMaxF32Value(state, arg0, arg1, true);
    const auto highMin = EmitMinMaxF32Value(state, maxAb, arg2, false);
    const auto median = EmitMinMaxF32Value(state, minAb, highMin, true);
    const auto nanAb = Binary(state, spv::OpLogicalOr, TypeBool(state), EmitClassifyF32(state, arg0).nan, EmitClassifyF32(state, arg1).nan);
    const auto anyNan = Binary(state, spv::OpLogicalOr, TypeBool(state), nanAb, EmitClassifyF32(state, arg2).nan);
    return Select(state, TypeF32(state), anyNan, min3, median);
}

std::uint32_t EmitFPRecip32(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto source = EmitFlushF32DenormToSignedZero(state, arg0);
    return Binary(state, spv::OpFDiv, TypeF32(state), ConstantF32(state, 0x3f800000u), source);
}

std::uint32_t EmitFPRecipIFlag32(SpirvEmitterState& state, std::uint32_t arg0) {
    return Binary(state, spv::OpFDiv, TypeF32(state), ConstantF32(state, 0x3f800000u), arg0);
}

std::uint32_t EmitFPRecipSqrt32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitExt(state, TypeF32(state), GLSLstd450InverseSqrt, {EmitFlushF32DenormToSignedZero(state, arg0)});
}

std::uint32_t EmitFPSqrt(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitExt(state, TypeF32(state), GLSLstd450Sqrt, {EmitFlushF32DenormToSignedZero(state, arg0)});
}

std::uint32_t EmitFPExp2(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitExt(state, TypeF32(state), GLSLstd450Exp2, {EmitFlushF32DenormToSignedZero(state, arg0)});
}

std::uint32_t EmitFPLog2(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitExt(state, TypeF32(state), GLSLstd450Log2, {EmitFlushF32DenormToSignedZero(state, arg0)});
}

std::uint32_t EmitFPSin(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cycle = EmitTrigCycleF32(state, arg0, true);
    const auto source = Binary(state, spv::OpFMul, TypeF32(state), cycle, ConstantF32(state, 0x40c90fdbu));
    return EmitExt(state, TypeF32(state), GLSLstd450Sin, {source});
}

std::uint32_t EmitFPCos(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto cycle = EmitTrigCycleF32(state, arg0, false);
    const auto source = Binary(state, spv::OpFMul, TypeF32(state), cycle, ConstantF32(state, 0x40c90fdbu));
    return EmitExt(state, TypeF32(state), GLSLstd450Cos, {source});
}

std::uint32_t EmitIdentity(SpirvValueEmitContext&, std::uint32_t value) {
    return value;
}

std::uint32_t EmitUndefU1(SpirvEmitterState& state, const IrValue& inst) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpUndef, TypeId(state, inst.Type()), result);
    return result;
}

std::uint32_t EmitDppMoveU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto flags = inst.Flags<DppMoveFlags>();
    const auto target = EmitDppTargetLane(state, flags.control);
    const auto shuffled = ctx.Shuffle(inst, 0, target.lane);
    if (flags.fetchInactive) {
        return EmitNative<spv::OpSelect, IrType::U32>(state, target.valid, shuffled, ConstantU32(state, 0u));
    }
    const auto ballot = ctx.Ballot(inst.Argument(1));
    const auto sourceActive = EmitBallotLaneActiveBool(state, ballot, target.lane);
    const auto canFetch = state.module.AllocateId();
    state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), canFetch, target.valid, sourceActive);
    return EmitNative<spv::OpSelect, IrType::U32>(state, canFetch, shuffled, ConstantU32(state, 0u));
}

std::uint32_t EmitDppUpdateU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto write = EmitDppWriteCondition(ctx, inst);
    return EmitNative<spv::OpSelect, IrType::U32>(ctx.state, write, ctx.Arg(inst, 0), ctx.Arg(inst, 1));
}

std::uint32_t EmitWqmU64(SpirvEmitterState& state, std::uint32_t value) {
    const auto shiftedOne = state.module.AllocateId();
    const auto mergedOne = state.module.AllocateId();
    const auto shiftedTwo = state.module.AllocateId();
    const auto mergedTwo = state.module.AllocateId();
    const auto quadBits = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU64(state), shiftedOne, value, ConstantU64(state, 0x0000000100000001ull));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU64(state), mergedOne, value, shiftedOne);
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU64(state), shiftedTwo, mergedOne, ConstantU64(state, 0x0000000200000002ull));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU64(state), mergedTwo, mergedOne, shiftedTwo);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU64(state), quadBits, mergedTwo, ConstantU64(state, 0x1111111111111111ull));
    state.module.AddFunction(spv::OpIMul, TypeU64(state), result, quadBits, ConstantU64(state, 0x0000000f0000000full));
    return result;
}

std::uint32_t EmitLaneId(SpirvEmitterState& state) {
    return state.program.Resources().stage == IrShaderStage::TessellationControl ? EmitInputComponentU32(state, StageInputKind::InvocationId, 0) : EmitSubgroupLocalInvocationId(state);
}

std::uint32_t EmitBallot(SpirvValueEmitContext& ctx, const IrValue* predicate) {
    return ctx.Ballot(predicate);
}

std::uint32_t EmitReadFirstLane(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto ballot = ctx.Ballot(inst.Argument(1));
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1u);
    const auto any = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseOr, TypeU32(state), low, high), ConstantU32(state, 0u));
    return ctx.Shuffle(inst, 0, Select(state, TypeU32(state), any, ctx.FirstLane(ballot), ConstantU32(state, 0u)));
}

std::uint32_t EmitReadLane(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const IrValue* selector = inst.Argument(1)->Resolve();
    if (selector != nullptr && selector->HasImmediate() && state.program.Resources().stage != IrShaderStage::Compute && state.laneCount == 1u) {
        const auto index = selector->ImmediateU32() & (state.program.WaveSize() - 1u);
        if (const auto reduced = EmitHalfWaveReduction(ctx, inst, index)) return *reduced;
        if (HostSubgroupNarrowerThanWave(state) && index >= state.hostSubgroupSize) FailOutsideHostSubgroup(ctx, inst, "v_readlane_b32 of lane " + std::to_string(index));
    }
    const auto lane = Binary(state, spv::OpBitwiseAnd, TypeU32(state), ctx.Arg(inst, 1), ConstantU32(state, state.program.WaveSize() - 1u));
    return ctx.Shuffle(inst, 0, lane);
}

std::uint32_t EmitWriteLane(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto lane = Binary(state, spv::OpBitwiseAnd, TypeU32(state), ctx.Arg(inst, 1), ConstantU32(state, state.program.WaveSize() - 1u));
    const auto hit = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), hit, EmitSubgroupLocalInvocationId(state), lane);
    return EmitNative<spv::OpSelect, IrType::U32>(state, hit, ctx.Arg(inst, 0), ctx.Arg(inst, 2));
}

std::uint32_t EmitPermlane16U32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto flags = inst.Flags<PermlaneFlags>();
    if (flags.x16 && state.hostSubgroupSize <= 16u && HostSubgroupNarrowerThanWave(state)) FailOutsideHostSubgroup(ctx, inst, "v_permlanex16_b32 from lanes 16-31");
    const auto subid = EmitSubgroupLocalInvocationId(state);
    const auto row = state.module.AllocateId();
    const auto rowValue = state.module.AllocateId();
    const auto lane = state.module.AllocateId();
    const auto lane8 = state.module.AllocateId();
    const auto shift = state.module.AllocateId();
    const auto upper = state.module.AllocateId();
    const auto selected = state.module.AllocateId();
    const auto shifted = state.module.AllocateId();
    const auto index = state.module.AllocateId();
    const auto target = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid, ConstantU32(state, 0xfffffff0u));
    if (flags.x16) {
        state.module.AddFunction(spv::OpBitwiseXor, TypeU32(state), rowValue, row, ConstantU32(state, 16u));
    } else {
        state.module.AddFunction(spv::OpCopyObject, TypeU32(state), rowValue, row);
    }
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid, ConstantU32(state, 15u));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane8, lane, ConstantU32(state, 7u));
    state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), shift, lane8, ConstantU32(state, 2u));
    state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), upper, lane, ConstantU32(state, 8u));
    state.module.AddFunction(spv::OpSelect, TypeU32(state), selected, upper, ctx.Arg(inst, 2), ctx.Arg(inst, 1));
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), shifted, selected, shift);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), index, shifted, ConstantU32(state, 15u));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, rowValue, index);
    const auto shuffled = ctx.Shuffle(inst, 0, target);
    if (flags.fetchInactive) {
        return shuffled;
    }
    const auto sourceActive = EmitBallotLaneActiveBool(state, ctx.Ballot(inst.Argument(3)), target);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, TypeU32(state), result, sourceActive, shuffled, flags.boundControl ? ConstantU32(state, 0u) : ctx.Arg(inst, 4));
    return result;
}

std::uint32_t EmitBpermuteU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto source = ctx.Arg(inst, 0);
    const auto shifted = Binary(state, spv::OpShiftRightLogical, TypeU32(state), ctx.Arg(inst, 1), ConstantU32(state, 2u));
    const auto index = Binary(state, spv::OpBitwiseAnd, TypeU32(state), shifted, ConstantU32(state, 31u));
    const auto base = Binary(state, spv::OpBitwiseAnd, TypeU32(state), EmitSubgroupLocalInvocationId(state), ConstantU32(state, ~31u));
    const auto target = Binary(state, spv::OpBitwiseOr, TypeU32(state), base, index);
    return EmitDsMaskedLaneRead(state, source, target, ctx.Arg(inst, 2));
}

std::uint32_t EmitPermuteU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto lane = EmitSubgroupLocalInvocationId(state);
    const auto base = Binary(state, spv::OpBitwiseAnd, TypeU32(state), lane, ConstantU32(state, ~31u));
    const auto own = Binary(state, spv::OpBitwiseAnd, TypeU32(state), lane, ConstantU32(state, 31u));
    auto result = ConstantU32(state, 0u);
    for (std::uint32_t source = 0; source < 32u; ++source) {
        const auto from = Binary(state, spv::OpBitwiseOr, TypeU32(state), base, ConstantU32(state, source));
        const auto value = ctx.Shuffle(inst, 0, from);
        const auto address = ctx.Shuffle(inst, 1, from);
        const auto active = ctx.Shuffle(inst, 2, from);
        const auto target = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u)), ConstantU32(state, 31u));
        const auto hit = Binary(state, spv::OpLogicalAnd, TypeBool(state), active, Binary(state, spv::OpIEqual, TypeBool(state), target, own));
        result = Select(state, TypeU32(state), hit, value, result);
    }
    return result;
}

std::uint32_t EmitSwizzleU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    state.module.AddFunction(spv::OpStore, ctx.scratchU32Variable, ctx.Arg(inst, 0));
    const auto source = EmitNative<spv::OpLoad, IrType::U32>(state, ctx.scratchU32Variable);
    const IrValue* control = inst.Argument(1);
    const auto target = EmitDsSwizzleTargetLane(state, EmitSubgroupLocalInvocationId(state), control->HasImmediate() ? control->ImmediateU32() : 0u);
    return EmitDsMaskedLaneRead(state, source, target, ctx.Arg(inst, 2));
}

std::uint32_t EmitBitCastU16F16(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitcast, IrType::U32>(state, arg0);
}

std::uint32_t EmitBitCastF32U32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitcast, IrType::F32>(state, arg0);
}

std::uint32_t EmitConvertF32U32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpConvertUToF, IrType::F32>(state, arg0);
}

std::uint32_t EmitCompositeConstructU64(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpCompositeConstruct, IrType::U64>(state, arg0, arg1);
}

std::uint32_t EmitCompositeConstructU32x2(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpCompositeConstruct, IrType::U32x2>(state, arg0, arg1);
}

std::uint32_t EmitCompositeConstructU32x3(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpCompositeConstruct, IrType::U32x3>(state, arg0, arg1, arg2);
}

std::uint32_t EmitCompositeConstructF32x2(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpCompositeConstruct, IrType::F32x2>(state, arg0, arg1);
}

std::uint32_t EmitCompositeConstructU32x4(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3) {
    return EmitNative<spv::OpCompositeConstruct, IrType::U32x4>(state, arg0, arg1, arg2, arg3);
}

std::uint32_t EmitBitFieldInsert(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3) {
    return EmitNative<spv::OpBitFieldInsert, IrType::U32>(state, arg0, arg1, arg2, arg3);
}

std::uint32_t EmitBitFieldUExtract(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpBitFieldUExtract, IrType::U32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitBitFieldSExtract(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpBitFieldSExtract, IrType::U32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitSelectU1(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpSelect, IrType::U1>(state, arg0, arg1, arg2);
}

std::uint32_t EmitSelectU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpSelect, IrType::U32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitSelectF32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpSelect, IrType::F32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitIAdd32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpIAdd, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitISub32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpISub, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitIMul32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpIMul, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitUDiv32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpUDiv, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitIAddCarry32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpIAddCarry, IrType::U32x2>(state, arg0, arg1);
}

std::uint32_t EmitShiftLeftLogical32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpShiftLeftLogical, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitShiftRightLogical32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpShiftRightLogical, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitShiftRightArithmetic32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpShiftRightArithmetic, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitBitwiseAnd32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpBitwiseAnd, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitBitwiseOr32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpBitwiseOr, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitBitwiseXor32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpBitwiseXor, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitBitwiseNot32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpNot, IrType::U32>(state, arg0);
}

std::uint32_t EmitBitReverse32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitReverse, IrType::U32>(state, arg0);
}

std::uint32_t EmitBitCount32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitCount, IrType::U32>(state, arg0);
}

std::uint32_t EmitSLessThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpSLessThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitULessThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpULessThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitIEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpIEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitSLessThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpSLessThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitULessThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpULessThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitSGreaterThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpSGreaterThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitUGreaterThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpUGreaterThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitINotEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpINotEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitSGreaterThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpSGreaterThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitUGreaterThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpUGreaterThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalOr(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpLogicalOr, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalAnd(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpLogicalAnd, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalXor(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpLogicalNotEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalNot(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpLogicalNot, IrType::U1>(state, arg0);
}

std::uint32_t EmitFPOrdEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFOrdEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPUnordEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPOrdNotEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    const auto ordered = [&](std::uint32_t value) {
        const auto magnitude = EmitNative<spv::OpBitwiseAnd, IrType::U32>(state, EmitNative<spv::OpBitcast, IrType::U32>(state, value), ConstantU32(state, 0x7fffffffu));
        return EmitNative<spv::OpULessThanEqual, IrType::U1>(state, magnitude, ConstantU32(state, 0x7f800000u));
    };
    const auto bothOrdered = EmitNative<spv::OpLogicalAnd, IrType::U1>(state, ordered(arg0), ordered(arg1));
    return EmitNative<spv::OpLogicalAnd, IrType::U1>(state, EmitNative<spv::OpFUnordNotEqual, IrType::U1>(state, arg0, arg1), bothOrdered);
}

std::uint32_t EmitFPUnordNotEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordNotEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPOrdLessThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFOrdLessThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPUnordLessThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordLessThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPOrdGreaterThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFOrdGreaterThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPUnordGreaterThan32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordGreaterThan, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPOrdLessThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFOrdLessThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPUnordLessThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordLessThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPOrdGreaterThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFOrdGreaterThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPUnordGreaterThanEqual32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpFUnordGreaterThanEqual, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitFPAdd32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return Exact(state, EmitNative<spv::OpFAdd, IrType::F32>(state, arg0, arg1));
}

std::uint32_t EmitFPSub32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return Exact(state, EmitNative<spv::OpFSub, IrType::F32>(state, arg0, arg1));
}

std::uint32_t EmitFPMul32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return Exact(state, EmitNative<spv::OpFMul, IrType::F32>(state, arg0, arg1));
}

std::uint32_t EmitAddU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpIAdd, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitTBufferSelectF32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpSelect, IrType::F32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitSelectValueU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitNative<spv::OpSelect, IrType::U32>(state, arg0, arg1, arg2);
}

std::uint32_t EmitOrU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpBitwiseOr, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitBitcastF32ToU32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitcast, IrType::U32>(state, arg0);
}

std::uint32_t EmitBitcastU32ToF32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpBitcast, IrType::F32>(state, arg0);
}

std::uint32_t EmitAndU32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpBitwiseAnd, IrType::U32>(state, arg0, arg1);
}

std::uint32_t EmitLogicalAndBool(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpLogicalAnd, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalOrBool(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1) {
    return EmitNative<spv::OpLogicalOr, IrType::U1>(state, arg0, arg1);
}

std::uint32_t EmitLogicalNotBool(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpLogicalNot, IrType::U1>(state, arg0);
}

std::uint32_t EmitTruncF32Value(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450Trunc, IrType::F32>(state, arg0);
}

std::uint32_t EmitFNegateValue(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitNative<spv::OpFNegate, IrType::F32>(state, arg0);
}

std::uint32_t EmitFAbsValue(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450FAbs, IrType::F32>(state, arg0);
}

static bool NativeF16Rte(const SpirvEmitterState& state) {
    const auto supported = [&](spv::Capability capability) {
        return std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) != state.supportedCapabilities.end();
    };
    return supported(spv::CapabilityFloat16) && supported(spv::CapabilityRoundingModeRTE) && supported(spv::CapabilityDenormPreserve);
}

static std::uint32_t EmitNativeF32ToF16BitsRte(SpirvEmitterState& state, std::uint32_t value) {
    state.module.EmitCapability(spv::CapabilityFloat16);
    state.module.EmitCapability(spv::CapabilityRoundingModeRTE);
    state.module.EmitCapability(spv::CapabilityDenormPreserve);
    if (!state.nativeF16ModesEmitted) {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeRoundingModeRTE, 16u);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDenormPreserve, 16u);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSignedZeroInfNanPreserve, 16u);
        state.nativeF16ModesEmitted = true;
    }
    const auto u32 = TypeU32(state);
    const auto f16 = state.module.Type(spv::OpTypeFloat, 16u);
    const auto half = Unary(state, spv::OpFConvert, f16, value);
    const auto pair = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, state.module.Type(spv::OpTypeVector, f16, 2u), pair, half, state.module.Constant(spv::OpConstant, f16, 0u));
    const auto bits = Unary(state, spv::OpBitcast, u32, pair);
    const auto source = Unary(state, spv::OpBitcast, u32, value);
    const auto magnitude = Binary(state, spv::OpBitwiseAnd, u32, source, ConstantU32(state, 0x7fffffffu));
    const auto sign = Binary(state, spv::OpBitwiseAnd, u32, Binary(state, spv::OpShiftRightLogical, u32, source, ConstantU32(state, 16u)), ConstantU32(state, 0x8000u));
    const auto payload = Binary(state, spv::OpShiftRightLogical, u32, Binary(state, spv::OpBitwiseAnd, u32, magnitude, ConstantU32(state, 0x7fffffu)), ConstantU32(state, 13u));
    const auto nan = Binary(state, spv::OpBitwiseOr, u32, sign, Binary(state, spv::OpBitwiseOr, u32, ConstantU32(state, 0x7e00u), payload));
    return Select(state, u32, Binary(state, spv::OpUGreaterThan, TypeBool(state), magnitude, ConstantU32(state, 0x7f800000u)), nan, bits);
}

std::uint32_t EmitF32ToF16BitsRte(SpirvEmitterState& state, std::uint32_t value) {
    if (NativeF16Rte(state)) return EmitNativeF32ToF16BitsRte(state, value);
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t bits) { return ConstantU32(state, bits); };
    const auto op = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, opcode, u32, lhs, rhs); };
    const auto test = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, opcode, boolean, lhs, rhs); };
    const auto pick = [&](std::uint32_t condition, std::uint32_t yes, std::uint32_t no) { return Select(state, u32, condition, yes, no); };
    const auto roundEven = [&](std::uint32_t mantissa, std::uint32_t shift) {
        const auto truncated = op(spv::OpShiftRightLogical, mantissa, shift);
        const auto remainder = op(spv::OpBitwiseAnd, mantissa, op(spv::OpISub, op(spv::OpShiftLeftLogical, constant(1u), shift), constant(1u)));
        const auto half = op(spv::OpShiftLeftLogical, constant(1u), op(spv::OpISub, shift, constant(1u)));
        const auto above = test(spv::OpUGreaterThan, remainder, half);
        const auto tie = Binary(state, spv::OpLogicalAnd, boolean, test(spv::OpIEqual, remainder, half), test(spv::OpINotEqual, op(spv::OpBitwiseAnd, truncated, constant(1u)), constant(0u)));
        return op(spv::OpIAdd, truncated, pick(Binary(state, spv::OpLogicalOr, boolean, above, tie), constant(1u), constant(0u)));
    };
    const auto bits = Unary(state, spv::OpBitcast, u32, value);
    const auto sign = op(spv::OpBitwiseAnd, op(spv::OpShiftRightLogical, bits, constant(16u)), constant(0x8000u));
    const auto magnitude = op(spv::OpBitwiseAnd, bits, constant(0x7fffffffu));
    const auto exponent = op(spv::OpShiftRightLogical, magnitude, constant(23u));
    const auto normal = op(spv::OpISub, roundEven(magnitude, constant(13u)), constant(112u << 10u));
    const auto mantissa = op(spv::OpBitwiseOr, op(spv::OpBitwiseAnd, magnitude, constant(0x7fffffu)), constant(0x800000u));
    const auto shift = Select(state, u32, test(spv::OpULessThan, exponent, constant(102u)), constant(31u), op(spv::OpISub, constant(126u), exponent));
    const auto subnormal = pick(test(spv::OpULessThan, exponent, constant(102u)), constant(0u), roundEven(mantissa, shift));
    auto result = pick(test(spv::OpULessThan, magnitude, constant(0x38800000u)), subnormal, normal);
    result = pick(test(spv::OpUGreaterThanEqual, magnitude, constant(0x477ff000u)), constant(0x7c00u), result);
    const auto nan = op(spv::OpBitwiseOr, constant(0x7e00u), op(spv::OpShiftRightLogical, op(spv::OpBitwiseAnd, magnitude, constant(0x7fffffu)), constant(13u)));
    result = pick(test(spv::OpUGreaterThan, magnitude, constant(0x7f800000u)), nan, result);
    return op(spv::OpBitwiseOr, sign, result);
}

std::uint32_t EmitPackHalf2x16(SpirvEmitterState& state, std::uint32_t arg0) {
    const auto component = [&](std::uint32_t index) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), value, arg0, index);
        return EmitF32ToF16BitsRte(state, value);
    };
    const auto high = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), component(1u), ConstantU32(state, 16u));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), component(0u), high);
}

std::uint32_t EmitFPFma32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return Exact(state, EmitGlsl<GLSLstd450Fma, IrType::F32>(state, arg0, arg1, arg2));
}

std::uint32_t EmitFPMad32(SpirvEmitterState& state, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {
    return EmitFPAdd32(state, EmitFPMul32(state, arg0, arg1), arg2);
}

std::uint32_t EmitFPRoundEven32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450RoundEven, IrType::F32>(state, arg0);
}

std::uint32_t EmitFPFloor32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450Floor, IrType::F32>(state, arg0);
}

std::uint32_t EmitFPCeil32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450Ceil, IrType::F32>(state, arg0);
}

std::uint32_t EmitFPTrunc32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450Trunc, IrType::F32>(state, arg0);
}

std::uint32_t EmitFPFract32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitGlsl<GLSLstd450Fract, IrType::F32>(state, arg0);
}

std::uint32_t EmitBitCastF16U16(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitBitCastU16F16(state, arg0);
}

std::uint32_t EmitConvertU32U16(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitBitCastU16F16(state, arg0);
}

std::uint32_t EmitConvertU32U8(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitBitCastU16F16(state, arg0);
}

std::uint32_t EmitBitCastU32F32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitBitCastU16F16(state, arg0);
}

std::uint32_t EmitConvertF32F16(SpirvEmitterState& state, std::uint32_t bits) {
    return EmitF16BitsToF32(state, bits);
}

std::uint32_t EmitCompositeExtractU32x2(SpirvEmitterState& state, std::uint32_t arg0, const IrValue* arg1) {
    return EmitCompositeExtractU64(state, arg0, arg1);
}

std::uint32_t EmitCompositeExtractU32x3(SpirvEmitterState& state, std::uint32_t arg0, const IrValue* arg1) {
    return EmitCompositeExtractU64(state, arg0, arg1);
}

std::uint32_t EmitCompositeExtractU32x4(SpirvEmitterState& state, std::uint32_t arg0, const IrValue* arg1) {
    return EmitCompositeExtractU64(state, arg0, arg1);
}

std::uint32_t EmitFPAbs32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitFAbsValue(state, arg0);
}

std::uint32_t EmitFPNeg32(SpirvEmitterState& state, std::uint32_t arg0) {
    return EmitFNegateValue(state, arg0);
}

std::uint32_t EmitFPCmpClass32(SpirvEmitterState& state, std::uint32_t value, std::uint32_t mask) {
    return EmitClassMaskF32(state, value, mask);
}

std::uint32_t EmitUndefU8(SpirvEmitterState& state, const IrValue& inst) {
    return EmitUndefU1(state, inst);
}

std::uint32_t EmitUndefU16(SpirvEmitterState& state, const IrValue& inst) {
    return EmitUndefU1(state, inst);
}

std::uint32_t EmitUndefU32(SpirvEmitterState& state, const IrValue& inst) {
    return EmitUndefU1(state, inst);
}

std::uint32_t EmitUndefU64(SpirvEmitterState& state, const IrValue& inst) {
    return EmitUndefU1(state, inst);
}

}
