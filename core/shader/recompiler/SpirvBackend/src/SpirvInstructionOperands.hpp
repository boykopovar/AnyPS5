#ifndef CORE_SHADER_RECOMPILER_SPIRVINSTRUCTIONOPERANDS_HPP
#define CORE_SHADER_RECOMPILER_SPIRVINSTRUCTIONOPERANDS_HPP

#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <string_view>

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

inline bool KnownCoreOpcode(spv::Op op) {
    return static_cast<std::uint32_t>(op) < 4096u && std::string_view(spv::OpToString(op)) != "Unknown";
}

inline bool SpecConstantIdOperation(spv::Op op) {
    switch (op) {
    case spv::OpSConvert:
    case spv::OpUConvert:
    case spv::OpFConvert:
    case spv::OpSNegate:
    case spv::OpNot:
    case spv::OpIAdd:
    case spv::OpISub:
    case spv::OpIMul:
    case spv::OpUDiv:
    case spv::OpSDiv:
    case spv::OpUMod:
    case spv::OpSRem:
    case spv::OpSMod:
    case spv::OpShiftRightLogical:
    case spv::OpShiftRightArithmetic:
    case spv::OpShiftLeftLogical:
    case spv::OpBitwiseOr:
    case spv::OpBitwiseXor:
    case spv::OpBitwiseAnd:
    case spv::OpLogicalOr:
    case spv::OpLogicalAnd:
    case spv::OpLogicalNot:
    case spv::OpLogicalEqual:
    case spv::OpLogicalNotEqual:
    case spv::OpSelect:
    case spv::OpIEqual:
    case spv::OpINotEqual:
    case spv::OpULessThan:
    case spv::OpSLessThan:
    case spv::OpUGreaterThan:
    case spv::OpSGreaterThan:
    case spv::OpULessThanEqual:
    case spv::OpSLessThanEqual:
    case spv::OpUGreaterThanEqual:
    case spv::OpSGreaterThanEqual:
    case spv::OpQuantizeToF16:
    case spv::OpConvertFToS:
    case spv::OpConvertSToF:
    case spv::OpConvertFToU:
    case spv::OpConvertUToF:
    case spv::OpConvertPtrToU:
    case spv::OpConvertUToPtr:
    case spv::OpGenericCastToPtr:
    case spv::OpPtrCastToGeneric:
    case spv::OpBitcast:
    case spv::OpFNegate:
    case spv::OpFAdd:
    case spv::OpFSub:
    case spv::OpFMul:
    case spv::OpFDiv:
    case spv::OpFRem:
    case spv::OpFMod:
    case spv::OpAccessChain:
    case spv::OpInBoundsAccessChain:
    case spv::OpPtrAccessChain:
    case spv::OpInBoundsPtrAccessChain:
        return true;
    default:
        return false;
    }
}

template<typename TInstruction, typename TVisitor>
bool VisitIdOperands(const TInstruction& instruction, std::uint32_t switchLiteralWords, const TVisitor& visit) {
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
    constexpr std::uint32_t operandMemoryAccess = static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask) | static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask) |
        static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask);
    const auto memoryOperands = [&](std::initializer_list<std::size_t> ids, std::size_t first) {
        for (auto index = first; index < size;) {
            const auto mask = instruction[index++];
            if ((mask & ~knownMemoryAccess) != 0u) return false;
            index += static_cast<std::size_t>(std::popcount(mask & operandMemoryAccess));
        }
        only(ids);
        for (auto index = first; index < size;) {
            const auto mask = instruction[index++];
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask)) != 0u) ++index;
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask)) != 0u && index < size) visit(index++);
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask)) != 0u && index < size) visit(index++);
        }
        return true;
    };
    switch (Opcode(instruction)) {
    case spv::OpNop:
    case spv::OpCapability:
    case spv::OpExtension:
    case spv::OpExtInstImport:
    case spv::OpMemoryModel:
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
    case spv::OpTypeOpaque:
    case spv::OpTypePipe:
    case spv::OpTypeSampler:
    case spv::OpConstantTrue:
    case spv::OpConstantFalse:
    case spv::OpConstant:
    case spv::OpConstantSampler:
    case spv::OpConstantNull:
    case spv::OpConstantPipeStorage:
    case spv::OpSpecConstantTrue:
    case spv::OpSpecConstantFalse:
    case spv::OpSpecConstant:
    case spv::OpLabel:
    case spv::OpNoLine:
    case spv::OpReturn:
    case spv::OpUnreachable:
    case spv::OpKill:
    case spv::OpFunctionEnd:
    case spv::OpTerminateInvocation:
    case spv::OpBeginInvocationInterlockEXT:
    case spv::OpEndInvocationInterlockEXT:
    case spv::OpDemoteToHelperInvocation:
        return true;
    case spv::OpSource:
        only({3});
        return true;
    case spv::OpTypeVector:
    case spv::OpTypeMatrix:
    case spv::OpTypeImage:
        only({2});
        return true;
    case spv::OpTypePointer:
        only({3});
        return true;
    case spv::OpTypeForwardPointer:
    case spv::OpExecutionMode:
    case spv::OpLine:
    case spv::OpSelectionMerge:
    case spv::OpLifetimeStart:
    case spv::OpLifetimeStop:
        only({1});
        return true;
    case spv::OpSpecConstantOp: {
        if (size < 4u) {
            only({1});
            return true;
        }
        const auto nested = static_cast<spv::Op>(instruction[3]);
        if (nested == spv::OpCompositeExtract) {
            only({1, 4});
            return true;
        }
        if (nested == spv::OpCompositeInsert || nested == spv::OpVectorShuffle) {
            only({1, 4, 5});
            return true;
        }
        if (!SpecConstantIdOperation(nested)) return false;
        only({1});
        from(4);
        return true;
    }
    case spv::OpVariable:
    case spv::OpFunction:
        only({1, 4});
        return true;
    case spv::OpLoad:
        return memoryOperands({1, 3}, 4);
    case spv::OpStore:
    case spv::OpCopyMemory:
        return memoryOperands({1, 2}, 3);
    case spv::OpCopyMemorySized:
        return memoryOperands({1, 2, 3}, 4);
    case spv::OpArrayLength:
    case spv::OpCompositeExtract:
    case spv::OpGenericCastToPtrExplicit:
        only({1, 3});
        return true;
    case spv::OpCompositeInsert:
    case spv::OpVectorShuffle:
        only({1, 3, 4});
        return true;
    case spv::OpExtInst:
        only({1, 3});
        from(5);
        return true;
    case spv::OpEntryPoint: {
        only({2});
        std::size_t index = 3;
        while (index < size) {
            const auto word = instruction[index++];
            if ((word & 0xffu) == 0u || (word & 0xff00u) == 0u || (word & 0xff0000u) == 0u || (word & 0xff000000u) == 0u) break;
        }
        from(index);
        return true;
    }
    case spv::OpExecutionModeId:
    case spv::OpDecorateId:
        only({1});
        from(3);
        return true;
    case spv::OpGroupMemberDecorate:
        only({1});
        for (std::size_t index = 2; index < size; index += 2u) visit(index);
        return true;
    case spv::OpLoopMerge:
        only({1, 2});
        return true;
    case spv::OpBranchConditional:
        only({1, 2, 3});
        return true;
    case spv::OpSwitch:
        only({1, 2});
        for (std::size_t index = 3; index + switchLiteralWords < size; index += switchLiteralWords + 1u) visit(index + switchLiteralWords);
        return true;
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
        return true;
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
        return true;
    case spv::OpImageWrite:
        only({1, 2, 3});
        from(5);
        return true;
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
        return true;
    case spv::OpSubgroupBallotKHR:
    case spv::OpSubgroupFirstInvocationKHR:
    case spv::OpSubgroupAllKHR:
    case spv::OpSubgroupAnyKHR:
    case spv::OpSubgroupAllEqualKHR:
    case spv::OpSubgroupReadInvocationKHR:
    case spv::OpGroupNonUniformRotateKHR:
    case spv::OpFmaKHR:
    case spv::OpReadClockKHR:
    case spv::OpSetMeshOutputsEXT:
    case spv::OpAtomicFMinEXT:
    case spv::OpAtomicFMaxEXT:
    case spv::OpAtomicFAddEXT:
        from(1);
        return true;
    default:
        if (!KnownCoreOpcode(Opcode(instruction))) return false;
        from(1);
        return true;
    }
}

}

#endif
