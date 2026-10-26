#include "BdaShader.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif

namespace {

template<typename TEmit>
std::vector<std::uint32_t> MakeShader(std::uint64_t address, bool coherent, bool stops, const TEmit& emit, std::uint32_t extracted = 0, bool writes = false) {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.Info().usesDma = true;
    program.Info().bdaWrites = writes;
    if (coherent) {
        MemoryInfo memory;
        memory.coherent = true;
        program.Resources().memoryInfo = {memory};
    }
    SpirvEmitterState state(program, {});
    EmitBaseHeader(state.module, program);
    const auto define = [&](std::uint32_t binding) {
        const auto variable = state.module.DefineGlobalVariable(TypeStorageBufferPointer(state), spv::StorageClassStorageBuffer);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, binding);
        return variable;
    };
    state.bdaPagetableVariable = define(0);
    state.faultBufferVariable = define(1);
    const auto output = define(2);
    std::vector<std::uint32_t> interfaceVariables;
    if (!stops) {
        interfaceVariables.push_back(state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32(state)), spv::StorageClassInput));
        state.module.AddAnnotation(spv::OpDecorate, interfaceVariables[0], spv::DecorationBuiltIn, spv::BuiltInLocalInvocationIndex);
    }
    state.bdaStopsInvocations = stops;
    DefineGetBdaPointer(state);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    SpirvValueEmitContext ctx(state);
    auto& instruction = writes ? program.CreateValue(IrOpcode::StoreAddressU32, IrType::Void) : extracted != 0u ? program.CreateValue(IrOpcode::LoadAddressU32x4, IrType::U32x4) : program.CreateValue(IrOpcode::LoadAddressU32, IrType::U32);
    IrBuilder ir(program);
    for (std::uint32_t dword = 0; dword < 4u; ++dword) {
        if ((extracted & (1u << dword)) == 0u) continue;
        auto& extract = program.CreateValue(IrOpcode::CompositeExtractU32x4, IrType::U32);
        extract.AddArgument(&instruction);
        extract.AddArgument(&ir.Constant(dword));
    }
    MemoryFlags flags{};
    flags.pc = 0x1234;
    instruction.SetFlags(flags);
    auto base = BdaConstant(state, address);
    if (!stops) {
        const auto index = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), index, interfaceVariables[0]);
        base = Binary(state, spv::OpIAdd, TypeScalarU64(state), base, Unary(state, spv::OpUConvert, TypeScalarU64(state), index));
    }
    const auto values = emit(ctx, instruction, base);
    for (std::uint32_t index = 0; index < values.size(); ++index) state.module.AddFunction(spv::OpStore, BdaWord(state, output, ConstantU32(state, index)), values[index]);
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, 1u, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", interfaceVariables);
    return state.module.Finalize();
}

}

std::vector<std::uint32_t> MakeBdaTestShader(std::uint64_t address, std::uint32_t bits, std::int64_t offset) {
    using namespace ShaderRecompiler;
    return MakeShader(address, false, true, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        if (offset != 0) base = AddBdaAddress(ctx, instruction, base, BdaConstant(ctx.state, offset < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(offset) : static_cast<std::uint64_t>(offset)), offset < 0);
        return std::vector<std::uint32_t>{EmitBdaRead(ctx, instruction, base, bits)};
    });
}

std::vector<std::uint32_t> MakeBdaSpanReadTestShader(std::uint64_t address, std::uint32_t offset, std::uint32_t extracted, bool coherent, bool stops) {
    using namespace ShaderRecompiler;
    return MakeShader(address, coherent, stops, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        const auto values = EmitBdaDwordReads(ctx, instruction, base, offset, 4u);
        return std::vector<std::uint32_t>(values.begin(), values.end());
    }, extracted);
}

std::vector<std::uint32_t> MakeBdaDwordWriteTestShader(std::uint64_t address, std::uint32_t dwords, const std::uint32_t* values) {
    using namespace ShaderRecompiler;
    return MakeShader(address, false, true, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        auto& state = ctx.state;
        std::vector<std::uint32_t> parts;
        for (std::uint32_t dword = 0; dword < dwords; ++dword) parts.push_back(ConstantU32(state, values[dword]));
        const auto composite = state.module.AllocateId();
        if (dwords == 2u) state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 2u), composite, parts[0], parts[1]);
        else if (dwords == 3u) state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 3u), composite, parts[0], parts[1], parts[2]);
        else state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4u), composite, parts[0], parts[1], parts[2], parts[3]);
        EmitBdaDwordWrites(ctx, instruction, base, 0u, dwords, composite);
        return std::vector<std::uint32_t>{ConstantU32(state, 1u)};
    }, 0u, true);
}

std::vector<std::uint32_t> MakeBdaDwordReadTestShader(std::uint64_t address, std::uint32_t dwords, bool coherent, bool stops) {
    using namespace ShaderRecompiler;
    return MakeShader(address, coherent, stops, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        const auto values = EmitBdaDwordReads(ctx, instruction, base, 0u, dwords, true);
        return std::vector<std::uint32_t>(values.begin(), values.begin() + dwords);
    });
}

std::vector<std::uint32_t> MakeBdaStoreTestShader(std::uint64_t address, std::uint32_t value, std::uint32_t bits, bool coherent) {
    using namespace ShaderRecompiler;
    auto shader = MakeShader(address, coherent, true, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        EmitBdaStore(ctx, instruction, base, ConstantU32(ctx.state, value), bits);
        return std::vector<std::uint32_t>{ConstantU32(ctx.state, 1u)};
    }, 0u, true);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    shader = ValidateAndOptimizeSpirv(shader, VK_API_VERSION_1_1, shader[1], false, false);
#endif
    return shader;
}

std::vector<std::uint32_t> MakeBdaWriteTestShader(std::uint64_t address, std::uint32_t value, std::uint32_t bits, bool coherent) {
    using namespace ShaderRecompiler;
    auto shader = MakeShader(address, coherent, true, [&](SpirvValueEmitContext& ctx, const IrValue& instruction, std::uint32_t base) {
        EmitBdaWrite(ctx, instruction, base, ConstantU32(ctx.state, value), bits);
        return std::vector<std::uint32_t>{ConstantU32(ctx.state, 1u)};
    }, 0u, true);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    shader = ValidateAndOptimizeSpirv(shader, VK_API_VERSION_1_1, shader[1], false, false);
#endif
    return shader;
}

std::vector<std::uint32_t> MakeBdaFaultSequenceTestShader(std::initializer_list<ShaderRecompiler::BdaAbi::FaultReason> reasons) {
    using namespace ShaderRecompiler;
    return MakeShader(0u, false, true, [&](SpirvValueEmitContext& ctx, const IrValue&, std::uint32_t base) {
        for (const auto reason : reasons) RecordBdaFault(ctx.state, base, ConstantU32(ctx.state, 4u), ConstantU32(ctx.state, 0x1234u), reason);
        return std::vector<std::uint32_t>{ConstantU32(ctx.state, 1u)};
    });
}

std::vector<std::uint32_t> MakeBdaAnchoredReadTestShader(const BdaAnchoredReadProgram& reads, bool anchored) {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.Info().usesDma = true;
    if (reads.coherent) {
        MemoryInfo memory;
        memory.coherent = true;
        program.Resources().memoryInfo = {memory};
    }
    SpirvEmitterState state(program, {});
    EmitBaseHeader(state.module, program);
    if (reads.elect) {
        state.module.EmitCapability(spv::CapabilityGroupNonUniform);
        state.module.EmitCapability(spv::CapabilityGroupNonUniformBallot);
        state.requirements.subgroupBallot = true;
    }
    const auto define = [&](std::uint32_t binding) {
        const auto variable = state.module.DefineGlobalVariable(TypeStorageBufferPointer(state), spv::StorageClassStorageBuffer);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, binding);
        return variable;
    };
    state.bdaPagetableVariable = define(0);
    state.faultBufferVariable = define(1);
    const auto output = define(2);
    std::vector<std::uint32_t> interfaceVariables{state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32(state)), spv::StorageClassInput)};
    state.module.AddAnnotation(spv::OpDecorate, interfaceVariables[0], spv::DecorationBuiltIn, spv::BuiltInLocalInvocationIndex);
    state.bdaStopsInvocations = reads.stops;
    DefineGetBdaPointer(state);
    if (anchored) DefineBdaAnchorFunction(state);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    SpirvValueEmitContext ctx(state);
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto input = [&](std::uint32_t word) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, value, BdaWord(state, output, word));
        return value;
    };
    const auto base = Binary(state, spv::OpBitwiseOr, u64, Unary(state, spv::OpUConvert, u64, input(ConstantU32(state, BdaAnchoredReadInputWord))), Binary(state, spv::OpShiftLeftLogical, u64, Unary(state, spv::OpUConvert, u64, input(ConstantU32(state, BdaAnchoredReadInputWord + 1u))), BdaConstant(state, 32u)));
    SpirvEmitterState::BdaAnchor anchor{};
    if (anchored) EmitBdaAnchorRange(state, anchor, base);
    const auto selector = input(ConstantU32(state, BdaAnchoredReadInputWord + 2u));
    const auto laneIndex = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, u32, laneIndex, interfaceVariables[0]);
    const auto low = input(Binary(state, spv::OpIAdd, u32, laneIndex, ConstantU32(state, BdaAnchoredReadInputWord + 3u)));
    const auto slot = Binary(state, spv::OpIMul, u32, laneIndex, ConstantU32(state, 4u));
    IrBuilder ir(program);
    for (std::uint32_t index = 0; index < reads.reads.size(); ++index) {
        const auto& read = reads.reads[index];
        const auto opcode = read.bits == 8u ? IrOpcode::LoadAddressU8 : read.bits == 16u ? IrOpcode::LoadAddressU16 : read.dwords == 2u ? IrOpcode::LoadAddressU32x2 : read.dwords == 3u ? IrOpcode::LoadAddressU32x3 : read.dwords == 4u ? IrOpcode::LoadAddressU32x4 : IrOpcode::LoadAddressU32;
        const auto type = read.bits != 32u || read.dwords == 1u ? IrType::U32 : read.dwords == 2u ? IrType::U32x2 : read.dwords == 3u ? IrType::U32x3 : IrType::U32x4;
        auto& instruction = program.CreateValue(opcode, type);
        MemoryFlags flags{};
        flags.pc = 0x1234;
        instruction.SetFlags(flags);
        if (read.bits == 32u && read.dwords > 1u) {
            const auto extract = read.dwords == 2u ? IrOpcode::CompositeExtractU32x2 : read.dwords == 3u ? IrOpcode::CompositeExtractU32x3 : IrOpcode::CompositeExtractU32x4;
            for (std::uint32_t dword = 0; dword < read.dwords; ++dword) {
                if ((read.used & (1u << dword)) == 0u) continue;
                auto& user = program.CreateValue(extract, IrType::U32);
                user.AddArgument(&instruction);
                user.AddArgument(&ir.Constant(dword));
            }
        }
        EmitIfCondition(state, Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, index)), [&] {
            const auto lookup = [&] {
                if (reads.sentinel) {
                    const auto sentinel = ConstantU32(state, BdaAnchoredReadSentinel);
                    return std::array<std::uint32_t, 4>{sentinel, sentinel, sentinel, sentinel};
                }
                const auto address = AddBdaAddress(ctx, instruction, base, Unary(state, spv::OpUConvert, u64, low), false);
                if (read.bits == 32u) return EmitBdaDwordReads(ctx, instruction, address, static_cast<std::uint32_t>(read.immediate), read.dwords);
                return std::array<std::uint32_t, 4>{EmitBdaRead(ctx, instruction, AddBdaImmediate(ctx, instruction, address, read.immediate), read.bits)};
            };
            const auto values = anchored ? EmitAnchoredBdaRead(state, instruction, anchor, low, read.immediate, read.bits, read.dwords, 0u, lookup) : lookup();
            for (std::uint32_t dword = 0; dword < (read.bits == 32u ? read.dwords : 1u); ++dword) {
                state.module.AddFunction(spv::OpStore, BdaWord(state, output, Binary(state, spv::OpIAdd, u32, slot, ConstantU32(state, dword))), values[dword]);
            }
        });
    }
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, reads.lanes, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", interfaceVariables);
    return state.module.Finalize();
}
