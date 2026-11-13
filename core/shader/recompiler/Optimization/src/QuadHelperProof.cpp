#include "Optimization/QuadHelperProof.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::size_t StepLimit = 1u << 21u;
constexpr std::size_t PathLimit = 1u << 14u;
constexpr int Absent = -1;
constexpr int Console = 0;
constexpr int Lowered = 1;

struct Refusal {
    std::string text;
};

std::string Describe(const IrValue* value) {
    char text[128];
    std::snprintf(text, sizeof(text), "%%%u (%s) in block $%u", value->Id(), std::string(IrOpcodeName(value->Opcode())).c_str(), value->Parent() == nullptr ? 0u : value->Parent()->Id());
    return text;
}

bool NoOp(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::Reference:
    case IrOpcode::ReferenceU32:
    case IrOpcode::Waitcnt:
    case IrOpcode::InstPrefetch:
    case IrOpcode::ControlNop:
    case IrOpcode::Branch:
    case IrOpcode::BranchConditional:
    case IrOpcode::Loop:
    case IrOpcode::LoopMerge:
        return true;
    default:
        return false;
    }
}

bool IsSelect(IrOpcode opcode) {
    return opcode == IrOpcode::SelectU32 || opcode == IrOpcode::SelectU1 || opcode == IrOpcode::SelectF32 || opcode == IrOpcode::Select;
}

bool CrossLane(const IrProgram& program, const IrValue& inst) {
    switch (inst.Opcode()) {
    case IrOpcode::ReadFirstLane:
    case IrOpcode::ReadLane:
    case IrOpcode::WriteLane:
    case IrOpcode::Ballot:
    case IrOpcode::DppMoveU32:
    case IrOpcode::DppUpdateU32:
    case IrOpcode::SwizzleU32:
    case IrOpcode::PermuteU32:
    case IrOpcode::BpermuteU32:
    case IrOpcode::Permlane16U32:
    case IrOpcode::ImageQueryLod:
    case IrOpcode::ImageSampleImplicitLod:
    case IrOpcode::FPInterpolateF32:
    case IrOpcode::FPInterpolateF16:
    case IrOpcode::InterpolateHostP1:
    case IrOpcode::InterpolateHostP2:
    case IrOpcode::GetInterpolationParameter:
    case IrOpcode::GetInterpolationParameterF16:
    case IrOpcode::DataAppend:
    case IrOpcode::DataConsume:
    case IrOpcode::DataOrderedCount:
    case IrOpcode::DataOrderedSwap:
        return true;
    case IrOpcode::ImageSampleRaw:
    case IrOpcode::ImageGatherRaw: {
        const auto index = inst.Flags<MemoryFlags>().index;
        if (index >= program.Resources().memoryInfo.size()) return true;
        return (program.Resources().memoryInfo[index].imageSampleFlags & (RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLod | RdnaImageSampleFlagLevelZero)) == 0u;
    }
    default:
        return false;
    }
}

bool Pure(IrOpcode opcode) {
    if (IrOpcodeHasSideEffects(opcode)) return false;
    if (BufferAccessOf(opcode) != BufferAccess::None || SharedAccessOf(opcode) != SharedAccess::None) return false;
    if (AddressOpcodeInfoOf(opcode).access != AddressAccess::None || ImageOpcodeInfoOf(opcode).access != ImageAccess::None) return false;
    switch (opcode) {
    case IrOpcode::GetUserData:
    case IrOpcode::GetBuiltin:
    case IrOpcode::GetBufferResource:
    case IrOpcode::GetImageResource:
    case IrOpcode::GetSamplerResource:
    case IrOpcode::GetAddressResource:
    case IrOpcode::GetShaderBase:
    case IrOpcode::TessellationBase:
    case IrOpcode::LaneId:
    case IrOpcode::MakeImageAddress:
        return true;
    case IrOpcode::Phi:
    case IrOpcode::Identity:
    case IrOpcode::Void:
    case IrOpcode::ShaderClock:
    case IrOpcode::RealtimeClock:
        return false;
    default:
        break;
    }
    const std::string_view name = IrOpcodeName(opcode);
    for (const std::string_view prefix : {"Get", "Set", "Load", "Store", "Read", "Write", "Image", "Data", "Mesh", "Undef", "Buffer", "Shared", "Address", "Interpolate", "Emit"}) {
        if (name.substr(0, prefix.size()) == prefix) return false;
    }
    return true;
}

bool WaveMaskBranch(BranchCondition condition) {
    return condition == BranchCondition::ExecZero || condition == BranchCondition::ExecNonZero || condition == BranchCondition::VccZero || condition == BranchCondition::VccNonZero;
}

class Prover {
public:
    Prover(const IrProgram& program, const IrValue& wqm, std::span<IrValue* const> reads, const IrValue& predicate)
        : program(program), wqm(wqm), reads(reads.begin(), reads.end()), predicate(predicate) {}

    std::string Run() {
        try {
            prepare();
            State start;
            IrBlock* block = wqm.Parent();
            const auto& instructions = body.at(block);
            const auto found = std::find(instructions.begin(), instructions.end(), &wqm);
            if (found == instructions.end()) throw Refusal{"the whole-quad mask is not in its block"};
            const auto next = static_cast<std::size_t>(found - instructions.begin()) + 1u;
            for (auto& at : start.at) at = Position{block, nullptr, next, true, false};
            if (!addFact(start, token(start, &predicate, Console), false)) return {};
            pending.push_back(std::move(start));
            while (!pending.empty()) {
                State state = std::move(pending.back());
                pending.pop_back();
                explore(state);
            }
        } catch (const Refusal& refusal) {
            return refusal.text;
        }
        return {};
    }

private:
    enum class Kind : std::uint8_t { Constant, Leaf, Fresh, Operation };

    struct Node {
        Kind kind = Kind::Constant;
        IrOpcode opcode = IrOpcode::Void;
        IrType type = IrType::Void;
        std::uint64_t bits = 0;
        const IrValue* source = nullptr;
        std::vector<int> arguments;
        std::vector<const IrValue*> sources;
    };

    struct Position {
        IrBlock* block = nullptr;
        IrBlock* from = nullptr;
        std::size_t next = 0;
        bool entered = false;
        bool done = false;
    };

    struct State {
        std::unordered_map<const IrValue*, std::array<int, 2>> values;
        std::map<int, bool> facts;
        std::array<Position, 2> at;
        std::array<bool, 2> helper{true, true};
        std::vector<std::string> trail;
    };

    struct Choice {
        IrBlock* target = nullptr;
        int token = Absent;
        bool value = false;
    };

    struct Visit {
        std::vector<std::uint32_t> differing;
        std::vector<std::tuple<std::uint32_t, int, int>> constants;
        std::array<bool, 2> helper{};
    };

    void prepare() {
        const auto& blocks = program.BlockOrder();
        if (blocks.empty()) throw Refusal{"the program has no blocks"};
        const auto& infos = program.Metadata().blockInfo;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            IrBlock* block = blocks[index];
            auto& instructions = body[block];
            std::size_t phis = 0;
            for (IrValue* inst : block->Instructions()) {
                if (inst->IsIdentity()) continue;
                instructions.push_back(inst);
                if (inst->IsPhi()) {
                    if (phis != instructions.size() - 1u) throw Refusal{"a phi follows another instruction in block $" + std::to_string(block->Id())};
                    ++phis;
                }
            }
            phiCount[block] = phis;
            if (infos.size() == blocks.size()) {
                infoIndex[block] = index;
                byId[infos[index].id] = block;
            }
        }
        entry = blocks.front();
        std::vector<IrBlock*> postorder;
        std::unordered_set<const IrBlock*> seen{entry};
        std::vector<std::pair<IrBlock*, std::size_t>> stack{{entry, 0u}};
        while (!stack.empty()) {
            auto& [block, next] = stack.back();
            if (next == block->Successors().size()) {
                postorder.push_back(block);
                stack.pop_back();
                continue;
            }
            IrBlock* successor = block->Successors()[next++];
            if (seen.insert(successor).second) stack.emplace_back(successor, 0u);
        }
        order.assign(postorder.rbegin(), postorder.rend());
        for (std::size_t index = 0; index < order.size(); ++index) rpo[order[index]] = index;
        idom[entry] = entry;
        for (bool changed = true; changed;) {
            changed = false;
            for (IrBlock* block : order) {
                if (block == entry) continue;
                const IrBlock* chosen = nullptr;
                for (const IrBlock* predecessor : block->Predecessors()) {
                    if (!idom.contains(predecessor)) continue;
                    chosen = chosen == nullptr ? predecessor : intersect(chosen, predecessor);
                }
                if (chosen != nullptr && idom[block] != chosen) {
                    idom[block] = chosen;
                    changed = true;
                }
            }
        }
    }

    const IrBlock* intersect(const IrBlock* left, const IrBlock* right) const {
        while (left != right) {
            while (rpo.at(left) > rpo.at(right)) left = idom.at(left);
            while (rpo.at(right) > rpo.at(left)) right = idom.at(right);
        }
        return left;
    }

    bool strictlyDominates(const IrBlock* dominator, const IrBlock* block) const {
        if (dominator == nullptr || block == nullptr || dominator == block || !idom.contains(block) || !idom.contains(dominator)) return false;
        for (const IrBlock* current = block; current != entry;) {
            current = idom.at(current);
            if (current == dominator) return true;
        }
        return false;
    }

    int intern(std::vector<std::uint64_t> key, Node node) {
        const auto [found, inserted] = interned.try_emplace(std::move(key), static_cast<int>(nodes.size()));
        if (inserted) nodes.push_back(std::move(node));
        return found->second;
    }

    int constant(IrType type, std::uint64_t bits) {
        if (type == IrType::Bool) bits = bits != 0u ? 1u : 0u;
        Node node;
        node.kind = Kind::Constant;
        node.type = type;
        node.bits = bits;
        return intern({0u, static_cast<std::uint64_t>(type), bits}, std::move(node));
    }

    int boolean(bool value) {
        return constant(IrType::Bool, value ? 1u : 0u);
    }

    int leaf(const IrValue* value) {
        Node node;
        node.kind = Kind::Leaf;
        node.type = value->Type();
        node.source = value;
        return intern({1u, reinterpret_cast<std::uintptr_t>(value)}, std::move(node));
    }

    int fresh(const IrValue* value) {
        Node node;
        node.kind = Kind::Fresh;
        node.type = value->Type();
        node.source = value;
        nodes.push_back(std::move(node));
        return static_cast<int>(nodes.size()) - 1;
    }

    int operation(IrOpcode opcode, IrType type, std::uint64_t flags, const std::vector<int>& arguments) {
        std::vector<std::uint64_t> key{3u, static_cast<std::uint64_t>(opcode), static_cast<std::uint64_t>(type), flags};
        for (const int argument : arguments) key.push_back(static_cast<std::uint64_t>(argument));
        Node node;
        node.kind = Kind::Operation;
        node.opcode = opcode;
        node.type = type;
        node.bits = flags;
        node.arguments = arguments;
        return intern(std::move(key), std::move(node));
    }

    std::optional<std::uint64_t> constantOf(int token) const {
        const Node& node = nodes[token];
        if (node.kind != Kind::Constant) return std::nullopt;
        return node.bits;
    }

    bool isOperation(int token, IrOpcode opcode) const {
        return nodes[token].kind == Kind::Operation && nodes[token].opcode == opcode;
    }

    void name(int token, const IrValue* value) {
        auto& sources = nodes[token].sources;
        if (std::find(sources.begin(), sources.end(), value) == sources.end()) sources.push_back(value);
    }

    int immediate(const IrValue* value) {
        if (value->Type() == IrType::Bool) return boolean(value->ImmediateBool());
        return constant(value->Type(), value->ImmediateU64());
    }

    int before(const IrValue* raw, std::uint32_t depth = 0) {
        const IrValue* value = raw->Resolve();
        if (value->HasImmediate()) return immediate(value);
        if (const auto found = prefix.find(value); found != prefix.end()) return found->second;
        int result = Absent;
        if (Pure(value->Opcode()) && depth < 256u) {
            std::vector<int> arguments;
            arguments.reserve(value->ArgumentCount());
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) arguments.push_back(before(value->Argument(index), depth + 1u));
            const auto folded = fold(value->Opcode(), value->Type(), arguments);
            result = folded ? *folded : operation(value->Opcode(), value->Type(), value->Flags<std::uint64_t>(), arguments);
        } else {
            result = leaf(value);
        }
        prefix.emplace(value, result);
        name(result, value);
        return result;
    }

    std::optional<int> current(const State& state, const IrValue* raw, int world) {
        const IrValue* value = raw->Resolve();
        if (value->HasImmediate()) return immediate(value);
        if (const auto found = state.values.find(value); found != state.values.end()) {
            if (found->second[world] == Absent) return std::nullopt;
            return found->second[world];
        }
        return before(value);
    }

    int token(const State& state, const IrValue* raw, int world) {
        const auto result = current(state, raw, world);
        if (!result) throw Refusal{Describe(raw->Resolve()) + " is read on a path where only the other run computed it"};
        return *result;
    }

    void assign(State& state, const IrValue* value, int world, int token) {
        auto [found, inserted] = state.values.try_emplace(value, std::array<int, 2>{Absent, Absent});
        found->second[world] = token;
        name(token, value);
    }

    std::optional<int> fold(IrOpcode opcode, IrType type, const std::vector<int>& arguments) {
        const auto argument = [&](std::size_t index) { return arguments.at(index); };
        const auto value = [&](std::size_t index) { return constantOf(arguments.at(index)); };
        const auto both = [&]() -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
            const auto left = value(0), right = value(1);
            if (!left || !right) return std::nullopt;
            return std::pair{*left, *right};
        };
        const auto u32 = [&](std::uint64_t bits) { return constant(IrType::U32, bits & 0xffffffffu); };
        if (arguments.size() == 1u) {
            switch (opcode) {
            case IrOpcode::LogicalNot:
                if (const auto known = value(0)) return boolean(*known == 0u);
                if (isOperation(argument(0), IrOpcode::LogicalNot)) return nodes[argument(0)].arguments[0];
                return std::nullopt;
            case IrOpcode::BitwiseNot32:
                if (const auto known = value(0)) return u32(~*known);
                return std::nullopt;
            default:
                return std::nullopt;
            }
        }
        if (arguments.size() == 3u && IsSelect(opcode)) {
            if (const auto condition = value(0)) return *condition != 0u ? argument(1) : argument(2);
            if (argument(1) == argument(2)) return argument(1);
            if (opcode == IrOpcode::SelectU1 && value(1) == std::optional<std::uint64_t>{1u} && value(2) == std::optional<std::uint64_t>{0u}) return argument(0);
            return std::nullopt;
        }
        if (arguments.size() != 2u) return std::nullopt;
        switch (opcode) {
        case IrOpcode::LogicalAnd:
            for (const auto [fixed, other] : {std::pair{0u, 1u}, std::pair{1u, 0u}}) {
                if (const auto known = value(fixed)) return *known != 0u ? argument(other) : boolean(false);
            }
            if (argument(0) == argument(1)) return argument(0);
            return std::nullopt;
        case IrOpcode::LogicalOr:
            for (const auto [fixed, other] : {std::pair{0u, 1u}, std::pair{1u, 0u}}) {
                if (const auto known = value(fixed)) return *known != 0u ? boolean(true) : argument(other);
            }
            if (argument(0) == argument(1)) return argument(0);
            return std::nullopt;
        case IrOpcode::LogicalXor:
            if (const auto known = both()) return boolean((known->first != 0u) != (known->second != 0u));
            if (argument(0) == argument(1)) return boolean(false);
            return std::nullopt;
        case IrOpcode::IEqual32:
        case IrOpcode::IEqual64:
            if (const auto known = both()) return boolean(known->first == known->second);
            if (argument(0) == argument(1)) return boolean(true);
            return std::nullopt;
        case IrOpcode::INotEqual32:
        case IrOpcode::INotEqual64:
            if (const auto known = both()) return boolean(known->first != known->second);
            if (argument(0) == argument(1)) return boolean(false);
            return std::nullopt;
        case IrOpcode::ULessThan32:
            if (const auto known = both()) return boolean(static_cast<std::uint32_t>(known->first) < static_cast<std::uint32_t>(known->second));
            return std::nullopt;
        case IrOpcode::UGreaterThan32:
            if (const auto known = both()) return boolean(static_cast<std::uint32_t>(known->first) > static_cast<std::uint32_t>(known->second));
            return std::nullopt;
        case IrOpcode::ULessThanEqual32:
            if (const auto known = both()) return boolean(static_cast<std::uint32_t>(known->first) <= static_cast<std::uint32_t>(known->second));
            return std::nullopt;
        case IrOpcode::UGreaterThanEqual32:
            if (const auto known = both()) return boolean(static_cast<std::uint32_t>(known->first) >= static_cast<std::uint32_t>(known->second));
            return std::nullopt;
        case IrOpcode::BitwiseAnd32:
            if (const auto known = both()) return u32(known->first & known->second);
            return std::nullopt;
        case IrOpcode::BitwiseOr32:
            if (const auto known = both()) return u32(known->first | known->second);
            return std::nullopt;
        case IrOpcode::BitwiseXor32:
            if (const auto known = both()) return u32(known->first ^ known->second);
            return std::nullopt;
        case IrOpcode::IAdd32:
            if (const auto known = both()) return u32(known->first + known->second);
            return std::nullopt;
        case IrOpcode::ISub32:
            if (const auto known = both()) return u32(known->first - known->second);
            return std::nullopt;
        case IrOpcode::ShiftLeftLogical32:
            if (const auto known = both()) return u32(static_cast<std::uint32_t>(known->first) << (known->second & 31u));
            return std::nullopt;
        case IrOpcode::ShiftRightLogical32:
            if (const auto known = both()) return u32(static_cast<std::uint32_t>(known->first) >> (known->second & 31u));
            return std::nullopt;
        case IrOpcode::CompositeExtractU64:
            if (isOperation(argument(0), IrOpcode::CompositeConstructU64) && value(1)) {
                const auto& parts = nodes[argument(0)].arguments;
                if (*value(1) < parts.size()) return parts[*value(1)];
            }
            return std::nullopt;
        default:
            break;
        }
        (void)type;
        return std::nullopt;
    }

    bool factTrue(const State& state, int token) const {
        const auto found = state.facts.find(token);
        return found != state.facts.end() && found->second;
    }

    bool implies(const State& state, int x, int y, std::uint32_t depth) {
        if (x == y) return true;
        if (depth > 24u) return false;
        const Node left = nodes[x];
        const Node right = nodes[y];
        if (left.kind == Kind::Constant) return left.bits == 0u;
        if (right.kind == Kind::Constant) return right.bits != 0u;
        if (right.kind == Kind::Operation && right.opcode == IrOpcode::LogicalAnd) {
            if (std::all_of(right.arguments.begin(), right.arguments.end(), [&](int part) { return factTrue(state, part) || implies(state, x, part, depth + 1u); })) return true;
        }
        if (right.kind == Kind::Operation && right.opcode == IrOpcode::LogicalOr) {
            if (std::any_of(right.arguments.begin(), right.arguments.end(), [&](int part) { return implies(state, x, part, depth + 1u); })) return true;
        }
        if (left.kind == Kind::Operation) {
            const auto& arguments = left.arguments;
            switch (left.opcode) {
            case IrOpcode::LogicalAnd:
                return std::any_of(arguments.begin(), arguments.end(), [&](int part) { return implies(state, part, y, depth + 1u); });
            case IrOpcode::LogicalOr:
                return std::all_of(arguments.begin(), arguments.end(), [&](int part) { return implies(state, part, y, depth + 1u); });
            case IrOpcode::SelectU1:
                if (constantOf(arguments[2]) == std::optional<std::uint64_t>{0u}) return implies(state, arguments[0], y, depth + 1u) || implies(state, arguments[1], y, depth + 1u);
                return implies(state, arguments[1], y, depth + 1u) && implies(state, arguments[2], y, depth + 1u);
            default:
                return false;
            }
        }
        if (left.source != nullptr && left.source->IsPhi()) return phiImplies(state, left.source, y);
        return false;
    }

    bool phiImplies(const State& state, const IrValue* phi, int y) {
        const auto candidates = nodes[y].sources;
        for (const IrValue* value : candidates) {
            const auto now = current(state, value, Console);
            const auto other = current(state, value, Lowered);
            if (now != std::optional<int>{y} && other != std::optional<int>{y}) continue;
            if (!strictlyDominates(value->Parent(), phi->Parent())) continue;
            if (staticImplies(phi, value, {}, 0u)) return true;
        }
        return false;
    }

    bool staticImplies(const IrValue* raw, const IrValue* target, std::vector<const IrValue*> visiting, std::uint32_t depth) {
        const IrValue* value = raw->Resolve();
        if (value == target) return true;
        if (depth > 64u) return false;
        if (value->HasImmediate()) return value->Type() == IrType::Bool && !value->ImmediateBool();
        switch (value->Opcode()) {
        case IrOpcode::LogicalAnd:
            return staticImplies(value->Argument(0), target, visiting, depth + 1u) || staticImplies(value->Argument(1), target, visiting, depth + 1u);
        case IrOpcode::LogicalOr:
            return staticImplies(value->Argument(0), target, visiting, depth + 1u) && staticImplies(value->Argument(1), target, visiting, depth + 1u);
        case IrOpcode::SelectU1: {
            const IrValue* otherwise = value->Argument(2)->Resolve();
            if (otherwise->HasImmediate() && otherwise->Type() == IrType::Bool && !otherwise->ImmediateBool()) {
                return staticImplies(value->Argument(0), target, visiting, depth + 1u) || staticImplies(value->Argument(1), target, visiting, depth + 1u);
            }
            return staticImplies(value->Argument(1), target, visiting, depth + 1u) && staticImplies(value->Argument(2), target, visiting, depth + 1u);
        }
        case IrOpcode::Phi: {
            if (!strictlyDominates(target->Parent(), value->Parent())) return false;
            if (std::find(visiting.begin(), visiting.end(), value) != visiting.end()) return true;
            visiting.push_back(value);
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
                if (!staticImplies(value->Argument(index), target, visiting, depth + 1u)) return false;
            }
            return true;
        }
        default:
            return false;
        }
    }

    std::optional<bool> known(const State& state, int token) {
        if (const auto bits = constantOf(token)) return *bits != 0u;
        if (const auto found = state.facts.find(token); found != state.facts.end()) return found->second;
        for (const auto& [fact, truth] : state.facts) {
            if (truth && implies(state, fact, token, 0u)) return true;
            if (!truth && implies(state, token, fact, 0u)) return false;
        }
        return std::nullopt;
    }

    bool record(State& state, int token, bool truth) {
        if (const auto already = known(state, token)) return *already == truth;
        state.facts[token] = truth;
        const Node node = nodes[token];
        if (node.kind != Kind::Operation) return true;
        switch (node.opcode) {
        case IrOpcode::LogicalNot:
            return record(state, node.arguments[0], !truth);
        case IrOpcode::LogicalAnd:
            return !truth || (record(state, node.arguments[0], true) && record(state, node.arguments[1], true));
        case IrOpcode::LogicalOr:
            return truth || (record(state, node.arguments[0], false) && record(state, node.arguments[1], false));
        case IrOpcode::SelectU1:
            if (truth && constantOf(node.arguments[2]) == std::optional<std::uint64_t>{0u}) return record(state, node.arguments[0], true) && record(state, node.arguments[1], true);
            return true;
        default:
            return true;
        }
    }

    bool addFact(State& state, int token, bool truth) {
        if (!record(state, token, truth)) return false;
        for (const auto& [fact, value] : state.facts) {
            if (value) continue;
            for (const auto& [other, otherValue] : state.facts) {
                if (otherValue && implies(state, other, fact, 0u)) return false;
            }
        }
        return true;
    }

    bool live(const IrValue* value, const IrBlock* at) {
        const auto key = std::pair{value, at};
        if (const auto found = liveness.find(key); found != liveness.end()) return found->second;
        const IrBlock* definition = value->Parent();
        bool result = false;
        if (definition != at || value->IsPhi()) {
            std::unordered_set<const IrBlock*> blockUses;
            std::vector<std::pair<const IrBlock*, const IrBlock*>> edgeUses;
            for (const IrValue* user : value->Uses()) {
                if (user->IsPhi()) {
                    for (std::size_t index = 0; index < user->ArgumentCount(); ++index) {
                        if (user->Argument(index)->Resolve() == value) edgeUses.emplace_back(user->PhiBlock(index), user->Parent());
                    }
                } else {
                    blockUses.insert(user->Parent());
                }
            }
            std::vector<const IrBlock*> stack{at};
            std::unordered_set<const IrBlock*> seen{at};
            while (!stack.empty() && !result) {
                const IrBlock* block = stack.back();
                stack.pop_back();
                if (blockUses.contains(block)) {
                    result = true;
                    break;
                }
                for (const IrBlock* successor : block->Successors()) {
                    if (std::find(edgeUses.begin(), edgeUses.end(), std::pair{block, successor}) != edgeUses.end()) {
                        result = true;
                        break;
                    }
                    if (successor == definition) continue;
                    if (seen.insert(successor).second) stack.push_back(successor);
                }
            }
        }
        liveness.emplace(key, result);
        return result;
    }

    bool readsAhead(const IrBlock* at) {
        if (const auto found = ahead.find(at); found != ahead.end()) return found->second;
        bool result = false;
        std::vector<const IrBlock*> stack{at};
        std::unordered_set<const IrBlock*> seen{at};
        while (!stack.empty() && !result) {
            const IrBlock* block = stack.back();
            stack.pop_back();
            if (block == wqm.Parent()) continue;
            for (const IrValue* read : reads) result = result || read->Parent() == block;
            for (const IrBlock* successor : block->Successors()) {
                if (seen.insert(successor).second) stack.push_back(successor);
            }
        }
        ahead.emplace(at, result);
        return result;
    }

    bool header(const IrBlock* block) const {
        return std::any_of(block->Predecessors().begin(), block->Predecessors().end(), [&](const IrBlock* predecessor) { return rpo.contains(predecessor) && rpo.at(predecessor) >= rpo.at(block); });
    }

    bool backEdge(const IrBlock* from, const IrBlock* to) const {
        return from != nullptr && to != nullptr && rpo.contains(from) && rpo.contains(to) && rpo.at(to) <= rpo.at(from);
    }

    void enter(State& state, int world) {
        Position& at = state.at[world];
        const auto& instructions = body.at(at.block);
        const std::size_t phis = phiCount.at(at.block);
        std::vector<std::pair<const IrValue*, int>> incoming;
        for (std::size_t index = 0; index < phis; ++index) {
            const IrValue* phi = instructions[index];
            std::optional<int> chosen;
            for (std::size_t operand = 0; operand < phi->ArgumentCount(); ++operand) {
                if (phi->PhiBlock(operand) == at.from) {
                    chosen = token(state, phi->Argument(operand), world);
                    break;
                }
            }
            if (!chosen) throw Refusal{Describe(phi) + " has no value for the edge from block $" + std::to_string(at.from == nullptr ? 0u : at.from->Id())};
            incoming.emplace_back(phi, *chosen);
        }
        for (const auto& [phi, chosen] : incoming) assign(state, phi, world, chosen);
        at.entered = true;
        at.next = phis;
    }

    bool settled(State& state, const IrBlock* block) {
        if ((state.helper[Console] || state.helper[Lowered]) && readsAhead(block)) return false;
        for (const auto& [value, pair] : state.values) {
            if (pair[Console] != pair[Lowered] && live(value, block)) return false;
        }
        return true;
    }

    bool widen(State& state, const IrBlock* block) {
        Visit visit;
        visit.helper = state.helper;
        for (auto found = state.values.begin(); found != state.values.end();) {
            const IrValue* value = found->first;
            auto& pair = found->second;
            if (!live(value, block)) {
                found = state.values.erase(found);
                continue;
            }
            if (pair[Console] == pair[Lowered]) {
                if (!constantOf(pair[Console])) {
                    const int shared = fresh(value);
                    pair = {shared, shared};
                }
            } else {
                for (int& side : pair) {
                    if (side != Absent && !constantOf(side)) side = fresh(value);
                }
                visit.differing.push_back(value->Id());
            }
            if (constantOf(pair[Console]) || (pair[Lowered] != Absent && constantOf(pair[Lowered]))) visit.constants.emplace_back(value->Id(), pair[Console], pair[Lowered]);
            ++found;
        }
        state.facts.clear();
        std::sort(visit.differing.begin(), visit.differing.end());
        std::sort(visit.constants.begin(), visit.constants.end());
        auto& earlier = visits[block];
        for (const Visit& old : earlier) {
            if (old.helper != visit.helper) continue;
            if (!std::includes(old.differing.begin(), old.differing.end(), visit.differing.begin(), visit.differing.end())) continue;
            const bool constants = std::all_of(old.constants.begin(), old.constants.end(), [&](const auto& constant) { return std::binary_search(visit.constants.begin(), visit.constants.end(), constant); });
            if (constants) return false;
        }
        earlier.push_back(std::move(visit));
        return true;
    }

    int compute(const State& state, const IrValue* inst, std::vector<int> arguments) {
        for (int& argument : arguments) {
            if (nodes[argument].type != IrType::Bool || constantOf(argument)) continue;
            if (const auto truth = known(state, argument)) argument = boolean(*truth);
        }
        int result = Absent;
        if (const auto folded = fold(inst->Opcode(), inst->Type(), arguments)) {
            result = *folded;
        } else if (Pure(inst->Opcode())) {
            result = operation(inst->Opcode(), inst->Type(), inst->Flags<std::uint64_t>(), arguments);
        } else {
            result = fresh(inst);
        }
        if (inst->Type() == IrType::Bool && !constantOf(result)) {
            if (const auto truth = known(state, result)) result = boolean(*truth);
        }
        return result;
    }

    std::vector<int> operands(const State& state, const IrValue* inst, int world) {
        std::vector<int> result;
        result.reserve(inst->ArgumentCount());
        for (std::size_t index = 0; index < inst->ArgumentCount(); ++index) result.push_back(token(state, inst->Argument(index), world));
        return result;
    }

    void fork(State& state, const std::vector<std::pair<int, std::vector<bool>>>& splits) {
        std::vector<State> children{state};
        for (const auto& [condition, truths] : splits) {
            std::vector<State> next;
            for (const State& child : children) {
                for (const bool truth : truths) {
                    State copy = child;
                    if (addFact(copy, condition, truth)) next.push_back(std::move(copy));
                }
            }
            children = std::move(next);
        }
        for (State& child : children) start(std::move(child));
    }

    static std::string trace(const State& state) {
        std::string text = "; path from the mask:";
        const std::size_t first = state.trail.size() > 24u ? state.trail.size() - 24u : 0u;
        if (first != 0u) text += " ...";
        for (std::size_t index = first; index < state.trail.size(); ++index) text += " " + state.trail[index];
        return text;
    }

    void start(State state) {
        if (++paths > PathLimit) throw Refusal{"the proof needs more than " + std::to_string(PathLimit) + " paths"};
        pending.push_back(std::move(state));
    }

    bool stepTogether(State& state, const IrValue* inst) {
        const IrOpcode opcode = inst->Opcode();
        if (NoOp(opcode)) return true;
        if (inst == &wqm) state.helper = {false, false};
        if (reads.contains(inst) && (state.helper[Console] || state.helper[Lowered])) {
            for (const int world : {Console, Lowered}) assign(state, inst, world, state.helper[world] ? boolean(world == Console) : compute(state, inst, operands(state, inst, world)));
            return true;
        }
        const auto console = operands(state, inst, Console);
        const auto lowered = operands(state, inst, Lowered);
        if (IrOpcodeHasSideEffects(opcode) || CrossLane(program, *inst)) {
            if (console != lowered) {
                const bool effect = IrOpcodeHasSideEffects(opcode);
                std::size_t operand = 0;
                while (console[operand] == lowered[operand]) ++operand;
                throw Refusal{Describe(inst) + (effect ? " writes, exports or orders memory with a value that differs on a quad-helper lane" : " hands another lane a value that differs on a quad-helper lane") + " (operand " + std::to_string(operand) + ", " + Describe(inst->Argument(operand)->Resolve()) + ")" + trace(state)};
            }
            if (inst->Type() != IrType::Void) {
                const int shared = fresh(inst);
                assign(state, inst, Console, shared);
                assign(state, inst, Lowered, shared);
            }
            return true;
        }
        const int first = compute(state, inst, console);
        const int second = console == lowered ? first : compute(state, inst, lowered);
        if (first != second) {
            std::vector<std::pair<int, std::vector<bool>>> splits;
            if (IsSelect(opcode) && console[0] == lowered[0] && (console[1] == lowered[1] || console[2] == lowered[2]) && !known(state, console[0])) {
                splits.push_back({console[0], {true, false}});
            }
            if (!splits.empty()) {
                fork(state, splits);
                return false;
            }
        }
        assign(state, inst, Console, first);
        assign(state, inst, Lowered, second);
        return true;
    }

    void stepAlone(State& state, const IrValue* inst, int world) {
        const IrOpcode opcode = inst->Opcode();
        if (NoOp(opcode)) return;
        if (inst == &wqm) state.helper[world] = false;
        if (reads.contains(inst) && state.helper[world]) {
            assign(state, inst, world, boolean(world == Console));
            return;
        }
        if (IrOpcodeHasSideEffects(opcode) || CrossLane(program, *inst)) {
            throw Refusal{Describe(inst) + " runs on a quad-helper lane in only one of the two programs"};
        }
        assign(state, inst, world, compute(state, inst, operands(state, inst, world)));
    }

    std::pair<std::vector<Choice>, int> choices(const State& state, IrBlock* block, int world) {
        const auto& successors = block->Successors();
        const auto member = [&](IrBlock* target) {
            if (std::find(successors.begin(), successors.end(), target) == successors.end()) throw Refusal{"block $" + std::to_string(block->Id()) + " branches to a block that is not its successor"};
            return target;
        };
        const auto target = [&](std::uint32_t id) -> IrBlock* {
            const auto found = byId.find(id);
            return found == byId.end() ? nullptr : found->second;
        };
        const BlockInfo* info = nullptr;
        if (const auto found = infoIndex.find(block); found != infoIndex.end()) info = &program.Metadata().blockInfo[found->second];
        const auto branch = [&](IrBlock* taken, IrBlock* other, int condition, BranchCondition kind) -> std::pair<std::vector<Choice>, int> {
            const auto truth = known(state, condition);
            if (WaveMaskBranch(kind)) {
                if (kind == BranchCondition::ExecZero || kind == BranchCondition::VccZero) {
                    if (!truth) return {{Choice{taken, condition, true}, Choice{other}}, condition};
                    if (*truth) return {{Choice{taken}, Choice{other}}, condition};
                    return {{Choice{other}}, condition};
                }
                if (!truth) return {{Choice{taken}, Choice{other, condition, false}}, condition};
                if (*truth) return {{Choice{taken}}, condition};
                return {{Choice{taken}, Choice{other}}, condition};
            }
            if (!truth) return {{Choice{taken, condition, true}, Choice{other, condition, false}}, condition};
            return {{Choice{*truth ? taken : other}}, condition};
        };
        if (info != nullptr && info->terminator.kind == TerminatorKind::ConditionalBranch && info->condition != nullptr) {
            IrBlock* taken = target(info->terminator.trueBlock);
            IrBlock* other = target(info->terminator.falseBlock);
            if (taken == nullptr || other == nullptr) return {{Choice{}}, Absent};
            if (taken == other) return {{Choice{member(taken)}}, Absent};
            return branch(member(taken), member(other), token(state, info->condition, world), info->terminator.condition);
        }
        if (info != nullptr && info->terminator.kind == TerminatorKind::Branch) {
            IrBlock* taken = target(info->terminator.trueBlock);
            if (taken == nullptr) return {{Choice{}}, Absent};
            return {{Choice{member(taken)}}, Absent};
        }
        const auto& instructions = body.at(block);
        if (!instructions.empty() && instructions.back()->Opcode() == IrOpcode::BranchConditional && successors.size() == 2u) {
            return branch(successors[0], successors[1], token(state, instructions.back()->Argument(0), world), BranchCondition::ScalarInstruction);
        }
        if (successors.empty()) return {{Choice{}}, Absent};
        std::vector<Choice> all;
        for (IrBlock* successor : successors) all.push_back(Choice{successor});
        return {all, Absent};
    }

    void move(State& state, int world, IrBlock* from, IrBlock* target) {
        state.at[world] = target == nullptr ? Position{nullptr, nullptr, 0u, false, true} : Position{target, from, 0u, false, false};
    }

    bool leaveTogether(State& state) {
        IrBlock* block = state.at[Console].block;
        const auto [console, consoleCondition] = choices(state, block, Console);
        const auto [lowered, loweredCondition] = choices(state, block, Lowered);
        std::vector<std::pair<Choice, Choice>> combinations;
        if (consoleCondition == loweredCondition) {
            for (const Choice& choice : console) combinations.emplace_back(choice, choice);
        } else {
            for (const Choice& left : console) {
                for (const Choice& right : lowered) combinations.emplace_back(left, right);
            }
        }
        std::vector<State> children;
        for (const auto& [left, right] : combinations) {
            if (left.target != right.target) {
                for (IrBlock* target : {left.target, right.target}) {
                    if (target != nullptr && backEdge(block, target)) throw Refusal{"the two programs leave block $" + std::to_string(block->Id()) + " for different blocks and one of them repeats a loop before they meet again"};
                }
            }
            State child;
            if (combinations.size() == 1u) child = std::move(state);
            else child = state;
            bool feasible = true;
            for (const Choice& choice : {left, right}) {
                if (choice.token != Absent && feasible) feasible = addFact(child, choice.token, choice.value);
            }
            if (!feasible) continue;
            move(child, Console, block, left.target);
            move(child, Lowered, block, right.target);
            children.push_back(std::move(child));
        }
        if (children.size() == 1u) {
            state = std::move(children.front());
            return true;
        }
        for (State& child : children) start(std::move(child));
        return false;
    }

    bool leaveAlone(State& state, int world) {
        IrBlock* block = state.at[world].block;
        const auto [options, condition] = choices(state, block, world);
        (void)condition;
        std::vector<State> children;
        for (const Choice& choice : options) {
            if (choice.target != nullptr && backEdge(block, choice.target)) throw Refusal{"one program repeats the loop at block $" + std::to_string(choice.target->Id()) + " while the other is still on another path"};
            State child;
            if (options.size() == 1u) child = std::move(state);
            else child = state;
            if (choice.token != Absent && !addFact(child, choice.token, choice.value)) continue;
            move(child, world, block, choice.target);
            children.push_back(std::move(child));
        }
        if (children.size() == 1u) {
            state = std::move(children.front());
            return true;
        }
        for (State& child : children) start(std::move(child));
        return false;
    }

    void explore(State& state) {
        for (;;) {
            if (++steps > StepLimit) throw Refusal{"the proof needs more than " + std::to_string(StepLimit) + " steps"};
            Position& console = state.at[Console];
            Position& lowered = state.at[Lowered];
            if (console.done && lowered.done) return;
            const bool together = !console.done && !lowered.done && console.block == lowered.block && console.entered == lowered.entered && (!console.entered || console.next == lowered.next);
            if (together) {
                IrBlock* block = console.block;
                if (!console.entered) {
                    state.trail.push_back("$" + std::to_string(block->Id()));
                    enter(state, Console);
                    enter(state, Lowered);
                    if (header(block) && backEdge(console.from, block) && backEdge(lowered.from, block) && !widen(state, block)) return;
                    if (settled(state, block)) return;
                    continue;
                }
                const auto& instructions = body.at(block);
                if (console.next < instructions.size()) {
                    if (!stepTogether(state, instructions[console.next])) return;
                    ++console.next;
                    ++lowered.next;
                    continue;
                }
                if (!leaveTogether(state)) return;
                continue;
            }
            int world = Console;
            if (console.done) world = Lowered;
            else if (lowered.done) world = Console;
            else if (console.entered) world = Console;
            else if (lowered.entered) world = Lowered;
            else world = rpo.at(console.block) <= rpo.at(lowered.block) ? Console : Lowered;
            Position& at = state.at[world];
            if (!at.entered) {
                state.trail.push_back((world == Console ? "c$" : "l$") + std::to_string(at.block->Id()));
                enter(state, world);
                continue;
            }
            const auto& instructions = body.at(at.block);
            if (at.next < instructions.size()) {
                stepAlone(state, instructions[at.next], world);
                ++at.next;
                continue;
            }
            if (!leaveAlone(state, world)) return;
        }
    }

    const IrProgram& program;
    const IrValue& wqm;
    std::unordered_set<const IrValue*> reads;
    const IrValue& predicate;

    std::unordered_map<const IrBlock*, std::vector<IrValue*>> body;
    std::unordered_map<const IrBlock*, std::size_t> phiCount;
    std::unordered_map<const IrBlock*, std::size_t> infoIndex;
    std::unordered_map<std::uint32_t, IrBlock*> byId;
    IrBlock* entry = nullptr;
    std::vector<IrBlock*> order;
    std::unordered_map<const IrBlock*, std::size_t> rpo;
    std::unordered_map<const IrBlock*, const IrBlock*> idom;

    std::vector<Node> nodes;
    std::map<std::vector<std::uint64_t>, int> interned;
    std::unordered_map<const IrValue*, int> prefix;
    std::map<std::pair<const IrValue*, const IrBlock*>, bool> liveness;
    std::unordered_map<const IrBlock*, bool> ahead;
    std::unordered_map<const IrBlock*, std::vector<Visit>> visits;

    std::vector<State> pending;
    std::size_t steps = 0;
    std::size_t paths = 0;
};

}

std::string ProveQuadHelpersUnobserved(const IrProgram& program, const IrValue& wqm, std::span<IrValue* const> reads, const IrValue& predicate) {
    return Prover(program, wqm, reads, predicate).Run();
}

}
