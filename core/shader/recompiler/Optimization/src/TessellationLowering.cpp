#include "Optimization/TessellationLowering.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::int64_t LocalVertexIndexCount = 8192;
constexpr std::int64_t TriangleFactorBytes = 16;
constexpr std::int64_t QuadFactorBytes = 24;
constexpr std::int64_t AddressLimit = 0xffffffffll;

struct Affine {
    std::int64_t vertex = 0;
    std::int64_t relative = 0;
    std::int64_t constant = 0;
    bool dynamic = false;
    std::int64_t dynamicScale = 0;
    const IrValue* dynamicRoot = nullptr;

    [[nodiscard]] bool Scalar() const { return vertex == 0 && relative == 0 && !dynamic; }
    bool operator==(const Affine& other) const = default;
};

bool Same(const Affine& left, const Affine& right) {
    return !left.dynamic && !right.dynamic && left == right;
}

std::optional<Affine> CommonMultiple(const Affine& left, const Affine& right) {
    if (left.vertex != right.vertex || left.relative != right.relative || left.constant != right.constant) return std::nullopt;
    const auto multiple = std::gcd(left.dynamic ? left.dynamicScale : 0, right.dynamic ? right.dynamicScale : 0);
    if (multiple == 0) return std::nullopt;
    return Affine{left.vertex, left.relative, left.constant, true, multiple, nullptr};
}

[[noreturn]] void Fail(const char* reason, std::uint32_t pc) {
    char text[256];
    std::snprintf(text, sizeof(text), "tessellation lowering: %s at pc 0x%x", reason, pc);
    throw std::runtime_error(text);
}

bool IsImmediate(const IrValue* raw, std::uint32_t expected) {
    const IrValue* value = raw->Resolve();
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == expected;
}

bool IsBase(const IrValue* raw, TessellationBaseKind kind) {
    const IrValue* value = raw->Resolve();
    return value->Opcode() == IrOpcode::TessellationBase && IsImmediate(value->Argument(0), static_cast<std::uint32_t>(kind));
}

bool OwnPatchRead(const IrValue* value) {
    if (SharedAccessOf(value->Opcode()) == SharedAccess::Read) return true;
    return BufferAccessOf(value->Opcode()) == BufferAccess::Read && value->ArgumentCount() > 3u && IsBase(value->Argument(3), TessellationBaseKind::OffChip);
}

std::int64_t FloorDiv(std::int64_t value, std::int64_t divisor) {
    const auto quotient = value / divisor;
    return quotient * divisor > value ? quotient - 1 : quotient;
}

IrValue* VariableOperand(IrValue* value, std::uint32_t constant) {
    if (IsImmediate(value->Argument(1), constant)) return value->Argument(0)->Resolve();
    if (IsImmediate(value->Argument(0), constant)) return value->Argument(1)->Resolve();
    return nullptr;
}

bool IsLaneId(const IrValue* value) {
    return value != nullptr && value->Resolve()->Opcode() == IrOpcode::LaneId;
}

std::optional<bool> LanesBelowHalf(IrValue* raw) {
    IrValue* value = raw->Resolve();
    if (value->Opcode() != IrOpcode::BitCount32) return std::nullopt;
    IrValue* mask = value->Argument(0)->Resolve();
    if (mask->Opcode() == IrOpcode::BitwiseAnd32) {
        mask = IsImmediate(mask->Argument(0), 0xffffffffu) ? mask->Argument(1)->Resolve() : IsImmediate(mask->Argument(1), 0xffffffffu) ? mask->Argument(0)->Resolve() : nullptr;
        if (mask == nullptr) return std::nullopt;
    }
    const auto below = [](IrValue* raw) {
        IrValue* value = raw->Resolve();
        if (value->Opcode() != IrOpcode::ISub32 || !IsImmediate(value->Argument(1), 1u)) return false;
        IrValue* bit = value->Argument(0)->Resolve();
        return bit->Opcode() == IrOpcode::ShiftLeftLogical32 && IsImmediate(bit->Argument(0), 1u) && IsLaneId(VariableOperand(bit->Argument(1)->Resolve(), 31u));
    };
    if (below(mask)) return true;
    if (mask->Opcode() != IrOpcode::SelectU32) return std::nullopt;
    IrValue* high = mask->Argument(0)->Resolve();
    if (high->Opcode() != IrOpcode::UGreaterThanEqual32 || !IsLaneId(high->Argument(0)) || !IsImmediate(high->Argument(1), 32u)) return std::nullopt;
    if (IsImmediate(mask->Argument(1), 0xffffffffu) && below(mask->Argument(2))) return true;
    if (below(mask->Argument(1)) && IsImmediate(mask->Argument(2), 0u)) return false;
    return std::nullopt;
}

bool IsTrue(const IrValue* value) {
    return value != nullptr && value->HasImmediate() && value->Type() == IrType::Bool && value->ImmediateBool();
}

using LaneWords = std::pair<IrValue*, IrValue*>;

IrValue* BallotPredicate(IrValue* word, std::uint32_t half) {
    IrValue* ballot = word->Argument(0)->Resolve();
    return ballot->Opcode() == IrOpcode::Ballot && IsImmediate(word->Argument(1), half) ? ballot->Argument(0)->Resolve() : nullptr;
}

bool SamePredicate(const IrValue* lhs, const IrValue* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    if (lhs == rhs) return true;
    return lhs->HasImmediate() && rhs->HasImmediate() && lhs->Type() == IrType::Bool && rhs->Type() == IrType::Bool && lhs->ImmediateBool() == rhs->ImmediateBool();
}

std::optional<LaneWords> LaneBitWords(IrValue* value, std::uint32_t waveSize) {
    if (value->Opcode() != IrOpcode::INotEqual32) return std::nullopt;
    IrValue* masked = VariableOperand(value, 0u);
    if (masked == nullptr || masked->Opcode() != IrOpcode::BitwiseAnd32) return std::nullopt;
    IrValue* shifted = VariableOperand(masked, 1u);
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32) return std::nullopt;
    IrValue* bit = shifted->Argument(1)->Resolve();
    if (bit->Opcode() != IrOpcode::BitwiseAnd32 || !IsLaneId(VariableOperand(bit, 31u))) return std::nullopt;
    IrValue* word = shifted->Argument(0)->Resolve();
    if (waveSize != 64u) return LaneWords{word, nullptr};
    if (word->Opcode() != IrOpcode::SelectU32) return LaneWords{word, word};
    IrValue* low = word->Argument(0)->Resolve();
    if (low->Opcode() != IrOpcode::ULessThan32 || !IsLaneId(low->Argument(0)) || !IsImmediate(low->Argument(1), 32u)) return LaneWords{word, word};
    return LaneWords{word->Argument(1)->Resolve(), word->Argument(2)->Resolve()};
}

class LaneBits {
public:
    explicit LaneBits(IrProgram& program) : program(program) {}

    IrValue* Of(const LaneWords& words) {
        std::vector<LaneWords> visited;
        if (matches(words, visited)) return &build(words);
        for (const auto& pair : visited) {
            if (known.at(pair)) known.erase(pair);
        }
        return nullptr;
    }

private:
    static LaneWords argument(const LaneWords& words, std::size_t index) {
        return {words.first->Argument(index)->Resolve(), words.second == nullptr ? nullptr : words.second->Argument(index)->Resolve()};
    }

    bool matches(const LaneWords& words, std::vector<LaneWords>& visited) {
        if (const auto found = known.find(words); found != known.end()) return found->second;
        known.emplace(words, true);
        visited.push_back(words);
        const bool result = check(words, visited);
        if (!result) known[words] = false;
        return result;
    }

    bool check(const LaneWords& words, std::vector<LaneWords>& visited) {
        const auto [low, high] = words;
        if (low->HasImmediate()) {
            if (low->Type() != IrType::U32 || (low->ImmediateU32() != 0u && low->ImmediateU32() != 0xffffffffu)) return false;
            return high == nullptr || (high->HasImmediate() && high->Type() == IrType::U32 && high->ImmediateU32() == low->ImmediateU32());
        }
        if (high != nullptr && (high->HasImmediate() || high->Opcode() != low->Opcode())) return false;
        switch (low->Opcode()) {
        case IrOpcode::CompositeExtractU32x4: {
            if (BallotPredicate(low, 0u) == nullptr) return false;
            return high == nullptr || SamePredicate(BallotPredicate(low, 0u), BallotPredicate(high, 1u));
        }
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
            return matches(argument(words, 0), visited) && matches(argument(words, 1), visited);
        case IrOpcode::BitwiseNot32:
            return matches(argument(words, 0), visited);
        case IrOpcode::SelectU32:
            if (high != nullptr && high->Argument(0)->Resolve() != low->Argument(0)->Resolve()) return false;
            return matches(argument(words, 1), visited) && matches(argument(words, 2), visited);
        case IrOpcode::Phi:
            if (low->ArgumentCount() == 0 || low->ArgumentCount() != low->PhiBlockCount()) return false;
            if (high != nullptr && (high->Parent() != low->Parent() || high->ArgumentCount() != low->ArgumentCount() || high->PhiBlockCount() != low->PhiBlockCount())) return false;
            for (std::size_t index = 0; index < low->ArgumentCount(); ++index) {
                if (high != nullptr && high->PhiBlock(index) != low->PhiBlock(index)) return false;
                if (!matches(argument(words, index), visited)) return false;
            }
            return true;
        default:
            return false;
        }
    }

    IrValue& boolean(bool value) {
        IrValue& result = program.CreateValue(IrOpcode::Void, IrType::Bool);
        result.SetImmediateBool(value);
        return result;
    }

    IrValue& emit(IrValue* position, IrOpcode opcode, std::initializer_list<IrValue*> arguments) {
        IrValue& value = program.CreateValue(opcode, IrType::Bool);
        for (IrValue* argument : arguments) value.AddArgument(argument);
        position->Parent()->InsertInstructionBefore(position, &value);
        return value;
    }

    IrValue& combine(IrValue* position, IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
        for (const auto& [fixed, other] : {std::pair{&lhs, &rhs}, std::pair{&rhs, &lhs}}) {
            if (!fixed->HasImmediate()) continue;
            if (opcode == IrOpcode::LogicalAnd) return fixed->ImmediateBool() ? *other : *fixed;
            if (opcode == IrOpcode::LogicalOr) return fixed->ImmediateBool() ? *fixed : *other;
            if (!fixed->ImmediateBool()) return *other;
        }
        return emit(position, opcode, {&lhs, &rhs});
    }

    IrValue& build(const LaneWords& words) {
        IrValue* low = words.first;
        if (low->HasImmediate()) return boolean(low->ImmediateU32() != 0u);
        if (const auto found = built.find(words); found != built.end()) return *found->second;
        IrValue* result = nullptr;
        switch (low->Opcode()) {
        case IrOpcode::CompositeExtractU32x4:
            result = BallotPredicate(low, 0u);
            break;
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32: {
            const auto opcode = low->Opcode() == IrOpcode::BitwiseAnd32 ? IrOpcode::LogicalAnd : low->Opcode() == IrOpcode::BitwiseOr32 ? IrOpcode::LogicalOr : IrOpcode::LogicalXor;
            IrValue& lhs = build(argument(words, 0));
            IrValue& rhs = build(argument(words, 1));
            result = &combine(low, opcode, lhs, rhs);
            break;
        }
        case IrOpcode::BitwiseNot32: {
            IrValue& operand = build(argument(words, 0));
            result = operand.HasImmediate() ? &boolean(!operand.ImmediateBool()) : &emit(low, IrOpcode::LogicalNot, {&operand});
            break;
        }
        case IrOpcode::SelectU32: {
            IrValue& chosen = build(argument(words, 1));
            IrValue& other = build(argument(words, 2));
            result = &emit(low, IrOpcode::SelectU1, {low->Argument(0)->Resolve(), &chosen, &other});
            break;
        }
        default: {
            IrValue& phi = program.CreateValue(IrOpcode::Phi, IrType::Bool);
            low->Parent()->InsertInstructionBefore(low, &phi);
            built.emplace(words, &phi);
            for (std::size_t index = 0; index < low->ArgumentCount(); ++index) phi.AddPhiOperand(low->PhiBlock(index), &build(argument(words, index)));
            return phi;
        }
        }
        built.emplace(words, result);
        return *result;
    }

    IrProgram& program;
    std::map<LaneWords, bool> known;
    std::map<LaneWords, IrValue*> built;
};

bool SameCondition(IrValue* rawLeft, IrValue* rawRight, std::uint32_t depth = 0) {
    IrValue* left = rawLeft->Resolve();
    IrValue* right = rawRight->Resolve();
    if (left == right) return true;
    if (left->HasImmediate() || right->HasImmediate()) return SamePredicate(left, right);
    if (depth > 16u || left->Opcode() != right->Opcode()) return false;
    const auto same = [&](std::size_t lhs, std::size_t rhs) { return SameCondition(left->Argument(lhs), right->Argument(rhs), depth + 1u); };
    switch (left->Opcode()) {
    case IrOpcode::LogicalNot:
        return same(0, 0);
    case IrOpcode::LogicalAnd:
    case IrOpcode::LogicalOr:
        return (same(0, 0) && same(1, 1)) || (same(0, 1) && same(1, 0));
    default:
        return false;
    }
}

bool Implies(IrValue* rawPredicate, IrValue* rawCondition) {
    IrValue* predicate = rawPredicate->Resolve();
    IrValue* condition = rawCondition->Resolve();
    if (SameCondition(predicate, condition)) return true;
    if (IsTrue(condition)) return true;
    if (condition->Opcode() == IrOpcode::LogicalAnd && Implies(predicate, condition->Argument(0)) && Implies(predicate, condition->Argument(1))) return true;
    return predicate->Opcode() == IrOpcode::LogicalAnd && (Implies(predicate->Argument(0), condition) || Implies(predicate->Argument(1), condition));
}

bool ImpliesNot(IrValue* rawPredicate, IrValue* rawCondition) {
    IrValue* predicate = rawPredicate->Resolve();
    IrValue* condition = rawCondition->Resolve();
    if (condition->HasImmediate() && condition->Type() == IrType::Bool && !condition->ImmediateBool()) return true;
    if (condition->Opcode() == IrOpcode::LogicalNot && Implies(predicate, condition->Argument(0))) return true;
    if (predicate->Opcode() == IrOpcode::LogicalNot && Implies(condition, predicate->Argument(0))) return true;
    return predicate->Opcode() == IrOpcode::LogicalAnd && (ImpliesNot(predicate->Argument(0), condition) || ImpliesNot(predicate->Argument(1), condition));
}

bool CarriesRelativeBits(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::Identity:
    case IrOpcode::Phi:
    case IrOpcode::IAdd32:
    case IrOpcode::ISub32:
    case IrOpcode::IMul32:
    case IrOpcode::ShiftLeftLogical32:
    case IrOpcode::ShiftRightLogical32:
    case IrOpcode::ShiftRightArithmetic32:
    case IrOpcode::BitwiseAnd32:
    case IrOpcode::BitwiseOr32:
    case IrOpcode::BitwiseXor32:
    case IrOpcode::BitwiseNot32:
    case IrOpcode::BitFieldUExtract:
    case IrOpcode::BitFieldSExtract:
    case IrOpcode::BitFieldInsert:
    case IrOpcode::Select:
    case IrOpcode::SelectU1:
    case IrOpcode::SelectU32:
    case IrOpcode::SelectF32:
    case IrOpcode::BitCastU32F32:
    case IrOpcode::BitCastF32U32:
    case IrOpcode::BitCastU16F16:
    case IrOpcode::BitCastF16U16:
    case IrOpcode::ConvertF32U32:
    case IrOpcode::ConvertU32F32:
    case IrOpcode::ConvertF32S32:
    case IrOpcode::ConvertS32F32:
    case IrOpcode::ConvertFToU32:
    case IrOpcode::ConvertUToF32:
    case IrOpcode::ConvertFToI32:
    case IrOpcode::ConvertIToF32:
    case IrOpcode::ConvertF32F16:
    case IrOpcode::ConvertF16F32:
    case IrOpcode::ConvertU16U32:
    case IrOpcode::ConvertU32U16:
    case IrOpcode::ConvertU8U32:
    case IrOpcode::ConvertU32U8:
    case IrOpcode::FPAdd32:
    case IrOpcode::FPSub32:
    case IrOpcode::FPMul32:
    case IrOpcode::FPMad32:
    case IrOpcode::FPFma32:
    case IrOpcode::FFma32:
    case IrOpcode::FPMin32:
    case IrOpcode::FPMax32:
    case IrOpcode::FPNeg32:
    case IrOpcode::FPAbs32:
    case IrOpcode::CompositeConstructU64:
    case IrOpcode::CompositeConstructU32x2:
    case IrOpcode::CompositeConstructU32x3:
    case IrOpcode::CompositeConstructU32x4:
    case IrOpcode::CompositeExtractU64:
    case IrOpcode::CompositeExtractU32x2:
    case IrOpcode::CompositeExtractU32x3:
    case IrOpcode::CompositeExtractU32x4:
        return true;
    default:
        return false;
    }
}

bool IsRelativeSelect(IrOpcode opcode) {
    return opcode == IrOpcode::Select || opcode == IrOpcode::SelectU1 || opcode == IrOpcode::SelectU32 || opcode == IrOpcode::SelectF32;
}

class AddressEvaluator {
public:
    AddressEvaluator(std::uint32_t vertexCount, std::int64_t relativeCount, bool laneVertex = false) : vertexCount(vertexCount), relativeCount(relativeCount), laneVertex(laneVertex) {}

    std::optional<Affine> Evaluate(IrValue* value, IrValue* predicate, bool allowDynamic = false) {
        strictMemo.clear();
        dynamicMemo.clear();
        independence.clear();
        active = predicate;
        return evaluate(value, allowDynamic);
    }

    bool IsInvocation(IrValue* value) {
        const auto address = Evaluate(value, nullptr);
        return address && *address == Affine{1, 0, 0};
    }

    [[nodiscard]] std::int64_t Lowest(const Affine& value) const {
        return value.constant + std::min<std::int64_t>(0, value.vertex * lastVertex()) + std::min<std::int64_t>(0, value.relative * (relativeCount - 1));
    }

    [[nodiscard]] std::int64_t Highest(const Affine& value) const {
        return value.constant + std::max<std::int64_t>(0, value.vertex * lastVertex()) + std::max<std::int64_t>(0, value.relative * (relativeCount - 1));
    }

private:
    [[nodiscard]] std::int64_t lastVertex() const { return vertexCount == 0 ? 0 : static_cast<std::int64_t>(vertexCount) - 1; }

    [[nodiscard]] std::optional<Affine> bounded(const Affine& value) const {
        if (value.dynamic) return value;
        if (Lowest(value) < 0 || Highest(value) > AddressLimit) return std::nullopt;
        return value;
    }

    [[nodiscard]] std::optional<Affine> scale(const Affine& value, std::int64_t factor) const {
        if (factor < 0 || factor >= (1ll << 31) || std::abs(value.dynamicScale) * factor > AddressLimit) return std::nullopt;
        if (value.dynamic && factor == 0) return Affine{};
        return bounded({value.vertex * factor, value.relative * factor, value.constant * factor, value.dynamic, value.dynamicScale * factor, value.dynamicRoot});
    }

    [[nodiscard]] std::optional<std::pair<Affine, Affine>> split(const Affine& value, std::int64_t divisor) const {
        if (value.dynamic) return std::nullopt;
        Affine high;
        Affine low;
        (value.vertex % divisor == 0 ? high.vertex : low.vertex) = value.vertex;
        (value.relative % divisor == 0 ? high.relative : low.relative) = value.relative;
        high.constant = FloorDiv(value.constant, divisor) * divisor;
        low.constant = value.constant - high.constant;
        if (Lowest(low) < 0 || Highest(low) >= divisor) return std::nullopt;
        return std::pair{high, low};
    }

    [[nodiscard]] std::optional<Affine> shiftRight(const Affine& value, std::int64_t shift) const {
        if (shift < 0 || shift >= 32) return std::nullopt;
        const auto divisor = 1ll << shift;
        const auto parts = split(value, divisor);
        if (!parts) return std::nullopt;
        return bounded({parts->first.vertex / divisor, parts->first.relative / divisor, parts->first.constant / divisor});
    }

    [[nodiscard]] std::optional<Affine> mask(const Affine& value, std::int64_t bits) const {
        if (value.dynamic) return std::nullopt;
        if (value.Scalar()) return Affine{0, 0, value.constant & bits};
        const auto width = std::bit_width(static_cast<std::uint64_t>(bits));
        if (width >= 32) return bits == AddressLimit ? std::optional(value) : std::nullopt;
        const auto divisor = 1ll << width;
        const auto parts = split(value, divisor);
        if (!parts) return std::nullopt;
        if (parts->second.Scalar()) return Affine{0, 0, parts->second.constant & bits};
        if (bits == divisor - 1) return parts->second;
        const auto lowest = 1ll << std::countr_zero(static_cast<std::uint64_t>(bits));
        if (bits != divisor - lowest) return std::nullopt;
        const auto field = split(parts->second, lowest);
        if (!field) return std::nullopt;
        return field->first;
    }

    [[nodiscard]] std::optional<Affine> disjoint(const Affine& high, const Affine& low) const {
        if (high.dynamic || low.dynamic || Lowest(low) < 0) return std::nullopt;
        const auto divisor = 1ll << std::bit_width(static_cast<std::uint64_t>(Highest(low)));
        if (high.vertex % divisor != 0 || high.relative % divisor != 0 || high.constant % divisor != 0) return std::nullopt;
        return bounded({high.vertex + low.vertex, high.relative + low.relative, high.constant + low.constant});
    }

    bool independentOfRelative(IrValue* raw) {
        IrValue* start = raw->Resolve();
        if (const auto found = independence.find(start); found != independence.end()) return found->second;
        std::vector<IrValue*> pending{start};
        std::unordered_set<IrValue*> visited;
        bool independent = true;
        while (independent && !pending.empty()) {
            IrValue* value = pending.back();
            pending.pop_back();
            if (value == nullptr || value->HasImmediate() || !visited.insert(value).second) continue;
            if (IsBase(value, TessellationBaseKind::RelativeIndex)) {
                independent = false;
                break;
            }
            if (value != start) {
                if (const auto known = evaluate(value, false)) {
                    independent = known->relative == 0;
                    continue;
                }
            }
            if (active != nullptr && value->Opcode() == IrOpcode::SelectU32 && Implies(active, value->Argument(0))) {
                pending.push_back(value->Argument(1)->Resolve());
                continue;
            }
            if (value != start && OwnPatchRead(value)) continue;
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
                if (IrValue* argument = value->Argument(index)) pending.push_back(argument->Resolve());
            }
        }
        independence[start] = independent;
        return independent;
    }

    std::optional<Affine> evaluate(IrValue* raw, bool dynamic) {
        IrValue* value = raw->Resolve();
        if (value->HasImmediate()) {
            if (value->Type() != IrType::U32) return std::nullopt;
            return Affine{0, 0, value->ImmediateU32()};
        }
        auto& memo = dynamic ? dynamicMemo : strictMemo;
        if (const auto found = memo.find(value); found != memo.end()) return found->second;
        memo.emplace(value, std::nullopt);
        auto result = compute(value, dynamic);
        if (!result && dynamic && independentOfRelative(value)) result = Affine{0, 0, 0, true, 1, value};
        memo[value] = result;
        return result;
    }

    std::optional<Affine> compute(IrValue* value, bool dynamic) {
        const auto argument = [&](std::size_t index) { return evaluate(value->Argument(index), dynamic); };
        const auto scalar = [&](std::size_t index) -> std::optional<std::int64_t> {
            const auto operand = argument(index);
            if (!operand || !operand->Scalar()) return std::nullopt;
            return operand->constant;
        };
        switch (value->Opcode()) {
        case IrOpcode::TessellationBase:
            return IsBase(value, TessellationBaseKind::RelativeIndex) ? std::optional(Affine{0, 1, 0}) : std::nullopt;
        case IrOpcode::GetBuiltin:
            return vertexCount != 0 && !laneVertex && IsImmediate(value->Argument(0), static_cast<std::uint32_t>(StageInputKind::InvocationId)) ? std::optional(Affine{1, 0, 0}) : std::nullopt;
        case IrOpcode::LaneId:
            return laneVertex ? std::optional(Affine{1, 0, 0}) : std::nullopt;
        case IrOpcode::BitCount32: {
            if (!laneVertex) return std::nullopt;
            const auto low = LanesBelowHalf(value);
            if (!low) return std::nullopt;
            return *low ? Affine{1, 0, 0} : Affine{};
        }
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32: {
            const auto lhs = argument(0);
            const auto rhs = argument(1);
            if (!lhs || !rhs) return std::nullopt;
            const std::int64_t sign = value->Opcode() == IrOpcode::IAdd32 ? 1 : -1;
            Affine sum{lhs->vertex + sign * rhs->vertex, lhs->relative + sign * rhs->relative, lhs->constant + sign * rhs->constant, lhs->dynamic || rhs->dynamic, lhs->dynamicScale, lhs->dynamicRoot};
            if (rhs->dynamic && !lhs->dynamic) {
                sum.dynamicScale = sign * rhs->dynamicScale;
                sum.dynamicRoot = rhs->dynamicRoot;
            } else if (rhs->dynamic && lhs->dynamicRoot != nullptr && lhs->dynamicRoot == rhs->dynamicRoot) {
                sum.dynamicScale = lhs->dynamicScale + sign * rhs->dynamicScale;
                if (sum.dynamicScale == 0) {
                    sum.dynamic = false;
                    sum.dynamicRoot = nullptr;
                }
            } else if (rhs->dynamic) {
                sum.dynamicScale = std::gcd(lhs->dynamicScale, rhs->dynamicScale);
                sum.dynamicRoot = nullptr;
            }
            return bounded(sum);
        }
        case IrOpcode::IMul32: {
            const auto lhs = argument(0);
            const auto rhs = argument(1);
            if (!lhs || !rhs) return std::nullopt;
            if (rhs->Scalar()) return scale(*lhs, rhs->constant);
            if (lhs->Scalar()) return scale(*rhs, lhs->constant);
            return std::nullopt;
        }
        case IrOpcode::ShiftLeftLogical32: {
            const auto lhs = argument(0);
            const auto shift = scalar(1);
            if (!lhs || !shift || *shift < 0 || *shift >= 31) return std::nullopt;
            return scale(*lhs, 1ll << *shift);
        }
        case IrOpcode::ShiftRightLogical32: {
            const auto lhs = argument(0);
            const auto shift = scalar(1);
            if (!lhs || !shift) return std::nullopt;
            return shiftRight(*lhs, *shift);
        }
        case IrOpcode::BitwiseAnd32: {
            const auto lhs = argument(0);
            const auto rhs = argument(1);
            if (!lhs || !rhs) return std::nullopt;
            if (rhs->Scalar()) return mask(*lhs, rhs->constant);
            if (lhs->Scalar()) return mask(*rhs, lhs->constant);
            return std::nullopt;
        }
        case IrOpcode::BitwiseOr32: {
            const auto lhs = argument(0);
            const auto rhs = argument(1);
            if (!lhs || !rhs) return std::nullopt;
            if (const auto result = disjoint(*lhs, *rhs)) return result;
            return disjoint(*rhs, *lhs);
        }
        case IrOpcode::BitFieldUExtract: {
            const auto source = argument(0);
            const auto offset = scalar(1);
            const auto count = scalar(2);
            if (!source || !offset || !count || *count < 0) return std::nullopt;
            if (*count == 0) return Affine{};
            const auto shifted = shiftRight(*source, *offset);
            if (!shifted || *count >= 32) return shifted;
            return mask(*shifted, (1ll << *count) - 1);
        }
        case IrOpcode::SelectU32: {
            if (active != nullptr && Implies(active, value->Argument(0))) return argument(1);
            if (active != nullptr && ImpliesNot(active, value->Argument(0))) return argument(2);
            const auto lhs = argument(1);
            const auto rhs = argument(2);
            if (!lhs || !rhs) return std::nullopt;
            if (Same(*lhs, *rhs)) return lhs;
            return dynamic ? CommonMultiple(*lhs, *rhs) : std::nullopt;
        }
        default:
            break;
        }
        if (!value->IsPhi() || value->ArgumentCount() == 0) return std::nullopt;
        auto result = argument(0);
        for (std::size_t index = 1; result && index < value->ArgumentCount(); ++index) {
            const auto other = argument(index);
            if (!other) return std::nullopt;
            if (Same(*result, *other)) continue;
            result = dynamic ? CommonMultiple(*result, *other) : std::nullopt;
        }
        return result && (dynamic || !result->dynamic) ? result : std::nullopt;
    }

    std::uint32_t vertexCount;
    std::int64_t relativeCount;
    bool laneVertex;
    IrValue* active = nullptr;
    std::unordered_map<IrValue*, std::optional<Affine>> strictMemo;
    std::unordered_map<IrValue*, std::optional<Affine>> dynamicMemo;
    std::unordered_map<IrValue*, bool> independence;
};

bool FirstInvocationOnly(IrValue* raw, AddressEvaluator& evaluator, std::unordered_set<IrValue*>& path) {
    IrValue* value = raw->Resolve();
    if (value->HasImmediate()) return value->Type() == IrType::Bool && !value->ImmediateBool();
    if (!path.insert(value).second) return true;
    const auto invocation = [&](std::size_t index) { return evaluator.IsInvocation(value->Argument(index)); };
    const auto immediate = [&](std::size_t index, std::uint32_t expected) { return IsImmediate(value->Argument(index), expected); };
    const bool result = [&] {
        switch (value->Opcode()) {
        case IrOpcode::LogicalAnd:
            return FirstInvocationOnly(value->Argument(0), evaluator, path) || FirstInvocationOnly(value->Argument(1), evaluator, path);
        case IrOpcode::ULessThan32:
            return invocation(0) && immediate(1, 1u);
        case IrOpcode::UGreaterThan32:
            return immediate(0, 1u) && invocation(1);
        case IrOpcode::ULessThanEqual32:
            return invocation(0) && immediate(1, 0u);
        case IrOpcode::UGreaterThanEqual32:
            return immediate(0, 0u) && invocation(1);
        case IrOpcode::IEqual32:
            return (invocation(0) && immediate(1, 0u)) || (immediate(0, 0u) && invocation(1));
        default:
            break;
        }
        if (!value->IsPhi() || value->ArgumentCount() == 0) return false;
        for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
            if (!FirstInvocationOnly(value->Argument(index), evaluator, path)) return false;
        }
        return true;
    }();
    path.erase(value);
    return result;
}

std::optional<std::uint32_t> InvocationValue(const IrValue* raw, std::uint32_t invocation, std::unordered_map<const IrValue*, std::optional<std::uint32_t>>& memo) {
    const IrValue* value = raw->Resolve();
    if (value->HasImmediate()) {
        if (value->Type() == IrType::Bool) return value->ImmediateBool() ? 1u : 0u;
        if (value->Type() == IrType::U32) return value->ImmediateU32();
        return std::nullopt;
    }
    if (const auto found = memo.find(value); found != memo.end()) return found->second;
    memo[value] = std::nullopt;
    const auto argument = [&](std::size_t index) { return InvocationValue(value->Argument(index), invocation, memo); };
    const auto binary = [&](auto operation) -> std::optional<std::uint32_t> {
        const auto lhs = argument(0);
        const auto rhs = argument(1);
        if (!lhs || !rhs) return std::nullopt;
        return operation(*lhs, *rhs);
    };
    const auto result = [&]() -> std::optional<std::uint32_t> {
        switch (value->Opcode()) {
        case IrOpcode::LaneId:
            return invocation;
        case IrOpcode::GetBuiltin:
            if (IsImmediate(value->Argument(0), static_cast<std::uint32_t>(StageInputKind::InvocationId))) return invocation;
            return std::nullopt;
        case IrOpcode::LogicalNot: {
            const auto operand = argument(0);
            if (!operand) return std::nullopt;
            return *operand == 0u ? 1u : 0u;
        }
        case IrOpcode::BitCount32: {
            const auto operand = argument(0);
            if (!operand) return std::nullopt;
            return static_cast<std::uint32_t>(std::popcount(*operand));
        }
        case IrOpcode::SelectU32:
        case IrOpcode::SelectU1: {
            const auto condition = argument(0);
            if (!condition) return std::nullopt;
            return argument(*condition != 0u ? 1u : 2u);
        }
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::LogicalAnd:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs & rhs; });
        case IrOpcode::BitwiseOr32:
        case IrOpcode::LogicalOr:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs | rhs; });
        case IrOpcode::BitwiseXor32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs ^ rhs; });
        case IrOpcode::ShiftLeftLogical32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) -> std::optional<std::uint32_t> { return rhs < 32u ? std::optional(lhs << rhs) : std::nullopt; });
        case IrOpcode::ShiftRightLogical32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) -> std::optional<std::uint32_t> { return rhs < 32u ? std::optional(lhs >> rhs) : std::nullopt; });
        case IrOpcode::IAdd32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs + rhs; });
        case IrOpcode::ISub32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs - rhs; });
        case IrOpcode::IMul32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs * rhs; });
        case IrOpcode::ULessThan32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs < rhs ? 1u : 0u; });
        case IrOpcode::ULessThanEqual32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs <= rhs ? 1u : 0u; });
        case IrOpcode::UGreaterThan32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs > rhs ? 1u : 0u; });
        case IrOpcode::UGreaterThanEqual32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs >= rhs ? 1u : 0u; });
        case IrOpcode::IEqual32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs == rhs ? 1u : 0u; });
        case IrOpcode::INotEqual32:
            return binary([](std::uint32_t lhs, std::uint32_t rhs) { return lhs != rhs ? 1u : 0u; });
        default:
            return std::nullopt;
        }
    }();
    memo[value] = result;
    return result;
}

struct Access {
    IrBlock* block = nullptr;
    IrValue* inst = nullptr;
    TessellationAttribute kind = TessellationAttribute::LocalOutput;
    bool write = false;
    std::uint32_t components = 1;
    bool half = false;
    Affine address;
    IrValue* predicate = nullptr;
    std::uint32_t pc = 0;
    IrValue* addressValue = nullptr;
    std::uint32_t memoryOffset = 0;
};

std::uint32_t ComponentCount(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::LoadSharedU32x2:
    case IrOpcode::WriteSharedU32x2:
    case IrOpcode::LoadBufferU32x2:
    case IrOpcode::StoreBufferU32x2:
        return 2u;
    case IrOpcode::LoadSharedU32x3:
    case IrOpcode::WriteSharedU32x3:
    case IrOpcode::LoadBufferU32x3:
    case IrOpcode::StoreBufferU32x3:
        return 3u;
    case IrOpcode::LoadSharedU32x4:
    case IrOpcode::WriteSharedU32x4:
    case IrOpcode::LoadBufferU32x4:
    case IrOpcode::StoreBufferU32x4:
        return 4u;
    case IrOpcode::LoadSharedU32:
    case IrOpcode::WriteSharedU32:
    case IrOpcode::LoadBufferU32:
    case IrOpcode::StoreBufferU32:
        return 1u;
    default:
        return 0u;
    }
}

class Lowering {
public:
    Lowering(IrProgram& program, const ShaderTessellationInputInfo& tessellation, const TessellationTargetLimits* limits)
        : program(program), tessellation(tessellation), limits(limits), stage(program.Resources().stage), merged(stage == IrShaderStage::TessellationControl && tessellation.mergedHull),
          capture(stage == IrShaderStage::TessellationEvaluation && tessellation.geometryItemDwords != 0u),
          evaluator(stage == IrShaderStage::TessellationControl ? tessellation.outputControlPoints : capture ? 1u : 0u, stage == IrShaderStage::Local ? LocalVertexIndexCount : static_cast<std::int64_t>(tessellation.patchesPerGroup), capture), localStride(program.Metadata().tessellationLocalStride) {}

    void Run() {
        if (merged) {
            if (tessellation.inputControlPoints > tessellation.outputControlPoints) Fail("a merged hull program with more input than output control points runs its local part on lanes the hull stage does not have", 0u);
            requireUniformBarriers();
            requireOwnLanes();
        }
        if (stage == IrShaderStage::TessellationControl || capture) lowerOwnLaneBits();
        collect();
        derive();
        orderControlOutputReads();
        for (const auto& access : accesses) rewrite(access);
        if (stage == IrShaderStage::TessellationEvaluation) removePrimitiveExports();
        constexpr DeadCodeEliminator deadCodeEliminator;
        deadCodeEliminator.Eliminate(program);
        if (stage == IrShaderStage::TessellationControl) removeUnobservedCycles();
        std::vector<IrValue*> relative;
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (stage == IrShaderStage::TessellationControl && inst->Opcode() == IrOpcode::Ballot && inst->HasUses()) Fail("the hull program uses a lane mask other than its own lane's bit", 0u);
                if (inst->Opcode() != IrOpcode::TessellationBase || !inst->HasUses()) continue;
                if (IsBase(inst, TessellationBaseKind::RelativeIndex)) {
                    relative.push_back(inst);
                    continue;
                }
                if (IsBase(inst, TessellationBaseKind::PassthroughPrimitive)) Fail("the primitive VGPR is used other than as the passthrough primitive export", 0u);
                Fail("a tessellation ring base is used outside a lowered ring access", 0u);
            }
        }
        const bool data = usedAsData(relative);
        if (data) requireGroupedPatches();
        if (data) keepComputedReadsInPatch(*relative.front());
        for (IrValue* base : relative) base->ReplaceAllUsesWith(data ? &groupedPatch(*base) : &constant(0u));
        deadCodeEliminator.Eliminate(program);
        if (stage == IrShaderStage::Local && tessellation.drawParameters) writeInstance();
        program.Metadata().tessellationLocalStride = localStride;
        program.Metadata().tessellationControlStride = controlStride;
        program.Metadata().tessellationPatchStride = patchStride;
        program.Metadata().tessellationSharedStride = sharedStride;
        if (merged) requireMergedLimits();
    }

private:
    void lowerOwnLaneBits() {
        LaneBits laneBits(program);
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                const auto words = LaneBitWords(inst, program.WaveSize());
                if (!words) continue;
                if (IrValue* bit = laneBits.Of(*words)) inst->ReplaceUsesWith(bit, true);
            }
        }
        ResolveControlFlowIdentities(program);
        constexpr DeadCodeEliminator identities;
        identities.RemoveIdentities(program);
    }

    void collect() {
        const auto& memoryInfo = program.Resources().memoryInfo;
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                const auto opcode = inst->Opcode();
                const auto shared = SharedAccessOf(opcode);
                const auto buffer = BufferAccessOf(opcode);
                if (shared == SharedAccess::None && buffer == BufferAccess::None) continue;
                const auto flags = inst->Flags<MemoryFlags>();
                const auto& memory = memoryInfo.at(flags.index);
                const bool half = opcode == IrOpcode::LoadSharedU16 || opcode == IrOpcode::WriteSharedU16;
                Access access{block, inst, TessellationAttribute::LocalOutput, false, half ? 1u : ComponentCount(opcode), half, {}, inst->Argument(inst->ArgumentCount() - 1u), flags.pc};
                IrValue* address = nullptr;
                if (shared != SharedAccess::None) {
                    if (memory.kind != ResourceKind::Lds) continue;
                    access.write = shared == SharedAccess::Write;
                    if (access.components == 0u || (shared != SharedAccess::Read && shared != SharedAccess::Write)) Fail("only 16-bit and 32-bit LDS reads and writes are supported in tessellation stages", flags.pc);
                    if (stage == IrShaderStage::Local && access.write) {
                        access.kind = TessellationAttribute::LocalOutput;
                    } else if (merged) {
                        access.kind = TessellationAttribute::ControlShared;
                    } else if (capture && access.write) {
                        access.kind = TessellationAttribute::GeometryOutput;
                    } else if (stage == IrShaderStage::TessellationControl && !access.write) {
                        access.kind = TessellationAttribute::ControlInput;
                    } else {
                        Fail("unsupported LDS access in a tessellation stage", flags.pc);
                    }
                    address = inst->Argument(0);
                } else {
                    if (memory.kind != ResourceKind::Buffer || (!IsBase(inst->Argument(3), TessellationBaseKind::OffChip) && !IsBase(inst->Argument(3), TessellationBaseKind::Factor))) continue;
                    if (buffer == BufferAccess::Atomic || access.components == 0u || memory.dataBits != 32u || memory.formatted || memory.typed || memory.d16 || memory.idxen) Fail("only untyped 32-bit tessellation ring accesses are supported", flags.pc);
                    access.write = buffer == BufferAccess::Write;
                    const bool factor = IsBase(inst->Argument(3), TessellationBaseKind::Factor);
                    if (stage == IrShaderStage::TessellationControl && access.write) {
                        access.kind = factor ? TessellationAttribute::Factor : TessellationAttribute::ControlOutput;
                    } else if (stage == IrShaderStage::TessellationControl && !factor) {
                        access.kind = TessellationAttribute::ControlOutput;
                    } else if (stage == IrShaderStage::TessellationEvaluation && !access.write && !factor) {
                        access.kind = TessellationAttribute::EvaluationInput;
                    } else {
                        Fail("unsupported tessellation ring access", flags.pc);
                    }
                    address = memory.offen ? inst->Argument(2) : nullptr;
                }
                if (address != nullptr) {
                    const auto evaluated = evaluator.Evaluate(address, access.predicate, access.kind == TessellationAttribute::ControlInput || access.kind == TessellationAttribute::EvaluationInput);
                    if (!evaluated) Fail("the tessellation ring or LDS address is not affine in the control point and relative patch", flags.pc);
                    access.address = *evaluated;
                    access.addressValue = address;
                }
                access.memoryOffset = memory.offset;
                access.address.constant += memory.offset;
                if ((access.address.constant & (access.half ? 1 : 3)) != 0 || (access.half && access.kind != TessellationAttribute::LocalOutput && access.kind != TessellationAttribute::ControlInput && access.kind != TessellationAttribute::ControlShared)) Fail("unaligned tessellation ring or LDS address", flags.pc);
                accesses.push_back(access);
            }
        }
    }

    void agree(std::uint32_t& stride, std::int64_t value, std::uint32_t pc) const {
        if (value <= 0 || (value & 3) != 0 || value > 0xffff || (stride != 0u && stride != value)) Fail("inconsistent tessellation stride", pc);
        stride = static_cast<std::uint32_t>(value);
    }

    void derive() {
        const auto inputs = static_cast<std::int64_t>(tessellation.inputControlPoints);
        const auto outputs = static_cast<std::int64_t>(tessellation.outputControlPoints);
        const auto soleControlPoint = [&](const Access& access) {
            const auto end = access.address.constant + (access.half ? 2 : 4 * static_cast<std::int64_t>(access.components));
            return outputs == 1 && access.address.vertex == 0 && access.address.relative > 0 && access.address.constant >= 0 && end <= access.address.relative;
        };
        for (auto& access : accesses) {
            if (access.kind == TessellationAttribute::LocalOutput) agree(localStride, access.address.relative, access.pc);
            if (access.kind == TessellationAttribute::ControlInput && access.address.vertex != 0) agree(localStride, access.address.vertex, access.pc);
            if (access.kind == TessellationAttribute::ControlInput && (access.address.dynamic || access.address.vertex == 0) && access.address.relative > 0 && access.address.relative % inputs == 0) agree(localStride, access.address.relative / inputs, access.pc);
            if (access.kind == TessellationAttribute::ControlOutput && access.write && access.address.vertex != 0) agree(controlStride, access.address.vertex, access.pc);
            if (access.kind == TessellationAttribute::ControlOutput && access.write && soleControlPoint(access)) agree(controlStride, access.address.relative, access.pc);
            if (access.kind == TessellationAttribute::ControlShared) {
                if (access.address.relative <= 0 || access.address.relative % inputs != 0) Fail("the merged hull program accesses LDS outside its patch", access.pc);
                agree(sharedStride, access.address.relative / inputs, access.pc);
            }
            if (access.kind == TessellationAttribute::EvaluationInput) {
                const auto end = access.address.constant + (access.half ? 2 : 4 * static_cast<std::int64_t>(access.components));
                if (access.address.relative > 0 && access.address.relative % outputs == 0 && access.address.constant >= 0 && end <= access.address.relative) {
                    agree(controlStride, access.address.relative / outputs, access.pc);
                } else if (access.address.dynamic) {
                    Fail("the domain shader reads off-chip memory at a computed offset outside its patch's control points", access.pc);
                } else {
                    access.kind = TessellationAttribute::PatchInput;
                }
            }
        }
        std::optional<std::int64_t> recordStride;
        std::int64_t patchLowest = AddressLimit;
        std::int64_t patchHighest = 0;
        const auto record = [&](const Access& access, std::int64_t end) {
            if (recordStride && *recordStride != access.address.relative) Fail("inconsistent patch-constant stride", access.pc);
            recordStride = access.address.relative;
            patchLowest = std::min(patchLowest, access.address.constant);
            patchHighest = std::max(patchHighest, end);
        };
        for (auto& access : accesses) {
            const auto& address = access.address;
            const auto end = address.constant + (access.half ? 2 : 4 * static_cast<std::int64_t>(access.components));
            switch (access.kind) {
            case TessellationAttribute::LocalOutput:
                if (address.vertex != 0 || address.constant < 0 || end > localStride) Fail("the local stage writes outside its vertex's LDS slot", access.pc);
                break;
            case TessellationAttribute::ControlInput:
                if (localStride == 0u) Fail("the hull shader reads no LDS per control point, so the local stride is unknown", access.pc);
                if ((address.vertex != 0 && address.vertex != localStride) || address.relative != inputs * localStride || address.constant < 0 || end + address.vertex * (outputs - 1) > inputs * localStride) Fail("the hull shader reads LDS outside its patch", access.pc);
                if (address.dynamic && (address.dynamicScale == 0 || address.dynamicScale % localStride != 0)) Fail("the hull shader reads LDS at a computed offset that is not a whole number of control points", access.pc);
                break;
            case TessellationAttribute::ControlOutput:
                if (!access.write) {
                    if (controlStride == 0u) Fail("the hull shader reads off-chip memory but writes no control point there, so the control point stride is unknown", access.pc);
                    if ((address.vertex != 0 && address.vertex != controlStride) || address.relative != outputs * controlStride || address.constant < 0 || end + address.vertex * (outputs - 1) > outputs * controlStride) Fail("the hull shader reads off-chip memory outside its patch's control points", access.pc);
                    break;
                }
                if (address.vertex == 0 && !soleControlPoint(access)) {
                    if (controlStride == 0u || evaluator.Lowest(address) < static_cast<std::int64_t>(tessellation.patchesPerGroup) * outputs * controlStride) Fail("the hull shader writes off-chip memory outside its control points and patch constants", access.pc);
                    record(access, end);
                    access.kind = TessellationAttribute::PatchOutput;
                    requireFirstInvocation(access);
                } else if (address.relative != outputs * controlStride || address.constant < 0 || end > controlStride) {
                    Fail("the hull shader writes off-chip memory outside its control point", access.pc);
                }
                break;
            case TessellationAttribute::EvaluationInput:
                if (address.constant < 0 || end > outputs * controlStride) Fail("the domain shader reads off-chip memory outside its patch's control points", access.pc);
                if (address.dynamic && (address.dynamicScale == 0 || address.dynamicScale % controlStride != 0)) Fail("the domain shader reads off-chip memory at a computed offset that is not a whole number of control points", access.pc);
                break;
            case TessellationAttribute::Factor: {
                const bool quads = tessellation.domain == 2u;
                const auto factorBytes = quads ? QuadFactorBytes : TriangleFactorBytes;
                if ((tessellation.domain != 1u && !quads) || address.vertex != 0 || address.relative != factorBytes || address.constant < 0 || end > factorBytes) Fail(quads ? "the hull shader writes tessellation factors outside its patch's quad factors" : "the hull shader writes tessellation factors outside its patch's triangle factors", access.pc);
                requireFirstInvocation(access);
                break;
            }
            case TessellationAttribute::PatchInput:
                if (controlStride == 0u) Fail("the domain shader reads patch constants but no control point, so their locations are unknown", access.pc);
                if (address.relative == 0) Fail("the domain shader reads off-chip memory that is not indexed by its relative patch", access.pc);
                if (evaluator.Lowest(address) < static_cast<std::int64_t>(tessellation.patchesPerGroup) * outputs * controlStride) Fail("the domain shader reads off-chip memory that is neither its patch's control points nor a patch-constant record", access.pc);
                record(access, end);
                break;
            case TessellationAttribute::ControlShared:
                if (access.write) {
                    if (address.vertex != sharedStride || address.constant < 0 || end > sharedStride) Fail("the merged hull program writes LDS outside its own control point's record", access.pc);
                } else if ((address.vertex != 0 && address.vertex != sharedStride) || address.constant < 0 || end + address.vertex * (outputs - 1) > inputs * sharedStride) {
                    Fail("the merged hull program reads LDS outside its patch's control point records", access.pc);
                }
                break;
            case TessellationAttribute::PatchOutput:
                break;
            case TessellationAttribute::GeometryOutput:
                if (address.vertex != 4 * static_cast<std::int64_t>(tessellation.geometryItemDwords) || address.relative != 0 || address.constant < 0 || end > address.vertex) Fail("the domain program writes LDS outside its vertex's ESGS item", access.pc);
                break;
            default:
                break;
            }
        }
        if (!recordStride) return;
        if (*recordStride == 0 || std::abs(*recordStride) < patchHighest - patchLowest) Fail(stage == IrShaderStage::TessellationEvaluation ? "the domain shader reads more than one patch-constant record" : "patch-constant records of neighbouring patches overlap", 0u);
        patchStride = static_cast<std::uint32_t>(std::abs(*recordStride));
    }

    bool dynamicInputAddress(IrValue* user, IrValue* value) {
        if (user->Opcode() != IrOpcode::GetTessellationAttribute || user->Argument(1)->Resolve() != value->Resolve()) return false;
        const bool control = IsImmediate(user->Argument(0), static_cast<std::uint32_t>(TessellationAttribute::ControlInput));
        const bool evaluation = IsImmediate(user->Argument(0), static_cast<std::uint32_t>(TessellationAttribute::EvaluationInput));
        if (!control && !evaluation) return false;
        const auto address = evaluator.Evaluate(value, user->Argument(2), true);
        const auto stride = control ? static_cast<std::int64_t>(tessellation.inputControlPoints) * localStride : static_cast<std::int64_t>(tessellation.outputControlPoints) * controlStride;
        return address && address->dynamic && address->relative == stride;
    }

    bool dependsOnRelative(IrValue* raw, IrValue* rawPredicate) {
        if (raw == nullptr) return false;
        IrValue* value = raw->Resolve();
        IrValue* predicate = rawPredicate == nullptr ? nullptr : rawPredicate->Resolve();
        if (value->HasImmediate()) return false;
        const auto key = std::pair<const IrValue*, const IrValue*>{value, predicate};
        if (const auto found = dependence.find(key); found != dependence.end()) return found->second == Dependence::Yes;
        const auto inserted = dependence.emplace(key, Dependence::Visiting);
        if (!inserted.second) return false;
        const bool result = [&] {
            if (const auto form = evaluator.Evaluate(value, predicate)) return form->relative != 0;
            if (IsRelativeSelect(value->Opcode()) && predicate != nullptr) {
                IrValue* condition = value->Argument(0);
                if (Implies(predicate, condition)) return dependsOnRelative(value->Argument(1), predicate);
                if (ImpliesNot(predicate, condition)) return dependsOnRelative(value->Argument(2), predicate);
            }
            for (IrValue* argument : value->Arguments()) {
                if (dependsOnRelative(argument, predicate)) return true;
            }
            return false;
        }();
        inserted.first->second = result ? Dependence::Yes : Dependence::No;
        return result;
    }

    bool windowLoadObserves(IrValue* store) {
        IrBlock* start = store->Parent();
        if (start == nullptr) return true;
        std::unordered_set<IrBlock*> seen;
        std::vector<IrBlock*> pending = start->Successors();
        bool loops = false;
        while (!pending.empty()) {
            IrBlock* block = pending.back();
            pending.pop_back();
            if (block == nullptr) continue;
            if (block == start) {
                loops = true;
                continue;
            }
            if (!seen.insert(block).second) continue;
            for (IrValue* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::LoadVectorWindow) return true;
            }
            for (IrBlock* next : block->Successors()) pending.push_back(next);
        }
        bool after = false;
        for (IrValue* inst : start->Instructions()) {
            if (inst == store) {
                after = true;
                continue;
            }
            if (inst->Opcode() == IrOpcode::LoadVectorWindow && (after || loops)) return true;
        }
        return false;
    }

    IrValue* effectPredicate(IrValue* user) const {
        if (!user->MayHaveSideEffects() || user->ArgumentCount() == 0u) return nullptr;
        IrValue* last = user->Argument(user->ArgumentCount() - 1u);
        if (last == nullptr || last->Resolve()->Type() != IrType::Bool) return nullptr;
        return last;
    }

    bool usedAsData(const std::vector<IrValue*>& bases) {
        dependence.clear();
        std::vector<IrValue*> pending(bases.begin(), bases.end());
        std::unordered_set<IrValue*> visited(bases.begin(), bases.end());
        while (!pending.empty()) {
            IrValue* value = pending.back();
            pending.pop_back();
            for (IrValue* user : value->Uses()) {
                if (user == nullptr) continue;
                if (CarriesRelativeBits(user->Opcode())) {
                    if (visited.insert(user).second) pending.push_back(user);
                    continue;
                }
                if (dynamicInputAddress(user, value)) continue;
                if (!dependsOnRelative(value, effectPredicate(user))) continue;
                if (user->Opcode() == IrOpcode::StoreVectorWindow && !windowLoadObserves(user)) continue;
                return true;
            }
        }
        return false;
    }

    void requireGroupedPatches() const {
        if (stage == IrShaderStage::Local) Fail("the relative patch or vertex index is used outside a tessellation ring or LDS address", 0u);
        if (tessellation.primitiveGroupSize == 0u) Fail(stage == IrShaderStage::TessellationControl ? "the hull program uses its relative patch index as data, but GE_CNTL.PRIM_GRP_SIZE is 0 or not programmed" : "the domain program uses its relative patch index as data, but GE_CNTL.PRIM_GRP_SIZE is 0 or not programmed", 0u);
        if (tessellation.instancePatches == 0u || restartsPerInstance()) return;
        if (stage == IrShaderStage::TessellationEvaluation) Fail("the domain program uses its relative patch index as data in an instanced draw whose instances share hull threadgroups (GE_CNTL.BREAK_WAVE_AT_EOI clear)", 0u);
        if (!tessellation.drawParameters) Fail("the hull program uses its relative patch index as data in an instanced draw whose instances share hull threadgroups, which needs the device's shaderDrawParameters", 0u);
    }

    IrValue& groupedPatch(IrValue& base) {
        const auto emit = [&](IrOpcode opcode, std::initializer_list<IrValue*> arguments) -> IrValue& {
            IrValue& value = program.CreateValue(opcode, IrType::U32);
            for (IrValue* argument : arguments) value.AddArgument(argument);
            base.Parent()->InsertInstructionBefore(&base, &value);
            return value;
        };
        const auto remainder = [&](IrValue& value, std::uint32_t divisor) -> IrValue& {
            IrValue& count = constant(divisor);
            return emit(IrOpcode::ISub32, {&value, &emit(IrOpcode::IMul32, {&emit(IrOpcode::UDiv32, {&value, &count}), &count})});
        };
        IrValue* primitive = &emit(IrOpcode::GetBuiltin, {&constant(static_cast<std::uint32_t>(StageInputKind::PrimitiveId)), &constant(0u)});
        if (tessellation.instancePatches != 0u && !restartsPerInstance()) {
            IrValue& always = program.CreateValue(IrOpcode::Void, IrType::Bool);
            always.SetImmediateBool(true);
            IrValue& instance = emit(IrOpcode::GetTessellationAttribute, {&constant(static_cast<std::uint32_t>(TessellationAttribute::InstanceInput)), &constant(0u), &always});
            primitive = &emit(IrOpcode::IAdd32, {&emit(IrOpcode::IMul32, {&instance, &constant(tessellation.instancePatches)}), primitive});
        }
        return remainder(remainder(*primitive, tessellation.primitiveGroupSize), tessellation.patchesPerGroup);
    }

    void removeUnobservedCycles() {
        std::unordered_set<IrValue*> observed;
        std::vector<IrValue*> pending;
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (inst->MayHaveSideEffects() && observed.insert(inst).second) pending.push_back(inst);
            }
        }
        while (!pending.empty()) {
            IrValue* value = pending.back();
            pending.pop_back();
            for (IrValue* argument : value->Arguments()) {
                if (argument != nullptr && observed.insert(argument).second) pending.push_back(argument);
            }
        }
        std::vector<IrValue*> unobserved;
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (!observed.contains(inst)) unobserved.push_back(inst);
            }
        }
        for (IrValue* inst : unobserved) {
            for (std::size_t index = 0; index < inst->ArgumentCount(); ++index) inst->ReplaceArgument(index, nullptr);
        }
        for (IrValue* inst : unobserved) {
            inst->Parent()->RemoveInstruction(inst);
            inst->Invalidate();
        }
    }

    void requireUniformBarriers() const {
        const auto& blocks = program.BlockOrder();
        for (IrBlock* block : blocks) {
            if (std::none_of(block->Instructions().begin(), block->Instructions().end(), [](const IrValue* inst) { return inst->Opcode() == IrOpcode::Barrier; })) continue;
            std::unordered_set<IrBlock*> seen{block};
            std::vector<IrBlock*> pending;
            if (blocks.front() != block) pending.push_back(blocks.front());
            while (!pending.empty()) {
                IrBlock* current = pending.back();
                pending.pop_back();
                if (!seen.insert(current).second) continue;
                if (current->Successors().empty()) Fail("the merged hull program has an s_barrier that some invocations do not reach", 0u);
                pending.insert(pending.end(), current->Successors().begin(), current->Successors().end());
            }
            seen = {};
            pending.assign(block->Successors().begin(), block->Successors().end());
            while (!pending.empty()) {
                IrBlock* current = pending.back();
                pending.pop_back();
                if (current == block) Fail("the merged hull program has an s_barrier inside a loop", 0u);
                if (!seen.insert(current).second) continue;
                pending.insert(pending.end(), current->Successors().begin(), current->Successors().end());
            }
        }
    }

    using Edges = std::unordered_map<const IrBlock*, std::vector<IrBlock*>>;

    std::optional<bool> uniformCondition(const IrValue* condition) const {
        std::optional<bool> result;
        for (std::uint32_t invocation = 0; invocation < tessellation.outputControlPoints; ++invocation) {
            std::unordered_map<const IrValue*, std::optional<std::uint32_t>> memo;
            const auto value = InvocationValue(condition, invocation, memo);
            if (!value || (result && *result != (*value != 0u))) return std::nullopt;
            result = *value != 0u;
        }
        return result;
    }

    Edges takenEdges() const {
        const auto& order = program.BlockOrder();
        const auto& blockInfo = program.Metadata().blockInfo;
        std::unordered_map<std::uint32_t, IrBlock*> byId;
        if (order.size() == blockInfo.size()) {
            for (std::size_t index = 0; index < order.size(); ++index) byId.emplace(blockInfo[index].id, order[index]);
        }
        Edges edges;
        for (std::size_t index = 0; index < order.size(); ++index) {
            auto& successors = edges[order[index]];
            successors.assign(order[index]->Successors().begin(), order[index]->Successors().end());
            if (byId.empty()) continue;
            const auto& info = blockInfo[index];
            if (info.terminator.kind != TerminatorKind::ConditionalBranch || info.condition == nullptr || info.condition->Resolve()->Type() != IrType::U1) continue;
            const auto uniform = uniformCondition(info.condition);
            if (!uniform) continue;
            const auto taken = byId.find(*uniform ? info.terminator.trueBlock : info.terminator.falseBlock);
            if (taken == byId.end() || std::find(successors.begin(), successors.end(), taken->second) == successors.end()) continue;
            successors.assign(1u, taken->second);
        }
        return edges;
    }

    static std::unordered_set<const IrBlock*> reachedFrom(const Edges& edges, std::vector<IrBlock*> pending, const IrBlock* avoided) {
        std::unordered_set<const IrBlock*> seen;
        while (!pending.empty()) {
            IrBlock* current = pending.back();
            pending.pop_back();
            if (current == avoided || !seen.insert(current).second) continue;
            const auto& next = edges.at(current);
            pending.insert(pending.end(), next.begin(), next.end());
        }
        return seen;
    }

    void orderControlOutputReads() {
        if (tessellation.outputControlPoints == 1u) return;
        std::unordered_set<const IrValue*> reads;
        std::unordered_set<const IrBlock*> readBlocks;
        std::uint32_t readPc = 0;
        for (const auto& access : accesses) {
            if (access.kind != TessellationAttribute::ControlOutput || access.write || access.address.vertex != 0) continue;
            if (reads.empty()) readPc = access.pc;
            reads.insert(access.inst);
            readBlocks.insert(access.block);
        }
        if (reads.empty()) return;
        const auto edges = takenEdges();
        IrBlock* entry = program.BlockOrder().front();
        const auto live = reachedFrom(edges, {entry}, nullptr);
        if (std::any_of(readBlocks.begin(), readBlocks.end(), [&](const IrBlock* block) { return !live.contains(block); })) Fail("the hull shader reads another control point's off-chip output in a block no control point reaches", readPc);
        IrBlock* barrier = nullptr;
        for (IrBlock* candidate : program.BlockOrder()) {
            if (!live.contains(candidate)) continue;
            const auto before = candidate == entry ? std::unordered_set<const IrBlock*>{} : reachedFrom(edges, {entry}, candidate);
            const bool everyPath = std::none_of(before.begin(), before.end(), [&](const IrBlock* block) { return edges.at(block).empty(); });
            const bool dominatesReads = std::none_of(readBlocks.begin(), readBlocks.end(), [&](const IrBlock* block) { return before.contains(block); });
            if (!everyPath || !dominatesReads || reachedFrom(edges, edges.at(candidate), nullptr).contains(candidate)) continue;
            if (barrier == nullptr || reachedFrom(edges, edges.at(barrier), nullptr).contains(candidate)) barrier = candidate;
        }
        if (barrier == nullptr) Fail("the hull shader reads another control point's off-chip output, but no block before the read is reached once by every invocation, so no barrier can order it after the writes", readPc);
        const auto after = reachedFrom(edges, edges.at(barrier), nullptr);
        IrValue* firstRead = nullptr;
        for (IrValue* inst : barrier->Instructions()) {
            if (reads.contains(inst)) {
                firstRead = inst;
                break;
            }
        }
        for (const auto& access : accesses) {
            if (access.kind != TessellationAttribute::ControlOutput || !access.write || !live.contains(access.block)) continue;
            bool late = after.contains(access.block);
            if (access.block == barrier && firstRead != nullptr) {
                const auto& instructions = barrier->Instructions();
                late = std::find(std::find(instructions.begin(), instructions.end(), firstRead), instructions.end(), access.inst) != instructions.end();
            }
            if (late) Fail("the hull shader reads another control point's off-chip output, but one of its writes there comes after the last block every invocation reaches before the read", access.pc);
        }
        IrValue* position = firstRead;
        if (position == nullptr) {
            const auto& instructions = barrier->Instructions();
            const auto branch = std::find_if(instructions.begin(), instructions.end(), [](const IrValue* inst) { return inst->Opcode() == IrOpcode::Branch || inst->Opcode() == IrOpcode::BranchConditional; });
            if (branch != instructions.end()) position = *branch;
        }
        IrValue& value = program.CreateValue(IrOpcode::Barrier, IrType::Void);
        if (position != nullptr) {
            barrier->InsertInstructionBefore(position, &value);
        } else {
            barrier->AppendInstruction(&value);
        }
    }

    void requireOwnLanes() const {
        for (IrBlock* block : program.BlockOrder()) {
            for (const IrValue* inst : block->Instructions()) {
                switch (inst->Opcode()) {
                case IrOpcode::ReadLane:
                case IrOpcode::WriteLane:
                case IrOpcode::DppMoveU32:
                case IrOpcode::DppUpdateU32:
                case IrOpcode::Permlane16U32:
                case IrOpcode::SwizzleU32:
                case IrOpcode::PermuteU32:
                case IrOpcode::BpermuteU32:
                    Fail("the merged hull program reads or writes another lane's register, which its hull stage does not hold at one lane per invocation", 0u);
                default:
                    break;
                }
            }
        }
    }

    void requireMergedLimits() const {
        if (limits == nullptr) Fail("the target has no tessellation limits for a merged hull program", 0u);
        const auto vectors = [](std::uint32_t bytes) { return (bytes + 15u) / 16u; };
        const auto patch = vectors(patchStride);
        const auto perVertex = vectors(controlStride) + vectors(sharedStride);
        const auto locations = vectors(controlStride) + patch + vectors(sharedStride);
        const auto total = 4ull * (static_cast<std::uint64_t>(tessellation.outputControlPoints) * perVertex + patch);
        if (4u * locations > limits->maxControlPerVertexOutputComponents || 4u * patch > limits->maxControlPerPatchOutputComponents || total > limits->maxControlTotalOutputComponents || 4u > limits->maxControlPerVertexInputComponents) {
            char text[256];
            std::snprintf(text, sizeof(text), "the merged hull program's outputs exceed the device limits (%u per-vertex locations, %u patch vectors, %llu components)", locations, patch, static_cast<unsigned long long>(total));
            Fail(text, 0u);
        }
    }

    void requireFirstInvocation(const Access& access) {
        if (tessellation.outputControlPoints == 1u) return;
        std::unordered_set<IrValue*> path;
        if (!FirstInvocationOnly(access.predicate, evaluator, path)) Fail("per-patch tessellation output is not written by control point 0 alone", access.pc);
    }

    IrValue& constant(std::uint32_t value) {
        IrValue& result = program.CreateValue(IrOpcode::Void, IrType::U32);
        result.SetImmediateU32(value);
        return result;
    }

    [[nodiscard]] bool restartsPerInstance() const {
        const auto patches = tessellation.instancePatches;
        const auto group = tessellation.primitiveGroupSize;
        return group != 0u && (patches % group == 0u || (group % tessellation.patchesPerGroup == 0u && patches % tessellation.patchesPerGroup == 0u));
    }

    void writeInstance() {
        IrBlock* entry = program.BlockOrder().front();
        IrValue* first = entry->Instructions().empty() ? nullptr : entry->Instructions().front();
        const auto place = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) -> IrValue& {
            IrValue& value = program.CreateValue(opcode, type);
            for (IrValue* argument : arguments) value.AddArgument(argument);
            if (first != nullptr) {
                entry->InsertInstructionBefore(first, &value);
            } else {
                entry->AppendInstruction(&value);
            }
            return value;
        };
        IrValue& index = place(IrOpcode::GetBuiltin, IrType::U32, {&constant(static_cast<std::uint32_t>(StageInputKind::InstanceIndex)), &constant(0u)});
        IrValue& base = place(IrOpcode::GetBuiltin, IrType::U32, {&constant(static_cast<std::uint32_t>(StageInputKind::BaseInstance)), &constant(0u)});
        IrValue& instance = place(IrOpcode::ISub32, IrType::U32, {&index, &base});
        IrValue& truth = program.CreateValue(IrOpcode::Void, IrType::Bool);
        truth.SetImmediateBool(true);
        static_cast<void>(place(IrOpcode::SetTessellationAttribute, IrType::Void, {&constant(static_cast<std::uint32_t>(TessellationAttribute::InstanceOutput)), &constant(0u), &instance, &truth}));
    }

    static IrValue* activeValue(IrValue* raw, IrValue* predicate) {
        IrValue* value = raw->Resolve();
        for (std::uint32_t depth = 0; depth < 16u && value->Opcode() == IrOpcode::SelectU32; ++depth) {
            if (Implies(predicate, value->Argument(0))) value = value->Argument(1)->Resolve();
            else if (ImpliesNot(predicate, value->Argument(0))) value = value->Argument(2)->Resolve();
            else break;
        }
        return value;
    }

    IrValue& insert(const Access& access, IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        IrValue& value = program.CreateValue(opcode, type);
        for (IrValue* argument : arguments) value.AddArgument(argument);
        access.block->InsertInstructionBefore(access.inst, &value);
        return value;
    }

    IrValue& componentAddress(const Access& access, std::uint32_t component) {
        if (access.address.dynamic) {
            const std::uint32_t byteOffset = access.memoryOffset + 4u * component;
            return byteOffset == 0u ? *access.addressValue : insert(access, IrOpcode::IAdd32, IrType::U32, {access.addressValue, &constant(byteOffset)});
        }
        auto address = access.address.constant + 4 * static_cast<std::int64_t>(component);
        if (access.kind == TessellationAttribute::PatchOutput || access.kind == TessellationAttribute::PatchInput) address -= FloorDiv(address, patchStride) * patchStride;
        IrValue& offset = constant(static_cast<std::uint32_t>(address));
        const bool indexed = access.kind == TessellationAttribute::ControlInput || ((access.kind == TessellationAttribute::ControlShared || access.kind == TessellationAttribute::ControlOutput) && !access.write);
        if (!indexed || access.address.vertex == 0) return offset;
        IrValue& invocation = insert(access, IrOpcode::GetBuiltin, IrType::U32, {&constant(static_cast<std::uint32_t>(StageInputKind::InvocationId)), &constant(0u)});
        IrValue& scaled = insert(access, IrOpcode::IMul32, IrType::U32, {&invocation, &constant(static_cast<std::uint32_t>(access.address.vertex))});
        return insert(access, IrOpcode::IAdd32, IrType::U32, {&scaled, &offset});
    }

    void keepComputedReadsInPatch(IrValue& base) {
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (inst->Opcode() != IrOpcode::GetTessellationAttribute) continue;
                const bool control = IsImmediate(inst->Argument(0), static_cast<std::uint32_t>(TessellationAttribute::ControlInput));
                if (!control && !IsImmediate(inst->Argument(0), static_cast<std::uint32_t>(TessellationAttribute::EvaluationInput))) continue;
                const auto address = evaluator.Evaluate(inst->Argument(1), inst->Argument(2), true);
                if (!address || !address->dynamic || address->relative == 0) continue;
                IrValue& patch = program.CreateValue(IrOpcode::IMul32, IrType::U32);
                patch.AddArgument(&base);
                patch.AddArgument(&constant(static_cast<std::uint32_t>(address->relative)));
                block->InsertInstructionBefore(inst, &patch);
                IrValue& inPatch = program.CreateValue(IrOpcode::ISub32, IrType::U32);
                inPatch.AddArgument(inst->Argument(1));
                inPatch.AddArgument(&patch);
                block->InsertInstructionBefore(inst, &inPatch);
                inst->ReplaceArgument(1, &inPatch);
            }
        }
    }

    void rewrite(const Access& access) {
        IrValue& kind = constant(static_cast<std::uint32_t>(access.kind));
        const bool shared = SharedAccessOf(access.inst->Opcode()) != SharedAccess::None;
        if (access.half) {
            auto word = access;
            word.address.constant &= ~std::int64_t{3};
            IrValue& address = componentAddress(word, 0u);
            auto record = word;
            record.write = false;
            IrValue& current = access.write && access.kind == TessellationAttribute::ControlShared ? componentAddress(record, 0u) : address;
            IrValue& shift = constant(static_cast<std::uint32_t>((access.address.constant & 2) * 8));
            IrValue& existing = insert(access, IrOpcode::GetTessellationAttribute, IrType::U32, {&kind, &current, access.predicate});
            if (access.write) {
                IrValue& value = insert(access, IrOpcode::ConvertU32U16, IrType::U32, {access.inst->Argument(1u)});
                IrValue& merged = insert(access, IrOpcode::BitFieldInsert, IrType::U32, {&existing, &value, &shift, &constant(16u)});
                static_cast<void>(insert(access, IrOpcode::SetTessellationAttribute, IrType::Void, {&kind, &address, &merged, access.predicate}));
            } else {
                IrValue& value = insert(access, IrOpcode::BitFieldUExtract, IrType::U32, {&existing, &shift, &constant(16u)});
                access.inst->ReplaceAllUsesWith(&insert(access, IrOpcode::ConvertU16U32, IrType::U16, {&value}));
            }
            access.block->RemoveInstruction(access.inst);
            access.inst->Invalidate();
            return;
        }
        if (access.write) {
            for (std::uint32_t component = 0; component < access.components; ++component) {
                IrValue* data = nullptr;
                if (shared) {
                    data = activeValue(access.inst->Argument(1u + component), access.predicate);
                } else if (access.components == 1u) {
                    data = activeValue(access.inst->Argument(4u), access.predicate);
                } else {
                    const auto extract = access.components == 2u ? IrOpcode::CompositeExtractU32x2 : access.components == 3u ? IrOpcode::CompositeExtractU32x3 : IrOpcode::CompositeExtractU32x4;
                    data = &insert(access, extract, IrType::U32, {access.inst->Argument(4u), &constant(component)});
                }
                static_cast<void>(insert(access, IrOpcode::SetTessellationAttribute, IrType::Void, {&kind, &componentAddress(access, component), data, access.predicate}));
            }
        } else {
            std::vector<IrValue*> values;
            for (std::uint32_t component = 0; component < access.components; ++component) {
                values.push_back(&insert(access, IrOpcode::GetTessellationAttribute, IrType::U32, {&kind, &componentAddress(access, component), access.predicate}));
            }
            IrValue* replacement = values.front();
            if (access.components == 2u) replacement = &insert(access, IrOpcode::CompositeConstructU32x2, IrType::U32x2, {values[0], values[1]});
            if (access.components == 3u) replacement = &insert(access, IrOpcode::CompositeConstructU32x3, IrType::U32x3, {values[0], values[1], values[2]});
            if (access.components == 4u) replacement = &insert(access, IrOpcode::CompositeConstructU32x4, IrType::U32x4, {values[0], values[1], values[2], values[3]});
            access.inst->ReplaceAllUsesWith(replacement);
        }
        access.block->RemoveInstruction(access.inst);
        access.inst->Invalidate();
    }

    void removePrimitiveExports() {
        const auto& exports = program.Metadata().exportInfo;
        for (IrBlock* block : program.BlockOrder()) {
            auto& instructions = block->Instructions();
            for (auto it = instructions.begin(); it != instructions.end();) {
                IrValue* inst = *it;
                const auto flags = inst->Flags<ExportFlags>();
                if (inst->Opcode() != IrOpcode::SetAttribute || exports.at(flags.index).kind != ExportTargetKind::Primitive) {
                    ++it;
                    continue;
                }
                const IrValue* data = inst->Argument(0)->Resolve();
                if ((exports.at(flags.index).en & 1u) == 0u || data->Opcode() != IrOpcode::CompositeConstructU32x4 || !IsBase(data->Argument(0), TessellationBaseKind::PassthroughPrimitive)) Fail("the domain shader exports a primitive other than the passthrough primitive", flags.pc);
                it = instructions.erase(it);
                inst->SetParent(nullptr);
                inst->Invalidate();
            }
        }
    }

    IrProgram& program;
    const ShaderTessellationInputInfo& tessellation;
    const TessellationTargetLimits* limits;
    IrShaderStage stage;
    bool merged;
    bool capture;
    AddressEvaluator evaluator;
    std::vector<Access> accesses;
    enum class Dependence : std::uint8_t { Visiting, No, Yes };

    std::uint32_t localStride;
    std::uint32_t controlStride = 0;
    std::uint32_t patchStride = 0;
    std::uint32_t sharedStride = 0;
    std::map<std::pair<const IrValue*, const IrValue*>, Dependence> dependence;
};

}

void TessellationLowering::Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation, const TessellationTargetLimits* limits) const {
    const auto stage = program.Resources().stage;
    if (stage != IrShaderStage::Local && stage != IrShaderStage::TessellationControl && stage != IrShaderStage::TessellationEvaluation) return;
    Lowering(program, tessellation, limits).Run();
}

}
