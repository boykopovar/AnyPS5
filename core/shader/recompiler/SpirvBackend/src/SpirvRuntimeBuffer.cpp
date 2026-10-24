#include "SpirvBackend/SpirvRuntimeBuffer.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "PipelineSpecialization.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <string>
#include <vector>

namespace ShaderRecompiler {
namespace {

struct RuntimeDescriptor {
    std::uint32_t base;
    std::uint32_t records;
    std::uint32_t stride;
    std::uint32_t word3;
    std::uint32_t swizzle;
    std::uint32_t index;
    std::uint32_t valid;
    std::uint32_t offset = 0;
    std::uint32_t soffset = 0;
};

struct RuntimeAddress {
    std::uint32_t guest;
    std::uint32_t inBounds;
};

std::uint32_t field(SpirvEmitterState& state, std::uint32_t word, std::uint32_t offset, std::uint32_t count) {
    return EmitBitFieldUExtract(state, word, ConstantU32(state, offset), ConstantU32(state, count));
}

std::uint32_t both(SpirvEmitterState& state, std::uint32_t left, std::uint32_t right) {
    return Binary(state, spv::OpLogicalAnd, TypeBool(state), left, right);
}

std::uint32_t equal(SpirvEmitterState& state, std::uint32_t left, std::uint32_t right) {
    return Binary(state, spv::OpIEqual, TypeBool(state), left, right);
}

std::uint32_t nonzero(SpirvEmitterState& state, std::uint32_t value) {
    return Binary(state, spv::OpINotEqual, TypeBool(state), value, ConstantU32(state, 0u));
}

std::uint32_t gcnWord3(SpirvEmitterState& state, std::uint32_t word1, std::uint32_t word3) {
    const auto u32 = TypeU32(state);
    const auto& formats = GcnBufferFormats();
    std::array<std::uint32_t, 128> words{};
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = ConstantU32(state, formats[index]);
    const auto arrayType = state.module.Type(spv::OpTypeArray, u32, ConstantU32(state, static_cast<std::uint32_t>(words.size())));
    if (state.gcnBufferFormatVariable == 0) state.gcnBufferFormatVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, arrayType), spv::StorageClassPrivate);
    state.module.AddFunction(spv::OpStore, state.gcnBufferFormatVariable, state.module.Constant(spv::OpConstantComposite, arrayType, std::span<const std::uint32_t>(words)));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassPrivate, u32), pointer, state.gcnBufferFormatVariable, field(state, word3, 12u, 7u));
    const auto format = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, u32, format, pointer);
    const auto outOfBounds = Select(state, u32, nonzero(state, field(state, word1, 16u, 14u)), ConstantU32(state, 1u << 28u), ConstantU32(state, 3u << 28u));
    const auto kept = Binary(state, spv::OpBitwiseAnd, u32, word3, ConstantU32(state, 0xc0e00fffu));
    return Binary(state, spv::OpBitwiseOr, u32, Binary(state, spv::OpBitwiseOr, u32, kept, Binary(state, spv::OpShiftLeftLogical, u32, format, ConstantU32(state, 12u))), outOfBounds);
}

void faultIf(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t condition, std::uint32_t address, std::uint32_t bytes, BdaAbi::FaultReason reason) {
    auto& state = context.state;
    EmitIfCondition(state, condition, [&] { RecordBdaFault(state, address, ConstantU32(state, bytes), BdaInstructionPc(state, instruction), reason); });
    StopBdaInvocationIf(state, condition);
}

RuntimeDescriptor descriptor(SpirvValueEmitContext& context, const IrValue& instruction) {
    auto& state = context.state;
    const auto* handle = instruction.Argument(0)->Resolve();
    if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) context.Fail(instruction, "requires a runtime four-dword V#");
    const auto& memory = context.Memory(instruction);
    const bool runtime = memory.gpuDescriptor || state.registrationModules;
    const auto first = PipelineSpecialization::BufferBase + memory.resource * PipelineSpecialization::BufferWords;
    auto word1 = runtime ? context.Arg(*handle, 1u) : state.module.SpecializationConstant(TypeU32(state), first, 0u);
    if (runtime && GcnGuestShaders()) word1 = Binary(state, spv::OpBitwiseAnd, TypeU32(state), word1, ConstantU32(state, GcnBufferWord1Mask));
    const bool runtimeFormat = memory.gpuDescriptor || (state.registrationModules && !state.program.Info().buffers.at(memory.resource).descriptorFormatted);
    auto word3 = runtimeFormat ? context.Arg(*handle, 3u) : state.module.SpecializationConstant(TypeU32(state), first + 1u, 0u);
    if (runtimeFormat && GcnGuestShaders()) word3 = gcnWord3(state, word1, word3);
    const auto u32 = TypeU32(state);
    auto base = ConstantU32(state, 0u);
    if (memory.gpuDescriptor || BufferAccessOf(instruction.Opcode()) == BufferAccess::Atomic) {
        const auto u64 = TypeScalarU64(state);
        const auto high = Binary(state, spv::OpShiftLeftLogical, u64, Unary(state, spv::OpUConvert, u64, field(state, GcnGuestShaders() ? Binary(state, spv::OpBitwiseAnd, u32, context.Arg(*handle, 1u), ConstantU32(state, GcnBufferWord1Mask)) : context.Arg(*handle, 1u), 0u, 16u)), BdaConstant(state, 32u));
        base = Binary(state, spv::OpBitwiseOr, u64, Unary(state, spv::OpUConvert, u64, context.Arg(*handle, 0u)), high);
    }
    auto invalid = nonzero(state, field(state, word3, 30u, 2u));
    auto index = ConstantU32(state, 0u);
    if (context.Memory(instruction).kind == ResourceKind::Buffer) {
        index = context.Arg(instruction, 1u);
        const auto addTid = nonzero(state, field(state, word3, 23u, 1u));
        if (state.program.Resources().stage == IrShaderStage::Compute) {
            const auto lane = state.laneCount == 1u && state.hostSubgroupSize < state.program.WaveSize() ?
                Binary(state, spv::OpBitwiseAnd, u32, EmitLocalInvocationIndex(state), ConstantU32(state, state.program.WaveSize() - 1u)) : EmitSubgroupLocalInvocationId(state);
            index = Binary(state, spv::OpIAdd, u32, index, Select(state, u32, addTid, lane, ConstantU32(state, 0u)));
        } else {
            invalid = Binary(state, spv::OpLogicalOr, TypeBool(state), invalid, addTid);
        }
    }
    if (memory.gpuDescriptor) faultIf(context, instruction, invalid, base, 0u, BdaAbi::FaultReason::InvalidDescriptor);
    const auto stride = field(state, word1, 16u, 14u);
    const auto present = runtime ? both(state, nonzero(state, context.Arg(*handle, 2u)), Binary(state, spv::OpLogicalOr, TypeBool(state), nonzero(state, context.Arg(*handle, 0u)), nonzero(state, field(state, word1, 0u, 16u)))) : nonzero(state, state.module.SpecializationConstant(u32, first + 2u, 0u));
    const auto valid = memory.gpuDescriptor ? both(state, Unary(state, spv::OpLogicalNot, TypeBool(state), invalid), Binary(state, spv::OpINotEqual, TypeBool(state), base, BdaConstant(state, 0u))) : present;
    return {base, context.Arg(*handle, 2u), stride, word3, both(state, nonzero(state, stride), nonzero(state, field(state, word1, 31u, 1u))), index, valid};
}

RuntimeAddress byteAddress(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t offset, std::uint32_t bytes) {
    auto& state = context.state;
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto add = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpIAdd, u32, left, right); };
    const auto mul = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpIMul, u32, left, right); };
    const auto less = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpULessThan, boolean, left, right); };
    const auto fits = [&](std::uint32_t offset, std::uint32_t size) {
        return both(state, Binary(state, spv::OpUGreaterThanEqual, boolean, size, bytes), Binary(state, spv::OpULessThanEqual, boolean, offset, Binary(state, spv::OpISub, u32, size, bytes)));
    };
    const auto soffset = resource.soffset != 0u ? resource.soffset : context.Arg(instruction, 3u);
    const auto indexShift = add(field(state, resource.word3, 21u, 2u), constant(3u));
    const auto indices = Binary(state, spv::OpShiftLeftLogical, u32, constant(1u), indexShift);
    const auto indexMsb = Binary(state, spv::OpShiftRightLogical, u32, resource.index, indexShift);
    const auto indexLsb = Binary(state, spv::OpBitwiseAnd, u32, resource.index, Binary(state, spv::OpISub, u32, indices, constant(1u)));
    const auto offsetMsb = Binary(state, spv::OpBitwiseAnd, u32, offset, constant(~3u));
    const auto offsetLsb = Binary(state, spv::OpBitwiseAnd, u32, offset, constant(3u));
    const auto swizzled = add(mul(add(mul(indexMsb, resource.stride), offsetMsb), indices), add(Binary(state, spv::OpShiftLeftLogical, u32, indexLsb, constant(2u)), offsetLsb));
    const auto linear = add(mul(resource.index, resource.stride), offset);
    const auto byte = add(Select(state, u32, resource.swizzle, swizzled, linear), soffset);
    const auto mode = field(state, resource.word3, 28u, 2u);
    const auto indexInBounds = less(resource.index, resource.records);
    const auto structured = both(state, indexInBounds, less(offset, resource.stride));
    const auto rawRecords = Binary(state, spv::OpISub, u32, resource.records, soffset);
    const auto raw = both(state, Binary(state, spv::OpULessThanEqual, boolean, soffset, resource.records), Select(state, boolean, resource.swizzle, both(state, less(resource.index, rawRecords), fits(offset, resource.stride)), fits(offset, rawRecords)));
    auto inBounds = Select(state, boolean, equal(state, mode, constant(0u)), structured, indexInBounds);
    inBounds = Select(state, boolean, less(mode, constant(2u)), inBounds, Select(state, boolean, equal(state, mode, constant(2u)), nonzero(state, resource.records), raw));
    inBounds = both(state, resource.valid, both(state, nonzero(state, field(state, resource.word3, 12u, 7u)), inBounds));
    return {byte, inBounds};
}

RuntimeAddress address(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t componentOffset, std::uint32_t bytes) {
    auto& state = context.state;
    const auto offset = Binary(state, spv::OpIAdd, TypeU32(state), resource.offset != 0u ? resource.offset : context.Arg(instruction, 2u), ConstantU32(state, resource.offset != 0u ? componentOffset : context.Memory(instruction).offset + componentOffset));
    const auto bytesAddress = byteAddress(context, instruction, resource, offset, ConstantU32(state, bytes));
    if (!context.Memory(instruction).gpuDescriptor) return bytesAddress;
    return {AddBdaAddress(context, instruction, resource.base, Unary(state, spv::OpUConvert, TypeScalarU64(state), bytesAddress.guest), false), bytesAddress.inBounds};
}

MemoryResourceAccess directAccess(SpirvValueEmitContext& context, const IrValue& instruction, bool wide = false, bool plainLoad = false) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto variable = wide ? state.storageBufferU64Variable : plainLoad || state.storageBufferCoherentVariable == 0u ? state.storageBufferVariable : state.storageBufferCoherentVariable;
    if (variable == 0u) context.Fail(instruction, "direct buffer descriptor array is missing");
    const auto element = ResourceForDescriptor(state, DescriptorBindingKind::Buffers, memory.resource);
    MemoryResourceAccess access;
    access.kind = memory.kind;
    access.objectPointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, wide ? TypeStorageBufferU64Pointer(state) : TypeStorageBufferPointer(state), access.objectPointer, variable, ConstantU32(state, element));
    access.byteOffset = state.memoryByteOffsets.at(element);
    access.memoryAccess = memory.coherent ? spv::MemoryAccessVolatileMask : 0u;
    {
        const auto* descriptor = instruction.Argument(0u)->Resolve();
        std::array<const IrValue*, 4> key{};
        for (std::size_t index = 0; index < key.size(); ++index) key[index] = descriptor->Argument(index)->Resolve();
        const auto found = std::find_if(state.bufferLengths.begin(), state.bufferLengths.end(), [&](const auto& entry) {
            return entry.descriptor == key && entry.variable == variable && entry.element == element && entry.label == state.currentLabel;
        });
        if (found != state.bufferLengths.end()) access.length = found->length;
        else {
            access.length = state.module.AllocateId();
            state.module.AddFunction(spv::OpArrayLength, TypeU32(state), access.length, access.objectPointer, 0u);
            state.bufferLengths.push_back({key, variable, element, state.currentLabel, access.length});
        }
    }
    return access;
}

std::uint32_t baseMisalignment(SpirvValueEmitContext& context, const IrValue& instruction) {
    const auto& memory = context.Memory(instruction);
    if (memory.kind == ResourceKind::ScalarBuffer) return ConstantU32(context.state, 0u);
    auto& state = context.state;
    return state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::BufferBase + memory.resource * PipelineSpecialization::BufferWords + 3u, 0u);
}

std::uint32_t directWordIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t address) {
    const auto relative = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto base = Binary(state, spv::OpShiftRightLogical, TypeU32(state), access.byteOffset, ConstantU32(state, 2u));
    return EmitAddU32(state, relative, base);
}

std::uint32_t readBuffer(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t address, std::uint32_t bits, std::uint32_t enable = 0u) {
    if (context.Memory(instruction).gpuDescriptor) return EmitBdaRead(context, instruction, address, bits);
    auto& state = context.state;
    const auto access = directAccess(context, instruction, false, !context.Memory(instruction).coherent);
    const auto index = directWordIndex(state, access, address);
    const auto misalignment = baseMisalignment(context, instruction);
    const auto unaligned = nonzero(state, misalignment);
    const auto next = EmitAddU32(state, index, ConstantU32(state, 1u));
    const auto last = Select(state, TypeU32(state), unaligned, next, index);
    const auto bounds = EmitMemoryElementInBounds(state, access, last);
    const auto inBounds = enable != 0u ? both(state, enable, bounds) : bounds;
    const auto load = [&](std::uint32_t word) {
        const auto value = state.module.AllocateId();
        const auto pointer = EmitMemoryElementPointer(state, access, word);
        if (access.memoryAccess != 0u) state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, access.memoryAccess);
        else state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), misalignment, ConstantU32(state, 3u));
    const auto combine = [&](std::uint32_t low, std::uint32_t high) {
        auto value = EmitOrU32(state, low, high);
        if (bits != 32u) {
            const auto subwordShift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), EmitAndConstant(state, address, 3u), ConstantU32(state, 3u));
            value = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, subwordShift);
        }
        return EmitAndConstant(state, value, bits == 32u ? 0xffffffffu : (1u << bits) - 1u);
    };
    const auto highShift = Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), shift);
    if (!LoadsSelectable(state, access)) {
        return EmitValueOrZeroIfCondition(state, inBounds, [&] {
            const auto low = Binary(state, spv::OpShiftRightLogical, TypeU32(state), load(index), shift);
            const auto high = EmitValueOrZeroIfCondition(state, unaligned, [&] { return Binary(state, spv::OpShiftLeftLogical, TypeU32(state), load(next), highShift); });
            return combine(low, high);
        });
    }
    const auto lowIndex = GuardedElementIndex(state, access, inBounds, index);
    const auto highIndex = Select(state, TypeU32(state), both(state, inBounds, unaligned), next, lowIndex);
    const auto low = Binary(state, spv::OpShiftRightLogical, TypeU32(state), load(lowIndex), shift);
    const auto high = EmitValueOrZero(state, unaligned, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), load(highIndex), highShift));
    return EmitValueOrZero(state, inBounds, combine(low, high));
}

std::uint32_t byteBuffers(SpirvEmitterState& state) {
    if (state.storageBufferU8Variable != 0u) return state.storageBufferU8Variable;
    if (state.program.Resources().stage != IrShaderStage::Compute) return 0u;
    if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityStorageBuffer8BitAccess)) == state.supportedCapabilities.end()) return 0u;
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    const auto binding = std::find_if(layout.descriptors.begin(), layout.descriptors.end(), [](const IrDescriptorBinding& descriptor) { return descriptor.kind == DescriptorBindingKind::Buffers; });
    if (binding == layout.descriptors.end() || state.storageBufferVariable == 0u) return 0u;
    state.module.EmitCapability(spv::CapabilityStorageBuffer8BitAccess);
    state.module.EmitExtension("SPV_KHR_8bit_storage");
    const auto count = ConstantU32(state, static_cast<std::uint32_t>(binding->resources.size()));
    const auto type = state.module.Type(spv::OpTypeArray, TypeStorageBufferU8Block(state), count);
    const auto variable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassStorageBuffer, type), spv::StorageClassStorageBuffer);
    state.module.AddName(variable, "buffers_u8");
    state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
    state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, NativeBinding(state.program.Resources().stage, DescriptorBindingKind::Buffers));
    state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationAliased);
    if (state.storageBufferU64Variable == 0u && state.storageBufferCoherentVariable == 0u) state.module.AddAnnotation(spv::OpDecorate, state.storageBufferVariable, spv::DecorationAliased);
    if (state.requirements.coherentBuffers) state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationCoherent);
    state.storageBufferU8Variable = variable;
    return variable;
}

void writeBuffer(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t address, std::uint32_t value, std::uint32_t bits = 32u) {
    if (context.Memory(instruction).gpuDescriptor) {
        EmitBdaWrite(context, instruction, address, value, bits);
        return;
    }
    auto& state = context.state;
    const auto access = directAccess(context, instruction);
    const auto u32 = TypeU32(state);
    const auto index = directWordIndex(state, access, address);
    const auto misalignment = baseMisalignment(context, instruction);
    const auto unaligned = nonzero(state, misalignment);
    const auto next = EmitAddU32(state, index, ConstantU32(state, 1u));
    const auto last = Select(state, u32, unaligned, next, index);
    const auto bytes = bits != 32u ? byteBuffers(state) : 0u;
    if (bytes != 0u) {
        const auto element = ResourceForDescriptor(state, DescriptorBindingKind::Buffers, context.Memory(instruction).resource);
        const auto block = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferU8Pointer(state), block, bytes, ConstantU32(state, element));
        EmitIfCondition(state, EmitMemoryElementInBounds(state, access, last), [&] {
            const auto first = EmitAddU32(state, EmitAddU32(state, Binary(state, spv::OpShiftLeftLogical, u32, index, ConstantU32(state, 2u)), misalignment), EmitAndConstant(state, address, 3u));
            for (std::uint32_t byte = 0; byte < bits / 8u; ++byte) {
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferU8ElementPointer(state), pointer, block, ConstantU32(state, 0u), EmitAddU32(state, first, ConstantU32(state, byte)));
                const auto shifted = byte == 0u ? value : Binary(state, spv::OpShiftRightLogical, u32, value, ConstantU32(state, byte * 8u));
                const auto stored = Unary(state, spv::OpUConvert, TypeU8(state), shifted);
                if (access.memoryAccess != 0u) state.module.AddFunction(spv::OpStore, pointer, stored, access.memoryAccess);
                else state.module.AddFunction(spv::OpStore, pointer, stored);
            }
        });
        return;
    }
    EmitIfCondition(state, EmitMemoryElementInBounds(state, access, last), [&] {
        auto mask = ConstantU32(state, bits == 32u ? 0xffffffffu : (1u << bits) - 1u);
        auto masked = Binary(state, spv::OpBitwiseAnd, u32, value, mask);
        if (bits != 32u) {
            const auto subwordShift = Binary(state, spv::OpShiftLeftLogical, u32, EmitAndConstant(state, address, 3u), ConstantU32(state, 3u));
            mask = Binary(state, spv::OpShiftLeftLogical, u32, mask, subwordShift);
            masked = Binary(state, spv::OpShiftLeftLogical, u32, masked, subwordShift);
        }
        const auto update = [&](std::uint32_t word, std::uint32_t wordMask, std::uint32_t replacement) {
            AtomicUpdate(state, EmitMemoryElementPointer(state, access, word), access.kind, [&](std::uint32_t old) {
                return EmitOrU32(state, Binary(state, spv::OpBitwiseAnd, u32, old, Unary(state, spv::OpNot, u32, wordMask)), replacement);
            });
        };
        if (bits == 32u) {
            EmitIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), [&] {
                const auto pointer = EmitMemoryElementPointer(state, access, index);
                if (access.memoryAccess != 0u) state.module.AddFunction(spv::OpStore, pointer, value, access.memoryAccess);
                else state.module.AddFunction(spv::OpStore, pointer, value);
            });
        }
        EmitIfCondition(state, bits == 32u ? unaligned : ConstantBool(state, true), [&] {
            const auto shift = Binary(state, spv::OpShiftLeftLogical, u32, misalignment, ConstantU32(state, 3u));
            update(index, Binary(state, spv::OpShiftLeftLogical, u32, mask, shift), Binary(state, spv::OpShiftLeftLogical, u32, masked, shift));
            EmitIfCondition(state, unaligned, [&] {
                const auto remaining = Binary(state, spv::OpISub, u32, ConstantU32(state, 32u), shift);
                update(next, Binary(state, spv::OpShiftRightLogical, u32, mask, remaining), Binary(state, spv::OpShiftRightLogical, u32, masked, remaining));
            });
        });
    });
}

std::uint32_t composite(SpirvEmitterState& state, std::uint32_t components, const std::array<std::uint32_t, 4>& values) {
    if (components == 1u) return values[0];
    const auto result = state.module.AllocateId();
    std::vector<std::uint32_t> words{spv::OpCompositeConstruct, TypeU32Composite(state, components), result};
    words.insert(words.end(), values.begin(), values.begin() + components);
    state.module.AddFunction(words);
    return result;
}

std::uint32_t storeData(SpirvValueEmitContext& context, const IrValue& instruction) {
    return context.Arg(instruction, instruction.ArgumentCount() - 2u);
}

std::array<std::uint32_t, 4> storeValues(SpirvEmitterState& state, std::uint32_t data, std::uint32_t components) {
    std::array<std::uint32_t, 4> result{};
    for (std::uint32_t component = 0; component < components; ++component) {
        result[component] = data;
        if (components != 1u) {
            result[component] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result[component], data, component);
        }
    }
    return result;
}

std::uint32_t formatted(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, const SpirvBufferFormatInfo& info, std::uint32_t components, bool store, std::uint32_t data) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto u32 = TypeU32(state);
    std::array<std::uint32_t, 4> values{};
    std::array<std::uint32_t, 4> selectors{};
    std::array<std::uint32_t, 4> selected{};
    std::array<RuntimeAddress, 4> addresses{};
    auto inBounds = ConstantBool(state, true);
    for (std::uint32_t output = 0; output < components; ++output) {
        selectors[output] = memory.typed ? ConstantU32(state, output < info.componentCount ? 4u + output : 0u) : field(state, resource.word3, output * 3u, 3u);
        selected[output] = Binary(state, spv::OpUMod, u32, Binary(state, spv::OpISub, u32, selectors[output], ConstantU32(state, 4u)), ConstantU32(state, info.componentCount));
    }
    std::array<std::uint32_t, 4> required{};
    for (std::uint32_t component = 0; component < info.componentCount; ++component) {
        required[component] = ConstantBool(state, store);
        if (!store) {
            for (std::uint32_t output = 0; output < components; ++output) {
                const auto fromMemory = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), selectors[output], ConstantU32(state, 4u));
                required[component] = Binary(state, spv::OpLogicalOr, TypeBool(state), required[component], both(state, fromMemory, equal(state, selected[output], ConstantU32(state, component))));
            }
        }
        addresses[component] = address(context, instruction, resource, GetFormatComponentByteOffset(info, component), info.packedBitfield ? 4u : info.componentBits[component] / 8u);
        inBounds = both(state, inBounds, Binary(state, spv::OpLogicalOr, TypeBool(state), Unary(state, spv::OpLogicalNot, TypeBool(state), required[component]), addresses[component].inBounds));
    }
    if (store) {
        const auto values = storeValues(state, data, components);
        EmitIfCondition(state, inBounds, [&] {
            auto packed = ConstantU32(state, 0u);
            for (std::uint32_t component = 0; component < info.componentCount; ++component) {
                const auto encoded = component >= components ? ConstantU32(state, 0u) : memory.d16 ? EmitD16StoreComponent(state, info, component, values[component]) : EmitFormatStoreComponent(state, info, component, values[component]);
                if (info.packedBitfield) {
                    packed = EmitOrU32(state, packed, Binary(state, spv::OpShiftLeftLogical, u32, EmitAndConstant(state, encoded, (1u << info.componentBits[component]) - 1u), ConstantU32(state, info.componentBitOffset[component])));
                } else {
                    writeBuffer(context, instruction, addresses[component].guest, encoded, info.componentBits[component]);
                }
            }
            if (info.packedBitfield) writeBuffer(context, instruction, addresses[0].guest, packed);
        });
        return ConstantU32(state, 0u);
    }
    for (std::uint32_t component = 0; component < info.componentCount; ++component) {
        values[component] = EmitValueOrZeroIfCondition(state, both(state, inBounds, required[component]), [&] {
            const auto bits = info.componentBits[component];
            auto raw = readBuffer(context, instruction, addresses[component].guest, info.packedBitfield ? 32u : bits);
            if (bits != 32u) {
                const auto signedType = IsSignedFormatComponent(info.type);
                const auto type = signedType ? TypeI32(state) : u32;
                const auto input = signedType ? Unary(state, spv::OpBitcast, type, raw) : raw;
                raw = state.module.AllocateId();
                state.module.AddFunction(signedType ? spv::OpBitFieldSExtract : spv::OpBitFieldUExtract, type, raw, input, ConstantU32(state, info.packedBitfield ? info.componentBitOffset[component] : 0u), ConstantU32(state, bits));
                if (signedType) raw = Unary(state, spv::OpBitcast, u32, raw);
            }
            return memory.d16 ? EmitD16FormatComponent(state, info, component, raw) : NormalizeFormatComponent(state, info, component, raw);
        });
    }
    const bool integer = info.type == SpirvFormatComponentType::Uint || info.type == SpirvFormatComponentType::Sint;
    const auto one = ConstantU32(state, integer ? 1u : memory.d16 ? 0x3c00u : 0x3f800000u);
    std::array<std::uint32_t, 4> outputs{};
    for (std::uint32_t output = 0; output < components; ++output) {
        auto value = values[0];
        for (std::uint32_t component = 1; component < info.componentCount; ++component) value = Select(state, u32, equal(state, selected[output], ConstantU32(state, component)), values[component], value);
        value = Select(state, u32, equal(state, selectors[output], ConstantU32(state, 1u)), one, value);
        outputs[output] = Select(state, u32, equal(state, selectors[output], ConstantU32(state, 0u)), ConstantU32(state, 0u), value);
    }
    return composite(state, components, outputs);
}

std::uint32_t formatTable(SpirvEmitterState& state, std::uint32_t formats) {
    if (state.bufferFormatTable != 0u) return state.bufferFormatTable;
    const auto u32 = TypeU32(state);
    const auto arrayType = state.module.Type(spv::OpTypeArray, u32, ConstantU32(state, 3u * (formats + 1u)));
    std::vector<std::uint32_t> elements;
    for (std::uint32_t index = 0; index <= formats; ++index) {
        std::array<std::uint32_t, 3> entry{};
        if (index != 0u) {
            const auto info = GetFormatInfo(static_cast<IrBufferFormat>(index));
            entry[0] = info.componentCount | (info.packedBitfield ? 8u : 0u) | (static_cast<std::uint32_t>(info.type) << 4u);
            for (std::uint32_t component = 0; component < info.componentCount; ++component) {
                entry[0] |= info.componentBits[component] << (8u + component * 6u);
                entry[1] |= GetFormatComponentByteOffset(info, component) << (component * 8u);
                entry[2] |= info.componentBitOffset[component] << (component * 8u);
            }
        }
        for (const auto word : entry) elements.push_back(ConstantU32(state, word));
    }
    const auto table = state.module.Constant(spv::OpConstantComposite, arrayType, elements);
    state.bufferFormatTable = state.module.DefineInitializedGlobalVariable(TypePointer(state, spv::StorageClassPrivate, arrayType), spv::StorageClassPrivate, table);
    return state.bufferFormatTable;
}

std::uint32_t genericFormattedLoad(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, std::uint32_t format, std::uint32_t formats) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto table = formatTable(state, formats);
    const auto entry = [&](std::uint32_t word) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassPrivate, u32), pointer, table, Binary(state, spv::OpIAdd, u32, Binary(state, spv::OpIMul, u32, format, constant(3u)), constant(word)));
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, value, pointer);
        return value;
    };
    const auto layout = entry(0u);
    const auto byteOffsets = entry(1u);
    const auto bitOffsets = entry(2u);
    const auto componentCount = field(state, layout, 0u, 3u);
    const auto packed = nonzero(state, field(state, layout, 3u, 1u));
    const auto type = field(state, layout, 4u, 4u);
    const auto isType = [&](SpirvFormatComponentType value) { return equal(state, type, constant(static_cast<std::uint32_t>(value))); };
    const auto signedType = Binary(state, spv::OpLogicalOr, boolean, isType(SpirvFormatComponentType::Sint), Binary(state, spv::OpLogicalOr, boolean, isType(SpirvFormatComponentType::Snorm), isType(SpirvFormatComponentType::Sscaled)));
    const auto integer = Binary(state, spv::OpLogicalOr, boolean, isType(SpirvFormatComponentType::Uint), isType(SpirvFormatComponentType::Sint));
    std::array<std::uint32_t, 4> bits{};
    std::array<std::uint32_t, 4> active{};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        bits[component] = field(state, layout, 8u + component * 6u, 6u);
        active[component] = Binary(state, spv::OpULessThan, boolean, constant(component), componentCount);
    }
    std::array<std::uint32_t, 4> selectors{};
    std::array<std::uint32_t, 4> selected{};
    for (std::uint32_t output = 0; output < components; ++output) {
        selectors[output] = field(state, resource.word3, output * 3u, 3u);
        selected[output] = Binary(state, spv::OpUMod, u32, Binary(state, spv::OpISub, u32, selectors[output], constant(4u)), componentCount);
    }
    std::array<std::uint32_t, 4> required{};
    std::array<RuntimeAddress, 4> addresses{};
    auto inBounds = ConstantBool(state, true);
    for (std::uint32_t component = 0; component < 4u; ++component) {
        required[component] = ConstantBool(state, false);
        for (std::uint32_t output = 0; output < components; ++output) {
            const auto fromMemory = Binary(state, spv::OpUGreaterThanEqual, boolean, selectors[output], constant(4u));
            required[component] = Binary(state, spv::OpLogicalOr, boolean, required[component], both(state, fromMemory, equal(state, selected[output], constant(component))));
        }
        required[component] = Select(state, boolean, active[component], required[component], ConstantBool(state, false));
        const auto componentOffset = field(state, byteOffsets, component * 8u, 8u);
        const auto bytes = Select(state, u32, packed, constant(4u), Binary(state, spv::OpShiftRightLogical, u32, bits[component], constant(3u)));
        const auto offset = Binary(state, spv::OpIAdd, u32, resource.offset != 0u ? resource.offset : Binary(state, spv::OpIAdd, u32, context.Arg(instruction, 2u), constant(memory.offset)), componentOffset);
        addresses[component] = byteAddress(context, instruction, resource, offset, bytes);
        if (memory.gpuDescriptor) {
            const auto u64 = TypeScalarU64(state);
            const auto guest = Binary(state, spv::OpIAdd, u64, resource.base, Unary(state, spv::OpUConvert, u64, addresses[component].guest));
            const auto overflow = Select(state, boolean, active[component], Binary(state, spv::OpULessThan, boolean, guest, resource.base), ConstantBool(state, false));
            EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, resource.base, constant(0u), BdaInstructionPc(state, instruction), BdaAbi::FaultReason::Overflow); });
            StopBdaInvocationIf(state, overflow);
            addresses[component].guest = guest;
        }
        inBounds = Select(state, boolean, required[component], both(state, inBounds, addresses[component].inBounds), inBounds);
    }
    std::array<std::uint32_t, 4> values{};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        values[component] = EmitValueOrZeroIfCondition(state, Select(state, boolean, required[component], inBounds, ConstantBool(state, false)), [&] {
            const auto width = Select(state, u32, packed, constant(32u), bits[component]);
            const auto dword = equal(state, width, constant(32u));
            const auto raw = memory.gpuDescriptor
                ? EmitValueIfElse(state, dword, u32, [&] { return readBuffer(context, instruction, addresses[component].guest, 32u); }, [&] {
                    return EmitBdaReadBytes(context, instruction, addresses[component].guest, Binary(state, spv::OpShiftRightLogical, u32, width, constant(3u)));
                })
                : Binary(state, spv::OpShiftRightLogical, u32, readBuffer(context, instruction, addresses[component].guest, 32u), Select(state, u32, dword, constant(0u), Binary(state, spv::OpShiftLeftLogical, u32, EmitAndConstant(state, addresses[component].guest, 3u), constant(3u))));
            const auto bitOffset = Select(state, u32, packed, field(state, bitOffsets, component * 8u, 8u), constant(0u));
            const auto unsignedField = EmitBitFieldUExtract(state, raw, bitOffset, bits[component]);
            const auto signedField = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitFieldSExtract, TypeI32(state), signedField, Unary(state, spv::OpBitcast, TypeI32(state), raw), bitOffset, bits[component]);
            const auto extracted = Select(state, u32, equal(state, bits[component], constant(32u)), raw, Select(state, u32, signedType, Unary(state, spv::OpBitcast, u32, signedField), unsignedField));
            std::vector<std::pair<SpirvFormatComponentType, std::uint32_t>> conversions;
            for (std::uint32_t index = 1; index <= formats; ++index) {
                const auto info = GetFormatInfo(static_cast<IrBufferFormat>(index));
                if (component >= info.componentCount) continue;
                const std::pair<SpirvFormatComponentType, std::uint32_t> key{info.type, info.componentBits[component]};
                if (std::find(conversions.begin(), conversions.end(), key) == conversions.end()) conversions.push_back(key);
            }
            auto value = constant(0u);
            for (const auto& [conversionType, conversionBits] : conversions) {
                SpirvBufferFormatInfo info{};
                info.type = conversionType;
                info.componentCount = 1u;
                info.componentBits[0] = conversionBits;
                const auto converted = memory.d16 ? EmitD16FormatComponent(state, info, 0u, extracted) : NormalizeFormatComponent(state, info, 0u, extracted);
                value = Select(state, u32, both(state, isType(conversionType), equal(state, bits[component], constant(conversionBits))), converted, value);
            }
            return value;
        });
    }
    const auto one = Select(state, u32, integer, constant(1u), constant(memory.d16 ? 0x3c00u : 0x3f800000u));
    std::array<std::uint32_t, 4> outputs{};
    for (std::uint32_t output = 0; output < components; ++output) {
        auto value = values[0];
        for (std::uint32_t component = 1; component < 4u; ++component) value = Select(state, u32, equal(state, selected[output], constant(component)), values[component], value);
        value = Select(state, u32, equal(state, selectors[output], constant(1u)), one, value);
        outputs[output] = Select(state, u32, equal(state, selectors[output], constant(0u)), constant(0u), value);
    }
    return composite(state, components, outputs);
}

std::uint32_t formatSwitch(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data);
std::uint32_t formattedCall(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data);
std::uint32_t formattedGpuCall(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data);

std::uint32_t formattedDispatch(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    if (memory.typed) {
        const auto info = GetFormatInfo(DecodeTBufferFormat(memory.dataFormat, memory.numberFormat));
        if (info.type == SpirvFormatComponentType::Unknown) context.Fail(instruction, "typed buffer instruction has an invalid format");
        return formatted(context, instruction, resource, info, components, store, data);
    }
    if (!store && memory.gpuDescriptor) {
        auto invalid = ConstantBool(state, false);
        for (std::uint32_t component = 0; component < components; ++component) {
            const auto selector = field(state, resource.word3, component * 3u, 3u);
            invalid = Binary(state, spv::OpLogicalOr, TypeBool(state), invalid, Binary(state, spv::OpLogicalOr, TypeBool(state), equal(state, selector, ConstantU32(state, 2u)), equal(state, selector, ConstantU32(state, 3u))));
        }
        faultIf(context, instruction, invalid, resource.base, 0u, BdaAbi::FaultReason::InvalidDescriptor);
    }
    const auto* word = instruction.Argument(0)->Resolve()->Argument(3)->Resolve();
    if (memory.gpuDescriptor && word->HasImmediate()) {
        const auto bits = (word->ImmediateU32() >> 12u) & 0x7fu;
        const auto format = static_cast<IrBufferFormat>(GcnGuestShaders() ? GcnBufferFormats()[bits] : bits);
        if (format == IrBufferFormat::Invalid) context.Fail(instruction, "formatted buffer instruction has an invalid constant format");
        return formatted(context, instruction, resource, GetFormatInfo(format), components, store, data);
    }
    if (!memory.gpuDescriptor) return formattedCall(context, instruction, resource, components, store, data);
    return formattedGpuCall(context, instruction, resource, components, store, data);
}

std::uint32_t formatSwitch(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto format = field(state, resource.word3, 12u, 7u);
    constexpr std::uint32_t count = static_cast<std::uint32_t>(IrBufferFormat::Format32_32_32_32Float);
    if (!store) {
        const auto type = components == 1u ? TypeU32(state) : TypeU32Composite(state, components);
        const auto zero = components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
        const auto unsupported = Binary(state, spv::OpLogicalOr, TypeBool(state), equal(state, format, ConstantU32(state, 0u)), Binary(state, spv::OpUGreaterThan, TypeBool(state), format, ConstantU32(state, count)));
        const auto result = EmitValueIfElse(state, unsupported, type, [&] {
            if (memory.gpuDescriptor) RecordBdaFault(state, resource.base, ConstantU32(state, 0u), BdaInstructionPc(state, instruction), BdaAbi::FaultReason::InvalidDescriptor);
            return zero;
        }, [&] { return genericFormattedLoad(context, instruction, resource, components, format, count); });
        if (memory.gpuDescriptor) StopBdaInvocationIf(state, unsupported);
        return result;
    }
    const auto merge = state.module.AllocateId();
    const auto invalid = state.module.AllocateId();
    std::array<std::uint32_t, count> labels{};
    std::vector<std::uint32_t> branch{spv::OpSwitch, format, invalid};
    for (std::uint32_t index = 0; index < count; ++index) {
        labels[index] = state.module.AllocateId();
        branch.insert(branch.end(), {index + 1u, labels[index]});
    }
    const auto type = store || components == 1u ? TypeU32(state) : TypeU32Composite(state, components);
    const auto result = state.module.AllocateId();
    std::vector<std::uint32_t> phi{spv::OpPhi, type, result};
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(branch);
    for (std::uint32_t index = 0; index < count; ++index) {
        EmitLabel(state, labels[index]);
        const auto value = formatted(context, instruction, resource, GetFormatInfo(static_cast<IrBufferFormat>(index + 1u)), components, store, data);
        const auto exit = state.module.AllocateId();
        state.module.AddFunction(spv::OpBranch, exit);
        EmitLabel(state, exit);
        state.module.AddFunction(spv::OpBranch, merge);
        phi.insert(phi.end(), {value, exit});
    }
    EmitLabel(state, invalid);
    if (memory.gpuDescriptor) RecordBdaFault(state, resource.base, ConstantU32(state, 0u), BdaInstructionPc(state, instruction), BdaAbi::FaultReason::InvalidDescriptor);
    const auto zero = store || components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
    const auto invalidExit = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, invalidExit);
    EmitLabel(state, invalidExit);
    state.module.AddFunction(spv::OpBranch, merge);
    phi.insert(phi.end(), {zero, invalidExit});
    EmitLabel(state, merge);
    state.module.AddFunction(phi);
    const auto unsupported = Binary(state, spv::OpLogicalOr, TypeBool(state), equal(state, format, ConstantU32(state, 0u)), Binary(state, spv::OpUGreaterThan, TypeBool(state), format, ConstantU32(state, count)));
    if (memory.gpuDescriptor) StopBdaInvocationIf(state, unsupported);
    return result;
}

std::uint32_t formattedCall(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto u32 = TypeU32(state);
    const auto valueType = components == 1u ? u32 : TypeU32Composite(state, components);
    const auto resultType = store ? state.module.Type(spv::OpTypeVoid) : valueType;
    const auto element = ResourceForDescriptor(state, DescriptorBindingKind::Buffers, memory.resource);
    const std::array<std::uint32_t, 6> key{memory.resource, components, store ? 1u : 0u, memory.d16 ? 1u : 0u, memory.coherent ? 1u : 0u, static_cast<std::uint32_t>(memory.kind)};
    auto function = state.formattedBufferFunctions.find(key);
    if (function == state.formattedBufferFunctions.end()) {
        const auto id = state.module.AllocateId();
        const auto functionType = store ? state.module.Type(spv::OpTypeFunction, resultType, u32, u32, u32, u32, u32, u32, u32, u32, u32, valueType) : state.module.Type(spv::OpTypeFunction, resultType, u32, u32, u32, u32, u32, u32, u32, u32, u32);
        const auto label = state.currentLabel;
        const auto byteOffset = state.memoryByteOffsets.at(element);
        struct Restore {
            SpirvEmitterState& state;
            std::uint32_t element;
            std::uint32_t label;
            std::uint32_t byteOffset;
            ~Restore() {
                state.module.EndHelperFunction();
                state.memoryByteOffsets.at(element) = byteOffset;
                state.currentLabel = label;
            }
        };
        state.module.BeginHelperFunction();
        const Restore restore{state, element, label, byteOffset};
        state.module.AddFunction(spv::OpFunction, resultType, id, spv::FunctionControlDontInlineMask, functionType);
        std::array<std::uint32_t, 10> parameters{};
        for (std::uint32_t index = 0; index < (store ? 10u : 9u); ++index) {
            parameters[index] = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionParameter, index == 9u ? valueType : u32, parameters[index]);
        }
        EmitLabel(state, state.module.AllocateId());
        state.memoryByteOffsets.at(element) = parameters[4];
        const auto first = PipelineSpecialization::BufferBase + memory.resource * PipelineSpecialization::BufferWords;
        RuntimeDescriptor shared{};
        if (state.registrationModules) {
            const auto word3 = state.program.Info().buffers.at(memory.resource).descriptorFormatted ? state.module.SpecializationConstant(u32, first + 1u, 0u) : parameters[6];
            shared = {ConstantU32(state, 0u), parameters[3], parameters[5], word3, nonzero(state, parameters[7]), parameters[0], nonzero(state, parameters[8]), parameters[1], parameters[2]};
        } else {
            const auto word1 = state.module.SpecializationConstant(u32, first, 0u);
            const auto stride = field(state, word1, 16u, 14u);
            const auto swizzle = both(state, nonzero(state, stride), nonzero(state, field(state, word1, 31u, 1u)));
            const auto valid = nonzero(state, state.module.SpecializationConstant(u32, first + 2u, 0u));
            shared = {ConstantU32(state, 0u), parameters[3], stride, state.module.SpecializationConstant(u32, first + 1u, 0u), swizzle, parameters[0], valid, parameters[1], parameters[2]};
        }
        const auto value = formatSwitch(context, instruction, shared, components, store, store ? parameters[9] : 0u);
        if (store) state.module.AddFunction(spv::OpReturn);
        else state.module.AddFunction(spv::OpReturnValue, value);
        state.module.AddFunction(spv::OpFunctionEnd);
        function = state.formattedBufferFunctions.emplace(key, id).first;
    }
    const auto offset = Binary(state, spv::OpIAdd, u32, context.Arg(instruction, 2u), ConstantU32(state, memory.offset));
    const auto result = state.module.AllocateId();
    const auto flag = [&](std::uint32_t condition) { return Select(state, u32, condition, ConstantU32(state, 1u), ConstantU32(state, 0u)); };
    const auto swizzle = flag(resource.swizzle);
    const auto valid = flag(resource.valid);
    if (store) state.module.AddFunction(spv::OpFunctionCall, resultType, result, function->second, resource.index, offset, context.Arg(instruction, 3u), resource.records, state.memoryByteOffsets.at(element), resource.stride, resource.word3, swizzle, valid, data);
    else state.module.AddFunction(spv::OpFunctionCall, resultType, result, function->second, resource.index, offset, context.Arg(instruction, 3u), resource.records, state.memoryByteOffsets.at(element), resource.stride, resource.word3, swizzle, valid);
    return store ? ConstantU32(state, 0u) : result;
}

std::uint32_t formattedGpuCall(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store, std::uint32_t data) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const bool stops = state.bdaStopsInvocations;
    const auto valueType = components == 1u ? u32 : TypeU32Composite(state, components);
    const auto resultType = stops ? (store ? u32 : state.module.Type(spv::OpTypeStruct, valueType, u32)) : (store ? state.module.Type(spv::OpTypeVoid) : valueType);
    const std::array<std::uint32_t, 6> key{components, store ? 1u : 0u, memory.d16 ? 1u : 0u, memory.coherent ? 1u : 0u, static_cast<std::uint32_t>(memory.kind), stops ? 1u : 0u};
    auto function = state.formattedGpuBufferFunctions.find(key);
    if (function == state.formattedGpuBufferFunctions.end()) {
        const auto id = state.module.AllocateId();
        const auto functionType = store ? state.module.Type(spv::OpTypeFunction, resultType, u64, u32, u32, u32, boolean, u32, boolean, u32, u32, u32, valueType)
                                        : state.module.Type(spv::OpTypeFunction, resultType, u64, u32, u32, u32, boolean, u32, boolean, u32, u32, u32);
        struct Restore {
            SpirvEmitterState& state;
            std::uint32_t label;
            std::uint32_t stopValue;
            ~Restore() {
                state.module.EndHelperFunction();
                state.currentLabel = label;
                state.bdaStopValue = stopValue;
                state.bdaPcOverride = 0u;
            }
        };
        state.module.BeginHelperFunction();
        const Restore restore{state, state.currentLabel, state.bdaStopValue};
        state.module.AddName(id, std::string(store ? "store" : "load") + "_formatted_gpu_buffer_x" + std::to_string(components));
        state.module.AddFunction(spv::OpFunction, resultType, id, spv::FunctionControlDontInlineMask, functionType);
        const std::array<std::uint32_t, 11> types{u64, u32, u32, u32, boolean, u32, boolean, u32, u32, u32, valueType};
        std::array<std::uint32_t, 11> parameters{};
        for (std::uint32_t index = 0; index < (store ? 11u : 10u); ++index) {
            parameters[index] = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionParameter, types[index], parameters[index]);
        }
        EmitLabel(state, state.module.AllocateId());
        const auto zero = components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
        if (stops) state.bdaStopValue = store ? ConstantU32(state, 1u) : state.module.Constant(spv::OpConstantComposite, resultType, zero, ConstantU32(state, 1u));
        state.bdaPcOverride = parameters[9];
        const RuntimeDescriptor shared{parameters[0], parameters[1], parameters[2], parameters[3], parameters[4], parameters[5], parameters[6], parameters[7], parameters[8]};
        const auto value = formatSwitch(context, instruction, shared, components, store, store ? parameters[10] : 0u);
        if (!stops && store) {
            state.module.AddFunction(spv::OpReturn);
        } else {
            auto returned = stops && store ? ConstantU32(state, 0u) : value;
            if (stops && !store) {
                returned = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, resultType, returned, value, ConstantU32(state, 0u));
            }
            state.module.AddFunction(spv::OpReturnValue, returned);
        }
        state.module.AddFunction(spv::OpFunctionEnd);
        function = state.formattedGpuBufferFunctions.emplace(key, id).first;
    }
    const auto offset = Binary(state, spv::OpIAdd, u32, context.Arg(instruction, 2u), ConstantU32(state, memory.offset));
    std::vector<std::uint32_t> call{spv::OpFunctionCall, resultType, state.module.AllocateId(), function->second, resource.base, resource.records, resource.stride, resource.word3, resource.swizzle, resource.index, resource.valid, offset, context.Arg(instruction, 3u), BdaInstructionPc(state, instruction)};
    if (store) call.push_back(data);
    state.module.AddFunction(call);
    const auto result = call[2];
    if (!stops) return store ? ConstantU32(state, 0u) : result;
    if (store) {
        StopBdaInvocationIf(state, Binary(state, spv::OpINotEqual, boolean, result, ConstantU32(state, 0u)));
        return ConstantU32(state, 0u);
    }
    const auto stopped = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, u32, stopped, result, 1u);
    StopBdaInvocationIf(state, Binary(state, spv::OpINotEqual, boolean, stopped, ConstantU32(state, 0u)));
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, valueType, value, result, 0u);
    return value;
}

struct CoherentPair {
    MemoryResourceAccess wide;
    std::uint32_t index;
    std::uint32_t usable;
};

std::optional<CoherentPair> coherentPair(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    if (components != 2u || !memory.coherent || memory.gpuDescriptor || memory.dataBits != 32u || state.storageBufferU64Variable == 0u) return std::nullopt;
    const auto low = address(context, instruction, resource, 0u, 4u);
    const auto high = address(context, instruction, resource, 4u, 4u);
    const auto wide = directAccess(context, instruction, true);
    const auto byte = EmitAddU32(state, low.guest, wide.byteOffset);
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byte, ConstantU32(state, 3u));
    const auto aligned = both(state, equal(state, baseMisalignment(context, instruction), ConstantU32(state, 0u)), equal(state, EmitAndConstant(state, byte, 7u), ConstantU32(state, 0u)));
    const auto adjacent = equal(state, high.guest, EmitAddU32(state, low.guest, ConstantU32(state, 4u)));
    const auto inBounds = both(state, both(state, low.inBounds, high.inBounds), EmitMemoryElementInBounds(state, wide, index));
    return CoherentPair{wide, index, both(state, both(state, aligned, adjacent), inBounds)};
}

}

std::uint32_t EmitRuntimeBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto type = components == 1u ? TypeU32(state) : TypeU32Composite(state, components);
    const auto zero = components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
    return EmitValueOrDefaultIfCondition(state, context.Arg(instruction, instruction.ArgumentCount() - 1u), type, zero, [&] {
        const auto resource = descriptor(context, instruction);
        if (memory.formatted) return EmitValueOrDefaultIfCondition(state, resource.valid, type, zero, [&] { return formattedDispatch(context, instruction, resource, components, false, 0u); });
        const auto readComponents = [&] {
            std::array<std::uint32_t, 4> values{};
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto access = address(context, instruction, resource, component * 4u, memory.dataBits / 8u);
                values[component] = memory.gpuDescriptor ? EmitValueOrZeroIfCondition(state, access.inBounds, [&] { return readBuffer(context, instruction, access.guest, memory.dataBits); })
                                                         : readBuffer(context, instruction, access.guest, memory.dataBits, access.inBounds);
            }
            return composite(state, components, values);
        };
        const auto pair = coherentPair(context, instruction, resource, components);
        if (!pair) return readComponents();
        return EmitValueIfElse(state, pair->usable, type, [&] {
            const auto pointer = EmitStorageBufferElementPointer(state, pair->wide, pair->index, TypeStorageBufferU64ElementPointer(state));
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpAtomicLoad, TypeScalarU64(state), value, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone));
            const auto high = Binary(state, spv::OpShiftRightLogical, TypeScalarU64(state), value, BdaConstant(state, 32u));
            return composite(state, 2u, {Unary(state, spv::OpUConvert, TypeU32(state), value), Unary(state, spv::OpUConvert, TypeU32(state), high), 0u, 0u});
        }, readComponents);
    });
}

void EmitRuntimeBufferStore(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    EmitIfCondition(state, context.Arg(instruction, instruction.ArgumentCount() - 1u), [&] {
        const auto resource = descriptor(context, instruction);
        if (memory.formatted) {
            EmitIfCondition(state, resource.valid, [&] { static_cast<void>(formattedDispatch(context, instruction, resource, components, true, storeData(context, instruction))); });
            return;
        }
        const auto values = storeValues(state, storeData(context, instruction), components);
        const auto writeComponents = [&] {
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto access = address(context, instruction, resource, component * 4u, memory.dataBits / 8u);
                EmitIfCondition(state, access.inBounds, [&] { writeBuffer(context, instruction, access.guest, values[component], memory.dataBits); });
            }
        };
        const auto pair = coherentPair(context, instruction, resource, components);
        if (!pair) {
            writeComponents();
            return;
        }
        EmitIfCondition(state, pair->usable, [&] {
            const auto u64 = TypeScalarU64(state);
            const auto high = Binary(state, spv::OpShiftLeftLogical, u64, Unary(state, spv::OpUConvert, u64, values[1]), BdaConstant(state, 32u));
            const auto value = Binary(state, spv::OpBitwiseOr, u64, Unary(state, spv::OpUConvert, u64, values[0]), high);
            const auto pointer = EmitStorageBufferElementPointer(state, pair->wide, pair->index, TypeStorageBufferU64ElementPointer(state));
            state.module.AddFunction(spv::OpAtomicStore, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
        });
        EmitIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), pair->usable), writeComponents);
    });
}

std::uint32_t EmitRuntimeScalarBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction) {
    auto& state = context.state;
    if (std::ranges::find(state.supportedCapabilities, spv::CapabilityInt64) == state.supportedCapabilities.end()) context.Fail(instruction, "scalar buffer bounds require shaderInt64");
    state.module.EmitCapability(spv::CapabilityInt64);
    const auto resource = descriptor(context, instruction);
    const auto u32 = TypeU32(state);
    const auto stride = Select(state, u32, equal(state, resource.stride, ConstantU32(state, 0u)), ConstantU32(state, 1u), resource.stride);
    const auto size = state.module.AllocateId();
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpUMulExtended, state.module.Type(spv::OpTypeStruct, u32, u32), size, stride, resource.records);
    state.module.AddFunction(spv::OpCompositeExtract, u32, low, size, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, u32, high, size, 1u);
    const auto address = EmitAddU32(state, context.Arg(instruction, 1u), ConstantU32(state, context.Memory(instruction).offset));
    const auto noCarry = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), address, context.Arg(instruction, 1u));
    const auto byte = EmitAndConstant(state, address, ~3u);
    const auto fits = Binary(state, spv::OpLogicalOr, TypeBool(state), nonzero(state, high), Binary(state, spv::OpULessThan, TypeBool(state), byte, EmitAndConstant(state, low, ~3u)));
    const auto inBounds = both(state, noCarry, both(state, resource.valid, fits));
    if (!context.Memory(instruction).gpuDescriptor) return readBuffer(context, instruction, byte, 32u, inBounds);
    const auto offset = Unary(state, spv::OpUConvert, TypeScalarU64(state), byte);
    return EmitValueOrZeroIfCondition(state, inBounds, [&] { return readBuffer(context, instruction, AddBdaAddress(context, instruction, resource.base, offset, false), 32u); });
}

std::uint32_t EmitRuntimeBufferAtomic(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t bits, std::uint32_t active, const std::function<std::uint32_t(std::uint32_t)>& operation) {
    auto& state = context.state;
    if (bits != 32u && bits != 64u) context.Fail(instruction, "unsupported runtime buffer atomic width");
    if (context.Memory(instruction).gpuDescriptor && (state.bdaAtomicPointerFunction == 0u || state.bdaNoteWriteFunction == 0u)) context.Fail(instruction, "BDA atomic functions are missing");
    if (!context.Memory(instruction).gpuDescriptor) {
        if (state.program.Resources().stage == IrShaderStage::TessellationControl) context.Fail(instruction, "BDA fault termination requires a barrier-safe tessellation-control execution protocol");
        if (std::ranges::find(state.supportedCapabilities, spv::CapabilityInt64) == state.supportedCapabilities.end()) context.Fail(instruction, "buffer atomic fault addresses require shaderInt64");
        state.module.EmitCapability(spv::CapabilityInt64);
    }
    const auto type = bits == 64u ? TypeU64(state) : TypeU32(state);
    const auto scalar = bits == 64u ? TypeScalarU64(state) : TypeU32(state);
    const auto zero = bits == 64u ? ConstantU64(state, 0u) : ConstantU32(state, 0u);
    return EmitValueOrDefaultIfCondition(state, active, type, zero, [&] {
        const auto resource = descriptor(context, instruction);
        const auto access = address(context, instruction, resource, 0u, bits / 8u);
        return EmitValueOrDefaultIfCondition(state, access.inBounds, type, zero, [&] {
            if (!context.Memory(instruction).gpuDescriptor) {
                const auto storage = directAccess(context, instruction, bits == 64u);
                const auto byte = EmitAddU32(state, access.guest, storage.byteOffset);
                const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byte, ConstantU32(state, bits == 64u ? 3u : 2u));
                auto valid = EmitMemoryElementInBounds(state, storage, index);
                if (bits == 64u) {
                    const auto unaligned = nonzero(state, EmitAndConstant(state, byte, bits / 8u - 1u));
                    const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), resource.base, Unary(state, spv::OpUConvert, TypeScalarU64(state), access.guest));
                    EmitIfCondition(state, unaligned, [&] {
                        const auto high = Unary(state, spv::OpUConvert, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeScalarU64(state), guest, BdaConstant(state, 32u)));
                        RecordBdaFaultWords(state, Unary(state, spv::OpUConvert, TypeU32(state), guest), high, ConstantU32(state, bits / 8u), BdaInstructionPc(state, instruction), BdaAbi::FaultReason::Unaligned);
                    });
                    StopBdaInvocationIf(state, unaligned);
                    valid = both(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), valid);
                }
                return EmitValueOrDefaultIfCondition(state, valid, type, zero, [&] {
                    const auto pointer = EmitStorageBufferElementPointer(state, storage, index, bits == 64u ? TypeStorageBufferU64ElementPointer(state) : TypeStorageBufferElementPointer(state));
                    return operation(pointer);
                });
            }
            const auto atomicAt = [&](std::uint32_t guest) {
                const auto physical = state.module.AllocateId();
                state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaAtomicPointerFunction, guest, ConstantU32(state, bits / 8u), BdaInstructionPc(state, instruction));
                const auto mapped = Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u));
                return EmitValueOrDefaultIfCondition(state, mapped, type, zero, [&] {
                    const auto pointer = state.module.AllocateId();
                    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, scalar), pointer, physical);
                    const auto result = operation(pointer);
                    state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, guest);
                    return result;
                });
            };
            if (bits == 32u) return atomicAt(Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), access.guest, BdaConstant(state, ~std::uint64_t{3u})));
            const auto unaligned = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), access.guest, BdaConstant(state, bits / 8u - 1u)), BdaConstant(state, 0u));
            faultIf(context, instruction, unaligned, access.guest, bits / 8u, BdaAbi::FaultReason::Unaligned);
            return EmitValueOrDefaultIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), type, zero, [&] { return atomicAt(access.guest); });
        });
    });
}

}
