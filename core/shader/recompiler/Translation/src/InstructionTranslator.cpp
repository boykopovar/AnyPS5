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

bool isCodeTableLoad(const ControlFlowGraph& cfg, std::uint32_t programCounter) {
    return std::find(cfg.codeTableLoadProgramCounters.begin(), cfg.codeTableLoadProgramCounters.end(), programCounter) != cfg.codeTableLoadProgramCounters.end();
}

void translateExternalFetch(TranslationContext& context, const RdnaInstruction& call, const ShaderVertexInputInfo* input) {
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
        fetch.formatted = true;
        context.TranslateEmbeddedFetch(fetch, static_cast<std::uint32_t>(index), static_cast<std::uint32_t>(destination.registersNum));
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

    const auto builtin = [&entryIr](StageInputKind kind, std::uint32_t component = 0u) -> IrValue& {
        return entryIr.Emit(IrOpcode::GetBuiltin, IrOpcodeType(IrOpcode::GetBuiltin), {&entryIr.Constant(static_cast<std::uint32_t>(kind)), &entryIr.Constant(component)});
    };
    const auto tessellationBase = [&entryIr](TessellationBaseKind kind) -> IrValue& {
        return entryIr.Emit(IrOpcode::TessellationBase, IrOpcodeType(IrOpcode::TessellationBase), {&entryIr.Constant(static_cast<std::uint32_t>(kind))});
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
        const std::uint32_t size = mesh.InputPrimitiveSize();
        const std::uint32_t stepCount = mesh.InputPrimitiveStep();
        if (options.waveSize != 64u || mesh.primitivesPerGroup == 0u || mesh.verticesPerGroup != mesh.InputVertexCount(mesh.primitivesPerGroup) || mesh.verticesPerGroup > totalThreads || mesh.primitivesPerGroup > totalThreads || totalThreads % 64u != 0u || totalThreads > 15u * 64u || mesh.esgsItemSize == 0u || mesh.esgsItemSize * mesh.verticesPerGroup > 0xffffu) {
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
        IrValue& wave = entryIr.ShiftRightLogical(local, u32(6u));
        IrValue& waveBase = entryIr.BitwiseAnd(local, u32(~63u));
        IrValue& vertexCount = minimum(subtractSaturate(vertices, waveBase), u32(64u));
        IrValue& primitiveCount = minimum(subtractSaturate(primitives, waveBase), u32(64u));
        IrValue& waveInfo = entryIr.BitwiseOr(entryIr.ShiftLeftLogical(wave, u32(24u)), u32((totalThreads / 64u) << 28u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.BitwiseOr(waveInfo, entryIr.BitwiseOr(entryIr.ShiftLeftLogical(primitiveCount, u32(8u)), vertexCount)));
        IrValue& parity = mesh.inputPrimitive == kTriStripPrimitiveType ? entryIr.BitwiseAnd(entryIr.IAdd(firstPrimitive, local), u32(1u)) : u32(0u);
        IrValue& vertex = entryIr.IMul(local, step);
        IrValue& item = u32(mesh.esgsItemSize);
        IrValue& first = fan ? entryIr.IMul(entryIr.IAdd(vertex, u32(1u)), item) : entryIr.IMul(entryIr.IAdd(vertex, parity), item);
        IrValue& second = fan ? entryIr.IMul(entryIr.IAdd(vertex, u32(2u)), item) : size >= 2u ? entryIr.IMul(entryIr.ISub(entryIr.IAdd(vertex, u32(1u)), parity), item) : u32(0u);
        IrValue& third = fan ? u32(0u) : size == 3u ? entryIr.IMul(entryIr.IAdd(vertex, u32(2u)), item) : u32(0u);
        entryIr.SetVectorReg(static_cast<VectorReg>(0), entryIr.BitwiseOr(entryIr.BitwiseAnd(first, u32(0xffffu)), entryIr.ShiftLeftLogical(second, u32(16u))));
        entryIr.SetVectorReg(static_cast<VectorReg>(1), entryIr.BitwiseAnd(third, u32(0xffffu)));
        entryIr.SetVectorReg(static_cast<VectorReg>(2), entryIr.IAdd(firstPrimitive, local));
        entryIr.SetVectorReg(static_cast<VectorReg>(3), u32(0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(4), u32(0u));
        if (options.userDataBaseRegister != 0u || options.userDataCount < 8u) {
            throw std::runtime_error("mesh shader translation requires the merged program's eight hidden user words");
        }
        IrValue& inputVertex = fan ? entryIr.Select(entryIr.IEqual(local, u32(0u)), u32(0u), entryIr.IAdd(firstVertex, local)) : entryIr.IAdd(firstVertex, local);
        IrValue& indexBytes = draw(3u);
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
        entryIr.SetVectorReg(static_cast<VectorReg>(5), entryIr.IAdd(draw(1u), entryIr.Select(indexed, index, inputVertex)));
        entryIr.SetVectorReg(static_cast<VectorReg>(6), u32(0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(7), u32(0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(8), entryIr.IAdd(draw(2u), builtin(StageInputKind::WorkgroupId, 1u)));
    } else if (options.stage == ShaderStageKind::Local) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        entryIr.SetVectorReg(static_cast<VectorReg>(2), builtin(StageInputKind::VertexIndex));
        entryIr.SetVectorReg(static_cast<VectorReg>(3), tessellationBase(TessellationBaseKind::RelativeIndex));
        entryIr.SetVectorReg(static_cast<VectorReg>(5), builtin(StageInputKind::InstanceIndex));
    } else if (options.stage == ShaderStageKind::TessellationControl) {
        const auto& tess = options.inputInfo.vertex->tess;
        entryIr.SetScalarReg(static_cast<ScalarReg>(2), tessellationBase(TessellationBaseKind::OffChip));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), tessellationBase(TessellationBaseKind::Factor));
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(0x81010000u | tess.inputControlPoints | (tess.outputControlPoints << 8u)));
        entryIr.SetVectorReg(static_cast<VectorReg>(0), builtin(StageInputKind::PrimitiveId));
        entryIr.SetVectorReg(static_cast<VectorReg>(1), entryIr.BitwiseOr(entryIr.ShiftLeftLogical(builtin(StageInputKind::InvocationId), entryIr.Constant(8u)), tessellationBase(TessellationBaseKind::RelativeIndex)));
    } else if (options.stage == ShaderStageKind::TessellationEvaluation) {
        entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(64u));
        entryIr.SetScalarReg(static_cast<ScalarReg>(4), tessellationBase(TessellationBaseKind::OffChip));
        entryIr.SetVectorReg(static_cast<VectorReg>(0), tessellationBase(TessellationBaseKind::PassthroughPrimitive));
        entryIr.SetVectorReg(static_cast<VectorReg>(5), builtin(StageInputKind::TessCoord, 0u));
        entryIr.SetVectorReg(static_cast<VectorReg>(6), builtin(StageInputKind::TessCoord, 1u));
        entryIr.SetVectorReg(static_cast<VectorReg>(7), tessellationBase(TessellationBaseKind::RelativeIndex));
        entryIr.SetVectorReg(static_cast<VectorReg>(8), builtin(StageInputKind::PrimitiveId));
    } else if (options.stage == ShaderStageKind::Pixel) {
        const auto* ps = options.inputInfo.pixel;
        const auto vgpr = [&](PixelInput input) { return ps->psInputVgpr[static_cast<std::uint32_t>(input)]; };
        const auto loaded = [&](PixelInput input) { return vgpr(input) != ShaderPixelInputInfo::NoPixelInputVgpr; };
        if (options.fragmentShaderBarycentricEnabled) {
            for (const auto [input, kind] : {std::pair{PixelInput::PerspectiveCenter, StageInputKind::BaryCoordSmooth}, std::pair{PixelInput::PerspectiveCentroid, StageInputKind::BaryCoordSmooth},
                                             std::pair{PixelInput::LinearCenter, StageInputKind::BaryCoordNoPerspective}, std::pair{PixelInput::LinearCentroid, StageInputKind::BaryCoordNoPerspective}}) {
                if (!loaded(input)) continue;
                entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(input)), builtin(kind, 0u));
                entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(input) + 1u), builtin(kind, 1u));
            }
        }
        if (loaded(PixelInput::PositionX)) {
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::PositionX)), builtin(StageInputKind::FragCoord, 0u));
        }
        if (loaded(PixelInput::PositionY)) {
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::PositionY)), builtin(StageInputKind::FragCoord, 1u));
        }
        if (loaded(PixelInput::PositionZ)) {
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::PositionZ)), builtin(StageInputKind::FragCoord, 2u));
        }
        if (loaded(PixelInput::PositionW)) {
            IrValue& reciprocalW = entryIr.BitCastF32(builtin(StageInputKind::FragCoord, 3u));
            IrValue& w = entryIr.Emit(IrOpcode::FPRecip32, IrOpcodeType(IrOpcode::FPRecip32), {&reciprocalW});
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::PositionW)), entryIr.BitCastU32(w));
        }
        if (loaded(PixelInput::FrontFace)) {
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::FrontFace)), builtin(StageInputKind::FrontFacing));
        }
        if (loaded(PixelInput::Ancillary)) {
            entryIr.SetVectorReg(static_cast<VectorReg>(vgpr(PixelInput::Ancillary)), builtin(StageInputKind::PackedAncillary));
        }
    } else if (options.stage == ShaderStageKind::Vertex) {
        if (options.userDataBaseRegister >= 8u) {
            entryIr.SetScalarReg(static_cast<ScalarReg>(3), entryIr.Constant(options.waveSize | (options.waveSize << 8u)));
        }
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
        context.SetPixelInput(options.inputInfo.pixel, options.fragmentShaderBarycentricEnabled);
        context.SetFloatMode(options.floatMode);
        for (std::uint32_t index = cfgBlock.instructionBegin; index < cfgBlock.instructionEnd; index++) {
            const auto& instruction = decoded.instructions[index];
            if (isCodeTableLoad(cfg, instruction.programCounter)) {
                const auto table = std::find_if(cfg.codeTableLoads.begin(), cfg.codeTableLoads.end(), [&](const auto& entry) { return entry.programCounter == instruction.programCounter; });
                if (table == cfg.codeTableLoads.end()) throw std::runtime_error("missing shader code table values");
                context.TranslateCodeTableLoad(instruction, *table);
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
                translateExternalFetch(context, instruction, options.inputInfo.vertex);
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
