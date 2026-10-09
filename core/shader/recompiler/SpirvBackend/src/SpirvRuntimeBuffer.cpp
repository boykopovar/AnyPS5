#include "SpirvBackend/SpirvRuntimeBuffer.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "PipelineSpecialization.hpp"
#include <algorithm>
#include <array>
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

void faultIf(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t condition, std::uint32_t address, std::uint32_t bytes, BdaAbi::FaultReason reason) {
    auto& state = context.state;
    EmitIfCondition(state, condition, [&] { RecordBdaFault(state, address, ConstantU32(state, bytes), ConstantU32(state, instruction.Flags<MemoryFlags>().pc), reason); });
    StopBdaInvocationIf(state, condition);
}

RuntimeDescriptor descriptor(SpirvValueEmitContext& context, const IrValue& instruction) {
    auto& state = context.state;
    const auto* handle = instruction.Argument(0)->Resolve();
    if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) context.Fail(instruction, "requires a runtime four-dword V#");
    const auto& memory = context.Memory(instruction);
    const auto first = PipelineSpecialization::BufferBase + memory.resource * PipelineSpecialization::BufferWords;
    const auto word1 = memory.gpuDescriptor ? context.Arg(*handle, 1u) : state.module.SpecializationConstant(TypeU32(state), first, 0u);
    const auto word3 = memory.gpuDescriptor ? context.Arg(*handle, 3u) : state.module.SpecializationConstant(TypeU32(state), first + 1u, 0u);
    const auto u32 = TypeU32(state);
    auto base = ConstantU32(state, 0u);
    if (memory.gpuDescriptor || BufferAccessOf(instruction.Opcode()) == BufferAccess::Atomic) {
        const auto u64 = TypeScalarU64(state);
        const auto high = Binary(state, spv::OpShiftLeftLogical, u64, Unary(state, spv::OpUConvert, u64, field(state, context.Arg(*handle, 1u), 0u, 16u)), BdaConstant(state, 32u));
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
    const auto valid = memory.gpuDescriptor ? both(state, Unary(state, spv::OpLogicalNot, TypeBool(state), invalid), Binary(state, spv::OpINotEqual, TypeBool(state), base, BdaConstant(state, 0u))) : nonzero(state, state.module.SpecializationConstant(u32, first + 2u, 0u));
    return {base, context.Arg(*handle, 2u), stride, word3, both(state, nonzero(state, stride), nonzero(state, field(state, word1, 31u, 1u))), index, valid};
}

RuntimeAddress address(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t componentOffset, std::uint32_t bytes) {
    auto& state = context.state;
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto add = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpIAdd, u32, left, right); };
    const auto mul = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpIMul, u32, left, right); };
    const auto less = [&](std::uint32_t left, std::uint32_t right) { return Binary(state, spv::OpULessThan, boolean, left, right); };
    const auto fits = [&](std::uint32_t offset, std::uint32_t size) {
        return both(state, Binary(state, spv::OpUGreaterThanEqual, boolean, size, constant(bytes)), Binary(state, spv::OpULessThanEqual, boolean, offset, Binary(state, spv::OpISub, u32, size, constant(bytes))));
    };
    const auto offset = add(context.Arg(instruction, 2u), constant(context.Memory(instruction).offset + componentOffset));
    const auto soffset = context.Arg(instruction, 3u);
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
    if (!context.Memory(instruction).gpuDescriptor) return {byte, inBounds};
    return {AddBdaAddress(context, instruction, resource.base, Unary(state, spv::OpUConvert, TypeScalarU64(state), byte), false), inBounds};
}

MemoryResourceAccess directAccess(SpirvValueEmitContext& context, const IrValue& instruction, bool wide = false) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto variable = wide ? state.storageBufferU64Variable : state.storageBufferVariable;
    if (variable == 0u) context.Fail(instruction, "direct buffer descriptor array is missing");
    const auto element = ResourceForDescriptor(state, DescriptorBindingKind::Buffers, memory.resource);
    MemoryResourceAccess access;
    access.kind = memory.kind;
    access.objectPointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, wide ? TypeStorageBufferU64Pointer(state) : TypeStorageBufferPointer(state), access.objectPointer, variable, ConstantU32(state, element));
    access.byteOffset = state.memoryByteOffsets.at(element);
    access.memoryAccess = memory.coherent ? spv::MemoryAccessVolatileMask : 0u;
    access.length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, TypeU32(state), access.length, access.objectPointer, 0u);
    return access;
}

std::uint32_t baseMisalignment(SpirvValueEmitContext& context, const IrValue& instruction) {
    const auto& memory = context.Memory(instruction);
    if (memory.kind == ResourceKind::ScalarBuffer) return ConstantU32(context.state, 0u);
    const auto first = PipelineSpecialization::BufferBase + memory.resource * PipelineSpecialization::BufferWords;
    return context.state.module.SpecializationConstant(TypeU32(context.state), first + 3u, 0u);
}

std::uint32_t directWordIndex(SpirvEmitterState& state, const MemoryResourceAccess& access, std::uint32_t address) {
    const auto relative = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto base = Binary(state, spv::OpShiftRightLogical, TypeU32(state), access.byteOffset, ConstantU32(state, 2u));
    return EmitAddU32(state, relative, base);
}

std::uint32_t readBuffer(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t address, std::uint32_t bits) {
    if (context.Memory(instruction).gpuDescriptor) return EmitBdaRead(context, instruction, address, bits);
    auto& state = context.state;
    const auto access = directAccess(context, instruction);
    const auto index = directWordIndex(state, access, address);
    const auto misalignment = baseMisalignment(context, instruction);
    const auto unaligned = nonzero(state, misalignment);
    const auto next = EmitAddU32(state, index, ConstantU32(state, 1u));
    const auto last = Select(state, TypeU32(state), unaligned, next, index);
    return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, access, last), [&] {
        const auto load = [&](std::uint32_t word) {
            const auto value = state.module.AllocateId();
            const auto pointer = EmitMemoryElementPointer(state, access, word);
            if (access.memoryAccess != 0u) state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, access.memoryAccess);
            else state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
            return value;
        };
        const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), misalignment, ConstantU32(state, 3u));
        const auto low = Binary(state, spv::OpShiftRightLogical, TypeU32(state), load(index), shift);
        const auto high = EmitValueOrZeroIfCondition(state, unaligned, [&] {
            return Binary(state, spv::OpShiftLeftLogical, TypeU32(state), load(next), Binary(state, spv::OpISub, TypeU32(state), ConstantU32(state, 32u), shift));
        });
        auto value = EmitOrU32(state, low, high);
        if (bits != 32u) {
            const auto subwordShift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), EmitAndConstant(state, address, 3u), ConstantU32(state, 3u));
            value = Binary(state, spv::OpShiftRightLogical, TypeU32(state), value, subwordShift);
        }
        return EmitAndConstant(state, value, bits == 32u ? 0xffffffffu : (1u << bits) - 1u);
    });
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

std::array<std::uint32_t, 4> storeValues(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components) {
    auto& state = context.state;
    std::array<std::uint32_t, 4> result{};
    const auto data = context.Arg(instruction, instruction.ArgumentCount() - 2u);
    for (std::uint32_t component = 0; component < components; ++component) {
        result[component] = data;
        if (components != 1u) {
            result[component] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), result[component], data, component);
        }
    }
    return result;
}

std::uint32_t formatted(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, const SpirvBufferFormatInfo& info, std::uint32_t components, bool store) {
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
        const auto data = storeValues(context, instruction, components);
        EmitIfCondition(state, inBounds, [&] {
            auto packed = ConstantU32(state, 0u);
            for (std::uint32_t component = 0; component < info.componentCount; ++component) {
                const auto encoded = component >= components ? ConstantU32(state, 0u) : memory.d16 ? EmitD16StoreComponent(state, info, component, data[component]) : EmitFormatStoreComponent(state, info, component, data[component]);
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

std::uint32_t formattedDispatch(SpirvValueEmitContext& context, const IrValue& instruction, const RuntimeDescriptor& resource, std::uint32_t components, bool store) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    if (memory.typed) {
        const auto info = GetFormatInfo(DecodeTBufferFormat(memory.dataFormat, memory.numberFormat));
        if (info.type == SpirvFormatComponentType::Unknown) context.Fail(instruction, "typed buffer instruction has an invalid format");
        return formatted(context, instruction, resource, info, components, store);
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
        const auto format = static_cast<IrBufferFormat>((word->ImmediateU32() >> 12u) & 0x7fu);
        if (format == IrBufferFormat::Invalid) context.Fail(instruction, "formatted buffer instruction has an invalid constant format");
        return formatted(context, instruction, resource, GetFormatInfo(format), components, store);
    }
    const auto format = field(state, resource.word3, 12u, 7u);
    const auto merge = state.module.AllocateId();
    const auto invalid = state.module.AllocateId();
    constexpr std::uint32_t count = static_cast<std::uint32_t>(IrBufferFormat::Format32_32_32_32Float);
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
        const auto value = formatted(context, instruction, resource, GetFormatInfo(static_cast<IrBufferFormat>(index + 1u)), components, store);
        const auto exit = state.module.AllocateId();
        state.module.AddFunction(spv::OpBranch, exit);
        EmitLabel(state, exit);
        state.module.AddFunction(spv::OpBranch, merge);
        phi.insert(phi.end(), {value, exit});
    }
    EmitLabel(state, invalid);
    if (memory.gpuDescriptor) RecordBdaFault(state, resource.base, ConstantU32(state, 0u), ConstantU32(state, instruction.Flags<MemoryFlags>().pc), BdaAbi::FaultReason::InvalidDescriptor);
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

}

std::uint32_t EmitRuntimeBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t components) {
    auto& state = context.state;
    const auto& memory = context.Memory(instruction);
    const auto type = components == 1u ? TypeU32(state) : TypeU32Composite(state, components);
    const auto zero = components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
    return EmitValueOrDefaultIfCondition(state, context.Arg(instruction, instruction.ArgumentCount() - 1u), type, zero, [&] {
        const auto resource = descriptor(context, instruction);
        if (memory.formatted) return EmitValueOrDefaultIfCondition(state, resource.valid, type, zero, [&] { return formattedDispatch(context, instruction, resource, components, false); });
        const auto readComponents = [&] {
            std::array<std::uint32_t, 4> values{};
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto access = address(context, instruction, resource, component * 4u, memory.dataBits / 8u);
                values[component] = EmitValueOrZeroIfCondition(state, access.inBounds, [&] { return readBuffer(context, instruction, access.guest, memory.dataBits); });
            }
            return composite(state, components, values);
        };
        if (components != 2u || !memory.coherent || memory.gpuDescriptor || state.storageBufferU64Variable == 0u) return readComponents();
        const auto runtime = address(context, instruction, resource, 0u, 8u);
        const auto wide = directAccess(context, instruction, true);
        const auto byte = EmitAddU32(state, runtime.guest, wide.byteOffset);
        const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byte, ConstantU32(state, 3u));
        const auto aligned = both(state, equal(state, baseMisalignment(context, instruction), ConstantU32(state, 0u)), equal(state, EmitAndConstant(state, byte, 7u), ConstantU32(state, 0u)));
        const auto inBounds = both(state, runtime.inBounds, EmitMemoryElementInBounds(state, wide, index));
        return EmitValueIfElse(state, both(state, aligned, inBounds), type, [&] {
            const auto pointer = EmitStorageBufferElementPointer(state, wide, index, TypeStorageBufferU64ElementPointer(state));
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
            EmitIfCondition(state, resource.valid, [&] { static_cast<void>(formattedDispatch(context, instruction, resource, components, true)); });
            return;
        }
        const auto values = storeValues(context, instruction, components);
        for (std::uint32_t component = 0; component < components; ++component) {
            const auto access = address(context, instruction, resource, component * 4u, memory.dataBits / 8u);
            EmitIfCondition(state, access.inBounds, [&] { writeBuffer(context, instruction, access.guest, values[component], memory.dataBits); });
        }
    });
}

std::uint32_t EmitRuntimeScalarBufferLoad(SpirvValueEmitContext& context, const IrValue& instruction) {
    auto& state = context.state;
    if (std::ranges::find(state.supportedCapabilities, spv::CapabilityInt64) == state.supportedCapabilities.end()) context.Fail(instruction, "scalar buffer bounds require shaderInt64");
    state.module.EmitCapability(spv::CapabilityInt64);
    const auto resource = descriptor(context, instruction);
    const auto u64 = TypeScalarU64(state);
    const auto size = Select(state, u64, equal(state, resource.stride, ConstantU32(state, 0u)), Unary(state, spv::OpUConvert, u64, resource.records), Binary(state, spv::OpIMul, u64, Unary(state, spv::OpUConvert, u64, resource.stride), Unary(state, spv::OpUConvert, u64, resource.records)));
    const auto address = EmitAddU32(state, context.Arg(instruction, 1u), ConstantU32(state, context.Memory(instruction).offset));
    const auto noCarry = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), address, context.Arg(instruction, 1u));
    const auto byte = EmitAndConstant(state, address, ~3u);
    const auto offset = Unary(state, spv::OpUConvert, u64, byte);
    const auto inBounds = both(state, noCarry, both(state, resource.valid, Binary(state, spv::OpULessThanEqual, TypeBool(state), Binary(state, spv::OpIAdd, u64, offset, BdaConstant(state, 4u)), size)));
    if (!context.Memory(instruction).gpuDescriptor) return EmitValueOrZeroIfCondition(state, inBounds, [&] { return readBuffer(context, instruction, byte, 32u); });
    return EmitValueOrZeroIfCondition(state, inBounds, [&] { return readBuffer(context, instruction, AddBdaAddress(context, instruction, resource.base, offset, false), 32u); });
}

std::uint32_t EmitRuntimeBufferAtomic(SpirvValueEmitContext& context, const IrValue& instruction, std::uint32_t bits, std::uint32_t active, const std::function<std::uint32_t(std::uint32_t)>& operation) {
    auto& state = context.state;
    if (bits != 32u && bits != 64u) context.Fail(instruction, "unsupported runtime buffer atomic width");
    if (context.Memory(instruction).gpuDescriptor && (state.bdaAtomicPointerFunction == 0u || state.bdaNoteWriteFunction == 0u)) context.Fail(instruction, "BDA atomic functions are missing");
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
                const auto unaligned = nonzero(state, EmitAndConstant(state, byte, bits / 8u - 1u));
                const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), resource.base, Unary(state, spv::OpUConvert, TypeScalarU64(state), access.guest));
                faultIf(context, instruction, unaligned, guest, bits / 8u, BdaAbi::FaultReason::Unaligned);
                const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byte, ConstantU32(state, bits == 64u ? 3u : 2u));
                return EmitValueOrDefaultIfCondition(state, both(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), EmitMemoryElementInBounds(state, storage, index)), type, zero, [&] {
                    const auto pointer = EmitStorageBufferElementPointer(state, storage, index, bits == 64u ? TypeStorageBufferU64ElementPointer(state) : TypeStorageBufferElementPointer(state));
                    return operation(pointer);
                });
            }
            const auto unaligned = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), access.guest, BdaConstant(state, bits / 8u - 1u)), BdaConstant(state, 0u));
            faultIf(context, instruction, unaligned, access.guest, bits / 8u, BdaAbi::FaultReason::Unaligned);
            return EmitValueOrDefaultIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned), type, zero, [&] {
                const auto physical = state.module.AllocateId();
                state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaAtomicPointerFunction, access.guest, ConstantU32(state, bits / 8u), ConstantU32(state, instruction.Flags<MemoryFlags>().pc));
                const auto mapped = Binary(state, spv::OpINotEqual, TypeBool(state), physical, BdaConstant(state, 0u));
                return EmitValueOrDefaultIfCondition(state, mapped, type, zero, [&] {
                    const auto pointer = state.module.AllocateId();
                    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, scalar), pointer, physical);
                    const auto result = operation(pointer);
                    state.module.AddFunction(spv::OpFunctionCall, state.module.Type(spv::OpTypeVoid), state.module.AllocateId(), state.bdaNoteWriteFunction, access.guest);
                    return result;
                });
            });
        });
    });
}

}
