#include "Optimization/TessellationLowering.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::int64_t PatchIndexCount = 256;
constexpr std::int64_t LocalVertexIndexCount = 8192;
constexpr std::int64_t TriangleFactorBytes = 16;
constexpr std::int64_t AddressLimit = 0xffffffffll;

struct Affine {
    std::int64_t vertex = 0;
    std::int64_t relative = 0;
    std::int64_t constant = 0;

    [[nodiscard]] bool Scalar() const { return vertex == 0 && relative == 0; }
    bool operator==(const Affine& other) const = default;
};

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

IrValue* BallotSource(IrValue* raw, std::uint32_t word) {
    IrValue* value = raw->Resolve();
    if (value->Opcode() != IrOpcode::CompositeExtractU32x4 || !IsImmediate(value->Argument(1), word)) return nullptr;
    IrValue* ballot = value->Argument(0)->Resolve();
    return ballot->Opcode() == IrOpcode::Ballot ? ballot->Argument(0)->Resolve() : nullptr;
}

bool IsTrue(const IrValue* value) {
    return value != nullptr && value->HasImmediate() && value->Type() == IrType::Bool && value->ImmediateBool();
}

IrValue* HostLaneWord(IrValue* raw, std::uint32_t word) {
    IrValue* value = raw->Resolve();
    while (value->Opcode() == IrOpcode::BitwiseAnd32 && IsTrue(BallotSource(value->Argument(1), word))) value = value->Argument(0)->Resolve();
    return value;
}

IrValue* SavedLaneBit(IrValue* value) {
    if (value->Opcode() != IrOpcode::INotEqual32) return nullptr;
    IrValue* masked = VariableOperand(value, 0u);
    if (masked == nullptr || masked->Opcode() != IrOpcode::BitwiseAnd32) return nullptr;
    IrValue* shifted = VariableOperand(masked, 1u);
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32) return nullptr;
    IrValue* bit = shifted->Argument(1)->Resolve();
    if (bit->Opcode() != IrOpcode::BitwiseAnd32 || !IsLaneId(VariableOperand(bit, 31u))) return nullptr;
    IrValue* word = shifted->Argument(0)->Resolve();
    if (word->Opcode() != IrOpcode::SelectU32) return BallotSource(HostLaneWord(word, 0u), 0u);
    IrValue* low = word->Argument(0)->Resolve();
    if (low->Opcode() != IrOpcode::ULessThan32 || !IsLaneId(low->Argument(0)) || !IsImmediate(low->Argument(1), 32u)) return nullptr;
    IrValue* source = BallotSource(HostLaneWord(word->Argument(1), 0u), 0u);
    return source != nullptr && BallotSource(HostLaneWord(word->Argument(2), 1u), 1u) == source ? source : nullptr;
}

bool Implies(IrValue* rawPredicate, IrValue* rawCondition) {
    IrValue* predicate = rawPredicate->Resolve();
    IrValue* condition = rawCondition->Resolve();
    if (predicate == condition) return true;
    if (IsTrue(condition)) return true;
    return predicate->Opcode() == IrOpcode::LogicalAnd && (Implies(predicate->Argument(0), condition) || Implies(predicate->Argument(1), condition));
}

class AddressEvaluator {
public:
    AddressEvaluator(std::uint32_t vertexCount, std::int64_t relativeCount) : vertexCount(vertexCount), relativeCount(relativeCount) {}

    std::optional<Affine> Evaluate(IrValue* value, IrValue* predicate) {
        memo.clear();
        active = predicate;
        return evaluate(value);
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
        if (Lowest(value) < 0 || Highest(value) > AddressLimit) return std::nullopt;
        return value;
    }

    [[nodiscard]] std::optional<Affine> scale(const Affine& value, std::int64_t factor) const {
        if (factor < 0 || factor >= (1ll << 31)) return std::nullopt;
        return bounded({value.vertex * factor, value.relative * factor, value.constant * factor});
    }

    [[nodiscard]] std::optional<std::pair<Affine, Affine>> split(const Affine& value, std::int64_t divisor) const {
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
        if (value.Scalar()) return Affine{0, 0, value.constant & bits};
        const auto width = std::bit_width(static_cast<std::uint64_t>(bits));
        if (width >= 32) return bits == AddressLimit ? std::optional(value) : std::nullopt;
        const auto divisor = 1ll << width;
        const auto parts = split(value, divisor);
        if (!parts) return std::nullopt;
        if (parts->second.Scalar()) return Affine{0, 0, parts->second.constant & bits};
        return bits == divisor - 1 ? std::optional(parts->second) : std::nullopt;
    }

    [[nodiscard]] std::optional<Affine> disjoint(const Affine& high, const Affine& low) const {
        if (Lowest(low) < 0) return std::nullopt;
        const auto divisor = 1ll << std::bit_width(static_cast<std::uint64_t>(Highest(low)));
        if (high.vertex % divisor != 0 || high.relative % divisor != 0 || high.constant % divisor != 0) return std::nullopt;
        return bounded({high.vertex + low.vertex, high.relative + low.relative, high.constant + low.constant});
    }

    std::optional<Affine> evaluate(IrValue* raw) {
        IrValue* value = raw->Resolve();
        if (value->HasImmediate()) {
            if (value->Type() != IrType::U32) return std::nullopt;
            return Affine{0, 0, value->ImmediateU32()};
        }
        if (const auto found = memo.find(value); found != memo.end()) return found->second;
        memo.emplace(value, std::nullopt);
        const auto result = compute(value);
        memo[value] = result;
        return result;
    }

    std::optional<Affine> compute(IrValue* value) {
        const auto argument = [&](std::size_t index) { return evaluate(value->Argument(index)); };
        const auto scalar = [&](std::size_t index) -> std::optional<std::int64_t> {
            const auto operand = argument(index);
            if (!operand || !operand->Scalar()) return std::nullopt;
            return operand->constant;
        };
        switch (value->Opcode()) {
        case IrOpcode::TessellationBase:
            return IsBase(value, TessellationBaseKind::RelativeIndex) ? std::optional(Affine{0, 1, 0}) : std::nullopt;
        case IrOpcode::GetBuiltin:
            return vertexCount != 0 && IsImmediate(value->Argument(0), static_cast<std::uint32_t>(StageInputKind::InvocationId)) ? std::optional(Affine{1, 0, 0}) : std::nullopt;
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32: {
            const auto lhs = argument(0);
            const auto rhs = argument(1);
            if (!lhs || !rhs) return std::nullopt;
            const std::int64_t sign = value->Opcode() == IrOpcode::IAdd32 ? 1 : -1;
            return bounded({lhs->vertex + sign * rhs->vertex, lhs->relative + sign * rhs->relative, lhs->constant + sign * rhs->constant});
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
            if (!lhs || !shift || *shift >= 31) return std::nullopt;
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
            if (!source || !offset || !count) return std::nullopt;
            if (*count == 0) return Affine{};
            const auto shifted = shiftRight(*source, *offset);
            if (!shifted || *count >= 32) return shifted;
            return mask(*shifted, (1ll << *count) - 1);
        }
        case IrOpcode::SelectU32: {
            if (active != nullptr && Implies(active, value->Argument(0))) return argument(1);
            const auto lhs = argument(1);
            const auto rhs = argument(2);
            return lhs && rhs && *lhs == *rhs ? lhs : std::nullopt;
        }
        default:
            break;
        }
        if (!value->IsPhi() || value->ArgumentCount() == 0) return std::nullopt;
        const auto first = argument(0);
        for (std::size_t index = 1; first && index < value->ArgumentCount(); ++index) {
            const auto other = argument(index);
            if (!other || !(*other == *first)) return std::nullopt;
        }
        return first;
    }

    std::uint32_t vertexCount;
    std::int64_t relativeCount;
    IrValue* active = nullptr;
    std::unordered_map<IrValue*, std::optional<Affine>> memo;
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

struct Access {
    IrBlock* block = nullptr;
    IrValue* inst = nullptr;
    TessellationAttribute kind = TessellationAttribute::LocalOutput;
    bool write = false;
    std::uint32_t components = 1;
    Affine address;
    IrValue* predicate = nullptr;
    std::uint32_t pc = 0;
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
    Lowering(IrProgram& program, const ShaderTessellationInputInfo& tessellation)
        : program(program), tessellation(tessellation), stage(program.Resources().stage), evaluator(stage == IrShaderStage::TessellationControl ? tessellation.outputControlPoints : 0u, stage == IrShaderStage::Local ? LocalVertexIndexCount : PatchIndexCount) {}

    void Run() {
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (IrValue* source = SavedLaneBit(inst)) inst->ReplaceAllUsesWith(source);
            }
        }
        collect();
        derive();
        for (const auto& access : accesses) rewrite(access);
        if (stage == IrShaderStage::TessellationEvaluation) removePrimitiveExports();
        constexpr DeadCodeEliminator deadCodeEliminator;
        deadCodeEliminator.Eliminate(program);
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
        requireRelativeIndependence(relative);
        for (IrValue* base : relative) base->ReplaceAllUsesWith(&constant(0u));
        deadCodeEliminator.Eliminate(program);
        program.Metadata().tessellationLocalStride = localStride;
        program.Metadata().tessellationControlStride = controlStride;
    }

private:
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
                Access access{block, inst, TessellationAttribute::LocalOutput, false, ComponentCount(opcode), {}, inst->Argument(inst->ArgumentCount() - 1u), flags.pc};
                IrValue* address = nullptr;
                if (shared != SharedAccess::None) {
                    if (memory.kind != ResourceKind::Lds) continue;
                    access.write = shared == SharedAccess::Write;
                    if (access.components == 0u || (shared != SharedAccess::Read && shared != SharedAccess::Write)) Fail("only 32-bit LDS reads and writes are supported in tessellation stages", flags.pc);
                    if (stage == IrShaderStage::Local && access.write) {
                        access.kind = TessellationAttribute::LocalOutput;
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
                    } else if (stage == IrShaderStage::TessellationEvaluation && !access.write && !factor) {
                        access.kind = TessellationAttribute::EvaluationInput;
                    } else {
                        Fail("unsupported tessellation ring access", flags.pc);
                    }
                    address = memory.offen ? inst->Argument(2) : nullptr;
                }
                if (address != nullptr) {
                    const auto evaluated = evaluator.Evaluate(address, access.predicate);
                    if (!evaluated) Fail("the tessellation ring or LDS address is not affine in the control point and relative patch", flags.pc);
                    access.address = *evaluated;
                }
                access.address.constant += memory.offset;
                if ((access.address.constant & 3) != 0) Fail("unaligned tessellation ring or LDS address", flags.pc);
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
        for (const auto& access : accesses) {
            if (access.kind == TessellationAttribute::LocalOutput) agree(localStride, access.address.relative, access.pc);
            if (access.kind == TessellationAttribute::ControlInput && access.address.vertex != 0) agree(localStride, access.address.vertex, access.pc);
            if (access.kind == TessellationAttribute::ControlOutput && access.address.vertex != 0) agree(controlStride, access.address.vertex, access.pc);
            if (access.kind == TessellationAttribute::EvaluationInput) {
                if (access.address.relative <= 0 || access.address.relative % outputs != 0) Fail("domain shader reads of patch constants or other patches are unsupported", access.pc);
                agree(controlStride, access.address.relative / outputs, access.pc);
            }
        }
        std::optional<std::int64_t> patchStride;
        std::int64_t patchLowest = AddressLimit;
        std::int64_t patchHighest = 0;
        for (auto& access : accesses) {
            const auto& address = access.address;
            const auto end = address.constant + 4 * static_cast<std::int64_t>(access.components);
            switch (access.kind) {
            case TessellationAttribute::LocalOutput:
                if (address.vertex != 0 || address.constant < 0 || end > localStride) Fail("the local stage writes outside its vertex's LDS slot", access.pc);
                break;
            case TessellationAttribute::ControlInput:
                if (localStride == 0u) Fail("the hull shader reads no LDS per control point, so the local stride is unknown", access.pc);
                if ((address.vertex != 0 && address.vertex != localStride) || address.relative != inputs * localStride || address.constant < 0 || end + address.vertex * (outputs - 1) > inputs * localStride) Fail("the hull shader reads LDS outside its patch", access.pc);
                break;
            case TessellationAttribute::ControlOutput:
                if (address.vertex == 0) {
                    if (controlStride == 0u || address.constant < outputs * controlStride) Fail("the hull shader writes off-chip memory outside its control points and patch constants", access.pc);
                    if (patchStride && *patchStride != address.relative) Fail("inconsistent patch-constant stride", access.pc);
                    patchStride = address.relative;
                    patchLowest = std::min(patchLowest, address.constant);
                    patchHighest = std::max(patchHighest, end);
                    access.kind = TessellationAttribute::PatchOutput;
                    requireFirstInvocation(access);
                } else if (address.relative != outputs * controlStride || address.constant < 0 || end > controlStride) {
                    Fail("the hull shader writes off-chip memory outside its control point", access.pc);
                }
                break;
            case TessellationAttribute::EvaluationInput:
                if (address.constant < 0 || end > outputs * controlStride) Fail("the domain shader reads off-chip memory outside its patch's control points", access.pc);
                break;
            case TessellationAttribute::Factor:
                if (tessellation.domain != 1u || address.vertex != 0 || address.relative != TriangleFactorBytes || address.constant < 0 || end > TriangleFactorBytes) Fail("the hull shader writes tessellation factors outside its patch's triangle factors", access.pc);
                requireFirstInvocation(access);
                break;
            case TessellationAttribute::PatchOutput:
                break;
            }
        }
        if (patchStride && (*patchStride == 0 || std::abs(*patchStride) < patchHighest - patchLowest)) Fail("patch-constant records of neighbouring patches overlap", 0u);
    }

    void requireRelativeIndependence(const std::vector<IrValue*>& bases) {
        std::vector<IrValue*> pending(bases.begin(), bases.end());
        std::unordered_set<IrValue*> visited(bases.begin(), bases.end());
        while (!pending.empty()) {
            IrValue* value = pending.back();
            pending.pop_back();
            for (IrValue* user : value->Uses()) {
                switch (user->Opcode()) {
                case IrOpcode::Identity:
                case IrOpcode::IAdd32:
                case IrOpcode::ISub32:
                case IrOpcode::IMul32:
                case IrOpcode::ShiftLeftLogical32:
                case IrOpcode::ShiftRightLogical32:
                case IrOpcode::BitwiseAnd32:
                case IrOpcode::BitwiseOr32:
                case IrOpcode::BitFieldUExtract:
                case IrOpcode::SelectU32:
                case IrOpcode::Phi:
                    if (visited.insert(user).second) pending.push_back(user);
                    continue;
                default:
                    break;
                }
                const auto address = evaluator.Evaluate(value, nullptr);
                if (!address || address->relative != 0) Fail("the relative patch or vertex index is used outside a tessellation ring or LDS address", 0u);
            }
        }
    }

    void requireFirstInvocation(const Access& access) {
        std::unordered_set<IrValue*> path;
        if (!FirstInvocationOnly(access.predicate, evaluator, path)) Fail("per-patch tessellation output is not written by control point 0 alone", access.pc);
    }

    IrValue& constant(std::uint32_t value) {
        IrValue& result = program.CreateValue(IrOpcode::Void, IrType::U32);
        result.SetImmediateU32(value);
        return result;
    }

    IrValue& insert(const Access& access, IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        IrValue& value = program.CreateValue(opcode, type);
        for (IrValue* argument : arguments) value.AddArgument(argument);
        access.block->InsertInstructionBefore(access.inst, &value);
        return value;
    }

    IrValue& componentAddress(const Access& access, std::uint32_t component) {
        IrValue& offset = constant(static_cast<std::uint32_t>(access.address.constant) + 4u * component);
        if (access.kind != TessellationAttribute::ControlInput || access.address.vertex == 0) return offset;
        IrValue& invocation = insert(access, IrOpcode::GetBuiltin, IrType::U32, {&constant(static_cast<std::uint32_t>(StageInputKind::InvocationId)), &constant(0u)});
        IrValue& scaled = insert(access, IrOpcode::IMul32, IrType::U32, {&invocation, &constant(static_cast<std::uint32_t>(access.address.vertex))});
        return insert(access, IrOpcode::IAdd32, IrType::U32, {&scaled, &offset});
    }

    void rewrite(const Access& access) {
        IrValue& kind = constant(static_cast<std::uint32_t>(access.kind));
        const bool shared = SharedAccessOf(access.inst->Opcode()) != SharedAccess::None;
        if (access.write) {
            for (std::uint32_t component = 0; component < access.components; ++component) {
                IrValue* data = nullptr;
                if (shared) {
                    data = access.inst->Argument(1u + component);
                } else if (access.components == 1u) {
                    data = access.inst->Argument(4u);
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
    IrShaderStage stage;
    AddressEvaluator evaluator;
    std::vector<Access> accesses;
    std::uint32_t localStride = 0;
    std::uint32_t controlStride = 0;
};

}

void TessellationLowering::Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation) const {
    const auto stage = program.Resources().stage;
    if (stage != IrShaderStage::Local && stage != IrShaderStage::TessellationControl && stage != IrShaderStage::TessellationEvaluation) return;
    Lowering(program, tessellation).Run();
}

}
