#include "Translation/InstructionTranslator.hpp"
#include "Recompiler.hpp"
#include "Translation/DispatchInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

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
    if (options.embeddedFetch != nullptr && options.stage != ShaderStageKind::Vertex && options.stage != ShaderStageKind::Local) throw std::runtime_error("embedded vertex fetch requires a vertex or local shader");
    if (options.userDataBaseRegister >= NumScalarRegs || options.userDataCount > NumScalarRegs - options.userDataBaseRegister) {
        throw std::runtime_error("shader user data exceeds the scalar register bank");
    }
    if (options.waveSize != 32u && options.waveSize != 64u) {
        throw std::runtime_error("shader translation requires wave32 or wave64, got " + std::to_string(options.waveSize));
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

bool isUnlinkedCall(const ControlFlowGraph& cfg, std::uint32_t programCounter) {
    return std::find(cfg.unlinkedCallProgramCounters.begin(), cfg.unlinkedCallProgramCounters.end(), programCounter) != cfg.unlinkedCallProgramCounters.end();
}

bool isCodeTableLoad(const ControlFlowGraph& cfg, std::uint32_t programCounter) {
    return std::find(cfg.codeTableLoadProgramCounters.begin(), cfg.codeTableLoadProgramCounters.end(), programCounter) != cfg.codeTableLoadProgramCounters.end();
}

template<typename TAction>
void forEachExternalFetchLoad(const RdnaInstruction& call, const ShaderVertexInputInfo* input, TAction action) {
    if (input == nullptr || input->resourcesNum <= 0) {
        throw std::runtime_error("s_swappc_b64 fetch-shader call at program counter " + std::to_string(call.programCounter) + " has no parsed fetch-shader plan to inline");
    }
    for (int index = 0; index < input->resourcesNum && index < ShaderVertexInputInfo::MaxResources; ++index) {
        const auto& destination = input->resourcesDst[index];
        if (destination.attrId < 0 || destination.registersNum <= 0) {
            continue;
        }
        if (destination.registersNum > 4 || destination.registerStart < 0) {
            throw std::runtime_error("external fetch-shader attribute " + std::to_string(destination.attrId) + " writes " + std::to_string(destination.registersNum) +
                " registers starting at " + std::to_string(destination.registerStart) + "; the inline model supports one to four");
        }
        RdnaInstruction fetch{};
        fetch.op = RdnaOpcode::BufferLoadFormatXyzw;
        fetch.programCounter = call.programCounter;
        fetch.destination.kind = RdnaOperandKind::VectorRegister;
        fetch.destination.reg = static_cast<std::uint32_t>(destination.registerStart);
        fetch.dataDwordCount = static_cast<std::uint32_t>(destination.registersNum);
        fetch.formatted = true;
        action(static_cast<std::uint32_t>(index), fetch);
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
    if (instruction.family == RdnaInstructionFamily::FLAT) {
        const bool compare = instruction.op == RdnaOpcode::FlatAtomicCmpswap || instruction.op == RdnaOpcode::FlatAtomicCmpswapX2 || instruction.op == RdnaOpcode::FlatAtomicFcmpswap || instruction.op == RdnaOpcode::FlatAtomicFcmpswapX2;
        includeVector(instruction.source2, std::max(instruction.dataDwordCount, 1u) * (compare ? 2u : 1u));
    }
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
        case RdnaOpcode::DsWrite2st64B64:
        case RdnaOpcode::DsWrxchg2RtnB64:
        case RdnaOpcode::DsWrxchg2st64RtnB64: {
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

    const auto setVector = [&entryIr, &program](VectorReg reg, IrValue& value) {
        auto& metadata = program.Metadata();
        const auto index = RegIndex(reg);
        if (metadata.vectorWindowSize != 0u && index >= metadata.vectorWindowBase) {
            metadata.vectorWindowSize = std::max(metadata.vectorWindowSize, index - metadata.vectorWindowBase + 1u);
        }
        entryIr.SetVectorReg(reg, value);
    };

    const auto builtin = [&entryIr](StageInputKind kind, std::uint32_t component = 0u) -> IrValue& {
        return entryIr.Emit(IrOpcode::GetBuiltin, IrOpcodeType(IrOpcode::GetBuiltin), {&entryIr.Constant(static_cast<std::uint32_t>(kind)), &entryIr.Constant(component)});
    };
    const auto drawIndex = [&entryIr, &options, &builtin](StageInputKind kind) -> IrValue& {
        const auto* fetch = options.embeddedFetch;
        const std::int32_t folded = fetch == nullptr ? -1 : kind == StageInputKind::VertexIndex ? fetch->vertexOffsetSgpr : fetch->instanceOffsetSgpr;
        IrValue& index = builtin(kind);
        return folded < 0 ? index : entryIr.ISub(index, entryIr.GetUserData(static_cast<ScalarReg>(folded)));
    };
    const auto tessellationBase = [&entryIr](TessellationBaseKind kind) -> IrValue& {
        return entryIr.Emit(IrOpcode::TessellationBase, IrOpcodeType(IrOpcode::TessellationBase), {&entryIr.Constant(static_cast<std::uint32_t>(kind))});
    };
    constexpr std::uint32_t MergedHullRecordBytes = 16u;
    const auto mergedHullRecord = [&entryIr](IrValue& address) -> IrValue& {
        return entryIr.Emit(IrOpcode::GetTessellationAttribute, IrOpcodeType(IrOpcode::GetTessellationAttribute), {&entryIr.Constant(static_cast<std::uint32_t>(TessellationAttribute::ControlInput)), &address, &entryIr.ConstantBool(true)});
    };

    for (std::uint32_t index = 0; index < options.userDataCount; index++) {
        const auto reg = static_cast<ScalarReg>(options.userDataBaseRegister + index);
        IrValue& value = entryIr.GetUserData(reg);
        entryIr.SetScalarReg(reg, value);
        entryIr.SetScalarMaskTag(reg, entryIr.ConstantBool(false));
    }

    IrValue* initialExec = &entryIr.ConstantBool(true);
    if (options.stage == ShaderStageKind::Pixel) {
        initialExec = &entryIr.IEqual(builtin(StageInputKind::HelperInvocation), entryIr.Constant(0u));
    }
    std::uint32_t totalThreads = 0;
    const auto* workgroup = shaderWorkgroupInput(options.stage, options.inputInfo);
    if (workgroup != nullptr) {
        totalThreads = std::max(workgroup->threadsNum[0], 1u) * std::max(workgroup->threadsNum[1], 1u) * std::max(workgroup->threadsNum[2], 1u);
        if (options.waveSize == 64u && workgroup->hostSubgroupSize == 32u && totalThreads % 64u != 0u) {
            initialExec = &entryIr.ULessThan(builtin(StageInputKind::LocalInvocationIndex), entryIr.Constant(totalThreads));
        }
    }
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
            setVector(static_cast<VectorReg>(index), builtin(StageInputKind::LocalInvocationId, index));
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
            IrValue& waveBits = entryIr.BitwiseOr(entryIr.ShiftLeftLogical(waveId, entryIr.Constant(6u)), entryIr.ShiftLeftLogical(waveId, entryIr.Constant(20u)));
            IrValue& firstBit = entryIr.Select(entryIr.IEqual(waveId, entryIr.Constant(0u)), entryIr.Constant(0x80000000u), entryIr.Constant(0u));
            entryIr.SetScalarReg(static_cast<ScalarReg>(cs->workgroupRegister + regOffset), entryIr.BitwiseOr(entryIr.BitwiseOr(waveBits, entryIr.Constant(waves)), firstBit));
        }
    } else if (options.stage == ShaderStageKind::Mesh) {
        const auto& mesh = options.inputInfo.vertex->mesh;
        const std::uint32_t size = mesh.InputPrimitiveSize();
        const std::uint32_t stepCount = mesh.InputPrimitiveStep();
        if (mesh.primitivesPerGroup == 0u || mesh.verticesPerGroup != mesh.InputVertexCount(mesh.primitivesPerGroup) || mesh.verticesPerGroup > totalThreads || mesh.primitivesPerGroup > totalThreads || totalThreads % options.waveSize != 0u || totalThreads / options.waveSize > 15u || mesh.esgsItemSize == 0u || mesh.esgsItemSize * mesh.verticesPerGroup > 0xffffu) {
            throw std::runtime_error("mesh shader translation configuration is not supported (wave " + std::to_string(options.waveSize) + ", primitives per group " + std::to_string(mesh.primitivesPerGroup) + ", vertices per group " + std::to_string(mesh.verticesPerGroup) + ", threads " + std::to_string(totalThreads) + ", ESGS item size " + std::to_string(mesh.esgsItemSize) + ")");
        }
        constexpr std::uint32_t kTriFanPrimitiveType = 5u;
        constexpr std::uint32_t kTriStripPrimitiveType = 6u;
        const bool fan = mesh.inputPrimitive == kTriFanPrimitiveType;
        const auto u32 = [&entryIr](std::uint32_t value) -> IrValue& {
            return entryIr.Constant(value);
        };
        const auto draw = [&entryIr](std::uint32_t index) -> IrValue& {
            return entryIr.Emit(IrOpcode::MeshDrawParameter, IrOpcodeType(IrOpcode::MeshDrawParameter), {&entryIr.Constant(index)});
        };
        const auto argument = [&entryIr](std::uint32_t index) -> IrValue& {
            return entryIr.Emit(IrOpcode::MeshArgument, IrOpcodeType(IrOpcode::MeshArgument), {&entryIr.Constant(index)});
        };
        const auto minimum = [&entryIr](IrValue& lhs, IrValue& rhs) -> IrValue& {
            return entryIr.Emit(IrOpcode::UMin32, IrOpcodeType(IrOpcode::UMin32), {&lhs, &rhs});
        };
        const auto subtractSaturate = [&entryIr, &minimum](IrValue& lhs, IrValue& rhs) -> IrValue& {
            return entryIr.ISub(lhs, minimum(lhs, rhs));
        };
        IrValue& local = builtin(StageInputKind::LocalInvocationIndex);
        IrValue& firstPrimitive = entryIr.IMul(builtin(StageInputKind::WorkgroupId, 0u), u32(mesh.primitivesPerGroup));
        IrValue& step = u32(stepCount);
        IrValue& firstVertex = entryIr.IMul(firstPrimitive, step);
        IrValue& indirect = entryIr.INotEqual(entryIr.BitwiseOr(draw(MeshArgumentAddressDword), draw(MeshArgumentAddressDword + 1u)), u32(0u));
        IrValue& indexCount = entryIr.Select(indirect, argument(MeshArgumentIndexCountDword), draw(0u));
        IrValue& firstIndex = argument(MeshArgumentFirstIndexDword);
        IrValue& vertices = minimum(subtractSaturate(indexCount, firstVertex), u32(mesh.verticesPerGroup));
        IrValue& primitives = entryIr.Select(entryIr.ULessThan(vertices, u32(size)), u32(0u), entryIr.IAdd(entryIr.Emit(IrOpcode::UDiv32, IrOpcodeType(IrOpcode::UDiv32), {&subtractSaturate(vertices, u32(size)), &step}), u32(1u)));
        entryIr.SetScalarReg(static_cast<ScalarReg>(2), entryIr.BitwiseOr(entryIr.ShiftLeftLogical(vertices, u32(12u)), entryIr.ShiftLeftLogical(primitives, u32(22u))));
        IrValue& wave = entryIr.ShiftRightLogical(local, u32(options.waveSize == 32u ? 5u : 6u));
        IrValue& waveBase = entryIr.BitwiseAnd(local, u32(~(options.waveSize - 1u)));
        IrValue& vertexCount = minimum(subtractSaturate(vertices, waveBase), u32(options.waveSize));
        IrValue& primitiveCount = minimum(subtractSaturate(primitives, waveBase), u32(options.waveSize));
        IrValue& waveInfo = entryIr.BitwiseOr(entryIr.ShiftLeftLogical(wave, u32(24u)), u32((totalThreads / options.waveSize) << 28u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.BitwiseOr(waveInfo, entryIr.BitwiseOr(entryIr.ShiftLeftLogical(primitiveCount, u32(8u)), vertexCount)));
        const bool strip = mesh.inputPrimitive == kTriStripPrimitiveType;
        IrValue& indexWord = draw(3u);
        IrValue& stripPosition = entryIr.IAdd(entryIr.IAdd(firstPrimitive, local), firstIndex);
        IrValue& restartTable = entryIr.INotEqual(entryIr.BitwiseAnd(indexWord, u32(MeshIndexRestartTable)), u32(0u));
        IrValue& stripStart = strip ? entryIr.Emit(IrOpcode::MeshRestartStart, IrOpcodeType(IrOpcode::MeshRestartStart), {&entryIr.IAdd(stripPosition, u32(2u)), &restartTable}) : u32(0u);
        IrValue& parity = strip ? entryIr.BitwiseAnd(entryIr.ISub(stripPosition, entryIr.Emit(IrOpcode::UMax32, IrOpcodeType(IrOpcode::UMax32), {&stripStart, &firstIndex})), u32(1u)) : u32(0u);
        IrValue& vertex = entryIr.IMul(local, step);
        IrValue& item = u32(mesh.esgsItemSize);
        const auto unlessRestarted = [&](IrValue& offset) -> IrValue& {
            return strip ? entryIr.Select(entryIr.ULessThan(stripPosition, stripStart), entryIr.IMul(vertex, item), offset) : offset;
        };
        IrValue& first = fan ? entryIr.IMul(entryIr.IAdd(vertex, u32(1u)), item) : unlessRestarted(entryIr.IMul(entryIr.IAdd(vertex, parity), item));
        IrValue& second = fan ? entryIr.IMul(entryIr.IAdd(vertex, u32(2u)), item) : size >= 2u ? unlessRestarted(entryIr.IMul(entryIr.ISub(entryIr.IAdd(vertex, u32(1u)), parity), item)) : u32(0u);
        IrValue& third = fan ? u32(0u) : size == 3u ? unlessRestarted(entryIr.IMul(entryIr.IAdd(vertex, u32(2u)), item)) : u32(0u);
        setVector(static_cast<VectorReg>(0), entryIr.BitwiseOr(entryIr.BitwiseAnd(first, u32(0xffffu)), entryIr.ShiftLeftLogical(second, u32(16u))));
        setVector(static_cast<VectorReg>(1), entryIr.BitwiseAnd(third, u32(0xffffu)));
        setVector(static_cast<VectorReg>(2), entryIr.IAdd(firstPrimitive, local));
        setVector(static_cast<VectorReg>(3), u32(0u));
        setVector(static_cast<VectorReg>(4), u32(0u));
        if (options.userDataBaseRegister != 0u || options.userDataCount < 8u) {
            throw std::runtime_error("mesh shader translation requires the merged program's eight hidden user words");
        }
        IrValue& inputVertex = fan ? entryIr.Select(entryIr.IEqual(local, u32(0u)), u32(0u), entryIr.IAdd(firstVertex, local)) : entryIr.IAdd(firstVertex, local);
        IrValue& indexBytes = entryIr.BitwiseAnd(indexWord, u32(0xffu));
        IrValue& indexed = entryIr.INotEqual(indexBytes, u32(0u));
        IrValue& byteOffset = entryIr.IMul(entryIr.IAdd(inputVertex, firstIndex), indexBytes);
        IrValue& indexResource = entryIr.Emit(IrOpcode::GetBufferResource, IrOpcodeType(IrOpcode::GetBufferResource), {&entryIr.GetUserData(static_cast<ScalarReg>(4)), &entryIr.GetUserData(static_cast<ScalarReg>(5)), &entryIr.GetUserData(static_cast<ScalarReg>(6)), &entryIr.GetUserData(static_cast<ScalarReg>(7))});
        const std::uint32_t memoryIndex = static_cast<std::uint32_t>(program.Resources().memoryInfo.size());
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Buffer, .resource = 1u, .offen = true});
        IrValue& packedIndex = entryIr.Emit(IrOpcode::LoadBufferU32, IrOpcodeType(IrOpcode::LoadBufferU32), {&indexResource, &u32(0u), &entryIr.BitwiseAnd(byteOffset, u32(~3u)), &u32(0u), &entryIr.LogicalAnd(indexed, entryIr.ULessThan(local, vertices))}, MemoryFlags{.index = memoryIndex});
        IrValue& index = entryIr.Emit(IrOpcode::BitFieldUExtract, IrOpcodeType(IrOpcode::BitFieldUExtract), {&packedIndex, &entryIr.IMul(entryIr.BitwiseAnd(byteOffset, u32(3u)), u32(8u)), &entryIr.IMul(indexBytes, u32(8u))});
        for (std::uint32_t reg = 4u; reg < 8u; reg++) {
            entryIr.SetScalarReg(static_cast<ScalarReg>(reg), u32(0u));
        }
        IrValue& restartIndex = entryIr.Select(entryIr.IEqual(indexBytes, u32(2u)), u32(0xffffu), u32(0xffffffffu));
        IrValue& fetchedIndex = entryIr.Select(entryIr.LogicalAnd(restartTable, entryIr.IEqual(index, restartIndex)), u32(0u), index);
        setVector(static_cast<VectorReg>(5), entryIr.IAdd(draw(1u), entryIr.Select(indexed, fetchedIndex, inputVertex)));
        setVector(static_cast<VectorReg>(6), u32(0u));
        setVector(static_cast<VectorReg>(7), u32(0u));
        setVector(static_cast<VectorReg>(8), entryIr.IAdd(draw(2u), builtin(StageInputKind::WorkgroupId, 1u)));
    } else if (options.stage == ShaderStageKind::Local) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        setVector(static_cast<VectorReg>(2), drawIndex(StageInputKind::VertexIndex));
        setVector(static_cast<VectorReg>(3), tessellationBase(TessellationBaseKind::RelativeIndex));
        setVector(static_cast<VectorReg>(5), drawIndex(StageInputKind::InstanceIndex));
        if (options.inputInfo.vertex->tess.mergedHull) {
            const auto kind = static_cast<std::uint32_t>(TessellationAttribute::LocalOutput);
            for (const auto [offset, index] : {std::pair{0u, StageInputKind::VertexIndex}, std::pair{4u, StageInputKind::InstanceIndex}}) {
                static_cast<void>(entryIr.Emit(IrOpcode::SetTessellationAttribute, IrType::Void, {&entryIr.Constant(kind), &entryIr.Constant(offset), &builtin(index), &entryIr.ConstantBool(true)}));
            }
            program.Metadata().tessellationLocalStride = MergedHullRecordBytes;
        }
    } else if (options.stage == ShaderStageKind::TessellationControl) {
        const auto& tess = options.inputInfo.vertex->tess;
        entryIr.SetScalarReg(static_cast<ScalarReg>(2), tessellationBase(TessellationBaseKind::OffChip));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), tessellationBase(TessellationBaseKind::Factor));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(0x81010000u | tess.inputControlPoints | (tess.outputControlPoints << 8u)));
        setVector(static_cast<VectorReg>(0), builtin(StageInputKind::PrimitiveId));
        setVector(static_cast<VectorReg>(1), entryIr.BitwiseOr(entryIr.ShiftLeftLogical(builtin(StageInputKind::InvocationId), entryIr.Constant(8u)), tessellationBase(TessellationBaseKind::RelativeIndex)));
        if (tess.mergedHull) {
            IrValue& point = entryIr.Emit(IrOpcode::UMin32, IrOpcodeType(IrOpcode::UMin32), {&builtin(StageInputKind::InvocationId), &entryIr.Constant(tess.inputControlPoints - 1u)});
            IrValue& record = entryIr.IMul(point, entryIr.Constant(MergedHullRecordBytes));
            setVector(static_cast<VectorReg>(2), mergedHullRecord(record));
            setVector(static_cast<VectorReg>(3), entryIr.IAdd(entryIr.IMul(tessellationBase(TessellationBaseKind::RelativeIndex), entryIr.Constant(tess.inputControlPoints)), builtin(StageInputKind::InvocationId)));
            setVector(static_cast<VectorReg>(5), mergedHullRecord(entryIr.IAdd(record, entryIr.Constant(4u))));
            program.Metadata().tessellationLocalStride = MergedHullRecordBytes;
        }
    } else if (options.stage == ShaderStageKind::TessellationEvaluation) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), tessellationBase(TessellationBaseKind::OffChip));
        setVector(static_cast<VectorReg>(0), tessellationBase(TessellationBaseKind::PassthroughPrimitive));
        setVector(static_cast<VectorReg>(5), builtin(StageInputKind::TessCoord, 0u));
        setVector(static_cast<VectorReg>(6), builtin(StageInputKind::TessCoord, 1u));
        setVector(static_cast<VectorReg>(7), tessellationBase(TessellationBaseKind::RelativeIndex));
        setVector(static_cast<VectorReg>(8), builtin(StageInputKind::PrimitiveId));
    } else if (options.stage == ShaderStageKind::Pixel) {
        const auto* ps = options.inputInfo.pixel;
        const auto vgpr = [&](PixelInput input) { return ps->psInputVgpr[static_cast<std::uint32_t>(input)]; };
        const auto loaded = [&](PixelInput input) { return vgpr(input) != ShaderPixelInputInfo::NoPixelInputVgpr; };
        const bool iterated = ps->psIteratesSamples;
        program.Metadata().pixelIteratesSamples = iterated;
        const bool offsets = options.interpolationFunctions && options.fragmentShaderBarycentricEnabled;
        const bool centers = iterated && offsets;
        if (ps->mergeSamples > 1u && !offsets && ps->SampleLocationsOffCenter() && (loaded(PixelInput::PerspectiveCenter) || loaded(PixelInput::LinearCenter))) {
            throw std::runtime_error("a partial write of a multisampled packed color target runs the pixel shader once per sample, and without interpolation functions and fragment shader barycentrics its centre-located barycentrics would follow the sample");
        }
        const bool sampleOffsets = offsets && ps->SampleLocationsOffCenter();
        const auto smoothSample = sampleOffsets ? StageInputKind::BaryCoordSmoothSample : StageInputKind::BaryCoordSmooth;
        const auto linearSample = sampleOffsets ? StageInputKind::BaryCoordNoPerspectiveSample : StageInputKind::BaryCoordNoPerspective;
        const auto smoothCenter = centers ? StageInputKind::BaryCoordSmoothCenter : StageInputKind::BaryCoordSmooth;
        const auto linearCenter = centers ? StageInputKind::BaryCoordNoPerspectiveCenter : StageInputKind::BaryCoordNoPerspective;
        for (const auto [input, kind] : {std::pair{PixelInput::PerspectiveSample, smoothSample}, std::pair{PixelInput::PerspectiveCenter, smoothCenter},
                                         std::pair{PixelInput::PerspectiveCentroid, StageInputKind::BaryCoordSmooth}, std::pair{PixelInput::LinearSample, linearSample},
                                         std::pair{PixelInput::LinearCenter, linearCenter}, std::pair{PixelInput::LinearCentroid, StageInputKind::BaryCoordNoPerspective}}) {
            if (!loaded(input)) continue;
            setVector(static_cast<VectorReg>(vgpr(input)), builtin(kind, 0u));
            setVector(static_cast<VectorReg>(vgpr(input) + 1u), builtin(kind, 1u));
        }
        const auto firstSampleOffset = [&](std::uint32_t component) -> IrValue& {
            const auto& locations = ps->sampleLocations;
            if (!ps->SampleLocationsVary()) return entryIr.ConstantF32(SampleLocationOffset(locations[0], component));
            IrValue& x = entryIr.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 0u))});
            IrValue& y = entryIr.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 1u))});
            IrValue& quadPixel = entryIr.BitwiseOr(entryIr.BitwiseAnd(x, entryIr.Constant(1u)), entryIr.ShiftLeftLogical(entryIr.BitwiseAnd(y, entryIr.Constant(1u)), entryIr.Constant(1u)));
            IrValue* offset = &entryIr.ConstantF32(SampleLocationOffset(locations[24], component));
            for (std::uint32_t pixel = 3u; pixel-- > 0u;) {
                offset = &entryIr.Select(entryIr.IEqual(quadPixel, entryIr.Constant(pixel)), entryIr.ConstantF32(SampleLocationOffset(locations[pixel * 8u], component)), *offset);
            }
            return *offset;
        };
        const auto position = [&](std::uint32_t component) -> IrValue& {
            IrValue& coordinate = builtin(StageInputKind::FragCoord, component);
            const auto location = ps->psPositionLocation;
            if (location == PixelPositionLocation::Centroid || (location == PixelPositionLocation::Center && !iterated) || (location == PixelPositionLocation::Sample && (iterated || !ps->SampleLocationsOffCenter()))) {
                return coordinate;
            }
            IrValue& value = entryIr.BitCastF32(coordinate);
            if (location == PixelPositionLocation::Sample) {
                return entryIr.BitCastU32(entryIr.Emit(IrOpcode::FPAdd32, IrOpcodeType(IrOpcode::FPAdd32), {&value, &firstSampleOffset(component)}));
            }
            IrValue& corner = entryIr.Emit(IrOpcode::FPFloor32, IrOpcodeType(IrOpcode::FPFloor32), {&value});
            if (location == PixelPositionLocation::UpperLeft) return entryIr.BitCastU32(corner);
            return entryIr.BitCastU32(entryIr.Emit(IrOpcode::FPAdd32, IrOpcodeType(IrOpcode::FPAdd32), {&corner, &entryIr.ConstantF32(0.5f)}));
        };
        if (loaded(PixelInput::PositionX)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::PositionX)), position(0u));
        }
        if (loaded(PixelInput::PositionY)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::PositionY)), position(1u));
        }
        if (loaded(PixelInput::PositionZ)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::PositionZ)), builtin(StageInputKind::FragCoord, 2u));
        }
        if (loaded(PixelInput::PositionW)) {
            IrValue& reciprocalBits = builtin(StageInputKind::FragCoord, 3u);
            IrValue& flushed = entryIr.Select(entryIr.IEqual(entryIr.BitwiseAnd(reciprocalBits, entryIr.Constant(0x7f800000u)), entryIr.Constant(0u)), entryIr.BitwiseAnd(reciprocalBits, entryIr.Constant(0x80000000u)), reciprocalBits);
            IrValue& w = entryIr.Emit(IrOpcode::FPRecip32, IrOpcodeType(IrOpcode::FPRecip32), {&entryIr.BitCastF32(flushed)});
            setVector(static_cast<VectorReg>(vgpr(PixelInput::PositionW)), entryIr.BitCastU32(w));
        }
        if (loaded(PixelInput::FrontFace)) {
            // The front value under FRONT_FACE_ALL_BITS is not measured (docs/dev/debt/shader.md).
            IrValue& facing = builtin(StageInputKind::FrontFacing);
            setVector(static_cast<VectorReg>(vgpr(PixelInput::FrontFace)), ps->psFrontFaceAllBits ? entryIr.Select(entryIr.IEqual(facing, entryIr.Constant(0x3f800000u)), entryIr.Constant(0xffffffffu), entryIr.Constant(0u)) : facing);
        }
        if (loaded(PixelInput::Ancillary)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::Ancillary)), builtin(StageInputKind::PackedAncillary));
        }
        if (loaded(PixelInput::SampleCoverage)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::SampleCoverage)), builtin(StageInputKind::SampleMask));
        }
        if (loaded(PixelInput::LineStipple)) {
            setVector(static_cast<VectorReg>(vgpr(PixelInput::LineStipple)), entryIr.Constant(0u));
        }
        if (loaded(PixelInput::PositionFixedPoint)) {
            IrValue& x = entryIr.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 0u))});
            IrValue& y = entryIr.Emit(IrOpcode::ConvertU32F32, IrType::U32, {&entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 1u))});
            setVector(static_cast<VectorReg>(vgpr(PixelInput::PositionFixedPoint)), entryIr.BitwiseOr(entryIr.BitwiseAnd(x, entryIr.Constant(0xffffu)), entryIr.ShiftLeftLogical(y, entryIr.Constant(16u))));
        }
    } else if (options.stage == ShaderStageKind::Vertex) {
        if (options.userDataBaseRegister >= 8u) {
            const auto hostLanes = options.inputInfo.vertex->hostSubgroupSize;
            const std::uint32_t lanes = hostLanes == 1u ? 1u : options.waveSize == 64u && hostLanes == 32u ? 32u : options.waveSize;
            entryIr.SetScalarReg(static_cast<ScalarReg>(2), entryIr.Constant((lanes << 12u) | (lanes << 22u)));
            entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(lanes | (lanes << 8u)));
        }
        if (options.gcnLegacyVertexInputs) {
            setVector(static_cast<VectorReg>(0), drawIndex(StageInputKind::VertexIndex));
            for (std::uint32_t reg = 1u; reg <= 3u; reg++) setVector(static_cast<VectorReg>(reg), drawIndex(StageInputKind::InstanceIndex));
        } else {
            setVector(static_cast<VectorReg>(5), drawIndex(StageInputKind::VertexIndex));
            setVector(static_cast<VectorReg>(8), drawIndex(StageInputKind::InstanceIndex));
        }
    }
}

}

IrProgram InstructionTranslator::Translate(const RdnaProgram& decoded, const ControlFlowGraph& cfg, const TranslateOptions& options) const {
    validateTranslateOptions(options);
    if (cfg.linkEscapeWaveSize == options.waveSize) {
        throw std::invalid_argument(cfg.linkEscapeReason);
    }
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
            if (cfg.hasFetchCall && instruction.programCounter == cfg.fetchCallProgramCounter) {
                forEachExternalFetchLoad(instruction, options.inputInfo.vertex, [&](std::uint32_t, const RdnaInstruction& fetch) {
                    vectorLimit = std::min(NumVectorRegs, std::max(vectorLimit, fetch.destination.reg + fetch.dataDwordCount));
                });
            }
        }
    }

    IrProgram program;
    program.SetWaveSize(options.waveSize);
    std::uint32_t windowBase = NumVectorRegs;
    bool indexedWrites = false;
    for (const auto& cfgBlock : cfg.blocks) {
        for (std::uint32_t index = cfgBlock.instructionBegin; index < cfgBlock.instructionEnd; index++) {
            const auto& instruction = decoded.instructions[index];
            if (isCodeTableLoad(cfg, instruction.programCounter)) continue;
            switch (instruction.op) {
                case RdnaOpcode::VMovrelsB32:
                    windowBase = std::min(windowBase, instruction.source0.reg);
                    break;
                case RdnaOpcode::VMovreldB32:
                    windowBase = std::min(windowBase, instruction.destination.reg);
                    indexedWrites = true;
                    break;
                case RdnaOpcode::VMovrelsdB32:
                case RdnaOpcode::VMovrelsd2B32:
                case RdnaOpcode::VSwaprelB32:
                    windowBase = std::min({windowBase, instruction.source0.reg, instruction.destination.reg});
                    indexedWrites = true;
                    break;
                default:
                    break;
            }
        }
    }
    const bool ancillaryWindow = options.stage == ShaderStageKind::Pixel && options.inputInfo.pixel != nullptr && options.inputInfo.pixel->psAncillary && options.inputInfo.pixel->psInputVgpr[static_cast<std::uint32_t>(PixelInput::Ancillary)] >= windowBase;
    if (windowBase < vectorLimit && !ancillaryWindow) {
        program.Metadata().vectorWindowBase = windowBase;
        program.Metadata().vectorWindowSize = vectorLimit - windowBase;
        program.Metadata().vectorWindowIndexedWrites = indexedWrites;
    }
    program.Resources().stage = toIrShaderStage(options.stage);
    program.Resources().shaderHash = options.shaderHash;
    program.Resources().userDataBase = options.userDataBaseRegister;
    program.Resources().userDataCount = options.userDataCount;
    program.Info().scratchDwords = options.scratchDwords;
    program.Info().sharedMemoryBytes = options.sharedMemoryBytes;
    if (options.embeddedFetch != nullptr) {
        program.Info().vertexOffsetSgpr = options.embeddedFetch->vertexOffsetSgpr;
        program.Info().instanceOffsetSgpr = options.embeddedFetch->instanceOffsetSgpr;
        program.Info().vertexOffsetShared = options.embeddedFetch->vertexOffsetShared || options.embeddedFetch->vertexIndexObserved;
        program.Info().instanceOffsetShared = options.embeddedFetch->instanceOffsetShared || options.embeddedFetch->instanceIndexObserved;
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
    if (program.Metadata().vectorWindowSize != 0u) {
        vectorLimit = program.Metadata().vectorWindowBase + program.Metadata().vectorWindowSize;
    }
    if (ancillaryWindow) {
        for (const IrValue* instruction : blocks.front()->Instructions()) {
            if (instruction->Opcode() == IrOpcode::SetVectorRegister) {
                vectorLimit = std::max(vectorLimit, instruction->Argument(0)->Register().index + 1u);
            }
        }
    }

    const auto entryRounding = ComputeEntryRounding(cfg, decoded, options.floatMode.has_value() ? options.floatMode->floatMode : 0u);
    for (const auto& cfgBlock : cfg.blocks) {
        const auto typedIndex = blockIndices.at(cfgBlock.id);
        TranslationContext context(program, *blocks[typedIndex], vectorLimit);
        context.SetPixelInput(options.inputInfo.pixel, options.fragmentShaderBarycentricEnabled);
        context.SetFloatMode(options.floatMode);
        context.SetNativeFloat16(options.nativeFloat16);
        context.SetFloat32DenormPreserve(options.float32DenormPreserve);
        context.SetFloat32FlushToZero(options.float32FlushToZero);
        context.SetEntryRounding(entryRounding.at(cfgBlock.id));
        for (std::uint32_t index = cfgBlock.instructionBegin; index < cfgBlock.instructionEnd; index++) {
            const auto& instruction = decoded.instructions[index];
            if (isCodeTableLoad(cfg, instruction.programCounter)) {
                const auto table = std::find_if(cfg.codeTableLoads.begin(), cfg.codeTableLoads.end(), [&](const auto& entry) { return entry.programCounter == instruction.programCounter; });
                if (table == cfg.codeTableLoads.end()) throw std::runtime_error("missing shader code table values");
                context.TranslateCodeTableLoad(instruction, *table);
                continue;
            }
            if (isUnlinkedCall(cfg, instruction.programCounter)) {
                context.TranslateUnlinkedCall(instruction);
                continue;
            }
            if (options.embeddedFetch != nullptr) {
                const auto load = std::ranges::find(options.embeddedFetch->loads, instruction.programCounter, &EmbeddedFetchLoad::programCounter);
                if (load != options.embeddedFetch->loads.end()) {
                    if (options.inputInfo.vertex == nullptr || instruction.destination.kind != RdnaOperandKind::VectorRegister || instruction.dataDwordCount != load->componentCount) throw std::runtime_error("prepared vertex fetch does not match its instruction");
                    const auto& vertex = *options.inputInfo.vertex;
                    std::uint32_t attribute = 0;
                    while (attribute < static_cast<std::uint32_t>(vertex.resourcesNum) && vertex.resourcesDst[attribute].attrId != load->attributeId) ++attribute;
                    if (attribute == static_cast<std::uint32_t>(vertex.resourcesNum)) throw std::runtime_error("prepared vertex fetch has no matching semantic");
                    context.TranslateEmbeddedFetch(instruction, attribute, load->componentCount);
                    continue;
                }
            }
            context.TranslateInstruction(instruction);
            if (cfg.hasFetchCall && instruction.programCounter == cfg.fetchCallProgramCounter) {
                forEachExternalFetchLoad(instruction, options.inputInfo.vertex, [&](std::uint32_t attribute, const RdnaInstruction& fetch) {
                    context.TranslateEmbeddedFetch(fetch, attribute, fetch.dataDwordCount);
                });
            }
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
