#include "Optimization/SingleLaneAnalysis.hpp"
#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::size_t PairLimit = std::size_t{1} << 18u;
constexpr std::uint32_t DepthLimit = 64;
constexpr std::uint64_t LaneOffsetLimit = std::uint64_t{1} << 16u;
constexpr std::size_t FootprintLimit = 64;

struct Rejection {
    const char* reason;
    const IrValue* at;
};

bool isImmediateU32(const IrValue* value, std::uint32_t immediate) {
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == immediate;
}

bool isLane(const IrValue* value) {
    return value->Resolve()->Opcode() == IrOpcode::LaneId;
}

bool isHalfSelector(const IrValue* value) {
    value = value->Resolve();
    return value->Opcode() == IrOpcode::ULessThan32 && isLane(value->Argument(0)) && isImmediateU32(value->Argument(1)->Resolve(), 32u);
}

bool isBitSelector(const IrValue* value) {
    value = value->Resolve();
    if (value->Opcode() != IrOpcode::BitwiseAnd32) return false;
    const IrValue* left = value->Argument(0)->Resolve();
    const IrValue* right = value->Argument(1)->Resolve();
    return (isLane(left) && isImmediateU32(right, 31u)) || (isLane(right) && isImmediateU32(left, 31u));
}

bool isHalfWord(const IrValue* value) {
    value = value->Resolve();
    return value->Opcode() == IrOpcode::SelectU32 && isHalfSelector(value->Argument(0));
}

const IrValue* otherThan(const IrValue* value, std::uint32_t immediate) {
    if (value->ArgumentCount() != 2u) return nullptr;
    const IrValue* left = value->Argument(0)->Resolve();
    const IrValue* right = value->Argument(1)->Resolve();
    if (isImmediateU32(right, immediate)) return left;
    if (isImmediateU32(left, immediate)) return right;
    return nullptr;
}

const IrValue* ownBitWord(const IrValue* value) {
    value = value->Resolve();
    if (value->Opcode() != IrOpcode::INotEqual32) return nullptr;
    const IrValue* bit = otherThan(value, 0u);
    if (bit == nullptr || bit->Opcode() != IrOpcode::BitwiseAnd32) return nullptr;
    const IrValue* shifted = otherThan(bit, 1u);
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32 || !isBitSelector(shifted->Argument(1))) return nullptr;
    return shifted->Argument(0)->Resolve();
}

bool isBallotWord(const IrValue* value, std::uint32_t half) {
    return value->Opcode() == IrOpcode::CompositeExtractU32x4 && value->Argument(0)->Resolve()->Opcode() == IrOpcode::Ballot && isImmediateU32(value->Argument(1)->Resolve(), half);
}

const IrValue* ballotOfHalves(const IrValue* low, const IrValue* high) {
    if (!isBallotWord(low, 0u) || !isBallotWord(high, 1u)) return nullptr;
    const IrValue* lowBallot = low->Argument(0)->Resolve();
    const IrValue* highBallot = high->Argument(0)->Resolve();
    if (lowBallot == highBallot) return lowBallot;
    if (lowBallot->Parent() != highBallot->Parent()) return nullptr;
    const IrValue* lowPredicate = lowBallot->Argument(0)->Resolve();
    const IrValue* highPredicate = highBallot->Argument(0)->Resolve();
    if (lowPredicate == highPredicate) return lowBallot;
    const bool sameImmediate = lowPredicate->HasImmediate() && highPredicate->HasImmediate() && lowPredicate->Type() == IrType::Bool && highPredicate->Type() == IrType::Bool && lowPredicate->ImmediateBool() == highPredicate->ImmediateBool();
    return sameImmediate ? lowBallot : nullptr;
}

bool isLaneMaskOp(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::BitwiseAnd32:
    case IrOpcode::BitwiseOr32:
    case IrOpcode::BitwiseXor32:
    case IrOpcode::BitwiseNot32:
    case IrOpcode::SelectU32:
    case IrOpcode::Phi:
        return true;
    default:
        return false;
    }
}

bool isLaneLocal(IrOpcode opcode) {
    if (BufferAccessOf(opcode) != BufferAccess::None || AddressOpcodeInfoOf(opcode).access != AddressAccess::None || ImageOpcodeInfoOf(opcode).access != ImageAccess::None) return true;
    switch (opcode) {
    case IrOpcode::Identity:
    case IrOpcode::Phi:
    case IrOpcode::Void:
    case IrOpcode::IAdd32:
    case IrOpcode::ISub32:
    case IrOpcode::IMul32:
    case IrOpcode::IDiv32:
    case IrOpcode::IMod32:
    case IrOpcode::IAnd32:
    case IrOpcode::IOr32:
    case IrOpcode::IXor32:
    case IrOpcode::IShiftLeft32:
    case IrOpcode::IShiftRightLogical32:
    case IrOpcode::IShiftRightArithmetic32:
    case IrOpcode::INegate32:
    case IrOpcode::INot32:
    case IrOpcode::FAdd32:
    case IrOpcode::FSub32:
    case IrOpcode::FMul32:
    case IrOpcode::FDiv32:
    case IrOpcode::FNegate32:
    case IrOpcode::FAbs32:
    case IrOpcode::FFma32:
    case IrOpcode::FSqrt32:
    case IrOpcode::FRcp32:
    case IrOpcode::FRsq32:
    case IrOpcode::FMin32:
    case IrOpcode::FMax32:
    case IrOpcode::FFloor32:
    case IrOpcode::FCeil32:
    case IrOpcode::FFract32:
    case IrOpcode::ConvertFToI32:
    case IrOpcode::ConvertIToF32:
    case IrOpcode::ConvertFToU32:
    case IrOpcode::ConvertUToF32:
    case IrOpcode::CompareEqual:
    case IrOpcode::CompareNotEqual:
    case IrOpcode::CompareLessThan:
    case IrOpcode::CompareGreaterThan:
    case IrOpcode::Select:
    case IrOpcode::Barrier:
    case IrOpcode::Reference:
    case IrOpcode::ReferenceU32:
    case IrOpcode::GetUserData:
    case IrOpcode::GetShaderBase:
    case IrOpcode::ShaderClock:
    case IrOpcode::RealtimeClock:
    case IrOpcode::UndefU1:
    case IrOpcode::UndefU8:
    case IrOpcode::UndefU16:
    case IrOpcode::UndefU32:
    case IrOpcode::UndefU64:
    case IrOpcode::BitCastU16F16:
    case IrOpcode::BitCastF16U16:
    case IrOpcode::BitCastU32F32:
    case IrOpcode::BitCastF32U32:
    case IrOpcode::ConvertU16U32:
    case IrOpcode::ConvertU32U16:
    case IrOpcode::ConvertU8U32:
    case IrOpcode::ConvertU32U8:
    case IrOpcode::ConvertF32F16:
    case IrOpcode::ConvertF16F32:
    case IrOpcode::ConvertS32F32:
    case IrOpcode::ConvertU32F32:
    case IrOpcode::ConvertF32S32:
    case IrOpcode::ConvertF32U32:
    case IrOpcode::CompositeConstructU64:
    case IrOpcode::CompositeConstructU32x2:
    case IrOpcode::CompositeConstructU32x3:
    case IrOpcode::CompositeConstructF32x2:
    case IrOpcode::CompositeConstructU32x4:
    case IrOpcode::CompositeExtractU64:
    case IrOpcode::CompositeExtractU32x2:
    case IrOpcode::CompositeExtractU32x3:
    case IrOpcode::CompositeExtractU32x4:
    case IrOpcode::PackHalf2x16:
    case IrOpcode::PackFloat2x16Rtz:
    case IrOpcode::FPAbs32:
    case IrOpcode::FPNeg32:
    case IrOpcode::FPSaturate32:
    case IrOpcode::BitFieldInsert:
    case IrOpcode::BitFieldUExtract:
    case IrOpcode::BitFieldSExtract:
    case IrOpcode::SelectU1:
    case IrOpcode::SelectF32:
    case IrOpcode::SelectU32:
    case IrOpcode::IAdd64:
    case IrOpcode::IAddCarry32:
    case IrOpcode::ISub64:
    case IrOpcode::IMul64:
    case IrOpcode::UDiv32:
    case IrOpcode::SMulHi:
    case IrOpcode::UMulHi:
    case IrOpcode::IAbs32:
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
    case IrOpcode::BitReverse32:
    case IrOpcode::BitCount32:
    case IrOpcode::BitCount64:
    case IrOpcode::FindUMsb32:
    case IrOpcode::FindUMsb64:
    case IrOpcode::FindILsb32:
    case IrOpcode::SMin32:
    case IrOpcode::UMin32:
    case IrOpcode::SMax32:
    case IrOpcode::UMax32:
    case IrOpcode::SMinTri32:
    case IrOpcode::UMinTri32:
    case IrOpcode::SMaxTri32:
    case IrOpcode::UMaxTri32:
    case IrOpcode::SMedTri32:
    case IrOpcode::UMedTri32:
    case IrOpcode::SLessThan32:
    case IrOpcode::SLessThan64:
    case IrOpcode::ULessThan32:
    case IrOpcode::ULessThan64:
    case IrOpcode::IEqual32:
    case IrOpcode::IEqual64:
    case IrOpcode::SLessThanEqual32:
    case IrOpcode::ULessThanEqual32:
    case IrOpcode::SGreaterThan32:
    case IrOpcode::UGreaterThan32:
    case IrOpcode::UGreaterThan64:
    case IrOpcode::INotEqual32:
    case IrOpcode::INotEqual64:
    case IrOpcode::SGreaterThanEqual32:
    case IrOpcode::UGreaterThanEqual32:
    case IrOpcode::LogicalOr:
    case IrOpcode::LogicalAnd:
    case IrOpcode::LogicalXor:
    case IrOpcode::LogicalNot:
    case IrOpcode::FPOrdEqual32:
    case IrOpcode::FPUnordEqual32:
    case IrOpcode::FPOrdNotEqual32:
    case IrOpcode::FPUnordNotEqual32:
    case IrOpcode::FPOrdLessThan32:
    case IrOpcode::FPUnordLessThan32:
    case IrOpcode::FPOrdGreaterThan32:
    case IrOpcode::FPUnordGreaterThan32:
    case IrOpcode::FPOrdLessThanEqual32:
    case IrOpcode::FPUnordLessThanEqual32:
    case IrOpcode::FPOrdGreaterThanEqual32:
    case IrOpcode::FPUnordGreaterThanEqual32:
    case IrOpcode::FPIsNan32:
    case IrOpcode::FPCmpClass32:
    case IrOpcode::FPAdd32:
    case IrOpcode::FPSub32:
    case IrOpcode::FPFma32:
    case IrOpcode::FPMad32:
    case IrOpcode::FPMul32:
    case IrOpcode::FPMin32:
    case IrOpcode::FPMax32:
    case IrOpcode::FPMinTri32:
    case IrOpcode::FPMaxTri32:
    case IrOpcode::FPMedTri32:
    case IrOpcode::FPRecip32:
    case IrOpcode::FPRecipIFlag32:
    case IrOpcode::FPRecipSqrt32:
    case IrOpcode::FPSqrt:
    case IrOpcode::FPSin:
    case IrOpcode::FPCos:
    case IrOpcode::FPExp2:
    case IrOpcode::FPLog2:
    case IrOpcode::FPRoundEven32:
    case IrOpcode::FPFloor32:
    case IrOpcode::FPCeil32:
    case IrOpcode::FPTrunc32:
    case IrOpcode::FPFract32:
    case IrOpcode::FPAdd64:
    case IrOpcode::FPMul64:
    case IrOpcode::FPFma64:
    case IrOpcode::FPFmaScale64:
    case IrOpcode::FPMin64:
    case IrOpcode::FPMax64:
    case IrOpcode::FPSaturate64:
    case IrOpcode::FPLdexp64:
    case IrOpcode::FPRoundEven64:
    case IrOpcode::FPFloor64:
    case IrOpcode::FPCeil64:
    case IrOpcode::FPTrunc64:
    case IrOpcode::FPFract64:
    case IrOpcode::FPFrexpMant64:
    case IrOpcode::FPFrexpExp64:
    case IrOpcode::FPRcp64:
    case IrOpcode::FPRsq64:
    case IrOpcode::FPSqrt64:
    case IrOpcode::FPTrigPreop64:
    case IrOpcode::FPDot2F32F16:
    case IrOpcode::ConvertF32F64:
    case IrOpcode::ConvertF64F32:
    case IrOpcode::ConvertF64S32:
    case IrOpcode::ConvertF64U32:
    case IrOpcode::ConvertS32F64:
    case IrOpcode::ConvertU32F64:
    case IrOpcode::GetSrtResource:
    case IrOpcode::GetBufferResource:
    case IrOpcode::GetAddressResource:
    case IrOpcode::GetScratchResource:
    case IrOpcode::GetImageResource:
    case IrOpcode::GetSamplerResource:
    case IrOpcode::MakeImageAddress:
    case IrOpcode::ReadConst:
    case IrOpcode::ReadConstBuffer:
    case IrOpcode::ImageQueryDimensions:
    case IrOpcode::ImageQueryLod:
    case IrOpcode::ImageBvhIntersectRay:
    case IrOpcode::ControlNop:
    case IrOpcode::Waitcnt:
    case IrOpcode::InstPrefetch:
    case IrOpcode::LoadVectorWindow:
    case IrOpcode::StoreVectorWindow:
        return true;
    default:
        return false;
    }
}

std::uint32_t sharedBytes(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::LoadSharedU8:
    case IrOpcode::WriteSharedU8:
        return 1u;
    case IrOpcode::LoadSharedU16:
    case IrOpcode::WriteSharedU16:
        return 2u;
    case IrOpcode::LoadSharedU32:
    case IrOpcode::WriteSharedU32:
    case IrOpcode::SharedAtomicFMin32:
    case IrOpcode::SharedAtomicFMax32:
    case IrOpcode::SharedAtomicSwap32:
    case IrOpcode::SharedAtomicIAdd32:
    case IrOpcode::SharedAtomicISub32:
    case IrOpcode::SharedAtomicInc32:
    case IrOpcode::SharedAtomicDec32:
    case IrOpcode::SharedAtomicSMin32:
    case IrOpcode::SharedAtomicUMin32:
    case IrOpcode::SharedAtomicSMax32:
    case IrOpcode::SharedAtomicUMax32:
    case IrOpcode::SharedAtomicAnd32:
    case IrOpcode::SharedAtomicOr32:
    case IrOpcode::SharedAtomicXor32:
    case IrOpcode::SharedAtomicRsub32:
    case IrOpcode::SharedAtomicFAdd32:
    case IrOpcode::SharedAtomicCmpst32:
    case IrOpcode::SharedAtomicCmpstF32:
    case IrOpcode::SharedAtomicMskor32:
    case IrOpcode::SharedAtomicWrap32:
        return 4u;
    case IrOpcode::LoadSharedU32x2:
    case IrOpcode::WriteSharedU32x2:
        return 8u;
    case IrOpcode::LoadSharedU32x3:
    case IrOpcode::WriteSharedU32x3:
        return 12u;
    case IrOpcode::LoadSharedU32x4:
    case IrOpcode::WriteSharedU32x4:
        return 16u;
    default:
        return 0u;
    }
}

class Uniformity {
public:
    explicit Uniformity(const IrProgram& program) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const IrBlock* block : program.BlockOrder()) {
                for (const IrValue* inst : block->Instructions()) {
                    if (varying.contains(inst) || !varies(program, *inst)) continue;
                    varying.insert(inst);
                    changed = true;
                }
            }
        }
    }

    [[nodiscard]] bool Uniform(const IrValue* value) const {
        value = value->Resolve();
        return value->HasImmediate() || !varying.contains(value);
    }

private:
    bool varies(const IrProgram& program, const IrValue& inst) const {
        switch (inst.Opcode()) {
        case IrOpcode::GetUserData:
        case IrOpcode::GetShaderBase:
        case IrOpcode::UndefU1:
        case IrOpcode::UndefU8:
        case IrOpcode::UndefU16:
        case IrOpcode::UndefU32:
        case IrOpcode::UndefU64:
            return false;
        case IrOpcode::LaneId:
        case IrOpcode::Ballot:
        case IrOpcode::ShaderClock:
        case IrOpcode::RealtimeClock:
        case IrOpcode::LoadVectorWindow:
            return true;
        case IrOpcode::GetBuiltin: {
            const IrValue* kind = inst.Argument(0)->Resolve();
            if (!kind->HasImmediate()) return true;
            const auto input = static_cast<StageInputKind>(kind->ImmediateU32());
            return input != StageInputKind::WorkgroupId && input != StageInputKind::DispatchThreadLimit;
        }
        case IrOpcode::ReadConst:
        case IrOpcode::ReadConstBuffer:
            return arguments(inst);
        default:
            break;
        }
        if (SharedAccessOf(inst.Opcode()) != SharedAccess::None || BufferAccessOf(inst.Opcode()) != BufferAccess::None || ImageOpcodeInfoOf(inst.Opcode()).access != ImageAccess::None) return true;
        if (AddressOpcodeInfoOf(inst.Opcode()).access != AddressAccess::None) {
            const auto index = inst.Flags<MemoryFlags>().index;
            const auto& memory = program.Resources().memoryInfo;
            const bool scalarRead = AddressOpcodeInfoOf(inst.Opcode()).access == AddressAccess::Read && index < memory.size() && memory[index].kind == ResourceKind::ScalarAddress;
            if (!scalarRead) return true;
        }
        return arguments(inst);
    }

    bool arguments(const IrValue& inst) const {
        for (std::size_t index = 0; index < inst.ArgumentCount(); ++index) {
            const IrValue* argument = inst.Argument(index);
            if (argument != nullptr && !Uniform(argument)) return true;
        }
        return false;
    }

    std::unordered_set<const IrValue*> varying;
};

class PairSymmetry {
public:
    [[nodiscard]] bool Symmetric(const IrValue* low, const IrValue* high) {
        const auto root = intern(low->Resolve(), high->Resolve());
        std::vector<std::pair<const IrValue*, const IrValue*>> children;
        while (!unexpanded.empty() && !overflow) {
            const auto current = unexpanded.back();
            unexpanded.pop_back();
            children.clear();
            if (!expand(pairs[current].low, pairs[current].high, children)) {
                pairs[current].broken = true;
                pending.push_back(current);
                continue;
            }
            for (const auto& [childLow, childHigh] : children) {
                const auto child = intern(childLow, childHigh);
                pairs[child].parents.push_back(current);
                if (pairs[child].broken) pending.push_back(child);
            }
        }
        if (overflow) return false;
        settle();
        return !pairs[root].broken;
    }

private:
    struct Pair {
        const IrValue* low = nullptr;
        const IrValue* high = nullptr;
        std::vector<std::size_t> parents;
        bool broken = false;
    };

    std::size_t intern(const IrValue* low, const IrValue* high) {
        const auto [found, inserted] = index.try_emplace({low, high}, pairs.size());
        if (inserted) {
            pairs.push_back({low, high, {}, false});
            unexpanded.push_back(found->second);
            overflow |= pairs.size() > PairLimit;
        }
        return found->second;
    }

    static bool expand(const IrValue* low, const IrValue* high, std::vector<std::pair<const IrValue*, const IrValue*>>& children) {
        if (low == high) return true;
        if (low->HasImmediate() || high->HasImmediate()) return low->HasImmediate() && high->HasImmediate() && low->Type() == high->Type() && low->Type() == IrType::U32 && low->ImmediateU32() == high->ImmediateU32();
        if (isBallotWord(low, 0u) && isBallotWord(high, 1u)) return ballotOfHalves(low, high) != nullptr;
        if (low->Opcode() != high->Opcode()) return false;
        switch (low->Opcode()) {
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
            children.emplace_back(low->Argument(0)->Resolve(), high->Argument(0)->Resolve());
            children.emplace_back(low->Argument(1)->Resolve(), high->Argument(1)->Resolve());
            return true;
        case IrOpcode::BitwiseNot32:
            children.emplace_back(low->Argument(0)->Resolve(), high->Argument(0)->Resolve());
            return true;
        case IrOpcode::SelectU32:
            if (low->Argument(0)->Resolve() != high->Argument(0)->Resolve()) return false;
            children.emplace_back(low->Argument(1)->Resolve(), high->Argument(1)->Resolve());
            children.emplace_back(low->Argument(2)->Resolve(), high->Argument(2)->Resolve());
            return true;
        case IrOpcode::Phi: {
            if (low->Parent() != high->Parent() || low->PhiBlockCount() != high->PhiBlockCount()) return false;
            for (std::size_t incoming = 0; incoming < low->PhiBlockCount(); ++incoming) {
                const IrValue* match = nullptr;
                for (std::size_t other = 0; other < high->PhiBlockCount(); ++other) {
                    if (high->PhiBlock(other) == low->PhiBlock(incoming)) match = high->Argument(other)->Resolve();
                }
                if (match == nullptr) return false;
                children.emplace_back(low->Argument(incoming)->Resolve(), match);
            }
            return true;
        }
        default:
            return false;
        }
    }

    void settle() {
        while (!pending.empty()) {
            const auto broken = pending.back();
            pending.pop_back();
            for (const auto parent : pairs[broken].parents) {
                if (pairs[parent].broken) continue;
                pairs[parent].broken = true;
                pending.push_back(parent);
            }
        }
    }

    std::map<std::pair<const IrValue*, const IrValue*>, std::size_t> index;
    std::vector<Pair> pairs;
    std::vector<std::size_t> unexpanded;
    std::vector<std::size_t> pending;
    bool overflow = false;
};

struct Term {
    const IrValue* value = nullptr;
    const IrValue* high = nullptr;

    bool operator==(const Term& other) const = default;
};

void collectTerms(const IrValue* predicate, std::vector<Term>& terms, bool& never, std::uint32_t depth = 0);

void collectPairTerms(const IrValue* low, const IrValue* high, std::vector<Term>& terms, bool& never, std::uint32_t depth) {
    if (low->HasImmediate() && high->HasImmediate() && low->Type() == IrType::U32 && high->Type() == IrType::U32 && low->ImmediateU32() == high->ImmediateU32()) {
        if (low->ImmediateU32() == 0u) never = true;
        else if (low->ImmediateU32() != 0xffffffffu) terms.push_back({low, high});
        return;
    }
    if (const IrValue* ballot = depth < DepthLimit ? ballotOfHalves(low, high) : nullptr; ballot != nullptr) {
        collectTerms(ballot->Argument(0), terms, never, depth + 1u);
        return;
    }
    if (depth < DepthLimit && low->Opcode() == IrOpcode::BitwiseAnd32 && high->Opcode() == IrOpcode::BitwiseAnd32) {
        collectPairTerms(low->Argument(0)->Resolve(), high->Argument(0)->Resolve(), terms, never, depth + 1u);
        collectPairTerms(low->Argument(1)->Resolve(), high->Argument(1)->Resolve(), terms, never, depth + 1u);
        return;
    }
    terms.push_back({low, high});
}

void collectTerms(const IrValue* predicate, std::vector<Term>& terms, bool& never, std::uint32_t depth) {
    predicate = predicate->Resolve();
    if (predicate->HasImmediate() && predicate->Type() == IrType::Bool) {
        if (!predicate->ImmediateBool()) never = true;
        return;
    }
    if (depth < DepthLimit && predicate->Opcode() == IrOpcode::LogicalAnd) {
        collectTerms(predicate->Argument(0), terms, never, depth + 1u);
        collectTerms(predicate->Argument(1), terms, never, depth + 1u);
        return;
    }
    if (const IrValue* word = ownBitWord(predicate); word != nullptr && depth < DepthLimit) {
        if (isHalfWord(word)) {
            collectPairTerms(word->Argument(1)->Resolve(), word->Argument(2)->Resolve(), terms, never, depth + 1u);
            return;
        }
        if (word->HasImmediate()) {
            collectPairTerms(word, word, terms, never, depth + 1u);
            return;
        }
    }
    terms.push_back({predicate, nullptr});
}

class Implication {
public:
    [[nodiscard]] bool Implies(const IrValue* premise, const Term& conclusion) {
        steps = 0;
        return bit(premise, conclusion, 0);
    }

    [[nodiscard]] bool HalvesImply(const IrValue* low, const IrValue* high, const Term& conclusion) {
        steps = 0;
        return halves(low->Resolve(), high->Resolve(), conclusion, 0);
    }

private:
    static constexpr std::size_t StepLimit = std::size_t{1} << 16u;

    bool bit(const IrValue* premise, const Term& conclusion, std::uint32_t depth) {
        premise = premise->Resolve();
        if (++steps > StepLimit || depth > DepthLimit) return false;
        if (conclusion.high == nullptr && premise == conclusion.value) return true;
        if (premise->HasImmediate()) return premise->Type() == IrType::Bool && !premise->ImmediateBool();
        if (premise->Opcode() == IrOpcode::LogicalAnd) return bit(premise->Argument(0), conclusion, depth + 1u) || bit(premise->Argument(1), conclusion, depth + 1u);
        if (premise->Opcode() == IrOpcode::LogicalOr) return bit(premise->Argument(0), conclusion, depth + 1u) && bit(premise->Argument(1), conclusion, depth + 1u);
        if (premise->Opcode() == IrOpcode::SelectU1) return bit(premise->Argument(1), conclusion, depth + 1u) && bit(premise->Argument(2), conclusion, depth + 1u);
        if (premise->Opcode() == IrOpcode::Phi) {
            if (!openBits.insert(premise).second) return true;
            bool all = true;
            for (std::size_t incoming = 0; all && incoming < premise->PhiBlockCount(); ++incoming) all = bit(premise->Argument(incoming), conclusion, depth + 1u);
            openBits.erase(premise);
            return all;
        }
        const IrValue* word = ownBitWord(premise);
        if (word == nullptr) return false;
        if (isHalfWord(word)) return halves(word->Argument(1)->Resolve(), word->Argument(2)->Resolve(), conclusion, depth + 1u);
        return isImmediateU32(word, 0u);
    }

    bool halves(const IrValue* low, const IrValue* high, const Term& conclusion, std::uint32_t depth) {
        if (++steps > StepLimit || depth > DepthLimit) return false;
        if (low == conclusion.value && high == conclusion.high) return true;
        if (low->HasImmediate() || high->HasImmediate()) return isImmediateU32(low, 0u) && isImmediateU32(high, 0u);
        if (const IrValue* ballot = ballotOfHalves(low, high)) return bit(ballot->Argument(0), conclusion, depth + 1u);
        if (low->Opcode() != high->Opcode()) return false;
        const auto argument = [](const IrValue* value, std::size_t index) { return value->Argument(index)->Resolve(); };
        switch (low->Opcode()) {
        case IrOpcode::BitwiseAnd32:
            return halves(argument(low, 0), argument(high, 0), conclusion, depth + 1u) || halves(argument(low, 1), argument(high, 1), conclusion, depth + 1u);
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
            return halves(argument(low, 0), argument(high, 0), conclusion, depth + 1u) && halves(argument(low, 1), argument(high, 1), conclusion, depth + 1u);
        case IrOpcode::SelectU32:
            return argument(low, 0) == argument(high, 0) && halves(argument(low, 1), argument(high, 1), conclusion, depth + 1u) && halves(argument(low, 2), argument(high, 2), conclusion, depth + 1u);
        case IrOpcode::Phi: {
            if (low->Parent() != high->Parent() || low->PhiBlockCount() != high->PhiBlockCount()) return false;
            const std::pair key{low, high};
            if (open.contains(key)) return true;
            open.insert(key);
            bool all = true;
            for (std::size_t incoming = 0; all && incoming < low->PhiBlockCount(); ++incoming) {
                const IrValue* match = nullptr;
                for (std::size_t other = 0; other < high->PhiBlockCount(); ++other) {
                    if (high->PhiBlock(other) == low->PhiBlock(incoming)) match = argument(high, other);
                }
                all = match != nullptr && halves(argument(low, incoming), match, conclusion, depth + 1u);
            }
            open.erase(key);
            return all;
        }
        default:
            return false;
        }
    }

    std::set<std::pair<const IrValue*, const IrValue*>> open;
    std::set<const IrValue*> openBits;
    std::size_t steps = 0;
};

std::optional<Rejection> checkVccBranch(const BlockInfo& info, const Uniformity& uniformity) {
    if (info.condition == nullptr) return std::nullopt;
    const IrValue* condition = info.condition->Resolve();
    if (info.exec == nullptr) return Rejection{"a VCC branch without its exec mask", condition};
    const IrValue* exec = info.exec->Resolve();
    if (exec->Opcode() == IrOpcode::Unreachable) return Rejection{"a VCC branch whose exec mask was removed", condition};
    const IrValue* predicate = condition;
    if (info.terminator.condition == BranchCondition::VccZero) {
        if (condition->Opcode() != IrOpcode::LogicalNot) return uniformity.Uniform(condition) ? std::nullopt : std::optional<Rejection>(Rejection{"a VCC branch on lane-varying bits", condition});
        predicate = condition->Argument(0)->Resolve();
    }
    std::vector<Term> terms;
    bool never = false;
    collectTerms(predicate, terms, never);
    if (never) return std::nullopt;
    Implication implication;
    for (const auto& term : terms) {
        if (term.high == nullptr && uniformity.Uniform(term.value)) continue;
        if (implication.Implies(exec, term)) continue;
        return Rejection{"a VCC branch on lane-varying bits", term.value};
    }
    return std::nullopt;
}

std::optional<Rejection> checkLaneMasks(const IrProgram& program) {
    std::vector<const IrValue*> masks;
    std::unordered_set<const IrValue*> isMask;
    std::vector<const IrValue*> halfWords;
    std::unordered_set<const IrValue*> isHalf;
    std::vector<const IrValue*> lanes;
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::LaneId) lanes.push_back(inst);
            if (inst->Opcode() != IrOpcode::Ballot) continue;
            for (const IrUse& use : inst->OperandUses()) {
                if (!isBallotWord(use.user, 0u) && !isBallotWord(use.user, 1u)) return Rejection{"a ballot read other than as a lane mask", use.user};
                if (isMask.insert(use.user).second) masks.push_back(use.user);
            }
        }
    }
    for (std::size_t next = 0; next < masks.size(); ++next) {
        for (const IrUse& use : masks[next]->OperandUses()) {
            const IrValue* user = use.user;
            if (isHalfWord(user) && use.operand != 0u) {
                if (isHalf.insert(user).second) halfWords.push_back(user);
                continue;
            }
            const bool combines = user->Opcode() == IrOpcode::Identity || (isLaneMaskOp(user->Opcode()) && (user->Opcode() != IrOpcode::SelectU32 || use.operand != 0u));
            if (!combines) return Rejection{"a lane mask read as a number", user};
            if (isMask.insert(user).second) masks.push_back(user);
        }
    }
    for (const IrValue* lane : lanes) {
        for (const IrUse& laneUse : lane->OperandUses()) {
            const IrValue* user = laneUse.user;
            if (isHalfSelector(user)) {
                for (const IrUse& use : user->OperandUses()) {
                    if (!isHalfWord(use.user) || use.operand != 0u) return Rejection{"the lane id read as a number", use.user};
                    if (isHalf.insert(use.user).second) halfWords.push_back(use.user);
                }
                continue;
            }
            if (!isBitSelector(user)) return Rejection{"the lane id read as a number", user};
            for (const IrUse& shift : user->OperandUses()) {
                const IrValue* shifted = shift.user;
                if (shifted->Opcode() != IrOpcode::ShiftRightLogical32 || shift.operand != 1u) return Rejection{"the lane id read as a number", shifted};
                const IrValue* word = shifted->Argument(0)->Resolve();
                if (!isHalfWord(word) && !word->HasImmediate()) return Rejection{"a lane's bit read from one half of a lane mask", shifted};
                for (const IrUse& bit : shifted->OperandUses()) {
                    if (bit.user->Opcode() != IrOpcode::BitwiseAnd32 || otherThan(bit.user, 1u) != shifted) return Rejection{"a lane mask shifted by the lane id read as a number", bit.user};
                }
            }
        }
    }
    PairSymmetry symmetry;
    for (const IrValue* word : halfWords) {
        for (const IrUse& use : word->OperandUses()) {
            if (use.user->Opcode() != IrOpcode::ShiftRightLogical32 || use.operand != 0u || !isBitSelector(use.user->Argument(1))) return Rejection{"half of a lane mask read as a number", use.user};
        }
        if (!symmetry.Symmetric(word->Argument(1), word->Argument(2))) return Rejection{"a lane mask whose halves differ", word};
    }
    return std::nullopt;
}

struct Affine {
    std::array<std::uint64_t, 3> coefficients{};
    std::uint64_t constant = 0;
};

class AffineEvaluator {
public:
    explicit AffineEvaluator(const std::array<std::uint32_t, 3>& sizes) : sizes(sizes) {}

    [[nodiscard]] std::uint64_t Max(const Affine& affine) const {
        std::uint64_t result = affine.constant;
        for (std::size_t axis = 0; axis < 3u; ++axis) result += affine.coefficients[axis] * (sizes[axis] - 1u);
        return result;
    }

    [[nodiscard]] std::optional<Affine> Of(const IrValue* value, std::uint32_t depth = 0) const {
        value = value->Resolve();
        if (value->HasImmediate()) return value->Type() == IrType::U32 ? std::optional<Affine>(Affine{{}, value->ImmediateU32()}) : std::nullopt;
        if (depth > DepthLimit) return std::nullopt;
        const auto argument = [&](std::size_t index) { return Of(value->Argument(index), depth + 1u); };
        const auto immediate = [&](std::size_t index) -> std::optional<std::uint32_t> {
            const IrValue* operand = value->Argument(index)->Resolve();
            return operand->HasImmediate() && operand->Type() == IrType::U32 ? std::optional<std::uint32_t>(operand->ImmediateU32()) : std::nullopt;
        };
        std::optional<Affine> result;
        switch (value->Opcode()) {
        case IrOpcode::GetBuiltin: {
            const auto kind = immediate(0);
            if (!kind) return std::nullopt;
            if (static_cast<StageInputKind>(*kind) == StageInputKind::LocalInvocationIndex) return Affine{{1u, sizes[0], std::uint64_t{sizes[0]} * sizes[1]}, 0u};
            const auto axis = immediate(1);
            if (static_cast<StageInputKind>(*kind) != StageInputKind::LocalInvocationId || !axis || *axis >= 3u) return std::nullopt;
            Affine id;
            id.coefficients[*axis] = 1u;
            return id;
        }
        case IrOpcode::IAdd32: {
            const auto a = argument(0), b = argument(1);
            if (!a || !b) return std::nullopt;
            result = sum(*a, *b);
            break;
        }
        case IrOpcode::IMul32: {
            auto a = argument(0), b = argument(1);
            if (!a || !b) return std::nullopt;
            if (!isConstant(*a)) std::swap(a, b);
            if (!isConstant(*a)) return std::nullopt;
            result = scaled(*b, a->constant);
            break;
        }
        case IrOpcode::ShiftLeftLogical32: {
            const auto a = argument(0);
            const auto shift = immediate(1);
            if (!a || !shift || *shift >= 32u) return std::nullopt;
            result = scaled(*a, std::uint64_t{1} << *shift);
            break;
        }
        case IrOpcode::BitFieldUExtract: {
            const auto a = argument(0);
            const auto offset = immediate(1), count = immediate(2);
            if (!a || offset != 0u || !count || *count > 32u || (*count < 32u && Max(*a) >> *count != 0u)) return std::nullopt;
            result = a;
            break;
        }
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseOr32: {
            auto mask = immediate(1);
            auto a = argument(0);
            if (!mask) {
                mask = immediate(0);
                a = argument(1);
            }
            if (!a || !mask) return std::nullopt;
            if (value->Opcode() == IrOpcode::BitwiseAnd32) {
                if ((*mask & (*mask + 1u)) != 0u || Max(*a) > *mask) return std::nullopt;
                result = a;
            } else {
                std::uint32_t zeros = 32u;
                if (a->constant != 0u) zeros = std::min<std::uint32_t>(zeros, static_cast<std::uint32_t>(std::countr_zero(a->constant)));
                for (const auto coefficient : a->coefficients) {
                    if (coefficient != 0u) zeros = std::min<std::uint32_t>(zeros, static_cast<std::uint32_t>(std::countr_zero(coefficient)));
                }
                if (zeros < 32u && (*mask >> zeros) != 0u) return std::nullopt;
                result = sum(*a, Affine{{}, *mask});
            }
            break;
        }
        default:
            return std::nullopt;
        }
        if (!result || Max(*result) > 0xffffffffull) return std::nullopt;
        return result;
    }

private:
    static bool isConstant(const Affine& affine) {
        return affine.coefficients == std::array<std::uint64_t, 3>{};
    }

    static Affine sum(const Affine& a, const Affine& b) {
        Affine result;
        for (std::size_t axis = 0; axis < 3u; ++axis) result.coefficients[axis] = a.coefficients[axis] + b.coefficients[axis];
        result.constant = a.constant + b.constant;
        return result;
    }

    std::optional<Affine> scaled(const Affine& affine, std::uint64_t factor) const {
        if (factor > 0xffffffffull) return std::nullopt;
        Affine result;
        for (std::size_t axis = 0; axis < 3u; ++axis) {
            if (affine.coefficients[axis] > 0xffffffffull) return std::nullopt;
            result.coefficients[axis] = affine.coefficients[axis] * factor;
        }
        if (affine.constant > 0xffffffffull) return std::nullopt;
        result.constant = affine.constant * factor;
        return result;
    }

    std::array<std::uint32_t, 3> sizes;
};

std::optional<Rejection> checkLds(const IrProgram& program, const std::array<std::uint32_t, 3>& sizes, bool& writes) {
    const AffineEvaluator evaluator(sizes);
    std::optional<std::array<std::uint64_t, 3>> stride;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> footprint;
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            const auto access = SharedAccessOf(inst->Opcode());
            if (access == SharedAccess::None) continue;
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= program.Resources().memoryInfo.size()) return Rejection{"an LDS access without memory metadata", inst};
            const auto& memory = program.Resources().memoryInfo[index];
            if (memory.kind != ResourceKind::Lds) return Rejection{"a GDS access", inst};
            const auto bytes = sharedBytes(inst->Opcode());
            if (bytes == 0u || inst->ArgumentCount() == 0u) return Rejection{"an LDS access that counts or locks across the wave", inst};
            writes |= access != SharedAccess::Read;
            auto affine = evaluator.Of(inst->Argument(0));
            if (!affine) return Rejection{"an LDS address that is not affine in the local invocation id", inst};
            for (std::size_t axis = 0; axis < 3u; ++axis) {
                if (sizes[axis] == 1u) affine->coefficients[axis] = 0u;
            }
            if (stride && *stride != affine->coefficients) return Rejection{"LDS accesses at different strides", inst};
            stride = affine->coefficients;
            const auto begin = affine->constant + memory.offset;
            if (evaluator.Max(*affine) + memory.offset + bytes > 0x100000000ull) return Rejection{"an LDS address that wraps", inst};
            footprint.emplace_back(begin, begin + bytes);
        }
    }
    if (!stride) return std::nullopt;
    std::sort(footprint.begin(), footprint.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : footprint) {
        if (!merged.empty() && range.first <= merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    if (merged.size() > FootprintLimit) return Rejection{"an LDS footprint of too many ranges", nullptr};
    std::uint64_t offsets = 1;
    for (const auto size : sizes) offsets *= 2u * size - 1u;
    if (offsets > LaneOffsetLimit) return Rejection{"a workgroup too large to check LDS privacy", nullptr};
    std::array<std::int64_t, 3> delta{};
    for (std::int64_t x = 1 - static_cast<std::int64_t>(sizes[0]); x < static_cast<std::int64_t>(sizes[0]); ++x) {
        for (std::int64_t y = 1 - static_cast<std::int64_t>(sizes[1]); y < static_cast<std::int64_t>(sizes[1]); ++y) {
            for (std::int64_t z = 1 - static_cast<std::int64_t>(sizes[2]); z < static_cast<std::int64_t>(sizes[2]); ++z) {
                if (x == 0 && y == 0 && z == 0) continue;
                delta = {x, y, z};
                std::int64_t shift = 0;
                for (std::size_t axis = 0; axis < 3u; ++axis) shift += delta[axis] * static_cast<std::int64_t>((*stride)[axis]);
                for (const auto& moved : merged) {
                    for (const auto& fixed : merged) {
                        const auto begin = static_cast<std::int64_t>(moved.first) + shift;
                        const auto end = static_cast<std::int64_t>(moved.second) + shift;
                        if (begin < static_cast<std::int64_t>(fixed.second) && static_cast<std::int64_t>(fixed.first) < end) return Rejection{"lanes that share LDS bytes", nullptr};
                    }
                }
            }
        }
    }
    return std::nullopt;
}

bool sameWord(const IrValue* left, const IrValue* right) {
    left = left->Resolve();
    right = right->Resolve();
    if (left == right) return true;
    return left->HasImmediate() && right->HasImmediate() && left->Type() == IrType::U32 && right->Type() == IrType::U32 && left->ImmediateU32() == right->ImmediateU32();
}

bool isSelect(IrOpcode opcode) {
    return opcode == IrOpcode::Select || opcode == IrOpcode::SelectU1 || opcode == IrOpcode::SelectF32 || opcode == IrOpcode::SelectU32;
}

bool writesMemory(IrOpcode opcode) {
    const auto buffer = BufferAccessOf(opcode);
    const auto address = AddressOpcodeInfoOf(opcode).access;
    const auto image = ImageOpcodeInfoOf(opcode).access;
    const auto shared = SharedAccessOf(opcode);
    return buffer == BufferAccess::Write || buffer == BufferAccess::Atomic || address == AddressAccess::Write || address == AddressAccess::Atomic || image == ImageAccess::Write || image == ImageAccess::Atomic || (shared != SharedAccess::None && shared != SharedAccess::Read);
}

const IrValue* lanePredicate(const IrValue& inst) {
    for (std::size_t index = inst.ArgumentCount(); index > 0u; --index) {
        const IrValue* argument = inst.Argument(index - 1u);
        if (argument != nullptr && argument->Resolve()->Type() == IrType::Bool) return argument->Resolve();
    }
    return nullptr;
}

const IrValue* branchExec(const BlockInfo& info) {
    if (info.exec != nullptr) return info.exec->Resolve();
    const IrValue* condition = info.condition->Resolve();
    if (info.terminator.condition == BranchCondition::ExecNonZero) return condition;
    if (info.terminator.condition == BranchCondition::ExecZero && condition->Opcode() == IrOpcode::LogicalNot) return condition->Argument(0)->Resolve();
    return nullptr;
}

bool pureLaneOp(IrOpcode opcode) {
    if (opcode == IrOpcode::ReadConst || opcode == IrOpcode::ReadConstBuffer) return true;
    if (!isLaneLocal(opcode) || opcode == IrOpcode::Phi || opcode == IrOpcode::ShaderClock || opcode == IrOpcode::RealtimeClock || opcode == IrOpcode::LoadVectorWindow || opcode == IrOpcode::StoreVectorWindow) return false;
    return BufferAccessOf(opcode) == BufferAccess::None && AddressOpcodeInfoOf(opcode).access == AddressAccess::None && ImageOpcodeInfoOf(opcode).access == ImageAccess::None && SharedAccessOf(opcode) == SharedAccess::None;
}

bool halfDecided(const BlockInfo& info, const Uniformity& uniformity) {
    if (info.terminator.kind != TerminatorKind::ConditionalBranch || info.condition == nullptr) return false;
    switch (info.terminator.condition) {
    case BranchCondition::ExecZero:
    case BranchCondition::ExecNonZero:
        return true;
    case BranchCondition::VccZero:
    case BranchCondition::VccNonZero: {
        const IrValue* predicate = info.condition->Resolve();
        if (info.terminator.condition == BranchCondition::VccZero && predicate->Opcode() == IrOpcode::LogicalNot) predicate = predicate->Argument(0)->Resolve();
        std::vector<Term> terms;
        bool never = false;
        collectTerms(predicate, terms, never);
        return std::ranges::any_of(terms, [&](const Term& term) { return term.high != nullptr || !uniformity.Uniform(term.value); });
    }
    default:
        return false;
    }
}

class SkippedRegions {
public:
    SkippedRegions(const IrProgram& program, const Uniformity& uniformity) : program(program), uniformity(uniformity), order(program.BlockOrder()) {
        const auto& infos = program.Metadata().blockInfo;
        for (std::size_t index = 0; index < order.size(); ++index) {
            indices.emplace(order[index], index);
            if (index < infos.size()) ids.emplace(infos[index].id, index);
        }
        postDominate();
        observe();
    }

    [[nodiscard]] std::optional<Rejection> Check() {
        const auto& infos = program.Metadata().blockInfo;
        for (std::size_t branch = 0; branch < order.size() && branch < infos.size(); ++branch) {
            if (!halfDecided(infos[branch], uniformity)) continue;
            if (auto rejected = checkBranch(branch, infos[branch])) return rejected;
        }
        return std::nullopt;
    }

private:
    using Bits = std::vector<std::uint64_t>;

    static bool test(const Bits& bits, std::size_t index) {
        return ((bits[index / 64u] >> (index % 64u)) & 1u) != 0u;
    }

    static void set(Bits& bits, std::size_t index) {
        bits[index / 64u] |= std::uint64_t{1} << (index % 64u);
    }

    std::size_t blockIndex(const IrBlock* block) const {
        const auto found = indices.find(block);
        return found == indices.end() ? order.size() : found->second;
    }

    std::size_t blockOf(std::uint32_t id) const {
        const auto found = ids.find(id);
        return found == ids.end() ? order.size() : found->second;
    }

    bool inRegion(const IrBlock* block) const {
        const auto index = blockIndex(block);
        return index < order.size() && region[index] != 0;
    }

    bool definedInRegion(const IrValue* value) const {
        return !value->HasImmediate() && inRegion(value->Parent());
    }

    void postDominate() {
        const std::size_t exit = order.size();
        const std::size_t words = (exit + 64u) / 64u;
        const Bits all(words, ~std::uint64_t{0});
        dominators.assign(exit + 1u, all);
        dominators[exit].assign(words, 0u);
        set(dominators[exit], exit);
        bool changed = true;
        while (changed) {
            changed = false;
            for (std::size_t index = exit; index-- > 0u;) {
                Bits next = order[index]->Successors().empty() ? dominators[exit] : all;
                for (const IrBlock* successor : order[index]->Successors()) {
                    const auto& after = dominators[blockIndex(successor)];
                    for (std::size_t word = 0; word < words; ++word) next[word] &= after[word];
                }
                set(next, index);
                if (next == dominators[index]) continue;
                dominators[index] = std::move(next);
                changed = true;
            }
        }
        reachesEnd.assign(exit, 0);
        std::vector<std::size_t> pending;
        for (std::size_t index = 0; index < exit; ++index) {
            if (!order[index]->Successors().empty()) continue;
            reachesEnd[index] = 1;
            pending.push_back(index);
        }
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            for (const IrBlock* predecessor : order[current]->Predecessors()) {
                const auto index = blockIndex(predecessor);
                if (index >= exit || reachesEnd[index]) continue;
                reachesEnd[index] = 1;
                pending.push_back(index);
            }
        }
    }

    void observe() {
        std::vector<const IrValue*> pending;
        const auto mark = [&](const IrValue* value) {
            if (value == nullptr) return;
            value = value->Resolve();
            if (value->HasImmediate() || !observed.insert(value).second) return;
            if (value->Opcode() == IrOpcode::Phi) pending.push_back(value);
        };
        for (const IrBlock* block : order) {
            for (const IrValue* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::Phi) continue;
                for (std::size_t index = 0; index < inst->ArgumentCount(); ++index) mark(inst->Argument(index));
            }
        }
        while (!pending.empty()) {
            const IrValue* phi = pending.back();
            pending.pop_back();
            for (std::size_t incoming = 0; incoming < phi->PhiBlockCount(); ++incoming) mark(phi->Argument(incoming));
        }
    }

    std::size_t immediatePostDominator(std::size_t block) const {
        std::size_t best = order.size();
        std::size_t bestCount = 0;
        for (std::size_t candidate = 0; candidate < order.size(); ++candidate) {
            if (candidate == block || !test(dominators[block], candidate)) continue;
            std::size_t count = 0;
            for (const auto word : dominators[candidate]) count += static_cast<std::size_t>(std::popcount(word));
            if (count <= bestCount) continue;
            best = candidate;
            bestCount = count;
        }
        return best;
    }

    std::vector<char> reach(std::size_t from, std::size_t stop) const {
        std::vector<char> seen(order.size(), 0);
        if (from >= order.size() || from == stop) return seen;
        std::vector<std::size_t> pending{from};
        seen[from] = 1;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            for (const IrBlock* successor : order[current]->Successors()) {
                const auto next = blockIndex(successor);
                if (next >= order.size() || next == stop || seen[next]) continue;
                seen[next] = 1;
                pending.push_back(next);
            }
        }
        return seen;
    }

    std::optional<Rejection> checkBranch(std::size_t branch, const BlockInfo& info) {
        const IrValue* condition = info.condition->Resolve();
        exec = branchExec(info);
        if (exec == nullptr) return Rejection{"a branch one half can decide alone without its exec mask", condition};
        terms.clear();
        bool never = false;
        collectTerms(exec, terms, never);
        if (never || terms.empty()) return std::nullopt;
        if (!reachesEnd[branch]) return Rejection{"a branch one half can decide alone that never reaches the end", condition};
        const bool skipOnFalse = info.terminator.condition == BranchCondition::ExecNonZero || info.terminator.condition == BranchCondition::VccNonZero;
        const auto skip = blockOf(skipOnFalse ? info.terminator.falseBlock : info.terminator.trueBlock);
        const auto wave = blockOf(skipOnFalse ? info.terminator.trueBlock : info.terminator.falseBlock);
        const auto merge = immediatePostDominator(branch);
        branchIndex = branch;
        skipIndex = skip;
        mergeIndex = merge;
        region = reach(wave, merge);
        skippedRegion = reach(skip, merge);
        for (std::size_t index = 0; index < region.size(); ++index) region[index] = region[index] != 0 && skippedRegion[index] == 0 ? 1 : 0;
        cyclic = region[branch] != 0;
        stability.clear();
        skippedValues.clear();
        if (cyclic && !monotone()) return Rejection{"a loop left through a branch one half can decide alone whose exec can widen", condition};
        for (std::size_t index = 0; index < order.size(); ++index) {
            if (region[index] != 0) {
                for (const IrValue* inst : order[index]->Instructions()) {
                    if (auto rejected = checkValue(*inst)) return rejected;
                }
                continue;
            }
            if (std::ranges::none_of(order[index]->Predecessors(), [&](const IrBlock* predecessor) { return inRegion(predecessor); })) continue;
            for (const IrValue* inst : order[index]->Instructions()) {
                if (inst->Opcode() != IrOpcode::Phi || !observed.contains(inst)) continue;
                if (auto rejected = checkExit(*inst)) return rejected;
            }
        }
        return std::nullopt;
    }

    std::optional<Rejection> checkValue(const IrValue& inst) {
        const auto opcode = inst.Opcode();
        if (opcode == IrOpcode::StoreVectorWindow && !windowKept(inst.Argument(1), inst.Argument(0)->Resolve(), 0)) return Rejection{"a vector window store one half can skip", &inst};
        if (writesMemory(opcode)) {
            const IrValue* predicate = lanePredicate(inst);
            if (predicate == nullptr || !impliesExec(predicate)) return Rejection{"a memory write one half can skip outside the branch's exec", &inst};
        }
        for (const IrUse& use : inst.OperandUses()) {
            const IrValue* user = use.user;
            if (user->Opcode() == IrOpcode::Phi || inRegion(user->Parent())) continue;
            if (cyclic && stable(&inst, 0)) continue;
            return Rejection{cyclic ? "a value that leaves a loop one half can leave early" : "a value that leaves a region one half can skip", &inst};
        }
        return std::nullopt;
    }

    std::optional<Rejection> checkExit(const IrValue& phi) {
        if (!cyclic) return merges(phi) ? std::nullopt : std::optional<Rejection>(Rejection{"a value that leaves a region one half can skip", &phi});
        for (std::size_t incoming = 0; incoming < phi.PhiBlockCount(); ++incoming) {
            if (inRegion(phi.PhiBlock(incoming)) && !stable(phi.Argument(incoming), 0)) return Rejection{"a value that leaves a loop one half can leave early", &phi};
        }
        return std::nullopt;
    }

    bool impliesExec(const IrValue* predicate) {
        Implication implication;
        return std::ranges::all_of(terms, [&](const Term& term) { return implication.Implies(predicate, term); });
    }

    bool halvesImplyExec(const IrValue* low, const IrValue* high) {
        Implication implication;
        return std::ranges::all_of(terms, [&](const Term& term) { return implication.HalvesImply(low, high, term); });
    }

    bool monotone() {
        if (!definedInRegion(exec)) return true;
        for (const Term& term : terms) {
            const IrValue* low = term.value->Resolve();
            const IrValue* high = term.high == nullptr ? nullptr : term.high->Resolve();
            if (low->Opcode() != IrOpcode::Phi || !definedInRegion(low)) continue;
            if (high != nullptr && (high->Opcode() != IrOpcode::Phi || high->Parent() != low->Parent())) continue;
            bool entered = false;
            bool narrows = true;
            for (std::size_t incoming = 0; narrows && incoming < low->PhiBlockCount(); ++incoming) {
                const IrBlock* from = low->PhiBlock(incoming);
                if (!inRegion(from)) {
                    entered = true;
                    continue;
                }
                if (high == nullptr) {
                    narrows = impliesExec(low->Argument(incoming));
                    continue;
                }
                const IrValue* matching = nullptr;
                for (std::size_t other = 0; other < high->PhiBlockCount(); ++other) {
                    if (high->PhiBlock(other) == from) matching = high->Argument(other);
                }
                narrows = matching != nullptr && halvesImplyExec(low->Argument(incoming), matching);
            }
            if (entered && narrows) return true;
        }
        return false;
    }

    bool skipSide(const IrBlock* from) const {
        const auto index = blockIndex(from);
        return index < order.size() && (skippedRegion[index] != 0 || (index == branchIndex && skipIndex == mergeIndex));
    }

    bool merges(const IrValue& phi) {
        bool skippedPath = false;
        for (std::size_t incoming = 0; incoming < phi.PhiBlockCount(); ++incoming) {
            if (!skipSide(phi.PhiBlock(incoming))) continue;
            skippedPath = true;
            const IrValue* kept = phi.Argument(incoming)->Resolve();
            for (std::size_t other = 0; other < phi.PhiBlockCount(); ++other) {
                if (!inRegion(phi.PhiBlock(other))) continue;
                if (!sameWord(skippedValue(phi.Argument(other)), skippedValue(kept)) && !chainTo(phi.Argument(other), kept, 0)) return false;
            }
        }
        return skippedPath;
    }

    const IrValue* settled(const IrValue* value) const {
        for (std::uint32_t step = 0; step <= DepthLimit; ++step) {
            const auto found = skippedValues.find(value);
            if (found == skippedValues.end() || found->second == value) return value;
            value = found->second;
        }
        return value;
    }

    const IrValue* skippedValue(const IrValue* value) {
        value = value->Resolve();
        if (const auto found = skippedValues.find(value); found != skippedValues.end()) return settled(found->second);
        const std::size_t depth = passing.size();
        if (const auto open = passing.find(value); open != passing.end()) {
            lowestOpen = std::min(lowestOpen, open->second);
            return value;
        }
        passing.emplace(value, depth);
        const std::size_t outer = std::exchange(lowestOpen, NoOpenCycle);
        const IrValue* result = value;
        if (isSelect(value->Opcode()) && value->ArgumentCount() == 3u) {
            const IrValue* kept = skippedValue(value->Argument(2));
            if (impliesExec(value->Argument(0)) || sameWord(skippedValue(value->Argument(1)), kept)) result = kept;
        } else if (value->Opcode() == IrOpcode::Phi && definedInRegion(value)) {
            const IrValue* base = nullptr;
            bool agree = true;
            for (std::size_t incoming = 0; agree && incoming < value->PhiBlockCount(); ++incoming) {
                const IrValue* candidate = skippedValue(value->Argument(incoming));
                if (candidate == value) continue;
                if (base == nullptr) base = candidate;
                else agree = sameWord(base, candidate);
            }
            if (agree && base != nullptr) result = base;
        }
        passing.erase(value);
        const bool pending = lowestOpen < depth;
        if (!pending || result != value) skippedValues.emplace(value, result);
        lowestOpen = std::min(outer, pending ? lowestOpen : NoOpenCycle);
        return settled(result);
    }

    bool chainTo(const IrValue* value, const IrValue* kept, std::uint32_t depth) {
        value = value->Resolve();
        if (value == kept) return true;
        if (depth > DepthLimit) return false;
        if (isSelect(value->Opcode()) && value->ArgumentCount() == 3u && chainTo(value->Argument(2), kept, depth + 1u) && (impliesExec(value->Argument(0)) || chainTo(value->Argument(1), kept, depth + 1u))) return true;
        if (value->Opcode() == IrOpcode::Phi && definedInRegion(value)) {
            if (!visiting.insert(value).second) return true;
            bool all = true;
            for (std::size_t incoming = 0; all && incoming < value->PhiBlockCount(); ++incoming) all = chainTo(value->Argument(incoming), kept, depth + 1u);
            visiting.erase(value);
            if (all) return true;
        }
        if (value->Type() == IrType::Bool && kept->Type() == IrType::Bool) return impliesExec(value) && impliesExec(kept);
        if (value->Type() != IrType::U32 || kept->Type() != IrType::U32) return false;
        if (value->ArgumentCount() == 2u) {
            const IrValue* left = value->Argument(0);
            const IrValue* right = value->Argument(1);
            if (value->Opcode() == IrOpcode::BitwiseAnd32 && ((chainTo(left, kept, depth + 1u) && fullWord(right, 0)) || (chainTo(right, kept, depth + 1u) && fullWord(left, 0)))) return true;
            if ((value->Opcode() == IrOpcode::BitwiseOr32 || value->Opcode() == IrOpcode::BitwiseXor32) && ((chainTo(left, kept, depth + 1u) && boundedWord(right, 0)) || (chainTo(right, kept, depth + 1u) && boundedWord(left, 0)))) return true;
        }
        return boundedWord(value, 0) && boundedWord(kept, 0);
    }

    bool fullWord(const IrValue* word, std::uint32_t depth) {
        word = word->Resolve();
        if (word->HasImmediate()) return word->Type() == IrType::U32 && word->ImmediateU32() == 0xffffffffu;
        if (depth > DepthLimit) return false;
        switch (word->Opcode()) {
        case IrOpcode::BitwiseNot32:
            return boundedWord(word->Argument(0), depth + 1u);
        case IrOpcode::BitwiseOr32:
            return fullWord(word->Argument(0), depth + 1u) || fullWord(word->Argument(1), depth + 1u);
        case IrOpcode::BitwiseAnd32:
            return fullWord(word->Argument(0), depth + 1u) && fullWord(word->Argument(1), depth + 1u);
        default:
            return false;
        }
    }

    bool windowKept(const IrValue* value, const IrValue* index, std::uint32_t depth) {
        value = value->Resolve();
        if (value->Opcode() == IrOpcode::LoadVectorWindow) return sameWord(value->Argument(0)->Resolve(), index);
        if (depth > DepthLimit || !isSelect(value->Opcode()) || value->ArgumentCount() != 3u) return false;
        if (!windowKept(value->Argument(2), index, depth + 1u)) return false;
        return impliesExec(value->Argument(0)) || windowKept(value->Argument(1), index, depth + 1u);
    }

    bool boundedWord(const IrValue* word, std::uint32_t depth) {
        word = word->Resolve();
        if (word->HasImmediate()) return word->Type() == IrType::U32 && word->ImmediateU32() == 0u;
        if (depth > DepthLimit) return false;
        if (terms.size() == 1u && terms.front().high != nullptr && (terms.front().value->Resolve() == word || terms.front().high->Resolve() == word)) return true;
        if (isBallotWord(word, 0u) || isBallotWord(word, 1u)) return impliesExec(word->Argument(0)->Resolve()->Argument(0));
        const auto argument = [&](std::size_t index) { return word->Argument(index); };
        switch (word->Opcode()) {
        case IrOpcode::BitwiseAnd32:
            return boundedWord(argument(0), depth + 1u) || boundedWord(argument(1), depth + 1u);
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
            return boundedWord(argument(0), depth + 1u) && boundedWord(argument(1), depth + 1u);
        case IrOpcode::SelectU32:
            return boundedWord(argument(1), depth + 1u) && boundedWord(argument(2), depth + 1u);
        case IrOpcode::Phi: {
            if (!visiting.insert(word).second) return true;
            bool all = true;
            for (std::size_t incoming = 0; all && incoming < word->PhiBlockCount(); ++incoming) all = boundedWord(argument(incoming), depth + 1u);
            visiting.erase(word);
            return all;
        }
        default:
            return false;
        }
    }

    bool stable(const IrValue* value, std::uint32_t depth) {
        value = value->Resolve();
        if (!definedInRegion(value)) return true;
        if (depth > DepthLimit) return false;
        if (const auto found = stability.find(value); found != stability.end()) return found->second;
        stability.emplace(value, true);
        const auto arguments = [&] {
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
                if (!stable(value->Argument(index), depth + 1u)) return false;
            }
            return true;
        };
        bool result = false;
        if (value->Opcode() == IrOpcode::Phi) {
            result = stablePhi(*value, depth);
        } else if (isSelect(value->Opcode()) && value->ArgumentCount() == 3u) {
            result = (impliesExec(value->Argument(0)) && stable(value->Argument(2), depth + 1u)) || arguments();
        } else if (value->Opcode() == IrOpcode::LoadVectorWindow) {
            result = stable(value->Argument(0), depth + 1u);
        } else if (value->Type() == IrType::Bool && impliesExec(value)) {
            result = true;
        } else if (pureLaneOp(value->Opcode())) {
            result = arguments();
        } else {
            result = boundedWord(value, 0);
        }
        stability[value] = result;
        return result;
    }

    bool stablePhi(const IrValue& phi, std::uint32_t depth) {
        const bool entered = std::ranges::any_of(phi.Parent()->Predecessors(), [&](const IrBlock* predecessor) { return !inRegion(predecessor); });
        if (entered) {
            for (std::size_t incoming = 0; incoming < phi.PhiBlockCount(); ++incoming) {
                if (inRegion(phi.PhiBlock(incoming)) && !chainTo(phi.Argument(incoming), &phi, 0)) return false;
            }
            return true;
        }
        for (std::size_t base = 0; base < phi.PhiBlockCount(); ++base) {
            const IrValue* kept = phi.Argument(base)->Resolve();
            bool all = true;
            for (std::size_t incoming = 0; all && incoming < phi.PhiBlockCount(); ++incoming) all = incoming == base || chainTo(phi.Argument(incoming), kept, 0);
            if (all && stable(kept, depth + 1u)) return true;
        }
        return false;
    }

    const IrProgram& program;
    const Uniformity& uniformity;
    const std::vector<IrBlock*>& order;
    std::unordered_map<const IrBlock*, std::size_t> indices;
    std::unordered_map<std::uint32_t, std::size_t> ids;
    std::vector<Bits> dominators;
    std::vector<char> reachesEnd;
    std::vector<char> region;
    std::vector<char> skippedRegion;
    std::size_t branchIndex = 0;
    std::size_t skipIndex = 0;
    std::size_t mergeIndex = 0;
    bool cyclic = false;
    const IrValue* exec = nullptr;
    std::vector<Term> terms;
    std::unordered_set<const IrValue*> visiting;
    std::unordered_map<const IrValue*, bool> stability;
    static constexpr std::size_t NoOpenCycle = ~std::size_t{0};

    std::unordered_map<const IrValue*, const IrValue*> skippedValues;
    std::unordered_map<const IrValue*, std::size_t> passing;
    std::size_t lowestOpen = NoOpenCycle;
    std::unordered_set<const IrValue*> observed;
};

std::optional<Rejection> analyze(const IrProgram& program, const ShaderWorkgroupInputInfo& workgroup) {
    if (program.Resources().stage != IrShaderStage::Compute || program.WaveSize() != 64u) return Rejection{"not a wave64 compute program", nullptr};
    const std::array<std::uint32_t, 3> sizes{std::max(workgroup.threadsNum[0], 1u), std::max(workgroup.threadsNum[1], 1u), std::max(workgroup.threadsNum[2], 1u)};
    bool reads = false;
    bool barrier = false;
    const auto& memory = program.Resources().memoryInfo;
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            const auto opcode = inst->Opcode();
            reads |= opcode == IrOpcode::ReadFirstLane || opcode == IrOpcode::ReadLane;
            barrier |= opcode == IrOpcode::Barrier;
            if (opcode == IrOpcode::GetBuiltin) {
                const IrValue* kind = inst->Argument(0)->Resolve();
                const auto input = kind->HasImmediate() ? static_cast<StageInputKind>(kind->ImmediateU32()) : StageInputKind::Parameter;
                if (input != StageInputKind::WorkgroupId && input != StageInputKind::LocalInvocationId && input != StageInputKind::LocalInvocationIndex && input != StageInputKind::GlobalInvocationId && input != StageInputKind::DispatchThreadLimit) return Rejection{"a builtin outside compute dispatch ids", inst};
                continue;
            }
            const bool wave = opcode == IrOpcode::LaneId || opcode == IrOpcode::Ballot || opcode == IrOpcode::ReadFirstLane || opcode == IrOpcode::ReadLane;
            if (!wave && !isLaneLocal(opcode) && SharedAccessOf(opcode) == SharedAccess::None) return Rejection{"an operation across lanes", inst};
            if (opcode == IrOpcode::ReadConstBuffer || (BufferAccessOf(opcode) == BufferAccess::None && AddressOpcodeInfoOf(opcode).access == AddressAccess::None && ImageOpcodeInfoOf(opcode).access == ImageAccess::None)) continue;
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= memory.size()) continue;
            if (memory[index].gpuDescriptor) return Rejection{"a GPU-selected buffer descriptor", inst};
        }
    }
    if (auto rejected = checkLaneMasks(program)) return rejected;
    bool writes = false;
    if (auto rejected = checkLds(program, sizes, writes)) return rejected;
    if (writes && sizes[0] * sizes[1] * sizes[2] <= program.WaveSize()) return Rejection{"LDS writes in a one-wave workgroup", nullptr};
    std::optional<Uniformity> uniformity;
    const auto uniform = [&]() -> const Uniformity& {
        if (!uniformity) uniformity.emplace(program);
        return *uniformity;
    };
    if (reads) {
        for (const IrBlock* block : program.BlockOrder()) {
            for (const IrValue* inst : block->Instructions()) {
                if ((inst->Opcode() == IrOpcode::ReadFirstLane || inst->Opcode() == IrOpcode::ReadLane) && !uniform().Uniform(inst->Argument(0))) return Rejection{"a lane read of a lane-varying value", inst};
            }
        }
    }
    bool execBranches = false;
    for (const auto& info : program.Metadata().blockInfo) {
        if (info.indirectTarget != nullptr && !uniform().Uniform(info.indirectTarget)) return Rejection{"an indirect branch on a lane-varying value", info.indirectTarget};
        if (info.terminator.kind != TerminatorKind::ConditionalBranch || info.condition == nullptr) continue;
        switch (info.terminator.condition) {
        case BranchCondition::Always:
            break;
        case BranchCondition::ExecZero:
        case BranchCondition::ExecNonZero:
            execBranches = true;
            break;
        case BranchCondition::VccZero:
        case BranchCondition::VccNonZero:
            if (auto rejected = checkVccBranch(info, uniform())) return rejected;
            execBranches |= halfDecided(info, uniform());
            break;
        default:
            if (!uniform().Uniform(info.condition)) return Rejection{"a scalar branch on a lane-varying value", info.condition->Resolve()};
            break;
        }
    }
    if (barrier && execBranches) return Rejection{"a barrier in a program with exec branches", nullptr};
    const auto& metadata = program.Metadata();
    if (metadata.skippedRegionsChecked) return metadata.skippedRegionRejection.empty() ? std::nullopt : std::optional<Rejection>(Rejection{metadata.skippedRegionRejection.c_str(), nullptr});
    if (execBranches) return SkippedRegions(program, uniform()).Check();
    return std::nullopt;
}

}

SingleLaneVerdict AnalyzeSingleLane(const IrProgram& program, const ShaderWorkgroupInputInfo& workgroup) {
    const auto rejected = analyze(program, workgroup);
    if (!rejected) return {true, nullptr, nullptr};
    return {false, rejected->reason, rejected->at};
}

void RecordSkippedRegions(IrProgram& program) {
    auto& metadata = program.Metadata();
    if (metadata.skippedRegionsChecked || program.Resources().stage != IrShaderStage::Compute || program.WaveSize() != 64u) return;
    const Uniformity uniformity(program);
    const auto rejected = SkippedRegions(program, uniformity).Check();
    metadata.skippedRegionsChecked = true;
    if (rejected) metadata.skippedRegionRejection = std::string(rejected->reason) + (rejected->at == nullptr ? std::string() : " at " + DescribeValue(rejected->at, 3));
}

}
