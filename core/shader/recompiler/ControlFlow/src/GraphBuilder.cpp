#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/ControlFlowHelpers.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
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

bool findLastWriter(const RdnaProgram& program, std::uint32_t end, std::initializer_list<std::uint32_t> indices, std::uint32_t& writer, std::uint32_t waveSize = 0u, std::uint32_t skipBranchesPast = 0u);

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
                load.family == RdnaInstructionFamily::VOP3 || load.family == RdnaInstructionFamily::VOPC || load.family == RdnaInstructionFamily::VINTRP) {
                if (load.destination.kind != RdnaOperandKind::ScalarRegister && load.destination2.kind != RdnaOperandKind::ScalarRegister) continue;
            }
            return false;
        }
        if (load.destination.kind != RdnaOperandKind::ScalarRegister || load.source0.kind != RdnaOperandKind::ScalarRegister) return false;
        auto loadedHigh = load.destination;
        ++loadedHigh.reg;
        if (!addsRegister(low, pcReg, load.destination) || !addsRegister(high, pcReg + 1u, loadedHigh)) return false;
        if (pcReg <= loadedHigh.reg && load.destination.reg <= pcReg + 1u) return false;
        const auto baseReg = load.source0.reg;
        std::uint32_t baseHighIndex = 0;
        if (!findLastWriter(program, loadIndex, {baseReg, baseReg + 1u}, baseHighIndex) || baseHighIndex < 2u) return false;
        const auto& basePc = program.instructions[baseHighIndex - 2u];
        const auto& baseLow = program.instructions[baseHighIndex - 1u];
        const auto& baseHigh = program.instructions[baseHighIndex];
        const auto offsetReg = scalarIndex(load.source1);
        std::uint32_t shiftIndex = 0, boundIndex = 0;
        if (offsetReg == NoScalarRegister ||
            !findLastWriter(program, loadIndex, {offsetReg}, shiftIndex) ||
            !findLastWriter(program, shiftIndex, {offsetReg}, boundIndex)) return false;
        const auto& shift = program.instructions[shiftIndex];
        const auto& bound = program.instructions[boundIndex];
        std::uint32_t displacement = 0, carry = 0, shiftAmount = 0, maximum = 0;
        if (basePc.op != RdnaOpcode::SGetpcB64 || !isRegister(basePc.destination, RdnaOperandKind::ScalarRegister, baseReg) ||
            baseLow.op != RdnaOpcode::SAddU32 || !addsImmediate(baseLow, baseReg, displacement) ||
            baseHigh.op != RdnaOpcode::SAddcU32 || !addsImmediate(baseHigh, baseReg + 1u, carry) || carry != 0u ||
            shift.op != RdnaOpcode::SLshlB32 || !sameRegister(shift.destination, load.source1) ||
            !sameRegister(shift.source0, load.source1) || !isImmediate(shift.source1, shiftAmount) || shiftAmount != 3u ||
            bound.op != RdnaOpcode::SMinU32 || !sameRegister(bound.destination, load.source1) ||
            !sameRegister(bound.source0, load.source1) || !isImmediate(bound.source1, maximum) || maximum > 255u) return false;
        if (load.source1.kind == RdnaOperandKind::ScalarRegister && load.source1.reg >= baseReg && load.source1.reg <= baseReg + 1u) return false;
        const auto firstProgramCounter = std::min(basePc.programCounter, bound.programCounter);
        for (const auto& instruction : program.instructions) {
            if (IsDirectBranchOpcode(instruction.op) && instruction.branchTarget > firstProgramCounter && instruction.branchTarget <= branch.programCounter) return false;
        }
        const std::uint64_t table = static_cast<std::uint64_t>(instructionEndProgramCounter(basePc)) + displacement + load.memoryOffset;
        const std::uint64_t byteCount = (static_cast<std::uint64_t>(maximum) + 1u) * 8u;
        if ((table & 3u) != 0u || table > program.code.size_bytes() || byteCount > program.code.size_bytes() - table) return false;
        result.load.programCounter = load.programCounter;
        result.firstProgramCounter = firstProgramCounter;
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

bool findLastWriter(const RdnaProgram& program, std::uint32_t end, std::initializer_list<std::uint32_t> indices, std::uint32_t& writer, std::uint32_t waveSize, std::uint32_t skipBranchesPast) {
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
    return count != 0u && slot != NoScalarRegister && slot <= linkRegister + 1u && linkRegister < slot + count;
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
            source2Count = instruction.imageSampler ? 4u : 0u;
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
        std::none_of(program.instructions.begin(), GcnGuestShaders() ? program.instructions.begin() + index : program.instructions.end(), [&](const RdnaInstruction& other) {
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
                default: return first != NoScalarRegister && reg >= first && reg - first < std::max(instruction.dataDwordCount, 1u);
            }
        case RdnaInstructionFamily::SOP2: return first != NoScalarRegister && reg >= first && reg - first < std::max(instruction.dataDwordCount, 1u);
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
            source2Count = instruction.imageSampler ? 4u : 0u;
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

struct LinkEscape {
    std::uint32_t waveSize = 0;
    std::string reason;
};

template <typename TError>
LinkEscape requireReturnAddressesStayInCalls(const RdnaProgram& program, const std::vector<SwappcCall>& calls, const std::vector<std::uint32_t>& unlinked, std::uint32_t knownWaveSize, const TError& escapeError) {
    const bool gcn = GcnGuestShaders();
    std::set<std::uint32_t> links;
    for (const auto& call : calls) {
        if (!call.fetch || gcn) links.insert(call.linkRegister);
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
    const auto escapeAt = [&](std::uint32_t waveSize) -> std::optional<std::invalid_argument> {
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
                const auto owner = std::ranges::find_if(calls, [&](const SwappcCall& call) { return (!call.fetch || gcn) && call.linkRegister == link; });
                return escapeError(*owner, instruction);
            }
        }
        return std::nullopt;
    };
    if (knownWaveSize != 0u) {
        if (const auto escape = escapeAt(knownWaveSize)) throw *escape;
        return {};
    }
    const auto narrow = escapeAt(32u);
    const auto wide = escapeAt(64u);
    if (narrow && wide) throw *narrow;
    if (narrow) return {32u, narrow->what()};
    if (wide) return {64u, wide->what()};
    return {};
}

std::vector<SwappcCall> analyzeSwappcCalls(const RdnaProgram& program, const SwappcInfo* swappc, std::vector<std::uint32_t>& unlinked, LinkEscape& linkEscape) {
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
            if (outsideRegion || (call.fetch && GcnGuestShaders())) continue;
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

    linkEscape = requireReturnAddressesStayInCalls(program, calls, unlinked, swappc != nullptr ? swappc->waveSize : 0u, escapeError);

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
        const std::uint32_t followingProgramCounter = index + 1u < program.instructions.size() ? program.instructions[index + 1u].programCounter : endProgramCounter;

        if (IsDirectBranchOpcode(instruction.op)) {
            if (!isValidTarget(instruction.branchTarget, instructionProgramCounters, firstProgramCounter, endProgramCounter)) {
                throw std::invalid_argument("branch at program counter " + toHexString(instruction.programCounter) + " targets invalid program counter " + toHexString(instruction.branchTarget));
            }
            labels.insert(instruction.branchTarget);
            if (instruction.op == RdnaOpcode::SBranch) {
                labels.insert(followingProgramCounter);
            } else if (nextProgramCounter <= endProgramCounter) {
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
        } else if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) {
            labels.insert(followingProgramCounter);
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

        if (last.op == RdnaOpcode::SEndpgm || last.op == RdnaOpcode::SCodeEnd) {
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
        const bool readerWritesAfterReading = index == reader && GcnGuestShaders();
        if (readerWritesAfterReading || !writesScalar(program.instructions[index], reg, waveSize)) continue;
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

enum class Definition { None, Writes, Unknown };

template <typename TDefines>
bool reachingDefinition(const RdnaProgram& program, std::uint32_t reader, const TDefines& defines, std::uint32_t& writer) {
    std::multimap<std::uint32_t, std::uint32_t> branches;
    std::set<std::uint32_t> callTargets;
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        std::uint32_t target = 0;
        if (IsDirectBranchOpcode(instruction.op)) branches.emplace(instruction.branchTarget, index);
        else if (instruction.op == RdnaOpcode::SSetpcB64 && resolveSetpcTarget(program, index, target)) branches.emplace(target, index);
        else if (instruction.op == RdnaOpcode::SCallB64) callTargets.insert(instruction.branchTarget);
    }
    std::vector<bool> seen(program.instructions.size());
    std::vector<std::uint32_t> pending;
    const auto predecessors = [&](std::uint32_t index) {
        if (index == 0u || callTargets.contains(program.instructions[index].programCounter)) return false;
        const auto before = pending.size();
        const auto op = program.instructions[index - 1u].op;
        if (op != RdnaOpcode::SBranch && op != RdnaOpcode::SEndpgm && op != RdnaOpcode::SCodeEnd && op != RdnaOpcode::SSetpcB64) pending.push_back(index - 1u);
        const auto [first, last] = branches.equal_range(program.instructions[index].programCounter);
        for (auto branch = first; branch != last; ++branch) pending.push_back(branch->second);
        return pending.size() != before;
    };
    if (!predecessors(reader)) return false;
    std::optional<std::uint32_t> found;
    while (!pending.empty()) {
        const auto index = pending.back();
        pending.pop_back();
        if (seen[index]) continue;
        seen[index] = true;
        const auto& instruction = program.instructions[index];
        std::uint32_t target = 0;
        if (instruction.op == RdnaOpcode::SCallB64 || (instruction.op == RdnaOpcode::SSwappcB64 && resolveSetpcTarget(program, index, target))) return false;
        const auto definition = defines(instruction);
        if (definition == Definition::Unknown) return false;
        if (definition == Definition::Writes) {
            if (found.has_value() && *found != index) return false;
            found = index;
            continue;
        }
        if (!predecessors(index)) return false;
    }
    if (!found.has_value()) return false;
    writer = *found;
    return true;
}

bool reachingWriter(const RdnaProgram& program, std::uint32_t reader, std::initializer_list<std::uint32_t> indices, std::uint32_t& writer, std::uint32_t waveSize) {
    return reachingDefinition(program, reader, [&](const RdnaInstruction& instruction) {
        return std::ranges::any_of(indices, [&](std::uint32_t reg) { return writesScalar(instruction, reg, waveSize); }) ? Definition::Writes : Definition::None;
    }, writer);
}

bool mayWriteVector(const RdnaInstruction& instruction, std::uint32_t reg) {
    const auto near = [&](const RdnaOperand& operand, std::uint32_t width) { return operand.kind == RdnaOperandKind::VectorRegister && operand.reg <= reg && reg - operand.reg < width; };
    switch (instruction.op) {
        case RdnaOpcode::VMovreldB32:
        case RdnaOpcode::VMovrelsdB32:
        case RdnaOpcode::VMovrelsd2B32:
        case RdnaOpcode::VSwaprelB32: return true;
        default: break;
    }
    switch (instruction.family) {
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF:
        case RdnaInstructionFamily::FLAT:
        case RdnaInstructionFamily::DS:
        case RdnaInstructionFamily::MIMG: {
            const auto width = std::max(instruction.dataDwordCount, 1u) + 1u;
            return near(instruction.destination, width) || near(instruction.destination2, width);
        }
        case RdnaInstructionFamily::VOP1:
        case RdnaInstructionFamily::VOP2:
        case RdnaInstructionFamily::VOP3:
        case RdnaInstructionFamily::VOP3P:
        case RdnaInstructionFamily::VOPC:
        case RdnaInstructionFamily::VINTRP: {
            const auto width = instruction.op == RdnaOpcode::VMqsadU32U8 ? 4u : readsVectorSources64(instruction.op) || instruction.op == RdnaOpcode::VQsadPkU16U8 || instruction.op == RdnaOpcode::VMqsadPkU16U8 ? 2u : 1u;
            return near(instruction.destination, width) || near(instruction.destination2, width) || (instruction.op == RdnaOpcode::VSwapB32 && near(instruction.source0, 1u));
        }
        case RdnaInstructionFamily::Unknown: return true;
        default: return false;
    }
}

bool laneOf(const RdnaOperand& operand, std::uint32_t& lane) {
    return operand.kind == RdnaOperandKind::IntegerInlineConstant && isImmediate(operand, lane) && lane < 32u;
}

bool restoreLaneSpill(const RdnaProgram& program, std::uint32_t reader, std::uint32_t first, std::uint32_t count, std::uint32_t waveSize, std::uint32_t& source, std::uint32_t& writer) {
    std::optional<std::uint32_t> vector, firstLane, firstSource, load;
    for (std::uint32_t offset = 0u; offset < count; ++offset) {
        std::uint32_t read = 0u, lane = 0u, spill = 0u, written = 0u;
        if (!reachingWriter(program, reader, {first + offset}, read, waveSize)) return false;
        const auto& restore = program.instructions[read];
        if (restore.op != RdnaOpcode::VReadlaneB32 || !isScalar(restore.destination, first + offset) || restore.source0.kind != RdnaOperandKind::VectorRegister || !laneOf(restore.source1, lane)) return false;
        if (vector.has_value() && (restore.source0.reg != *vector || lane != *firstLane + offset)) return false;
        vector = restore.source0.reg;
        if (!firstLane.has_value()) firstLane = lane;
        const auto spilled = [&](const RdnaInstruction& instruction) {
            std::uint32_t target = 0u;
            if (instruction.op == RdnaOpcode::VWritelaneB32 && instruction.destination.kind == RdnaOperandKind::VectorRegister && instruction.destination.reg == *vector) {
                if (!laneOf(instruction.source1, target)) return Definition::Unknown;
                return target == lane ? Definition::Writes : Definition::None;
            }
            return mayWriteVector(instruction, *vector) ? Definition::Unknown : Definition::None;
        };
        if (!reachingDefinition(program, read, spilled, spill)) return false;
        const auto saved = scalarIndex(program.instructions[spill].source0);
        if (saved == NoScalarRegister || (firstSource.has_value() && saved != *firstSource + offset)) return false;
        if (!firstSource.has_value()) firstSource = saved;
        if (!reachingWriter(program, spill, {saved}, written, waveSize) || (load.has_value() && written != *load)) return false;
        load = written;
    }
    if (!firstSource.has_value() || !load.has_value()) return false;
    source = *firstSource;
    writer = *load;
    return true;
}

std::uint32_t branchWord(std::uint32_t opcode, std::size_t from, std::size_t to, const char* what) {
    const auto offset = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from) - 1;
    if (offset < -0x8000 || offset > 0x7fff) throw std::invalid_argument(std::string(what) + " lands out of the 16-bit branch range");
    return opcode | (static_cast<std::uint32_t>(offset) & 0xffffu);
}

std::vector<std::uint32_t> expandProgramCounterReads(std::span<const std::uint32_t> code, std::uint64_t address) {
    constexpr auto Inside = std::numeric_limits<std::size_t>::max();
    std::vector<RdnaInstruction> instructions;
    std::vector<std::size_t> moved(code.size() + 1u, Inside);
    std::size_t size = 0;
    bool reads = false;
    for (std::uint32_t word = 0; word < code.size();) {
        auto instruction = DecodeRdnaInstruction(word * 4u, code, word);
        moved[word] = size;
        if (instruction.op == RdnaOpcode::SGetpcB64) {
            if (instruction.destination.kind != RdnaOperandKind::ScalarRegister || instruction.destination.reg % 2u != 0u || instruction.destination.reg + 1u >= 106u) throw std::invalid_argument("a table callee reads its program counter into another operand than an SGPR pair at callee offset " + toHexString(instruction.programCounter));
            reads = true;
        }
        size += instruction.op == RdnaOpcode::SGetpcB64 ? 4u : instruction.wordCount;
        word += instruction.wordCount;
        instructions.push_back(std::move(instruction));
    }
    if (!reads) return {code.begin(), code.end()};
    moved[code.size()] = size;
    std::vector<std::uint32_t> expanded;
    expanded.reserve(size);
    for (const auto& instruction : instructions) {
        const auto word = instruction.programCounter / 4u;
        if (instruction.op == RdnaOpcode::SGetpcB64) {
            const auto value = address + instruction.programCounter + 4u;
            const auto reg = instruction.destination.reg;
            expanded.insert(expanded.end(), {0xbe8003ffu | (reg << 16u), static_cast<std::uint32_t>(value), 0xbe8003ffu | ((reg + 1u) << 16u), static_cast<std::uint32_t>(value >> 32u)});
            continue;
        }
        if (IsDirectBranchOpcode(instruction.op)) {
            const auto target = instruction.branchTarget / 4u;
            if (instruction.branchTarget % 4u != 0u || target > code.size() || moved[target] == Inside) throw std::invalid_argument("a table callee that reads its program counter branches outside its instructions at callee offset " + toHexString(instruction.programCounter));
            expanded.push_back(branchWord(code[word] & 0xffff0000u, moved[word], moved[target], "a table callee's branch"));
            continue;
        }
        expanded.insert(expanded.end(), code.begin() + word, code.begin() + word + instruction.wordCount);
    }
    return expanded;
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
        if ((source != link || GcnGuestShaders()) && userData(source) && userData(source + 1u) && unchanged(source, index) && unchanged(source + 1u, index)) {
            pair = source;
        } else {
            std::uint32_t low = 0, high = 0;
            bool reaches = findLastWriter(program, index, {source}, low) && findLastWriter(program, index, {source + 1u}, high) &&
                std::none_of(labels.begin(), labels.end(), [&](std::uint32_t label) {
                    return label > program.instructions[std::min(low, high)].programCounter && label <= instruction.programCounter;
                });
            if (!reaches) reaches = reachingWriter(program, index, {source}, low, swappc.waveSize) && reachingWriter(program, index, {source + 1u}, high, swappc.waveSize);
            const auto& lowWriter = program.instructions[low];
            const auto& highWriter = program.instructions[high];
            std::uint32_t from = NoScalarRegister;
            if (low == high && lowWriter.op == RdnaOpcode::SMovB64 && isScalar(lowWriter.destination, source)) {
                from = scalarIndex(lowWriter.source0);
            } else if (lowWriter.op == RdnaOpcode::SMovB32 && highWriter.op == RdnaOpcode::SMovB32 && isScalar(lowWriter.destination, source) && isScalar(highWriter.destination, source + 1u)) {
                from = scalarIndex(lowWriter.source0);
                if (from != NoScalarRegister && !isScalar(highWriter.source0, from + 1u)) from = NoScalarRegister;
            }
            if (!reaches || from == NoScalarRegister || from % 2u != 0u || !userData(from) || !userData(from + 1u) || !unchanged(from, low) || !unchanged(from + 1u, high)) continue;
            pair = from;
        }
        calls.push_back({instruction.programCounter, link, pair});
    }
    return calls;
}

namespace {

std::optional<std::size_t> userDataTailIndex(const RdnaProgram& program) {
    std::size_t end = program.instructions.size();
    while (end > 0u && program.instructions[end - 1u].op == RdnaOpcode::SCodeEnd) --end;
    if (end == 0u) return std::nullopt;
    if (end < program.instructions.size() && program.instructions[end - 1u].op == RdnaOpcode::SSetpcB64) return end - 1u;
    if (program.instructions[end - 1u].op != RdnaOpcode::SEndpgm) return std::nullopt;
    for (std::size_t index = end - 1u; index-- > 0u;) {
        const auto op = program.instructions[index].op;
        if (op == RdnaOpcode::SSetpcB64) return index;
        if (IsDirectBranchOpcode(op) || op == RdnaOpcode::SCallB64 || op == RdnaOpcode::SSwappcB64 || op == RdnaOpcode::SGetpcB64 ||
            op == RdnaOpcode::SEndpgm || op == RdnaOpcode::SCodeEnd) return std::nullopt;
    }
    return std::nullopt;
}

std::uint32_t userDataTailEnd(const RdnaProgram& program) {
    std::size_t end = program.instructions.size();
    while (end > 0u && program.instructions[end - 1u].op == RdnaOpcode::SCodeEnd) --end;
    return instructionEndProgramCounter(program.instructions[end - 1u]);
}

}

std::optional<UserDataTail> FindUserDataTail(const RdnaProgram& program, std::uint32_t userDataBase, std::uint32_t userDataCount) {
    const auto tailIndex = userDataTailIndex(program);
    if (!tailIndex) return std::nullopt;
    const auto& tail = program.instructions[*tailIndex];
    if (tail.source0.kind != RdnaOperandKind::ScalarRegister) return std::nullopt;
    const auto trailingBegin = instructionEndProgramCounter(tail);
    const auto trailingEnd = userDataTailEnd(program);
    const auto reg = tail.source0.reg;
    const auto userData = [&](std::uint32_t candidate) { return candidate >= userDataBase && candidate - userDataBase < userDataCount; };
    if (reg % 2u != 0u) return std::nullopt;
    bool highUnchanged = true;
    std::optional<std::uint32_t> constantHigh;
    std::optional<std::uint32_t> copiedLow;
    bool trailingReached = false;
    for (const auto& instruction : program.instructions) {
        if (instruction.programCounter >= tail.programCounter) break;
        if (IsDirectBranchOpcode(instruction.op)) {
            if (trailingBegin == trailingEnd || instruction.branchTarget < trailingBegin || instruction.branchTarget >= trailingEnd) return std::nullopt;
            trailingReached = true;
        }
        if (instruction.op == RdnaOpcode::SCallB64 || instruction.op == RdnaOpcode::SSwappcB64 ||
            instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) return std::nullopt;
        if (writesScalar(instruction, reg)) {
            const auto source = scalarIndex(instruction.source0);
            if (copiedLow.has_value() || instruction.op != RdnaOpcode::SMovB32 || !isScalar(instruction.destination, reg) || source == NoScalarRegister || !userData(source) ||
                !unchangedUntil(program, source, static_cast<std::uint32_t>(&instruction - program.instructions.data()), 64u)) return std::nullopt;
            copiedLow = source;
        }
        if (writesScalar(instruction, reg + 1u)) {
            highUnchanged = false;
            std::uint32_t value;
            if (instruction.op == RdnaOpcode::SMovB32 && isScalar(instruction.destination, reg + 1u) && isImmediate(instruction.source0, value)) constantHigh = value;
            else constantHigh.reset();
        }
    }
    if (trailingBegin != trailingEnd && !trailingReached) return std::nullopt;
    if (copiedLow) {
        if (!constantHigh) return std::nullopt;
        return UserDataTail{tail.programCounter, *copiedLow, constantHigh};
    }
    if (!userData(reg)) return std::nullopt;
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
    const auto trailingBegin = words + 1u;
    const auto trailingEnd = userDataTailEnd(program) / 4u;
    const auto trailingWords = trailingEnd - trailingBegin;
    if (trailingWords > 0x7fffu) throw std::invalid_argument("user-data tail's trailing code is too long to branch over");
    std::vector<std::uint32_t> linked(program.code.begin(), program.code.begin() + words);
    linked.push_back(0xbf820000u | trailingWords);
    linked.insert(linked.end(), program.code.begin() + trailingBegin, program.code.begin() + trailingEnd);
    linked.insert(linked.end(), continuation.code.begin(), continuation.code.end());
    return linked;
}

std::optional<std::uint32_t> UserDataCalleeLength(std::span<const std::uint32_t> callee, std::uint32_t linkRegister, bool allowGetpc) {
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
        const auto at = [&] { return " at callee offset " + toHexString(programCounter); };
        if (instruction.op == RdnaOpcode::SGetpcB64 && !allowGetpc) throw std::invalid_argument("user-data callee reads its program counter" + at() + ", which a linked copy does not keep");
        if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) throw std::invalid_argument("user-data callee makes a call" + at());
        if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) throw std::invalid_argument("user-data callee ends the program" + at() + " before it returns");
        if (IsDirectBranchOpcode(instruction.op)) {
            if (instruction.branchTarget >= 0x80000000u) throw std::invalid_argument("user-data callee branches before its start" + at());
            furthest = std::max(furthest, instruction.branchTarget / 4u);
        }
        if (instruction.op != RdnaOpcode::SSetpcB64) continue;
        if (!isScalar(instruction.source0, linkRegister)) throw std::invalid_argument("user-data callee jumps through another register pair than its link s[" + std::to_string(linkRegister) + "]" + at());
        if (furthest >= word) throw std::invalid_argument("user-data callee returns" + at() + " before code it branches to");
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
        const bool lowOk = loadedDword(program, target, index, swappc.waveSize, reader, lowLoad, lowDword);
        const bool highOk = loadedDword(program, target + 1u, index, swappc.waveSize, reader, highLoad, highDword);
        if (!lowOk || !highOk || lowLoad != highLoad || highDword != lowDword + 1u) continue;
        const auto loadReaches = [&](std::uint32_t targetRegister) {
            auto at = index;
            auto reg = targetRegister;
            for (std::uint32_t copies = 0u; copies < 4u; ++copies) {
                std::uint32_t writer = 0;
                if (!reachingWriter(program, at, {reg}, writer, swappc.waveSize)) return false;
                if (writer == lowLoad) return true;
                const auto& copy = program.instructions[writer];
                const auto destination = scalarIndex(copy.destination);
                const auto source = scalarIndex(copy.source0);
                if (copy.op == RdnaOpcode::SMovB32 && source != NoScalarRegister) {
                    reg = source;
                } else if (copy.op == RdnaOpcode::SMovB64 && destination != NoScalarRegister && reg - destination < 2u && source != NoScalarRegister) {
                    reg = source + (reg - destination);
                } else {
                    return false;
                }
                at = writer;
            }
            return false;
        };
        if (!sameBlock(lowLoad, index) && !(loadReaches(target) && loadReaches(target + 1u))) continue;
        const auto& load = program.instructions[lowLoad];
        const auto descriptor = scalarIndex(load.source0);
        if (load.source0.kind != RdnaOperandKind::ScalarRegister || descriptor % 4u != 0u || load.memoryOffset >= 0x100000u || (load.memoryOffset & 3u) != 0u) continue;
        const bool descriptorInUserData = userData(descriptor) && userData(descriptor + 1u) && userData(descriptor + 2u) && userData(descriptor + 3u) &&
            unchangedUntil(program, descriptor, lowLoad, swappc.waveSize) && unchangedUntil(program, descriptor + 1u, lowLoad, swappc.waveSize) &&
            unchangedUntil(program, descriptor + 2u, lowLoad, swappc.waveSize) && unchangedUntil(program, descriptor + 3u, lowLoad, swappc.waveSize);
        if (descriptorInUserData) {
            std::uint32_t userDataStride = 0;
            if (load.source1.kind != RdnaOperandKind::Null) {
                const auto offset = scalarIndex(load.source1);
                std::uint32_t shift = 0, shiftIndex = 0;
                if (offset == NoScalarRegister || !findLastWriter(program, lowLoad, {offset}, shiftIndex, swappc.waveSize, reader) || !sameBlock(shiftIndex, index)) continue;
                const auto& shifted = program.instructions[shiftIndex];
                if (shifted.op != RdnaOpcode::SLshlB32 || !isScalar(shifted.destination, offset) || !isImmediate(shifted.source1, shift) || shift < 2u || shift > 24u) continue;
                userDataStride = 1u << shift;
            }
            calls.push_back({instruction.programCounter, link, target, descriptor, 0u, load.memoryOffset + lowDword * 4u, userDataStride, std::nullopt, 0u, 0u, true});
            continue;
        }
        std::uint32_t descriptorLoad = 0;
        const bool sameBlockWriter = findLastWriter(program, lowLoad, {descriptor, descriptor + 1u, descriptor + 2u, descriptor + 3u}, descriptorLoad, swappc.waveSize, reader) && sameBlock(descriptorLoad, index);
        if (!sameBlockWriter && !reachingWriter(program, lowLoad, {descriptor, descriptor + 1u, descriptor + 2u, descriptor + 3u}, descriptorLoad, swappc.waveSize)) continue;
        auto spilledDescriptor = descriptor;
        if (program.instructions[descriptorLoad].op == RdnaOpcode::VReadlaneB32 && !restoreLaneSpill(program, lowLoad, descriptor, 4u, swappc.waveSize, spilledDescriptor, descriptorLoad)) continue;
        std::optional<std::uint32_t> nestedDescriptorOffset;
        auto tableDescriptor = spilledDescriptor;
        if (const auto& inner = program.instructions[descriptorLoad]; isScalarBufferLoad(inner.op)) {
            const auto innerLoaded = scalarIndex(inner.destination);
            const auto outer = scalarIndex(inner.source0);
            if (inner.source0.kind != RdnaOperandKind::ScalarRegister || inner.source1.kind != RdnaOperandKind::Null || innerLoaded == NoScalarRegister || outer == NoScalarRegister || outer % 4u != 0u || spilledDescriptor < innerLoaded ||
                spilledDescriptor - innerLoaded + 4u > inner.dataDwordCount || inner.memoryOffset >= 0x100000u || (inner.memoryOffset & 3u) != 0u) continue;
            std::uint32_t outerLoad = 0;
            const auto innerIndex = descriptorLoad;
            const bool outerInBlock = findLastWriter(program, innerIndex, {outer, outer + 1u, outer + 2u, outer + 3u}, outerLoad, swappc.waveSize, reader) && sameBlock(outerLoad, innerIndex);
            if (!outerInBlock && !reachingWriter(program, innerIndex, {outer, outer + 1u, outer + 2u, outer + 3u}, outerLoad, swappc.waveSize)) continue;
            nestedDescriptorOffset = inner.memoryOffset + (spilledDescriptor - innerLoaded) * 4u;
            tableDescriptor = outer;
            descriptorLoad = outerLoad;
        }
        const auto& table = program.instructions[descriptorLoad];
        const auto pointer = scalarIndex(table.source0);
        const auto loaded = scalarIndex(table.destination);
        if ((table.op != RdnaOpcode::SLoadDwordx4 && table.op != RdnaOpcode::SLoadDwordx8 && table.op != RdnaOpcode::SLoadDwordx16) || loaded == NoScalarRegister || tableDescriptor < loaded ||
            (tableDescriptor - loaded) % 4u != 0u || tableDescriptor - loaded + 4u > table.dataDwordCount || pointer == NoScalarRegister || pointer % 2u != 0u || table.source1.kind != RdnaOperandKind::Null ||
            table.memoryOffset >= 0x100000u || (table.memoryOffset & 3u) != 0u) continue;
        std::optional<std::uint64_t> constantPointer;
        std::uint32_t constantPointerLowProgramCounter = 0;
        std::uint32_t constantPointerHighProgramCounter = 0;
        if (!(userData(pointer) && userData(pointer + 1u) && unchangedUntil(program, pointer, descriptorLoad, swappc.waveSize) && unchangedUntil(program, pointer + 1u, descriptorLoad, swappc.waveSize))) {
            std::uint32_t lowWriter = 0, highWriter = 0, low = 0, high = 0;
            if (!findLastWriter(program, descriptorLoad, {pointer}, lowWriter, swappc.waveSize, reader) || !findLastWriter(program, descriptorLoad, {pointer + 1u}, highWriter, swappc.waveSize, reader) ||
                !sameBlock(std::min(lowWriter, highWriter), descriptorLoad)) continue;
            const auto& lowMove = program.instructions[lowWriter];
            const auto& highMove = program.instructions[highWriter];
            if (lowMove.op != RdnaOpcode::SMovB32 || highMove.op != RdnaOpcode::SMovB32 || !isScalar(lowMove.destination, pointer) || !isScalar(highMove.destination, pointer + 1u) ||
                !isImmediate(lowMove.source0, low) || !isImmediate(highMove.source0, high)) continue;
            constantPointer = static_cast<std::uint64_t>(high) << 32u | low;
            constantPointerLowProgramCounter = lowMove.programCounter;
            constantPointerHighProgramCounter = highMove.programCounter;
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
        calls.push_back({instruction.programCounter, link, target, constantPointer ? 0u : pointer, table.memoryOffset + (tableDescriptor - loaded) * 4u, load.memoryOffset + lowDword * 4u, stride, constantPointer, constantPointerLowProgramCounter, constantPointerHighProgramCounter, false, nestedDescriptorOffset});
    }
    return calls;
}

std::optional<std::vector<std::uint32_t>> RetargetTableCallPointer(std::span<const std::uint32_t> code, const RdnaProgram& program, const TableCall& call, std::uint64_t address) {
    if (!call.constantPointer) return std::nullopt;
    std::vector<std::uint32_t> patched(code.begin(), code.end());
    const auto rewrite = [&](std::uint32_t programCounter, std::uint32_t value) {
        const auto* instruction = FindInstructionAtProgramCounter(program, programCounter);
        if (instruction == nullptr || instruction->op != RdnaOpcode::SMovB32) return false;
        const auto word = programCounter / 4u;
        if (instruction->source0.kind == RdnaOperandKind::LiteralConstant) {
            if (word + 1u >= patched.size()) return false;
            patched[word + 1u] = value;
            return true;
        }
        if (instruction->source0.kind == RdnaOperandKind::IntegerInlineConstant && value <= 64u) {
            patched[word] = (patched[word] & ~0xffu) | (128u + value);
            return true;
        }
        return false;
    };
    if (!rewrite(call.constantPointerLowProgramCounter, static_cast<std::uint32_t>(address))) return std::nullopt;
    if (!rewrite(call.constantPointerHighProgramCounter, static_cast<std::uint32_t>(address >> 32u))) return std::nullopt;
    return patched;
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
    std::vector<std::pair<std::size_t, std::size_t>> stations;
    for (std::size_t call = 0; call < tableCalls.size(); ++call) {
        const auto& table = tableCalls[call];
        const auto& callees = tableCallees[call];
        const auto word = callWord(table.programCounter, "table");
        const auto at = "the table callees of the call at program counter " + toHexString(table.programCounter);
        const auto original = linked[word];
        const auto count = callees.size();
        std::vector<std::vector<std::uint32_t>> codes;
        for (const auto& callee : callees) codes.push_back(expandProgramCounterReads(callee.code, callee.target));
        const auto relays = 2u * (tableCalls.size() - call - 1u);
        std::vector<std::size_t> path;
        for (const auto& [position, block] : stations) path.push_back(position + 2u * (call - block - 1u));
        const auto station = [&] {
            if (relays == 0u) return;
            stations.push_back({linked.size(), call});
            linked.insert(linked.end(), relays, 0u);
        };
        station();
        const auto start = linked.size();
        std::vector<std::size_t> segments(count + 1u);
        segments[0] = start + 1u;
        for (std::size_t callee = 0; callee < count; ++callee) segments[callee + 1u] = segments[callee] + 16u + codes[callee].size() + relays;
        const auto one = [&](std::size_t callee) { return segments[callee] + (callee < count ? 7u : 2u); };
        const auto back = [&](std::size_t callee) { return callee != 0u ? segments[callee - 1u] + 15u : path.empty() ? word + 1u : path.back() + 1u; };
        const auto fault = segments[count] + 3u;
        const auto branch = [&](std::uint32_t opcode, std::size_t target) { linked.push_back(branchWord(opcode, linked.size(), target, at.c_str())); };
        for (std::size_t hop = 0; hop < path.size(); ++hop) {
            linked[path[hop]] = branchWord(0xbf820000u, path[hop], hop + 1u < path.size() ? path[hop + 1u] : start, at.c_str());
            linked[path[hop] + 1u] = branchWord(0xbf820000u, path[hop] + 1u, hop == 0u ? word + 1u : path[hop - 1u] + 1u, at.c_str());
        }
        linked[word] = branchWord(0xbf820000u, word, path.empty() ? start : path.front(), at.c_str());
        branch(0xbf850000u, one(0u));
        for (std::size_t callee = 0; callee < count; ++callee) {
            for (const bool sccOne : {false, true}) {
                const auto next = sccOne ? one(callee + 1u) : segments[callee + 1u];
                linked.insert(linked.end(), {0xbf07ff00u | table.targetRegister, static_cast<std::uint32_t>(callees[callee].target)});
                branch(0xbf850000u, next);
                linked.insert(linked.end(), {0xbf07ff00u | (table.targetRegister + 1u), static_cast<std::uint32_t>(callees[callee].target >> 32u)});
                branch(0xbf850000u, next);
                if (sccOne) linked.push_back(0xbf068080u);
                else branch(0xbf820000u, segments[callee] + 14u);
            }
            linked.push_back(0xbb000001u | (table.linkRegister << 16u));
            branch(0xbf820000u, back(callee));
            linked.insert(linked.end(), codes[callee].begin(), codes[callee].end());
            station();
        }
        linked.push_back(0xbf078080u);
        branch(0xbf820000u, fault);
        linked.push_back(0xbf068080u);
        fallbacks.push_back(static_cast<std::uint32_t>(linked.size() * 4u));
        linked.push_back(original);
        branch(0xbf820000u, back(count));
    }
    linked.push_back(0xbf9f0000u);
    return {std::move(linked), std::move(fallbacks)};
}

namespace {

constexpr std::uint32_t SplitScalarRegisters = 108u;
constexpr std::uint32_t SplitM0 = 124u;
constexpr std::uint32_t SplitScc = 127u;
using SplitRegisters = std::bitset<128>;

bool operandIs(const RdnaInstruction& instruction, RdnaOperandKind kind) {
    return instruction.source0.kind == kind || instruction.source1.kind == kind || instruction.source2.kind == kind || instruction.source3.kind == kind;
}

bool splitReadsScc(const RdnaInstruction& instruction) {
    switch (instruction.op) {
        case RdnaOpcode::SCbranchScc0:
        case RdnaOpcode::SCbranchScc1:
        case RdnaOpcode::SCselectB32:
        case RdnaOpcode::SCselectB64:
        case RdnaOpcode::SCmovB32:
        case RdnaOpcode::SCmovB64:
        case RdnaOpcode::SCmovkI32:
        case RdnaOpcode::SAddcU32:
        case RdnaOpcode::SSubbU32: return true;
        default: return operandIs(instruction, RdnaOperandKind::Scc);
    }
}

bool splitReadsM0(const RdnaInstruction& instruction) {
    if (instruction.family == RdnaInstructionFamily::DS || instruction.family == RdnaInstructionFamily::VINTRP) return true;
    switch (instruction.op) {
        case RdnaOpcode::SMovrelsB32:
        case RdnaOpcode::SMovrelsB64:
        case RdnaOpcode::SMovreldB32:
        case RdnaOpcode::SMovreldB64:
        case RdnaOpcode::SMovrelsd2B32:
        case RdnaOpcode::VMovreldB32:
        case RdnaOpcode::VMovrelsB32:
        case RdnaOpcode::VMovrelsdB32:
        case RdnaOpcode::VMovrelsd2B32:
        case RdnaOpcode::SSendmsg:
        case RdnaOpcode::SSendmsghalt: return true;
        default: return operandIs(instruction, RdnaOperandKind::M0);
    }
}

SplitRegisters splitReads(const RdnaInstruction& instruction, std::uint32_t waveSize) {
    SplitRegisters reads;
    for (std::uint32_t reg = 0; reg < SplitScalarRegisters; ++reg) reads[reg] = readsScalarDword(instruction, reg, waveSize);
    reads[SplitM0] = splitReadsM0(instruction);
    reads[SplitScc] = splitReadsScc(instruction);
    return reads;
}

bool splitOverwritesScc(const RdnaInstruction& instruction) {
    if (instruction.family == RdnaInstructionFamily::SOPC) return true;
    switch (instruction.op) {
        case RdnaOpcode::SAddU32:
        case RdnaOpcode::SSubU32:
        case RdnaOpcode::SAddI32:
        case RdnaOpcode::SSubI32:
        case RdnaOpcode::SAddcU32:
        case RdnaOpcode::SSubbU32:
        case RdnaOpcode::SMinI32:
        case RdnaOpcode::SMinU32:
        case RdnaOpcode::SMaxI32:
        case RdnaOpcode::SMaxU32:
        case RdnaOpcode::SAndB32:
        case RdnaOpcode::SAndB64:
        case RdnaOpcode::SOrB32:
        case RdnaOpcode::SOrB64:
        case RdnaOpcode::SXorB32:
        case RdnaOpcode::SXorB64:
        case RdnaOpcode::SAndn2B32:
        case RdnaOpcode::SAndn2B64:
        case RdnaOpcode::SOrn2B32:
        case RdnaOpcode::SOrn2B64:
        case RdnaOpcode::SNandB32:
        case RdnaOpcode::SNandB64:
        case RdnaOpcode::SNorB32:
        case RdnaOpcode::SNorB64:
        case RdnaOpcode::SXnorB32:
        case RdnaOpcode::SXnorB64:
        case RdnaOpcode::SLshlB32:
        case RdnaOpcode::SLshlB64:
        case RdnaOpcode::SLshrB32:
        case RdnaOpcode::SLshrB64:
        case RdnaOpcode::SAshrI32:
        case RdnaOpcode::SAshrI64:
        case RdnaOpcode::SBfeU32:
        case RdnaOpcode::SBfeI32:
        case RdnaOpcode::SBfeU64:
        case RdnaOpcode::SBfeI64:
        case RdnaOpcode::SAbsdiffI32:
        case RdnaOpcode::SNotB32:
        case RdnaOpcode::SNotB64:
        case RdnaOpcode::SAbsI32:
        case RdnaOpcode::SAndSaveexecB32:
        case RdnaOpcode::SOrSaveexecB32:
        case RdnaOpcode::SAndSaveexecB64:
        case RdnaOpcode::SOrSaveexecB64:
        case RdnaOpcode::SXorSaveexecB64:
        case RdnaOpcode::SAndn2SaveexecB64: return true;
        default: return false;
    }
}

SplitRegisters splitKills(const RdnaInstruction& instruction, std::uint32_t waveSize) {
    SplitRegisters kills;
    for (std::uint32_t reg = 0; reg < SplitScalarRegisters; ++reg) kills[reg] = overwritesScalar(instruction, reg, waveSize);
    kills[SplitM0] = instruction.destination.kind == RdnaOperandKind::M0 && (instruction.op == RdnaOpcode::SMovB32 || instruction.op == RdnaOpcode::SMovkI32);
    kills[SplitScc] = splitOverwritesScc(instruction);
    return kills;
}

SplitRegisters splitWrites(const RdnaInstruction& instruction) {
    SplitRegisters writes;
    for (std::uint32_t reg = 0; reg < SplitScalarRegisters; ++reg) writes[reg] = writesScalar(instruction, reg);
    writes[SplitM0] = instruction.destination.kind == RdnaOperandKind::M0 || instruction.destination2.kind == RdnaOperandKind::M0 || instruction.op == RdnaOpcode::SMovreldB32 || instruction.op == RdnaOpcode::SMovreldB64 || instruction.op == RdnaOpcode::SMovrelsd2B32;
    writes[SplitScc] = true;
    return writes;
}

using SplitVectors = std::bitset<256>;

void splitVectorWrites(const RdnaInstruction& instruction, SplitVectors& writes) {
    if (instruction.op == RdnaOpcode::VMovreldB32 || instruction.op == RdnaOpcode::VMovrelsdB32 || instruction.op == RdnaOpcode::VMovrelsd2B32 || instruction.op == RdnaOpcode::VMovrelsB32) {
        writes.set();
        return;
    }
    const bool memory = instruction.family == RdnaInstructionFamily::MUBUF || instruction.family == RdnaInstructionFamily::MTBUF || instruction.family == RdnaInstructionFamily::FLAT || instruction.family == RdnaInstructionFamily::DS || instruction.family == RdnaInstructionFamily::MIMG;
    const auto mark = [&](const RdnaOperand& operand, std::uint32_t count) {
        if (operand.kind != RdnaOperandKind::VectorRegister) return;
        for (auto reg = operand.reg; reg < operand.reg + count && reg < writes.size(); ++reg) writes.set(reg);
    };
    mark(instruction.destination, memory ? std::max(instruction.dataDwordCount, 2u) + 1u : 2u);
    mark(instruction.destination2, 2u);
}

bool repeatsVisibly(const RdnaInstruction& instruction) {
    if (instruction.family == RdnaInstructionFamily::EXP || instruction.gds) return true;
    switch (instruction.op) {
        case RdnaOpcode::BufferAtomicAdd:
        case RdnaOpcode::BufferAtomicAddX2:
        case RdnaOpcode::BufferAtomicAnd:
        case RdnaOpcode::BufferAtomicAndX2:
        case RdnaOpcode::BufferAtomicCmpswap:
        case RdnaOpcode::BufferAtomicCmpswapX2:
        case RdnaOpcode::BufferAtomicCsub:
        case RdnaOpcode::BufferAtomicDec:
        case RdnaOpcode::BufferAtomicDecX2:
        case RdnaOpcode::BufferAtomicFcmpswap:
        case RdnaOpcode::BufferAtomicFcmpswapX2:
        case RdnaOpcode::BufferAtomicFmax:
        case RdnaOpcode::BufferAtomicFmaxX2:
        case RdnaOpcode::BufferAtomicFmin:
        case RdnaOpcode::BufferAtomicFminX2:
        case RdnaOpcode::BufferAtomicInc:
        case RdnaOpcode::BufferAtomicIncX2:
        case RdnaOpcode::BufferAtomicOr:
        case RdnaOpcode::BufferAtomicOrX2:
        case RdnaOpcode::BufferAtomicSmax:
        case RdnaOpcode::BufferAtomicSmaxX2:
        case RdnaOpcode::BufferAtomicSmin:
        case RdnaOpcode::BufferAtomicSminX2:
        case RdnaOpcode::BufferAtomicSub:
        case RdnaOpcode::BufferAtomicSubX2:
        case RdnaOpcode::BufferAtomicSwap:
        case RdnaOpcode::BufferAtomicSwapX2:
        case RdnaOpcode::BufferAtomicUmax:
        case RdnaOpcode::BufferAtomicUmaxX2:
        case RdnaOpcode::BufferAtomicUmin:
        case RdnaOpcode::BufferAtomicUminX2:
        case RdnaOpcode::BufferAtomicXor:
        case RdnaOpcode::BufferAtomicXorX2:
        case RdnaOpcode::BufferStoreByte:
        case RdnaOpcode::BufferStoreByteD16Hi:
        case RdnaOpcode::BufferStoreDword:
        case RdnaOpcode::BufferStoreDwordx2:
        case RdnaOpcode::BufferStoreDwordx3:
        case RdnaOpcode::BufferStoreDwordx4:
        case RdnaOpcode::BufferStoreFormatD16HiX:
        case RdnaOpcode::BufferStoreFormatD16X:
        case RdnaOpcode::BufferStoreFormatD16Xy:
        case RdnaOpcode::BufferStoreFormatD16Xyz:
        case RdnaOpcode::BufferStoreFormatD16Xyzw:
        case RdnaOpcode::BufferStoreFormatX:
        case RdnaOpcode::BufferStoreFormatXy:
        case RdnaOpcode::BufferStoreFormatXyz:
        case RdnaOpcode::BufferStoreFormatXyzw:
        case RdnaOpcode::BufferStoreShort:
        case RdnaOpcode::BufferStoreShortD16Hi:
        case RdnaOpcode::DsGwsBarrier:
        case RdnaOpcode::DsGwsInit:
        case RdnaOpcode::DsGwsSemaBr:
        case RdnaOpcode::DsGwsSemaP:
        case RdnaOpcode::DsGwsSemaReleaseAll:
        case RdnaOpcode::DsGwsSemaV:
        case RdnaOpcode::DsOrderedCount:
        case RdnaOpcode::FlatAtomicAdd:
        case RdnaOpcode::FlatAtomicAddX2:
        case RdnaOpcode::FlatAtomicAnd:
        case RdnaOpcode::FlatAtomicAndX2:
        case RdnaOpcode::FlatAtomicCmpswap:
        case RdnaOpcode::FlatAtomicCmpswapX2:
        case RdnaOpcode::FlatAtomicDec:
        case RdnaOpcode::FlatAtomicDecX2:
        case RdnaOpcode::FlatAtomicFcmpswap:
        case RdnaOpcode::FlatAtomicFcmpswapX2:
        case RdnaOpcode::FlatAtomicFmax:
        case RdnaOpcode::FlatAtomicFmaxX2:
        case RdnaOpcode::FlatAtomicFmin:
        case RdnaOpcode::FlatAtomicFminX2:
        case RdnaOpcode::FlatAtomicInc:
        case RdnaOpcode::FlatAtomicIncX2:
        case RdnaOpcode::FlatAtomicOr:
        case RdnaOpcode::FlatAtomicOrX2:
        case RdnaOpcode::FlatAtomicSmax:
        case RdnaOpcode::FlatAtomicSmaxX2:
        case RdnaOpcode::FlatAtomicSmin:
        case RdnaOpcode::FlatAtomicSminX2:
        case RdnaOpcode::FlatAtomicSub:
        case RdnaOpcode::FlatAtomicSubX2:
        case RdnaOpcode::FlatAtomicSwap:
        case RdnaOpcode::FlatAtomicSwapX2:
        case RdnaOpcode::FlatAtomicUmax:
        case RdnaOpcode::FlatAtomicUmaxX2:
        case RdnaOpcode::FlatAtomicUmin:
        case RdnaOpcode::FlatAtomicUminX2:
        case RdnaOpcode::FlatAtomicXor:
        case RdnaOpcode::FlatAtomicXorX2:
        case RdnaOpcode::FlatStoreByte:
        case RdnaOpcode::FlatStoreByteD16Hi:
        case RdnaOpcode::FlatStoreDword:
        case RdnaOpcode::FlatStoreDwordx2:
        case RdnaOpcode::FlatStoreDwordx3:
        case RdnaOpcode::FlatStoreDwordx4:
        case RdnaOpcode::FlatStoreShort:
        case RdnaOpcode::FlatStoreShortD16Hi:
        case RdnaOpcode::GlobalAtomicCsub:
        case RdnaOpcode::GlobalStoreDwordAddtid:
        case RdnaOpcode::ImageAtomicAdd:
        case RdnaOpcode::ImageAtomicAnd:
        case RdnaOpcode::ImageAtomicCmpswap:
        case RdnaOpcode::ImageAtomicDec:
        case RdnaOpcode::ImageAtomicFcmpswap:
        case RdnaOpcode::ImageAtomicFmax:
        case RdnaOpcode::ImageAtomicFmin:
        case RdnaOpcode::ImageAtomicInc:
        case RdnaOpcode::ImageAtomicOr:
        case RdnaOpcode::ImageAtomicSmax:
        case RdnaOpcode::ImageAtomicSmin:
        case RdnaOpcode::ImageAtomicSub:
        case RdnaOpcode::ImageAtomicSwap:
        case RdnaOpcode::ImageAtomicUmax:
        case RdnaOpcode::ImageAtomicUmin:
        case RdnaOpcode::ImageAtomicXor:
        case RdnaOpcode::ImageStore:
        case RdnaOpcode::ImageStoreBy2:
        case RdnaOpcode::ImageStoreBy4:
        case RdnaOpcode::ImageStoreMip:
        case RdnaOpcode::ImageStoreMipBy2:
        case RdnaOpcode::ImageStoreMipBy4:
        case RdnaOpcode::ImageStoreMipPck:
        case RdnaOpcode::ImageStoreMipPck2:
        case RdnaOpcode::ImageStoreMipPck4:
        case RdnaOpcode::ImageStorePck:
        case RdnaOpcode::ImageStorePck2:
        case RdnaOpcode::ImageStorePck4:
        case RdnaOpcode::SGetregB32:
        case RdnaOpcode::SMemrealtime:
        case RdnaOpcode::SMemtime:
        case RdnaOpcode::SSendmsg:
        case RdnaOpcode::SSendmsghalt:
        case RdnaOpcode::TbufferStoreFormatD16X:
        case RdnaOpcode::TbufferStoreFormatD16Xy:
        case RdnaOpcode::TbufferStoreFormatD16Xyz:
        case RdnaOpcode::TbufferStoreFormatD16Xyzw:
        case RdnaOpcode::TbufferStoreFormatX:
        case RdnaOpcode::TbufferStoreFormatXy:
        case RdnaOpcode::TbufferStoreFormatXyz:
        case RdnaOpcode::TbufferStoreFormatXyzw:
            return true;
        default: return false;
    }
}

std::optional<std::string> splitCalleeRefusal(const TableCallee& callee, const TableCall& call, SplitRegisters& written, SplitVectors& vectors) {
    const auto at = "callee " + toHexString(callee.target);
    if (callee.code.empty()) return at + " has no code";
    std::vector<RdnaInstruction> instructions;
    for (std::uint32_t word = 0; word < callee.code.size();) {
        auto instruction = DecodeRdnaInstruction(word * 4u, callee.code, word);
        word += std::max(instruction.wordCount, 1u);
        instructions.push_back(std::move(instruction));
    }
    for (std::size_t index = 0; index < instructions.size(); ++index) {
        const auto& instruction = instructions[index];
        const bool last = index + 1u == instructions.size();
        if (instruction.family == RdnaInstructionFamily::DS) return at + " accesses LDS at program counter " + toHexString(instruction.programCounter);
        if (instruction.op == RdnaOpcode::SBarrier) return at + " holds an s_barrier";
        if (instruction.op == RdnaOpcode::SEndpgm) return at + " ends the program";
        if (instruction.op == RdnaOpcode::SSwappcB64) return at + " calls another function";
        if (instruction.op == RdnaOpcode::SSetpcB64 && !last) return at + " returns before its last instruction";
        if (last && (instruction.op != RdnaOpcode::SSetpcB64 || instruction.source0.kind != RdnaOperandKind::ScalarRegister || instruction.source0.reg != call.linkRegister)) return at + " does not end with s_setpc_b64 to its link register";
        written |= splitWrites(instruction);
        splitVectorWrites(instruction, vectors);
    }
    return std::nullopt;
}

std::string splitRegisterName(std::uint32_t reg) {
    switch (reg) {
        case 106u: return "vcc_lo";
        case 107u: return "vcc_hi";
        case SplitM0: return "m0";
        case SplitScc: return "scc";
        default: return "s" + std::to_string(reg);
    }
}

std::optional<std::uint32_t> calleeRegisterReadAfterCall(const RdnaProgram& program, std::uint32_t callIndex, const SplitRegisters& written, std::uint32_t waveSize) {
    const auto size = static_cast<std::uint32_t>(program.instructions.size());
    std::vector<SplitRegisters> reads(size), kills(size), live(size);
    std::vector<std::array<std::uint32_t, 2>> next(size, {size, size});
    for (std::uint32_t index = 0; index < size; ++index) {
        const auto& instruction = program.instructions[index];
        reads[index] = splitReads(instruction, waveSize);
        kills[index] = splitKills(instruction, waveSize);
        if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) continue;
        if (IsDirectBranchOpcode(instruction.op)) {
            const auto target = instructionIndexOfProgramCounter(program, instruction.branchTarget);
            next[index][0] = target < size ? target : size + 1u;
            if (IsConditionalBranchOpcode(instruction.op)) next[index][1] = index + 1u;
        } else {
            next[index][0] = index + 1u;
        }
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (auto index = size; index-- > 0u;) {
            SplitRegisters out;
            for (const auto successor : next[index]) {
                if (successor < size) out |= live[successor];
                if (successor == size + 1u) out.set();
            }
            const auto in = reads[index] | (out & ~kills[index]);
            if (in != live[index]) {
                live[index] = in;
                changed = true;
            }
        }
    }
    if (callIndex + 1u >= size) return std::nullopt;
    const auto clobbered = live[callIndex + 1u] & written;
    for (std::uint32_t reg = 0; reg < clobbered.size(); ++reg) {
        if (clobbered[reg]) return reg;
    }
    return std::nullopt;
}

}

std::optional<TableCallSplit> SplitTableCall(const RdnaProgram& program, const TableCall& call, std::span<const TableCallee> callees, std::uint32_t waveSize, std::string& refusal) {
    const auto refuse = [&](std::string reason) -> std::optional<TableCallSplit> {
        refusal = std::move(reason);
        return std::nullopt;
    };
    const auto callIndex = instructionIndexOfProgramCounter(program, call.programCounter);
    if (callIndex == InvalidControlFlowId || program.instructions[callIndex].op != RdnaOpcode::SSwappcB64) return refuse("the table call does not name an s_swappc_b64");
    std::optional<std::uint32_t> backIndex;
    for (auto index = callIndex + 1u; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.op == RdnaOpcode::SCbranchExecnz && instruction.branchTarget <= call.programCounter) {
            backIndex = index;
            break;
        }
        if (IsDirectBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SSwappcB64) break;
    }
    if (!backIndex) return refuse("the table call is not followed by the s_cbranch_execnz of a waterfall loop");
    const auto& back = program.instructions[*backIndex];
    const auto loopIndex = instructionIndexOfProgramCounter(program, back.branchTarget);
    if (loopIndex == InvalidControlFlowId) return refuse("the waterfall loop branches back into the middle of an instruction");
    std::optional<std::uint32_t> pickIndex;
    for (auto index = loopIndex; index <= *backIndex; ++index) {
        const auto& instruction = program.instructions[index];
        if (index != callIndex && (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SEndpgm)) return refuse("the waterfall loop holds another call, return or end at program counter " + toHexString(instruction.programCounter));
        if (index != *backIndex && IsDirectBranchOpcode(instruction.op)) return refuse("the waterfall loop branches at program counter " + toHexString(instruction.programCounter));
        if (instruction.op == RdnaOpcode::SBarrier) return refuse("the waterfall loop holds an s_barrier");
        switch (instruction.family) {
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF:
        case RdnaInstructionFamily::FLAT:
        case RdnaInstructionFamily::DS:
        case RdnaInstructionFamily::MIMG:
        case RdnaInstructionFamily::EXP:
            return refuse("the waterfall loop accesses vector or LDS memory at program counter " + toHexString(instruction.programCounter));
        default:
            break;
        }
        if (instruction.op == RdnaOpcode::VReadfirstlaneB32 && index < callIndex && !pickIndex) pickIndex = index;
    }
    if (!pickIndex) return refuse("the waterfall loop does not pick a lane's value with v_readfirstlane_b32 before the call");
    const auto& pick = program.instructions[*pickIndex];
    const bool scalarPick = pick.destination.kind == RdnaOperandKind::ScalarRegister || pick.destination.kind == RdnaOperandKind::VccLo || pick.destination.kind == RdnaOperandKind::VccHi || pick.destination.kind == RdnaOperandKind::M0;
    if (!scalarPick || pick.source0.kind != RdnaOperandKind::VectorRegister) return refuse("the waterfall loop's v_readfirstlane_b32 does not read a VGPR into a scalar register");
    const auto narrows = [&](const RdnaInstruction& instruction) {
        if (instruction.family != RdnaInstructionFamily::VOPC && instruction.family != RdnaInstructionFamily::VOP3) return false;
        const auto reads = [&](const RdnaOperand& scalar, const RdnaOperand& vector) {
            return scalar.kind == pick.destination.kind && (scalar.kind != RdnaOperandKind::ScalarRegister || scalar.reg == pick.destination.reg) && vector.kind == RdnaOperandKind::VectorRegister && vector.reg == pick.source0.reg;
        };
        return reads(instruction.source0, instruction.source1) || reads(instruction.source1, instruction.source0);
    };
    if (std::none_of(program.instructions.begin() + *pickIndex + 1, program.instructions.begin() + callIndex, narrows)) return refuse("the waterfall loop does not compare the picked value with its VGPR before the call");
    if (*backIndex + 1u >= program.instructions.size()) return refuse("the waterfall loop is the end of the program");
    if (callees.empty()) return refuse("the call table holds no callee");
    SplitRegisters written;
    SplitVectors vectors;
    for (const auto& callee : callees) {
        if (auto reason = splitCalleeRefusal(callee, call, written, vectors)) return refuse(std::move(*reason));
    }
    for (auto index = loopIndex; index <= *backIndex; ++index) splitVectorWrites(program.instructions[index], vectors);
    const auto exitIndex = *backIndex + 1u;
    const auto loopProgramCounter = program.instructions[loopIndex].programCounter;
    const auto exitProgramCounter = program.instructions[exitIndex].programCounter;
    if (loopIndex == 0u) return refuse("the waterfall loop starts the program");
    const auto& before = program.instructions[loopIndex - 1u];
    if (IsDirectBranchOpcode(before.op) || before.op == RdnaOpcode::SEndpgm || before.op == RdnaOpcode::SSetpcB64 || before.op == RdnaOpcode::SSwappcB64 || before.op == RdnaOpcode::SCallB64) return refuse("the waterfall loop is not entered by falling through from the instruction before it");
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (index != callIndex && (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SCallB64)) return refuse("the program calls or jumps through a register at program counter " + toHexString(instruction.programCounter));
        if (!IsDirectBranchOpcode(instruction.op) || index == *backIndex) continue;
        const auto target = instruction.branchTarget;
        if (target >= loopProgramCounter && target < exitProgramCounter) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " enters the waterfall loop");
        if (index < loopIndex && target > exitProgramCounter) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " skips the end of the waterfall loop");
        if (index >= exitIndex && target < loopProgramCounter) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " runs the waterfall loop again");
    }
    for (std::uint32_t index = 0; index < loopIndex; ++index) {
        if (repeatsVisibly(program.instructions[index])) return refuse("the instruction at program counter " + toHexString(program.instructions[index].programCounter) + " before the waterfall loop writes memory or reads a value that changes between dispatches, and every part runs it again");
    }
    if (const auto reg = calleeRegisterReadAfterCall(program, callIndex, written, waveSize)) return refuse("a callee writes " + splitRegisterName(*reg) + ", which the program may read after the table call before writing it, so its value would depend on the order the waterfall loop serves its lanes in");
    TableCallSplit split{loopProgramCounter, call.programCounter, exitProgramCounter, {}};
    for (std::uint32_t reg = 0; reg < vectors.size(); ++reg) {
        if (vectors[reg]) split.vectors.push_back(reg);
    }
    return split;
}

namespace {

constexpr std::uint32_t NopWord = 0xbf800000u;
constexpr std::uint32_t EndProgramWord = 0xbf810000u;

void replaceInstruction(std::vector<std::uint32_t>& words, const RdnaProgram& program, std::uint32_t programCounter, std::uint32_t word) {
    const auto index = instructionIndexOfProgramCounter(program, programCounter);
    if (index == InvalidControlFlowId) throw std::logic_error("split program counter " + toHexString(programCounter) + " does not start an instruction");
    const auto end = instructionEndProgramCounter(program.instructions[index]) / 4u;
    for (auto at = programCounter / 4u; at < end; ++at) words[at] = NopWord;
    words[programCounter / 4u] = word;
}

SplitTableCallProgram splitProgram(std::vector<std::uint32_t> code, const TableCall& call, std::span<const std::uint32_t> vectors, std::uint64_t selectedTarget, std::initializer_list<SplitStateAbi::Point> points) {
    const std::vector<SplitStateAbi::Point> stored(points);
    return {std::move(code), SplitStateAbi::Encode({stored, vectors, static_cast<std::uint32_t>(vectors.size()), call.targetRegister, selectedTarget})};
}

}

SplitTableCallPrograms BuildSplitTableCallPrograms(const RdnaProgram& program, const TableCall& call, std::span<const TableCallee> callees, const TableCallSplit& split, std::uint32_t vectorCount) {
    using SplitStateAbi::Action;
    using SplitStateAbi::Area;
    const auto loopIndex = instructionIndexOfProgramCounter(program, split.loopProgramCounter);
    if (loopIndex == InvalidControlFlowId || loopIndex == 0u) throw std::logic_error("split loop does not follow an instruction");
    const auto end = instructionEndProgramCounter(program.instructions.back()) / 4u;
    const std::vector<std::uint32_t> words(program.code.begin(), program.code.begin() + end);
    std::vector<std::uint32_t> vectors;
    for (const auto reg : split.vectors) {
        if (reg < vectorCount) vectors.push_back(reg);
    }
    SplitTableCallPrograms programs;
    programs.stateVectors = static_cast<std::uint32_t>(vectors.size());
    auto entry = words;
    replaceInstruction(entry, program, split.callProgramCounter, NopWord);
    replaceInstruction(entry, program, split.exitProgramCounter, EndProgramWord);
    programs.entry = splitProgram(std::move(entry), call, vectors, 0u, {
        {0u, false, Action::ClearTarget, Area::Exit},
        {split.callProgramCounter, false, Action::SaveTarget, Area::Exit},
        {split.exitProgramCounter, false, Action::Save, Area::Exit},
    });
    auto replay = words;
    replaceInstruction(replay, program, split.exitProgramCounter, EndProgramWord);
    const auto replayProgram = RdnaInstructionDecoder{}.Decode(replay);
    const std::vector<TableCall> calls{call};
    for (const auto& callee : callees) {
        const std::vector<std::vector<TableCallee>> linkedCallees{{callee}};
        auto linked = LinkCalls(replayProgram, {}, {}, calls, linkedCallees);
        for (const auto fallback : linked.fallbacks) linked.code[fallback / 4u] = NopWord;
        programs.callees.push_back(splitProgram(std::move(linked.code), call, vectors, callee.target, {
            {split.exitProgramCounter, false, Action::SaveSelected, Area::Exit},
        }));
    }
    auto resume = words;
    replaceInstruction(resume, program, split.callProgramCounter, NopWord);
    programs.resume = splitProgram(std::move(resume), call, vectors, 0u, {
        {split.exitProgramCounter, false, Action::Load, Area::Exit},
    });
    return programs;
}

namespace {

bool endsFlow(RdnaOpcode op) {
    return op == RdnaOpcode::SBranch || op == RdnaOpcode::SEndpgm || op == RdnaOpcode::SSetpcB64 || op == RdnaOpcode::SSwappcB64;
}

bool relocatable(const RdnaInstruction& instruction) {
    return !IsDirectBranchOpcode(instruction.op) && !endsFlow(instruction.op) && instruction.op != RdnaOpcode::SGetpcB64 && instruction.op != RdnaOpcode::SCallB64;
}

constexpr std::uint32_t ContinuationStubWords = 64u;
constexpr std::uint32_t BranchWord = 0xbf820000u;
constexpr std::uint32_t BranchSccOneWord = 0xbf850000u;
constexpr std::uint32_t CompareNotZeroWord = 0xbf078000u;

std::uint32_t branchWord(std::uint32_t base, std::uint32_t from, std::uint32_t to) {
    const auto offset = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from) - 1;
    if (offset < std::numeric_limits<std::int16_t>::min() || offset > std::numeric_limits<std::int16_t>::max()) throw std::logic_error("continuation branch from dword " + std::to_string(from) + " to dword " + std::to_string(to) + " is out of range");
    return base | (static_cast<std::uint32_t>(offset) & 0xffffu);
}

std::vector<std::uint32_t> instructionWords(const RdnaProgram& program, std::uint32_t index) {
    const auto& instruction = program.instructions[index];
    return {program.code.begin() + instruction.programCounter / 4u, program.code.begin() + instructionEndProgramCounter(instruction) / 4u};
}

}

std::optional<TableCallContinuation> ContinueTableCall(const RdnaProgram& program, const TableCall& call, std::span<const TableCallee> callees, std::uint32_t vectorCount, std::string& refusal) {
    const auto refuse = [&](std::string reason) -> std::optional<TableCallContinuation> {
        refusal = std::move(reason);
        return std::nullopt;
    };
    const auto callIndex = instructionIndexOfProgramCounter(program, call.programCounter);
    if (callIndex == InvalidControlFlowId || program.instructions[callIndex].op != RdnaOpcode::SSwappcB64) return refuse("the table call does not name an s_swappc_b64");
    if (callIndex + 1u >= program.instructions.size()) return refuse("the table call is the end of the program");
    if (callees.empty()) return refuse("the call table holds no callee");
    if (call.targetRegister + 1u >= SplitStateAbi::ScalarRegisters) return refuse("the table call's target is not in an SGPR pair");
    const auto end = instructionEndProgramCounter(program.instructions.back()) / 4u;
    if (end + ContinuationStubWords > static_cast<std::uint32_t>(std::numeric_limits<std::int16_t>::max())) return refuse("the program holds " + std::to_string(end) + " dwords, too many for a branch from its start to a stub after its end");
    for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (index != callIndex && (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SCallB64)) return refuse("the program calls or jumps through a register at program counter " + toHexString(instruction.programCounter));
        if (instruction.family == RdnaInstructionFamily::DS) return refuse("the program accesses LDS at program counter " + toHexString(instruction.programCounter) + ", which does not outlive a dispatch");
        if (instruction.op == RdnaOpcode::SBarrier) return refuse("the program holds an s_barrier at program counter " + toHexString(instruction.programCounter));
    }
    auto first = callIndex;
    while (first > 0u && !endsFlow(program.instructions[first - 1u].op)) --first;
    const bool trampoline = first != 0u;
    std::uint32_t low = callIndex;
    std::uint32_t high = callIndex;
    if (trampoline) {
        const auto firstProgramCounter = program.instructions[first].programCounter;
        low = std::numeric_limits<std::uint32_t>::max();
        high = 0;
        for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
            const auto& instruction = program.instructions[index];
            if ((index >= first && index <= callIndex) || !IsDirectBranchOpcode(instruction.op)) continue;
            if (instruction.branchTarget < firstProgramCounter || instruction.branchTarget > call.programCounter) continue;
            low = std::min(low, index);
            high = std::max(high, index);
        }
        if (low > high) return refuse("no branch reaches the table call");
    }
    const auto returnsFromTrampoline = [&](std::uint32_t index) {
        return trampoline && index == callIndex + 1u && program.instructions[index].op == RdnaOpcode::SBranch;
    };
    for (bool grown = true; grown;) {
        grown = false;
        for (std::uint32_t index = 0; index < program.instructions.size(); ++index) {
            const auto& instruction = program.instructions[index];
            if (returnsFromTrampoline(index) || !IsDirectBranchOpcode(instruction.op) || instruction.branchTarget > instruction.programCounter) continue;
            const auto target = instructionIndexOfProgramCounter(program, instruction.branchTarget);
            if (target == InvalidControlFlowId) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " lands inside an instruction");
            if (index < low || target > high) continue;
            if (target < low || index > high) {
                low = std::min(low, target);
                high = std::max(high, index);
                grown = true;
            }
        }
    }
    if (low < 2u) return refuse("the loop around the table call starts at the beginning of the program");
    if (!relocatable(program.instructions[0]) || !relocatable(program.instructions[low - 1u])) return refuse("the first instruction or the one before the loop around the table call branches or reads the program counter");
    const auto lowProgramCounter = program.instructions[low].programCounter;
    const auto highProgramCounter = program.instructions[high].programCounter;
    for (std::uint32_t index = 0; index < low; ++index) {
        const auto& instruction = program.instructions[index];
        if (repeatsVisibly(instruction)) return refuse("the instruction at program counter " + toHexString(instruction.programCounter) + " before the loop around the table call writes memory or reads a value that changes between dispatches, and every round runs it again");
        if (IsDirectBranchOpcode(instruction.op) && instruction.branchTarget >= lowProgramCounter && instruction.branchTarget <= highProgramCounter) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " enters the loop around the table call");
    }
    for (auto index = low; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (IsDirectBranchOpcode(instruction.op) && instruction.branchTarget < lowProgramCounter) return refuse("the branch at program counter " + toHexString(instruction.programCounter) + " leaves the loop around the table call for an earlier instruction");
    }
    SplitRegisters calleeWritten;
    SplitVectors vectors;
    for (const auto& callee : callees) {
        if (auto reason = splitCalleeRefusal(callee, call, calleeWritten, vectors)) return refuse(std::move(*reason));
    }
    SplitRegisters callerWritten;
    for (auto index = low; index < program.instructions.size(); ++index) {
        callerWritten |= splitWrites(program.instructions[index]);
        splitVectorWrites(program.instructions[index], vectors);
    }
    SplitRegisters prefixWritten;
    for (std::uint32_t index = 0; index < low; ++index) prefixWritten |= splitWrites(program.instructions[index]);
    const auto written = calleeWritten | callerWritten;
    auto replay = callIndex;
    while (replay > low && relocatable(program.instructions[replay - 1u]) && !repeatsVisibly(program.instructions[replay - 1u])) --replay;
    for (const auto& instruction : program.instructions) {
        if (!IsDirectBranchOpcode(instruction.op)) continue;
        const auto target = instructionIndexOfProgramCounter(program, instruction.branchTarget);
        if (target != InvalidControlFlowId && target > replay && target <= callIndex) replay = target;
    }
    TableCallContinuation continuation{lowProgramCounter, program.instructions[replay].programCounter, call.programCounter, {}, {}};
    SplitRegisters replayReads;
    SplitRegisters replayWrites;
    for (auto index = replay; index < callIndex; ++index) {
        replayReads |= splitReads(program.instructions[index], 64u) & ~replayWrites;
        replayWrites |= splitWrites(program.instructions[index]);
    }
    for (std::uint32_t reg = 0; reg < vectorCount && reg < vectors.size(); ++reg) {
        if (vectors[reg]) continuation.registers.push_back(reg);
    }
    for (std::uint32_t reg = 0; reg < SplitStateAbi::ScalarRegisters; ++reg) {
        if (!written[reg]) continue;
        continuation.registers.push_back(SplitStateAbi::ScalarBase + reg);
        if (!callerWritten[reg] && prefixWritten[reg] && replayReads[reg]) continuation.assumed.push_back(SplitStateAbi::ScalarBase + reg);
    }
    continuation.registers.insert(continuation.registers.end(), {SplitStateAbi::ExecRegister, SplitStateAbi::VccRegister, SplitStateAbi::SccRegister, SplitStateAbi::M0Register});
    return continuation;
}

SplitTableCallPrograms BuildTableCallContinuation(const RdnaProgram& program, const TableCall& call, std::span<const TableCallee> callees, const TableCallContinuation& continuation) {
    using SplitStateAbi::Action;
    using SplitStateAbi::Area;
    const auto callIndex = instructionIndexOfProgramCounter(program, continuation.callProgramCounter);
    const auto headerIndex = instructionIndexOfProgramCounter(program, continuation.headerProgramCounter);
    const auto replayIndex = instructionIndexOfProgramCounter(program, continuation.replayProgramCounter);
    if (callIndex == InvalidControlFlowId || headerIndex == InvalidControlFlowId || replayIndex == InvalidControlFlowId || headerIndex < 2u || replayIndex < headerIndex || replayIndex > callIndex || callIndex + 1u >= program.instructions.size()) throw std::logic_error("table call continuation does not match its program");
    const auto end = instructionEndProgramCounter(program.instructions.back()) / 4u;
    const std::vector<std::uint32_t> words(program.code.begin(), program.code.begin() + end);
    const auto returnProgramCounter = program.instructions[callIndex + 1u].programCounter;
    auto relocated = words;
    const auto startStub = static_cast<std::uint32_t>(relocated.size());
    relocated.insert(relocated.end(), {CompareNotZeroWord | call.targetRegister, 0u, CompareNotZeroWord | (call.targetRegister + 1u), 0u, EndProgramWord});
    const auto startResume = static_cast<std::uint32_t>(relocated.size());
    relocated[startStub + 1u] = branchWord(BranchSccOneWord, startStub + 1u, startResume);
    relocated[startStub + 3u] = branchWord(BranchSccOneWord, startStub + 3u, startResume);
    const auto first = instructionWords(program, 0u);
    relocated.insert(relocated.end(), first.begin(), first.end());
    relocated.push_back(branchWord(BranchWord, static_cast<std::uint32_t>(relocated.size()), program.instructions[1].programCounter / 4u));
    replaceInstruction(relocated, program, 0u, branchWord(BranchWord, 0u, startStub));
    const auto headerStub = static_cast<std::uint32_t>(relocated.size());
    const auto before = instructionWords(program, headerIndex - 1u);
    relocated.insert(relocated.end(), before.begin(), before.end());
    const auto go = static_cast<std::uint32_t>(relocated.size());
    relocated.insert(relocated.end(), {NopWord, EndProgramWord});
    const auto beforeProgramCounter = program.instructions[headerIndex - 1u].programCounter;
    replaceInstruction(relocated, program, beforeProgramCounter, branchWord(BranchWord, beforeProgramCounter / 4u, headerStub));
    const auto slots = SplitStateAbi::SlotCount(continuation.registers);
    const auto plan = [&](std::vector<std::uint32_t> code, std::uint64_t selected, std::initializer_list<SplitStateAbi::Point> points, std::span<const std::uint32_t> assumed = {}) {
        const std::vector<SplitStateAbi::Point> stored(points);
        return SplitTableCallProgram{std::move(code), SplitStateAbi::Encode({stored, continuation.registers, slots, call.targetRegister, selected, assumed})};
    };
    SplitTableCallPrograms programs;
    programs.continuation = true;
    programs.stateVectors = slots;
    auto entry = words;
    replaceInstruction(entry, program, continuation.callProgramCounter, EndProgramWord);
    programs.entry = plan(std::move(entry), 0u, {
        {0u, false, Action::ClearTarget, Area::Exit},
        {continuation.replayProgramCounter, false, Action::SaveState, Area::Exit},
        {continuation.callProgramCounter, false, Action::StoreTarget, Area::Exit},
    });
    auto resume = relocated;
    resume[go] = branchWord(BranchWord, go, returnProgramCounter / 4u);
    replaceInstruction(resume, program, continuation.callProgramCounter, EndProgramWord);
    programs.resume = plan(std::move(resume), 0u, {
        {startStub * 4u, false, Action::LoadTarget, Area::Exit},
        {go * 4u, false, Action::LoadState, Area::Exit},
        {go * 4u, false, Action::ClearTarget, Area::Exit},
        {continuation.replayProgramCounter, false, Action::SaveState, Area::Exit},
        {continuation.callProgramCounter, false, Action::StoreTarget, Area::Exit},
    });
    auto served = relocated;
    served[go] = branchWord(BranchWord, go, continuation.replayProgramCounter / 4u);
    const auto servedProgram = RdnaInstructionDecoder{}.Decode(served);
    const std::vector<TableCall> calls{call};
    for (const auto& callee : callees) {
        const std::vector<std::vector<TableCallee>> linkedCallees{{callee}};
        auto linked = LinkCalls(servedProgram, {}, {}, calls, linkedCallees);
        for (const auto fallback : linked.fallbacks) linked.code[fallback / 4u] = EndProgramWord;
        replaceInstruction(linked.code, program, returnProgramCounter, EndProgramWord);
        programs.callees.push_back(plan(std::move(linked.code), callee.target, {
            {startStub * 4u, false, Action::LoadTarget, Area::Exit},
            {go * 4u, false, Action::LoadState, Area::Exit},
            {returnProgramCounter, false, Action::SaveState, Area::Exit},
        }, continuation.assumed));
    }
    return programs;
}

ControlFlowGraph GraphBuilder::Build(const RdnaProgram& program, const SwappcInfo* swappc) const {
    std::vector<std::uint32_t> unlinked;
    LinkEscape linkEscape;
    const std::vector<SwappcCall> calls = analyzeSwappcCalls(program, swappc, unlinked, linkEscape);
    ControlFlowGraph graph;
    for (const auto index : unlinked) graph.unlinkedCallProgramCounters.push_back(program.instructions[index].programCounter);
    graph.linkEscapeWaveSize = linkEscape.waveSize;
    graph.linkEscapeReason = std::move(linkEscape.reason);
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
