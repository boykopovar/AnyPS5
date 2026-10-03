#include "ControlFlow/GraphBuilder.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

std::string toHexString(std::uint32_t value) {
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return std::string(buffer);
}

std::uint32_t instructionEndProgramCounter(const RdnaInstruction& instruction) {
    return instruction.programCounter + instruction.wordCount * 4u;
}

void addUnique(std::vector<std::uint32_t>& values, std::uint32_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void sortUnique(std::vector<std::uint32_t>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::uint32_t remapId(std::uint32_t id, const std::vector<std::uint32_t>& idMap) {
    return id != InvalidControlFlowId && id < idMap.size() ? idMap[id] : id;
}

void remapIds(std::vector<std::uint32_t>& values, const std::vector<std::uint32_t>& idMap) {
    for (auto& value : values) {
        value = remapId(value, idMap);
    }
    sortUnique(values);
}

std::uint32_t estimatedSpirvWords(const RdnaInstruction& instruction) {
    switch (instruction.op) {
    case RdnaOpcode::VReadfirstlaneB32:
    case RdnaOpcode::VReadlaneB32:
    case RdnaOpcode::VWritelaneB32:
    case RdnaOpcode::VPermlane16B32:
    case RdnaOpcode::VPermlanex16B32:
    case RdnaOpcode::DsSwizzleB32:
    case RdnaOpcode::DsBpermuteB32:
        return 120u;
    case RdnaOpcode::DsPermuteB32:
        return 1200u;
    case RdnaOpcode::ImageBvhIntersectRay:
        return 2000u;
    default:
        break;
    }
    switch (instruction.family) {
    case RdnaInstructionFamily::SOPP:
        return 0u;
    case RdnaInstructionFamily::SOP1:
    case RdnaInstructionFamily::SOP2:
    case RdnaInstructionFamily::SOPK:
    case RdnaInstructionFamily::SOPC:
        return 8u;
    case RdnaInstructionFamily::VOP1:
    case RdnaInstructionFamily::VOP2:
    case RdnaInstructionFamily::VOP3:
    case RdnaInstructionFamily::VOP3P:
    case RdnaInstructionFamily::VOPC:
    case RdnaInstructionFamily::VINTRP:
        return 60u;
    case RdnaInstructionFamily::EXP:
        return 20u;
    case RdnaInstructionFamily::MUBUF:
    case RdnaInstructionFamily::MTBUF:
    case RdnaInstructionFamily::FLAT:
        return 120u;
    case RdnaInstructionFamily::SMEM:
        return 180u;
    case RdnaInstructionFamily::MIMG:
        return 220u;
    case RdnaInstructionFamily::DS:
        return 400u;
    case RdnaInstructionFamily::Unknown:
        break;
    }
    return 60u;
}

bool isValidTarget(std::uint32_t target, const std::set<std::uint32_t>& instructionProgramCounters, std::uint32_t firstProgramCounter, std::uint32_t endProgramCounter) {
    return target == endProgramCounter || (target >= firstProgramCounter && instructionProgramCounters.contains(target));
}

bool isRegister(const RdnaOperand& operand, RdnaOperandKind kind, std::uint32_t reg) {
    return operand.kind == kind && operand.reg == reg;
}

bool isImmediate(const RdnaOperand& operand, std::uint32_t& value) {
    if (operand.kind == RdnaOperandKind::IntegerInlineConstant || operand.kind == RdnaOperandKind::LiteralConstant || operand.kind == RdnaOperandKind::FloatInlineConstant) {
        value = operand.value;
        return true;
    }
    return false;
}

bool resolveSetpcTarget(const RdnaProgram& program, std::uint32_t setpcIndex, std::uint32_t& target) {
    if (setpcIndex >= program.instructions.size()) {
        return false;
    }

    const auto& setpc = program.instructions[setpcIndex];
    if (setpc.op != RdnaOpcode::SSetpcB64 || setpc.source0.kind != RdnaOperandKind::ScalarRegister) {
        return false;
    }

    const auto pcRegister = setpc.source0.reg;
    if (setpcIndex >= 2u) {
        const auto& arithmetic = program.instructions[setpcIndex - 1u];
        const auto& getProgramCounter = program.instructions[setpcIndex - 2u];
        if (getProgramCounter.op == RdnaOpcode::SGetpcB64 && getProgramCounter.destination.kind == RdnaOperandKind::ScalarRegister && getProgramCounter.destination.reg == pcRegister && arithmetic.destination.kind == RdnaOperandKind::ScalarRegister && arithmetic.destination.reg == pcRegister) {
            std::uint32_t immediate = 0;
            const bool adds = arithmetic.op == RdnaOpcode::SAddU32 || arithmetic.op == RdnaOpcode::SAddI32;
            const bool subtracts = arithmetic.op == RdnaOpcode::SSubU32 || arithmetic.op == RdnaOpcode::SSubI32;
            if ((adds || subtracts) && (isRegister(arithmetic.source0, RdnaOperandKind::ScalarRegister, pcRegister) || isRegister(arithmetic.source1, RdnaOperandKind::ScalarRegister, pcRegister)) && (isImmediate(arithmetic.source0, immediate) || isImmediate(arithmetic.source1, immediate))) {
                const auto base = instructionEndProgramCounter(getProgramCounter);
                target = (adds ? base + immediate : base - immediate) & ~3u;
                return true;
            }
        }
    }

    if (setpcIndex >= 1u) {
        const auto& getProgramCounter = program.instructions[setpcIndex - 1u];
        if (getProgramCounter.op == RdnaOpcode::SGetpcB64 && getProgramCounter.destination.kind == RdnaOperandKind::ScalarRegister && getProgramCounter.destination.reg == pcRegister) {
            target = instructionEndProgramCounter(getProgramCounter);
            return true;
        }
    }

    return false;
}

struct BoundedJumpTable {
    ControlFlowGraph::CodeTableLoad load;
    std::vector<std::uint32_t> targets;
    std::uint32_t firstProgramCounter = 0;
};

bool sameRegister(const RdnaOperand& first, const RdnaOperand& second) {
    return first.kind == second.kind && first.reg == second.reg &&
        (first.kind == RdnaOperandKind::ScalarRegister || first.kind == RdnaOperandKind::VccLo);
}

bool addsRegister(const RdnaInstruction& instruction, std::uint32_t reg, const RdnaOperand& other) {
    return isRegister(instruction.destination, RdnaOperandKind::ScalarRegister, reg) &&
        ((isRegister(instruction.source0, RdnaOperandKind::ScalarRegister, reg) && sameRegister(instruction.source1, other)) ||
         (isRegister(instruction.source1, RdnaOperandKind::ScalarRegister, reg) && sameRegister(instruction.source0, other)));
}

bool addsImmediate(const RdnaInstruction& instruction, std::uint32_t reg, std::uint32_t& immediate) {
    return isRegister(instruction.destination, RdnaOperandKind::ScalarRegister, reg) &&
        ((isRegister(instruction.source0, RdnaOperandKind::ScalarRegister, reg) && isImmediate(instruction.source1, immediate)) ||
         (isRegister(instruction.source1, RdnaOperandKind::ScalarRegister, reg) && isImmediate(instruction.source0, immediate)));
}

bool resolveJumpTable(const RdnaProgram& program, std::uint32_t index, BoundedJumpTable& result) {
    if (index < 9u) return false;
    const auto& branch = program.instructions[index];
    if (branch.source0.kind != RdnaOperandKind::ScalarRegister) return false;
    const auto pcReg = branch.source0.reg;
    const auto& pc = program.instructions[index - 3u];
    const auto& low = program.instructions[index - 2u];
    const auto& high = program.instructions[index - 1u];
    if (pc.op != RdnaOpcode::SGetpcB64 || !isRegister(pc.destination, RdnaOperandKind::ScalarRegister, pcReg) ||
        low.op != RdnaOpcode::SAddU32 || high.op != RdnaOpcode::SAddcU32) return false;
    for (std::uint32_t loadIndex = index - 3u; loadIndex-- > 5u;) {
        const auto& load = program.instructions[loadIndex];
        if (load.op != RdnaOpcode::SLoadDwordx2) {
            if (load.op == RdnaOpcode::SWaitcnt) continue;
            if (load.family == RdnaInstructionFamily::VOP1 || load.family == RdnaInstructionFamily::VOP2 ||
                load.family == RdnaInstructionFamily::VOP3 || load.family == RdnaInstructionFamily::VOPC) {
                if (load.destination.kind != RdnaOperandKind::ScalarRegister && load.destination2.kind != RdnaOperandKind::ScalarRegister) continue;
            }
            return false;
        }
        if (load.destination.kind != RdnaOperandKind::ScalarRegister || load.source0.kind != RdnaOperandKind::ScalarRegister) return false;
        auto loadedHigh = load.destination;
        ++loadedHigh.reg;
        if (!addsRegister(low, pcReg, load.destination) || !addsRegister(high, pcReg + 1u, loadedHigh)) return false;
        if (pcReg <= loadedHigh.reg && load.destination.reg <= pcReg + 1u) return false;
        const auto& basePc = program.instructions[loadIndex - 3u];
        const auto& baseLow = program.instructions[loadIndex - 2u];
        const auto& baseHigh = program.instructions[loadIndex - 1u];
        const auto& shift = program.instructions[loadIndex - 4u];
        const auto& bound = program.instructions[loadIndex - 5u];
        const auto baseReg = load.source0.reg;
        std::uint32_t displacement = 0, carry = 0, shiftAmount = 0, maximum = 0;
        if (basePc.op != RdnaOpcode::SGetpcB64 || !isRegister(basePc.destination, RdnaOperandKind::ScalarRegister, baseReg) ||
            baseLow.op != RdnaOpcode::SAddU32 || !addsImmediate(baseLow, baseReg, displacement) ||
            baseHigh.op != RdnaOpcode::SAddcU32 || !addsImmediate(baseHigh, baseReg + 1u, carry) || carry != 0u ||
            shift.op != RdnaOpcode::SLshlB32 || !sameRegister(shift.destination, load.source1) ||
            !sameRegister(shift.source0, load.source1) || !isImmediate(shift.source1, shiftAmount) || shiftAmount != 3u ||
            bound.op != RdnaOpcode::SMinU32 || !sameRegister(bound.destination, load.source1) ||
            !sameRegister(bound.source0, load.source1) || !isImmediate(bound.source1, maximum) || maximum > 255u) return false;
        if (load.source1.kind == RdnaOperandKind::ScalarRegister && load.source1.reg >= baseReg && load.source1.reg <= baseReg + 1u) return false;
        for (const auto& instruction : program.instructions) {
            if (IsDirectBranchOpcode(instruction.op) && instruction.branchTarget > bound.programCounter && instruction.branchTarget <= branch.programCounter) return false;
        }
        const std::uint64_t table = static_cast<std::uint64_t>(instructionEndProgramCounter(basePc)) + displacement + load.memoryOffset;
        const std::uint64_t byteCount = (static_cast<std::uint64_t>(maximum) + 1u) * 8u;
        if ((table & 3u) != 0u || table > program.code.size_bytes() || byteCount > program.code.size_bytes() - table) return false;
        result.load.programCounter = load.programCounter;
        result.firstProgramCounter = bound.programCounter;
        for (std::uint32_t entry = 0; entry <= maximum; ++entry) {
            const auto word = static_cast<std::size_t>(table / 4u) + entry * 2u;
            const std::uint64_t value = program.code[word] | (static_cast<std::uint64_t>(program.code[word + 1u]) << 32u);
            const auto offset = std::bit_cast<std::int64_t>(value);
            if (offset < -static_cast<std::int64_t>(instructionEndProgramCounter(pc)) || offset > UINT32_MAX) return false;
            const auto target = static_cast<std::uint64_t>(static_cast<std::int64_t>(instructionEndProgramCounter(pc)) + offset);
            if (target < instructionEndProgramCounter(branch) || target > UINT32_MAX || (target & 3u) != 0u) return false;
            result.load.values.push_back(value);
            result.targets.push_back(static_cast<std::uint32_t>(target));
        }
        return true;
    }
    return false;
}

BranchCondition conditionForOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::SBranch: return BranchCondition::Always;
        case RdnaOpcode::SCbranchScc0: return BranchCondition::SccZero;
        case RdnaOpcode::SCbranchScc1: return BranchCondition::SccNonZero;
        case RdnaOpcode::SCbranchVccz: return BranchCondition::VccZero;
        case RdnaOpcode::SCbranchVccnz: return BranchCondition::VccNonZero;
        case RdnaOpcode::SCbranchExecz: return BranchCondition::ExecZero;
        case RdnaOpcode::SCbranchExecnz: return BranchCondition::ExecNonZero;
        case RdnaOpcode::SSubvectorLoopBegin:
        case RdnaOpcode::SSubvectorLoopEnd: return BranchCondition::ScalarInstruction;
        default: break;
    }
    throw std::logic_error("unreachable branch condition for opcode " + std::to_string(static_cast<int>(opcode)));
}

void rebuildPredecessors(ControlFlowGraph& graph) {
    for (auto& block : graph.blocks) {
        block.predecessors.clear();
        sortUnique(block.successors);
    }
    for (const auto& block : graph.blocks) {
        for (const auto successor : block.successors) {
            addUnique(graph.blocks[successor].predecessors, block.id);
        }
    }
    for (auto& block : graph.blocks) {
        sortUnique(block.predecessors);
    }
}

void pruneUnreachableBlocks(ControlFlowGraph& graph) {
    if (graph.entryBlock >= graph.blocks.size()) {
        throw std::invalid_argument("control flow graph entry block is out of range");
    }

    std::vector<bool> reachable(graph.blocks.size(), false);
    std::vector<std::uint32_t> pending{graph.entryBlock};
    while (!pending.empty()) {
        const auto blockId = pending.back();
        pending.pop_back();
        if (blockId >= graph.blocks.size() || reachable[blockId]) {
            continue;
        }
        reachable[blockId] = true;
        for (const auto successor : graph.blocks[blockId].successors) {
            pending.push_back(successor);
        }
    }

    if (std::ranges::all_of(reachable, [](bool value) { return value; })) {
        return;
    }

    std::vector<std::uint32_t> idMap(graph.blocks.size(), InvalidControlFlowId);
    std::vector<BasicBlock> blocks;
    blocks.reserve(static_cast<std::size_t>(std::ranges::count(reachable, true)));
    for (std::uint32_t oldId = 0; oldId < graph.blocks.size(); ++oldId) {
        if (!reachable[oldId]) {
            continue;
        }
        idMap[oldId] = static_cast<std::uint32_t>(blocks.size());
        blocks.push_back(std::move(graph.blocks[oldId]));
    }

    graph.entryBlock = remapId(graph.entryBlock, idMap);
    graph.blocks = std::move(blocks);
    for (auto& block : graph.blocks) {
        block.id = remapId(block.id, idMap);
        remapIds(block.successors, idMap);
        block.predecessors.clear();
        block.terminator.trueBlock = remapId(block.terminator.trueBlock, idMap);
        block.terminator.falseBlock = remapId(block.terminator.falseBlock, idMap);
    }

    rebuildPredecessors(graph);
}

}

std::vector<BasicBlock> GraphBuilder::splitIntoBlocks(const RdnaProgram& program) const {
    if (program.instructions.empty()) {
        throw std::invalid_argument("cannot build a control flow graph for an empty program");
    }

    const std::uint32_t firstProgramCounter = program.instructions.front().programCounter;
    const std::uint32_t endProgramCounter = instructionEndProgramCounter(program.instructions.back());

    std::set<std::uint32_t> instructionProgramCounters;
    for (const auto& instruction : program.instructions) {
        instructionProgramCounters.insert(instruction.programCounter);
    }

    std::set<std::uint32_t> labels;
    labels.insert(firstProgramCounter);
    labels.insert(endProgramCounter);

    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        const std::uint32_t nextProgramCounter = instructionEndProgramCounter(instruction);

        if (IsDirectBranchOpcode(instruction.op)) {
            if (!isValidTarget(instruction.branchTarget, instructionProgramCounters, firstProgramCounter, endProgramCounter)) {
                throw std::invalid_argument("branch at program counter " + toHexString(instruction.programCounter) + " targets invalid program counter " + toHexString(instruction.branchTarget));
            }
            labels.insert(instruction.branchTarget);
            if (nextProgramCounter <= endProgramCounter) {
                labels.insert(nextProgramCounter);
            }
        } else if (instruction.op == RdnaOpcode::SSetpcB64) {
            std::uint32_t target = 0;
            if (!resolveSetpcTarget(program, index, target)) {
                BoundedJumpTable table;
                if (!resolveJumpTable(program, index, table)) throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(instruction.programCounter));
                for (const auto tableTarget : table.targets) {
                    if (!instructionProgramCounters.contains(tableTarget)) throw std::invalid_argument("jump table targets invalid instruction boundary " + toHexString(tableTarget));
                    labels.insert(tableTarget);
                }
                labels.insert(nextProgramCounter);
                continue;
            }
            if (!isValidTarget(target, instructionProgramCounters, firstProgramCounter, endProgramCounter)) {
                throw std::invalid_argument("s_setpc_b64 at program counter " + toHexString(instruction.programCounter) + " targets invalid program counter " + toHexString(target));
            }
            labels.insert(target);
            if (nextProgramCounter <= endProgramCounter) {
                labels.insert(nextProgramCounter);
            }
        } else if (instruction.op == RdnaOpcode::SEndpgm) {
            labels.insert(nextProgramCounter);
        }
    }

    const std::vector<std::uint32_t> sortedLabels(labels.begin(), labels.end());

    std::vector<BasicBlock> blocks;
    blocks.reserve(sortedLabels.size());
    for (std::size_t i = 0; i < sortedLabels.size(); ++i) {
        const std::uint32_t start = sortedLabels[i];
        if (start > endProgramCounter) {
            continue;
        }
        if (start != endProgramCounter && !instructionProgramCounters.contains(start)) {
            throw std::invalid_argument("control flow graph label does not start on an instruction boundary: " + toHexString(start));
        }

        BasicBlock block;
        block.id = static_cast<std::uint32_t>(blocks.size());
        block.startProgramCounter = start;
        block.endProgramCounter = i + 1u < sortedLabels.size() ? sortedLabels[i + 1u] : endProgramCounter;
        block.instructionBegin = static_cast<std::uint32_t>(std::lower_bound(program.instructions.begin(), program.instructions.end(), block.startProgramCounter, [](const RdnaInstruction& instruction, std::uint32_t programCounter) { return instruction.programCounter < programCounter; }) - program.instructions.begin());
        block.instructionEnd = static_cast<std::uint32_t>(std::lower_bound(program.instructions.begin(), program.instructions.end(), block.endProgramCounter, [](const RdnaInstruction& instruction, std::uint32_t programCounter) { return instruction.programCounter < programCounter; }) - program.instructions.begin());
        blocks.push_back(std::move(block));
    }

    return blocks;
}

void GraphBuilder::linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program) const {
    std::map<std::uint32_t, std::uint32_t> programCounterToBlock;
    for (const auto& block : blocks) {
        programCounterToBlock.emplace(block.startProgramCounter, block.id);
    }

    for (auto& block : blocks) {
        block.terminator = Terminator{};
        if (block.instructionBegin == block.instructionEnd) {
            block.terminator.kind = TerminatorKind::Return;
            continue;
        }

        const auto& last = program.instructions[block.instructionEnd - 1u];
        const std::uint32_t nextProgramCounter = instructionEndProgramCounter(last);

        if (last.op == RdnaOpcode::SEndpgm) {
            block.terminator.kind = TerminatorKind::Return;
        } else if (last.op == RdnaOpcode::SSetpcB64) {
            std::uint32_t target = 0;
            if (!resolveSetpcTarget(program, block.instructionEnd - 1u, target)) {
                BoundedJumpTable table;
                if (!resolveJumpTable(program, block.instructionEnd - 1u, table)) throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(last.programCounter));
                block.terminator.kind = TerminatorKind::IndirectBranch;
                block.terminator.indirectPcSgpr = last.source0.reg;
                sortUnique(table.targets);
                for (const auto tableTarget : table.targets) {
                    block.terminator.indirectTargetProgramCounters.push_back(tableTarget);
                    block.terminator.indirectTargets.push_back(programCounterToBlock.at(tableTarget));
                    addUnique(block.successors, programCounterToBlock.at(tableTarget));
                }
                continue;
            }
            block.terminator.kind = TerminatorKind::Branch;
            block.terminator.condition = BranchCondition::Always;
            block.terminator.trueBlock = programCounterToBlock.at(target);
        } else if (last.op == RdnaOpcode::SBranch) {
            block.terminator.kind = TerminatorKind::Branch;
            block.terminator.condition = BranchCondition::Always;
            block.terminator.trueBlock = programCounterToBlock.at(last.branchTarget);
        } else if (IsConditionalBranchOpcode(last.op)) {
            block.terminator.kind = TerminatorKind::ConditionalBranch;
            block.terminator.condition = conditionForOpcode(last.op);
            block.terminator.trueBlock = programCounterToBlock.at(last.branchTarget);
            const auto fallthrough = programCounterToBlock.find(nextProgramCounter);
            if (fallthrough == programCounterToBlock.end()) {
                throw std::invalid_argument("conditional branch at program counter " + toHexString(last.programCounter) + " has no fallthrough block");
            }
            block.terminator.falseBlock = fallthrough->second;
        } else {
            const auto next = programCounterToBlock.find(block.endProgramCounter);
            if (next != programCounterToBlock.end() && block.endProgramCounter != block.startProgramCounter) {
                block.terminator.kind = TerminatorKind::Branch;
                block.terminator.condition = BranchCondition::Always;
                block.terminator.trueBlock = next->second;
            } else {
                block.terminator.kind = TerminatorKind::Return;
            }
        }

        switch (block.terminator.kind) {
            case TerminatorKind::Branch: addUnique(block.successors, block.terminator.trueBlock); break;
            case TerminatorKind::ConditionalBranch:
                addUnique(block.successors, block.terminator.trueBlock);
                addUnique(block.successors, block.terminator.falseBlock);
                break;
            case TerminatorKind::IndirectBranch:
            case TerminatorKind::Return:
            case TerminatorKind::Unsupported: break;
        }
    }

    for (auto& block : blocks) {
        sortUnique(block.successors);
    }
    for (const auto& block : blocks) {
        for (const auto successor : block.successors) {
            addUnique(blocks[successor].predecessors, block.id);
        }
    }
    for (auto& block : blocks) {
        sortUnique(block.predecessors);
    }
}

ControlFlowGraph GraphBuilder::Build(const RdnaProgram& program) const {
    ControlFlowGraph graph;
    graph.blocks = splitIntoBlocks(program);
    linkBlocks(graph.blocks, program);
    const auto originalSize = graph.blocks.size();
    for (std::uint32_t id = 0; id < originalSize; ++id) {
        const auto term = graph.blocks[id].terminator;
        if (term.kind != TerminatorKind::IndirectBranch) continue;
        BoundedJumpTable table;
        if (!resolveJumpTable(program, graph.blocks[id].instructionEnd - 1u, table)) throw std::logic_error("jump table resolution changed");
        if (table.firstProgramCounter < graph.blocks[id].startProgramCounter) throw std::invalid_argument("jump table bound does not dominate its branch in the same block");
        graph.codeTableLoadProgramCounters.push_back(table.load.programCounter);
        graph.codeTableLoads.push_back(std::move(table.load));
        auto current = id;
        for (std::size_t entry = 0; entry < term.indirectTargets.size(); ++entry) {
            const bool last = entry + 1u == term.indirectTargets.size();
            const auto next = static_cast<std::uint32_t>(graph.blocks.size());
            auto& block = graph.blocks[current];
            block.terminator = Terminator{};
            block.terminator.kind = last ? TerminatorKind::Branch : TerminatorKind::ConditionalBranch;
            block.terminator.condition = last ? BranchCondition::Always : BranchCondition::IndirectTarget;
            block.terminator.trueBlock = term.indirectTargets[entry];
            block.terminator.indirectPcSgpr = term.indirectPcSgpr;
            block.terminator.indirectTargetProgramCounters = {term.indirectTargetProgramCounters[entry]};
            block.successors = {term.indirectTargets[entry]};
            if (!last) {
                block.terminator.falseBlock = next;
                block.successors.push_back(next);
                BasicBlock comparison;
                comparison.id = next;
                comparison.startProgramCounter = block.endProgramCounter;
                comparison.endProgramCounter = block.endProgramCounter;
                comparison.instructionBegin = block.instructionEnd;
                comparison.instructionEnd = block.instructionEnd;
                graph.blocks.push_back(std::move(comparison));
                current = next;
            }
        }
    }
    rebuildPredecessors(graph);
    graph.entryBlock = 0;
    pruneUnreachableBlocks(graph);
    for (auto& block : graph.blocks) {
        for (auto index = block.instructionBegin; index < block.instructionEnd; ++index) {
            block.estimatedSpirvWords += estimatedSpirvWords(program.instructions[index]);
        }
    }
    return graph;
}

}
