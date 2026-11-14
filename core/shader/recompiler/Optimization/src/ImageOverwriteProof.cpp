#include "Optimization/ImageOverwriteProof.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t NoRegister = NoOverwriteRegister;
constexpr std::size_t MaxLanes = 1024;
constexpr std::size_t MaxTileCells = 1u << 16u;

struct Form {
    std::array<std::int64_t, 3> group{};
    std::uint32_t uniform = NoRegister;
    std::vector<std::int64_t> lane;

    [[nodiscard]] bool HasGroup() const { return group[0] != 0 || group[1] != 0 || group[2] != 0; }
    [[nodiscard]] bool LaneOnly() const { return !HasGroup() && uniform == NoRegister; }
    [[nodiscard]] std::optional<std::uint32_t> GroupAxis() const {
        std::optional<std::uint32_t> axis;
        for (std::uint32_t index = 0; index < 3; ++index) {
            if (group[index] == 0) continue;
            if (axis.has_value()) return std::nullopt;
            axis = index;
        }
        return axis;
    }
    [[nodiscard]] bool Uniform() const { return std::all_of(lane.begin(), lane.end(), [&](std::int64_t value) { return value == lane.front(); }); }
};

std::uint32_t U32(std::int64_t value) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(value));
}

bool ImmediateOperand(const IrValue* value, std::uint32_t& result) {
    const auto* resolved = value->Resolve();
    if (!resolved->HasImmediate()) return false;
    result = resolved->ImmediateU32();
    return true;
}

struct Bound {
    std::uint32_t axis = 0;
    std::int64_t scale = 0;
    std::vector<std::int64_t> lane;
    std::uint32_t reg = NoRegister;
    std::uint32_t immediate = 0;
};

struct Atoms {
    std::uint32_t fullGroups = 0;
    std::vector<bool> active;
    std::vector<Bound> bounds;
};

class Prover {
public:
    explicit Prover(const std::array<std::uint32_t, 3>& groupThreads) : threads(groupThreads), lanes(static_cast<std::size_t>(groupThreads[0]) * groupThreads[1] * groupThreads[2]) {}

    [[nodiscard]] std::size_t Lanes() const { return lanes; }
    std::vector<ImageOverwriteRange> ranges;
    bool failed = false;

    std::optional<Form> Evaluate(const IrValue* value) {
        value = value->Resolve();
        if (const auto found = memo.find(value); found != memo.end()) return found->second;
        auto result = compute(value);
        memo.emplace(value, result);
        return result;
    }

    std::optional<Form> Half(const IrValue* value, std::uint32_t shift) {
        value = value->Resolve();
        const auto op = value->Opcode();
        std::uint32_t immediate = 0;
        if ((op == IrOpcode::BitwiseOr32 || op == IrOpcode::BitwiseXor32) && value->ArgumentCount() == 2) {
            auto left = Half(value->Argument(0), shift);
            auto right = Half(value->Argument(1), shift);
            if (!left || !right) return std::nullopt;
            if (zero(*left)) return right;
            if (zero(*right)) return left;
            if (!left->LaneOnly() || !right->LaneOnly()) return std::nullopt;
            for (std::size_t index = 0; index < lanes; ++index) left->lane[index] = op == IrOpcode::BitwiseOr32 ? (U32(left->lane[index]) | U32(right->lane[index])) : (U32(left->lane[index]) ^ U32(right->lane[index]));
            return left;
        }
        if (op == IrOpcode::ShiftLeftLogical32 && value->ArgumentCount() == 2 && ImmediateOperand(value->Argument(1), immediate) && immediate == 16u) {
            if (shift == 0u) return constant(0);
            return truncated(value->Argument(0), 16u);
        }
        if (op == IrOpcode::BitwiseAnd32 && value->ArgumentCount() == 2) {
            for (std::size_t side = 0; side < 2; ++side) {
                if (ImmediateOperand(value->Argument(side), immediate) && immediate == 0xffffu) {
                    if (shift != 0u) return constant(0);
                    return truncated(value->Argument(1 - side), 16u);
                }
            }
        }
        if (op == IrOpcode::BitFieldUExtract && value->ArgumentCount() == 3) {
            std::uint32_t offset = 0;
            std::uint32_t count = 0;
            if (ImmediateOperand(value->Argument(1), offset) && ImmediateOperand(value->Argument(2), count) && offset == 0u && count != 0u && count <= 16u) {
                if (shift != 0u) return constant(0);
                return truncated(value->Argument(0), count);
            }
        }
        auto whole = Evaluate(value);
        if (!whole) return std::nullopt;
        if (whole->LaneOnly()) {
            for (auto& lane : whole->lane) lane = (U32(lane) >> shift) & 0xffffu;
            return whole;
        }
        if (!require(*whole, 16u)) return std::nullopt;
        if (shift != 0u) return constant(0);
        return whole;
    }

    bool Collect(const IrValue* predicate, Atoms& atoms) {
        predicate = predicate->Resolve();
        if (predicate->HasImmediate()) return predicate->ImmediateBool();
        if (predicate->Opcode() == IrOpcode::LogicalAnd && predicate->ArgumentCount() == 2) return Collect(predicate->Argument(0), atoms) && Collect(predicate->Argument(1), atoms);
        if ((predicate->Opcode() == IrOpcode::ULessThan32 || predicate->Opcode() == IrOpcode::UGreaterThan32) && predicate->ArgumentCount() == 2) {
            const bool less = predicate->Opcode() == IrOpcode::ULessThan32;
            if (less32(predicate->Argument(less ? 0 : 1), predicate->Argument(less ? 1 : 0), atoms)) return true;
        }
        const auto local = localBool(predicate);
        if (!local) return false;
        for (std::size_t index = 0; index < lanes; ++index) atoms.active[index] = atoms.active[index] && (*local)[index];
        return true;
    }

private:
    std::array<std::uint32_t, 3> threads;
    std::size_t lanes;
    std::unordered_map<const IrValue*, std::optional<Form>> memo;

    [[nodiscard]] Form constant(std::int64_t value) const {
        Form form;
        form.lane.assign(lanes, value);
        return form;
    }

    [[nodiscard]] static bool zero(const Form& form) {
        return form.LaneOnly() && std::all_of(form.lane.begin(), form.lane.end(), [](std::int64_t value) { return U32(value) == 0u; });
    }

    [[nodiscard]] std::int64_t local(std::size_t lane, std::uint32_t axis) const {
        if (axis == 0) return static_cast<std::int64_t>(lane % threads[0]);
        if (axis == 1) return static_cast<std::int64_t>((lane / threads[0]) % threads[1]);
        return static_cast<std::int64_t>(lane / (static_cast<std::size_t>(threads[0]) * threads[1]));
    }

    bool require(const Form& form, std::uint32_t bits) {
        if (form.uniform != NoRegister || bits == 0u || bits >= 32u) return false;
        const auto axis = form.GroupAxis();
        if (!axis.has_value() || form.group[*axis] <= 0) return false;
        const auto [low, high] = std::minmax_element(form.lane.begin(), form.lane.end());
        if (*low < 0 || *high > std::numeric_limits<std::int32_t>::max()) return false;
        const auto scale = static_cast<std::uint32_t>(form.group[*axis]);
        for (auto& range : ranges) {
            if (range.groupAxis != *axis || range.scale != scale || range.bits != bits) continue;
            range.minLane = std::min(range.minLane, static_cast<std::int32_t>(*low));
            range.maxLane = std::max(range.maxLane, static_cast<std::int32_t>(*high));
            return true;
        }
        if (ranges.size() >= 4u) return false;
        ranges.push_back({*axis, scale, static_cast<std::int32_t>(*low), static_cast<std::int32_t>(*high), bits});
        return true;
    }

    std::optional<Form> truncated(const IrValue* value, std::uint32_t bits) {
        auto form = Evaluate(value);
        if (!form) return std::nullopt;
        if (form->LaneOnly()) {
            const auto mask = bits >= 32u ? 0xffffffffu : (1u << bits) - 1u;
            for (auto& lane : form->lane) lane = U32(lane) & mask;
            return form;
        }
        if (!require(*form, bits)) return std::nullopt;
        return form;
    }

    std::optional<Form> compute(const IrValue* value) {
        if (value->HasImmediate()) return constant(static_cast<std::int32_t>(value->ImmediateU32()));
        const auto op = value->Opcode();
        const auto argument = [&](std::size_t index) { return Evaluate(value->Argument(index)); };
        std::uint32_t immediate = 0;
        switch (op) {
        case IrOpcode::GetBuiltin: {
            std::uint32_t kind = 0;
            std::uint32_t component = 0;
            if (value->ArgumentCount() != 2 || !ImmediateOperand(value->Argument(0), kind) || !ImmediateOperand(value->Argument(1), component) || component >= 3u) return std::nullopt;
            Form form = constant(0);
            switch (static_cast<StageInputKind>(kind)) {
            case StageInputKind::WorkgroupId: form.group[component] = 1; return form;
            case StageInputKind::LocalInvocationId:
                for (std::size_t lane = 0; lane < lanes; ++lane) form.lane[lane] = local(lane, component);
                return form;
            case StageInputKind::LocalInvocationIndex:
                if (component != 0u) return std::nullopt;
                for (std::size_t lane = 0; lane < lanes; ++lane) form.lane[lane] = static_cast<std::int64_t>(lane);
                return form;
            case StageInputKind::GlobalInvocationId:
                form.group[component] = threads[component];
                for (std::size_t lane = 0; lane < lanes; ++lane) form.lane[lane] = local(lane, component);
                return form;
            default: return std::nullopt;
            }
        }
        case IrOpcode::GetUserData: {
            if (value->ArgumentCount() != 1 || value->Argument(0)->Type() != IrType::ScalarReg) return std::nullopt;
            const auto index = value->Argument(0)->Register().index;
            if (index >= 16u) return std::nullopt;
            Form form = constant(0);
            form.uniform = index;
            return form;
        }
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32: {
            if (value->ArgumentCount() != 2) return std::nullopt;
            auto left = argument(0);
            auto right = argument(1);
            if (!left || !right) return std::nullopt;
            const std::int64_t sign = op == IrOpcode::IAdd32 ? 1 : -1;
            if (right->uniform != NoRegister && (sign < 0 || left->uniform != NoRegister)) return std::nullopt;
            if (right->uniform != NoRegister) left->uniform = right->uniform;
            for (std::size_t axis = 0; axis < 3; ++axis) left->group[axis] += sign * right->group[axis];
            for (std::size_t lane = 0; lane < lanes; ++lane) left->lane[lane] += sign * right->lane[lane];
            return left;
        }
        case IrOpcode::IMul32: {
            if (value->ArgumentCount() != 2) return std::nullopt;
            auto left = argument(0);
            auto right = argument(1);
            if (!left || !right) return std::nullopt;
            if (!(right->LaneOnly() && right->Uniform())) std::swap(left, right);
            if (!(right->LaneOnly() && right->Uniform()) || left->uniform != NoRegister) return std::nullopt;
            const auto factor = static_cast<std::int64_t>(static_cast<std::int32_t>(U32(right->lane.front())));
            if (factor < 0 || factor > 0xffff) return std::nullopt;
            for (auto& coefficient : left->group) coefficient *= factor;
            for (auto& lane : left->lane) lane *= factor;
            return left;
        }
        case IrOpcode::ShiftLeftLogical32: {
            if (value->ArgumentCount() != 2 || !ImmediateOperand(value->Argument(1), immediate) || immediate >= 24u) return std::nullopt;
            auto form = argument(0);
            if (!form || form->uniform != NoRegister) return std::nullopt;
            for (auto& coefficient : form->group) coefficient <<= immediate;
            for (auto& lane : form->lane) lane = form->LaneOnly() ? static_cast<std::int64_t>(U32(lane) << immediate) : lane * (std::int64_t{1} << immediate);
            return form;
        }
        case IrOpcode::ShiftRightLogical32: {
            if (value->ArgumentCount() != 2 || !ImmediateOperand(value->Argument(1), immediate) || immediate >= 32u) return std::nullopt;
            auto form = argument(0);
            if (!form || !form->LaneOnly()) return std::nullopt;
            for (auto& lane : form->lane) lane = U32(lane) >> immediate;
            return form;
        }
        case IrOpcode::BitwiseAnd32: {
            if (value->ArgumentCount() != 2) return std::nullopt;
            for (std::size_t side = 0; side < 2; ++side) {
                if (!ImmediateOperand(value->Argument(side), immediate)) continue;
                auto other = Evaluate(value->Argument(1 - side));
                if (!other) return std::nullopt;
                if (other->LaneOnly()) {
                    for (auto& lane : other->lane) lane = U32(lane) & immediate;
                    return other;
                }
                if (immediate == 0xffffffffu) return other;
                if (immediate == 0u || (immediate & (immediate + 1u)) != 0u) return std::nullopt;
                return truncated(value->Argument(1 - side), static_cast<std::uint32_t>(std::popcount(immediate)));
            }
            auto left = argument(0);
            auto right = argument(1);
            if (!left || !right || !left->LaneOnly() || !right->LaneOnly()) return std::nullopt;
            for (std::size_t lane = 0; lane < lanes; ++lane) left->lane[lane] = U32(left->lane[lane]) & U32(right->lane[lane]);
            return left;
        }
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32: {
            if (value->ArgumentCount() != 2) return std::nullopt;
            auto left = argument(0);
            auto right = argument(1);
            if (!left || !right) return std::nullopt;
            if (left->LaneOnly() && right->LaneOnly()) {
                for (std::size_t lane = 0; lane < lanes; ++lane) left->lane[lane] = op == IrOpcode::BitwiseOr32 ? (U32(left->lane[lane]) | U32(right->lane[lane])) : (U32(left->lane[lane]) ^ U32(right->lane[lane]));
                return left;
            }
            if (left->LaneOnly()) std::swap(left, right);
            if (!right->LaneOnly() || left->uniform != NoRegister) return std::nullopt;
            int zeros = 32;
            for (const auto coefficient : left->group) {
                if (coefficient != 0) zeros = std::min(zeros, std::countr_zero(static_cast<std::uint64_t>(coefficient < 0 ? -coefficient : coefficient)));
            }
            for (const auto lane : left->lane) {
                if (lane < 0) return std::nullopt;
                if (U32(lane) != 0u) zeros = std::min(zeros, std::countr_zero(U32(lane)));
            }
            for (const auto lane : right->lane) {
                if (zeros < 32 && U32(lane) >= (1u << zeros)) return std::nullopt;
            }
            for (std::size_t lane = 0; lane < lanes; ++lane) left->lane[lane] += U32(right->lane[lane]);
            return left;
        }
        case IrOpcode::BitFieldUExtract: {
            std::uint32_t offset = 0;
            std::uint32_t count = 0;
            if (value->ArgumentCount() != 3 || !ImmediateOperand(value->Argument(1), offset) || !ImmediateOperand(value->Argument(2), count) || offset >= 32u || count == 0u || offset + count > 32u) return std::nullopt;
            auto form = argument(0);
            if (!form) return std::nullopt;
            if (form->LaneOnly()) {
                const auto mask = count >= 32u ? 0xffffffffu : (1u << count) - 1u;
                for (auto& lane : form->lane) lane = (U32(lane) >> offset) & mask;
                return form;
            }
            if (offset != 0u) return std::nullopt;
            return truncated(value->Argument(0), count);
        }
        default: return std::nullopt;
        }
    }

    bool less32(const IrValue* left, const IrValue* right, Atoms& atoms) {
        const auto* small = left->Resolve();
        const auto* large = right->Resolve();
        std::uint32_t kind = 0;
        std::uint32_t component = 0;
        std::uint32_t limitKind = 0;
        std::uint32_t limitComponent = 0;
        if (small->Opcode() == IrOpcode::GetBuiltin && large->Opcode() == IrOpcode::GetBuiltin && small->ArgumentCount() == 2 && large->ArgumentCount() == 2
            && ImmediateOperand(small->Argument(0), kind) && ImmediateOperand(small->Argument(1), component) && ImmediateOperand(large->Argument(0), limitKind) && ImmediateOperand(large->Argument(1), limitComponent)
            && static_cast<StageInputKind>(kind) == StageInputKind::GlobalInvocationId && static_cast<StageInputKind>(limitKind) == StageInputKind::DispatchThreadLimit && component == limitComponent && component < 3u) {
            atoms.fullGroups |= 1u << component;
            return true;
        }
        const auto value = Evaluate(small);
        const auto bound = Evaluate(large);
        if (!value || !bound) return false;
        if (value->LaneOnly() && bound->LaneOnly()) {
            for (std::size_t lane = 0; lane < lanes; ++lane) atoms.active[lane] = atoms.active[lane] && U32(value->lane[lane]) < U32(bound->lane[lane]);
            return true;
        }
        const auto axis = value->GroupAxis();
        if (!axis.has_value() || value->uniform != NoRegister || bound->HasGroup() || !bound->Uniform()) return false;
        Bound entry{*axis, value->group[*axis], value->lane};
        if (bound->uniform != NoRegister) {
            if (U32(bound->lane.front()) != 0u) return false;
            entry.reg = bound->uniform;
        } else {
            entry.immediate = U32(bound->lane.front());
            if (entry.immediate == 0u) return false;
        }
        atoms.bounds.push_back(std::move(entry));
        return true;
    }

    std::optional<std::vector<bool>> localBool(const IrValue* value) {
        value = value->Resolve();
        if (value->HasImmediate()) return std::vector<bool>(lanes, value->ImmediateBool());
        const auto op = value->Opcode();
        if ((op == IrOpcode::LogicalAnd || op == IrOpcode::LogicalOr) && value->ArgumentCount() == 2) {
            auto left = localBool(value->Argument(0));
            auto right = localBool(value->Argument(1));
            if (!left || !right) return std::nullopt;
            for (std::size_t lane = 0; lane < lanes; ++lane) (*left)[lane] = op == IrOpcode::LogicalAnd ? (*left)[lane] && (*right)[lane] : (*left)[lane] || (*right)[lane];
            return left;
        }
        if (op == IrOpcode::LogicalNot && value->ArgumentCount() == 1) {
            auto inner = localBool(value->Argument(0));
            if (!inner) return std::nullopt;
            inner->flip();
            return inner;
        }
        const bool compare = op == IrOpcode::ULessThan32 || op == IrOpcode::UGreaterThan32 || op == IrOpcode::ULessThanEqual32 || op == IrOpcode::UGreaterThanEqual32 || op == IrOpcode::IEqual32 || op == IrOpcode::INotEqual32;
        if (!compare || value->ArgumentCount() != 2) return std::nullopt;
        const auto left = Evaluate(value->Argument(0));
        const auto right = Evaluate(value->Argument(1));
        if (!left || !right || !left->LaneOnly() || !right->LaneOnly()) return std::nullopt;
        std::vector<bool> result(lanes);
        for (std::size_t lane = 0; lane < lanes; ++lane) {
            const auto a = U32(left->lane[lane]);
            const auto b = U32(right->lane[lane]);
            switch (op) {
            case IrOpcode::ULessThan32: result[lane] = a < b; break;
            case IrOpcode::UGreaterThan32: result[lane] = a > b; break;
            case IrOpcode::ULessThanEqual32: result[lane] = a <= b; break;
            case IrOpcode::UGreaterThanEqual32: result[lane] = a >= b; break;
            case IrOpcode::IEqual32: result[lane] = a == b; break;
            default: result[lane] = a != b; break;
            }
        }
        return result;
    }
};

std::vector<const IrValue*> Conjuncts(const IrValue* predicate) {
    std::vector<const IrValue*> result;
    std::vector<const IrValue*> pending{predicate->Resolve()};
    while (!pending.empty()) {
        const auto* value = pending.back();
        pending.pop_back();
        result.push_back(value);
        if (value->Opcode() == IrOpcode::LogicalAnd && value->ArgumentCount() == 2) {
            pending.push_back(value->Argument(0)->Resolve());
            pending.push_back(value->Argument(1)->Resolve());
        }
    }
    return result;
}

const IrValue* TerminatorCondition(const IrBlock& block) {
    const auto& instructions = block.Instructions();
    for (auto it = instructions.rbegin(); it != instructions.rend(); ++it) {
        if ((*it)->Opcode() == IrOpcode::Reference && (*it)->ArgumentCount() == 1) return (*it)->Argument(0)->Resolve();
    }
    return nullptr;
}

bool ReachedOnce(const IrProgram& program, const IrBlock& store, const IrValue* predicate) {
    const auto& order = program.BlockOrder();
    const auto& infos = program.Metadata().blockInfo;
    if (order.size() != infos.size()) return false;
    std::unordered_map<const IrBlock*, std::size_t> index;
    std::unordered_map<std::uint32_t, const IrBlock*> byId;
    for (std::size_t position = 0; position < order.size(); ++position) {
        if (order[position] == nullptr) return false;
        index.emplace(order[position], position);
        byId.emplace(infos[position].id, order[position]);
        const auto& terminator = infos[position].terminator;
        if (terminator.loopHeader || terminator.kind == TerminatorKind::IndirectBranch || terminator.kind == TerminatorKind::Unsupported) return false;
    }
    const auto conjuncts = Conjuncts(predicate);
    const auto harmless = [&](const IrBlock* from, const IrBlock* to) {
        const auto found = index.find(from);
        if (found == index.end()) return false;
        const auto& info = infos[found->second];
        if (info.terminator.kind != TerminatorKind::ConditionalBranch) return false;
        if (info.terminator.condition != BranchCondition::ExecZero && info.terminator.condition != BranchCondition::VccZero) return false;
        const auto target = byId.find(info.terminator.trueBlock);
        if (target == byId.end() || target->second != to) return false;
        const auto* condition = TerminatorCondition(*from);
        if (condition == nullptr || condition->Opcode() != IrOpcode::LogicalNot || condition->ArgumentCount() != 1) return false;
        const auto* mask = condition->Argument(0)->Resolve();
        return std::find(conjuncts.begin(), conjuncts.end(), mask) != conjuncts.end();
    };
    std::unordered_map<const IrBlock*, int> state;
    bool cyclic = false;
    bool escapes = false;
    const auto* entry = program.BlockOrder().front();
    std::vector<std::pair<const IrBlock*, std::size_t>> stack{{entry, 0}};
    state[entry] = 1;
    while (!stack.empty() && !cyclic) {
        auto& [block, next] = stack.back();
        const auto& successors = block->Successors();
        if (next == 0 && successors.empty() && block != &store) escapes = true;
        if (next >= successors.size()) {
            state[block] = 2;
            stack.pop_back();
            continue;
        }
        const auto* successor = successors[next++];
        if (block == &store) continue;
        if (harmless(block, successor)) continue;
        const auto seen = state[successor];
        if (seen == 1) cyclic = true;
        else if (seen == 0) {
            state[successor] = 1;
            stack.emplace_back(successor, 0);
        }
    }
    return !cyclic && !escapes && state[&store] != 0;
}

bool AnyCycle(const IrProgram& program) {
    std::unordered_map<const IrBlock*, int> state;
    const auto* entry = program.BlockOrder().front();
    std::vector<std::pair<const IrBlock*, std::size_t>> stack{{entry, 0}};
    state[entry] = 1;
    while (!stack.empty()) {
        auto& [block, next] = stack.back();
        if (next >= block->Successors().size()) {
            state[block] = 2;
            stack.pop_back();
            continue;
        }
        const auto* successor = block->Successors()[next++];
        const auto seen = state[successor];
        if (seen == 1) return true;
        if (seen == 0) {
            state[successor] = 1;
            stack.emplace_back(successor, 0);
        }
    }
    return false;
}

struct Store {
    std::array<Form, 2> coordinate;
    Atoms atoms;
};

std::optional<ImageOverwrite> ProveImage(const IrProgram& program, std::uint32_t image, const std::array<std::uint32_t, 3>& threads) {
    const auto& info = program.Resources().info;
    const auto& resource = info.images[image];
    if (resource.resourceClass != ImageResourceClass::Storage || !resource.written || resource.read || resource.atomic || resource.table != NoTable || resource.optionalDescriptor || resource.byElements != 0u || resource.packed || resource.r128 || resource.dimension != RdnaImageDimension::Dim2D || resource.mipMode != ImageMipMode::None) return std::nullopt;
    for (std::uint32_t other = 0; other < info.images.size(); ++other) {
        if (other == image || info.images[other].source != resource.source) continue;
        const auto& alias = info.images[other];
        if (alias.dimensionQueryOnly) continue;
        if (alias.resourceClass != ImageResourceClass::Storage || alias.read || alias.atomic) return std::nullopt;
    }
    Prover prover(threads);
    if (prover.Lanes() == 0u || prover.Lanes() > MaxLanes) return std::nullopt;
    const auto& memoryInfo = program.Resources().memoryInfo;
    std::vector<Store> stores;
    for (const auto* block : program.BlockOrder()) {
        for (const auto* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::ImageWrite) continue;
            const auto flags = inst->Flags<MemoryFlags>();
            if (flags.index >= memoryInfo.size()) return std::nullopt;
            const auto& memory = memoryInfo[flags.index];
            if (memory.kind != ResourceKind::Image || memory.resource != image || memory.planningOnly) continue;
            if (memory.imageDimension != RdnaImageDimension::Dim2D || memory.imageHasMip || memory.imageByElements != 0u || memory.imagePacked || memory.imageR128 || inst->ArgumentCount() != 4) return std::nullopt;
            const auto* address = inst->Argument(1)->Resolve();
            if (address->Opcode() != IrOpcode::MakeImageAddress) return std::nullopt;
            Store store;
            for (std::uint32_t component = 0; component < 2; ++component) {
                const auto layout = GetRdnaImageAddressComponentLayout(memory.imageSampleFlags, component);
                const auto operand = layout.bitOffset / 32u;
                if (operand >= address->ArgumentCount()) return std::nullopt;
                std::optional<Form> form;
                if (layout.bitWidth == 16u) form = prover.Half(address->Argument(operand), layout.bitOffset & 31u);
                else if (layout.bitWidth == 32u && (layout.bitOffset & 31u) == 0u) form = prover.Evaluate(address->Argument(operand));
                if (!form) return std::nullopt;
                store.coordinate[component] = std::move(*form);
            }
            store.atoms.active.assign(prover.Lanes(), true);
            if (!prover.Collect(inst->Argument(3), store.atoms)) return std::nullopt;
            if (!ReachedOnce(program, *block, inst->Argument(3))) return std::nullopt;
            stores.push_back(std::move(store));
        }
    }
    if (stores.empty()) return std::nullopt;
    ImageOverwrite proof;
    const auto& first = stores.front();
    for (std::size_t index = 0; index < 2; ++index) {
        const auto axis = first.coordinate[index].GroupAxis();
        if (!axis.has_value() || first.coordinate[index].group[*axis] <= 0 || first.coordinate[index].group[*axis] > 4096) return std::nullopt;
        proof.axes[index].groupAxis = *axis;
        proof.axes[index].tile = static_cast<std::uint32_t>(first.coordinate[index].group[*axis]);
        proof.axes[index].offsetRegister = first.coordinate[index].uniform;
    }
    if (proof.axes[0].groupAxis == proof.axes[1].groupAxis) return std::nullopt;
    const auto tileX = proof.axes[0].tile;
    const auto tileY = proof.axes[1].tile;
    if (std::size_t{tileX} * tileY > MaxTileCells) return std::nullopt;
    std::vector<bool> covered(std::size_t{tileX} * tileY, false);
    bool boundsSet = false;
    for (const auto& store : stores) {
        for (std::size_t index = 0; index < 2; ++index) {
            const auto& coordinate = store.coordinate[index];
            if (coordinate.group != first.coordinate[index].group || coordinate.uniform != first.coordinate[index].uniform) return std::nullopt;
        }
        proof.fullGroups |= store.atoms.fullGroups;
        std::array<std::uint32_t, 2> reg{NoRegister, NoRegister};
        std::array<std::uint32_t, 2> immediate{0u, 0u};
        for (const auto& bound : store.atoms.bounds) {
            std::size_t index = 2;
            for (std::size_t candidate = 0; candidate < 2; ++candidate) {
                if (proof.axes[candidate].groupAxis == bound.axis) index = candidate;
            }
            if (index == 2 || bound.scale != static_cast<std::int64_t>(proof.axes[index].tile)) return std::nullopt;
            for (std::size_t lane = 0; lane < prover.Lanes(); ++lane) {
                if (store.atoms.active[lane] && (bound.lane[lane] < 0 || bound.lane[lane] > store.coordinate[index].lane[lane])) return std::nullopt;
            }
            if (bound.reg != NoRegister) {
                if (reg[index] != NoRegister && reg[index] != bound.reg) return std::nullopt;
                reg[index] = bound.reg;
            } else {
                immediate[index] = immediate[index] == 0u ? bound.immediate : std::min(immediate[index], bound.immediate);
            }
        }
        for (std::size_t index = 0; index < 2; ++index) {
            if (!boundsSet) {
                proof.axes[index].boundRegister = reg[index];
                proof.axes[index].boundImmediate = immediate[index];
            } else if (proof.axes[index].boundRegister != reg[index] || proof.axes[index].boundImmediate != immediate[index]) {
                return std::nullopt;
            }
        }
        boundsSet = true;
        for (std::size_t lane = 0; lane < prover.Lanes(); ++lane) {
            if (!store.atoms.active[lane]) continue;
            const auto x = store.coordinate[0].lane[lane];
            const auto y = store.coordinate[1].lane[lane];
            if (x < 0 || y < 0 || x >= (std::int64_t{1} << 30) || y >= (std::int64_t{1} << 30)) return std::nullopt;
            proof.axes[0].maxLane = std::max(proof.axes[0].maxLane, static_cast<std::uint32_t>(x));
            proof.axes[1].maxLane = std::max(proof.axes[1].maxLane, static_cast<std::uint32_t>(y));
            if (x < tileX && y < tileY) covered[static_cast<std::size_t>(y) * tileX + static_cast<std::size_t>(x)] = true;
        }
    }
    if (!std::all_of(covered.begin(), covered.end(), [](bool cell) { return cell; })) return std::nullopt;
    if (prover.ranges.size() > proof.ranges.size()) return std::nullopt;
    std::copy(prover.ranges.begin(), prover.ranges.end(), proof.ranges.begin());
    proof.proved = true;
    return proof;
}

}

void ProveImageOverwrites(IrProgram& program, const std::array<std::uint32_t, 3>& threads) {
    auto& info = program.Resources().info;
    for (auto& image : info.images) image.overwrite = {};
    if (info.usesDma || program.BlockOrder().empty() || program.BlockOrder().front() == nullptr || AnyCycle(program)) return;
    for (std::uint32_t image = 0; image < info.images.size(); ++image) {
        if (const auto proof = ProveImage(program, image, threads)) info.images[image].overwrite = *proof;
    }
}

}
