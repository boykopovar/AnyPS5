#include "Translation/EmbeddedVertexFetch.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t kScalarSlotCount = 108;
constexpr std::uint32_t kVectorSlotCount = 256;
constexpr std::uint32_t kVccLoSlot = 106;
constexpr std::uint32_t kVccHiSlot = 107;
constexpr std::uint32_t kEmbeddedFetchRegisterShift = 8;
constexpr std::uint32_t kVertexIndexVgpr = 5;
constexpr std::uint32_t kInstanceIndexVgpr = 8;

enum class SgprValueKind {
    Unknown,
    Constant,
    AttributeTable,
    Attribute,
    BufferTable,
    Buffer
};

struct SgprValue {
    SgprValueKind kind = SgprValueKind::Unknown;
    std::int32_t attributeId = -1;
    std::uint32_t constant = 0;
    std::vector<std::uint32_t> loadPcs;
};

using ScalarBank = std::array<SgprValue, kScalarSlotCount>;
using VectorLaneMap = std::map<std::uint64_t, SgprValue>;

bool isScalarOperand(const RdnaOperand& operand) {
    return operand.kind == RdnaOperandKind::ScalarRegister || operand.kind == RdnaOperandKind::VccLo || operand.kind == RdnaOperandKind::VccHi;
}

bool isVectorOperand(const RdnaOperand& operand) {
    return operand.kind == RdnaOperandKind::VectorRegister;
}

std::uint32_t scalarSlot(const RdnaOperand& operand) {
    switch (operand.kind) {
    case RdnaOperandKind::VccLo:
        return kVccLoSlot;
    case RdnaOperandKind::VccHi:
        return kVccHiSlot;
    default:
        return operand.reg;
    }
}

std::uint64_t vectorLaneKey(std::uint32_t reg, std::uint32_t lane) {
    return (static_cast<std::uint64_t>(reg) << 32u) | lane;
}

std::uint32_t normalizeLane(std::uint32_t lane, std::uint32_t waveSize) {
    return lane % waveSize;
}

void clearVectorLanes(VectorLaneMap& lanes, std::uint32_t reg) {
    const auto first = lanes.lower_bound(vectorLaneKey(reg, 0u));
    const auto last = lanes.lower_bound(vectorLaneKey(reg + 1u, 0u));
    lanes.erase(first, last);
}

void clearScalarRange(ScalarBank& sgprs, const RdnaOperand& dst, std::uint32_t size) {
    if (!isScalarOperand(dst)) {
        return;
    }
    const auto slot = scalarSlot(dst);
    for (std::uint32_t i = 0u; i < size && slot + i < sgprs.size(); i++) {
        sgprs[slot + i] = SgprValue{};
    }
}

bool tryConstantOperand(const ScalarBank& sgprs, const RdnaOperand& operand, std::uint32_t& value) {
    switch (operand.kind) {
    case RdnaOperandKind::LiteralConstant:
    case RdnaOperandKind::IntegerInlineConstant:
    case RdnaOperandKind::FloatInlineConstant:
        value = operand.value;
        return true;
    case RdnaOperandKind::Null:
        value = 0u;
        return true;
    default:
        break;
    }
    if (isScalarOperand(operand) && scalarSlot(operand) < sgprs.size() && sgprs[scalarSlot(operand)].kind == SgprValueKind::Constant) {
        value = sgprs[scalarSlot(operand)].constant;
        return true;
    }
    return false;
}

bool trySmemOffset(const ScalarBank& sgprs, const RdnaInstruction& inst, std::uint32_t& rawOffset) {
    std::uint32_t base = 0u;
    if (!tryConstantOperand(sgprs, inst.source1, base)) {
        return false;
    }
    const std::uint64_t value = static_cast<std::uint64_t>(base) + inst.memoryOffset;
    if (value > 0xFFFFFFFFull) {
        return false;
    }
    rawOffset = static_cast<std::uint32_t>(value);
    return true;
}

bool isScalarLoad(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::SLoadDword:
    case RdnaOpcode::SLoadDwordx2:
    case RdnaOpcode::SLoadDwordx4:
    case RdnaOpcode::SLoadDwordx8:
    case RdnaOpcode::SLoadDwordx16:
        return true;
    default:
        return false;
    }
}

bool isFormattedBufferLoad(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::BufferLoadFormatX:
    case RdnaOpcode::BufferLoadFormatXy:
    case RdnaOpcode::BufferLoadFormatXyz:
    case RdnaOpcode::BufferLoadFormatXyzw:
        return true;
    default:
        return false;
    }
}

bool isAttributePropagationAlu(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::SBfeU32:
    case RdnaOpcode::SAndB32:
    case RdnaOpcode::SAddI32:
    case RdnaOpcode::SAddU32:
    case RdnaOpcode::SLshlB32:
        return true;
    default:
        return false;
    }
}

std::int32_t bufferAttributeFromOffset(std::uint32_t rawOffset, std::uint32_t dword) {
    return static_cast<std::int32_t>((rawOffset + dword * 4u) / 16u);
}

std::uint32_t decodedDstSize(const RdnaInstruction& inst) {
    return std::max<std::uint32_t>(inst.dataDwordCount, 1u);
}

std::uint32_t embeddedFetchDstSize(const RdnaInstruction& inst) {
    return inst.op == RdnaOpcode::VMadU64U32 ? 2u : decodedDstSize(inst);
}

// How many consecutive SGPRs a scalar operand of the instruction names, over-estimated where the
// decoder does not say (descriptor bases, 64-bit pairs, carry-ins): a register counted as read that
// is not only costs the driver's fold decision, never its correctness.
std::uint32_t scalarOperandWidth(const RdnaInstruction& inst, std::uint32_t operandIndex) {
    switch (inst.family) {
    case RdnaInstructionFamily::SMEM:
        return operandIndex == 0u ? 4u : 1u;
    case RdnaInstructionFamily::MUBUF:
    case RdnaInstructionFamily::MTBUF:
        return operandIndex == 1u ? 4u : 1u;
    case RdnaInstructionFamily::MIMG:
        return 8u;
    case RdnaInstructionFamily::FLAT:
        return 2u;
    case RdnaInstructionFamily::VOP2:
    case RdnaInstructionFamily::VOP3:
    case RdnaInstructionFamily::VOP3P:
        return inst.is64Bit || operandIndex == 2u ? 2u : 1u;
    default:
        return inst.is64Bit ? 2u : 1u;
    }
}

bool touchesSgpr(const RdnaInstruction& inst, std::uint32_t sgpr) {
    const std::array<const RdnaOperand*, 4> sources{&inst.source0, &inst.source1, &inst.source2, &inst.source3};
    for (std::uint32_t i = 0u; i < sources.size(); i++) {
        if (!isScalarOperand(*sources[i])) continue;
        const auto first = scalarSlot(*sources[i]);
        if (sgpr >= first && sgpr < first + scalarOperandWidth(inst, i)) return true;
    }
    if (isScalarOperand(inst.destination)) {
        const auto first = scalarSlot(inst.destination);
        if (sgpr >= first && sgpr < first + std::max(inst.is64Bit ? 2u : 1u, decodedDstSize(inst))) return true;
    }
    return false;
}

}

EmbeddedFetchPlan EmbeddedVertexFetchAnalyzer::Analyze(const RdnaProgram& program, std::uint32_t attributeTableRegister, std::uint32_t bufferTableRegister, std::uint32_t userDataBaseRegister, std::uint32_t userDataCount, std::uint32_t waveSize) const {
    if (waveSize != 32u && waveSize != 64u) {
        throw std::runtime_error("unsupported wave size for embedded vertex fetch analysis");
    }
    const std::uint32_t attribSlot = attributeTableRegister + kEmbeddedFetchRegisterShift;
    const std::uint32_t bufferSlot = bufferTableRegister + kEmbeddedFetchRegisterShift;
    if (attribSlot + 1u >= kScalarSlotCount || bufferSlot + 1u >= kScalarSlotCount) {
        throw std::runtime_error("embedded vertex fetch register out of range");
    }

    EmbeddedFetchPlan plan;
    ScalarBank sgprs{};
    std::array<bool, kVectorSlotCount> vgprIsIndex{};
    VectorLaneMap vectorLanes;
    const bool trackVectorLanes = std::none_of(program.instructions.begin(), program.instructions.end(), [](const RdnaInstruction& inst) {
        return IsDirectBranchOpcode(inst.op) || inst.op == RdnaOpcode::SSetpcB64;
    });

    sgprs[attribSlot].kind = SgprValueKind::AttributeTable;
    sgprs[attribSlot + 1u].kind = SgprValueKind::AttributeTable;
    sgprs[bufferSlot].kind = SgprValueKind::BufferTable;
    sgprs[bufferSlot + 1u].kind = SgprValueKind::BufferTable;

    std::int32_t vertexOffsetCandidate = -1;
    std::int32_t instanceOffsetCandidate = -1;
    bool vertexOffsetConflict = false;
    bool instanceOffsetConflict = false;
    std::vector<std::uint32_t> vertexAddPcs;
    std::vector<std::uint32_t> instanceAddPcs;

    for (const auto& inst : program.instructions) {
        const bool vertexIndexAccumulator = isVectorOperand(inst.destination) && (inst.destination.reg == 0u || (userDataBaseRegister == 8u && inst.destination.reg == kVertexIndexVgpr));
        const bool instanceIndexAccumulator = isVectorOperand(inst.destination) && (inst.destination.reg == (userDataBaseRegister == 8u ? kInstanceIndexVgpr : 3u));
        std::uint32_t sadZero = 0u;
        const bool indexOffsetAdd = (vertexIndexAccumulator || instanceIndexAccumulator) && isScalarOperand(inst.source0) &&
            ((inst.op == RdnaOpcode::VAddI32 && isVectorOperand(inst.source1) && inst.source1.reg == inst.destination.reg) ||
             (userDataBaseRegister == 8u && (inst.destination.reg == kVertexIndexVgpr || inst.destination.reg == kInstanceIndexVgpr) &&
              inst.op == RdnaOpcode::VSadU32 && isVectorOperand(inst.source2) && inst.source2.reg == inst.destination.reg &&
              tryConstantOperand(sgprs, inst.source1, sadZero) && sadZero == 0u));
        if (plan.loads.empty() && indexOffsetAdd) {
            const auto reg = scalarSlot(inst.source0);
            if (reg >= userDataBaseRegister && reg - userDataBaseRegister < userDataCount) {
                auto& candidate = vertexIndexAccumulator ? vertexOffsetCandidate : instanceOffsetCandidate;
                auto& conflict = vertexIndexAccumulator ? vertexOffsetConflict : instanceOffsetConflict;
                if (candidate >= 0 && candidate != static_cast<std::int32_t>(reg)) {
                    conflict = true;
                } else {
                    candidate = static_cast<std::int32_t>(reg);
                }
                (vertexIndexAccumulator ? vertexAddPcs : instanceAddPcs).push_back(inst.programCounter);
            }
        }
        switch (inst.op) {
        case RdnaOpcode::VWritelaneB32: {
            std::uint32_t lane = 0u;
            if (isVectorOperand(inst.destination) && inst.destination.reg < vgprIsIndex.size()) {
                vgprIsIndex[inst.destination.reg] = false;
            }
            if (trackVectorLanes && isVectorOperand(inst.destination) && isScalarOperand(inst.source0) && scalarSlot(inst.source0) < sgprs.size() && tryConstantOperand(sgprs, inst.source1, lane)) {
                vectorLanes[vectorLaneKey(inst.destination.reg, normalizeLane(lane, waveSize))] = sgprs[scalarSlot(inst.source0)];
            } else if (isVectorOperand(inst.destination)) {
                clearVectorLanes(vectorLanes, inst.destination.reg);
            }
            break;
        }
        case RdnaOpcode::VReadlaneB32: {
            std::uint32_t lane = 0u;
            if (trackVectorLanes && isScalarOperand(inst.destination) && scalarSlot(inst.destination) < sgprs.size() && isVectorOperand(inst.source0) && tryConstantOperand(sgprs, inst.source1, lane)) {
                const auto found = vectorLanes.find(vectorLaneKey(inst.source0.reg, normalizeLane(lane, waveSize)));
                sgprs[scalarSlot(inst.destination)] = found != vectorLanes.end() ? found->second : SgprValue{};
            } else if (isScalarOperand(inst.destination)) {
                clearScalarRange(sgprs, inst.destination, 1u);
            }
            break;
        }
        case RdnaOpcode::SMovB32:
            if (isScalarOperand(inst.destination) && isScalarOperand(inst.source0) && scalarSlot(inst.source0) < sgprs.size()) {
                sgprs[scalarSlot(inst.destination)] = sgprs[scalarSlot(inst.source0)];
            } else if (isScalarOperand(inst.destination)) {
                std::uint32_t value = 0u;
                if (tryConstantOperand(sgprs, inst.source0, value)) {
                    auto& dst = sgprs[scalarSlot(inst.destination)];
                    dst = SgprValue{};
                    dst.kind = SgprValueKind::Constant;
                    dst.constant = value;
                } else {
                    clearScalarRange(sgprs, inst.destination, 1u);
                }
            }
            break;
        case RdnaOpcode::SMovreldB32:
        case RdnaOpcode::SMovreldB64:
        case RdnaOpcode::SMovrelsd2B32:
            sgprs.fill(SgprValue{});
            break;
        case RdnaOpcode::SMovrelsB32:
        case RdnaOpcode::SMovrelsB64:
            if (isScalarOperand(inst.destination)) {
                clearScalarRange(sgprs, inst.destination, decodedDstSize(inst));
            }
            break;
        case RdnaOpcode::SMovkI32:
            if (isScalarOperand(inst.destination)) {
                auto& dst = sgprs[scalarSlot(inst.destination)];
                dst = SgprValue{};
                dst.kind = SgprValueKind::Constant;
                dst.constant = inst.source0.value;
            }
            break;
        case RdnaOpcode::SCmovkI32:
            if (isScalarOperand(inst.destination)) {
                clearScalarRange(sgprs, inst.destination, 1u);
            }
            break;
        default:
            if (isScalarLoad(inst.op)) {
                if (isScalarOperand(inst.source0) && scalarSlot(inst.source0) < sgprs.size() && sgprs[scalarSlot(inst.source0)].kind == SgprValueKind::AttributeTable) {
                    std::uint32_t rawOffset = 0u;
                    if (trySmemOffset(sgprs, inst, rawOffset)) {
                        const auto slot = scalarSlot(inst.destination);
                        const std::int32_t index = static_cast<std::int32_t>(rawOffset / 4u);
                        for (std::uint32_t i = 0u; i < decodedDstSize(inst) && slot + i < sgprs.size(); i++) {
                            auto& dst = sgprs[slot + i];
                            dst = SgprValue{};
                            dst.kind = SgprValueKind::Attribute;
                            dst.attributeId = index + static_cast<std::int32_t>(i);
                            dst.loadPcs.push_back(inst.programCounter);
                        }
                    } else {
                        clearScalarRange(sgprs, inst.destination, decodedDstSize(inst));
                    }
                } else if (isScalarOperand(inst.source0) && scalarSlot(inst.source0) < sgprs.size() && sgprs[scalarSlot(inst.source0)].kind == SgprValueKind::BufferTable) {
                    const auto slot = scalarSlot(inst.destination);
                    std::uint32_t rawOffset = 0u;
                    if (trySmemOffset(sgprs, inst, rawOffset)) {
                        for (std::uint32_t i = 0u; i < decodedDstSize(inst) && slot + i < sgprs.size(); i++) {
                            auto& dst = sgprs[slot + i];
                            dst = SgprValue{};
                            dst.kind = SgprValueKind::Buffer;
                            dst.attributeId = bufferAttributeFromOffset(rawOffset, i);
                            dst.loadPcs.push_back(inst.programCounter);
                        }
                    } else if (isScalarOperand(inst.source1) && scalarSlot(inst.source1) < sgprs.size() && sgprs[scalarSlot(inst.source1)].kind == SgprValueKind::Attribute && (inst.memoryOffset & 0x3u) == 0u) {
                        for (std::uint32_t i = 0u; i < decodedDstSize(inst) && slot + i < sgprs.size(); i++) {
                            auto& dst = sgprs[slot + i];
                            dst = SgprValue{};
                            dst.kind = SgprValueKind::Buffer;
                            dst.attributeId = sgprs[scalarSlot(inst.source1)].attributeId;
                            dst.loadPcs = sgprs[scalarSlot(inst.source1)].loadPcs;
                            dst.loadPcs.push_back(inst.programCounter);
                        }
                    } else {
                        clearScalarRange(sgprs, inst.destination, decodedDstSize(inst));
                    }
                } else {
                    clearScalarRange(sgprs, inst.destination, decodedDstSize(inst));
                }
            } else if (inst.op == RdnaOpcode::VCndmaskB32) {
                if (isVectorOperand(inst.destination) && inst.destination.reg < vgprIsIndex.size()) {
                    clearVectorLanes(vectorLanes, inst.destination.reg);
                }
                if (isVectorOperand(inst.destination) && inst.destination.reg < vgprIsIndex.size() && isVectorOperand(inst.source0) && inst.source0.reg == kInstanceIndexVgpr && isVectorOperand(inst.source1) && inst.source1.reg == kVertexIndexVgpr) {
                    vgprIsIndex[inst.destination.reg] = true;
                }
            } else if (isAttributePropagationAlu(inst.op)) {
                if (isScalarOperand(inst.destination) && isScalarOperand(inst.source0) && scalarSlot(inst.source0) < sgprs.size() && sgprs[scalarSlot(inst.source0)].kind == SgprValueKind::Attribute) {
                    sgprs[scalarSlot(inst.destination)] = sgprs[scalarSlot(inst.source0)];
                } else if (isScalarOperand(inst.destination)) {
                    std::uint32_t src0 = 0u;
                    std::uint32_t src1 = 0u;
                    if (tryConstantOperand(sgprs, inst.source0, src0) && tryConstantOperand(sgprs, inst.source1, src1)) {
                        auto& dst = sgprs[scalarSlot(inst.destination)];
                        dst = SgprValue{};
                        dst.kind = SgprValueKind::Constant;
                        switch (inst.op) {
                        case RdnaOpcode::SAndB32:
                            dst.constant = src0 & src1;
                            break;
                        case RdnaOpcode::SLshlB32:
                            dst.constant = src0 << (src1 & 31u);
                            break;
                        case RdnaOpcode::SBfeU32:
                            dst.constant = src0 >> (src1 & 31u);
                            break;
                        default:
                            dst.constant = src0 + src1;
                            break;
                        }
                    } else {
                        clearScalarRange(sgprs, inst.destination, 1u);
                    }
                }
            } else if (isFormattedBufferLoad(inst.op)) {
                if (isVectorOperand(inst.source0) && inst.source0.reg < vgprIsIndex.size() && vgprIsIndex[inst.source0.reg] && isScalarOperand(inst.source1) && scalarSlot(inst.source1) < sgprs.size() && sgprs[scalarSlot(inst.source1)].kind == SgprValueKind::Buffer) {
                    const auto& buffer = sgprs[scalarSlot(inst.source1)];
                    if (plan.loads.empty()) {
                        if (!vertexOffsetConflict) {
                            plan.vertexOffsetSgpr = vertexOffsetCandidate;
                        }
                        if (!instanceOffsetConflict) {
                            plan.instanceOffsetSgpr = instanceOffsetCandidate;
                        }
                    }
                    auto& load = plan.loads.emplace_back();
                    load.programCounter = inst.programCounter;
                    load.attributeId = buffer.attributeId;
                    load.componentCount = decodedDstSize(inst);
                    load.prologLoads = buffer.loadPcs;
                }
            }
            break;
        }
        if (inst.op == RdnaOpcode::VMovreldB32) {
            vectorLanes.clear();
        } else if (inst.op != RdnaOpcode::VWritelaneB32 && isVectorOperand(inst.destination)) {
            for (std::uint32_t i = 0u; i < embeddedFetchDstSize(inst) && inst.destination.reg + i < vgprIsIndex.size(); i++) {
                clearVectorLanes(vectorLanes, inst.destination.reg + i);
            }
        }
    }

    plan.vertexOffsetConflict = vertexOffsetConflict;
    plan.instanceOffsetConflict = instanceOffsetConflict;
    const auto shared = [&](std::int32_t sgpr, const std::vector<std::uint32_t>& addPcs) {
        if (sgpr < 0) return false;
        for (const auto& inst : program.instructions) {
            if (std::find(addPcs.begin(), addPcs.end(), inst.programCounter) != addPcs.end()) continue;
            if (touchesSgpr(inst, static_cast<std::uint32_t>(sgpr))) return true;
        }
        return false;
    };
    plan.vertexOffsetShared = shared(plan.vertexOffsetSgpr, vertexAddPcs);
    plan.instanceOffsetShared = shared(plan.instanceOffsetSgpr, instanceAddPcs);

    return plan;
}

}
