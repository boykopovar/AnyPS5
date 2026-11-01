#include "Optimization/MaskedSelectEliminator.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t DepthLimit = 8;

bool isSelect(IrOpcode opcode) {
    return opcode == IrOpcode::SelectU1 || opcode == IrOpcode::SelectU32 || opcode == IrOpcode::SelectF32;
}

bool isLaneLocal(IrOpcode opcode) {
    if (IrOpcodeHasSideEffects(opcode)) return false;
    switch (opcode) {
        case IrOpcode::Identity:
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32:
        case IrOpcode::IMul32:
        case IrOpcode::SelectU1:
        case IrOpcode::SelectU32:
        case IrOpcode::SelectF32:
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
        case IrOpcode::ConvertF32F64:
        case IrOpcode::ConvertF64F32:
        case IrOpcode::ConvertF64S32:
        case IrOpcode::ConvertF64U32:
        case IrOpcode::ConvertS32F64:
        case IrOpcode::ConvertU32F64:
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
        case IrOpcode::FPAdd16x2:
        case IrOpcode::FPMul16x2:
        case IrOpcode::BitFieldInsert:
        case IrOpcode::BitFieldUExtract:
        case IrOpcode::BitFieldSExtract:
        case IrOpcode::IAdd64:
        case IrOpcode::IAddCarry32:
        case IrOpcode::ISub64:
        case IrOpcode::IMul64:
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
        case IrOpcode::FPAbs32:
        case IrOpcode::FPNeg32:
        case IrOpcode::FPSaturate32:
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
        case IrOpcode::MakeImageAddress:
        case IrOpcode::ImageQueryDimensions:
            return true;
        default:
            return false;
    }
}

bool readsOnlyWhereActive(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::LoadBufferU8:
        case IrOpcode::LoadBufferU16:
        case IrOpcode::LoadBufferU32:
        case IrOpcode::LoadBufferU32x2:
        case IrOpcode::LoadBufferU32x3:
        case IrOpcode::LoadBufferU32x4:
        case IrOpcode::StoreBufferU8:
        case IrOpcode::StoreBufferU16:
        case IrOpcode::StoreBufferU32:
        case IrOpcode::StoreBufferU32x2:
        case IrOpcode::StoreBufferU32x3:
        case IrOpcode::StoreBufferU32x4:
        case IrOpcode::LoadSharedU8:
        case IrOpcode::LoadSharedU16:
        case IrOpcode::LoadSharedU32:
        case IrOpcode::LoadSharedU32x2:
        case IrOpcode::LoadSharedU32x3:
        case IrOpcode::LoadSharedU32x4:
        case IrOpcode::WriteSharedU8:
        case IrOpcode::WriteSharedU16:
        case IrOpcode::WriteSharedU32:
        case IrOpcode::WriteSharedU32x2:
        case IrOpcode::WriteSharedU32x3:
        case IrOpcode::WriteSharedU32x4:
        case IrOpcode::SetAttribute:
        case IrOpcode::ImageRead:
        case IrOpcode::ImageWrite:
            return true;
        default:
            return false;
    }
}

bool isExplicitLodSample(const IrProgram& program, const IrValue& value) {
    if (value.Opcode() != IrOpcode::ImageSampleRaw && value.Opcode() != IrOpcode::ImageGatherRaw) return false;
    if (program.Resources().stage != IrShaderStage::Pixel) return true;
    const auto index = value.Flags<MemoryFlags>().index;
    if (index >= program.Resources().memoryInfo.size()) return false;
    return (program.Resources().memoryInfo[index].imageSampleFlags & (RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLod | RdnaImageSampleFlagLevelZero)) != 0u;
}

bool isImmediate(const IrValue* value, std::uint32_t immediate) {
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == immediate;
}

const IrValue* otherArgument(const IrValue* value, std::uint32_t immediate) {
    if (value->ArgumentCount() != 2u) return nullptr;
    const IrValue* left = value->Argument(0)->Resolve();
    const IrValue* right = value->Argument(1)->Resolve();
    if (isImmediate(right, immediate)) return left;
    if (isImmediate(left, immediate)) return right;
    return nullptr;
}

struct ThreadBit {
    const IrValue* low = nullptr;
    const IrValue* high = nullptr;
    const IrValue* word = nullptr;
};

bool isLane(const IrValue* value) {
    return value->Resolve()->Opcode() == IrOpcode::LaneId;
}

std::optional<ThreadBit> threadBit(const IrValue* value, std::uint32_t waveSize) {
    value = value->Resolve();
    if (value->Opcode() != IrOpcode::INotEqual32) return std::nullopt;
    const IrValue* bit = otherArgument(value, 0u);
    if (bit == nullptr || bit->Opcode() != IrOpcode::BitwiseAnd32) return std::nullopt;
    const IrValue* shifted = otherArgument(bit, 1u);
    if (shifted == nullptr || shifted->Opcode() != IrOpcode::ShiftRightLogical32) return std::nullopt;
    const IrValue* amount = shifted->Argument(1)->Resolve();
    if (amount->Opcode() != IrOpcode::BitwiseAnd32) return std::nullopt;
    const IrValue* lane = otherArgument(amount, 31u);
    if (lane == nullptr || !isLane(lane)) return std::nullopt;
    const IrValue* word = shifted->Argument(0)->Resolve();
    if (waveSize == 32u || word->HasImmediate()) return ThreadBit{word, word, word};
    if (word->Opcode() != IrOpcode::SelectU32) return std::nullopt;
    const IrValue* half = word->Argument(0)->Resolve();
    if (half->Opcode() != IrOpcode::ULessThan32 || !isLane(half->Argument(0)) || !isImmediate(half->Argument(1)->Resolve(), 32u)) return std::nullopt;
    return ThreadBit{word->Argument(1)->Resolve(), word->Argument(2)->Resolve(), word};
}

bool alwaysTrue(const IrValue* value, std::uint32_t waveSize, std::uint32_t depth = 0) {
    value = value->Resolve();
    if (value->HasImmediate()) return value->Type() == IrType::Bool && value->ImmediateBool();
    if (depth > DepthLimit) return false;
    if (value->Opcode() == IrOpcode::LogicalAnd) return alwaysTrue(value->Argument(0), waveSize, depth + 1u) && alwaysTrue(value->Argument(1), waveSize, depth + 1u);
    const auto bit = threadBit(value, waveSize);
    return bit && isImmediate(bit->low, 0xffffffffu) && isImmediate(bit->high, 0xffffffffu);
}

using BlockSet = std::unordered_set<const IrBlock*>;

BlockSet uniformEntryBlocks(const IrProgram& program) {
    BlockSet result;
    if (program.BlockOrder().empty()) return result;
    const IrBlock* block = program.BlockOrder().front();
    while (block->Predecessors().empty() || (block->Predecessors().size() == 1u && result.contains(block->Predecessors().front()))) {
        if (!result.insert(block).second || block->Successors().size() != 1u) break;
        block = block->Successors().front();
    }
    return result;
}

bool sameCondition(const IrValue* condition, const IrValue* predicate, std::uint32_t depth) {
    condition = condition->Resolve();
    predicate = predicate->Resolve();
    if (condition == predicate || (predicate->HasImmediate() && predicate->Type() == IrType::Bool && predicate->ImmediateBool())) return true;
    if (depth > DepthLimit) return false;
    return condition->Opcode() == IrOpcode::LogicalAnd && (sameCondition(condition->Argument(0), predicate, depth + 1u) || sameCondition(condition->Argument(1), predicate, depth + 1u));
}

bool withinBallotWord(const IrValue* condition, const IrValue* word, std::uint32_t half, const BlockSet& uniform, std::uint32_t depth) {
    if (word->Opcode() != IrOpcode::CompositeExtractU32x4 || !isImmediate(word->Argument(1)->Resolve(), half)) return false;
    const IrValue* ballot = word->Argument(0)->Resolve();
    return ballot->Opcode() == IrOpcode::Ballot && uniform.contains(ballot->Parent()) && sameCondition(condition, ballot->Argument(0), depth + 1u);
}

bool withinWords(const IrValue* condition, const IrValue* low, const IrValue* high, std::uint32_t waveSize, const BlockSet& uniform, std::uint32_t depth) {
    if (depth > DepthLimit) return false;
    if (low->Opcode() == IrOpcode::CompositeExtractU32x4) {
        return withinBallotWord(condition, low, 0u, uniform, depth) && (waveSize == 32u || withinBallotWord(condition, high, 1u, uniform, depth));
    }
    if (low->Opcode() != IrOpcode::BitwiseAnd32) return false;
    const IrValue* lowLeft = low->Argument(0)->Resolve();
    const IrValue* lowRight = low->Argument(1)->Resolve();
    if (waveSize == 32u) return withinWords(condition, lowLeft, high, waveSize, uniform, depth + 1u) && withinWords(condition, lowRight, high, waveSize, uniform, depth + 1u);
    if (high->Opcode() != IrOpcode::BitwiseAnd32) return false;
    const IrValue* highLeft = high->Argument(0)->Resolve();
    const IrValue* highRight = high->Argument(1)->Resolve();
    return (withinWords(condition, lowLeft, highLeft, waveSize, uniform, depth + 1u) && withinWords(condition, lowRight, highRight, waveSize, uniform, depth + 1u)) ||
           (withinWords(condition, lowLeft, highRight, waveSize, uniform, depth + 1u) && withinWords(condition, lowRight, highLeft, waveSize, uniform, depth + 1u));
}

const IrValue* wholeQuadSource(const IrValue* mask, std::uint32_t waveSize) {
    const auto bit = threadBit(mask, waveSize);
    if (!bit || bit->low->Opcode() != IrOpcode::CompositeExtractU64 || !isImmediate(bit->low->Argument(1)->Resolve(), 0u)) return nullptr;
    const IrValue* wqm = bit->low->Argument(0)->Resolve();
    if (waveSize != 32u && (bit->high->Opcode() != IrOpcode::CompositeExtractU64 || bit->high->Argument(0)->Resolve() != wqm || !isImmediate(bit->high->Argument(1)->Resolve(), 1u))) return nullptr;
    if (wqm->Opcode() != IrOpcode::WqmU64) return nullptr;
    const IrValue* source = wqm->Argument(0)->Resolve();
    return source->Opcode() == IrOpcode::CompositeConstructU64 ? source : nullptr;
}

enum class Region : std::uint8_t { Unreached, Once, Loop };

Region regionOf(const std::vector<Region>& regions, const IrBlock* block) {
    return block != nullptr && block->Id() < regions.size() ? regions[block->Id()] : Region::Unreached;
}

bool followable(const std::vector<Region>& regions, const IrBlock* block, bool acrossLoops) {
    const Region region = regionOf(regions, block);
    return region == Region::Once || (acrossLoops && region == Region::Loop);
}

std::vector<Region> blockRegions(const IrProgram& program) {
    const auto& order = program.BlockOrder();
    if (order.empty()) return {};
    std::unordered_map<const IrBlock*, int> state;
    std::unordered_set<const IrBlock*> looping;
    std::vector<std::pair<const IrBlock*, std::size_t>> stack;
    state[order.front()] = 1;
    stack.emplace_back(order.front(), 0u);
    while (!stack.empty()) {
        auto& [block, next] = stack.back();
        if (next == block->Successors().size()) {
            state[block] = 2;
            stack.pop_back();
            continue;
        }
        const IrBlock* source = block;
        const IrBlock* successor = block->Successors()[next++];
        int& successorState = state[successor];
        if (successorState == 0) {
            successorState = 1;
            stack.emplace_back(successor, 0u);
        } else if (successorState == 1) {
            std::unordered_set<const IrBlock*> body{successor};
            std::vector<const IrBlock*> pending{source};
            while (!pending.empty()) {
                const IrBlock* member = pending.back();
                pending.pop_back();
                if (!body.insert(member).second) continue;
                for (const IrBlock* predecessor : member->Predecessors()) pending.push_back(predecessor);
            }
            looping.insert(body.begin(), body.end());
        }
    }
    std::vector<Region> result;
    for (const auto& [block, visited] : state) {
        if (visited != 2) continue;
        if (block->Id() >= result.size()) result.resize(block->Id() + 1u, Region::Unreached);
        result[block->Id()] = looping.contains(block) ? Region::Loop : Region::Once;
    }
    return result;
}

using MaskSet = std::uint32_t;

class MaskSets {
public:
    static constexpr MaskSet None = 0;
    static constexpr MaskSet All = 1;

    explicit MaskSets(std::uint64_t& steps) : steps(steps), sets(2) {}

    MaskSet Single(std::uint32_t mask) {
        sets.push_back({mask});
        return static_cast<MaskSet>(sets.size() - 1u);
    }

    MaskSet Of(std::vector<std::uint32_t>&& masks) {
        if (masks.empty()) return None;
        std::sort(masks.begin(), masks.end());
        sets.push_back(std::move(masks));
        return static_cast<MaskSet>(sets.size() - 1u);
    }

    MaskSet Union(MaskSet left, MaskSet right) {
        if (left == right || right == None) return left;
        if (left == None) return right;
        if (left == All || right == All) return All;
        std::vector<std::uint32_t> merged;
        merged.reserve(sets[left].size() + sets[right].size());
        std::set_union(sets[left].begin(), sets[left].end(), sets[right].begin(), sets[right].end(), std::back_inserter(merged));
        return keep(left, right, std::move(merged));
    }

    MaskSet Intersection(MaskSet left, MaskSet right) {
        if (left == right || right == All) return left;
        if (left == All) return right;
        if (left == None || right == None) return None;
        std::vector<std::uint32_t> common;
        std::set_intersection(sets[left].begin(), sets[left].end(), sets[right].begin(), sets[right].end(), std::back_inserter(common));
        return keep(left, right, std::move(common));
    }

    [[nodiscard]] bool Contains(MaskSet set, std::uint32_t mask) const {
        return set == All || (set != None && std::binary_search(sets[set].begin(), sets[set].end(), mask));
    }

    [[nodiscard]] bool Same(MaskSet left, MaskSet right) const {
        return left == right || (left > All && right > All && sets[left] == sets[right]);
    }

private:
    MaskSet keep(MaskSet left, MaskSet right, std::vector<std::uint32_t>&& result) {
        steps += sets[left].size() + sets[right].size();
        if (result.empty()) return None;
        if (result.size() == sets[left].size()) return left;
        if (result.size() == sets[right].size()) return right;
        sets.push_back(std::move(result));
        return static_cast<MaskSet>(sets.size() - 1u);
    }

    std::uint64_t& steps;
    std::vector<std::vector<std::uint32_t>> sets;
};

class Components {
public:
    template<typename TSuccessors, typename TVisit>
    void From(std::uint32_t root, TSuccessors&& successors, TVisit&& visit) {
        if (seen(root)) return;
        open(root, successors);
        while (!frames.empty()) {
            Frame& frame = frames.back();
            if (frame.next != frame.end) {
                const std::uint32_t successor = edges[frame.next++];
                if (successor == frame.node) frame.cyclic = true;
                if (!seen(successor)) {
                    open(successor, successors);
                } else if (low[successor] != Closed) {
                    low[frame.node] = std::min(low[frame.node], order[successor]);
                }
                continue;
            }
            const std::uint32_t node = frame.node;
            const bool cyclic = frame.cyclic;
            edges.resize(frame.begin);
            frames.pop_back();
            if (!frames.empty()) low[frames.back().node] = std::min(low[frames.back().node], low[node]);
            if (low[node] != order[node]) continue;
            members.clear();
            do {
                members.push_back(pending.back());
                pending.pop_back();
                low[members.back()] = Closed;
            } while (members.back() != node);
            visit(members, cyclic || members.size() != 1u);
        }
    }

private:
    static constexpr std::uint32_t Closed = 0xffffffffu;

    struct Frame {
        std::uint32_t node = 0;
        std::size_t begin = 0;
        std::size_t next = 0;
        std::size_t end = 0;
        bool cyclic = false;
    };

    [[nodiscard]] bool seen(std::uint32_t node) const {
        return node < order.size() && order[node] != 0u;
    }

    template<typename TSuccessors>
    void open(std::uint32_t node, TSuccessors& successors) {
        if (node >= order.size()) {
            order.resize(node + 1u, 0u);
            low.resize(node + 1u, 0u);
        }
        order[node] = low[node] = ++opened;
        pending.push_back(node);
        const std::size_t begin = edges.size();
        successors(node, edges);
        frames.push_back(Frame{node, begin, begin, edges.size(), false});
    }

    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> low;
    std::vector<std::uint32_t> pending;
    std::vector<std::uint32_t> edges;
    std::vector<std::uint32_t> members;
    std::vector<Frame> frames;
    std::uint32_t opened = 0;
};

struct MaskFamily {
    bool acrossLoops = false;
    bool wholeQuads = false;

    bool operator==(const MaskFamily&) const = default;
};

enum class Implication : std::uint8_t { Condition, Words, SelectedTrue, SelectedFalse };

struct ImplicationKey {
    const IrValue* first = nullptr;
    const IrValue* second = nullptr;
    Implication kind = Implication::Condition;

    bool operator==(const ImplicationKey&) const = default;
};

struct ImplicationKeyHash {
    std::size_t operator()(const ImplicationKey& key) const {
        return std::hash<const IrValue*>{}(key.first) * 31u ^ std::hash<const IrValue*>{}(key.second);
    }
};

class ImpliedMasks {
public:
    ImpliedMasks(const std::vector<Region>& regions, const BlockSet& uniform, std::uint32_t waveSize, MaskFamily family, MaskSets& sets, std::uint64_t& steps) : regions(regions), uniform(uniform), waveSize(waveSize), family(family), sets(sets), steps(steps) {}

    void AddMask(const IrValue* mask) {
        if (!mark(masks, mask)) return;
        const auto bit = threadBit(mask, waveSize);
        if (bit) {
            mark(followed, bit->word);
            sameWords[wordsKey(bit->low, bit->high)].masks.push_back(mask->Id());
        }
        if (!family.wholeQuads) return;
        mark(followed, mask);
        const IrValue* source = wholeQuadSource(mask, waveSize);
        const auto known = std::find_if(quads.begin(), quads.end(), [&](const Quads& entry) { return entry.source == source; });
        if (known == quads.end()) {
            quads.push_back(Quads{source, sets.Single(mask->Id())});
        } else {
            known->masks = sets.Union(known->masks, sets.Single(mask->Id()));
        }
    }

    MaskSet Of(const IrValue* guard) {
        const std::uint32_t root = condition(guard);
        components.From(root, [this](std::uint32_t node, std::vector<std::uint32_t>& out) { expand(node, out); }, [this](const std::vector<std::uint32_t>& members, bool cyclic) { settle(members, cyclic); });
        return nodes[root].implied;
    }

    [[nodiscard]] bool Follows(const IrValue& value) const {
        return value.Id() < followed.size() && followed[value.Id()] != 0u;
    }

private:
    struct Node {
        ImplicationKey key;
        MaskSet own = MaskSets::None;
        MaskSet implied = MaskSets::All;
        bool every = false;
        bool queued = false;
        std::uint32_t group = 0;
        std::vector<std::uint32_t> inputs;
        std::vector<std::uint32_t> outputs;
    };

    struct Quads {
        const IrValue* source = nullptr;
        MaskSet masks = MaskSets::None;
    };

    struct SameWords {
        std::vector<std::uint32_t> masks;
        MaskSet set = MaskSets::None;
    };

    static bool mark(std::vector<std::uint8_t>& marks, const IrValue* value) {
        if (value->Id() >= marks.size()) marks.resize(value->Id() + 1u, 0u);
        return std::exchange(marks[value->Id()], std::uint8_t{1}) == 0u;
    }

    std::uint32_t add(const ImplicationKey& key) {
        nodes.push_back(Node{key});
        mark(followed, key.first);
        if (key.second != nullptr) mark(followed, key.second);
        return static_cast<std::uint32_t>(nodes.size() - 1u);
    }

    [[nodiscard]] ImplicationKey wordsKey(const IrValue* low, const IrValue* high) const {
        return ImplicationKey{low, waveSize == 32u ? low : high, Implication::Words};
    }

    std::uint32_t words(const IrValue* low, const IrValue* high) {
        const ImplicationKey key = wordsKey(low, high);
        const auto known = wordNodes.find(key);
        return known != wordNodes.end() ? known->second : wordNodes.emplace(key, add(key)).first->second;
    }

    std::uint32_t condition(const IrValue* value) {
        value = value->Resolve();
        if (value->Id() >= conditionNodes.size()) conditionNodes.resize(value->Id() + 1u, Unknown);
        if (conditionNodes[value->Id()] == Unknown) conditionNodes[value->Id()] = add(ImplicationKey{value});
        return conditionNodes[value->Id()];
    }

    bool merges(const IrValue& phi) const {
        if (!followable(regions, phi.Parent(), family.acrossLoops) || phi.ArgumentCount() != phi.PhiBlockCount()) return false;
        for (std::size_t index = 0; index < phi.ArgumentCount(); ++index) {
            const IrValue* incoming = phi.Argument(index)->Resolve();
            if (!incoming->HasImmediate() && !followable(regions, incoming->Parent(), family.acrossLoops)) return false;
        }
        return true;
    }

    static const IrValue* incomingFrom(const IrValue& phi, const IrBlock* predecessor) {
        for (std::size_t index = 0; index < phi.PhiBlockCount(); ++index) {
            if (phi.PhiBlock(index) == predecessor) return phi.Argument(index)->Resolve();
        }
        return nullptr;
    }

    void expand(std::uint32_t node, std::vector<std::uint32_t>& out) {
        const ImplicationKey key = nodes[node].key;
        Node shape{key};
        if (key.kind == Implication::Condition) {
            expandCondition(key.first, shape);
        } else if (key.kind == Implication::Words) {
            expandWords(key.first, key.second, shape);
        } else {
            expandSelected(key.first, key.kind == Implication::SelectedTrue, shape);
        }
        out.insert(out.end(), shape.inputs.begin(), shape.inputs.end());
        for (const std::uint32_t input : shape.inputs) nodes[input].outputs.push_back(node);
        shape.outputs = std::move(nodes[node].outputs);
        nodes[node] = std::move(shape);
    }

    void expandCondition(const IrValue* value, Node& shape) {
        if (value->Id() < masks.size() && masks[value->Id()] != 0u && !threadBit(value, waveSize)) shape.own = sets.Single(value->Id());
        if (value->HasImmediate()) {
            if (value->Type() == IrType::Bool && !value->ImmediateBool()) shape.own = MaskSets::All;
            return;
        }
        for (const Quads& entry : quads) {
            ++steps;
            if (withinWords(value, entry.source->Argument(0)->Resolve(), entry.source->Argument(1)->Resolve(), waveSize, uniform, 0u)) shape.own = sets.Union(shape.own, entry.masks);
        }
        if (value->Opcode() == IrOpcode::LogicalAnd) {
            shape.inputs = {condition(value->Argument(0)), condition(value->Argument(1))};
        } else if (value->Opcode() == IrOpcode::SelectU1) {
            shape.every = true;
            shape.inputs = {add(ImplicationKey{value, nullptr, Implication::SelectedTrue}), add(ImplicationKey{value, nullptr, Implication::SelectedFalse})};
        } else if (value->IsPhi()) {
            if (!merges(*value)) return;
            shape.every = true;
            for (std::size_t index = 0; index < value->ArgumentCount(); ++index) shape.inputs.push_back(condition(value->Argument(index)));
        } else if (const auto bit = threadBit(value, waveSize)) {
            mark(followed, bit->word);
            shape.inputs = {words(bit->low, bit->high)};
        }
    }

    void expandSelected(const IrValue* select, bool whenTrue, Node& shape) {
        const IrValue* selector = select->Argument(0)->Resolve();
        shape.inputs = {condition(select->Argument(whenTrue ? 1u : 2u))};
        if (whenTrue) {
            shape.inputs.push_back(condition(selector));
        } else if (selector->Opcode() == IrOpcode::LogicalNot) {
            shape.inputs.push_back(condition(selector->Argument(0)));
        }
    }

    void expandWords(const IrValue* low, const IrValue* high, Node& shape) {
        if (isImmediate(low, 0u) && (waveSize == 32u || isImmediate(high, 0u))) {
            shape.own = MaskSets::All;
            return;
        }
        if (const auto same = sameWords.find(wordsKey(low, high)); same != sameWords.end()) {
            if (same->second.set == MaskSets::None) same->second.set = sets.Of(std::move(same->second.masks));
            shape.own = same->second.set;
        }
        if (low->Opcode() == IrOpcode::CompositeExtractU32x4 && isImmediate(low->Argument(1)->Resolve(), 0u)) {
            const IrValue* ballot = low->Argument(0)->Resolve();
            const bool sameBallot = waveSize == 32u || (high->Opcode() == IrOpcode::CompositeExtractU32x4 && high->Argument(0)->Resolve() == ballot && isImmediate(high->Argument(1)->Resolve(), 1u));
            if (ballot->Opcode() == IrOpcode::Ballot && sameBallot) shape.inputs = {condition(ballot->Argument(0))};
        } else if (low->Opcode() == IrOpcode::BitwiseAnd32) {
            if (waveSize == 32u) {
                shape.inputs = {words(low->Argument(0)->Resolve(), high), words(low->Argument(1)->Resolve(), high)};
            } else if (high->Opcode() == IrOpcode::BitwiseAnd32) {
                for (std::size_t lowIndex = 0; lowIndex < 2u; ++lowIndex) {
                    for (std::size_t highIndex = 0; highIndex < 2u; ++highIndex) shape.inputs.push_back(words(low->Argument(lowIndex)->Resolve(), high->Argument(highIndex)->Resolve()));
                }
            }
        } else if (low->IsPhi() && merges(*low) && (waveSize == 32u || (high->IsPhi() && high->Parent() == low->Parent() && merges(*high)))) {
            for (std::size_t index = 0; index < low->PhiBlockCount(); ++index) {
                const IrValue* highIncoming = waveSize == 32u ? high : incomingFrom(*high, low->PhiBlock(index));
                if (highIncoming == nullptr) {
                    shape.inputs.clear();
                    return;
                }
                shape.inputs.push_back(words(low->Argument(index)->Resolve(), highIncoming));
            }
            shape.every = true;
        }
    }

    MaskSet evaluate(const Node& node) {
        MaskSet result = node.every ? MaskSets::All : MaskSets::None;
        for (const std::uint32_t input : node.inputs) {
            ++steps;
            result = node.every ? sets.Intersection(result, nodes[input].implied) : sets.Union(result, nodes[input].implied);
        }
        return sets.Union(result, node.own);
    }

    void settle(const std::vector<std::uint32_t>& members, bool cyclic) {
        if (!cyclic) {
            nodes[members.front()].implied = evaluate(nodes[members.front()]);
            return;
        }
        ++groups;
        queue.assign(members.rbegin(), members.rend());
        for (const std::uint32_t member : members) {
            nodes[member].queued = true;
            nodes[member].group = groups;
        }
        while (!queue.empty()) {
            const std::uint32_t member = queue.back();
            queue.pop_back();
            nodes[member].queued = false;
            const MaskSet result = evaluate(nodes[member]);
            if (sets.Same(result, nodes[member].implied)) continue;
            nodes[member].implied = result;
            for (const std::uint32_t output : nodes[member].outputs) {
                if (nodes[output].group == groups && !std::exchange(nodes[output].queued, true)) queue.push_back(output);
            }
        }
    }

    static constexpr std::uint32_t Unknown = 0xffffffffu;

    const std::vector<Region>& regions;
    const BlockSet& uniform;
    std::uint32_t waveSize;
    MaskFamily family;
    MaskSets& sets;
    std::uint64_t& steps;
    std::vector<std::uint8_t> masks;
    std::vector<std::uint8_t> followed;
    std::vector<Quads> quads;
    std::vector<Node> nodes;
    std::vector<std::uint32_t> conditionNodes;
    std::unordered_map<ImplicationKey, std::uint32_t, ImplicationKeyHash> wordNodes;
    std::unordered_map<ImplicationKey, SameWords, ImplicationKeyHash> sameWords;
    std::vector<std::uint32_t> queue;
    std::uint32_t groups = 0;
    Components components;
};

MaskFamily familyOf(const std::vector<Region>& regions, const IrValue* mask, std::uint32_t waveSize) {
    return MaskFamily{mask->HasImmediate() || regionOf(regions, mask->Parent()) == Region::Once, wholeQuadSource(mask, waveSize) != nullptr};
}

class GuardProof {
public:
    GuardProof(const IrProgram& program, const std::vector<Region>& regions, const BlockSet& uniform, const IrValue* observation, std::span<const IrBlock* const> path = {}) : program(program), regions(regions), uniform(uniform), observation(observation), path(path), waveSize(program.WaveSize()) {}

    bool ImpliesMask(const IrValue* condition, const IrValue* low, const IrValue* high) {
        return within(condition, Words{low, high}, 0u);
    }

    bool Implies(const IrValue* left, const IrValue* right, unsigned depth = 0) {
        left = resolve(left);
        right = resolve(right);
        if (!follow(left, depth) || !follow(right, depth)) return false;
        if (left == right || truth(left, false) || truth(right, true)) return true;
        if (right->Opcode() == IrOpcode::LogicalAnd) return Implies(left, right->Argument(0), depth + 1u) && Implies(left, right->Argument(1), depth + 1u);
        if (right->Opcode() == IrOpcode::LogicalOr) return Implies(left, right->Argument(0), depth + 1u) || Implies(left, right->Argument(1), depth + 1u);
        if (left->Opcode() == IrOpcode::LogicalAnd) return Implies(left->Argument(0), right, depth + 1u) || Implies(left->Argument(1), right, depth + 1u);
        if (left->Opcode() == IrOpcode::LogicalOr) return Implies(left->Argument(0), right, depth + 1u) && Implies(left->Argument(1), right, depth + 1u);
        if (left->IsPhi() && merges(*left)) {
            return std::all_of(left->Arguments().begin(), left->Arguments().end(), [&](const IrValue* incoming) { return Implies(incoming, right, depth + 1u); });
        }
        if (left->Opcode() == IrOpcode::SelectU1) return Implies(left->Argument(1), right, depth + 1u) && Implies(left->Argument(2), right, depth + 1u);
        if (const auto bit = threadBit(right, waveSize)) return within(left, Words{bit->low, bit->high}, depth + 1u);
        if (const auto bit = threadBit(left, waveSize)) return fromWords(Words{bit->low, bit->high}, right, depth + 1u);
        return false;
    }

private:
    struct Words {
        const IrValue* low;
        const IrValue* high;
    };

    bool follow(const IrValue* value, unsigned depth) {
        if (depth > 64u || remaining == 0u) return false;
        --remaining;
        return value->HasImmediate() || regionOf(regions, value->Parent()) == Region::Once;
    }

    static bool truth(const IrValue* value, bool expected) {
        return value->Type() == IrType::Bool && value->HasImmediate() && value->ImmediateBool() == expected;
    }

    bool constant(Words words, std::uint32_t value) const {
        return isImmediate(words.low, value) && (waveSize == 32u || isImmediate(words.high, value));
    }

    bool same(Words left, Words right) const {
        return left.low == right.low && (waveSize == 32u || left.high == right.high);
    }

    bool merges(const IrValue& phi) const {
        if (regionOf(regions, phi.Parent()) != Region::Once || phi.ArgumentCount() == 0u || phi.ArgumentCount() != phi.PhiBlockCount()) return false;
        return std::all_of(phi.Arguments().begin(), phi.Arguments().end(), [&](const IrValue* incoming) { return incoming->Resolve()->HasImmediate() || regionOf(regions, incoming->Resolve()->Parent()) == Region::Once; });
    }

    static const IrValue* incomingFrom(const IrValue& phi, const IrBlock* predecessor) {
        for (std::size_t index = 0; index < phi.PhiBlockCount(); ++index) if (phi.PhiBlock(index) == predecessor) return phi.Argument(index)->Resolve();
        return nullptr;
    }

    const IrValue* resolve(const IrValue* value) const {
        for (unsigned depth = 0; depth < 64u; ++depth) {
            value = value->Resolve();
            if (!value->IsPhi()) return value;
            const auto block = std::find(path.begin(), path.end(), value->Parent());
            if (block == path.end() || block == path.begin()) return value;
            const IrValue* incoming = incomingFrom(*value, *(block - 1));
            if (incoming == nullptr) return value;
            value = incoming;
        }
        return value;
    }

    bool capturedHere(const IrValue* value) const {
        if (uniform.contains(value->Parent())) return true;
        const IrBlock* observedBlock = path.empty() ? observation->Parent() : path.back();
        if (!truth(value->Argument(0)->Resolve(), true) || observedBlock == nullptr || value->Parent() == nullptr) return false;
        if (value->Parent() == observedBlock) {
            if (!path.empty() && observation->Parent() != observedBlock) return true;
            const auto& instructions = value->Parent()->Instructions();
            const auto captured = std::find(instructions.begin(), instructions.end(), value);
            const auto observed = std::find(instructions.begin(), instructions.end(), observation);
            return captured != instructions.end() && observed != instructions.end() && std::distance(instructions.begin(), captured) < std::distance(instructions.begin(), observed);
        }
        BlockSet visited;
        std::vector<const IrBlock*> pending{program.BlockOrder().front()};
        while (!pending.empty()) {
            const IrBlock* block = pending.back();
            pending.pop_back();
            if (block == value->Parent() || !visited.insert(block).second) continue;
            if (block == observedBlock) return false;
            pending.insert(pending.end(), block->Successors().begin(), block->Successors().end());
        }
        return true;
    }

    const IrValue* ballot(const IrValue* word, std::uint32_t half) const {
        if (word->Opcode() != IrOpcode::CompositeExtractU32x4 || !isImmediate(word->Argument(1)->Resolve(), half)) return nullptr;
        const IrValue* value = word->Argument(0)->Resolve();
        return value->Opcode() == IrOpcode::Ballot ? value : nullptr;
    }

    std::optional<Words> quadSource(Words words) const {
        if (words.low->Opcode() != IrOpcode::CompositeExtractU64 || !isImmediate(words.low->Argument(1)->Resolve(), 0u)) return std::nullopt;
        const IrValue* wqm = words.low->Argument(0)->Resolve();
        if (waveSize != 32u && (words.high->Opcode() != IrOpcode::CompositeExtractU64 || words.high->Argument(0)->Resolve() != wqm || !isImmediate(words.high->Argument(1)->Resolve(), 1u))) return std::nullopt;
        if (wqm->Opcode() != IrOpcode::WqmU64) return std::nullopt;
        const IrValue* source = wqm->Argument(0)->Resolve();
        if (source->Opcode() != IrOpcode::CompositeConstructU64 || source->ArgumentCount() != 2u) return std::nullopt;
        return Words{source->Argument(0)->Resolve(), source->Argument(1)->Resolve()};
    }

    std::vector<Words> parts(Words words) const {
        const bool lowAnd = words.low->Opcode() == IrOpcode::BitwiseAnd32;
        const bool highAnd = waveSize != 32u && words.high->Opcode() == IrOpcode::BitwiseAnd32;
        std::vector<Words> result;
        if (!lowAnd && !highAnd) return result;
        for (unsigned low = 0; low < (lowAnd ? 2u : 1u); ++low) {
            for (unsigned high = 0; high < (highAnd ? 2u : 1u); ++high) {
                result.push_back(Words{lowAnd ? words.low->Argument(low)->Resolve() : words.low, highAnd ? words.high->Argument(high)->Resolve() : words.high});
            }
        }
        return result;
    }

    template<typename TProof>
    std::optional<bool> intersection(Words words, TProof proof) {
        const bool lowAnd = words.low->Opcode() == IrOpcode::BitwiseAnd32;
        const bool highAnd = waveSize != 32u && words.high->Opcode() == IrOpcode::BitwiseAnd32;
        if (!lowAnd && !highAnd) return std::nullopt;
        const auto part = [&](unsigned low, unsigned high) {
            return Words{lowAnd ? words.low->Argument(low)->Resolve() : words.low, highAnd ? words.high->Argument(high)->Resolve() : words.high};
        };
        if (!lowAnd || !highAnd) return proof(part(0u, 0u)) && proof(part(lowAnd ? 1u : 0u, highAnd ? 1u : 0u));
        return (proof(part(0u, 0u)) && proof(part(1u, 1u))) || (proof(part(0u, 1u)) && proof(part(1u, 0u)));
    }

    bool within(const IrValue* condition, Words words, unsigned depth) {
        condition = resolve(condition);
        words = Words{resolve(words.low), resolve(words.high)};
        if (!follow(condition, depth) || !follow(words.low, depth) || (waveSize != 32u && !follow(words.high, depth))) return false;
        if (truth(condition, false) || constant(words, 0xffffffffu)) return true;
        if (const auto bit = threadBit(condition, waveSize)) return subset(Words{bit->low, bit->high}, words, depth + 1u);
        if (const auto result = intersection(words, [&](Words part) { return within(condition, part, depth + 1u); })) return *result;
        if (const auto source = quadSource(words)) return within(condition, *source, depth + 1u);
        const IrValue* lowBallot = ballot(words.low, 0u);
        const IrValue* highBallot = waveSize == 32u ? lowBallot : ballot(words.high, 1u);
        if (lowBallot && highBallot && capturedHere(lowBallot) && capturedHere(highBallot)) return Implies(condition, lowBallot->Argument(0), depth + 1u) && Implies(condition, highBallot->Argument(0), depth + 1u);
        if (condition->Opcode() == IrOpcode::LogicalAnd) return within(condition->Argument(0), words, depth + 1u) || within(condition->Argument(1), words, depth + 1u);
        if (condition->Opcode() == IrOpcode::LogicalOr) return within(condition->Argument(0), words, depth + 1u) && within(condition->Argument(1), words, depth + 1u);
        if (condition->IsPhi() && merges(*condition)) return std::all_of(condition->Arguments().begin(), condition->Arguments().end(), [&](const IrValue* incoming) { return within(incoming, words, depth + 1u); });
        return false;
    }

    bool fromWords(Words words, const IrValue* condition, unsigned depth) {
        condition = resolve(condition);
        words = Words{resolve(words.low), resolve(words.high)};
        if (!follow(condition, depth) || !follow(words.low, depth) || (waveSize != 32u && !follow(words.high, depth))) return false;
        if (truth(condition, true) || constant(words, 0u)) return true;
        if (const auto bit = threadBit(condition, waveSize)) return subset(words, Words{bit->low, bit->high}, depth + 1u);
        if (condition->Opcode() == IrOpcode::LogicalAnd) return fromWords(words, condition->Argument(0), depth + 1u) && fromWords(words, condition->Argument(1), depth + 1u);
        if (condition->Opcode() == IrOpcode::LogicalOr) return fromWords(words, condition->Argument(0), depth + 1u) || fromWords(words, condition->Argument(1), depth + 1u);
        const auto intersections = parts(words);
        if (!intersections.empty()) return std::any_of(intersections.begin(), intersections.end(), [&](Words part) { return fromWords(part, condition, depth + 1u); });
        const IrValue* lowBallot = ballot(words.low, 0u);
        const IrValue* highBallot = waveSize == 32u ? lowBallot : ballot(words.high, 1u);
        return lowBallot && highBallot && Implies(lowBallot->Argument(0), condition, depth + 1u) && Implies(highBallot->Argument(0), condition, depth + 1u);
    }

    bool subset(Words left, Words right, unsigned depth) {
        left = Words{resolve(left.low), resolve(left.high)};
        right = Words{resolve(right.low), resolve(right.high)};
        if (!follow(left.low, depth) || !follow(right.low, depth) || (waveSize != 32u && (!follow(left.high, depth) || !follow(right.high, depth)))) return false;
        if (same(left, right) || constant(left, 0u) || constant(right, 0xffffffffu)) return true;
        if (const auto result = intersection(right, [&](Words part) { return subset(left, part, depth + 1u); })) return *result;
        const auto leftParts = parts(left);
        if (!leftParts.empty() && std::any_of(leftParts.begin(), leftParts.end(), [&](Words part) { return subset(part, right, depth + 1u); })) return true;
        if (const auto source = quadSource(right); source && subset(left, *source, depth + 1u)) return true;
        if (left.low->IsPhi() && merges(*left.low) && (waveSize == 32u || (left.high->IsPhi() && left.high->Parent() == left.low->Parent() && merges(*left.high)))) {
            for (std::size_t index = 0; index < left.low->PhiBlockCount(); ++index) {
                const IrValue* highIncoming = waveSize == 32u ? left.high : incomingFrom(*left.high, left.low->PhiBlock(index));
                if (highIncoming == nullptr || !subset(Words{left.low->Argument(index)->Resolve(), highIncoming}, right, depth + 1u)) return false;
            }
            return true;
        }
        const IrValue* lowBallot = ballot(left.low, 0u);
        const IrValue* highBallot = waveSize == 32u ? lowBallot : ballot(left.high, 1u);
        if (lowBallot && highBallot && within(lowBallot->Argument(0), right, depth + 1u) && within(highBallot->Argument(0), right, depth + 1u)) return true;
        lowBallot = ballot(right.low, 0u);
        highBallot = waveSize == 32u ? lowBallot : ballot(right.high, 1u);
        return lowBallot && highBallot && capturedHere(lowBallot) && capturedHere(highBallot) && fromWords(left, lowBallot->Argument(0), depth + 1u) && fromWords(left, highBallot->Argument(0), depth + 1u);
    }

    const IrProgram& program;
    const std::vector<Region>& regions;
    const BlockSet& uniform;
    const IrValue* observation;
    std::span<const IrBlock* const> path;
    std::uint32_t waveSize;
    std::uint32_t remaining = 8192u;
};

class UnreadLanes;

using FirstLaneReads = std::array<std::unique_ptr<UnreadLanes>, 4>;

class UnreadLanes {
public:
    UnreadLanes(IrProgram& program, const std::vector<Region>& regions, const BlockSet& uniform, MaskFamily family, std::uint64_t& steps, FirstLaneReads* shared = nullptr)
        : program(program), regions(regions), uniform(uniform), family(family), sampledByQuad(family.wholeQuads && program.Resources().stage == IrShaderStage::Pixel), firstLane(shared != nullptr), sameBlock(firstLane && !family.acrossLoops), steps(steps), sets(steps), implied(regions, uniform, program.WaveSize(), family, sets, steps), firstLanes(firstLane ? *shared : owned) {
        if (!firstLane) return;
        for (const IrBlock* block : program.BlockOrder()) {
            for (const IrValue* inst : block->Instructions()) {
                if (inst->Opcode() != IrOpcode::ReadFirstLane) continue;
                const IrValue* mask = inst->Argument(1)->Resolve();
                if (familyOf(regions, mask, program.WaveSize()) == family) implied.AddMask(mask);
            }
        }
    }

    bool UnreadOutsideItsMask(IrValue& read) {
        if (solving) return false;
        solving = true;
        static_cast<void>(readerOf(read));
        components.From(read.Id(), [this](std::uint32_t node, std::vector<std::uint32_t>& out) { expand(node, out); }, [this](const std::vector<std::uint32_t>& members, bool cyclic) { settle(members, cyclic); });
        solving = false;
        return sets.Contains(unread[read.Id()], read.Argument(1)->Resolve()->Id());
    }

    [[nodiscard]] bool Follows(const IrValue& value) const {
        return implied.Follows(value) || (value.Id() < readers.size() && readers[value.Id()] != Reader::Unknown);
    }

    std::uint32_t Remove() {
        std::vector<IrValue*> order;
        for (auto blockIt = program.BlockOrder().rbegin(); blockIt != program.BlockOrder().rend(); ++blockIt) {
            const bool single = regionOf(regions, *blockIt) == Region::Once;
            const auto& instructions = (*blockIt)->Instructions();
            for (auto it = instructions.rbegin(); it != instructions.rend(); ++it) {
                IrValue* inst = *it;
                order.push_back(inst);
                if (!single || !isSelect(inst->Opcode()) || inst->ArgumentCount() != 3u || !inst->HasUses()) continue;
                const IrValue* mask = inst->Argument(0)->Resolve();
                if (familyOf(regions, mask, program.WaveSize()) != family) continue;
                implied.AddMask(mask);
                slot(writes, inst->Id(), static_cast<const IrValue*>(nullptr)) = mask;
                found = true;
            }
        }
        if (!found) return 0;
        for (IrValue* inst : order) {
            if (halted) break;
            static_cast<void>(readerOf(*inst));
            components.From(inst->Id(), [this](std::uint32_t node, std::vector<std::uint32_t>& out) { expand(node, out); }, [this](const std::vector<std::uint32_t>& members, bool cyclic) { settle(members, cyclic); });
        }
        if (removed != 0u) {
            for (IrBlock* block : program.BlockOrder()) std::erase_if(block->Instructions(), [](const IrValue* inst) { return inst->Parent() == nullptr; });
        }
        return removed;
    }

private:
    static constexpr std::uint32_t Queued = 0xffffffffu;

    template<typename TValue>
    static TValue& slot(std::vector<TValue>& table, std::uint32_t id, TValue fill) {
        if (id >= table.size()) table.resize(id + 1u, fill);
        return table[id];
    }

    enum class Reader : std::uint8_t { Unknown, Outside, Reads, Passes };

    Reader readerOf(IrValue& user) {
        Reader& known = slot(readers, user.Id(), Reader::Unknown);
        if (known != Reader::Unknown) return known;
        slot(values, user.Id(), static_cast<IrValue*>(nullptr)) = &user;
        slot(unread, user.Id(), MaskSets::All);
        slot(groups, user.Id(), std::uint32_t{0});
        if (sameBlock ? user.IsPhi() : !followable(regions, user.Parent(), family.acrossLoops)) return known = Reader::Outside;
        const bool passes = user.IsPhi() || isLaneLocal(user.Opcode()) || isExplicitLodSample(program, user) || (sampledByQuad && user.Opcode() == IrOpcode::ImageSampleRaw);
        return known = passes ? Reader::Passes : Reader::Reads;
    }

    [[nodiscard]] bool leaves(const IrValue& value, const IrValue& user) const {
        return sameBlock && user.Parent() != value.Parent();
    }

    [[nodiscard]] bool readsFirstLane(const IrUse& use) const {
        return use.user->Opcode() == IrOpcode::ReadFirstLane && use.operand == 0u;
    }

    [[nodiscard]] bool solvedHere(const IrValue& read) const {
        return firstLane && familyOf(regions, read.Argument(1)->Resolve(), program.WaveSize()) == family;
    }

    bool unreadOutsideItsMask(IrValue& read) {
        const IrValue* mask = read.Argument(1)->Resolve();
        if (solvedHere(read)) return sets.Contains(unread[read.Id()], mask->Id());
        const MaskFamily readFamily = familyOf(regions, mask, program.WaveSize());
        auto& reads = firstLanes[(readFamily.acrossLoops ? 2u : 0u) + (readFamily.wholeQuads ? 1u : 0u)];
        if (!reads) reads = std::make_unique<UnreadLanes>(program, regions, uniform, readFamily, steps, &firstLanes);
        return reads->UnreadOutsideItsMask(read);
    }

    void expand(std::uint32_t node, std::vector<std::uint32_t>& out) {
        for (const IrUse& use : values[node]->OperandUses()) {
            const Reader reader = readerOf(*use.user);
            if (reader == Reader::Outside || leaves(*values[node], *use.user)) continue;
            if (reader == Reader::Passes || (readsFirstLane(use) && solvedHere(*use.user))) out.push_back(use.user->Id());
        }
    }

    MaskSet unreadBy(const IrValue& value, const IrUse& use) {
        IrValue* user = use.user;
        const Reader reader = readerOf(*user);
        if (reader == Reader::Outside || leaves(value, *user)) return MaskSets::None;
        MaskSet guarded = MaskSets::None;
        if (isSelect(user->Opcode()) && use.operand == 1u) {
            guarded = implied.Of(user->Argument(0));
        } else if (user->Opcode() == IrOpcode::LogicalAnd && use.operand < 2u) {
            guarded = implied.Of(user->Argument(1u - use.operand));
        } else if (readsOnlyWhereActive(user->Opcode()) && use.operand + 1u < user->ArgumentCount()) {
            guarded = implied.Of(user->Argument(user->ArgumentCount() - 1u));
        } else if (readsFirstLane(use)) {
            guarded = implied.Of(user->Argument(1));
            if (guarded != MaskSets::None && !unreadOutsideItsMask(*user)) guarded = MaskSets::None;
        }
        if (guarded == MaskSets::All || reader != Reader::Passes) return guarded;
        return sets.Union(guarded, unread[user->Id()]);
    }

    MaskSet evaluate(std::uint32_t node) {
        MaskSet result = MaskSets::All;
        for (const IrUse& use : values[node]->OperandUses()) {
            if (result == MaskSets::None) break;
            ++steps;
            result = sets.Intersection(result, unreadBy(*values[node], use));
        }
        return result;
    }

    void settle(const std::vector<std::uint32_t>& members, bool cyclic) {
        if (halted) return;
        if (!cyclic) {
            unread[members.front()] = evaluate(members.front());
            remove(members.front());
            return;
        }
        ++group;
        queue.assign(members.rbegin(), members.rend());
        for (const std::uint32_t member : members) groups[member] = Queued;
        while (!queue.empty()) {
            const std::uint32_t member = queue.back();
            queue.pop_back();
            groups[member] = group;
            const MaskSet result = evaluate(member);
            if (sets.Same(result, unread[member])) continue;
            unread[member] = result;
            for (const IrValue* argument : values[member]->Arguments()) {
                if (argument == nullptr || argument->Id() >= groups.size() || groups[argument->Id()] != group) continue;
                groups[argument->Id()] = Queued;
                queue.push_back(argument->Id());
            }
        }
    }

    void remove(std::uint32_t node) {
        IrValue* select = values[node];
        const IrValue* mask = node < writes.size() ? writes[node] : nullptr;
        if (mask == nullptr || !select->HasUses() || select->Argument(0)->Resolve() != mask || !sets.Contains(unread[node], mask->Id())) return;
        halted = implied.Follows(*select) || std::any_of(owned.begin(), owned.end(), [&](const auto& reads) { return reads && reads->Follows(*select); });
        select->ReplaceAllUsesWith(select->Argument(1));
        select->Invalidate();
        select->SetParent(nullptr);
        ++removed;
    }

    IrProgram& program;
    const std::vector<Region>& regions;
    const BlockSet& uniform;
    MaskFamily family;
    bool sampledByQuad;
    bool firstLane;
    bool sameBlock;
    std::uint64_t& steps;
    MaskSets sets;
    ImpliedMasks implied;
    FirstLaneReads owned;
    FirstLaneReads& firstLanes;
    Components components;
    std::vector<IrValue*> values;
    std::vector<const IrValue*> writes;
    std::vector<MaskSet> unread;
    std::vector<Reader> readers;
    std::vector<std::uint32_t> groups;
    std::vector<std::uint32_t> queue;
    std::uint32_t group = 0;
    std::uint32_t removed = 0;
    bool found = false;
    bool halted = false;
    bool solving = false;
};

std::uint32_t removeFullWaveSelects(IrProgram& program, const std::vector<Region>& regions, std::vector<IrValue*>& selects) {
    std::uint32_t removed = 0;
    selects.clear();
    for (auto blockIt = program.BlockOrder().rbegin(); blockIt != program.BlockOrder().rend(); ++blockIt) {
        const bool single = regionOf(regions, *blockIt) == Region::Once;
        auto& instructions = (*blockIt)->Instructions();
        auto it = instructions.end();
        while (it != instructions.begin()) {
            --it;
            IrValue* inst = *it;
            if (!isSelect(inst->Opcode()) || inst->ArgumentCount() != 3u) continue;
            if (!inst->HasUses() || !alwaysTrue(inst->Argument(0), program.WaveSize())) {
                if (single) selects.push_back(inst);
                continue;
            }
            inst->ReplaceAllUsesWith(inst->Argument(1));
            inst->Invalidate();
            inst->SetParent(nullptr);
            it = instructions.erase(it);
            ++removed;
        }
    }
    return removed;
}

bool hasUnreadCandidate(const std::vector<IrValue*>& selects, const std::vector<Region>& regions, MaskFamily family, std::uint32_t waveSize) {
    return std::any_of(selects.begin(), selects.end(), [&](const IrValue* select) { return select->Parent() != nullptr && select->HasUses() && familyOf(regions, select->Argument(0)->Resolve(), waveSize) == family; });
}

}

MaskedSelectEliminationStats MaskedSelectEliminator::Eliminate(IrProgram& program) const {
    MaskedSelectEliminationStats stats;
    const auto regions = blockRegions(program);
    const BlockSet uniform = uniformEntryBlocks(program);
    std::vector<IrValue*> selects;
    std::uint32_t removed = removeFullWaveSelects(program, regions, selects);
    for (const MaskFamily family : {MaskFamily{true, false}, MaskFamily{true, true}, MaskFamily{false, false}, MaskFamily{false, true}}) {
        if (hasUnreadCandidate(selects, regions, family, program.WaveSize())) removed += UnreadLanes(program, regions, uniform, family, stats.steps).Remove();
    }
    stats.removedSelects = removed;
    if (removed != 0u) DeadCodeEliminator{}.Eliminate(program);
    return stats;
}

bool GuardImplies(const IrProgram& program, const IrValue* condition, const IrValue* consequence) {
    if (condition == nullptr || consequence == nullptr) return false;
    condition = condition->Resolve();
    consequence = consequence->Resolve();
    if (condition->Type() != IrType::Bool || consequence->Type() != IrType::Bool) return false;
    if (condition == consequence || (condition->HasImmediate() && !condition->ImmediateBool()) || (consequence->HasImmediate() && consequence->ImmediateBool())) return true;
    const auto regions = blockRegions(program);
    if (std::ranges::find(regions, Region::Loop) != regions.end()) return false;
    if ((!condition->HasImmediate() && regionOf(regions, condition->Parent()) != Region::Once) || (!consequence->HasImmediate() && regionOf(regions, consequence->Parent()) != Region::Once)) return false;
    const BlockSet uniform = uniformEntryBlocks(program);
    std::uint64_t steps = 0;
    MaskSets sets(steps);
    ImpliedMasks implied(regions, uniform, program.WaveSize(), familyOf(regions, consequence, program.WaveSize()), sets, steps);
    implied.AddMask(consequence);
    return sets.Contains(implied.Of(condition), consequence->Id()) || GuardProof(program, regions, uniform, condition).Implies(condition, consequence);
}

bool GuardImpliesMaskOnPath(const IrProgram& program, const IrValue* condition, const IrValue* low, const IrValue* high, std::span<const IrBlock* const> path) {
    if (condition == nullptr || low == nullptr || high == nullptr || path.empty() || program.BlockOrder().empty() || path.front() != program.BlockOrder().front()) return false;
    if (condition->Resolve()->Type() != IrType::Bool || low->Resolve()->Type() != IrType::U32 || high->Resolve()->Type() != IrType::U32) return false;
    BlockSet visited;
    for (std::size_t index = 0; index < path.size(); ++index) {
        if (path[index] == nullptr || !visited.insert(path[index]).second) return false;
        if (index != 0u && std::ranges::find(path[index - 1u]->Successors(), path[index]) == path[index - 1u]->Successors().end()) return false;
    }
    if (!condition->Resolve()->HasImmediate() && std::ranges::find(path, condition->Resolve()->Parent()) == path.end()) return false;
    const auto regions = blockRegions(program);
    if (std::ranges::find(regions, Region::Loop) != regions.end()) return false;
    const auto uniform = uniformEntryBlocks(program);
    return GuardProof(program, regions, uniform, condition->Resolve(), path).ImpliesMask(condition, low, high);
}

}
