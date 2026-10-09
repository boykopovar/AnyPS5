#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

void EmitBdaOverflowCheck(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction) {
    const auto overflow = Binary(state, spv::OpUGreaterThan, TypeBool(state), address, BdaConstant(state, std::numeric_limits<std::uint64_t>::max() - bytes));
    EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, address, ConstantU32(state, bytes), instruction, BdaAbi::FaultReason::Overflow); });
    StopBdaInvocationIf(state, overflow);
}

bool BdaCoherent(const SpirvEmitterState& state, const IrValue& inst) {
    const auto index = inst.Flags<MemoryFlags>().index;
    const auto& memory = state.program.Resources().memoryInfo;
    return index < memory.size() && memory[index].coherent;
}

std::uint32_t BdaAccessMask(bool coherent) {
    return spv::MemoryAccessAlignedMask | (coherent ? spv::MemoryAccessVolatileMask : 0u);
}

std::uint32_t BdaAccessMask(const SpirvEmitterState& state, const IrValue& inst) {
    return BdaAccessMask(BdaCoherent(state, inst));
}

std::uint32_t EmitBdaByte(SpirvEmitterState& state, std::uint32_t guest, std::uint32_t instruction, std::uint32_t accessMask) {
    const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
    const auto bytePointer = TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType);
    const auto physical = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaPointerFunction, guest, ConstantU32(state, 1u), instruction);
    if (state.bdaStopsInvocations) {
        StopBdaInvocationIf(state, Binary(state, spv::OpIEqual, TypeBool(state), physical, BdaConstant(state, 0u)));
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, bytePointer, pointer, physical);
        const auto loaded = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, byteType, loaded, pointer, accessMask, 1u);
        return Unary(state, spv::OpUConvert, TypeU32(state), loaded);
    }
    // Unmapped bytes read as zero; the fault is recorded and every invocation reaches the
    // program's barriers.
    const auto mapped = Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u));
    const auto before = state.currentLabel;
    const auto loadLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, mapped, loadLabel, merge);
    EmitLabel(state, loadLabel);
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, bytePointer, pointer, physical);
    const auto loaded = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, byteType, loaded, pointer, accessMask, 1u);
    const auto widened = Unary(state, spv::OpUConvert, TypeU32(state), loaded);
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeU32(state), value, widened, loadLabel, ConstantU32(state, 0u), before);
    return value;
}

std::uint32_t EmitBdaBytes(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, std::uint32_t accessMask) {
    auto result = ConstantU32(state, 0u);
    for (std::uint32_t byte = 0; byte < bytes; ++byte) {
        const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), address, BdaConstant(state, byte));
        const auto value = EmitBdaByte(state, guest, instruction, accessMask);
        result = Binary(state, spv::OpBitwiseOr, TypeU32(state), result, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, byte * 8u)));
    }
    return result;
}

// The dwords of a wide load that the program extracts (all of them when the composite is used
// whole). Dead-code elimination has dropped the unused extracts, as it dropped unused dword loads
// when every dword was its own instruction.
std::uint32_t UsedBdaDwords(const IrValue& inst, std::uint32_t dwords) {
    const auto all = (1u << dwords) - 1u;
    if (dwords == 1u) return all;
    std::uint32_t used = 0;
    for (const IrValue* user : inst.Uses()) {
        const auto opcode = user->Opcode();
        if (opcode != IrOpcode::CompositeExtractU32x2 && opcode != IrOpcode::CompositeExtractU32x3 && opcode != IrOpcode::CompositeExtractU32x4) return all;
        const IrValue* index = user->ArgumentCount() > 1u ? user->Argument(1) : nullptr;
        if (index == nullptr || !index->HasImmediate() || index->ImmediateU32() >= dwords) return all;
        used |= 1u << index->ImmediateU32();
    }
    return used;
}

void EmitBdaStoreAt(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t pointerType, std::uint32_t value, std::uint32_t instruction, std::uint32_t accessMask) {
    const auto physical = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaWritePointerFunction, address, ConstantU32(state, bytes), instruction);
    EmitIfCondition(state, Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u)), [&] {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, pointerType, pointer, physical);
        state.module.AddFunction(spv::OpStore, pointer, value, accessMask, bytes);
        state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, address);
    });
}

std::uint32_t BdaAddressUnaligned(SpirvEmitterState& state, std::uint32_t address) {
    return Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), address, BdaConstant(state, 3u)), BdaConstant(state, 0u));
}

std::uint32_t AddBdaSignedOffset(SpirvEmitterState& state, std::uint32_t address, std::uint32_t offset, std::uint32_t instruction) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto negative = Binary(state, spv::OpSLessThan, boolean, offset, ConstantU32(state, 0u));
    const auto magnitude = Unary(state, spv::OpUConvert, u64, Select(state, u32, negative, Binary(state, spv::OpISub, u32, ConstantU32(state, 0u), offset), offset));
    const auto sum = Binary(state, spv::OpIAdd, u64, address, magnitude);
    const auto result = Select(state, u64, negative, Binary(state, spv::OpISub, u64, address, magnitude), sum);
    const auto overflow = Select(state, boolean, negative, Binary(state, spv::OpUGreaterThan, boolean, magnitude, address), Binary(state, spv::OpULessThan, boolean, sum, address));
    EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, address, ConstantU32(state, 0u), instruction, BdaAbi::FaultReason::Overflow); });
    StopBdaInvocationIf(state, overflow);
    return result;
}

std::uint32_t BdaSpanReadResultType(SpirvEmitterState& state) {
    const auto values = TypeU32Vector(state, 4);
    return state.bdaStopsInvocations ? state.module.Type(spv::OpTypeStruct, values, TypeU32(state)) : values;
}

std::uint32_t DefineBdaSpanReadFunction(SpirvEmitterState& state, bool coherent) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto values = TypeU32Vector(state, 4);
    const auto zero = ConstantU32(state, 0u);
    const bool stops = state.bdaStopsInvocations;
    const auto result = BdaSpanReadResultType(state);
    const auto reader = state.bdaDwordReadFunctions[stops][coherent];
    const auto function = state.module.AllocateId();
    state.module.AddName(function, std::string("read_bda_span") + (stops ? "_stop" : "") + (coherent ? "_coherent" : ""));
    state.module.AddFunction(spv::OpFunction, result, function, spv::FunctionControlDontInlineMask, state.module.Type(spv::OpTypeFunction, result, u64, u32, u32, u32));
    const auto address = state.module.AllocateId();
    const auto offset = state.module.AllocateId();
    const auto used = state.module.AllocateId();
    const auto instruction = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, address);
    state.module.AddFunction(spv::OpFunctionParameter, u32, offset);
    state.module.AddFunction(spv::OpFunctionParameter, u32, used);
    state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
    EmitLabel(state, state.module.AllocateId());
    const auto first = EmitGlsl<GLSLstd450FindILsb, IrType::U32>(state, used);
    const auto last = EmitGlsl<GLSLstd450FindUMsb, IrType::U32>(state, used);
    const auto firstBytes = Binary(state, spv::OpShiftLeftLogical, u32, first, ConstantU32(state, 2u));
    const auto firstAddress = AddBdaSignedOffset(state, address, Binary(state, spv::OpIAdd, u32, offset, firstBytes), instruction);
    const auto span = Binary(state, spv::OpISub, u32, Binary(state, spv::OpShiftLeftLogical, u32, Binary(state, spv::OpIAdd, u32, last, ConstantU32(state, 1u)), ConstantU32(state, 2u)), firstBytes);
    const auto physical = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, u64, physical, state.bdaProbeFunction, firstAddress, span, instruction);
    const auto mapped = Binary(state, spv::OpINotEqual, boolean, physical, BdaConstant(state, 0u));
    const auto wide = Binary(state, spv::OpLogicalAnd, boolean, mapped, Unary(state, spv::OpLogicalNot, boolean, BdaAddressUnaligned(state, physical)));
    const auto isUsed = [&](std::uint32_t dword) {
        return Binary(state, spv::OpINotEqual, boolean, Binary(state, spv::OpBitwiseAnd, u32, used, ConstantU32(state, 1u << dword)), zero);
    };
    const auto composite = [&](const std::array<std::uint32_t, 4>& parts) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, values, value, parts[0], parts[1], parts[2], parts[3]);
        return value;
    };
    const auto read = EmitValueIfElse(state, wide, values, [&] {
        std::array<std::uint32_t, 4> loaded{};
        for (std::uint32_t dword = 0; dword < 4u; ++dword) {
            loaded[dword] = EmitValueOrZeroIfCondition(state, isUsed(dword), [&] {
                const auto delta = Unary(state, spv::OpUConvert, u64, Binary(state, spv::OpISub, u32, ConstantU32(state, dword * 4u), firstBytes));
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, Binary(state, spv::OpIAdd, u64, physical, delta));
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpLoad, u32, value, pointer, BdaAccessMask(coherent), 4u);
                return value;
            });
        }
        return composite(loaded);
    }, [&] {
        std::array<std::uint32_t, 4> bytes{};
        for (std::uint32_t dword = 0; dword < 4u; ++dword) {
            bytes[dword] = EmitValueOrZeroIfCondition(state, isUsed(dword), [&] {
                const auto isFirst = Binary(state, spv::OpIEqual, boolean, first, ConstantU32(state, dword));
                const auto dwordAddress = EmitValueIfElse(state, isFirst, u64, [&] { return firstAddress; }, [&] {
                    return AddBdaSignedOffset(state, address, Binary(state, spv::OpIAdd, u32, offset, ConstantU32(state, dword * 4u)), instruction);
                });
                const auto pair = state.module.AllocateId();
                state.module.AddFunction(spv::OpFunctionCall, TypeU32Vector(state, 2), pair, reader, dwordAddress, instruction);
                if (stops) {
                    const auto stopped = state.module.AllocateId();
                    state.module.AddFunction(spv::OpCompositeExtract, u32, stopped, pair, 1u);
                    StopBdaInvocationIf(state, Binary(state, spv::OpINotEqual, boolean, stopped, zero));
                }
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, u32, value, pair, 0u);
                return value;
            });
        }
        return composite(bytes);
    });
    auto returned = read;
    if (stops) {
        returned = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, result, returned, read, zero);
    }
    state.module.AddFunction(spv::OpReturnValue, returned);
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
}

}

void DefineBdaDwordReadFunctions(SpirvEmitterState& state) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto pair = TypeU32Vector(state, 2);
    const auto type = state.module.Type(spv::OpTypeFunction, pair, u64, u32);
    const auto& memory = state.program.Resources().memoryInfo;
    const bool coherentAccesses = std::any_of(memory.begin(), memory.end(), [](const MemoryInfo& info) { return info.coherent; });
    const bool stops = state.bdaStopsInvocations;
    if (stops) state.bdaStopValue = state.module.Constant(spv::OpConstantComposite, pair, ConstantU32(state, 0u), ConstantU32(state, 1u));
    for (const bool stop : {false, true}) {
        for (const bool coherent : {false, true}) {
            if ((stop && !stops) || (coherent && !coherentAccesses)) continue;
            const auto function = state.module.AllocateId();
            state.module.AddName(function, std::string("read_bda_dword_bytes") + (stop ? "_stop" : "") + (coherent ? "_coherent" : ""));
            state.module.AddFunction(spv::OpFunction, pair, function, spv::FunctionControlDontInlineMask, type);
            const auto address = state.module.AllocateId();
            const auto instruction = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionParameter, u64, address);
            state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
            EmitLabel(state, state.module.AllocateId());
            state.bdaStopsInvocations = stop;
            EmitBdaOverflowCheck(state, address, 4u, instruction);
            const auto value = EmitBdaBytes(state, address, 4u, instruction, BdaAccessMask(coherent));
            const auto result = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeConstruct, pair, result, value, ConstantU32(state, 0u));
            state.module.AddFunction(spv::OpReturnValue, result);
            state.module.AddFunction(spv::OpFunctionEnd);
            state.bdaDwordReadFunctions[stop][coherent] = function;
        }
    }
    state.bdaStopsInvocations = stops;
    state.bdaStopValue = 0;
}

void DefineBdaSpanReadFunctions(SpirvEmitterState& state) {
    if (state.bdaProbeFunction == 0) return;
    const auto& memory = state.program.Resources().memoryInfo;
    const bool coherentAccesses = std::any_of(memory.begin(), memory.end(), [](const MemoryInfo& info) { return info.coherent; });
    const bool stops = state.bdaStopsInvocations;
    const auto zero = ConstantU32(state, 0u);
    for (const bool stop : {false, true}) {
        if (stop && !stops) continue;
        state.bdaStopsInvocations = stop;
        state.bdaStopValue = stop ? state.module.Constant(spv::OpConstantComposite, BdaSpanReadResultType(state), state.module.Constant(spv::OpConstantComposite, TypeU32Vector(state, 4), zero, zero, zero, zero), ConstantU32(state, 1u)) : 0u;
        for (const bool coherent : {false, true}) {
            if (coherent && !coherentAccesses) continue;
            state.bdaSpanReadFunctions[stop][coherent] = DefineBdaSpanReadFunction(state, coherent);
        }
    }
    state.bdaStopsInvocations = stops;
    state.bdaStopValue = 0;
}

// APS5_BDA_BYTE_READS=1 restores one lookup and one byte load per byte for every read.
bool BdaByteReadsForced() {
    static const bool forced = std::getenv("APS5_BDA_BYTE_READS") != nullptr;
    return forced;
}

std::uint32_t BdaInstructionPc(SpirvEmitterState& state, const IrValue& inst) {
    return state.bdaPcOverride != 0u ? state.bdaPcOverride : ConstantU32(state, inst.Flags<MemoryFlags>().pc);
}

std::uint32_t AddBdaAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t offset, bool subtract) {
    auto& state = ctx.state;
    const auto result = Binary(state, subtract ? spv::OpISub : spv::OpIAdd, TypeScalarU64(state), address, offset);
    const auto overflow = subtract ? Binary(state, spv::OpUGreaterThan, TypeBool(state), offset, address) : Binary(state, spv::OpULessThan, TypeBool(state), result, address);
    EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, address, ConstantU32(state, 0u), BdaInstructionPc(state, inst), BdaAbi::FaultReason::Overflow); });
    StopBdaInvocationIf(state, overflow);
    return result;
}

std::uint32_t AddBdaImmediate(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::int32_t immediate) {
    if (immediate == 0) {
        return address;
    }
    const auto magnitude = immediate < 0 ? -static_cast<std::int64_t>(immediate) : static_cast<std::int64_t>(immediate);
    return AddBdaAddress(ctx, inst, address, BdaConstant(ctx.state, static_cast<std::uint64_t>(magnitude)), immediate < 0);
}

void ValidateBdaTarget(const IrProgram& program, const SpirvTargetOptions& target) {
    if (program.Info().usesFaultBuffer && target.bdaAbiVersion != BdaAbi::Version) throw std::runtime_error("the fault buffer needs the BDA fault ABI, which the target lacks");
    if (!program.Info().usesDma) return;
    if (target.bdaAbiVersion != BdaAbi::Version) throw std::runtime_error("unsupported BDA ABI version");
    for (const auto capability : {spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess}) {
        if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) == target.supportedCapabilities.end()) throw std::runtime_error("BDA requires unsupported SPIR-V capability " + std::to_string(capability));
    }
    for (const auto extension : {"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"}) {
        if (std::find(target.supportedExtensions.begin(), target.supportedExtensions.end(), extension) == target.supportedExtensions.end()) throw std::runtime_error(std::string("BDA requires unsupported extension ") + extension);
    }
    if (program.Resources().stage == IrShaderStage::TessellationControl) throw std::runtime_error("BDA fault termination requires a barrier-safe tessellation-control execution protocol");
}

bool BdaInvocationsMayStop(const IrProgram& program) {
    for (const auto* block : program.BlockOrder()) {
        for (const auto* instruction : block->Instructions()) {
            if (instruction->Opcode() == IrOpcode::Barrier) return false;
        }
    }
    return true;
}

std::uint32_t EmitBdaRead(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t bits) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u && bits != 32u) ctx.Fail(inst, "unsupported BDA read width");
    if (state.bdaPointerFunction == 0) ctx.Fail(inst, "BDA lookup function is missing");
    if (bits == 32u) return EmitBdaDwordReads(ctx, inst, address, 0u, 1u)[0];
    const auto instruction = BdaInstructionPc(state, inst);
    EmitBdaOverflowCheck(state, address, bits / 8u, instruction);
    return EmitBdaBytes(state, address, bits / 8u, instruction, BdaAccessMask(state, inst));
}

void EmitBdaWrite(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t value, std::uint32_t bits) {
    auto& state = ctx.state;
    if (state.bdaWritePointerFunction == 0 || state.bdaNoteWriteFunction == 0) ctx.Fail(inst, "BDA write functions are missing");
    const auto instruction = BdaInstructionPc(state, inst);
    if (bits != 8u && bits != 16u && bits != 32u) ctx.Fail(inst, "unsupported BDA write width");
    if (bits != 32u) {
        EmitBdaOverflowCheck(state, address, bits / 8u, instruction);
        for (std::uint32_t byte = 0u; byte < bits / 8u; ++byte) {
            const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), address, BdaConstant(state, byte));
            const auto physical = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaWritePointerFunction, guest, ConstantU32(state, 1u), instruction);
            EmitIfCondition(state, Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u)), [&] {
                const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType), pointer, physical);
                const auto shifted = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, ConstantU32(state, byte * 8u));
                state.module.AddFunction(spv::OpStore, pointer, Unary(state, spv::OpUConvert, byteType, shifted), BdaAccessMask(state, inst), 1u);
                state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, guest);
            });
        }
        return;
    }
    const auto unaligned = BdaAddressUnaligned(state, address);
    EmitIfCondition(state, unaligned, [&] { RecordBdaFault(state, address, ConstantU32(state, 4u), instruction, BdaAbi::FaultReason::Unaligned); });
    EmitIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), [&] {
        EmitBdaStoreAt(state, address, 4u, TypePhysicalU32Pointer(state), value, instruction, BdaAccessMask(state, inst));
    });
}

void DefineBdaByteWriteFunctions(SpirvEmitterState& state) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto voidType = state.module.Type(spv::OpTypeVoid);
    const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
    const auto bytePointer = TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType);
    const auto type = state.module.Type(spv::OpTypeFunction, voidType, u64, u32, u32, u32);
    const auto& memory = state.program.Resources().memoryInfo;
    const bool coherentAccesses = std::any_of(memory.begin(), memory.end(), [](const MemoryInfo& info) { return info.coherent; });
    for (const bool coherent : {false, true}) {
        if (coherent && !coherentAccesses) continue;
        const auto function = state.module.AllocateId();
        state.module.AddName(function, std::string("write_bda_bytes") + (coherent ? "_coherent" : ""));
        state.module.AddFunction(spv::OpFunction, voidType, function, spv::FunctionControlDontInlineMask, type);
        const auto address = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        const auto bytes = state.module.AllocateId();
        const auto instruction = state.module.AllocateId();
        state.module.AddFunction(spv::OpFunctionParameter, u64, address);
        state.module.AddFunction(spv::OpFunctionParameter, u32, value);
        state.module.AddFunction(spv::OpFunctionParameter, u32, bytes);
        state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
        EmitLabel(state, state.module.AllocateId());
        const auto counter = state.module.AllocateId();
        state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, u32), counter, spv::StorageClassFunction);
        state.module.AddFunction(spv::OpStore, counter, ConstantU32(state, 0u));
        const auto header = state.module.AllocateId();
        const auto body = state.module.AllocateId();
        const auto continuation = state.module.AllocateId();
        const auto merge = state.module.AllocateId();
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, header);
        const auto byte = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, byte, counter);
        const auto pending = Binary(state, spv::OpULessThan, TypeBool(state), byte, bytes);
        state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, pending, body, merge);
        EmitLabel(state, body);
        const auto guest = Binary(state, spv::OpIAdd, u64, address, Unary(state, spv::OpUConvert, u64, byte));
        const auto shifted = Binary(state, spv::OpShiftRightLogical, u32, value, Binary(state, spv::OpShiftLeftLogical, u32, byte, ConstantU32(state, 3u)));
        EmitBdaStoreAt(state, guest, 1u, bytePointer, Unary(state, spv::OpUConvert, byteType, shifted), instruction, BdaAccessMask(coherent));
        state.module.AddFunction(spv::OpBranch, continuation);
        EmitLabel(state, continuation);
        state.module.AddFunction(spv::OpStore, counter, Binary(state, spv::OpIAdd, u32, byte, ConstantU32(state, 1u)));
        state.module.AddFunction(spv::OpBranch, header);
        EmitLabel(state, merge);
        state.module.AddFunction(spv::OpReturn);
        state.module.AddFunction(spv::OpFunctionEnd);
        state.bdaByteWriteFunctions[coherent] = function;
    }
}

void EmitBdaStore(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t value, std::uint32_t bits) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u && bits != 32u) ctx.Fail(inst, "unsupported BDA store width");
    if (state.bdaWritePointerFunction == 0 || state.bdaNoteWriteFunction == 0) ctx.Fail(inst, "BDA write functions are missing");
    const auto instruction = BdaInstructionPc(state, inst);
    const auto storeBytes = [&] {
        const auto function = state.bdaByteWriteFunctions[BdaCoherent(state, inst)];
        if (function == 0) ctx.Fail(inst, "BDA byte write function is missing");
        state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), function, address, value, ConstantU32(state, bits / 8u), instruction);
    };
    if (bits != 32u) {
        storeBytes();
        return;
    }
    const auto unaligned = BdaAddressUnaligned(state, address);
    EmitIfCondition(state, unaligned, storeBytes);
    EmitIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), [&] {
        EmitBdaStoreAt(state, address, 4u, TypePhysicalU32Pointer(state), value, instruction, BdaAccessMask(state, inst));
    });
}

void EmitBdaDwordWrites(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t offset, std::uint32_t dwords, std::uint32_t value) {
    auto& state = ctx.state;
    if (dwords < 2u || dwords > 4u) ctx.Fail(inst, "unsupported BDA dword store count");
    if (state.bdaWritePointerFunction == 0 || state.bdaNoteWriteFunction == 0) ctx.Fail(inst, "BDA write functions are missing");
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    std::array<std::uint32_t, 4> values{};
    std::array<std::uint32_t, 4> addresses{};
    for (std::uint32_t dword = 0; dword < dwords; ++dword) {
        values[dword] = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, u32, values[dword], value, dword);
        addresses[dword] = AddBdaImmediate(ctx, inst, address, static_cast<std::int32_t>(offset + dword * 4u));
    }
    const auto storeDwords = [&] {
        for (std::uint32_t dword = 0; dword < dwords; ++dword) EmitBdaStore(ctx, inst, addresses[dword], values[dword], 32u);
    };
    if (state.bdaWriteProbeFunction == 0) {
        storeDwords();
        return;
    }
    const auto instruction = BdaInstructionPc(state, inst);
    const auto accessMask = BdaAccessMask(state, inst);
    const auto physical = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, u64, physical, state.bdaWriteProbeFunction, addresses[0], ConstantU32(state, dwords * 4u), instruction);
    const auto mapped = Binary(state, spv::OpINotEqual, boolean, physical, BdaConstant(state, 0u));
    const auto aligned = Binary(state, spv::OpLogicalAnd, boolean, Unary(state, spv::OpLogicalNot, boolean, BdaAddressUnaligned(state, addresses[0])), Unary(state, spv::OpLogicalNot, boolean, BdaAddressUnaligned(state, physical)));
    const auto wide = Binary(state, spv::OpLogicalAnd, boolean, mapped, aligned);
    const auto wideLabel = state.module.AllocateId();
    const auto dwordLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, wide, wideLabel, dwordLabel);
    EmitLabel(state, wideLabel);
    const auto vectorBytes = dwords == 3u ? 4u : dwords * 4u;
    const auto separate = [&] {
        for (std::uint32_t dword = 0; dword < dwords; ++dword) {
            const auto element = dword == 0u ? physical : Binary(state, spv::OpIAdd, u64, physical, BdaConstant(state, dword * 4u));
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, element);
            state.module.AddFunction(spv::OpStore, pointer, values[dword], accessMask, 4u);
        }
    };
    if (dwords == 3u) {
        separate();
    } else {
        const auto vectorAligned = Binary(state, spv::OpIEqual, boolean, Binary(state, spv::OpBitwiseAnd, u64, physical, BdaConstant(state, vectorBytes - 1u)), BdaConstant(state, 0u));
        const auto vectorType = TypeU32Vector(state, dwords);
        const auto vector = state.module.AllocateId();
        if (dwords == 2u) state.module.AddFunction(spv::OpCompositeConstruct, vectorType, vector, values[0], values[1]);
        else state.module.AddFunction(spv::OpCompositeConstruct, vectorType, vector, values[0], values[1], values[2], values[3]);
        EmitIfCondition(state, vectorAligned, [&] {
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, vectorType), pointer, physical);
            state.module.AddFunction(spv::OpStore, pointer, vector, accessMask, vectorBytes);
        });
        EmitIfCondition(state, Unary(state, spv::OpLogicalNot, boolean, vectorAligned), separate);
    }
    const auto last = addresses[dwords - 1u];
    state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, addresses[0]);
    const auto pageShift = BdaConstant(state, BdaAbi::WrittenPageShift);
    const auto crosses = Binary(state, spv::OpINotEqual, boolean, Binary(state, spv::OpShiftRightLogical, u64, addresses[0], pageShift), Binary(state, spv::OpShiftRightLogical, u64, last, pageShift));
    EmitIfCondition(state, crosses, [&] {
        state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, last);
    });
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, dwordLabel);
    storeDwords();
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
}

std::uint32_t EmitBdaAtomic(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t bytes, const std::function<std::uint32_t(std::uint32_t)>& operation) {
    auto& state = ctx.state;
    if (bytes != 4u && bytes != 8u) ctx.Fail(inst, "unsupported BDA atomic width");
    if (state.bdaWritePointerFunction == 0 || state.bdaNoteWriteFunction == 0) ctx.Fail(inst, "BDA write functions are missing");
    const auto instruction = BdaInstructionPc(state, inst);
    const auto type = bytes == 8u ? TypeScalarU64(state) : TypeU32(state);
    const auto zero = bytes == 8u ? BdaConstant(state, 0u) : ConstantU32(state, 0u);
    const auto unaligned = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), address, BdaConstant(state, bytes - 1u)), BdaConstant(state, 0u));
    EmitIfCondition(state, unaligned, [&] { RecordBdaFault(state, address, ConstantU32(state, bytes), instruction, BdaAbi::FaultReason::Unaligned); });
    return EmitValueOrDefaultIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), type, zero, [&] {
        const auto physical = state.module.AllocateId();
        state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaWritePointerFunction, address, ConstantU32(state, bytes), instruction);
        return EmitValueOrDefaultIfCondition(state, Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u)), type, zero, [&] {
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, type), pointer, physical);
            const auto old = operation(pointer);
            state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, address);
            return old;
        });
    });
}

// One probe of the span from the first to the last extracted dword replaces 4 lookups per dword (a
// Bink DC pass spent ~450 dependent table levels per iteration on them). The span is loaded
// dword-wise from the returned device address when it lies in one range and that address is
// 4-aligned; a span crossing ranges (the table is byte-granular, see tests/BdaExecution.cpp), an
// unaligned mirror or a wrapping span falls back to the byte path. That path repeats, per extracted
// dword in order, the address, overflow check and byte lookups of a separate dword load, so every
// fault is recorded exactly as one; the probe records none, and a span it accepts cannot fault.
std::array<std::uint32_t, 4> EmitBdaDwordReads(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t offset, std::uint32_t dwords, bool everyDword) {
    auto& state = ctx.state;
    if (dwords == 0u || dwords > 4u) ctx.Fail(inst, "unsupported BDA read width");
    if (state.bdaPointerFunction == 0) ctx.Fail(inst, "BDA lookup function is missing");
    const auto instruction = BdaInstructionPc(state, inst);
    const auto used = everyDword ? (1u << dwords) - 1u : UsedBdaDwords(inst, dwords);
    const auto isUsed = [&](std::uint32_t dword) { return (used & (1u << dword)) != 0u; };
    std::array<std::uint32_t, 4> values{};
    std::uint32_t first = dwords;
    std::uint32_t last = 0;
    for (std::uint32_t dword = 0; dword < dwords; ++dword) {
        if (!isUsed(dword)) {
            values[dword] = ConstantU32(state, 0u);
            continue;
        }
        first = std::min(first, dword);
        last = dword;
    }
    if (used == 0u) return values;
    std::array<std::uint32_t, 4> addresses{};
    const auto dwordAddress = [&](std::uint32_t dword) { return AddBdaImmediate(ctx, inst, address, static_cast<std::int32_t>(offset + dword * 4u)); };
    const auto readBytes = [&](std::array<std::uint32_t, 4>& into) {
        const auto function = state.bdaDwordReadFunctions[state.bdaStopsInvocations][BdaCoherent(state, inst)];
        if (function == 0) ctx.Fail(inst, "BDA dword read function is missing");
        for (std::uint32_t dword = first; dword <= last; ++dword) {
            if (!isUsed(dword)) continue;
            if (dword != first) addresses[dword] = dwordAddress(dword);
            const auto read = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, TypeU32Vector(state, 2), read, function, addresses[dword], instruction);
            into[dword] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), into[dword], read, 0u);
            if (!state.bdaStopsInvocations) continue;
            const auto stopped = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), stopped, read, 1u);
            StopBdaInvocationIf(state, Binary(state, spv::OpINotEqual, TypeBool(state), stopped, ConstantU32(state, 0u)));
        }
    };
    if (state.bdaProbeFunction == 0) {
        addresses[first] = dwordAddress(first);
        readBytes(values);
        return values;
    }
    const auto function = state.bdaSpanReadFunctions[state.bdaStopsInvocations][BdaCoherent(state, inst)];
    if (function == 0) ctx.Fail(inst, "BDA span read function is missing");
    const auto read = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, BdaSpanReadResultType(state), read, function, address, ConstantU32(state, offset), ConstantU32(state, used), instruction);
    if (state.bdaStopsInvocations) {
        const auto stopped = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), stopped, read, 1u);
        StopBdaInvocationIf(state, Binary(state, spv::OpINotEqual, TypeBool(state), stopped, ConstantU32(state, 0u)));
    }
    for (std::uint32_t dword = first; dword <= last; ++dword) {
        if (!isUsed(dword)) continue;
        values[dword] = state.module.AllocateId();
        if (state.bdaStopsInvocations) state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), values[dword], read, 0u, dword);
        else state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), values[dword], read, dword);
    }
    return values;
}

}
