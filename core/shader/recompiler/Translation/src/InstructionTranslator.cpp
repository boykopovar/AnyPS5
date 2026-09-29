#include "Translation/InstructionTranslator.hpp"
#include "Translation/DispatchInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ShaderRecompiler {

namespace {

IrShaderStage toIrShaderStage(ShaderStageKind stage) {
    switch (stage) {
    case ShaderStageKind::Unknown:
        return IrShaderStage::Unknown;
    case ShaderStageKind::Vertex:
        return IrShaderStage::Vertex;
    case ShaderStageKind::Pixel:
        return IrShaderStage::Pixel;
    case ShaderStageKind::Fetch:
        return IrShaderStage::Fetch;
    case ShaderStageKind::Compute:
        return IrShaderStage::Compute;
    case ShaderStageKind::Mesh:
        return IrShaderStage::Mesh;
    case ShaderStageKind::Local:
        return IrShaderStage::Local;
    case ShaderStageKind::TessellationControl:
        return IrShaderStage::TessellationControl;
    case ShaderStageKind::TessellationEvaluation:
        return IrShaderStage::TessellationEvaluation;
    }
    throw std::runtime_error("InstructionTranslator::Translate unknown shader stage kind");
}

void validateTranslateOptions(const TranslateOptions& options) {
    if (options.userDataBaseRegister >= NumScalarRegs || options.userDataCount > NumScalarRegs - options.userDataBaseRegister) {
        throw std::runtime_error("shader user data exceeds the scalar register bank");
    }
    if (options.waveSize != 32u && options.waveSize != 64u) {
        throw std::runtime_error("shader translation requires wave32 or wave64, got " + std::to_string(options.waveSize));
    }
    if (options.embeddedFetch != nullptr && options.stage != ShaderStageKind::Vertex && options.stage != ShaderStageKind::Local) {
        throw std::runtime_error("embedded vertex fetch requires a vertex or local shader");
    }
    switch (options.stage) {
    case ShaderStageKind::Vertex:
    case ShaderStageKind::Local:
    case ShaderStageKind::TessellationControl:
    case ShaderStageKind::TessellationEvaluation:
    case ShaderStageKind::Mesh:
        if (options.inputInfo.vertex == nullptr) {
            throw std::runtime_error("vertex shader translation has no vertex input metadata");
        }
        return;
    case ShaderStageKind::Pixel:
        if (options.inputInfo.pixel == nullptr) {
            throw std::runtime_error("pixel shader translation has no pixel input metadata");
        }
        return;
    case ShaderStageKind::Compute:
        if (options.inputInfo.compute == nullptr) {
            throw std::runtime_error("compute shader translation has no compute input metadata");
        }
        return;
    case ShaderStageKind::Unknown:
    case ShaderStageKind::Fetch:
        break;
    }
    throw std::runtime_error(
        "shader translation has an unsupported stage: options.stage=" +
        std::to_string(static_cast<int>(options.stage))
    );
}

const ShaderWorkgroupInputInfo* shaderWorkgroupInput(ShaderStageKind stage, const ShaderStageInputInfo& inputInfo) {
    switch (stage) {
    case ShaderStageKind::Compute:
        return inputInfo.compute;
    case ShaderStageKind::Mesh:
        return inputInfo.vertex != nullptr ? &inputInfo.vertex->mesh : nullptr;
    default:
        return nullptr;
    }
}

bool isCodeTableLoad(const ControlFlowGraph& cfg, std::uint32_t programCounter) {
    return std::find(cfg.codeTableLoadProgramCounters.begin(), cfg.codeTableLoadProgramCounters.end(), programCounter) != cfg.codeTableLoadProgramCounters.end();
}

const EmbeddedFetchLoad* findEmbeddedFetchLoad(const EmbeddedFetchPlan* plan, std::uint32_t programCounter) {
    if (plan == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(plan->loads.begin(), plan->loads.end(), [programCounter](const EmbeddedFetchLoad& load) {
        return load.programCounter == programCounter;
    });
    return found != plan->loads.end() ? &*found : nullptr;
}

int resolveEmbeddedFetchResource(const ShaderVertexInputInfo& input, const EmbeddedFetchLoad& load) {
    if (load.attributeId >= 0 && load.attributeId < input.resourcesNum && input.resourcesDst[load.attributeId].attrId == load.attributeId) {
        return load.attributeId;
    }
    for (int index = 0; index < input.resourcesNum; index++) {
        const auto& destination = input.resourcesDst[index];
        if (destination.attrId == load.attributeId && load.componentCount <= static_cast<std::uint32_t>(std::max(destination.registersNum, 1))) {
            return index;
        }
    }
    for (int index = 0; index < input.resourcesNum; index++) {
        if (input.resourcesDst[index].attrId == load.attributeId) {
            return index;
        }
    }
    return -1;
}

bool isBufferDwordLoad(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::BufferLoadFormatX:
    case RdnaOpcode::BufferLoadFormatXy:
    case RdnaOpcode::BufferLoadFormatXyz:
    case RdnaOpcode::BufferLoadFormatXyzw:
    case RdnaOpcode::BufferLoadDword:
    case RdnaOpcode::BufferLoadDwordx2:
    case RdnaOpcode::BufferLoadDwordx3:
    case RdnaOpcode::BufferLoadDwordx4:
    case RdnaOpcode::TbufferLoadFormatX:
    case RdnaOpcode::TbufferLoadFormatXy:
    case RdnaOpcode::TbufferLoadFormatXyz:
    case RdnaOpcode::TbufferLoadFormatXyzw:
        return true;
    default:
        return false;
    }
}

void includeInstructionVectorRegisters(const RdnaInstruction& instruction, std::uint32_t& vectorLimit) {
    const auto includeVector = [&vectorLimit](const RdnaOperand& operand, std::uint32_t count = 1u) {
        if (operand.kind == RdnaOperandKind::VectorRegister) {
            vectorLimit = std::min(NumVectorRegs, std::max(vectorLimit, operand.reg + count));
        }
    };
    const bool memoryFamily = instruction.family == RdnaInstructionFamily::MUBUF || instruction.family == RdnaInstructionFamily::MTBUF || instruction.family == RdnaInstructionFamily::FLAT || instruction.family == RdnaInstructionFamily::DS || instruction.family == RdnaInstructionFamily::MIMG;
    includeVector(instruction.destination, memoryFamily ? std::max(instruction.dataDwordCount, 1u) : 1u);
    includeVector(instruction.destination2);
    includeVector(instruction.source0);
    includeVector(instruction.source1);
    includeVector(instruction.source2);
    includeVector(instruction.source3);
    if (instruction.family == RdnaInstructionFamily::DS) {
        switch (instruction.op) {
        case RdnaOpcode::DsWriteB64:
        case RdnaOpcode::DsWriteB96:
        case RdnaOpcode::DsWriteB128:
            includeVector(instruction.source1, instruction.dataDwordCount);
            break;
        case RdnaOpcode::DsWrite2B32:
        case RdnaOpcode::DsWrite2st64B32:
        case RdnaOpcode::DsWrite2B64:
        case RdnaOpcode::DsWrite2st64B64: {
            const std::uint32_t width = std::max(instruction.dataDwordCount / 2u, 1u);
            includeVector(instruction.source1, width);
            includeVector(instruction.source2, width);
            break;
        }
        default:
            break;
        }
    }
    for (std::uint32_t index = 0; index + 1u < instruction.imageAddressComponents && index < MaxRdnaImageNsaAddressComponents; index++) {
        vectorLimit = std::min(NumVectorRegs, std::max(vectorLimit, instruction.imageNsaVectorRegisters[index] + 1u));
    }
}

void emitEntryPrologue(IrProgram& program, IrBlock& entryBlock, const TranslateOptions& options) {
    IrBuilder entryIr(program);
    entryIr.SetInsertionPoint(entryBlock);

    const auto builtin = [&entryIr](StageInputKind kind, std::uint32_t component = 0u) -> IrValue& {
        return entryIr.Emit(IrOpcode::GetBuiltin, IrOpcodeType(IrOpcode::GetBuiltin), {&entryIr.Constant(static_cast<std::uint32_t>(kind)), &entryIr.Constant(component)});
    };

    for (std::uint32_t index = 0; index < options.userDataCount; index++) {
        const auto reg = static_cast<ScalarReg>(options.userDataBaseRegister + index);
        IrValue& value = entryIr.GetUserData(reg);
        entryIr.SetScalarReg(reg, value);
        entryIr.SetScalarMaskTag(reg, entryIr.ConstantBool(false));
    }

    IrValue* initialExec = &entryIr.ConstantBool(true);
    std::uint32_t totalThreads = 0;
    const auto* workgroup = shaderWorkgroupInput(options.stage, options.inputInfo);
    if (workgroup != nullptr) {
        totalThreads = std::max(workgroup->threadsNum[0], 1u) * std::max(workgroup->threadsNum[1], 1u) * std::max(workgroup->threadsNum[2], 1u);
        if (options.waveSize == 64u && workgroup->hostSubgroupSize == 32u && totalThreads % 64u != 0u) {
            initialExec = &entryIr.ULessThan(builtin(StageInputKind::LocalInvocationIndex), entryIr.Constant(totalThreads));
        }
    }
    // A dispatch sized in threads launches a partial last workgroup; the host rounds it up to
    // whole groups, so threads past the dispatch size start inactive.
    if (options.stage == ShaderStageKind::Compute && options.inputInfo.compute->partialGroups) {
        for (std::uint32_t axis = 0; axis < 3u; axis++) {
            initialExec = &entryIr.LogicalAnd(*initialExec, entryIr.ULessThan(builtin(StageInputKind::GlobalInvocationId, axis), builtin(StageInputKind::DispatchThreadLimit, axis)));
        }
    }
    entryIr.SetExec(*initialExec);
    IrValue& initialMask = entryIr.Emit(IrOpcode::Ballot, IrOpcodeType(IrOpcode::Ballot), {initialExec});
    entryIr.SetExecLo(entryIr.CompositeExtract(initialMask, 0u));
    entryIr.SetExecHi(options.waveSize == 64u ? entryIr.CompositeExtract(initialMask, 1u) : entryIr.Constant(0u));

    if (options.stage == ShaderStageKind::Compute) {
        const auto* cs = options.inputInfo.compute;
        const std::uint32_t threadIds = cs->threadIdsNum > 0 ? std::min<std::uint32_t>(static_cast<std::uint32_t>(cs->threadIdsNum), 3u) : 0u;
        for (std::uint32_t index = 0; index < threadIds; index++) {
            entryIr.SetVectorReg(static_cast<VectorReg>(index), builtin(StageInputKind::LocalInvocationId, index));
        }
        std::uint32_t regOffset = 0;
        for (std::uint32_t index = 0; index < 3u; index++) {
            if (cs->groupId[index]) {
                entryIr.SetScalarReg(static_cast<ScalarReg>(cs->workgroupRegister + regOffset++), builtin(StageInputKind::WorkgroupId, index));
            }
        }
        if (cs->tgSizeEn) {
            const std::uint32_t waveSize = cs->waveSize != 0u ? cs->waveSize : 64u;
            const std::uint32_t waves = std::min((totalThreads + waveSize - 1u) / waveSize, 0x3fu);
            IrValue& localIndex = builtin(StageInputKind::LocalInvocationIndex);
            IrValue& waveId = entryIr.Emit(IrOpcode::UDiv32, IrOpcodeType(IrOpcode::UDiv32), {&localIndex, &entryIr.Constant(waveSize)});
            IrValue& waveBits = entryIr.ShiftLeftLogical(waveId, entryIr.Constant(20u));
            IrValue& firstBit = entryIr.Select(entryIr.IEqual(waveId, entryIr.Constant(0u)), entryIr.Constant(0x80000000u), entryIr.Constant(0u));
            entryIr.SetScalarReg(static_cast<ScalarReg>(cs->workgroupRegister + regOffset), entryIr.BitwiseOr(entryIr.BitwiseOr(waveBits, entryIr.Constant(waves)), firstBit));
        }
    } else if (options.stage == ShaderStageKind::Mesh) {
        const auto& mesh = options.inputInfo.vertex->mesh;
        if (options.waveSize != 64u || mesh.primitivesPerGroup == 0u || mesh.verticesPerGroup > 64u || totalThreads > 15u * 64u) {
            throw std::runtime_error("mesh shader translation configuration is not supported");
        }
        constexpr std::uint32_t kTriStripPrimitiveType = 6u;
        const auto u32 = [&entryIr](std::uint32_t value) -> IrValue& {
            return entryIr.Constant(value);
        };
        const auto draw = [&entryIr](std::uint32_t index) -> IrValue& {
            return entryIr.Emit(IrOpcode::MeshDrawParameter, IrOpcodeType(IrOpcode::MeshDrawParameter), {&entryIr.Constant(index)});
        };
        const auto minimum = [&entryIr](IrValue& lhs, IrValue& rhs) -> IrValue& {
            return entryIr.Emit(IrOpcode::UMin32, IrOpcodeType(IrOpcode::UMin32), {&lhs, &rhs});
        };
        const auto subtractSaturate = [&entryIr, &minimum](IrValue& lhs, IrValue& rhs) -> IrValue& {
            return entryIr.ISub(lhs, minimum(lhs, rhs));
        };
        IrValue& local = builtin(StageInputKind::LocalInvocationIndex);
        IrValue& primitiveChunk = entryIr.IMul(builtin(StageInputKind::WorkgroupId, 0u), u32(mesh.primitivesPerGroup));
        IrValue& step = u32(mesh.InputPrimitiveStep());
        IrValue& size = u32(mesh.InputPrimitiveSize());
        IrValue& chunk = entryIr.IMul(primitiveChunk, step);
        IrValue& vertices = minimum(subtractSaturate(draw(0u), chunk), u32(mesh.verticesPerGroup));
        IrValue& primitives = entryIr.Select(entryIr.ULessThan(vertices, size), u32(0u), entryIr.IAdd(entryIr.Emit(IrOpcode::UDiv32, IrOpcodeType(IrOpcode::UDiv32), {&subtractSaturate(vertices, size), &step}), u32(1u)));
        IrValue& wave = entryIr.ShiftRightLogical(local, u32(6u));
        IrValue& waveBase = entryIr.BitwiseAnd(local, u32(~63u));
        IrValue& vertexCount = minimum(subtractSaturate(vertices, waveBase), u32(64u));
        IrValue& primitiveCount = minimum(subtractSaturate(primitives, waveBase), u32(64u));
        IrValue& waveInfo = entryIr.BitwiseOr(entryIr.ShiftLeftLogical(wave, u32(24u)), u32(((totalThreads + 63u) / 64u) << 28u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.BitwiseOr(waveInfo, entryIr.BitwiseOr(entryIr.ShiftLeftLogical(primitiveCount, u32(8u)), vertexCount)));
        IrValue& parity = mesh.inputPrimitive == kTriStripPrimitiveType ? entryIr.BitwiseAnd(entryIr.IAdd(primitiveChunk, local), u32(1u)) : u32(0u);
        IrValue& vertex = entryIr.IMul(local, step);
        IrValue& first = entryIr.IAdd(vertex, parity);
        IrValue& second = mesh.InputPrimitiveSize() >= 2u ? entryIr.ISub(entryIr.IAdd(vertex, u32(1u)), parity) : u32(0u);
        IrValue& third = mesh.InputPrimitiveSize() == 3u ? entryIr.IAdd(vertex, u32(2u)) : u32(0u);
        entryIr.SetVectorReg(static_cast<VectorReg>(0), entryIr.BitwiseOr(entryIr.ShiftLeftLogical(first, u32(2u)), entryIr.ShiftLeftLogical(second, u32(18u))));
        entryIr.SetVectorReg(static_cast<VectorReg>(1), entryIr.ShiftLeftLogical(third, u32(2u)));
        IrValue& inputVertex = entryIr.IAdd(chunk, local);
        IrValue& indexBytes = draw(3u);
        IrValue& indexed = entryIr.INotEqual(indexBytes, u32(0u));
        IrValue& indexLow = draw(4u);
        IrValue& byteOffset = entryIr.IAdd(entryIr.BitwiseAnd(indexLow, u32(3u)), entryIr.IMul(inputVertex, indexBytes));
        IrValue& indexResource = entryIr.Emit(IrOpcode::GetAddressResource, IrOpcodeType(IrOpcode::GetAddressResource), {&entryIr.BitwiseAnd(indexLow, u32(~3u)), &draw(5u)});
        const std::uint32_t memoryIndex = static_cast<std::uint32_t>(program.Resources().memoryInfo.size());
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Global});
        IrValue& packedIndex = entryIr.Emit(IrOpcode::LoadAddressU32, IrOpcodeType(IrOpcode::LoadAddressU32), {&indexResource, &entryIr.BitwiseAnd(byteOffset, u32(~3u)), &u32(0u), &entryIr.LogicalAnd(indexed, entryIr.ULessThan(local, vertices))}, MemoryFlags{.index = memoryIndex});
        IrValue& index = entryIr.Emit(IrOpcode::BitFieldUExtract, IrOpcodeType(IrOpcode::BitFieldUExtract), {&packedIndex, &entryIr.IMul(entryIr.BitwiseAnd(byteOffset, u32(3u)), u32(8u)), &entryIr.IMul(indexBytes, u32(8u))});
        entryIr.SetVectorReg(static_cast<VectorReg>(5), entryIr.IAdd(draw(1u), entryIr.Select(indexed, index, inputVertex)));
        entryIr.SetVectorReg(static_cast<VectorReg>(8), entryIr.IAdd(draw(2u), builtin(StageInputKind::WorkgroupId, 1u)));
    } else if (options.stage == ShaderStageKind::Local) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        entryIr.SetVectorReg(static_cast<VectorReg>(2), builtin(StageInputKind::VertexIndex));
        entryIr.SetVectorReg(static_cast<VectorReg>(3), entryIr.Constant(0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(5), builtin(StageInputKind::InstanceIndex));
    } else if (options.stage == ShaderStageKind::TessellationControl) {
        const auto& tess = options.inputInfo.vertex->tess;
        entryIr.SetScalarReg(static_cast<ScalarReg>(2), entryIr.Emit(IrOpcode::TessellationBase, IrOpcodeType(IrOpcode::TessellationBase), {&entryIr.Constant(0u)}));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), entryIr.Emit(IrOpcode::TessellationBase, IrOpcodeType(IrOpcode::TessellationBase), {&entryIr.Constant(1u)}));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(0x81010000u | tess.inputControlPoints | (tess.outputControlPoints << 8u)));
        entryIr.SetVectorReg(static_cast<VectorReg>(0), builtin(StageInputKind::PrimitiveId));
        entryIr.SetVectorReg(static_cast<VectorReg>(1), entryIr.ShiftLeftLogical(builtin(StageInputKind::InvocationId), entryIr.Constant(8u)));
    } else if (options.stage == ShaderStageKind::TessellationEvaluation) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), entryIr.Emit(IrOpcode::TessellationBase, IrOpcodeType(IrOpcode::TessellationBase), {&entryIr.Constant(0u)}));
        entryIr.SetVectorReg(static_cast<VectorReg>(5), builtin(StageInputKind::TessCoord, 0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(6), builtin(StageInputKind::TessCoord, 1u));
        entryIr.SetVectorReg(static_cast<VectorReg>(7), entryIr.Constant(0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(8), builtin(StageInputKind::PrimitiveId));
    } else if (options.stage == ShaderStageKind::Pixel) {
        const auto* ps = options.inputInfo.pixel;
        if (options.fragmentShaderBarycentricEnabled && ps->psPerspectiveCenterVgpr != std::numeric_limits<std::uint32_t>::max()) {
            entryIr.SetVectorReg(static_cast<VectorReg>(ps->psPerspectiveCenterVgpr), builtin(StageInputKind::BaryCoordSmooth, 0u));
            entryIr.SetVectorReg(static_cast<VectorReg>(ps->psPerspectiveCenterVgpr + 1u), builtin(StageInputKind::BaryCoordSmooth, 1u));
        }
        std::uint32_t reg = ps->psSystemInputBase;
        if (ps->psPosX) {
            entryIr.SetVectorReg(static_cast<VectorReg>(reg++), builtin(StageInputKind::FragCoord, 0u));
        }
        if (ps->psPosY) {
            entryIr.SetVectorReg(static_cast<VectorReg>(reg++), builtin(StageInputKind::FragCoord, 1u));
        }
        if (ps->psPosZ) {
            entryIr.SetVectorReg(static_cast<VectorReg>(reg++), builtin(StageInputKind::FragCoord, 2u));
        }
        if (ps->psPosW) {
            IrValue& reciprocalW = entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 3u));
            IrValue& w = entryIr.Emit(IrOpcode::FPRecip32, IrOpcodeType(IrOpcode::FPRecip32), {&reciprocalW});
            entryIr.SetVectorReg(static_cast<VectorReg>(reg++), entryIr.BitCastU32(w));
        }
        if (ps->psFrontFace) {
            entryIr.SetVectorReg(static_cast<VectorReg>(reg++), builtin(StageInputKind::FrontFacing));
        }
        if (ps->psAncillary) {
            entryIr.SetVectorReg(static_cast<VectorReg>(reg), builtin(StageInputKind::PackedAncillary));
        }
    } else if (options.stage == ShaderStageKind::Vertex) {
        entryIr.SetVectorReg(static_cast<VectorReg>(5), builtin(StageInputKind::VertexIndex));
        entryIr.SetVectorReg(static_cast<VectorReg>(8), builtin(StageInputKind::InstanceIndex));
    }
}

}

IrProgram InstructionTranslator::Translate(const RdnaProgram& decoded, const ControlFlowGraph& cfg, const TranslateOptions& options) const {
    validateTranslateOptions(options);
    if (cfg.blocks.empty()) {
        throw std::runtime_error("cannot translate an empty control flow graph");
    }

    std::uint32_t vectorLimit = 1u;
    for (const auto& cfgBlock : cfg.blocks) {
        for (std::uint32_t index = cfgBlock.instructionBegin; index < cfgBlock.instructionEnd; index++) {
            if (index >= decoded.instructions.size()) {
                throw std::runtime_error("control flow graph block " + std::to_string(cfgBlock.id) + " references instruction " + std::to_string(index) + " outside decoded program of size " + std::to_string(decoded.instructions.size()));
            }
            const auto& instruction = decoded.instructions[index];
            if (isCodeTableLoad(cfg, instruction.programCounter)) {
                continue;
            }
            includeInstructionVectorRegisters(instruction, vectorLimit);
        }
    }

    IrProgram program;
    program.SetWaveSize(options.waveSize);
    program.Resources().stage = toIrShaderStage(options.stage);
    program.Resources().shaderHash = options.shaderHash;
    program.Resources().userDataBase = options.userDataBaseRegister;
    program.Resources().userDataCount = options.userDataCount;
    program.Info().scratchDwords = options.scratchDwords;
    if (options.embeddedFetch != nullptr) {
        program.Info().vertexOffsetSgpr = options.embeddedFetch->vertexOffsetSgpr;
        program.Info().instanceOffsetSgpr = options.embeddedFetch->instanceOffsetSgpr;
        program.Info().vertexOffsetShared = options.embeddedFetch->vertexOffsetShared;
        program.Info().instanceOffsetShared = options.embeddedFetch->instanceOffsetShared;
        program.Info().vertexOffsetConflict = options.embeddedFetch->vertexOffsetConflict;
        program.Info().instanceOffsetConflict = options.embeddedFetch->instanceOffsetConflict;
    }
    program.Metadata().cfgFailureKind = cfg.failureKind;
    program.Metadata().failureReason = cfg.unsupportedReason;

    const auto maxIdIterator = std::max_element(cfg.blocks.begin(), cfg.blocks.end(), [](const BasicBlock& lhs, const BasicBlock& rhs) {
        return lhs.id < rhs.id;
    });
    if (maxIdIterator == cfg.blocks.end() || maxIdIterator->id == InvalidControlFlowId) {
        throw std::runtime_error("cannot allocate a synthetic entry block id");
    }
    const std::uint32_t entryBlockId = maxIdIterator->id + 1u;

    std::vector<IrBlock*> blocks;
    std::vector<BlockInfo> blockInfos;
    blocks.reserve(cfg.blocks.size() + 1u);
    blockInfos.reserve(cfg.blocks.size() + 1u);

    Terminator entryTerminator;
    entryTerminator.kind = TerminatorKind::Branch;
    entryTerminator.trueBlock = cfg.blocks.front().id;
    blocks.push_back(&program.CreateBlock());
    blockInfos.push_back(BlockInfo{entryBlockId, cfg.blocks.front().startProgramCounter, cfg.blocks.front().startProgramCounter, entryTerminator});

    std::unordered_map<std::uint32_t, std::size_t> blockIndices;
    blockIndices.reserve(cfg.blocks.size());
    for (const auto& sourceBlock : cfg.blocks) {
        if (!blockIndices.emplace(sourceBlock.id, blocks.size()).second) {
            throw std::runtime_error("control flow graph contains duplicate block id " + std::to_string(sourceBlock.id));
        }
        blocks.push_back(&program.CreateBlock());
        blockInfos.push_back(BlockInfo{sourceBlock.id, sourceBlock.startProgramCounter, sourceBlock.endProgramCounter, sourceBlock.terminator});
    }

    for (const auto& sourceBlock : cfg.blocks) {
        const auto sourceIndex = blockIndices.at(sourceBlock.id);
        for (const auto successor : sourceBlock.successors) {
            const auto target = blockIndices.find(successor);
            if (target == blockIndices.end()) {
                throw std::runtime_error("control flow graph block " + std::to_string(sourceBlock.id) + " has unknown successor " + std::to_string(successor));
            }
            blocks[sourceIndex]->AddBranch(blocks[target->second]);
        }
    }
    blocks.front()->AddBranch(blocks[blockIndices.at(cfg.blocks.front().id)]);

    emitEntryPrologue(program, *blocks.front(), options);

    for (const auto& cfgBlock : cfg.blocks) {
        const auto typedIndex = blockIndices.at(cfgBlock.id);
        TranslationContext context(program, *blocks[typedIndex], vectorLimit);
        for (std::uint32_t index = cfgBlock.instructionBegin; index < cfgBlock.instructionEnd; index++) {
            const auto& instruction = decoded.instructions[index];
            if (isCodeTableLoad(cfg, instruction.programCounter)) {
                continue;
            }
            const auto* embedded = findEmbeddedFetchLoad(options.embeddedFetch, instruction.programCounter);
            if (embedded != nullptr && isBufferDwordLoad(instruction.op) && instruction.dataDwordCount == embedded->componentCount && instruction.destination.kind == RdnaOperandKind::VectorRegister) {
                if (options.inputInfo.vertex == nullptr) {
                    throw std::runtime_error("embedded vertex fetch requires vertex input metadata");
                }
                const auto resource = resolveEmbeddedFetchResource(*options.inputInfo.vertex, *embedded);
                if (resource < 0 || resource >= options.inputInfo.vertex->resourcesNum) {
                    throw std::runtime_error("embedded vertex fetch at program counter " + std::to_string(instruction.programCounter) + " has no resource for attribute " + std::to_string(embedded->attributeId));
                }
                context.TranslateEmbeddedFetch(instruction, static_cast<std::uint32_t>(resource), embedded->componentCount, options.inputInfo.vertex->resources[resource]);
                continue;
            }
            context.TranslateInstruction(instruction);
        }
        context.AddBranchCondition(cfgBlock, blockInfos[typedIndex]);
    }

    program.Metadata().blockInfo = std::move(blockInfos);
    program.BlockOrder() = blocks;

    ValidateProgram(program, false);
    return program;
}

void InstructionTranslator::translateInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg, const TranslateOptions& options) const {
    DispatchInstruction(builder, instruction, cfg, options);
}

TranslationContext::TranslationContext(IrProgram& program, IrBlock& block, std::uint32_t vectorLimit) : program(program), ir(program), block(block), instructionBranchCondition(IrU1(program.CreateValue(IrOpcode::Void, IrType::Bool))), currentVectorLimit(vectorLimit) {
    ir.SetInsertionPoint(block);
}

}
