#ifndef CORE_SHADER_RECOMPILER_SPIRVINSTRUCTIONOPERANDS_HPP
#define CORE_SHADER_RECOMPILER_SPIRVINSTRUCTIONOPERANDS_HPP

#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include <cstdint>
#include <initializer_list>

namespace ShaderRecompiler::SpirvInstructions {

template<typename TInstruction>
spv::Op Opcode(const TInstruction& instruction) {
    return instruction.empty() ? spv::OpNop : static_cast<spv::Op>(instruction[0] & 0xffffu);
}

template<typename TInstruction>
std::uint32_t Result(const TInstruction& instruction) {
    if (instruction.empty()) return 0u;
    bool result = false;
    bool type = false;
    spv::HasResultAndType(Opcode(instruction), &result, &type);
    return result ? instruction.at(type ? 2u : 1u) : 0u;
}

template<typename TInstruction, typename TVisitor>
void VisitIdOperands(const TInstruction& instruction, std::uint32_t switchLiteralWords, const TVisitor& visit) {
    const auto size = instruction.size();
    const auto from = [&](std::size_t first) {
        for (auto index = first; index < size; ++index) visit(index);
    };
    const auto only = [&](std::initializer_list<std::size_t> indices) {
        for (const auto index : indices) {
            if (index < size) visit(index);
        }
    };
    constexpr std::uint32_t knownMemoryAccess = static_cast<std::uint32_t>(spv::MemoryAccessVolatileMask) | static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask) |
        static_cast<std::uint32_t>(spv::MemoryAccessNontemporalMask) | static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask) |
        static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask) | static_cast<std::uint32_t>(spv::MemoryAccessNonPrivatePointerMask);
    const auto memoryOperands = [&](std::size_t index) {
        while (index < size) {
            const auto mask = instruction[index++];
            if ((mask & ~knownMemoryAccess) != 0u) {
                from(index);
                return;
            }
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask)) != 0u) ++index;
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask)) != 0u && index < size) visit(index++);
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask)) != 0u && index < size) visit(index++);
        }
    };
    switch (Opcode(instruction)) {
    case spv::OpNop:
    case spv::OpCapability:
    case spv::OpExtension:
    case spv::OpExtInstImport:
    case spv::OpMemoryModel:
    case spv::OpSource:
    case spv::OpSourceContinued:
    case spv::OpSourceExtension:
    case spv::OpString:
    case spv::OpName:
    case spv::OpMemberName:
    case spv::OpModuleProcessed:
    case spv::OpDecorate:
    case spv::OpMemberDecorate:
    case spv::OpDecorateString:
    case spv::OpMemberDecorateString:
    case spv::OpTypeVoid:
    case spv::OpTypeBool:
    case spv::OpTypeInt:
    case spv::OpTypeFloat:
    case spv::OpTypeSampler:
    case spv::OpConstantTrue:
    case spv::OpConstantFalse:
    case spv::OpConstant:
    case spv::OpConstantSampler:
    case spv::OpConstantNull:
    case spv::OpSpecConstantTrue:
    case spv::OpSpecConstantFalse:
    case spv::OpSpecConstant:
    case spv::OpLabel:
    case spv::OpNoLine:
    case spv::OpReturn:
    case spv::OpUnreachable:
    case spv::OpKill:
    case spv::OpFunctionEnd:
        return;
    case spv::OpTypeVector:
    case spv::OpTypeMatrix:
    case spv::OpTypeImage:
        only({2});
        return;
    case spv::OpTypePointer:
        only({3});
        return;
    case spv::OpTypeForwardPointer:
    case spv::OpExecutionMode:
    case spv::OpLine:
    case spv::OpSelectionMerge:
        only({1});
        return;
    case spv::OpSpecConstantOp:
        only({1});
        from(4);
        return;
    case spv::OpVariable:
    case spv::OpFunction:
        only({1, 4});
        return;
    case spv::OpLoad:
        only({1, 3});
        memoryOperands(4);
        return;
    case spv::OpStore:
    case spv::OpCopyMemory:
        only({1, 2});
        memoryOperands(3);
        return;
    case spv::OpCopyMemorySized:
        only({1, 2, 3});
        memoryOperands(4);
        return;
    case spv::OpArrayLength:
    case spv::OpCompositeExtract:
        only({1, 3});
        return;
    case spv::OpCompositeInsert:
    case spv::OpVectorShuffle:
        only({1, 3, 4});
        return;
    case spv::OpExtInst:
        only({1, 3});
        from(5);
        return;
    case spv::OpEntryPoint: {
        only({2});
        std::size_t index = 3;
        while (index < size) {
            const auto word = instruction[index++];
            if ((word & 0xffu) == 0u || (word & 0xff00u) == 0u || (word & 0xff0000u) == 0u || (word & 0xff000000u) == 0u) break;
        }
        from(index);
        return;
    }
    case spv::OpExecutionModeId:
    case spv::OpDecorateId:
        only({1});
        from(3);
        return;
    case spv::OpLoopMerge:
        only({1, 2});
        return;
    case spv::OpBranchConditional:
        only({1, 2, 3});
        return;
    case spv::OpSwitch:
        only({1, 2});
        for (std::size_t index = 3; index + switchLiteralWords < size; index += switchLiteralWords + 1u) visit(index + switchLiteralWords);
        return;
    case spv::OpImageSampleImplicitLod:
    case spv::OpImageSampleExplicitLod:
    case spv::OpImageSampleProjImplicitLod:
    case spv::OpImageSampleProjExplicitLod:
    case spv::OpImageFetch:
    case spv::OpImageRead:
    case spv::OpImageSparseSampleImplicitLod:
    case spv::OpImageSparseSampleExplicitLod:
    case spv::OpImageSparseSampleProjImplicitLod:
    case spv::OpImageSparseSampleProjExplicitLod:
    case spv::OpImageSparseFetch:
    case spv::OpImageSparseRead:
        only({1, 3, 4});
        from(6);
        return;
    case spv::OpImageSampleDrefImplicitLod:
    case spv::OpImageSampleDrefExplicitLod:
    case spv::OpImageSampleProjDrefImplicitLod:
    case spv::OpImageSampleProjDrefExplicitLod:
    case spv::OpImageGather:
    case spv::OpImageDrefGather:
    case spv::OpImageSparseSampleDrefImplicitLod:
    case spv::OpImageSparseSampleDrefExplicitLod:
    case spv::OpImageSparseSampleProjDrefImplicitLod:
    case spv::OpImageSparseSampleProjDrefExplicitLod:
    case spv::OpImageSparseGather:
    case spv::OpImageSparseDrefGather:
        only({1, 3, 4, 5});
        from(7);
        return;
    case spv::OpImageWrite:
        only({1, 2, 3});
        from(5);
        return;
    case spv::OpGroupIAdd:
    case spv::OpGroupFAdd:
    case spv::OpGroupFMin:
    case spv::OpGroupUMin:
    case spv::OpGroupSMin:
    case spv::OpGroupFMax:
    case spv::OpGroupUMax:
    case spv::OpGroupSMax:
    case spv::OpGroupNonUniformBallotBitCount:
    case spv::OpGroupNonUniformIAdd:
    case spv::OpGroupNonUniformFAdd:
    case spv::OpGroupNonUniformIMul:
    case spv::OpGroupNonUniformFMul:
    case spv::OpGroupNonUniformSMin:
    case spv::OpGroupNonUniformUMin:
    case spv::OpGroupNonUniformFMin:
    case spv::OpGroupNonUniformSMax:
    case spv::OpGroupNonUniformUMax:
    case spv::OpGroupNonUniformFMax:
    case spv::OpGroupNonUniformBitwiseAnd:
    case spv::OpGroupNonUniformBitwiseOr:
    case spv::OpGroupNonUniformBitwiseXor:
    case spv::OpGroupNonUniformLogicalAnd:
    case spv::OpGroupNonUniformLogicalOr:
    case spv::OpGroupNonUniformLogicalXor:
        only({1, 3});
        from(5);
        return;
    default:
        from(1);
        return;
    }
}

}

#endif
