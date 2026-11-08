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
    if (offset + size > 8u) return std::nullopt;
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
    case RdnaOpcode::SDenormMode:
        return RoundingWrite{ModeDenormBits, (instruction.source0.value << 4u) & ModeDenormBits, true};
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
    const auto mask = static_cast<std::uint8_t>(write.mask & ModeTrackedBits);
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
    joined.value = static_cast<std::uint8_t>(lhs.value & ~joined.unknown & ModeTrackedBits);
    return joined;
}

RoundingUse RoundingUseOf(RdnaOpcode opcode) {
    constexpr std::uint32_t round32 = ModeRound32Bits;
    constexpr std::uint32_t round1664 = ModeRound1664Bits;
    constexpr std::uint32_t denorm32In = 0x10u;
    constexpr std::uint32_t denorm32 = ModeDenorm32Bits;
    constexpr std::uint32_t denorm1664In = 0x40u;
    constexpr std::uint32_t denorm1664Out = 0x80u;
    constexpr std::uint32_t denorm1664 = ModeDenorm1664Bits;
    switch (opcode) {
    case RdnaOpcode::VCvtF32I32:
    case RdnaOpcode::VCvtF32U32:
        return {round32, RoundingUseKind::Directed32};
    case RdnaOpcode::VCvtPkU8F32:
        return {round32 | denorm32In, RoundingUseKind::Directed32};
    case RdnaOpcode::VLdexpF32:
        return {round32 | denorm32, RoundingUseKind::Directed32};
    case RdnaOpcode::VCvtF16F32:
        return {round1664 | denorm1664Out | denorm32In, RoundingUseKind::Directed16};
    case RdnaOpcode::VCvtF16U16:
    case RdnaOpcode::VCvtF16I16:
        return {round1664 | denorm1664Out, RoundingUseKind::Directed16};
    case RdnaOpcode::VLdexpF16:
        return {round1664 | denorm1664, RoundingUseKind::Directed16};
    case RdnaOpcode::VInterpP1llF16:
    case RdnaOpcode::VInterpP1lvF16:
    case RdnaOpcode::VInterpP2F16:
        return {ModeTrackedBits, RoundingUseKind::SelfChecked};
    case RdnaOpcode::VMadMixloF16:
    case RdnaOpcode::VMadMixhiF16:
        return {ModeTrackedBits, RoundingUseKind::SelfChecked};
    case RdnaOpcode::VPkAddF16:
    case RdnaOpcode::VPkMulF16:
    case RdnaOpcode::VPkFmaF16:
    case RdnaOpcode::VPkFmacF16:
    case RdnaOpcode::VPkMinF16:
    case RdnaOpcode::VPkMaxF16:
        return {round1664 | denorm1664, RoundingUseKind::SelfChecked};
    case RdnaOpcode::VAddF32:
    case RdnaOpcode::VSubF32:
    case RdnaOpcode::VSubrevF32:
    case RdnaOpcode::VMulF32:
    case RdnaOpcode::VMulLegacyF32:
    case RdnaOpcode::VMacF32:
    case RdnaOpcode::VMullitF32:
    case RdnaOpcode::VFmaF32:
    case RdnaOpcode::VMadmkF32:
    case RdnaOpcode::VMadakF32:
    case RdnaOpcode::VFractF32:
    case RdnaOpcode::VDivScaleF32:
    case RdnaOpcode::VDivFixupF32:
        return {round32 | denorm32, RoundingUseKind::Unmodelled};
    case RdnaOpcode::VMadF32:
    case RdnaOpcode::VMadLegacyF32:
    case RdnaOpcode::VMacLegacyF32:
    case RdnaOpcode::VDivFmasF32:
    case RdnaOpcode::VCvtF32F64:
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
    case RdnaOpcode::VFractF16:
    case RdnaOpcode::VFmaF64:
    case RdnaOpcode::VAddF64:
    case RdnaOpcode::VMulF64:
    case RdnaOpcode::VLdexpF64:
    case RdnaOpcode::VFractF64:
        return {round1664 | denorm1664, RoundingUseKind::Unmodelled};
    case RdnaOpcode::VDivFmasF64:
        return {round1664 | denorm1664Out, RoundingUseKind::Unmodelled};
    case RdnaOpcode::VDivFixupF16:
        return {round1664 | denorm1664In, RoundingUseKind::Unmodelled};
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
    case RdnaOpcode::VMinF64:
    case RdnaOpcode::VMaxF64:
    case RdnaOpcode::VDivFixupF64:
    case RdnaOpcode::VFloorF64:
    case RdnaOpcode::VCeilF64:
    case RdnaOpcode::VFrexpMantF64:
    case RdnaOpcode::VFrexpExpI32F64:
    case RdnaOpcode::VMinF16:
    case RdnaOpcode::VMaxF16:
    case RdnaOpcode::VMin3F16:
    case RdnaOpcode::VMax3F16:
    case RdnaOpcode::VMed3F16:
    case RdnaOpcode::VCmpEqF16:
    case RdnaOpcode::VCmpGeF16:
    case RdnaOpcode::VCmpGtF16:
    case RdnaOpcode::VCmpLeF16:
    case RdnaOpcode::VCmpLgF16:
    case RdnaOpcode::VCmpLtF16:
    case RdnaOpcode::VCmpNeqF16:
    case RdnaOpcode::VCmpNgeF16:
    case RdnaOpcode::VCmpNgtF16:
    case RdnaOpcode::VCmpNleF16:
    case RdnaOpcode::VCmpNlgF16:
    case RdnaOpcode::VCmpNltF16:
    case RdnaOpcode::VCmpxEqF16:
    case RdnaOpcode::VCmpxGeF16:
    case RdnaOpcode::VCmpxGtF16:
    case RdnaOpcode::VCmpxLeF16:
    case RdnaOpcode::VCmpxLgF16:
    case RdnaOpcode::VCmpxLtF16:
    case RdnaOpcode::VCmpxNeqF16:
    case RdnaOpcode::VCmpxNgeF16:
    case RdnaOpcode::VCmpxNgtF16:
    case RdnaOpcode::VCmpxNleF16:
    case RdnaOpcode::VCmpxNlgF16:
    case RdnaOpcode::VCmpxNltF16:
    case RdnaOpcode::VCvtF32F16:
    case RdnaOpcode::VPackB32F16:
    case RdnaOpcode::VCvtNormI16F16:
    case RdnaOpcode::VCvtNormU16F16:
    case RdnaOpcode::VCvtPknormI16F16:
    case RdnaOpcode::VCvtPknormU16F16:
    case RdnaOpcode::VFrexpMantF16:
    case RdnaOpcode::VFrexpExpI16F16:
    case RdnaOpcode::DsCmpstF64:
    case RdnaOpcode::DsCmpstRtnF64:
    case RdnaOpcode::DsMaxF64:
    case RdnaOpcode::DsMaxRtnF64:
    case RdnaOpcode::DsMinF64:
    case RdnaOpcode::DsMinRtnF64:
    case RdnaOpcode::BufferAtomicFcmpswapX2:
    case RdnaOpcode::BufferAtomicFmaxX2:
    case RdnaOpcode::BufferAtomicFminX2:
    case RdnaOpcode::FlatAtomicFcmpswapX2:
    case RdnaOpcode::FlatAtomicFmaxX2:
    case RdnaOpcode::FlatAtomicFminX2:
        return {denorm1664In, RoundingUseKind::Denormal};
    case RdnaOpcode::VDivScaleF64:
    case RdnaOpcode::VRcpF16:
    case RdnaOpcode::VSqrtF16:
    case RdnaOpcode::VRsqF16:
    case RdnaOpcode::VLogF16:
    case RdnaOpcode::VExpF16:
    case RdnaOpcode::VFloorF16:
    case RdnaOpcode::VCeilF16:
    case RdnaOpcode::VTruncF16:
    case RdnaOpcode::VRndneF16:
    case RdnaOpcode::VSinF16:
    case RdnaOpcode::VCosF16:
        return {denorm1664, RoundingUseKind::Denormal};
    case RdnaOpcode::VCmpEqF32:
    case RdnaOpcode::VCmpGeF32:
    case RdnaOpcode::VCmpGtF32:
    case RdnaOpcode::VCmpLeF32:
    case RdnaOpcode::VCmpLgF32:
    case RdnaOpcode::VCmpLtF32:
    case RdnaOpcode::VCmpNeqF32:
    case RdnaOpcode::VCmpNgeF32:
    case RdnaOpcode::VCmpNgtF32:
    case RdnaOpcode::VCmpNleF32:
    case RdnaOpcode::VCmpNlgF32:
    case RdnaOpcode::VCmpNltF32:
    case RdnaOpcode::VCmpxEqF32:
    case RdnaOpcode::VCmpxGeF32:
    case RdnaOpcode::VCmpxGtF32:
    case RdnaOpcode::VCmpxLeF32:
    case RdnaOpcode::VCmpxLgF32:
    case RdnaOpcode::VCmpxLtF32:
    case RdnaOpcode::VCmpxNeqF32:
    case RdnaOpcode::VCmpxNgeF32:
    case RdnaOpcode::VCmpxNgtF32:
    case RdnaOpcode::VCmpxNleF32:
    case RdnaOpcode::VCmpxNlgF32:
    case RdnaOpcode::VCmpxNltF32:
    case RdnaOpcode::VCvtI32F32:
    case RdnaOpcode::VCvtU32F32:
    case RdnaOpcode::VCvtRpiI32F32:
    case RdnaOpcode::VCvtFlrI32F32:
    case RdnaOpcode::VFrexpExpI32F32:
    case RdnaOpcode::VCvtF64F32:
    case RdnaOpcode::VCvtPknormI16F32:
    case RdnaOpcode::VCvtPknormU16F32:
    case RdnaOpcode::DsMaxF32:
    case RdnaOpcode::DsMaxRtnF32:
    case RdnaOpcode::DsMinF32:
    case RdnaOpcode::DsMinRtnF32:
    case RdnaOpcode::DsCmpstF32:
    case RdnaOpcode::DsCmpstRtnF32:
    case RdnaOpcode::BufferAtomicFcmpswap:
    case RdnaOpcode::BufferAtomicFmax:
    case RdnaOpcode::BufferAtomicFmin:
    case RdnaOpcode::FlatAtomicFcmpswap:
    case RdnaOpcode::FlatAtomicFmax:
    case RdnaOpcode::FlatAtomicFmin:
    case RdnaOpcode::ImageAtomicFcmpswap:
    case RdnaOpcode::ImageAtomicFmax:
    case RdnaOpcode::ImageAtomicFmin:
        return {denorm32In, RoundingUseKind::Denormal};
    case RdnaOpcode::DsAddF32:
    case RdnaOpcode::DsAddRtnF32:
    case RdnaOpcode::VMinF32:
    case RdnaOpcode::VMaxF32:
    case RdnaOpcode::VMin3F32:
    case RdnaOpcode::VMax3F32:
    case RdnaOpcode::VMed3F32:
    case RdnaOpcode::VFrexpMantF32:
    case RdnaOpcode::VSinF32:
    case RdnaOpcode::VCosF32:
    case RdnaOpcode::VFloorF32:
    case RdnaOpcode::VCeilF32:
        return {denorm32, RoundingUseKind::Denormal};
    case RdnaOpcode::VCvtPkrtzF16F32:
        return {denorm32In | denorm1664Out, RoundingUseKind::Denormal};
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
    start.value = static_cast<std::uint8_t>(initial & ModeTrackedBits);
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
    mode.floatMode = (mode.floatMode & ~ModeTrackedBits) | (state.value & ModeTrackedBits);
    floatMode = mode;
}

void TranslationContext::writeRounding(const RdnaInstruction& inst) {
    const auto write = DecodeRoundingWrite(inst);
    const char* name = inst.op == RdnaOpcode::SRoundMode ? "s_round_mode" : inst.op == RdnaOpcode::SDenormMode ? "s_denorm_mode" : inst.op == RdnaOpcode::SSetregB32 ? "s_setreg_b32" : "s_setreg_imm32_b32";
    if (!write.has_value()) {
        throw std::runtime_error(std::string(name) + " at pc " + std::to_string(inst.programCounter) + " writes a hardware register field other than the MODE round and denormal fields");
    }
    const RoundingState next = ApplyRoundingWrite(rounding, *write);
    if (!declaredFloatMode.has_value() && (next.value != ModeUndeclaredSeed || next.unknown != 0u)) {
        throw std::runtime_error(std::string(name) + " at pc " + std::to_string(inst.programCounter) + " changes the MODE fields of a shader without a declared FLOAT_MODE: not implemented");
    }
    setRounding(next);
}

void TranslationContext::checkRoundingUse(const RdnaInstruction& inst) const {
    const RoundingUse use = RoundingUseOf(inst.op);
    if (use.fields == 0u) return;
    const std::string where = "instruction (opcode " + std::to_string(inst.opcodeId) + ") at pc " + std::to_string(inst.programCounter);
    if ((rounding.unknown & use.fields) != 0u) {
        throw std::runtime_error(where + " reads MODE bits that differ between the paths reaching it or were written from an SGPR: not implemented");
    }
    const std::uint32_t initial = declaredFloatMode.has_value() ? declaredFloatMode->floatMode : ModeUndeclaredSeed;
    const std::uint32_t changed = (rounding.value ^ initial) & use.fields;
    if (changed == 0u) return;
    switch (use.kind) {
    case RoundingUseKind::Directed32:
    case RoundingUseKind::SelfChecked:
    case RoundingUseKind::Denormal:
        return;
    case RoundingUseKind::Directed16:
        if (((floatMode.has_value() ? floatMode->floatMode : 0xc0u) & ModeDenorm1664Bits) != ModeDenorm1664Bits) {
            throw std::runtime_error(where + " runs with an f16 rounding mode set at run time and f16 denormals not kept: not implemented");
        }
        return;
    case RoundingUseKind::Unmodelled:
        if (!setsDirectedRounding(rounding.value, initial, use.fields)) return;
        throw std::runtime_error(where + " runs with MODE rounding " + std::to_string(rounding.value) + " set at run time, and the instruction does not model directed rounding");
    }
}

}
