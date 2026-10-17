#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/ControlFlowHelpers.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

std::uint32_t instructionEndProgramCounter(const RdnaInstruction& instruction) {
    return instruction.programCounter + instruction.wordCount * 4u;
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
    case RdnaOpcode::ImageBvh64IntersectRay:
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

constexpr std::uint32_t NoScalarRegister = UINT32_MAX;

std::uint32_t scalarIndex(const RdnaOperand& operand) {
    switch (operand.kind) {
        case RdnaOperandKind::ScalarRegister: return operand.reg;
        case RdnaOperandKind::VccLo: return 106u;
        case RdnaOperandKind::VccHi: return 107u;
        default: return NoScalarRegister;
    }
}

bool isScalar(const RdnaOperand& operand, std::uint32_t index) {
    return index != NoScalarRegister && scalarIndex(operand) == index;
}

bool addsImmediateTo(const RdnaInstruction& instruction, std::uint32_t index, std::uint32_t& immediate) {
    return isScalar(instruction.destination, index) &&
        ((isScalar(instruction.source0, index) && isImmediate(instruction.source1, immediate)) ||
         (isScalar(instruction.source1, index) && isImmediate(instruction.source0, immediate)));
}

bool subtractsImmediateFrom(const RdnaInstruction& instruction, std::uint32_t index, std::uint32_t& immediate) {
    return isScalar(instruction.destination, index) && isScalar(instruction.source0, index) && isImmediate(instruction.source1, immediate);
}

bool resolveLongSetpcTarget(const RdnaProgram& program, std::uint32_t setpcIndex, std::uint32_t& target) {
    if (setpcIndex < 3u) return false;
    const auto& setpc = program.instructions[setpcIndex];
    const auto& pc = program.instructions[setpcIndex - 3u];
    const auto& low = program.instructions[setpcIndex - 2u];
    const auto& high = program.instructions[setpcIndex - 1u];
    const auto pcRegister = scalarIndex(setpc.source0);
    const bool adds = low.op == RdnaOpcode::SAddU32 && high.op == RdnaOpcode::SAddcU32;
    const bool subtracts = low.op == RdnaOpcode::SSubU32 && high.op == RdnaOpcode::SSubbU32;
    std::uint32_t lowImmediate = 0, highImmediate = 0;
    if (!(setpc.op == RdnaOpcode::SSetpcB64 || setpc.op == RdnaOpcode::SSwappcB64) || pcRegister == NoScalarRegister || pcRegister % 2u != 0u || pc.op != RdnaOpcode::SGetpcB64 || !isScalar(pc.destination, pcRegister)) return false;
    if (adds ? !addsImmediateTo(low, pcRegister, lowImmediate) || !addsImmediateTo(high, pcRegister + 1u, highImmediate)
             : !subtracts || !subtractsImmediateFrom(low, pcRegister, lowImmediate) || !subtractsImmediateFrom(high, pcRegister + 1u, highImmediate)) return false;
    const auto offset = (static_cast<std::uint64_t>(highImmediate) << 32u) | lowImmediate;
    const auto base = static_cast<std::uint64_t>(instructionEndProgramCounter(pc));
    const auto destination = adds ? base + offset : base - offset;
    if (destination > UINT32_MAX || (destination & 3u) != 0u) return false;
    target = static_cast<std::uint32_t>(destination);
    return true;
}

bool resolveSetpcTarget(const RdnaProgram& program, std::uint32_t setpcIndex, std::uint32_t& target) {
    if (setpcIndex >= program.instructions.size()) {
        return false;
    }
    if (resolveLongSetpcTarget(program, setpcIndex, target)) {
        return true;
    }

    const auto& setpc = program.instructions[setpcIndex];
    if (!(setpc.op == RdnaOpcode::SSetpcB64 || setpc.op == RdnaOpcode::SSwappcB64) || setpc.source0.kind != RdnaOperandKind::ScalarRegister) {
        return false;
    }

    const auto pcRegister = setpc.source0.reg;
    if (setpcIndex >= 2u) {
        const auto& arithmetic = program.instructions[setpcIndex - 1u];
        const auto& getProgramCounter = program.instructions[setpcIndex - 2u];
        if (getProgramCounter.op == RdnaOpcode::SGetpcB64 && getProgramCounter.destination.kind == RdnaOperandKind::ScalarRegister && getProgramCounter.destination.reg == pcRegister && arithmetic.destination.kind == RdnaOperandKind::ScalarRegister && arithmetic.destination.reg == pcRegister) {
            std::uint32_t immediate = 0;
            const bool adds = arithmetic.op == RdnaOpcode::SAddU32 || arithmetic.op == RdnaOpcode::SAddI32;
            const bool subtracts = (arithmetic.op == RdnaOpcode::SSubU32 || arithmetic.op == RdnaOpcode::SSubI32) && isRegister(arithmetic.source0, RdnaOperandKind::ScalarRegister, pcRegister);
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

bool writesScalar(const RdnaInstruction& instruction, std::uint32_t index, std::uint32_t waveSize = 0u) {
    std::uint32_t count = instruction.dataDwordCount;
    switch (instruction.family) {
        case RdnaInstructionFamily::SOP1:
        case RdnaInstructionFamily::SOP2:
        case RdnaInstructionFamily::SOPK:
            if (instruction.op == RdnaOpcode::SMovreldB32 || instruction.op == RdnaOpcode::SMovreldB64 || instruction.op == RdnaOpcode::SMovrelsd2B32) return true;
            break;
        case RdnaInstructionFamily::SMEM:
            if (instruction.op == RdnaOpcode::SMemtime || instruction.op == RdnaOpcode::SMemrealtime) count = 2u;
            break;
        case RdnaInstructionFamily::VOP1:
        case RdnaInstructionFamily::VOP2:
        case RdnaInstructionFamily::VOP3:
        case RdnaInstructionFamily::VOP3P:
        case RdnaInstructionFamily::VOPC:
            count = instruction.op == RdnaOpcode::VReadlaneB32 || instruction.op == RdnaOpcode::VReadfirstlaneB32 || waveSize == 32u ? 1u : 2u;
            break;
        case RdnaInstructionFamily::SOPC:
        case RdnaInstructionFamily::SOPP:
        case RdnaInstructionFamily::VINTRP:
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF:
        case RdnaInstructionFamily::FLAT:
        case RdnaInstructionFamily::DS:
        case RdnaInstructionFamily::MIMG:
        case RdnaInstructionFamily::EXP: return false;
        case RdnaInstructionFamily::Unknown: return true;
    }
    const auto covers = [&](const RdnaOperand& operand) {
        const auto first = scalarIndex(operand);
        return first != NoScalarRegister && index >= first && index - first < count;
    };
    return covers(instruction.destination) || covers(instruction.destination2);
}

bool findLastWriter(const RdnaProgram& program, std::uint32_t end, std::initializer_list<std::uint32_t> indices, std::uint32_t& writer, std::uint32_t waveSize = 0u, std::uint32_t skipBranchesPast = 0u) {
    for (auto position = end; position-- > 0u;) {
        const auto& instruction = program.instructions[position];
        const bool skipped = skipBranchesPast != 0u && IsDirectBranchOpcode(instruction.op) && instruction.op != RdnaOpcode::SBranch && instruction.branchTarget > skipBranchesPast;
        if ((IsDirectBranchOpcode(instruction.op) && !skipped) || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SEndpgm) return false;
        if (std::ranges::any_of(indices, [&](std::uint32_t index) { return writesScalar(instruction, index, waveSize); })) {
            writer = position;
            return true;
        }
    }
    return false;
}

bool resolveDwordJumpTable(const RdnaProgram& program, std::uint32_t index, BoundedJumpTable& result) {
    if (index < 2u) return false;
    const auto& branch = program.instructions[index];
    const auto& low = program.instructions[index - 2u];
    const auto& high = program.instructions[index - 1u];
    if (branch.source0.kind != RdnaOperandKind::ScalarRegister || branch.source0.reg % 2u != 0u) return false;
    const auto pcReg = branch.source0.reg;
    const auto entryReg = scalarIndex(low.source1);
    std::uint32_t borrow = 0;
    if (low.op != RdnaOpcode::SSubU32 || !isScalar(low.destination, pcReg) || !isScalar(low.source0, pcReg) ||
        entryReg == NoScalarRegister || entryReg == pcReg || entryReg == pcReg + 1u ||
        high.op != RdnaOpcode::SSubbU32 || !subtractsImmediateFrom(high, pcReg + 1u, borrow) || borrow != 0u) return false;
    std::uint32_t loadIndex = 0, shiftIndex = 0, boundIndex = 0, baseIndex = 0;
    if (!findLastWriter(program, index - 2u, {entryReg, pcReg, pcReg + 1u}, loadIndex)) return false;
    const auto& load = program.instructions[loadIndex];
    const auto offsetReg = scalarIndex(load.source1);
    if (load.op != RdnaOpcode::SLoadDword || !isScalar(load.destination, entryReg) || writesScalar(load, pcReg) || writesScalar(load, pcReg + 1u) ||
        !isScalar(load.source0, pcReg) || offsetReg == NoScalarRegister || offsetReg == pcReg || offsetReg == pcReg + 1u) return false;
    if (!findLastWriter(program, loadIndex, {offsetReg}, shiftIndex) || !findLastWriter(program, shiftIndex, {offsetReg}, boundIndex) ||
        !findLastWriter(program, loadIndex, {pcReg, pcReg + 1u}, baseIndex) || baseIndex < 2u) return false;
    const auto& shift = program.instructions[shiftIndex];
    const auto& bound = program.instructions[boundIndex];
    const auto& basePc = program.instructions[baseIndex - 2u];
    const auto& baseLow = program.instructions[baseIndex - 1u];
    const auto& baseHigh = program.instructions[baseIndex];
    std::uint32_t shiftAmount = 0, maximum = 0, displacement = 0, carry = 0;
    if (shift.op != RdnaOpcode::SLshlB32 || !isScalar(shift.destination, offsetReg) || !isScalar(shift.source0, offsetReg) ||
        !isImmediate(shift.source1, shiftAmount) || shiftAmount != 2u ||
        bound.op != RdnaOpcode::SMinU32 || !isScalar(bound.destination, offsetReg) ||
        !(isImmediate(bound.source1, maximum) || isImmediate(bound.source0, maximum)) || maximum > 255u ||
        basePc.op != RdnaOpcode::SGetpcB64 || !isScalar(basePc.destination, pcReg) ||
        baseLow.op != RdnaOpcode::SAddU32 || !addsImmediateTo(baseLow, pcReg, displacement) ||
        baseHigh.op != RdnaOpcode::SAddcU32 || !addsImmediateTo(baseHigh, pcReg + 1u, carry) || carry != 0u) return false;
    const auto& first = program.instructions[std::min(boundIndex, baseIndex - 2u)];
    for (const auto& instruction : program.instructions) {
        if (IsDirectBranchOpcode(instruction.op) && instruction.branchTarget > first.programCounter && instruction.branchTarget <= branch.programCounter) return false;
    }
    const auto base = static_cast<std::int64_t>(instructionEndProgramCounter(basePc)) + displacement;
    const auto table = base + static_cast<std::int32_t>(load.memoryOffset);
    const auto byteCount = (static_cast<std::int64_t>(maximum) + 1) * 4;
    if (table < 0 || (table & 3) != 0 || table + byteCount > static_cast<std::int64_t>(program.code.size_bytes())) return false;
    BoundedJumpTable resolved;
    resolved.load.programCounter = load.programCounter;
    resolved.firstProgramCounter = first.programCounter;
    for (std::uint32_t entry = 0; entry <= maximum; ++entry) {
        const auto value = program.code[static_cast<std::size_t>(table / 4) + entry];
        const auto target = base - static_cast<std::int64_t>(value);
        if (target < static_cast<std::int64_t>(instructionEndProgramCounter(branch)) || target > UINT32_MAX || (target & 3) != 0) return false;
        resolved.load.values.push_back(value);
        resolved.targets.push_back(static_cast<std::uint32_t>(target));
    }
    result = std::move(resolved);
    return true;
}

bool resolveBoundedJumpTable(const RdnaProgram& program, std::uint32_t index, BoundedJumpTable& result) {
    if (resolveJumpTable(program, index, result)) return true;
    result = BoundedJumpTable{};
    return resolveDwordJumpTable(program, index, result);
}

bool pairOverlapsRegister(const RdnaOperand& operand, std::uint32_t linkRegister, std::uint32_t count = 1u) {
    const std::uint32_t slot = scalarIndex(operand);
    return slot != NoScalarRegister && slot <= linkRegister + 1u && linkRegister < slot + count;
}

bool readsLinkRegister(const RdnaInstruction& instruction, std::uint32_t linkRegister) {
    if (instruction.op == RdnaOpcode::SMovrelsB32 || instruction.op == RdnaOpcode::SMovrelsB64) return true;
    std::uint32_t source0Count = 1u, source1Count = 1u, source2Count = 1u;
    switch (instruction.family) {
        case RdnaInstructionFamily::SMEM:
            source0Count = 2u;
            switch (instruction.op) {
                case RdnaOpcode::SBufferLoadDword:
                case RdnaOpcode::SBufferLoadDwordx2:
                case RdnaOpcode::SBufferLoadDwordx4:
                case RdnaOpcode::SBufferLoadDwordx8:
                case RdnaOpcode::SBufferLoadDwordx16: source0Count = 4u; break;
                default: break;
            }
            break;
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF: source1Count = 4u; break;
        case RdnaInstructionFamily::MIMG:
            source1Count = instruction.imageR128 ? 4u : 8u;
            source2Count = 4u;
            break;
        case RdnaInstructionFamily::FLAT:
            source0Count = 2u;
            source1Count = instruction.memorySegment == 2u ? 2u : 1u;
            break;
        default: break;
    }
    return pairOverlapsRegister(instruction.source0, linkRegister, source0Count) || pairOverlapsRegister(instruction.source1, linkRegister, source1Count) ||
        pairOverlapsRegister(instruction.source2, linkRegister, source2Count) || pairOverlapsRegister(instruction.source3, linkRegister);
}

bool touchesLinkRegister(const RdnaInstruction& instruction, std::uint32_t linkRegister) {
    return writesScalar(instruction, linkRegister) || writesScalar(instruction, linkRegister + 1u) ||
        pairOverlapsRegister(instruction.destination, linkRegister) || pairOverlapsRegister(instruction.destination2, linkRegister) ||
        readsLinkRegister(instruction, linkRegister);
}

bool isScalarLoad(RdnaOpcode op) {
    switch (op) {
        case RdnaOpcode::SLoadDword:
        case RdnaOpcode::SLoadDwordx2:
        case RdnaOpcode::SLoadDwordx4:
        case RdnaOpcode::SLoadDwordx8:
        case RdnaOpcode::SLoadDwordx16:
        case RdnaOpcode::SBufferLoadDword:
        case RdnaOpcode::SBufferLoadDwordx2:
        case RdnaOpcode::SBufferLoadDwordx4:
        case RdnaOpcode::SBufferLoadDwordx8:
        case RdnaOpcode::SBufferLoadDwordx16: return true;
        default: return false;
    }
}

bool onlyWritesLinkRegister(const RdnaInstruction& instruction, std::uint32_t linkRegister) {
    const bool overwrites = instruction.family == RdnaInstructionFamily::SOP2 || instruction.op == RdnaOpcode::SMovB32 || instruction.op == RdnaOpcode::SMovB64 ||
        (instruction.family == RdnaInstructionFamily::SMEM && isScalarLoad(instruction.op));
    return overwrites && !readsLinkRegister(instruction, linkRegister);
}

bool isFetchCallPrefixOpcode(const RdnaInstruction& instruction) {
    return IsScalarAluOpcode(instruction.op) || instruction.op == RdnaOpcode::SNop || instruction.op == RdnaOpcode::SWaitcnt ||
        instruction.op == RdnaOpcode::SWaitcntDepctr || instruction.op == RdnaOpcode::SWaitIdle || instruction.op == RdnaOpcode::SIcacheInv ||
        instruction.op == RdnaOpcode::SSetprio || instruction.op == RdnaOpcode::SClause;
}

std::uint32_t instructionIndexOfProgramCounter(const RdnaProgram& program, std::uint32_t programCounter) {
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        if (program.instructions[index].programCounter == programCounter) {
            return index;
        }
    }
    return InvalidControlFlowId;
}

const SwappcCall* findCallAt(const std::vector<SwappcCall>& calls, std::uint32_t index) {
    for (const auto& call : calls) {
        if (call.callIndex == index) {
            return &call;
        }
    }
    return nullptr;
}

const SwappcCall* findReturnAt(const std::vector<SwappcCall>& calls, std::uint32_t index) {
    for (const auto& call : calls) {
        if (!call.fetch && call.returnIndex == index) {
            return &call;
        }
    }
    return nullptr;
}

bool isFetchCall(const RdnaProgram& program, std::uint32_t index, const SwappcInfo* swappc, bool outsideProgram) {
    const auto& instruction = program.instructions[index];
    const bool positional = std::all_of(program.instructions.begin(), program.instructions.begin() + index, isFetchCallPrefixOpcode);
    const std::uint32_t targetRegister = scalarIndex(instruction.source0);
    const bool userDataPair = swappc != nullptr && targetRegister != NoScalarRegister &&
        targetRegister >= swappc->userDataBaseRegister && targetRegister + 1u < swappc->userDataBaseRegister + swappc->userDataCount &&
        std::none_of(program.instructions.begin(), program.instructions.end(), [&](const RdnaInstruction& other) {
            return writesScalar(other, targetRegister) || writesScalar(other, targetRegister + 1u);
        });
    return swappc != nullptr && swappc->fetchCallAllowed && positional && (userDataPair || outsideProgram);
}

bool overwritesScalar(const RdnaInstruction& instruction, std::uint32_t reg, std::uint32_t waveSize) {
    const auto first = scalarIndex(instruction.destination);
    switch (instruction.family) {
        case RdnaInstructionFamily::SMEM:
            return isScalarLoad(instruction.op) && first != NoScalarRegister && reg >= first && reg - first < instruction.dataDwordCount;
        case RdnaInstructionFamily::SOP1:
            switch (instruction.op) {
                case RdnaOpcode::SMovreldB32:
                case RdnaOpcode::SMovreldB64:
                case RdnaOpcode::SMovrelsd2B32:
                case RdnaOpcode::SCmovB32:
                case RdnaOpcode::SCmovB64:
                case RdnaOpcode::SBitset0B32:
                case RdnaOpcode::SBitset0B64:
                case RdnaOpcode::SBitset1B32:
                case RdnaOpcode::SBitset1B64:
                case RdnaOpcode::SSwappcB64:
                case RdnaOpcode::SSetpcB64: return false;
                case RdnaOpcode::SMovB64:
                case RdnaOpcode::SGetpcB64: return first != NoScalarRegister && reg >= first && reg - first < 2u;
                default: return first != NoScalarRegister && reg == first;
            }
        case RdnaInstructionFamily::SOP2: return first != NoScalarRegister && reg == first;
        case RdnaInstructionFamily::SOPK: return instruction.op == RdnaOpcode::SMovkI32 && reg == first;
        case RdnaInstructionFamily::VOP1:
        case RdnaInstructionFamily::VOP2:
        case RdnaInstructionFamily::VOP3:
        case RdnaInstructionFamily::VOP3P:
        case RdnaInstructionFamily::VOPC: {
            const std::uint32_t count = instruction.op != RdnaOpcode::VReadlaneB32 && instruction.op != RdnaOpcode::VReadfirstlaneB32 && waveSize == 64u ? 2u : 1u;
            return std::ranges::any_of(std::array{&instruction.destination, &instruction.destination2}, [&](const RdnaOperand* operand) {
                const auto slot = scalarIndex(*operand);
                return slot != NoScalarRegister && reg >= slot && reg - slot < count;
            });
        }
        default: return false;
    }
}

bool readsVectorSources64(RdnaOpcode op) {
    switch (op) {
        case RdnaOpcode::VMadU64U32:
        case RdnaOpcode::VMadI64I32:
        case RdnaOpcode::VFmaF64:
        case RdnaOpcode::VAddF64:
        case RdnaOpcode::VMulF64:
        case RdnaOpcode::VMinF64:
        case RdnaOpcode::VMaxF64:
        case RdnaOpcode::VLdexpF64:
        case RdnaOpcode::VCvtI32F64:
        case RdnaOpcode::VCvtF64I32:
        case RdnaOpcode::VCvtF32F64:
        case RdnaOpcode::VCvtF64F32:
        case RdnaOpcode::VCvtU32F64:
        case RdnaOpcode::VCvtF64U32:
        case RdnaOpcode::VTruncF64:
        case RdnaOpcode::VCeilF64:
        case RdnaOpcode::VRndneF64:
        case RdnaOpcode::VFloorF64:
        case RdnaOpcode::VFrexpExpI32F64:
        case RdnaOpcode::VFrexpMantF64:
        case RdnaOpcode::VFractF64:
        case RdnaOpcode::VRcpF64:
        case RdnaOpcode::VRsqF64:
        case RdnaOpcode::VSqrtF64:
        case RdnaOpcode::VTrigPreopF64:
        case RdnaOpcode::VDivScaleF64:
        case RdnaOpcode::VDivFmasF64:
        case RdnaOpcode::VDivFixupF64:
        case RdnaOpcode::VLshlrevB64:
        case RdnaOpcode::VLshrrevB64:
        case RdnaOpcode::VAshrrevI64:
        case RdnaOpcode::VCmpEqI64:
        case RdnaOpcode::VCmpLtU64:
        case RdnaOpcode::VCmpEqU64:
        case RdnaOpcode::VCmpGtU64:
        case RdnaOpcode::VCmpNeU64:
        case RdnaOpcode::VCmpxNeI64:
        case RdnaOpcode::VCmpxNeU64:
        case RdnaOpcode::VCmpFI64:
        case RdnaOpcode::VCmpLtI64:
        case RdnaOpcode::VCmpLeI64:
        case RdnaOpcode::VCmpGtI64:
        case RdnaOpcode::VCmpNeI64:
        case RdnaOpcode::VCmpGeI64:
        case RdnaOpcode::VCmpTI64:
        case RdnaOpcode::VCmpxFI64:
        case RdnaOpcode::VCmpxLtI64:
        case RdnaOpcode::VCmpxEqI64:
        case RdnaOpcode::VCmpxLeI64:
        case RdnaOpcode::VCmpxGtI64:
        case RdnaOpcode::VCmpxGeI64:
        case RdnaOpcode::VCmpxTI64:
        case RdnaOpcode::VCmpFU64:
        case RdnaOpcode::VCmpLeU64:
        case RdnaOpcode::VCmpGeU64:
        case RdnaOpcode::VCmpTU64:
        case RdnaOpcode::VCmpxFU64:
        case RdnaOpcode::VCmpxLtU64:
        case RdnaOpcode::VCmpxEqU64:
        case RdnaOpcode::VCmpxLeU64:
        case RdnaOpcode::VCmpxGtU64:
        case RdnaOpcode::VCmpxGeU64:
        case RdnaOpcode::VCmpxTU64:
        case RdnaOpcode::VCmpFF64:
        case RdnaOpcode::VCmpLtF64:
        case RdnaOpcode::VCmpEqF64:
        case RdnaOpcode::VCmpLeF64:
        case RdnaOpcode::VCmpGtF64:
        case RdnaOpcode::VCmpLgF64:
        case RdnaOpcode::VCmpGeF64:
        case RdnaOpcode::VCmpOF64:
        case RdnaOpcode::VCmpUF64:
        case RdnaOpcode::VCmpNgeF64:
        case RdnaOpcode::VCmpNlgF64:
        case RdnaOpcode::VCmpNgtF64:
        case RdnaOpcode::VCmpNleF64:
        case RdnaOpcode::VCmpNeqF64:
        case RdnaOpcode::VCmpNltF64:
        case RdnaOpcode::VCmpTruF64:
        case RdnaOpcode::VCmpClassF64:
        case RdnaOpcode::VCmpxFF64:
        case RdnaOpcode::VCmpxLtF64:
        case RdnaOpcode::VCmpxEqF64:
        case RdnaOpcode::VCmpxLeF64:
        case RdnaOpcode::VCmpxGtF64:
        case RdnaOpcode::VCmpxLgF64:
        case RdnaOpcode::VCmpxGeF64:
        case RdnaOpcode::VCmpxOF64:
        case RdnaOpcode::VCmpxUF64:
        case RdnaOpcode::VCmpxNgeF64:
        case RdnaOpcode::VCmpxNlgF64:
        case RdnaOpcode::VCmpxNgtF64:
        case RdnaOpcode::VCmpxNleF64:
        case RdnaOpcode::VCmpxNeqF64:
        case RdnaOpcode::VCmpxNltF64:
        case RdnaOpcode::VCmpxTruF64:
        case RdnaOpcode::VCmpxClassF64:
            return true;
        default: return false;
    }
}

bool readsScalarDword(const RdnaInstruction& instruction, std::uint32_t reg, std::uint32_t waveSize) {
    if (instruction.op == RdnaOpcode::SMovrelsB32 || instruction.op == RdnaOpcode::SMovrelsB64) return true;
    const auto overlaps = [&](const RdnaOperand& operand, std::uint32_t count) {
        const auto slot = scalarIndex(operand);
        return slot != NoScalarRegister && slot <= reg && reg - slot < count;
    };
    std::uint32_t source0Count = 1u, source1Count = 1u, source2Count = 1u, source3Count = 1u;
    switch (instruction.family) {
        case RdnaInstructionFamily::SMEM:
            switch (instruction.op) {
                case RdnaOpcode::SBufferLoadDword:
                case RdnaOpcode::SBufferLoadDwordx2:
                case RdnaOpcode::SBufferLoadDwordx4:
                case RdnaOpcode::SBufferLoadDwordx8:
                case RdnaOpcode::SBufferLoadDwordx16: source0Count = 4u; break;
                default: source0Count = 2u; break;
            }
            break;
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF: source1Count = 4u; break;
        case RdnaInstructionFamily::MIMG:
            source1Count = instruction.imageR128 ? 4u : 8u;
            source2Count = 4u;
            break;
        case RdnaInstructionFamily::FLAT:
            source0Count = 2u;
            source1Count = 2u;
            break;
        case RdnaInstructionFamily::SOPK:
            if (instruction.op != RdnaOpcode::SMovkI32 && instruction.op != RdnaOpcode::SCallB64 && overlaps(instruction.destination, 1u)) return true;
            break;
        case RdnaInstructionFamily::SOP1:
        case RdnaInstructionFamily::SOP2:
        case RdnaInstructionFamily::SOPC:
            switch (instruction.op) {
                case RdnaOpcode::SBcnt0I32B64:
                case RdnaOpcode::SBcnt1I32B64:
                case RdnaOpcode::SFf0I32B64:
                case RdnaOpcode::SFf1I32B64:
                case RdnaOpcode::SFlbitI32B64:
                case RdnaOpcode::SFlbitI32I64:
                case RdnaOpcode::SCmpEqU64:
                case RdnaOpcode::SCmpLgU64:
                case RdnaOpcode::SBitcmp0B64:
                case RdnaOpcode::SBitcmp1B64: source0Count = source1Count = 2u; break;
                default: source0Count = source1Count = instruction.family == RdnaInstructionFamily::SOPC ? 1u : std::max(instruction.dataDwordCount, 1u); break;
            }
            break;
        case RdnaInstructionFamily::VOP1:
        case RdnaInstructionFamily::VOP2:
        case RdnaInstructionFamily::VOP3:
        case RdnaInstructionFamily::VOP3P:
        case RdnaInstructionFamily::VOPC:
            source0Count = source1Count = source2Count = source3Count = readsVectorSources64(instruction.op) ? 2u : 1u;
            switch (instruction.op) {
                case RdnaOpcode::VCndmaskB32:
                case RdnaOpcode::VAddcU32:
                case RdnaOpcode::VSubCoCiU32:
                case RdnaOpcode::VSubrevCoCiU32:
                case RdnaOpcode::VDivFmasF32:
                case RdnaOpcode::VDivFmasF64: source2Count = std::max(source2Count, waveSize == 32u ? 1u : 2u); break;
                default: break;
            }
            break;
        default: break;
    }
    switch (instruction.op) {
        case RdnaOpcode::SBitset0B32:
        case RdnaOpcode::SBitset0B64:
        case RdnaOpcode::SBitset1B32:
        case RdnaOpcode::SBitset1B64:
        case RdnaOpcode::SCmovB32:
        case RdnaOpcode::SCmovB64:
            if (overlaps(instruction.destination, 2u)) return true;
            break;
        default: break;
    }
    return overlaps(instruction.source0, source0Count) || overlaps(instruction.source1, source1Count) || overlaps(instruction.source2, source2Count) || overlaps(instruction.source3, source3Count);
}

template <typename TError>
void requireReturnAddressesStayInCalls(const RdnaProgram& program, const std::vector<SwappcCall>& calls, const std::vector<std::uint32_t>& unlinked, std::uint32_t waveSize, const TError& escapeError) {
    std::set<std::uint32_t> links;
    for (const auto& call : calls) {
        if (!call.fetch) links.insert(call.linkRegister);
    }
    const auto size = static_cast<std::uint32_t>(program.instructions.size());
    const auto successors = [&](std::uint32_t index) {
        std::vector<std::uint32_t> next;
        const auto& instruction = program.instructions[index];
        if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) return next;
        if (IsDirectBranchOpcode(instruction.op)) {
            next.push_back(instructionIndexOfProgramCounter(program, instruction.branchTarget));
            if (IsConditionalBranchOpcode(instruction.op)) next.push_back(index + 1u);
        } else if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) {
            const SwappcCall* call = findCallAt(calls, index);
            next.push_back(call == nullptr || call->fetch ? index + 1u : call->targetIndex);
        } else if (instruction.op == RdnaOpcode::SSetpcB64) {
            std::uint32_t target = 0;
            if (const SwappcCall* call = findReturnAt(calls, index); call != nullptr) {
                next.push_back(call->callIndex + 1u);
            } else if (resolveSetpcTarget(program, index, target)) {
                next.push_back(instructionIndexOfProgramCounter(program, target));
            }
        } else {
            next.push_back(index + 1u);
        }
        std::erase_if(next, [&](std::uint32_t successor) { return successor >= size; });
        return next;
    };
    for (const auto link : links) {
        const auto writesLink = [&](std::uint32_t index) {
            const SwappcCall* call = findCallAt(calls, index);
            return (call != nullptr && call->linkRegister == link) || (std::ranges::find(unlinked, index) != unlinked.end() && scalarIndex(program.instructions[index].destination) == link);
        };
        std::vector<std::uint8_t> holds(size, 0u);
        std::vector<std::uint32_t> work;
        for (std::uint32_t index = 0; index < size; ++index) {
            if (writesLink(index)) work.push_back(index);
        }
        const auto after = [&](std::uint32_t index) -> std::uint8_t {
            if (writesLink(index)) return 3u;
            std::uint8_t value = holds[index];
            if (overwritesScalar(program.instructions[index], link, waveSize)) value &= 2u;
            if (overwritesScalar(program.instructions[index], link + 1u, waveSize)) value &= 1u;
            return value;
        };
        while (!work.empty()) {
            const auto index = work.back();
            work.pop_back();
            const auto value = after(index);
            for (const auto successor : successors(index)) {
                if ((holds[successor] | value) == holds[successor]) continue;
                holds[successor] |= value;
                work.push_back(successor);
            }
        }
        for (std::uint32_t index = 0; index < size; ++index) {
            const auto& instruction = program.instructions[index];
            if (holds[index] == 0u || writesLink(index) || instruction.family == RdnaInstructionFamily::SOPC || findReturnAt(calls, index) != nullptr) continue;
            const bool inside = std::ranges::any_of(calls, [&](const SwappcCall& call) { return !call.fetch && call.linkRegister == link && index >= call.targetIndex && index <= call.returnIndex; });
            if (inside || !(((holds[index] & 1u) != 0u && readsScalarDword(instruction, link, waveSize)) || ((holds[index] & 2u) != 0u && readsScalarDword(instruction, link + 1u, waveSize)))) continue;
            const auto owner = std::ranges::find_if(calls, [&](const SwappcCall& call) { return !call.fetch && call.linkRegister == link; });
            throw escapeError(*owner, instruction);
        }
    }
}

std::vector<SwappcCall> analyzeSwappcCalls(const RdnaProgram& program, const SwappcInfo* swappc, std::vector<std::uint32_t>& unlinked) {
    std::vector<SwappcCall> calls;
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.op != RdnaOpcode::SSwappcB64 && instruction.op != RdnaOpcode::SCallB64) {
            continue;
        }
        const std::uint32_t linkRegister = scalarIndex(instruction.destination);
        if (linkRegister == NoScalarRegister || linkRegister % 2u != 0u || linkRegister > 104u) {
            throw std::invalid_argument("unsupported scalar call link register at program counter " + toHexString(instruction.programCounter) + ": the link must be an ordinary aligned scalar register pair");
        }
        SwappcCall call;
        call.callIndex = index;
        call.linkRegister = linkRegister;
        std::uint32_t target = instruction.branchTarget;
        const bool staticTarget = instruction.op == RdnaOpcode::SCallB64 || resolveSetpcTarget(program, index, target);
        call.targetIndex = staticTarget ? instructionIndexOfProgramCounter(program, target) : InvalidControlFlowId;
        if (instruction.op == RdnaOpcode::SCallB64 && call.targetIndex == InvalidControlFlowId) {
            throw std::invalid_argument("s_call_b64 at program counter " + toHexString(instruction.programCounter) + " targets invalid instruction boundary " + toHexString(target));
        }
        if (instruction.op == RdnaOpcode::SSwappcB64 && isFetchCall(program, index, swappc, staticTarget && call.targetIndex == InvalidControlFlowId)) {
            call.fetch = true;
            call.returnTargetProgramCounter = instructionEndProgramCounter(instruction);
            calls.push_back(call);
            continue;
        }
        if (!staticTarget || call.targetIndex == InvalidControlFlowId) {
            if (swappc != nullptr && std::ranges::find(swappc->linkedCallFallbacks, instruction.programCounter) != swappc->linkedCallFallbacks.end()) {
                unlinked.push_back(index);
                continue;
            }
            if (std::ranges::any_of(calls, [&](const SwappcCall& other) { return other.linkRegister == call.linkRegister; })) {
                throw std::invalid_argument("unclosable scalar call/return pairing at program counter " + toHexString(instruction.programCounter) +
                    ": link register s[" + std::to_string(call.linkRegister) + "] is shared with another call");
            }
            throw std::invalid_argument("computed/data-dependent s_swappc_b64 call target at program counter " + toHexString(instruction.programCounter) + " is not statically resolvable");
        }
        call.targetProgramCounter = target;
        calls.push_back(call);
    }

    const auto sharedLinkError = [&](const SwappcCall& call) {
        return std::invalid_argument("unclosable scalar call/return pairing at program counter " + toHexString(program.instructions[call.callIndex].programCounter) +
            ": link register s[" + std::to_string(call.linkRegister) + "] is shared with another call");
    };
    const auto escapeError = [&](const SwappcCall& call, const RdnaInstruction& instruction) {
        return std::invalid_argument("scalar call link register s[" + std::to_string(call.linkRegister) + "] written at program counter " +
            toHexString(program.instructions[call.callIndex].programCounter) + " escapes the constant-offset call/return model (used at program counter " +
            toHexString(instruction.programCounter) + ")");
    };
    const auto sharesLink = [&](const SwappcCall& call) {
        return std::ranges::count_if(calls, [&](const SwappcCall& other) { return other.linkRegister == call.linkRegister; }) > 1;
    };
    for (std::size_t first = 0; first < calls.size(); ++first) {
        for (std::size_t second = first + 1u; second < calls.size(); ++second) {
            if (calls[first].linkRegister == calls[second].linkRegister && (calls[first].fetch || calls[second].fetch)) throw sharedLinkError(calls[second]);
        }
    }

    for (auto& call : calls) {
        if (sharesLink(call)) {
            call.returnIndex = InvalidControlFlowId;
            for (auto index = call.targetIndex; index < program.instructions.size(); ++index) {
                const auto& instruction = program.instructions[index];
                if (instruction.op == RdnaOpcode::SSetpcB64 && isScalar(instruction.source0, call.linkRegister)) {
                    call.returnIndex = index;
                    break;
                }
            }
            if (call.returnIndex == InvalidControlFlowId) {
                throw std::invalid_argument("unclosable scalar call/return pairing at program counter " + toHexString(program.instructions[call.callIndex].programCounter) +
                    ": no paired s_setpc_b64 return reads link register s[" + std::to_string(call.linkRegister) + "]");
            }
            call.returnTargetProgramCounter = instructionEndProgramCounter(program.instructions[call.callIndex]);
            continue;
        }
        std::uint32_t regionEnd = InvalidControlFlowId;
        if (!call.fetch) {
            std::uint32_t returns = 0;
            for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
                const auto& instruction = program.instructions[index];
                if (index != call.callIndex && instruction.op == RdnaOpcode::SSetpcB64 && isScalar(instruction.source0, call.linkRegister)) {
                    ++returns;
                    regionEnd = index;
                }
            }
            if (returns != 1u || regionEnd < call.targetIndex) regionEnd = InvalidControlFlowId;
        }
        std::uint32_t returnIndex = InvalidControlFlowId;
        for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
            if (index == call.callIndex) {
                continue;
            }
            const auto& instruction = program.instructions[index];
            if (instruction.op == RdnaOpcode::SSetpcB64 && isScalar(instruction.source0, call.linkRegister)) {
                if (returnIndex != InvalidControlFlowId) {
                    throw std::invalid_argument("unclosable scalar call/return pairing at program counter " + toHexString(program.instructions[call.callIndex].programCounter) +
                        ": multiple s_setpc_b64 returns read link register s[" + std::to_string(call.linkRegister) + "]");
                }
                returnIndex = index;
                continue;
            }
            const bool outsideRegion = regionEnd != InvalidControlFlowId && (index < call.targetIndex || index > regionEnd);
            if (outsideRegion) continue;
            if (touchesLinkRegister(instruction, call.linkRegister)) {
                throw escapeError(call, instruction);
            }
        }
        if (call.fetch) {
            if (returnIndex != InvalidControlFlowId) {
                throw std::invalid_argument("unclosable s_swappc_b64 fetch-shader call at program counter " + toHexString(program.instructions[call.callIndex].programCounter) +
                    ": its s_setpc_b64 return must live in the fetch-shader code");
            }
            continue;
        }
        if (returnIndex == InvalidControlFlowId) {
            throw std::invalid_argument("unclosable scalar call/return pairing at program counter " + toHexString(program.instructions[call.callIndex].programCounter) +
                ": no paired s_setpc_b64 return reads link register s[" + std::to_string(call.linkRegister) + "]");
        }
        if (returnIndex < call.targetIndex) {
            throw std::invalid_argument("unclosable scalar call/return pairing: return precedes the call target at program counter " +
                toHexString(program.instructions[call.callIndex].programCounter));
        }
        call.returnIndex = returnIndex;
        call.returnTargetProgramCounter = instructionEndProgramCounter(program.instructions[call.callIndex]);
    }

    for (const auto& call : calls) {
        if (!sharesLink(call)) continue;
        for (const auto& other : calls) {
            if (&other == &call || other.linkRegister != call.linkRegister) continue;
            const bool callInside = other.callIndex >= call.targetIndex && other.callIndex <= call.returnIndex;
            const bool regionsMeet = other.targetIndex <= call.returnIndex && call.targetIndex <= other.returnIndex;
            if (callInside || regionsMeet) throw sharedLinkError(other);
        }
        for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
            if (std::ranges::any_of(calls, [&](const SwappcCall& other) { return other.callIndex == index && other.linkRegister == call.linkRegister; })) continue;
            const auto& instruction = program.instructions[index];
            const auto region = std::ranges::find_if(calls, [&](const SwappcCall& other) { return other.linkRegister == call.linkRegister && index >= other.targetIndex && index <= other.returnIndex; });
            if (region != calls.end() && index != region->returnIndex && touchesLinkRegister(instruction, call.linkRegister)) throw escapeError(*region, instruction);
        }
    }

    requireReturnAddressesStayInCalls(program, calls, unlinked, swappc != nullptr ? swappc->waveSize : 0u, escapeError);

    for (std::size_t first = 0; first < calls.size(); ++first) {
        if (!calls[first].fetch && calls[first].callIndex >= calls[first].targetIndex && calls[first].callIndex <= calls[first].returnIndex) {
            throw std::invalid_argument("recursive scalar call at program counter " + toHexString(program.instructions[calls[first].callIndex].programCounter) +
                ": the call executes inside its own call region");
        }
        for (std::size_t second = first + 1u; second < calls.size(); ++second) {
            if (calls[first].fetch || calls[second].fetch) {
                continue;
            }
            const bool disjoint = calls[first].returnIndex < calls[second].targetIndex || calls[second].returnIndex < calls[first].targetIndex;
            const bool firstContainsSecond = calls[first].targetIndex <= calls[second].targetIndex && calls[second].returnIndex <= calls[first].returnIndex;
            const bool secondContainsFirst = calls[second].targetIndex <= calls[first].targetIndex && calls[first].returnIndex <= calls[second].returnIndex;
            if (!disjoint && !firstContainsSecond && !secondContainsFirst) {
                throw std::invalid_argument("recursive scalar calls at program counter " + toHexString(program.instructions[calls[first].callIndex].programCounter) +
                    " and program counter " + toHexString(program.instructions[calls[second].callIndex].programCounter) + ": call regions interleave");
            }
        }
    }

    const bool anyStaticCall = std::any_of(calls.begin(), calls.end(), [](const SwappcCall& call) { return !call.fetch; });
    if (anyStaticCall) {
        struct Transfer {
            std::uint32_t index;
            std::uint32_t targetIndex;
        };
        std::vector<Transfer> transfers;
        for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
            const auto& instruction = program.instructions[index];
            if (IsDirectBranchOpcode(instruction.op)) {
                transfers.push_back(Transfer{index, instructionIndexOfProgramCounter(program, instruction.branchTarget)});
                if (IsConditionalBranchOpcode(instruction.op)) transfers.push_back(Transfer{index, index + 1u});
                continue;
            }
            if (instruction.op == RdnaOpcode::SSetpcB64) {
                if (findReturnAt(calls, index) != nullptr) {
                    continue;
                }
                std::uint32_t target = 0;
                if (!resolveSetpcTarget(program, index, target)) {
                    throw std::invalid_argument("unclosable scalar call region in a program with calls: dynamic s_setpc_b64 at program counter " +
                        toHexString(instruction.programCounter) + " cannot be confined to a call region");
                }
                transfers.push_back(Transfer{index, instructionIndexOfProgramCounter(program, target)});
                continue;
            }
            if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) {
                const SwappcCall* call = findCallAt(calls, index);
                transfers.push_back(Transfer{index, call == nullptr || call->fetch ? index + 1u : call->targetIndex});
                continue;
            }
            if (instruction.op != RdnaOpcode::SEndpgm && instruction.op != RdnaOpcode::SCodeEnd) transfers.push_back(Transfer{index, index + 1u});
        }
        for (const auto& call : calls) {
            if (call.fetch) {
                continue;
            }
            for (const auto& transfer : transfers) {
                const bool sourceInside = transfer.index >= call.targetIndex && transfer.index <= call.returnIndex;
                const bool targetInside = transfer.targetIndex != InvalidControlFlowId && transfer.targetIndex >= call.targetIndex && transfer.targetIndex <= call.returnIndex;
                if (!sourceInside && targetInside && transfer.index != call.callIndex) {
                    throw std::invalid_argument("unclosable scalar call/return pairing at program counter " +
                        toHexString(program.instructions[call.callIndex].programCounter) + ": control flow enters the call region from program counter " +
                        toHexString(program.instructions[transfer.index].programCounter));
                }
                if (sourceInside && !targetInside && transfer.index != call.returnIndex) {
                    throw std::invalid_argument("unclosable scalar call/return pairing at program counter " +
                        toHexString(program.instructions[call.callIndex].programCounter) + ": control flow leaves the call region at program counter " +
                        toHexString(program.instructions[transfer.index].programCounter));
                }
            }
        }
    }
    return calls;
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

std::vector<BasicBlock> GraphBuilder::splitIntoBlocks(const RdnaProgram& program, const std::vector<SwappcCall>& calls) const {
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
            if (const SwappcCall* returnCall = findReturnAt(calls, index); returnCall != nullptr) {
                target = returnCall->returnTargetProgramCounter;
            } else if (!resolveSetpcTarget(program, index, target)) {
                BoundedJumpTable table;
                if (!resolveBoundedJumpTable(program, index, table)) throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(instruction.programCounter));
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
        } else if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) {
            const SwappcCall* call = findCallAt(calls, index);
            if (call != nullptr && !call->fetch) {
                labels.insert(call->targetProgramCounter);
                if (nextProgramCounter <= endProgramCounter) {
                    labels.insert(nextProgramCounter);
                }
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

void GraphBuilder::linkBlocks(std::vector<BasicBlock>& blocks, const RdnaProgram& program, const std::vector<SwappcCall>& calls) const {
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
            if (const SwappcCall* returnCall = findReturnAt(calls, block.instructionEnd - 1u); returnCall != nullptr) {
                block.terminator.kind = TerminatorKind::Branch;
                block.terminator.condition = BranchCondition::Always;
                block.terminator.trueBlock = programCounterToBlock.at(returnCall->returnTargetProgramCounter);
            } else if (!resolveSetpcTarget(program, block.instructionEnd - 1u, target)) {
                BoundedJumpTable table;
                if (!resolveBoundedJumpTable(program, block.instructionEnd - 1u, table)) throw std::invalid_argument("unsupported dynamic s_setpc_b64 at program counter " + toHexString(last.programCounter));
                block.terminator.kind = TerminatorKind::IndirectBranch;
                block.terminator.indirectPcSgpr = last.source0.reg;
                sortUnique(table.targets);
                for (const auto tableTarget : table.targets) {
                    block.terminator.indirectTargetProgramCounters.push_back(tableTarget);
                    block.terminator.indirectTargets.push_back(programCounterToBlock.at(tableTarget));
                    addUnique(block.successors, programCounterToBlock.at(tableTarget));
                }
                continue;
            } else {
                if (block.instructionEnd - block.instructionBegin < 4u && resolveLongSetpcTarget(program, block.instructionEnd - 1u, target)) throw std::invalid_argument("long branch at program counter " + toHexString(last.programCounter) + " does not start in its block");
                block.terminator.kind = TerminatorKind::Branch;
                block.terminator.condition = BranchCondition::Always;
                block.terminator.trueBlock = programCounterToBlock.at(target);
            }
        } else if (const SwappcCall* call = findCallAt(calls, block.instructionEnd - 1u); call != nullptr && !call->fetch) {
            std::uint32_t ignoredTarget = 0;
            if (block.instructionEnd - block.instructionBegin < 4u && resolveLongSetpcTarget(program, block.instructionEnd - 1u, ignoredTarget)) throw std::invalid_argument("long branch at program counter " + toHexString(last.programCounter) + " does not start in its block");
            block.terminator.kind = TerminatorKind::Branch;
            block.terminator.condition = BranchCondition::Always;
            block.terminator.trueBlock = programCounterToBlock.at(call->targetProgramCounter);
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

std::optional<std::uint32_t> UnresolvableSwappcTarget(const RdnaProgram& program, const SwappcInfo* swappc) {
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.op != RdnaOpcode::SSwappcB64) {
            continue;
        }
        std::uint32_t target = instruction.branchTarget;
        const bool staticTarget = resolveSetpcTarget(program, index, target);
        const bool inProgram = staticTarget && instructionIndexOfProgramCounter(program, target) != InvalidControlFlowId;
        if (!inProgram && !isFetchCall(program, index, swappc, staticTarget)) {
            return instruction.programCounter;
        }
    }
    return std::nullopt;
}

namespace {

std::set<std::uint32_t> branchLabels(const RdnaProgram& program) {
    std::set<std::uint32_t> labels;
    for (const auto& instruction : program.instructions) {
        if (IsDirectBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SCallB64) labels.insert(instruction.branchTarget);
    }
    return labels;
}

bool unchangedUntil(const RdnaProgram& program, std::uint32_t reg, std::uint32_t reader, std::uint32_t waveSize) {
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        if (!writesScalar(program.instructions[index], reg, waveSize)) continue;
        if (index <= reader) return false;
        for (std::uint32_t later = index; later < program.instructions.size(); ++later) {
            const auto& instruction = program.instructions[later];
            if ((IsDirectBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SCallB64) && instruction.branchTarget <= program.instructions[reader].programCounter) return false;
        }
    }
    return true;
}

bool isScalarBufferLoad(RdnaOpcode op) {
    switch (op) {
        case RdnaOpcode::SBufferLoadDword:
        case RdnaOpcode::SBufferLoadDwordx2:
        case RdnaOpcode::SBufferLoadDwordx4:
        case RdnaOpcode::SBufferLoadDwordx8:
        case RdnaOpcode::SBufferLoadDwordx16: return true;
        default: return false;
    }
}

bool loadedDword(const RdnaProgram& program, std::uint32_t reg, std::uint32_t before, std::uint32_t waveSize, std::uint32_t skipBranchesPast, std::uint32_t& load, std::uint32_t& dword) {
    for (std::uint32_t copies = 0; copies < 4u; ++copies) {
        std::uint32_t writer = 0;
        if (!findLastWriter(program, before, {reg}, writer, waveSize, skipBranchesPast)) return false;
        const auto& instruction = program.instructions[writer];
        const auto destination = scalarIndex(instruction.destination);
        const auto source = scalarIndex(instruction.source0);
        if (instruction.op == RdnaOpcode::SMovB32 && destination == reg && source != NoScalarRegister) {
            reg = source;
        } else if (instruction.op == RdnaOpcode::SMovB64 && destination != NoScalarRegister && reg - destination < 2u && source != NoScalarRegister) {
            reg = source + (reg - destination);
        } else if (isScalarBufferLoad(instruction.op) && destination != NoScalarRegister && reg >= destination && reg - destination < instruction.dataDwordCount) {
            load = writer;
            dword = reg - destination;
            return true;
        } else {
            return false;
        }
        before = writer;
    }
    return false;
}

std::uint32_t branchWord(std::uint32_t opcode, std::size_t from, std::size_t to, const char* what) {
    const auto offset = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from) - 1;
    if (offset < -0x8000 || offset > 0x7fff) throw std::invalid_argument(std::string(what) + " lands out of the 16-bit branch range");
    return opcode | (static_cast<std::uint32_t>(offset) & 0xffffu);
}

}


std::vector<UserDataCall> FindUserDataCalls(const RdnaProgram& program, const SwappcInfo& swappc) {
    const auto labels = branchLabels(program);
    const auto userData = [&](std::uint32_t reg) { return reg >= swappc.userDataBaseRegister && reg - swappc.userDataBaseRegister < swappc.userDataCount; };
    const auto unchanged = [&](std::uint32_t reg, std::uint32_t reader) { return unchangedUntil(program, reg, reader, swappc.waveSize); };
    std::vector<UserDataCall> calls;
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.op != RdnaOpcode::SSwappcB64) continue;
        std::uint32_t target = 0;
        if (resolveSetpcTarget(program, index, target)) continue;
        const auto link = scalarIndex(instruction.destination);
        const auto source = scalarIndex(instruction.source0);
        if (link == NoScalarRegister || source == NoScalarRegister || source % 2u != 0u) continue;
        std::uint32_t pair = NoScalarRegister;
        if (source != link && userData(source) && userData(source + 1u) && unchanged(source, index) && unchanged(source + 1u, index)) {
            pair = source;
        } else {
            std::uint32_t low = 0, high = 0;
            if (!findLastWriter(program, index, {source}, low) || !findLastWriter(program, index, {source + 1u}, high)) continue;
            const auto first = std::min(low, high);
            const bool sameBlock = std::none_of(labels.begin(), labels.end(), [&](std::uint32_t label) {
                return label > program.instructions[first].programCounter && label <= instruction.programCounter;
            });
            const auto& lowWriter = program.instructions[low];
            const auto& highWriter = program.instructions[high];
            std::uint32_t from = NoScalarRegister;
            if (low == high && lowWriter.op == RdnaOpcode::SMovB64 && isScalar(lowWriter.destination, source)) {
                from = scalarIndex(lowWriter.source0);
            } else if (lowWriter.op == RdnaOpcode::SMovB32 && highWriter.op == RdnaOpcode::SMovB32 && isScalar(lowWriter.destination, source) && isScalar(highWriter.destination, source + 1u)) {
                from = scalarIndex(lowWriter.source0);
                if (from != NoScalarRegister && !isScalar(highWriter.source0, from + 1u)) from = NoScalarRegister;
            }
            if (!sameBlock || from == NoScalarRegister || from % 2u != 0u || !userData(from) || !userData(from + 1u) || !unchanged(from, low) || !unchanged(from + 1u, high)) continue;
            pair = from;
        }
        calls.push_back({instruction.programCounter, link, pair});
    }
    return calls;
}

std::optional<UserDataTail> FindUserDataTail(const RdnaProgram& program, std::uint32_t userDataBase, std::uint32_t userDataCount) {
    if (program.instructions.size() < 2u || program.instructions.back().op != RdnaOpcode::SCodeEnd) return std::nullopt;
    const auto& tail = program.instructions[program.instructions.size() - 2u];
    if (tail.op != RdnaOpcode::SSetpcB64 || tail.source0.kind != RdnaOperandKind::ScalarRegister) return std::nullopt;
    const auto reg = tail.source0.reg;
    if (reg % 2u != 0u || reg < userDataBase || reg - userDataBase >= userDataCount) return std::nullopt;
    bool highUnchanged = true;
    std::optional<std::uint32_t> constantHigh;
    for (const auto& instruction : program.instructions) {
        if (instruction.programCounter >= tail.programCounter) break;
        if (IsDirectBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SCallB64 || instruction.op == RdnaOpcode::SSwappcB64 ||
            instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd ||
            writesScalar(instruction, reg)) return std::nullopt;
        if (writesScalar(instruction, reg + 1u)) {
            highUnchanged = false;
            std::uint32_t value;
            if (instruction.op == RdnaOpcode::SMovB32 && isScalar(instruction.destination, reg + 1u) && isImmediate(instruction.source0, value)) constantHigh = value;
            else constantHigh.reset();
        }
    }
    if (!constantHigh && (!highUnchanged || reg - userDataBase + 1u >= userDataCount)) return std::nullopt;
    return UserDataTail{tail.programCounter, reg, constantHigh};
}

std::vector<std::uint32_t> LinkUserDataTail(const RdnaProgram& program, const UserDataTail& tail, const RdnaProgram& continuation) {
    const auto recognized = FindUserDataTail(program, tail.userDataRegister, 2u);
    if (!recognized || recognized->programCounter != tail.programCounter || recognized->constantHigh != tail.constantHigh) throw std::invalid_argument("user-data tail is not a linear jump through a proven user-data address");
    for (const auto& instruction : continuation.instructions) {
        if (instruction.op == RdnaOpcode::SGetpcB64) throw std::invalid_argument("user-data tail continuation reads its program counter, which a linked copy does not keep");
    }
    const auto words = tail.programCounter / 4u;
    std::vector<std::uint32_t> linked(program.code.begin(), program.code.begin() + words);
    linked.push_back(0xbf820000u);
    linked.insert(linked.end(), continuation.code.begin(), continuation.code.end());
    return linked;
}

std::optional<std::uint32_t> UserDataCalleeLength(std::span<const std::uint32_t> callee, std::uint32_t linkRegister) {
    std::uint32_t furthest = 0;
    for (std::uint32_t word = 0; word < callee.size();) {
        const auto programCounter = word * 4u;
        RdnaInstruction instruction;
        try {
            instruction = DecodeRdnaInstruction(programCounter, callee, word);
        } catch (const std::out_of_range&) {
            if (callee.size() - word < 16u) return std::nullopt;
            throw;
        }
        word += instruction.wordCount;
        const auto at = " at callee offset " + toHexString(programCounter);
        if (instruction.op == RdnaOpcode::SGetpcB64) throw std::invalid_argument("user-data callee reads its program counter" + at + ", which a linked copy does not keep");
        if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) throw std::invalid_argument("user-data callee makes a call" + at);
        if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) throw std::invalid_argument("user-data callee ends the program" + at + " before it returns");
        if (IsDirectBranchOpcode(instruction.op)) {
            if (instruction.branchTarget >= 0x80000000u) throw std::invalid_argument("user-data callee branches before its start" + at);
            furthest = std::max(furthest, instruction.branchTarget / 4u);
        }
        if (instruction.op != RdnaOpcode::SSetpcB64) continue;
        if (!isScalar(instruction.source0, linkRegister)) throw std::invalid_argument("user-data callee jumps through another register pair than its link s[" + std::to_string(linkRegister) + "]" + at);
        if (furthest >= word) throw std::invalid_argument("user-data callee returns" + at + " before code it branches to");
        return word;
    }
    return std::nullopt;
}

std::vector<std::uint32_t> LinkUserDataCalls(const RdnaProgram& program, std::span<const UserDataCall> calls, std::span<const std::vector<std::uint32_t>> callees) {
    return LinkCalls(program, calls, callees, {}, {}).code;
}

std::vector<TableCall> FindTableCalls(const RdnaProgram& program, const SwappcInfo& swappc) {
    const auto labels = branchLabels(program);
    const auto userData = [&](std::uint32_t reg) { return reg >= swappc.userDataBaseRegister && reg - swappc.userDataBaseRegister < swappc.userDataCount; };
    const auto sameBlock = [&](std::uint32_t first, std::uint32_t last) {
        return std::none_of(labels.begin(), labels.end(), [&](std::uint32_t label) {
            return label > program.instructions[first].programCounter && label <= program.instructions[last].programCounter;
        });
    };
    std::vector<TableCall> calls;
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.op != RdnaOpcode::SSwappcB64) continue;
        std::uint32_t ignored = 0;
        if (resolveSetpcTarget(program, index, ignored)) continue;
        const auto link = scalarIndex(instruction.destination);
        const auto target = scalarIndex(instruction.source0);
        if (link == NoScalarRegister || link % 2u != 0u || link > 104u || target == NoScalarRegister || target % 2u != 0u || target > 104u) continue;
        std::uint32_t lowLoad = 0, lowDword = 0, highLoad = 0, highDword = 0;
        const auto reader = instruction.programCounter;
        if (!loadedDword(program, target, index, swappc.waveSize, reader, lowLoad, lowDword) || !loadedDword(program, target + 1u, index, swappc.waveSize, reader, highLoad, highDword)) continue;
        if (lowLoad != highLoad || highDword != lowDword + 1u || !sameBlock(lowLoad, index)) continue;
        const auto& load = program.instructions[lowLoad];
        const auto descriptor = scalarIndex(load.source0);
        if (load.source0.kind != RdnaOperandKind::ScalarRegister || descriptor % 4u != 0u || load.memoryOffset >= 0x100000u || (load.memoryOffset & 3u) != 0u) continue;
        std::uint32_t descriptorLoad = 0;
        if (!findLastWriter(program, lowLoad, {descriptor, descriptor + 1u, descriptor + 2u, descriptor + 3u}, descriptorLoad, swappc.waveSize, reader) || !sameBlock(descriptorLoad, index)) continue;
        const auto& table = program.instructions[descriptorLoad];
        const auto pointer = scalarIndex(table.source0);
        if (table.op != RdnaOpcode::SLoadDwordx4 || !isScalar(table.destination, descriptor) || pointer == NoScalarRegister || pointer % 2u != 0u || table.source1.kind != RdnaOperandKind::Null ||
            table.memoryOffset >= 0x100000u || (table.memoryOffset & 3u) != 0u) continue;
        std::optional<std::uint64_t> constantPointer;
        if (!(userData(pointer) && userData(pointer + 1u) && unchangedUntil(program, pointer, descriptorLoad, swappc.waveSize) && unchangedUntil(program, pointer + 1u, descriptorLoad, swappc.waveSize))) {
            std::uint32_t lowWriter = 0, highWriter = 0, low = 0, high = 0;
            if (!findLastWriter(program, descriptorLoad, {pointer}, lowWriter, swappc.waveSize, reader) || !findLastWriter(program, descriptorLoad, {pointer + 1u}, highWriter, swappc.waveSize, reader) ||
                !sameBlock(std::min(lowWriter, highWriter), descriptorLoad)) continue;
            const auto& lowMove = program.instructions[lowWriter];
            const auto& highMove = program.instructions[highWriter];
            if (lowMove.op != RdnaOpcode::SMovB32 || highMove.op != RdnaOpcode::SMovB32 || !isScalar(lowMove.destination, pointer) || !isScalar(highMove.destination, pointer + 1u) ||
                !isImmediate(lowMove.source0, low) || !isImmediate(highMove.source0, high)) continue;
            constantPointer = static_cast<std::uint64_t>(high) << 32u | low;
        }
        std::uint32_t stride = 0;
        if (load.source1.kind != RdnaOperandKind::Null) {
            const auto offset = scalarIndex(load.source1);
            std::uint32_t shift = 0, shiftIndex = 0;
            if (offset == NoScalarRegister || !findLastWriter(program, lowLoad, {offset}, shiftIndex, swappc.waveSize, reader) || !sameBlock(shiftIndex, index)) continue;
            const auto& shifted = program.instructions[shiftIndex];
            if (shifted.op != RdnaOpcode::SLshlB32 || !isScalar(shifted.destination, offset) || !isImmediate(shifted.source1, shift) || shift < 2u || shift > 24u) continue;
            stride = 1u << shift;
        }
        calls.push_back({instruction.programCounter, link, target, constantPointer ? 0u : pointer, table.memoryOffset, load.memoryOffset + lowDword * 4u, stride, constantPointer});
    }
    return calls;
}

LinkedCalls LinkCalls(const RdnaProgram& program, std::span<const UserDataCall> userDataCalls, std::span<const std::vector<std::uint32_t>> userDataCallees,
    std::span<const TableCall> tableCalls, std::span<const std::vector<TableCallee>> tableCallees) {
    if (userDataCalls.size() != userDataCallees.size()) throw std::logic_error("user-data calls and callees differ in number");
    if (tableCalls.size() != tableCallees.size()) throw std::logic_error("table calls and callees differ in number");
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        BoundedJumpTable table;
        if (program.instructions[index].op == RdnaOpcode::SSetpcB64 && resolveBoundedJumpTable(program, index, table)) {
            throw std::invalid_argument("a program that jumps through a table in its code (s_setpc_b64 at program counter " + toHexString(program.instructions[index].programCounter) + ") cannot have callees linked over the words after its last instruction");
        }
    }
    const auto end = instructionEndProgramCounter(program.instructions.back()) / 4u;
    std::vector<std::uint32_t> linked(program.code.begin(), program.code.begin() + end);
    std::vector<std::uint32_t> fallbacks;
    const auto callWord = [&](std::uint32_t programCounter, const char* kind) {
        const auto index = instructionIndexOfProgramCounter(program, programCounter);
        if (index == InvalidControlFlowId || program.instructions[index].op != RdnaOpcode::SSwappcB64 || program.instructions[index].wordCount != 1u) {
            throw std::logic_error(std::string(kind) + " call does not name an s_swappc_b64");
        }
        return programCounter / 4u;
    };
    for (std::size_t call = 0; call < userDataCalls.size(); ++call) {
        const auto word = callWord(userDataCalls[call].programCounter, "user-data");
        const auto offset = static_cast<std::int64_t>(linked.size()) - static_cast<std::int64_t>(word) - 1;
        if (offset > 0x7fff) throw std::invalid_argument("user-data callee of the call at program counter " + toHexString(userDataCalls[call].programCounter) + " lands out of s_call_b64 range");
        linked[word] = 0xbb000000u | (userDataCalls[call].linkRegister << 16u) | static_cast<std::uint32_t>(offset);
        linked.insert(linked.end(), userDataCallees[call].begin(), userDataCallees[call].end());
    }
    for (std::size_t call = 0; call < tableCalls.size(); ++call) {
        const auto& table = tableCalls[call];
        const auto& callees = tableCallees[call];
        const auto word = callWord(table.programCounter, "table");
        const auto at = "the table callees of the call at program counter " + toHexString(table.programCounter);
        const auto original = linked[word];
        const auto count = callees.size();
        const auto dispatch = linked.size();
        const auto zero = dispatch + 1u;
        const auto one = zero + count * 7u + 2u;
        const auto calls = one + count * 8u + 2u;
        const auto fault = calls + count * 2u;
        const auto branch = [&](std::uint32_t opcode, std::size_t target) { linked.push_back(branchWord(opcode, linked.size(), target, at.c_str())); };
        linked[word] = branchWord(0xbf820000u, word, dispatch, at.c_str());
        branch(0xbf850000u, one);
        for (const bool sccOne : {false, true}) {
            for (std::size_t callee = 0; callee < count; ++callee) {
                const auto next = linked.size() + (sccOne ? 8u : 7u);
                linked.insert(linked.end(), {0xbf07ff00u | table.targetRegister, static_cast<std::uint32_t>(callees[callee].target)});
                branch(0xbf850000u, next);
                linked.insert(linked.end(), {0xbf07ff00u | (table.targetRegister + 1u), static_cast<std::uint32_t>(callees[callee].target >> 32u)});
                branch(0xbf850000u, next);
                if (sccOne) linked.push_back(0xbf068080u);
                branch(0xbf820000u, calls + callee * 2u);
            }
            linked.push_back(sccOne ? 0xbf068080u : 0xbf078080u);
            branch(0xbf820000u, fault);
        }
        for (std::size_t callee = 0; callee < count; ++callee) {
            linked.push_back(0u);
            branch(0xbf820000u, word + 1u);
        }
        fallbacks.push_back(static_cast<std::uint32_t>(linked.size() * 4u));
        linked.push_back(original);
        branch(0xbf820000u, word + 1u);
        for (std::size_t callee = 0; callee < count; ++callee) {
            const auto site = calls + callee * 2u;
            const auto offset = static_cast<std::int64_t>(linked.size()) - static_cast<std::int64_t>(site) - 1;
            if (offset > 0x7fff) throw std::invalid_argument(at + " land out of s_call_b64 range");
            linked[site] = 0xbb000000u | (table.linkRegister << 16u) | static_cast<std::uint32_t>(offset);
            linked.insert(linked.end(), callees[callee].code.begin(), callees[callee].code.end());
        }
    }
    linked.push_back(0xbf9f0000u);
    return {std::move(linked), std::move(fallbacks)};
}

ControlFlowGraph GraphBuilder::Build(const RdnaProgram& program, const SwappcInfo* swappc) const {
    std::vector<std::uint32_t> unlinked;
    const std::vector<SwappcCall> calls = analyzeSwappcCalls(program, swappc, unlinked);
    ControlFlowGraph graph;
    for (const auto index : unlinked) graph.unlinkedCallProgramCounters.push_back(program.instructions[index].programCounter);
    graph.blocks = splitIntoBlocks(program, calls);
    linkBlocks(graph.blocks, program, calls);
    for (const auto& call : calls) {
        if (call.fetch) {
            graph.hasFetchCall = true;
            graph.fetchCallProgramCounter = program.instructions[call.callIndex].programCounter;
        }
    }
    const auto originalSize = graph.blocks.size();
    for (std::uint32_t id = 0; id < originalSize; ++id) {
        const auto term = graph.blocks[id].terminator;
        if (term.kind != TerminatorKind::IndirectBranch) continue;
        BoundedJumpTable table;
        if (!resolveBoundedJumpTable(program, graph.blocks[id].instructionEnd - 1u, table)) throw std::logic_error("jump table resolution changed");
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
