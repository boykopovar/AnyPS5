#include "StencilReplay.hpp"
#include "Optimization/MaskedSelectEliminator.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace ShaderRecompiler {

namespace {

bool Boolean(const IrValue* value, bool expected) {
    if (value == nullptr) return expected;
    value = value->Resolve();
    return value->Type() == IrType::U1 && value->HasImmediate() && value->ImmediateBool() == expected;
}

bool Implies(const IrValue* left, const IrValue* right, unsigned depth = 0) {
    if (depth > 32u) return false;
    if (left) left = left->Resolve();
    if (right) right = right->Resolve();
    if (left == right || Boolean(left, false) || Boolean(right, true)) return true;
    if (!left || !right) return false;
    if (left->Opcode() == IrOpcode::LogicalAnd) return Implies(left->Argument(0), right, depth + 1u) || Implies(left->Argument(1), right, depth + 1u);
    if (right->Opcode() == IrOpcode::LogicalAnd) return Implies(left, right->Argument(0), depth + 1u) && Implies(left, right->Argument(1), depth + 1u);
    if (left->Opcode() == IrOpcode::LogicalOr) return Implies(left->Argument(0), right, depth + 1u) && Implies(left->Argument(1), right, depth + 1u);
    if (right->Opcode() == IrOpcode::LogicalOr) return Implies(left, right->Argument(0), depth + 1u) || Implies(left, right->Argument(1), depth + 1u);
    return false;
}

struct ReplayMask {
    const IrValue* low = nullptr;
    const IrValue* high = nullptr;
};

bool Immediate(const IrValue* value, std::uint32_t expected) {
    value = value->Resolve();
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == expected;
}

bool FaultControlFlowAcyclic(const IrProgram& program) {
    const auto& blocks = program.BlockOrder();
    const auto& metadata = program.Metadata().blockInfo;
    if (blocks.empty() || blocks.size() != metadata.size() || blocks.size() > 4096u) return false;
    std::map<std::uint32_t, std::size_t> indices;
    std::set<const IrBlock*> owned;
    for (std::size_t index = 0; index < blocks.size(); ++index) if (!blocks[index] || !owned.insert(blocks[index]).second || !indices.emplace(metadata[index].id, index).second) return false;
    std::vector<std::vector<std::size_t>> successors(blocks.size());
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& term = metadata[index].terminator;
        if (term.loopHeader) return false;
        const auto edge = [&](std::uint32_t id) {
            const auto found = indices.find(id);
            if (found == indices.end()) return false;
            successors[index].push_back(found->second);
            return true;
        };
        if (term.kind == TerminatorKind::Branch) { if (!edge(term.trueBlock)) return false; }
        else if (term.kind == TerminatorKind::ConditionalBranch) { if (!metadata[index].condition || !edge(term.trueBlock) || !edge(term.falseBlock)) return false; }
        else if (term.kind != TerminatorKind::Return) return false;
        std::set<const IrBlock*> expected;
        for (const auto next : successors[index]) expected.insert(blocks[next]);
        const std::set<const IrBlock*> actual(blocks[index]->Successors().begin(), blocks[index]->Successors().end());
        if (actual != expected) return false;
    }
    std::vector<unsigned> colors(blocks.size());
    std::vector<std::pair<std::size_t, std::size_t>> pending{{0u, 0u}};
    colors[0u] = 1u;
    while (!pending.empty()) {
        auto& [block, edge] = pending.back();
        if (edge == successors[block].size()) { colors[block] = 2u; pending.pop_back(); continue; }
        const auto next = successors[block][edge++];
        if (colors[next] == 1u) return false;
        if (colors[next] == 2u) continue;
        colors[next] = 1u;
        pending.emplace_back(next, 0u);
    }
    return true;
}

const IrValue* Other(const IrValue* value, std::uint32_t immediate) {
    if (value->ArgumentCount() != 2u) return nullptr;
    const IrValue* left = value->Argument(0)->Resolve();
    const IrValue* right = value->Argument(1)->Resolve();
    if (Immediate(left, immediate)) return right;
    if (Immediate(right, immediate)) return left;
    return nullptr;
}

class MaskReplaySafety {
public:
    explicit MaskReplaySafety(const IrProgram& program) : program(program), waveSize(program.WaveSize()) {}

    bool Check() {
        if (program.Resources().stage != IrShaderStage::Pixel || (waveSize != 32u && waveSize != 64u) || !paths()) return false;
        for (const auto* block : program.BlockOrder()) {
            for (const auto* value : block->Instructions()) {
                if (const auto mask = own(value)) {
                    if (!maskStable(*mask, 0u)) return false;
                    ownMasks.emplace(value, *mask);
                }
            }
        }
        for (const auto& info : program.Metadata().blockInfo) {
            if (info.terminator.kind != TerminatorKind::ConditionalBranch) continue;
            if (!control(info)) return false;
        }
        for (const auto& [value, mask] : ownMasks) if (helperObserved(value)) return false;
        for (const auto* block : program.BlockOrder()) {
            for (const auto* value : block->Instructions()) {
                if ((value->Opcode() == IrOpcode::LaneId || value->Opcode() == IrOpcode::Ballot || value->Opcode() == IrOpcode::WqmU64) && !restricted.contains(value)) return false;
            }
        }
        for (const auto* value : restricted) {
            for (const auto& use : value->OperandUses()) {
                if (restricted.contains(use.user) || ownMasks.contains(use.user) || use.user->Opcode() == IrOpcode::Reference || use.user->Opcode() == IrOpcode::ReferenceU32) continue;
                return false;
            }
        }
        return true;
    }

private:
    enum class HelperMask : unsigned { Visiting, RealOnly, QuadStable, Uncertain };

    void restrict(const IrValue* value) {
        if (value && !value->HasImmediate()) restricted.insert(value);
    }

    std::optional<ReplayMask> own(const IrValue* value) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::INotEqual32) return std::nullopt;
        const IrValue* bit = Other(value, 0u);
        if (!bit || bit->Opcode() != IrOpcode::BitwiseAnd32) return std::nullopt;
        const IrValue* shifted = Other(bit, 1u);
        if (!shifted || shifted->Opcode() != IrOpcode::ShiftRightLogical32 || shifted->ArgumentCount() != 2u) return std::nullopt;
        const IrValue* amount = shifted->Argument(1)->Resolve();
        if (amount->Opcode() != IrOpcode::BitwiseAnd32) return std::nullopt;
        const IrValue* lane = Other(amount, 31u);
        if (!lane || lane->Opcode() != IrOpcode::LaneId) return std::nullopt;
        const IrValue* word = shifted->Argument(0)->Resolve();
        ReplayMask result{word, word};
        if (word->Opcode() == IrOpcode::SelectU32 && word->ArgumentCount() == 3u) {
            const IrValue* half = word->Argument(0)->Resolve();
            if (half->Opcode() != IrOpcode::ULessThan32 || half->ArgumentCount() != 2u || half->Argument(0)->Resolve()->Opcode() != IrOpcode::LaneId || !Immediate(half->Argument(1), 32u)) return std::nullopt;
            restrict(half);
            restrict(half->Argument(0)->Resolve());
            restrict(word);
            result = ReplayMask{word->Argument(1)->Resolve(), word->Argument(2)->Resolve()};
        } else if (waveSize != 32u && !word->HasImmediate()) return std::nullopt;
        restrict(bit);
        restrict(shifted);
        restrict(amount);
        restrict(lane);
        return result;
    }

    const IrValue* ballotWord(const IrValue* word, std::uint32_t half) const {
        if (word->Opcode() != IrOpcode::CompositeExtractU32x4 || word->ArgumentCount() != 2u || !Immediate(word->Argument(1), half)) return nullptr;
        const IrValue* ballot = word->Argument(0)->Resolve();
        return ballot->Opcode() == IrOpcode::Ballot && ballot->ArgumentCount() == 1u ? ballot : nullptr;
    }

    bool maskStable(ReplayMask mask, unsigned depth) {
        if (depth > 128u) return false;
        mask = ReplayMask{mask.low->Resolve(), mask.high->Resolve()};
        const auto key = std::pair{mask.low->Id(), waveSize == 32u ? mask.low->Id() : mask.high->Id()};
        if (const auto found = masks.find(key); found != masks.end()) return found->second == 2u;
        masks[key] = 1u;
        bool valid = false;
        const auto finish = [&](bool accepted) {
            masks[key] = accepted ? 2u : 3u;
            if (accepted) { restrict(mask.low); if (waveSize != 32u) restrict(mask.high); }
            return accepted;
        };
        if ((Immediate(mask.low, 0u) && (waveSize == 32u || Immediate(mask.high, 0u))) || (Immediate(mask.low, 0xffffffffu) && (waveSize == 32u || Immediate(mask.high, 0xffffffffu)))) return finish(true);
        const IrValue* lowBallot = ballotWord(mask.low, 0u);
        const IrValue* highBallot = waveSize == 32u ? lowBallot : ballotWord(mask.high, 1u);
        if (lowBallot && highBallot) {
            valid = stable(lowBallot->Argument(0), depth + 1u) && stable(highBallot->Argument(0), depth + 1u);
            if (valid) { restrict(lowBallot); restrict(highBallot); }
        } else if (mask.low->Opcode() == IrOpcode::BitwiseAnd32 && (waveSize == 32u || mask.high->Opcode() == IrOpcode::BitwiseAnd32)) {
            const auto pair = [&](unsigned low, unsigned high) { return ReplayMask{mask.low->Argument(low)->Resolve(), waveSize == 32u ? mask.low->Argument(low)->Resolve() : mask.high->Argument(high)->Resolve()}; };
            valid = (maskStable(pair(0u, 0u), depth + 1u) && maskStable(pair(1u, 1u), depth + 1u)) || (waveSize != 32u && maskStable(pair(0u, 1u), depth + 1u) && maskStable(pair(1u, 0u), depth + 1u));
        } else if (mask.low->Opcode() == IrOpcode::CompositeExtractU64 && mask.low->ArgumentCount() == 2u && Immediate(mask.low->Argument(1), 0u)) {
            const IrValue* wqm = mask.low->Argument(0)->Resolve();
            const bool paired = waveSize == 32u || (mask.high->Opcode() == IrOpcode::CompositeExtractU64 && mask.high->ArgumentCount() == 2u && mask.high->Argument(0)->Resolve() == wqm && Immediate(mask.high->Argument(1), 1u));
            if (paired && wqm->Opcode() == IrOpcode::WqmU64 && wqm->ArgumentCount() == 1u) {
                const IrValue* source = wqm->Argument(0)->Resolve();
                valid = source->Opcode() == IrOpcode::CompositeConstructU64 && source->ArgumentCount() == 2u && maskStable(ReplayMask{source->Argument(0)->Resolve(), source->Argument(1)->Resolve()}, depth + 1u);
                if (valid) {
                    const auto provenance = helpers(ReplayMask{source->Argument(0)->Resolve(), source->Argument(1)->Resolve()}, depth + 1u);
                    valid = provenance == HelperMask::RealOnly || provenance == HelperMask::QuadStable;
                }
                if (valid) { restrict(wqm); restrict(source); }
            }
        } else if (mask.low->IsPhi() && mask.low->ArgumentCount() != 0u && mask.low->ArgumentCount() == mask.low->PhiBlockCount() && (waveSize == 32u || (mask.high->IsPhi() && mask.high->Parent() == mask.low->Parent() && mask.high->ArgumentCount() == mask.high->PhiBlockCount()))) {
            valid = true;
            for (std::size_t index = 0; index < mask.low->PhiBlockCount(); ++index) {
                const IrValue* high = mask.low->Argument(index)->Resolve();
                if (waveSize != 32u) {
                    high = nullptr;
                    for (std::size_t highIndex = 0; highIndex < mask.high->PhiBlockCount(); ++highIndex) if (mask.high->PhiBlock(highIndex) == mask.low->PhiBlock(index)) high = mask.high->Argument(highIndex)->Resolve();
                }
                if (!high || !maskStable(ReplayMask{mask.low->Argument(index)->Resolve(), high}, depth + 1u)) { valid = false; break; }
            }
        }
        return finish(valid);
    }

    std::optional<std::uint32_t> onHelper(const IrValue* value, unsigned depth) {
        if (depth > 128u) return std::nullopt;
        value = value->Resolve();
        if (value->HasImmediate()) {
            if (value->Type() == IrType::Bool) return value->ImmediateBool() ? 1u : 0u;
            if (value->Type() == IrType::U32) return value->ImmediateU32();
            return std::nullopt;
        }
        if (const auto found = helperValues.find(value); found != helperValues.end()) return found->second;
        helperValues.emplace(value, std::nullopt);
        std::optional<std::uint32_t> result;
        const auto argument = [&](unsigned index) { return onHelper(value->Argument(index), depth + 1u); };
        if (value->Opcode() == IrOpcode::GetBuiltin && value->ArgumentCount() == 2u && Immediate(value->Argument(0), static_cast<std::uint32_t>(StageInputKind::HelperInvocation)) && Immediate(value->Argument(1), 0u)) result = 1u;
        else if (const auto mask = own(value)) {
            if (helpers(*mask, depth + 1u) == HelperMask::RealOnly) result = 0u;
        } else if (value->Opcode() == IrOpcode::LogicalNot && value->ArgumentCount() == 1u) {
            if (const auto source = argument(0u)) result = *source == 0u ? 1u : 0u;
        } else if (value->Opcode() == IrOpcode::LogicalAnd && value->ArgumentCount() == 2u) {
            const auto left = argument(0u), right = argument(1u);
            if ((left && *left == 0u) || (right && *right == 0u)) result = 0u;
            else if (left && right) result = 1u;
        } else if (value->Opcode() == IrOpcode::LogicalOr && value->ArgumentCount() == 2u) {
            const auto left = argument(0u), right = argument(1u);
            if ((left && *left != 0u) || (right && *right != 0u)) result = 1u;
            else if (left && right) result = 0u;
        } else if ((value->Opcode() == IrOpcode::IEqual32 || value->Opcode() == IrOpcode::INotEqual32) && value->ArgumentCount() == 2u) {
            const auto left = argument(0u), right = argument(1u);
            if (left && right) result = (*left == *right) == (value->Opcode() == IrOpcode::IEqual32) ? 1u : 0u;
        } else if ((value->Opcode() == IrOpcode::SelectU1 || value->Opcode() == IrOpcode::SelectU32) && value->ArgumentCount() == 3u) {
            if (const auto condition = argument(0u)) result = argument(*condition != 0u ? 1u : 2u);
            else {
                const auto left = argument(1u), right = argument(2u);
                if (left && right && left == right) result = left;
            }
        } else if (value->IsPhi() && value->ArgumentCount() != 0u && value->ArgumentCount() == value->PhiBlockCount()) {
            result = argument(0u);
            for (unsigned index = 1u; result && index < value->ArgumentCount(); ++index) if (argument(index) != result) result = std::nullopt;
        }
        helperValues[value] = result;
        return result;
    }

    HelperMask helpers(ReplayMask mask, unsigned depth) {
        if (depth > 128u) return HelperMask::Uncertain;
        mask = ReplayMask{mask.low->Resolve(), mask.high->Resolve()};
        const auto key = std::pair{mask.low->Id(), waveSize == 32u ? mask.low->Id() : mask.high->Id()};
        if (const auto found = helperMasks.find(key); found != helperMasks.end()) return found->second;
        helperMasks[key] = HelperMask::Visiting;
        HelperMask result = HelperMask::Uncertain;
        if (Immediate(mask.low, 0u) && (waveSize == 32u || Immediate(mask.high, 0u))) result = HelperMask::RealOnly;
        else if (Immediate(mask.low, 0xffffffffu) && (waveSize == 32u || Immediate(mask.high, 0xffffffffu))) result = HelperMask::QuadStable;
        else {
            const IrValue* lowBallot = ballotWord(mask.low, 0u);
            const IrValue* highBallot = waveSize == 32u ? lowBallot : ballotWord(mask.high, 1u);
            if (lowBallot && highBallot) {
                const auto low = onHelper(lowBallot->Argument(0), depth + 1u), high = onHelper(highBallot->Argument(0), depth + 1u);
                if (low && high && *low == 0u && *high == 0u) result = HelperMask::RealOnly;
            } else if (mask.low->Opcode() == IrOpcode::BitwiseAnd32 && (waveSize == 32u || mask.high->Opcode() == IrOpcode::BitwiseAnd32)) {
                const auto pair = [&](unsigned low, unsigned high) { return ReplayMask{mask.low->Argument(low)->Resolve(), waveSize == 32u ? mask.low->Argument(low)->Resolve() : mask.high->Argument(high)->Resolve()}; };
                const auto combine = [&](unsigned firstHigh, unsigned secondHigh) {
                    const auto first = pair(0u, firstHigh), second = pair(1u, secondHigh);
                    if (!maskStable(first, depth + 1u) || !maskStable(second, depth + 1u)) return HelperMask::Uncertain;
                    const auto left = helpers(first, depth + 1u), right = helpers(second, depth + 1u);
                    if (left == HelperMask::RealOnly || right == HelperMask::RealOnly) return HelperMask::RealOnly;
                    if (left == HelperMask::QuadStable && right == HelperMask::QuadStable) return HelperMask::QuadStable;
                    return HelperMask::Uncertain;
                };
                result = combine(0u, 1u);
                if (waveSize != 32u && result != HelperMask::RealOnly) {
                    const auto swapped = combine(1u, 0u);
                    if (swapped == HelperMask::RealOnly || (swapped == HelperMask::QuadStable && result == HelperMask::Uncertain)) result = swapped;
                }
            } else if (mask.low->Opcode() == IrOpcode::CompositeExtractU64 && mask.low->ArgumentCount() == 2u && Immediate(mask.low->Argument(1), 0u)) {
                const IrValue* wqm = mask.low->Argument(0)->Resolve();
                const bool paired = waveSize == 32u || (mask.high->Opcode() == IrOpcode::CompositeExtractU64 && mask.high->ArgumentCount() == 2u && mask.high->Argument(0)->Resolve() == wqm && Immediate(mask.high->Argument(1), 1u));
                if (paired && wqm->Opcode() == IrOpcode::WqmU64 && wqm->ArgumentCount() == 1u) {
                    const IrValue* source = wqm->Argument(0)->Resolve();
                    if (source->Opcode() == IrOpcode::CompositeConstructU64 && source->ArgumentCount() == 2u) {
                        const auto input = helpers(ReplayMask{source->Argument(0)->Resolve(), source->Argument(1)->Resolve()}, depth + 1u);
                        if (input == HelperMask::RealOnly || input == HelperMask::QuadStable) result = HelperMask::QuadStable;
                    }
                }
            } else if (mask.low->IsPhi() && mask.low->ArgumentCount() != 0u && mask.low->ArgumentCount() == mask.low->PhiBlockCount() && (waveSize == 32u || (mask.high->IsPhi() && mask.high->Parent() == mask.low->Parent() && mask.high->ArgumentCount() == mask.high->PhiBlockCount()))) {
                result = HelperMask::RealOnly;
                for (std::size_t index = 0; index < mask.low->PhiBlockCount(); ++index) {
                    const IrValue* high = mask.low->Argument(index)->Resolve();
                    if (waveSize != 32u) {
                        high = nullptr;
                        for (std::size_t highIndex = 0; highIndex < mask.high->PhiBlockCount(); ++highIndex) if (mask.high->PhiBlock(highIndex) == mask.low->PhiBlock(index)) high = mask.high->Argument(highIndex)->Resolve();
                    }
                    const auto incoming = high ? helpers(ReplayMask{mask.low->Argument(index)->Resolve(), high}, depth + 1u) : HelperMask::Uncertain;
                    if (incoming == HelperMask::Uncertain || incoming == HelperMask::Visiting) { result = HelperMask::Uncertain; break; }
                    if (incoming == HelperMask::QuadStable) result = HelperMask::QuadStable;
                }
            }
        }
        helperMasks[key] = result;
        return result;
    }

    bool stable(const IrValue* value, unsigned depth) {
        if (depth > 512u) return false;
        value = value->Resolve();
        if (value->HasImmediate()) return true;
        if (const auto found = stableValues.find(value); found != stableValues.end()) return found->second == 2u;
        stableValues[value] = 1u;
        bool accepted = false;
        if (const auto mask = own(value)) {
            accepted = maskStable(*mask, depth + 1u);
            if (accepted) ownMasks.emplace(value, *mask);
        } else if (value->Opcode() != IrOpcode::Ballot && value->Opcode() != IrOpcode::LaneId && value->Opcode() != IrOpcode::WqmU64) {
            accepted = std::all_of(value->Arguments().begin(), value->Arguments().end(), [&](const IrValue* argument) { return stable(argument, depth + 1u); });
        }
        stableValues[value] = accepted ? 2u : 3u;
        return accepted;
    }

    bool uniform(const IrValue* value, unsigned depth = 0) {
        if (depth > 128u) return false;
        value = value->Resolve();
        if (value->HasImmediate()) return true;
        if (const auto found = uniformValues.find(value); found != uniformValues.end()) return found->second == 2u;
        uniformValues[value] = 1u;
        bool accepted = value->Opcode() == IrOpcode::GetUserData || value->Opcode() == IrOpcode::GetShaderBase || value->Opcode() == IrOpcode::GetSrtResource;
        const bool transfer = Detail::IsRuntimeUniformOp(value->Opcode()) || value->Opcode() == IrOpcode::ReadConst || value->Opcode() == IrOpcode::FPOrdLessThan32 || value->Opcode() == IrOpcode::FPOrdGreaterThan32;
        if (!accepted && transfer && !value->MayHaveSideEffects() && value->ArgumentCount() != 0u) {
            accepted = std::all_of(value->Arguments().begin(), value->Arguments().end(), [&](const IrValue* argument) { return uniform(argument, depth + 1u); });
        }
        uniformValues[value] = accepted ? 2u : 3u;
        return accepted;
    }

    bool paths() {
        const auto& blocks = program.BlockOrder();
        const auto& metadata = program.Metadata().blockInfo;
        if (blocks.empty() || blocks.size() != metadata.size()) return false;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            if (!indices.emplace(metadata[index].id, index).second) return false;
            blockIndices.emplace(blocks[index], index);
        }
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            const auto& term = metadata[index].terminator;
            if (term.loopHeader) return false;
            std::set<const IrBlock*> expected;
            const auto target = [&](std::uint32_t id) {
                const auto found = indices.find(id);
                if (found == indices.end()) return false;
                expected.insert(blocks[found->second]);
                return true;
            };
            if (term.kind == TerminatorKind::Branch) { if (!target(term.trueBlock)) return false; }
            else if (term.kind == TerminatorKind::ConditionalBranch) { if (!metadata[index].condition || !target(term.trueBlock) || !target(term.falseBlock)) return false; }
            else if (term.kind != TerminatorKind::Return) return false;
            const std::set<const IrBlock*> actual(blocks[index]->Successors().begin(), blocks[index]->Successors().end());
            if (actual != expected) return false;
        }
        std::vector<const IrBlock*> path;
        std::set<const IrBlock*> visiting;
        std::function<bool(std::size_t)> visit = [&](std::size_t index) {
            if (path.size() >= 128u || allPaths.size() >= 256u || !visiting.insert(blocks[index]).second) return false;
            path.push_back(blocks[index]);
            const auto& term = metadata[index].terminator;
            bool valid = !term.loopHeader;
            const auto edge = [&](std::uint32_t id) { const auto found = indices.find(id); return found != indices.end() && visit(found->second); };
            if (term.kind == TerminatorKind::Return) allPaths.push_back(path);
            else if (term.kind == TerminatorKind::Branch) valid = valid && edge(term.trueBlock);
            else if (term.kind == TerminatorKind::ConditionalBranch) valid = valid && metadata[index].condition && edge(term.trueBlock) && edge(term.falseBlock);
            else valid = false;
            path.pop_back();
            visiting.erase(blocks[index]);
            return valid;
        };
        return visit(0u);
    }

    const IrValue* live(const std::vector<const IrBlock*>& path) const {
        const IrValue* result = nullptr;
        for (const auto* block : path) {
            for (const auto* instruction : block->Instructions()) {
                if (instruction->Opcode() != IrOpcode::SetAttribute) continue;
                const auto& info = program.Metadata().exportInfo.at(instruction->Flags<ExportFlags>().index);
                if (info.vm) result = instruction->Argument(1)->Resolve();
            }
        }
        return result;
    }

    bool guaranteed(const IrBlock* branch, ReplayMask mask, std::optional<const IrBlock*> continuing = {}) {
        for (const auto& path : allPaths) {
            const auto found = std::find(path.begin(), path.end(), branch);
            if (found == path.end()) continue;
            const IrValue* kept = live(path);
            if (Boolean(kept, false)) continue;
            if (!kept || (continuing && (found + 1 == path.end() || *(found + 1) != *continuing)) || !GuardImpliesMaskOnPath(program, kept, mask.low, mask.high, path)) return false;
        }
        return true;
    }

    std::optional<ReplayMask> any(const IrValue* value) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::INotEqual32) return std::nullopt;
        const IrValue* words = Other(value, 0u);
        if (!words || words->Opcode() != IrOpcode::BitwiseOr32 || words->ArgumentCount() != 2u) return std::nullopt;
        ReplayMask mask{words->Argument(0)->Resolve(), words->Argument(1)->Resolve()};
        if (!maskStable(mask, 0u)) {
            mask = ReplayMask{mask.high, mask.low};
            if (!maskStable(mask, 0u)) return std::nullopt;
        }
        restrict(words);
        restrict(value);
        return mask;
    }

    bool sameValue(const IrValue* left, const IrValue* right) const {
        left = left->Resolve();
        right = right->Resolve();
        return left == right || (left->HasImmediate() && right->HasImmediate() && left->Type() == IrType::U32 && right->Type() == IrType::U32 && left->ImmediateU32() == right->ImmediateU32());
    }

    bool inactiveEquals(const IrValue* value, const IrValue* other, const IrValue* predicate, unsigned depth = 0) {
        if (depth > 32u) return false;
        value = value->Resolve();
        if (sameValue(value, other)) return true;
        if ((value->Opcode() == IrOpcode::SelectU32 || value->Opcode() == IrOpcode::SelectF32 || value->Opcode() == IrOpcode::SelectU1) && value->ArgumentCount() == 3u && (value->Argument(0)->Resolve() == predicate->Resolve() || GuardImplies(program, value->Argument(0), predicate))) return inactiveEquals(value->Argument(2), other, predicate, depth + 1u);
        return false;
    }

    bool quadObserved(const IrValue* value) const {
        std::set<const IrValue*> visited;
        std::vector<const IrValue*> pending{value};
        while (!pending.empty()) {
            const IrValue* current = pending.back();
            pending.pop_back();
            if (!visited.insert(current).second) continue;
            for (const auto& info : program.Metadata().blockInfo) if (info.condition && info.condition->Resolve() == current) return true;
            for (const auto& use : current->OperandUses()) {
                const auto opcode = use.user->Opcode();
                if (ImageOpcodeInfoOf(opcode).needsSampler || opcode == IrOpcode::Ballot || opcode == IrOpcode::WqmU64 || opcode == IrOpcode::ReadLane || opcode == IrOpcode::ReadFirstLane || opcode == IrOpcode::DppMoveU32 || opcode == IrOpcode::DppUpdateU32 || opcode == IrOpcode::SwizzleU32 || opcode == IrOpcode::PermuteU32 || opcode == IrOpcode::BpermuteU32 || opcode == IrOpcode::Permlane16U32 || opcode == IrOpcode::WriteLane || SharedAccessOf(opcode) != SharedAccess::None) return true;
                if (opcode == IrOpcode::SetAttribute || opcode == IrOpcode::Reference || opcode == IrOpcode::ReferenceU32) continue;
                pending.push_back(use.user);
            }
        }
        return false;
    }

    bool helperObserved(const IrValue* value) const {
        std::set<const IrValue*> visited;
        std::vector<const IrValue*> pending{value};
        while (!pending.empty()) {
            const IrValue* current = pending.back();
            pending.pop_back();
            if (!visited.insert(current).second) continue;
            for (const auto& info : program.Metadata().blockInfo) {
                if (!info.condition || info.condition->Resolve() != current) continue;
                const auto kind = info.terminator.condition;
                if (kind != BranchCondition::ExecZero && kind != BranchCondition::ExecNonZero && kind != BranchCondition::VccZero && kind != BranchCondition::VccNonZero) return true;
            }
            for (const auto& use : current->OperandUses()) {
                const auto opcode = use.user->Opcode();
                if (ImageOpcodeInfoOf(opcode).needsSampler) return true;
                if (restricted.contains(use.user) || opcode == IrOpcode::SetAttribute || opcode == IrOpcode::Reference || opcode == IrOpcode::ReferenceU32) continue;
                pending.push_back(use.user);
            }
        }
        return false;
    }

    bool optionalBody(const IrBlock* branch, const IrBlock* body, const IrBlock* merge, const IrValue* predicate) {
        const auto& bodyInfo = program.Metadata().blockInfo[blockIndices.at(body)];
        if (bodyInfo.terminator.kind != TerminatorKind::Branch || indices.at(bodyInfo.terminator.trueBlock) != blockIndices.at(merge)) return false;
        std::set<const IrValue*> merged;
        for (const auto* instruction : merge->Instructions()) {
            if (!instruction->IsPhi()) continue;
            if (instruction->ArgumentCount() != 2u || instruction->PhiBlockCount() != 2u) return false;
            const IrValue* skipped = nullptr;
            const IrValue* taken = nullptr;
            for (std::size_t index = 0; index < 2u; ++index) {
                if (instruction->PhiBlock(index) == branch) skipped = instruction->Argument(index)->Resolve();
                if (instruction->PhiBlock(index) == body) taken = instruction->Argument(index)->Resolve();
            }
            if (!skipped || !taken || !inactiveEquals(taken, skipped, predicate) || quadObserved(instruction)) return false;
            merged.insert(instruction);
        }
        for (const auto* instruction : body->Instructions()) {
            if (instruction->Opcode() == IrOpcode::SetAttribute || instruction->Opcode() == IrOpcode::Discard) return false;
            for (const auto& use : instruction->OperandUses()) {
                if (use.user->Parent() == body || merged.contains(use.user) || use.user->Opcode() == IrOpcode::Reference || use.user->Opcode() == IrOpcode::ReferenceU32) continue;
                return false;
            }
        }
        return !merged.empty();
    }

    bool control(const BlockInfo& info) {
        const auto* branch = program.BlockOrder()[indices.at(info.id)];
        const auto* trueBlock = program.BlockOrder()[indices.at(info.terminator.trueBlock)];
        const auto* falseBlock = program.BlockOrder()[indices.at(info.terminator.falseBlock)];
        const IrValue* condition = info.condition->Resolve();
        const bool waveZero = info.terminator.condition == BranchCondition::ExecZero || info.terminator.condition == BranchCondition::VccZero;
        const bool waveNonZero = info.terminator.condition == BranchCondition::ExecNonZero || info.terminator.condition == BranchCondition::VccNonZero;
        if (waveZero || waveNonZero) {
            const IrValue* predicate = condition;
            if (waveZero) {
                if (condition->Opcode() != IrOpcode::LogicalNot || condition->ArgumentCount() != 1u) return false;
                predicate = condition->Argument(0)->Resolve();
            }
            if (!stable(predicate, 0u)) return false;
            if (uniform(predicate)) return true;
            if (predicate->Opcode() == IrOpcode::LogicalAnd && predicate->ArgumentCount() == 2u) {
                for (unsigned index = 0; index < 2u; ++index) {
                    const auto mask = own(predicate->Argument(index));
                    if (mask && uniform(predicate->Argument(1u - index)) && guaranteed(branch, *mask)) return true;
                }
            }
            const auto* body = waveZero ? falseBlock : trueBlock;
            const auto* merge = waveZero ? trueBlock : falseBlock;
            return optionalBody(branch, body, merge, predicate);
        }
        bool inverted = false;
        if (condition->Opcode() == IrOpcode::LogicalNot && condition->ArgumentCount() == 1u) { condition = condition->Argument(0)->Resolve(); inverted = true; }
        if (const auto mask = any(condition)) {
            if (inverted) restrict(info.condition->Resolve());
            return guaranteed(branch, *mask, inverted ? falseBlock : trueBlock);
        }
        return stable(info.condition, 0u);
    }

    const IrProgram& program;
    std::uint32_t waveSize;
    std::set<const IrValue*> restricted;
    std::map<const IrValue*, ReplayMask> ownMasks;
    std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> masks;
    std::map<std::pair<std::uint32_t, std::uint32_t>, HelperMask> helperMasks;
    std::unordered_map<const IrValue*, std::optional<std::uint32_t>> helperValues;
    std::unordered_map<const IrValue*, unsigned> stableValues;
    std::unordered_map<const IrValue*, unsigned> uniformValues;
    std::map<std::uint32_t, std::size_t> indices;
    std::map<const IrBlock*, std::size_t> blockIndices;
    std::vector<std::vector<const IrBlock*>> allPaths;
};

std::optional<std::set<std::uint32_t>> Values(const IrProgram& program, const IrValue* value, std::set<const IrValue*>& visiting, unsigned depth, std::optional<unsigned> component = {}, const IrValue* active = nullptr) {
    if (value == nullptr || depth > 64u) return std::nullopt;
    value = value->Resolve();
    if (!visiting.insert(value).second) return std::nullopt;
    const auto finish = [&](std::optional<std::set<std::uint32_t>> result) {
        visiting.erase(value);
        if (result && result->size() > 16u) return std::optional<std::set<std::uint32_t>>{};
        return result;
    };
    const auto child = [&](const IrValue* next, std::optional<unsigned> part = {}) { return Values(program, next, visiting, depth + 1u, part, active); };
    if (component) {
        if (value->Opcode() == IrOpcode::CompositeConstructU32x4 && *component < value->ArgumentCount()) return finish(child(value->Argument(*component)));
        return finish(std::nullopt);
    }
    if (value->HasImmediate() && value->Type() == IrType::U32) return finish(std::set<std::uint32_t>{value->ImmediateU32() & 0xffu});
    if (value->Opcode() == IrOpcode::CompositeExtractU32x4 && value->ArgumentCount() == 2u && value->Argument(1)->Resolve()->HasImmediate()) return finish(child(value->Argument(0), value->Argument(1)->Resolve()->ImmediateU32()));
    if (value->Opcode() == IrOpcode::SelectU32 || value->Opcode() == IrOpcode::Select) {
        if (value->ArgumentCount() != 3u) return finish(std::nullopt);
        if (active != nullptr && (Implies(active, value->Argument(0)) || GuardImplies(program, active, value->Argument(0)))) return finish(child(value->Argument(1)));
        auto left = child(value->Argument(1));
        auto right = child(value->Argument(2));
        if (!left || !right) return finish(std::nullopt);
        left->insert(right->begin(), right->end());
        return finish(std::move(left));
    }
    if (value->Opcode() == IrOpcode::Phi) {
        std::set<std::uint32_t> result;
        for (const auto* argument : value->Arguments()) {
            const auto values = child(argument);
            if (!values) return finish(std::nullopt);
            result.insert(values->begin(), values->end());
        }
        return finish(std::move(result));
    }
    return finish(std::nullopt);
}

}

std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program) {
    std::set<std::uint32_t> result;
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            if (instruction->Opcode() != IrOpcode::SetAttribute) continue;
            const auto& info = program.Metadata().exportInfo.at(instruction->Flags<ExportFlags>().index);
            if (info.kind != ExportTargetKind::MrtZ || (info.en & 2u) == 0u) continue;
            std::set<const IrValue*> visiting;
            const auto values = Values(program, instruction->Argument(0), visiting, 0u, 1u, instruction->Argument(1));
            if (!values) return {};
            result.insert(values->begin(), values->end());
            if (result.size() > 16u) return {};
        }
    }
    return {result.begin(), result.end()};
}

bool StencilReferenceDefinedForKeptInvocations(const IrProgram& program) {
    const auto& blocks = program.BlockOrder();
    const auto& metadata = program.Metadata().blockInfo;
    if (blocks.empty() || blocks.size() != metadata.size()) return false;
    std::map<std::uint32_t, std::size_t> indices;
    for (std::size_t index = 0; index < metadata.size(); ++index) if (!indices.emplace(metadata[index].id, index).second) return false;
    std::vector<std::vector<std::size_t>> successors(blocks.size()), predecessors(blocks.size());
    for (std::size_t index = 0; index < metadata.size(); ++index) {
        const auto& term = metadata[index].terminator;
        if (term.loopHeader || (term.kind != TerminatorKind::Branch && term.kind != TerminatorKind::ConditionalBranch && term.kind != TerminatorKind::Return)) return false;
        const auto edge = [&](std::uint32_t id) {
            const auto found = indices.find(id);
            if (found == indices.end()) return false;
            successors[index].push_back(found->second);
            predecessors[found->second].push_back(index);
            return true;
        };
        if (term.kind == TerminatorKind::Branch && !edge(term.trueBlock)) return false;
        if (term.kind == TerminatorKind::ConditionalBranch && (!edge(term.trueBlock) || !edge(term.falseBlock))) return false;
    }
    std::vector<unsigned> reached(blocks.size());
    std::function<bool(std::size_t)> visit = [&](std::size_t index) {
        if (reached[index] == 1u) return false;
        if (reached[index] == 2u) return true;
        reached[index] = 1u;
        for (const auto next : successors[index]) if (!visit(next)) return false;
        reached[index] = 2u;
        return true;
    };
    if (!visit(0u)) return false;
    struct State {
        std::set<const IrValue*> written;
        const IrValue* live = nullptr;
        bool liveKnown = true;
    };
    std::vector<std::optional<State>> states(blocks.size());
    bool valid = true;
    std::function<const State&(std::size_t)> resolve = [&](std::size_t index) -> const State& {
        if (states[index]) return *states[index];
        State state;
        bool first = true;
        for (const auto previous : predecessors[index]) {
            if (reached[previous] != 2u) continue;
            const auto& incoming = resolve(previous);
            if (first) { state = incoming; first = false; }
            else {
                std::set<const IrValue*> common;
                std::set_intersection(state.written.begin(), state.written.end(), incoming.written.begin(), incoming.written.end(), std::inserter(common, common.begin()));
                state.written = std::move(common);
                state.liveKnown = state.liveKnown && incoming.liveKnown && state.live == incoming.live;
            }
        }
        for (const auto* instruction : blocks[index]->Instructions()) {
            if (instruction->Opcode() != IrOpcode::SetAttribute) continue;
            const auto& info = program.Metadata().exportInfo.at(instruction->Flags<ExportFlags>().index);
            const auto* active = instruction->Argument(1)->Resolve();
            if (info.kind == ExportTargetKind::MrtZ && (info.en & 2u) != 0u && !Boolean(active, false)) state.written.insert(Boolean(active, true) ? nullptr : active);
            if (info.vm) {
                state.live = Boolean(active, true) ? nullptr : active;
                state.liveKnown = true;
            }
        }
        if (metadata[index].terminator.kind == TerminatorKind::Return) {
            const bool killed = state.liveKnown && Boolean(state.live, false);
            const bool written = state.written.contains(nullptr) || (state.liveKnown && std::ranges::any_of(state.written, [&](const IrValue* guard) { return Implies(state.live, guard) || GuardImplies(program, state.live, guard); }));
            if (!killed && !written) valid = false;
        }
        states[index] = std::move(state);
        return *states[index];
    };
    for (std::size_t index = 0; index < blocks.size(); ++index) if (reached[index] == 2u) resolve(index);
    return valid;
}

bool LayerReplaySafe(const IrProgram& program) {
    if (program.Info().bdaWrites) return false;
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            const auto opcode = instruction->Opcode();
            if (opcode == IrOpcode::ImageBvhIntersectRay || (BufferAccessOf(opcode) == BufferAccess::Write || BufferAccessOf(opcode) == BufferAccess::Atomic) || AddressOpcodeInfoOf(opcode).access == AddressAccess::Write || AddressOpcodeInfoOf(opcode).access == AddressAccess::Atomic || ImageOpcodeInfoOf(opcode).access == ImageAccess::Write || ImageOpcodeInfoOf(opcode).access == ImageAccess::Atomic) return false;
            if (opcode == IrOpcode::UndefU1 || opcode == IrOpcode::UndefU8 || opcode == IrOpcode::UndefU16 || opcode == IrOpcode::UndefU32 || opcode == IrOpcode::UndefU64) return false;
            if (opcode == IrOpcode::ShaderClock || opcode == IrOpcode::RealtimeClock) return false;
            if (SharedAccessOf(opcode) != SharedAccess::None) {
                const auto memory = instruction->Flags<MemoryFlags>().index;
                if (memory >= program.Resources().memoryInfo.size() || program.Resources().memoryInfo[memory].kind != ResourceKind::Lds) return false;
                continue;
            }
            if (!instruction->MayHaveSideEffects()) continue;
            switch (opcode) {
                case IrOpcode::Barrier:
                case IrOpcode::MeshAllocate:
                case IrOpcode::Branch:
                case IrOpcode::BranchConditional:
                case IrOpcode::Return:
                case IrOpcode::Unreachable:
                case IrOpcode::Discard:
                case IrOpcode::Reference:
                case IrOpcode::ReferenceU32:
                case IrOpcode::SetAttribute:
                case IrOpcode::InterpolateHostP2:
                case IrOpcode::SetRegister:
                case IrOpcode::SetThreadBitScalarRegister:
                case IrOpcode::SetScalarMaskTag:
                case IrOpcode::SetScalarRegister:
                case IrOpcode::SetVectorRegister:
                case IrOpcode::StoreVectorWindow:
                case IrOpcode::SetGotoVariable:
                case IrOpcode::SetScc:
                case IrOpcode::SetExec:
                case IrOpcode::SetExecLo:
                case IrOpcode::SetExecHi:
                case IrOpcode::SetVcc:
                case IrOpcode::SetVccLo:
                case IrOpcode::SetVccHi:
                case IrOpcode::SetM0:
                case IrOpcode::Waitcnt:
                case IrOpcode::InstPrefetch:
                    break;
                case IrOpcode::Sendmsg:
                    if (instruction->ArgumentCount() != 0u) return false;
                    break;
                default:
                    return false;
            }
        }
    }
    return true;
}

bool StencilReplaySafe(const IrProgram& program) {
    if (program.Info().sharedMemoryBytes != 0u || program.Info().bdaWrites || program.Info().usesDma || (program.Info().usesFaultBuffer && program.Resources().guardedSrtSlots.empty())) return false;
    if (program.Info().usesFaultBuffer && !FaultControlFlowAcyclic(program)) return false;
    for (const auto& input : program.Info().inputs) if (input.kind == StageInputKind::PrimitiveId) return false;
    bool masks = false;
    for (const auto& info : program.Metadata().blockInfo) {
        if (info.terminator.loopHeader) return false;
        if (info.terminator.kind != TerminatorKind::ConditionalBranch) continue;
        const auto kind = info.terminator.condition;
        masks = masks || kind == BranchCondition::ExecZero || kind == BranchCondition::ExecNonZero || kind == BranchCondition::VccZero || kind == BranchCondition::VccNonZero;
    }
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            const auto opcode = instruction->Opcode();
            if (opcode == IrOpcode::Loop || opcode == IrOpcode::LoopMerge || opcode == IrOpcode::ImageBvhIntersectRay || BufferAccessOf(opcode) == BufferAccess::Atomic || AddressOpcodeInfoOf(opcode).access == AddressAccess::Write || AddressOpcodeInfoOf(opcode).access == AddressAccess::Atomic || SharedAccessOf(opcode) != SharedAccess::None || ImageOpcodeInfoOf(opcode).access == ImageAccess::Write || ImageOpcodeInfoOf(opcode).access == ImageAccess::Atomic) return false;
            if (opcode == IrOpcode::UndefU1 || opcode == IrOpcode::UndefU8 || opcode == IrOpcode::UndefU16 || opcode == IrOpcode::UndefU32 || opcode == IrOpcode::UndefU64) return false;
            if (opcode == IrOpcode::ShaderClock || opcode == IrOpcode::RealtimeClock || opcode == IrOpcode::ReadFirstLane || opcode == IrOpcode::ReadLane || opcode == IrOpcode::WriteLane || opcode == IrOpcode::DppMoveU32 || opcode == IrOpcode::DppUpdateU32 || opcode == IrOpcode::Permlane16U32 || opcode == IrOpcode::PermuteU32 || opcode == IrOpcode::BpermuteU32 || opcode == IrOpcode::SwizzleU32) return false;
            masks = masks || opcode == IrOpcode::Ballot || opcode == IrOpcode::LaneId || opcode == IrOpcode::WqmU64;
            if (!instruction->MayHaveSideEffects()) continue;
            switch (opcode) {
                case IrOpcode::Branch:
                case IrOpcode::BranchConditional:
                case IrOpcode::Return:
                case IrOpcode::Unreachable:
                case IrOpcode::Discard:
                case IrOpcode::Reference:
                case IrOpcode::ReferenceU32:
                case IrOpcode::SetAttribute:
                case IrOpcode::InterpolateHostP2:
                case IrOpcode::SetRegister:
                case IrOpcode::SetThreadBitScalarRegister:
                case IrOpcode::SetScalarMaskTag:
                case IrOpcode::SetScalarRegister:
                case IrOpcode::SetVectorRegister:
                case IrOpcode::StoreVectorWindow:
                case IrOpcode::SetGotoVariable:
                case IrOpcode::SetScc:
                case IrOpcode::SetExec:
                case IrOpcode::SetExecLo:
                case IrOpcode::SetExecHi:
                case IrOpcode::SetVcc:
                case IrOpcode::SetVccLo:
                case IrOpcode::SetVccHi:
                case IrOpcode::SetM0:
                case IrOpcode::Waitcnt:
                case IrOpcode::InstPrefetch:
                    break;
                case IrOpcode::Sendmsg:
                    if (program.Resources().stage != IrShaderStage::Vertex || instruction->ArgumentCount() != 0u) return false;
                    break;
                default:
                    return false;
            }
        }
    }
    return !masks || MaskReplaySafety(program).Check();
}

}
