#include "Translation/RoundingMode.hpp"
#include "Translation/TranslationContext.hpp"
#include <deque>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t ModeRegisterId = 1u;

struct ModeField {
    std::uint32_t mask = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

std::optional<ModeField> roundingField(std::uint32_t simm16) {
    if ((simm16 & 0x3fu) != ModeRegisterId) return std::nullopt;
    const std::uint32_t offset = (simm16 >> 6u) & 0x1fu;
    const std::uint32_t size = ((simm16 >> 11u) & 0x1fu) + 1u;
    if (offset + size > 4u) return std::nullopt;
    return ModeField{((1u << size) - 1u) << offset, offset, size};
}

RoundingState transfer(RoundingState state, const BasicBlock& block, const RdnaProgram& program) {
    for (std::uint32_t index = block.instructionBegin; index < block.instructionEnd; ++index) {
        if (const auto write = DecodeRoundingWrite(program.instructions.at(index)); write.has_value()) {
            state = ApplyRoundingWrite(state, *write);
        }
    }
    return state;
}

bool setsDirectedRounding(std::uint32_t value, std::uint32_t initial, std::uint32_t fields) {
    for (const std::uint32_t group : {ModeRound32Bits, ModeRound1664Bits}) {
        const std::uint32_t used = fields & group;
        if (used != 0u && (value & group) != (initial & group) && (value & group) != 0u) return true;
    }
    return false;
}

}

std::optional<RoundingWrite> DecodeRoundingWrite(const RdnaInstruction& instruction) {
    switch (instruction.op) {
    case RdnaOpcode::SRoundMode:
        return RoundingWrite{ModeRoundBits, instruction.source0.value & ModeRoundBits, true};
    case RdnaOpcode::SSetregImm32B32: {
        const auto field = roundingField(instruction.source1.value);
        if (!field.has_value()) return std::nullopt;
        const std::uint32_t written = instruction.source0.value & ((1u << field->size) - 1u);
        return RoundingWrite{field->mask, written << field->offset, true};
    }
    case RdnaOpcode::SSetregB32: {
        const auto field = roundingField(instruction.source1.value);
        if (!field.has_value()) return std::nullopt;
        return RoundingWrite{field->mask, 0u, false};
    }
    default:
        return std::nullopt;
    }
}

RoundingState ApplyRoundingWrite(RoundingState state, const RoundingWrite& write) {
    const auto mask = static_cast<std::uint8_t>(write.mask & ModeRoundBits);
    if (write.known) {
        state.value = static_cast<std::uint8_t>((state.value & ~mask) | (write.value & mask));
        state.unknown = static_cast<std::uint8_t>(state.unknown & ~mask);
    } else {
        state.value = static_cast<std::uint8_t>(state.value & ~mask);
        state.unknown = static_cast<std::uint8_t>(state.unknown | mask);
    }
    return state;
}

RoundingState JoinRoundingStates(RoundingState lhs, RoundingState rhs) {
    RoundingState joined;
    joined.unknown = static_cast<std::uint8_t>(lhs.unknown | rhs.unknown | (lhs.value ^ rhs.value));
    joined.value = static_cast<std::uint8_t>(lhs.value & ~joined.unknown & ModeRoundBits);
    return joined;
}

RoundingUse RoundingUseOf(RdnaOpcode opcode) {
    constexpr std::uint32_t round32 = ModeRound32Bits;
    constexpr std::uint32_t round1664 = ModeRound1664Bits;
    switch (opcode) {
    case RdnaOpcode::VCvtF32I32:
    case RdnaOpcode::VCvtF32U32:
    case RdnaOpcode::VLdexpF32:
    case RdnaOpcode::VCvtPkU8F32:
        return {round32, RoundingUseKind::Directed32};
    case RdnaOpcode::VCvtF16F32:
    case RdnaOpcode::VCvtF16U16:
    case RdnaOpcode::VCvtF16I16:
    case RdnaOpcode::VLdexpF16:
        return {round1664, RoundingUseKind::Directed16};
    case RdnaOpcode::VMadMixloF16:
    case RdnaOpcode::VMadMixhiF16:
    case RdnaOpcode::VInterpP1llF16:
    case RdnaOpcode::VInterpP1lvF16:
    case RdnaOpcode::VInterpP2F16:
        return {ModeRoundBits, RoundingUseKind::SelfChecked};
    case RdnaOpcode::VPkAddF16:
    case RdnaOpcode::VPkMulF16:
    case RdnaOpcode::VPkFmaF16:
    case RdnaOpcode::VPkFmacF16:
        return {round1664, RoundingUseKind::SelfChecked};
    case RdnaOpcode::VAddF32:
    case RdnaOpcode::VSubF32:
    case RdnaOpcode::VSubrevF32:
    case RdnaOpcode::VMulF32:
    case RdnaOpcode::VMulLegacyF32:
    case RdnaOpcode::VMadF32:
    case RdnaOpcode::VMacF32:
    case RdnaOpcode::VMadLegacyF32:
    case RdnaOpcode::VMacLegacyF32:
    case RdnaOpcode::VMullitF32:
    case RdnaOpcode::VFmaF32:
    case RdnaOpcode::VMadmkF32:
    case RdnaOpcode::VMadakF32:
    case RdnaOpcode::VFractF32:
    case RdnaOpcode::VCvtF32F64:
    case RdnaOpcode::VDivScaleF32:
    case RdnaOpcode::VDivFmasF32:
    case RdnaOpcode::VDivFixupF32:
    case RdnaOpcode::VCubemaF32:
    case RdnaOpcode::VDot2F32F16:
    case RdnaOpcode::VDot2cF32F16:
    case RdnaOpcode::VInterpP1F32:
    case RdnaOpcode::VInterpP2F32:
        return {round32, RoundingUseKind::Unmodelled};
    case RdnaOpcode::VAddF16:
    case RdnaOpcode::VSubF16:
    case RdnaOpcode::VSubrevF16:
    case RdnaOpcode::VMulF16:
    case RdnaOpcode::VFmacF16:
    case RdnaOpcode::VFmamkF16:
    case RdnaOpcode::VFmaakF16:
    case RdnaOpcode::VFmaF16:
    case RdnaOpcode::VDivFixupF16:
    case RdnaOpcode::VFractF16:
    case RdnaOpcode::VFmaF64:
    case RdnaOpcode::VDivFmasF64:
    case RdnaOpcode::VAddF64:
    case RdnaOpcode::VMulF64:
    case RdnaOpcode::VLdexpF64:
    case RdnaOpcode::VFractF64:
        return {round1664, RoundingUseKind::Unmodelled};
    default:
        return {};
    }
}

std::unordered_map<std::uint32_t, RoundingState> ComputeEntryRounding(const ControlFlowGraph& cfg, const RdnaProgram& program, std::uint32_t initial) {
    std::unordered_map<std::uint32_t, RoundingState> entries;
    if (cfg.blocks.empty()) return entries;
    std::unordered_map<std::uint32_t, const BasicBlock*> blocks;
    for (const auto& block : cfg.blocks) blocks.emplace(block.id, &block);

    RoundingState start;
    start.value = static_cast<std::uint8_t>(initial & ModeRoundBits);
    entries.emplace(cfg.blocks.front().id, start);
    std::deque<std::uint32_t> pending{cfg.blocks.front().id};
    while (!pending.empty()) {
        const std::uint32_t id = pending.front();
        pending.pop_front();
        const BasicBlock& block = *blocks.at(id);
        const RoundingState exit = transfer(entries.at(id), block, program);
        for (const std::uint32_t successor : block.successors) {
            const auto target = blocks.find(successor);
            if (target == blocks.end()) throw std::runtime_error("control flow graph block " + std::to_string(id) + " has unknown successor " + std::to_string(successor));
            const auto existing = entries.find(successor);
            if (existing == entries.end()) {
                entries.emplace(successor, exit);
                pending.push_back(successor);
                continue;
            }
            const RoundingState joined = JoinRoundingStates(existing->second, exit);
            if (joined == existing->second) continue;
            existing->second = joined;
            pending.push_back(successor);
        }
    }
    for (const auto& block : cfg.blocks) entries.emplace(block.id, start);
    return entries;
}

void TranslationContext::setRounding(const RoundingState& state) {
    rounding = state;
    if (!declaredFloatMode.has_value()) return;
    ShaderFloatMode mode = *declaredFloatMode;
    mode.floatMode = (mode.floatMode & ~ModeRoundBits) | (state.value & ModeRoundBits);
    floatMode = mode;
}

void TranslationContext::writeRounding(const RdnaInstruction& inst) {
    const auto write = DecodeRoundingWrite(inst);
    const char* name = inst.op == RdnaOpcode::SRoundMode ? "s_round_mode" : inst.op == RdnaOpcode::SSetregB32 ? "s_setreg_b32" : "s_setreg_imm32_b32";
    if (!write.has_value()) {
        throw std::runtime_error(std::string(name) + " at pc " + std::to_string(inst.programCounter) + " writes a hardware register field other than the MODE rounding fields");
    }
    const RoundingState next = ApplyRoundingWrite(rounding, *write);
    if (!declaredFloatMode.has_value() && (next.value != 0u || next.unknown != 0u)) {
        throw std::runtime_error(std::string(name) + " at pc " + std::to_string(inst.programCounter) + " changes the rounding fields of a shader without a declared FLOAT_MODE: not implemented");
    }
    setRounding(next);
}

void TranslationContext::checkRoundingUse(const RdnaInstruction& inst) const {
    const RoundingUse use = RoundingUseOf(inst.op);
    if (use.fields == 0u) return;
    const std::string where = "instruction (opcode " + std::to_string(inst.opcodeId) + ") at pc " + std::to_string(inst.programCounter);
    if ((rounding.unknown & use.fields) != 0u) {
        throw std::runtime_error(where + " reads MODE rounding bits that differ between the paths reaching it or were written from an SGPR: not implemented");
    }
    const std::uint32_t initial = declaredFloatMode.has_value() ? declaredFloatMode->floatMode : 0u;
    const std::uint32_t changed = (rounding.value ^ initial) & use.fields;
    if (changed == 0u) return;
    switch (use.kind) {
    case RoundingUseKind::Directed32:
    case RoundingUseKind::SelfChecked:
        return;
    case RoundingUseKind::Directed16:
        if (((declaredFloatMode.has_value() ? declaredFloatMode->floatMode : 0xc0u) & 0xc0u) != 0xc0u) {
            throw std::runtime_error(where + " runs with an f16 rounding mode set at run time and f16 denormals not kept: not implemented");
        }
        return;
    case RoundingUseKind::Unmodelled:
        if (!setsDirectedRounding(rounding.value, initial, use.fields)) return;
        throw std::runtime_error(where + " runs with MODE rounding " + std::to_string(rounding.value) + " set at run time, and the instruction does not model directed rounding");
    }
}

}
