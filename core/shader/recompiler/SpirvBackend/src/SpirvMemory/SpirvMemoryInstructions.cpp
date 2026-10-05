#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace ShaderRecompiler {
namespace {

std::uint32_t AndCondition(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs) {
    return Binary(state, spv::OpLogicalAnd, TypeBool(state), lhs, rhs);
}

std::uint32_t BufferByteAddress(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t index, std::uint32_t offset, std::uint32_t soffset) {
    auto& state = ctx.state;
    const std::uint32_t packed = StorageBufferPackedStride(state, mem);
    const std::uint32_t stride = packed & 0x3fffu;
    const bool swizzle = stride != 0u && ((packed >> 14u) & 1u) != 0u;
    if (((packed >> 20u) & 1u) != 0u) {
        const auto lane = Binary(state, spv::OpBitwiseAnd, TypeU32(state), EmitSubgroupLocalInvocationId(state), ConstantU32(state, 63u));
        index = Binary(state, spv::OpIAdd, TypeU32(state), index, lane);
    }
    if (mem.offset != 0u) {
        offset = Binary(state, spv::OpIAdd, TypeU32(state), offset, ConstantU32(state, mem.offset));
    }
    std::uint32_t address = 0;
    if (!swizzle) {
        if (stride == 0u) {
            address = offset;
        } else {
            const auto indexed = stride == 1u ? index : Binary(state, spv::OpIMul, TypeU32(state), index, ConstantU32(state, stride));
            address = Binary(state, spv::OpIAdd, TypeU32(state), indexed, offset);
        }
    } else {
        const std::uint32_t strideEnum = (packed >> 16u) & 3u;
        const std::uint32_t indexStride = 8u << strideEnum;
        const auto indexMsb = Binary(state, spv::OpShiftRightLogical, TypeU32(state), index, ConstantU32(state, strideEnum + 3u));
        const auto indexLsb = Binary(state, spv::OpBitwiseAnd, TypeU32(state), index, ConstantU32(state, indexStride - 1u));
        const auto offsetMsb = Binary(state, spv::OpBitwiseAnd, TypeU32(state), offset, ConstantU32(state, ~3u));
        const auto offsetLsb = Binary(state, spv::OpBitwiseAnd, TypeU32(state), offset, ConstantU32(state, 3u));
        const auto indexedMsb = stride == 1u ? indexMsb : Binary(state, spv::OpIMul, TypeU32(state), indexMsb, ConstantU32(state, stride));
        const auto msb = Binary(state, spv::OpIMul, TypeU32(state), Binary(state, spv::OpIAdd, TypeU32(state), indexedMsb, offsetMsb), ConstantU32(state, indexStride));
        const auto lsb = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), indexLsb, ConstantU32(state, 2u)), offsetLsb);
        address = Binary(state, spv::OpIAdd, TypeU32(state), msb, lsb);
    }
    const IrValue* soffsetValue = inst.Argument(3)->Resolve();
    if (soffsetValue->HasImmediate() && soffsetValue->Type() == IrType::U32 && soffsetValue->ImmediateU32() == 0u) {
        return address;
    }
    return Binary(state, spv::OpIAdd, TypeU32(state), address, soffset);
}

struct WordPair {
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

WordPair AddWordPair(SpirvEmitterState& state, const WordPair& value, const WordPair& addend) {
    const auto low = Binary(state, spv::OpIAdd, TypeU32(state), value.low, addend.low);
    const auto carry = Binary(state, spv::OpULessThan, TypeBool(state), low, value.low);
    const auto carryValue = Select(state, TypeU32(state), carry, ConstantU32(state, 1u), ConstantU32(state, 0u));
    const auto high = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpIAdd, TypeU32(state), value.high, addend.high), carryValue);
    return {low, high};
}

std::uint32_t ScratchByteAddress(SpirvValueEmitContext& ctx, const MemoryInfo& mem, std::uint32_t low, std::uint32_t high) {
    auto& state = ctx.state;
    const auto immediate = static_cast<std::int32_t>(mem.offset);
    const WordPair addend{ConstantU32(state, static_cast<std::uint32_t>(immediate)), ConstantU32(state, immediate < 0 ? 0xffffffffu : 0u)};
    const auto sum = AddWordPair(state, {low, high}, addend);
    const auto valid = Binary(state, spv::OpIEqual, TypeBool(state), sum.high, ConstantU32(state, 0u));
    return Select(state, TypeU32(state), valid, sum.low, ConstantU32(state, 0xffffffffu));
}

std::uint32_t ByteAddress(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    auto& state = ctx.state;
    switch (mem.kind) {
    case ResourceKind::Buffer:
        return BufferByteAddress(ctx, inst, mem, ctx.Arg(inst, 1), ctx.Arg(inst, 2), ctx.Arg(inst, 3));
    case ResourceKind::Lds:
    case ResourceKind::Gds:
        if (mem.kind == ResourceKind::Lds) {
            if (const auto found = state.requirements.functionLdsAddresses.find(&inst); found != state.requirements.functionLdsAddresses.end()) {
                return ConstantU32(state, found->second + mem.offset);
            }
        }
        if (mem.offset == 0u) {
            return ctx.Arg(inst, 0);
        }
        return Binary(state, spv::OpIAdd, TypeU32(state), ctx.Arg(inst, 0), ConstantU32(state, mem.offset));
    case ResourceKind::Scratch:
        return ScratchByteAddress(ctx, mem, ctx.Arg(inst, 1), ctx.Arg(inst, 2));
    default:
        ctx.Fail(inst, "has a memory kind without a dword-addressed emitter");
    }
}

std::uint32_t DwordIndex(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    return Binary(ctx.state, spv::OpShiftRightLogical, TypeU32(ctx.state), ByteAddress(ctx, inst, mem), ConstantU32(ctx.state, 2u));
}

std::uint32_t ActiveArgument(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return ctx.Arg(inst, inst.ArgumentCount() - 1u);
}

std::uint32_t LoadWordInBounds(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t index) {
    auto& state = ctx.state;
    const auto pointer = EmitMemoryElementPointer(state, resource, index);
    const auto value = state.module.AllocateId();
    if (resource.memoryAccess != 0u) state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, resource.memoryAccess);
    else state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

std::uint32_t LoadSubwordInBounds(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t address, std::uint32_t index, std::uint32_t bits, bool signExtend) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u) {
        ctx.Fail("subword load has an unsupported width");
    }
    const auto word = LoadWordInBounds(ctx, resource, index);
    const auto byte = Binary(state, spv::OpBitwiseAnd, TypeU32(state), address, ConstantU32(state, 3u));
    const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), byte, ConstantU32(state, 3u));
    const auto value = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), word, shift), ConstantU32(state, bits == 8u ? 0xffu : 0xffffu));
    if (!signExtend) {
        return value;
    }
    const auto left = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, 32u - bits));
    return Binary(state, spv::OpShiftRightArithmetic, TypeU32(state), left, ConstantU32(state, 32u - bits));
}

std::uint32_t LoadWordPrepared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource) {
    auto& state = ctx.state;
    const auto index = EmitMemoryElementIndex(state, resource, DwordIndex(ctx, inst, mem));
    return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
        return LoadWordInBounds(ctx, resource, index);
    });
}

std::uint32_t LoadWord(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    return EmitValueOrZeroIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        return LoadWordPrepared(ctx, inst, mem, resource);
    });
}

std::uint32_t LoadSubwordPrepared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource, std::uint32_t bits, bool signExtend) {
    auto& state = ctx.state;
    const auto address = ByteAddress(ctx, inst, mem);
    const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto index = EmitMemoryElementIndex(state, resource, rawIndex);
    return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
        return LoadSubwordInBounds(ctx, resource, address, index, bits, signExtend);
    });
}

std::uint32_t LoadSubword(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t bits) {
    return EmitValueOrZeroIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        return LoadSubwordPrepared(ctx, inst, mem, resource, bits, false);
    });
}

std::uint32_t ConstantDeviceAddress(SpirvEmitterState& state, std::uint64_t value) {
    return state.module.Constant(spv::OpConstant, TypeScalarU64(state), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u));
}

std::uint32_t DeviceAddressFromWords(SpirvEmitterState& state, std::uint32_t low, std::uint32_t high) {
    const auto low64 = Unary(state, spv::OpUConvert, TypeScalarU64(state), low);
    const auto high64 = Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), Unary(state, spv::OpUConvert, TypeScalarU64(state), high), ConstantDeviceAddress(state, 32u));
    return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), low64, high64);
}

std::pair<std::uint32_t, std::uint32_t> RuntimeBufferByteAddress(SpirvEmitterState& state, std::uint32_t index, std::uint32_t offset, std::uint32_t soffset, std::uint32_t immediate, std::uint32_t stride, std::uint32_t swizzle, std::uint32_t indexStride) {
    const auto u32 = TypeU32(state);
    const auto add = [&](std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, spv::OpIAdd, u32, lhs, rhs); };
    const auto mul = [&](std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, spv::OpIMul, u32, lhs, rhs); };
    if (immediate != 0u) offset = add(offset, ConstantU32(state, immediate));
    const auto linear = add(mul(index, stride), offset);
    const auto indexShift = add(indexStride, ConstantU32(state, 3u));
    const auto indices = Binary(state, spv::OpShiftLeftLogical, u32, ConstantU32(state, 1u), indexShift);
    const auto indexMsb = Binary(state, spv::OpShiftRightLogical, u32, index, indexShift);
    const auto indexLsb = Binary(state, spv::OpBitwiseAnd, u32, index, Binary(state, spv::OpISub, u32, indices, ConstantU32(state, 1u)));
    const auto offsetMsb = Binary(state, spv::OpBitwiseAnd, u32, offset, ConstantU32(state, ~3u));
    const auto offsetLsb = Binary(state, spv::OpBitwiseAnd, u32, offset, ConstantU32(state, 3u));
    const auto swizzled = add(mul(add(mul(indexMsb, stride), offsetMsb), indices), add(Binary(state, spv::OpShiftLeftLogical, u32, indexLsb, ConstantU32(state, 2u)), offsetLsb));
    return {offset, add(Select(state, u32, swizzle, swizzled, linear), soffset)};
}

template <typename TFunction>
void ForEachGpuDescriptorDword(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components, TFunction&& function) {
    auto& state = ctx.state;
    const IrValue* handle = inst.Argument(0)->Resolve();
    if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) {
        ctx.Fail(inst, "has no GPU-selected V#");
    }
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    const auto word1 = ctx.Arg(*handle, 1);
    const auto records = ctx.Arg(*handle, 2);
    const auto word3 = ctx.Arg(*handle, 3);
    const auto field = [&](std::uint32_t word, std::uint32_t first, std::uint32_t count) { return EmitBitFieldUExtract(state, word, ConstantU32(state, first), ConstantU32(state, count)); };
    const auto nonzero = [&](std::uint32_t value) { return Binary(state, spv::OpINotEqual, boolean, value, ConstantU32(state, 0u)); };
    const auto hasDword = [&](std::uint32_t offset, std::uint32_t size) {
        return AndCondition(state, Binary(state, spv::OpUGreaterThanEqual, boolean, size, ConstantU32(state, 4u)), Binary(state, spv::OpULessThanEqual, boolean, offset, Binary(state, spv::OpISub, u32, size, ConstantU32(state, 4u))));
    };
    const auto stride = field(word1, 16u, 14u);
    const auto swizzle = AndCondition(state, nonzero(stride), nonzero(field(word1, 31u, 1u)));
    const auto lane = Binary(state, spv::OpBitwiseAnd, u32, EmitSubgroupLocalInvocationId(state), ConstantU32(state, 63u));
    const auto index = Binary(state, spv::OpIAdd, u32, ctx.Arg(inst, 1), Select(state, u32, nonzero(field(word3, 23u, 1u)), lane, ConstantU32(state, 0u)));
    const auto soffset = ctx.Arg(inst, 3);
    const auto base = DeviceAddressFromWords(state, ctx.Arg(*handle, 0), field(word1, 0u, 16u));
    const auto mode = field(word3, 28u, 2u);
    const auto indexInBounds = Binary(state, spv::OpULessThan, boolean, index, records);
    const auto scalarInBounds = Binary(state, spv::OpULessThanEqual, boolean, soffset, records);
    const auto rawRecords = Binary(state, spv::OpISub, u32, records, soffset);
    const auto rawIndexInBounds = Binary(state, spv::OpULessThan, boolean, index, rawRecords);
    for (std::uint32_t component = 0; component < components; component++) {
        const auto [offset, byte] = RuntimeBufferByteAddress(state, index, ctx.Arg(inst, 2), soffset, mem.offset + component * 4u, stride, swizzle, field(word3, 21u, 2u));
        const auto structured = AndCondition(state, indexInBounds, Binary(state, spv::OpULessThan, boolean, offset, stride));
        const auto raw = AndCondition(state, scalarInBounds, Select(state, boolean, swizzle, AndCondition(state, rawIndexInBounds, hasDword(offset, stride)), hasDword(offset, rawRecords)));
        auto inBounds = Select(state, boolean, Binary(state, spv::OpIEqual, boolean, mode, ConstantU32(state, 0u)), structured, indexInBounds);
        inBounds = Select(state, boolean, Binary(state, spv::OpULessThan, boolean, mode, ConstantU32(state, 2u)), inBounds, Select(state, boolean, Binary(state, spv::OpIEqual, boolean, mode, ConstantU32(state, 2u)), nonzero(records), raw));
        const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), base, Unary(state, spv::OpUConvert, TypeScalarU64(state), byte));
        function(component, guest, AndCondition(state, nonzero(field(word3, 12u, 7u)), inBounds));
    }
}

std::uint32_t ConstructU32Composite(SpirvEmitterState& state, std::uint32_t components, const std::array<std::uint32_t, 4>& values);

std::uint32_t LoadGpuDescriptor(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t components) {
    std::array<std::uint32_t, 4> values{};
    ForEachGpuDescriptorDword(ctx, inst, ctx.Memory(inst), components, [&](std::uint32_t component, std::uint32_t guest, std::uint32_t inBounds) {
        values[component] = EmitValueOrZeroIfCondition(ctx.state, inBounds, [&] { return EmitBdaRead(ctx, inst, guest, 32u); });
    });
    return components == 1u ? values[0] : ConstructU32Composite(ctx.state, components, values);
}

void StoreGpuDescriptor(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t components) {
    auto& state = ctx.state;
    const auto data = ctx.Arg(inst, inst.ArgumentCount() - 2u);
    ForEachGpuDescriptorDword(ctx, inst, ctx.Memory(inst), components, [&](std::uint32_t component, std::uint32_t guest, std::uint32_t inBounds) {
        auto value = data;
        if (components != 1u) {
            value = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), value, data, component);
        }
        EmitIfCondition(state, inBounds, [&] { EmitBdaWrite(ctx, inst, guest, value); });
    });
}

std::uint32_t GuestAddressBase(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    auto& state = ctx.state;
    auto low = ctx.Arg(inst, 1);
    if (mem.kind == ResourceKind::ScalarAddress) {
        low = Binary(state, spv::OpBitwiseAnd, TypeU32(state), low, ConstantU32(state, ~3u));
    }
    if (mem.addressIsFull) {
        return DeviceAddressFromWords(state, low, ctx.Arg(inst, 2));
    }
    const IrValue* argument = inst.Argument(0);
    const IrValue* handle = argument != nullptr ? argument->Resolve() : nullptr;
    if (handle == nullptr || handle->Opcode() != IrOpcode::GetAddressResource || handle->ArgumentCount() != 2u) {
        ctx.Fail(inst, "has no address base pair");
    }
    const auto base = DeviceAddressFromWords(state, ctx.Arg(*handle, 0), ctx.Arg(*handle, 1));
    return AddBdaAddress(ctx, inst, base, Unary(state, spv::OpUConvert, TypeScalarU64(state), low), false);
}

std::uint32_t GuestAddress(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    const auto address = GuestAddressBase(ctx, inst, mem);
    auto immediate = static_cast<std::int32_t>(mem.offset);
    if (mem.kind == ResourceKind::ScalarAddress) {
        immediate = static_cast<std::int32_t>(static_cast<std::uint32_t>(immediate) & ~3u);
    }
    return AddBdaImmediate(ctx, inst, address, immediate);
}

std::uint32_t LoadBda(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t bits) {
    return EmitValueOrZeroIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        return EmitBdaRead(ctx, inst, GuestAddress(ctx, inst, mem), bits);
    });
}

IrBufferFormat BufferFormatOf(const SpirvEmitterState& state, const MemoryInfo& mem) {
    return mem.typed ? DecodeTBufferFormat(mem.dataFormat, mem.numberFormat) : StorageBufferFormat(state, mem);
}

SpirvBufferFormatInfo MemoryFormatInfo(const SpirvEmitterState& state, const MemoryInfo& mem) {
    return GetFormatInfo(mem.formatted ? BufferFormatOf(state, mem) : IrBufferFormat::Invalid);
}

MemoryInfo RebaseFormattedComponent(MemoryInfo mem, const SpirvBufferFormatInfo& info, std::uint32_t component) {
    mem.offset += GetFormatComponentByteOffset(info, component);
    mem.dataDwords = 1u;
    mem.componentIndex = component;
    return mem;
}

MemoryInfo RebaseRawComponent(MemoryInfo mem, std::uint32_t component) {
    mem.offset += component * 4u;
    mem.dataDwords = 1u;
    mem.componentIndex = component;
    return mem;
}

SpirvFormattedSource ResolveOutputSource(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const SpirvBufferFormatInfo& info, std::uint32_t outputComponent) {
    if (mem.typed) {
        if (outputComponent < info.componentCount) {
            return {SpirvFormattedSourceKind::Memory, outputComponent};
        }
        return {};
    }
    const auto swizzle = ctx.state.program.Info().buffers.at(mem.resource).descriptorSwizzle;
    return ResolveFormattedSource(info, (swizzle >> (outputComponent * 3u)) & 0x7u);
}

std::uint32_t FormattedConstant(SpirvValueEmitContext& ctx, const SpirvBufferFormatInfo& info, SpirvFormattedSourceKind kind) {
    return ConstantU32(ctx.state, FormattedConstantBits(info, kind));
}

template<typename TLoadWord, typename TLoadSubword>
std::uint32_t LoadFormattedComponent(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const SpirvBufferFormatInfo& info, std::uint32_t outputComponent, TLoadWord&& loadWord, TLoadSubword&& loadSubword) {
    auto& state = ctx.state;
    const auto source = ResolveOutputSource(ctx, mem, info, outputComponent);
    if (source.kind != SpirvFormattedSourceKind::Memory) {
        const bool integer = info.type == SpirvFormatComponentType::Uint || info.type == SpirvFormatComponentType::Sint;
        if (mem.d16 && source.kind == SpirvFormattedSourceKind::One && !integer) return ConstantU32(state, 0x3c00u);
        return FormattedConstant(ctx, info, source.kind);
    }
    const auto component = source.component;
    const auto bits = info.componentBits[component];
    const bool signedType = IsSignedFormatComponent(info.type);
    std::uint32_t raw = 0;
    if (info.packedBitfield) {
        raw = loadWord(component);
        const auto type = signedType ? TypeI32(state) : TypeU32(state);
        const auto sourceValue = signedType ? Unary(state, spv::OpBitcast, type, raw) : raw;
        const auto extracted = state.module.AllocateId();
        state.module.AddFunction(signedType ? spv::OpBitFieldSExtract : spv::OpBitFieldUExtract, type, extracted, sourceValue, ConstantU32(state, info.componentBitOffset[component]), ConstantU32(state, bits));
        raw = signedType ? Unary(state, spv::OpBitcast, TypeU32(state), extracted) : extracted;
    } else if (bits == 32u) {
        raw = loadWord(component);
    } else {
        raw = loadSubword(component, bits, signedType);
    }
    return mem.d16 ? EmitD16FormatComponent(state, info, component, raw) : NormalizeFormatComponent(state, info, component, raw);
}

std::uint32_t FormattedLoadPrepared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t outputComponent, const MemoryResourceAccess& resource) {
    const auto info = MemoryFormatInfo(ctx.state, mem);
    if (info.type == SpirvFormatComponentType::Unknown) {
        if (mem.d16) ctx.Fail(inst, "is a D16 format load with an unknown buffer format");
        return LoadWordPrepared(ctx, inst, RebaseRawComponent(mem, outputComponent), resource);
    }
    return LoadFormattedComponent(ctx, mem, info, outputComponent, [&](std::uint32_t component) {
        return LoadWordPrepared(ctx, inst, RebaseFormattedComponent(mem, info, component), resource);
    }, [&](std::uint32_t component, std::uint32_t bits, bool signExtend) {
        return LoadSubwordPrepared(ctx, inst, RebaseFormattedComponent(mem, info, component), resource, bits, signExtend);
    });
}

std::uint32_t FormattedLoad(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    return EmitValueOrZeroIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        return FormattedLoadPrepared(ctx, inst, mem, 0u, resource);
    });
}

void StoreWordInBounds(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t index, std::uint32_t data) {
    auto& state = ctx.state;
    if (resource.memoryAccess != 0u) state.module.AddFunction(spv::OpStore, EmitMemoryElementPointer(state, resource, index), data, resource.memoryAccess);
    else state.module.AddFunction(spv::OpStore, EmitMemoryElementPointer(state, resource, index), data);
}

void StoreSubwordInBounds(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const MemoryResourceAccess& resource, std::uint32_t address, std::uint32_t index, std::uint32_t bits, std::uint32_t data) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u) {
        ctx.Fail("subword store has an unsupported width");
    }
    const auto pointer = EmitMemoryElementPointer(state, resource, index);
    const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), address, ConstantU32(state, 3u)), ConstantU32(state, 3u));
    const auto fieldMask = ConstantU32(state, bits == 8u ? 0xffu : 0xffffu);
    const auto mask = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), fieldMask, shift);
    const auto value = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), data, fieldMask), shift);
    const auto merge = [&](std::uint32_t old) {
        return Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), old, Unary(state, spv::OpNot, TypeU32(state), mask)), value);
    };
    if (mem.kind == ResourceKind::Scratch) {
        const auto old = state.module.AllocateId();
        if (resource.memoryAccess != 0u) {
            state.module.AddFunction(spv::OpLoad, TypeU32(state), old, pointer, resource.memoryAccess);
            state.module.AddFunction(spv::OpStore, pointer, merge(old), resource.memoryAccess);
        } else {
            state.module.AddFunction(spv::OpLoad, TypeU32(state), old, pointer);
            state.module.AddFunction(spv::OpStore, pointer, merge(old));
        }
    } else {
        AtomicUpdate(state, pointer, mem.kind, merge);
    }
}

void StoreSubwordPrepared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource, std::uint32_t bits, std::uint32_t data) {
    auto& state = ctx.state;
    const auto address = ByteAddress(ctx, inst, mem);
    const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto index = EmitMemoryElementIndex(state, resource, rawIndex);
    EmitIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
        StoreSubwordInBounds(ctx, mem, resource, address, index, bits, data);
    });
}

void StoreSubword(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t bits) {
    EmitIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        StoreSubwordPrepared(ctx, inst, mem, resource, bits, ctx.Arg(inst, inst.ArgumentCount() - 2u));
    });
}

void StoreWordPrepared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource, std::uint32_t data) {
    auto& state = ctx.state;
    const auto index = EmitMemoryElementIndex(state, resource, DwordIndex(ctx, inst, mem));
    EmitIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
        StoreWordInBounds(ctx, resource, index, data);
    });
}

void StoreWord(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    EmitIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        StoreWordPrepared(ctx, inst, mem, resource, ctx.Arg(inst, inst.ArgumentCount() - 2u));
    });
}

struct PreparedFormattedMemory {
    SpirvBufferFormatInfo info;
    MemoryResourceAccess resource;
    std::array<std::uint32_t, 4> addresses{};
    std::array<std::uint32_t, 4> indices{};
    std::uint32_t inBounds = 0;
};

enum class FormattedAccess {
    Load,
    Store
};

PreparedFormattedMemory PrepareFormattedMemory(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource, const SpirvBufferFormatInfo& info, std::uint32_t components, FormattedAccess access) {
    auto& state = ctx.state;
    PreparedFormattedMemory plan;
    plan.info = info;
    plan.resource = resource;
    std::array<bool, 4> required{};
    if (access == FormattedAccess::Load) {
        for (std::uint32_t output = 0; output < components; output++) {
            const auto source = ResolveOutputSource(ctx, mem, plan.info, output);
            if (source.kind == SpirvFormattedSourceKind::Memory) {
                required.at(source.component) = true;
            }
        }
    } else {
        for (std::uint32_t component = 0; component < plan.info.componentCount; component++) {
            required.at(component) = true;
        }
    }
    bool firstBound = true;
    for (std::uint32_t component = 0; component < plan.info.componentCount; component++) {
        if (!required.at(component)) {
            continue;
        }
        const auto byteOffset = GetFormatComponentByteOffset(plan.info, component);
        bool reused = false;
        for (std::uint32_t previous = 0; previous < component; previous++) {
            if (required.at(previous) && GetFormatComponentByteOffset(plan.info, previous) == byteOffset) {
                plan.addresses.at(component) = plan.addresses.at(previous);
                plan.indices.at(component) = plan.indices.at(previous);
                reused = true;
                break;
            }
        }
        if (reused) {
            continue;
        }
        const auto componentMem = RebaseFormattedComponent(mem, plan.info, component);
        plan.addresses.at(component) = ByteAddress(ctx, inst, componentMem);
        const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), plan.addresses.at(component), ConstantU32(state, 2u));
        plan.indices.at(component) = EmitMemoryElementIndex(state, resource, rawIndex);
        const auto componentBound = EmitMemoryElementInBounds(state, resource, plan.indices.at(component));
        if (firstBound) {
            plan.inBounds = componentBound;
            firstBound = false;
        } else {
            plan.inBounds = AndCondition(state, plan.inBounds, componentBound);
        }
    }
    if (firstBound) {
        plan.inBounds = ConstantBool(state, true);
    }
    return plan;
}

std::uint32_t LoadFormattedInBounds(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const PreparedFormattedMemory& plan, std::uint32_t outputComponent) {
    return LoadFormattedComponent(ctx, mem, plan.info, outputComponent, [&](std::uint32_t component) {
        return LoadWordInBounds(ctx, plan.resource, plan.indices.at(component));
    }, [&](std::uint32_t component, std::uint32_t bits, bool signExtend) {
        return LoadSubwordInBounds(ctx, plan.resource, plan.addresses.at(component), plan.indices.at(component), bits, signExtend);
    });
}

std::uint32_t ConstructU32Composite(SpirvEmitterState& state, std::uint32_t components, const std::array<std::uint32_t, 4>& values) {
    const auto result = state.module.AllocateId();
    std::vector<std::uint32_t> words{spv::OpCompositeConstruct, TypeU32Composite(state, components), result};
    words.insert(words.end(), values.begin(), values.begin() + components);
    state.module.AddFunction(words);
    return result;
}

std::uint32_t FormattedOutOfBoundsValue(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const PreparedFormattedMemory& plan, std::uint32_t components) {
    auto& state = ctx.state;
    std::array<std::uint32_t, 4> values{};
    for (std::uint32_t component = 0; component < components; component++) {
        const auto source = ResolveOutputSource(ctx, mem, plan.info, component);
        values.at(component) = source.kind == SpirvFormattedSourceKind::Memory ? ConstantU32(state, 0u) : FormattedConstant(ctx, plan.info, source.kind);
    }
    return ConstructU32Composite(state, components, values);
}

void StoreFormattedElement(SpirvValueEmitContext& ctx, const MemoryInfo& mem, const PreparedFormattedMemory& plan, const std::array<std::uint32_t, 4>& data, std::uint32_t components) {
    auto& state = ctx.state;
    const auto& info = plan.info;
    std::array<std::uint32_t, 4> encoded{};
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        encoded.at(component) = component >= components ? ConstantU32(state, 0u) : mem.d16 ? EmitD16StoreComponent(state, info, component, data.at(component)) : EmitFormatStoreComponent(state, info, component, data.at(component));
    }
    if (info.packedBitfield) {
        auto word = ConstantU32(state, 0u);
        for (std::uint32_t component = 0; component < info.componentCount; component++) {
            const std::uint32_t mask = (1u << info.componentBits[component]) - 1u;
            word = EmitOrU32(state, word, EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitAndConstant(state, encoded.at(component), mask), ConstantU32(state, info.componentBitOffset[component])));
        }
        StoreWordInBounds(ctx, plan.resource, plan.indices.at(0), word);
        return;
    }
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto bits = info.componentBits[component];
        if (bits == 8u || bits == 16u) {
            StoreSubwordInBounds(ctx, mem, plan.resource, plan.addresses.at(component), plan.indices.at(component), bits, encoded.at(component));
        } else {
            StoreWordInBounds(ctx, plan.resource, plan.indices.at(component), encoded.at(component));
        }
    }
}

void StoreFormattedValues(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, const MemoryResourceAccess& resource, const SpirvBufferFormatInfo& info, const std::array<std::uint32_t, 4>& data, std::uint32_t components) {
    const auto plan = PrepareFormattedMemory(ctx, inst, mem, resource, info, components, FormattedAccess::Store);
    EmitIfCondition(ctx.state, plan.inBounds, [&]() {
        StoreFormattedElement(ctx, mem, plan, data, components);
    });
}

void FormattedStore(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    EmitIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
        const auto info = MemoryFormatInfo(ctx.state, mem);
        const auto data = ctx.Arg(inst, inst.ArgumentCount() - 2u);
        if (info.type == SpirvFormatComponentType::Unknown) {
            if (mem.d16) ctx.Fail(inst, "is a D16 format store with an unknown buffer format");
            StoreWordPrepared(ctx, inst, RebaseRawComponent(mem, 0u), resource, data);
            return;
        }
        StoreFormattedValues(ctx, inst, mem, resource, info, {data, 0u, 0u, 0u}, 1u);
    });
}

std::uint32_t LoadWideBuffer(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components) {
    auto& state = ctx.state;
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU32Composite(state, components), ConstantU32CompositeZero(state, components), [&]() {
        const auto resource = PrepareMemoryResourceAccess(state, mem);
        const auto info = MemoryFormatInfo(state, mem);
        if (info.type != SpirvFormatComponentType::Unknown) {
            const auto plan = PrepareFormattedMemory(ctx, inst, mem, resource, info, components, FormattedAccess::Load);
            return EmitValueOrDefaultIfCondition(state, plan.inBounds, TypeU32Composite(state, components), FormattedOutOfBoundsValue(ctx, mem, plan, components), [&]() {
                std::array<std::uint32_t, 4> values{};
                for (std::uint32_t component = 0; component < components; component++) {
                    values.at(component) = LoadFormattedInBounds(ctx, mem, plan, component);
                }
                return ConstructU32Composite(state, components, values);
            });
        }
        std::array<std::uint32_t, 4> values{};
        for (std::uint32_t component = 0; component < components; component++) {
            values.at(component) = LoadWordPrepared(ctx, inst, RebaseRawComponent(mem, component), resource);
        }
        return ConstructU32Composite(state, components, values);
    });
}

void StoreWideBuffer(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components) {
    auto& state = ctx.state;
    EmitIfCondition(state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(state, mem);
        const auto composite = ctx.Arg(inst, inst.ArgumentCount() - 2u);
        const auto info = MemoryFormatInfo(state, mem);
        if (info.type != SpirvFormatComponentType::Unknown) {
            std::array<std::uint32_t, 4> data{};
            for (std::uint32_t component = 0; component < components; component++) {
                data.at(component) = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), data.at(component), composite, component);
            }
            StoreFormattedValues(ctx, inst, mem, resource, info, data, components);
            return;
        }
        if (mem.d16) ctx.Fail(inst, "is a D16 format store with an unknown buffer format");
        for (std::uint32_t component = 0; component < components; component++) {
            const auto data = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), data, composite, component);
            StoreWordPrepared(ctx, inst, RebaseRawComponent(mem, component), resource, data);
        }
    });
}

std::uint32_t LoadWideShared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components) {
    auto& state = ctx.state;
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU32Composite(state, components), ConstantU32CompositeZero(state, components), [&]() {
        const auto resource = PrepareMemoryResourceAccess(state, mem);
        const auto base = ByteAddress(ctx, inst, mem);
        std::array<std::uint32_t, 4> values{};
        for (std::uint32_t component = 0; component < components; component++) {
            const auto address = component == 0u ? base : Binary(state, spv::OpIAdd, TypeU32(state), base, ConstantU32(state, component * 4u));
            const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
            const auto index = EmitMemoryElementIndex(state, resource, rawIndex);
            values.at(component) = EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
                return LoadWordInBounds(ctx, resource, index);
            });
        }
        return ConstructU32Composite(state, components, values);
    });
}

void StoreWideShared(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components) {
    auto& state = ctx.state;
    EmitIfCondition(state, ActiveArgument(ctx, inst), [&]() {
        const auto resource = PrepareMemoryResourceAccess(state, mem);
        const auto base = ByteAddress(ctx, inst, mem);
        for (std::uint32_t component = 0; component < components; component++) {
            const auto address = component == 0u ? base : Binary(state, spv::OpIAdd, TypeU32(state), base, ConstantU32(state, component * 4u));
            const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
            const auto index = EmitMemoryElementIndex(state, resource, rawIndex);
            EmitIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&]() {
                StoreWordInBounds(ctx, resource, index, ctx.Arg(inst, component + 1u));
            });
        }
    });
}

const MemoryInfo& BufferMemory(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto& mem = ctx.Memory(inst);
    if (mem.kind != ResourceKind::Buffer) {
        ctx.Fail(inst, "must access a buffer resource");
    }
    if (mem.gpuDescriptor) {
        ctx.Fail(inst, "accesses a GPU-selected V# in a way only raw dword loads and stores support");
    }
    return mem;
}

bool EmitGpuDescriptorAccess(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t components, bool store) {
    if (!ctx.Memory(inst).gpuDescriptor) {
        return false;
    }
    auto& state = ctx.state;
    if (store) {
        EmitIfCondition(state, ActiveArgument(ctx, inst), [&] { StoreGpuDescriptor(ctx, inst, components); });
        return true;
    }
    const auto type = components == 1u ? TypeU32(state) : TypeU32Composite(state, components);
    const auto zero = components == 1u ? ConstantU32(state, 0u) : ConstantU32CompositeZero(state, components);
    ctx.Define(inst, EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), type, zero, [&] { return LoadGpuDescriptor(ctx, inst, components); }));
    return true;
}

const MemoryInfo& SharedMemory(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto& mem = ctx.Memory(inst);
    if (mem.kind != ResourceKind::Lds && mem.kind != ResourceKind::Gds) {
        ctx.Fail(inst, "must access LDS or GDS");
    }
    return mem;
}

void LoadAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t bits) {
    const auto& mem = ctx.Memory(inst);
    if (bits == 32u && mem.planningOnly) {
        return;
    }
    switch (mem.kind) {
    case ResourceKind::Scratch:
        ctx.Define(inst, bits == 32u ? LoadWord(ctx, inst, mem) : LoadSubword(ctx, inst, mem, bits));
        return;
    case ResourceKind::ScalarAddress:
    case ResourceKind::Flat:
    case ResourceKind::Global:
        ctx.Define(inst, LoadBda(ctx, inst, mem, bits));
        return;
    default:
        ctx.Fail(inst, "must read a scratch or physical address resource");
    }
}

std::uint32_t LoadBdaWide(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, std::uint32_t components) {
    auto& state = ctx.state;
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU32Composite(state, components), ConstantU32CompositeZero(state, components), [&]() {
        return ConstructU32Composite(state, components, EmitBdaDwordReads(ctx, inst, GuestAddressBase(ctx, inst, mem), mem.offset, components));
    });
}

void LoadAddressWide(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t components) {
    const auto& mem = ctx.Memory(inst);
    if (mem.planningOnly) {
        return;
    }
    switch (mem.kind) {
    case ResourceKind::Flat:
    case ResourceKind::Global:
        ctx.Define(inst, LoadBdaWide(ctx, inst, mem, components));
        return;
    default:
        ctx.Fail(inst, "must read a physical address resource");
    }
}

void StoreAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t bits) {
    const auto& mem = ctx.Memory(inst);
    switch (mem.kind) {
    case ResourceKind::Scratch:
        if (bits == 32u) {
            StoreWord(ctx, inst, mem);
        } else {
            StoreSubword(ctx, inst, mem, bits);
        }
        return;
    case ResourceKind::Flat:
    case ResourceKind::Global:
        EmitIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
            EmitBdaStore(ctx, inst, GuestAddress(ctx, inst, mem), ctx.Arg(inst, inst.ArgumentCount() - 2u), bits);
        });
        return;
    default:
        ctx.Fail(inst, "must write a scratch or physical address resource");
    }
}

struct PreparedMemoryElement {
    MemoryResourceAccess resource;
    std::uint32_t index = 0;
};

PreparedMemoryElement PrepareMemoryElement(SpirvValueEmitContext& ctx, const MemoryInfo& mem, std::uint32_t rawIndex) {
    const auto resource = PrepareMemoryResourceAccess(ctx.state, mem);
    return {resource, EmitMemoryElementIndex(ctx.state, resource, rawIndex)};
}

std::uint32_t SpirvAtomicOpcode(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::BufferAtomicCmpSwap32:
        return spv::OpAtomicCompareExchange;
    case IrOpcode::BufferAtomicSwap32:
    case IrOpcode::BufferAtomicSwap64:
    case IrOpcode::SharedAtomicSwap32:
        return spv::OpAtomicExchange;
    case IrOpcode::BufferAtomicIAdd32:
    case IrOpcode::SharedAtomicIAdd32:
    case IrOpcode::BufferAtomicIAdd64:
        return spv::OpAtomicIAdd;
    case IrOpcode::BufferAtomicISub32:
    case IrOpcode::SharedAtomicISub32:
    case IrOpcode::BufferAtomicISub64:
        return spv::OpAtomicISub;
    case IrOpcode::BufferAtomicSMin32:
    case IrOpcode::SharedAtomicSMin32:
    case IrOpcode::BufferAtomicSMin64:
        return spv::OpAtomicSMin;
    case IrOpcode::BufferAtomicUMin32:
    case IrOpcode::SharedAtomicUMin32:
    case IrOpcode::BufferAtomicUMin64:
        return spv::OpAtomicUMin;
    case IrOpcode::BufferAtomicSMax32:
    case IrOpcode::SharedAtomicSMax32:
    case IrOpcode::BufferAtomicSMax64:
        return spv::OpAtomicSMax;
    case IrOpcode::BufferAtomicUMax32:
    case IrOpcode::SharedAtomicUMax32:
    case IrOpcode::BufferAtomicUMax64:
        return spv::OpAtomicUMax;
    case IrOpcode::BufferAtomicAnd32:
    case IrOpcode::SharedAtomicAnd32:
    case IrOpcode::BufferAtomicAnd64:
        return spv::OpAtomicAnd;
    case IrOpcode::BufferAtomicOr32:
    case IrOpcode::BufferAtomicOr64:
    case IrOpcode::SharedAtomicOr32:
        return spv::OpAtomicOr;
    case IrOpcode::BufferAtomicXor32:
    case IrOpcode::SharedAtomicXor32:
    case IrOpcode::BufferAtomicXor64:
        return spv::OpAtomicXor;
    default:
        throw std::runtime_error("SpirvAtomicOpcode: opcode has no SPIR-V atomic instruction");
    }
}

std::uint32_t EmitAtomicOperation(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t pointer, std::uint32_t scope) {
    auto& state = ctx.state;
    const auto old = state.module.AllocateId();
    if (inst.Opcode() == IrOpcode::BufferAtomicCmpSwap32) {
        const auto desired = ctx.Arg(inst, inst.ArgumentCount() - 3u);
        const auto comparator = ctx.Arg(inst, inst.ArgumentCount() - 2u);
        state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), old, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), desired, comparator);
    } else {
        const auto value = ctx.Arg(inst, inst.ArgumentCount() - 2u);
        state.module.AddFunction(SpirvAtomicOpcode(inst.Opcode()), TypeU32(state), old, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
    }
    return old;
}

template<typename TOperation>
std::uint32_t EmitAtomicAccess(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, TOperation&& operation) {
    auto& state = ctx.state;
    return EmitValueOrZeroIfCondition(state, ActiveArgument(ctx, inst), [&]() {
        const auto access = PrepareMemoryElement(ctx, mem, DwordIndex(ctx, inst, mem));
        return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, access.resource, access.index), [&]() {
            return operation(EmitMemoryElementPointer(state, access.resource, access.index));
        });
    });
}

template<typename TReplacement>
std::uint32_t EmitAtomicUpdate(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem, TReplacement&& replacement) {
    const auto value = ctx.Arg(inst, inst.ArgumentCount() - 2u);
    return EmitAtomicAccess(ctx, inst, mem, [&](std::uint32_t pointer) {
        return AtomicUpdate(ctx.state, pointer, mem.kind, [&](std::uint32_t old) {
            return replacement(ctx.state, old, value);
        });
    });
}

// Buffer atomics whose operand is the identity element (add, sub, or, xor of 0) leave memory
// unchanged. Tile-classification kernels issue one such atomic per bin per wave with a mostly zero
// count, and every atomic on a host-imported range is a serialized PCIe round trip (Demon's Souls
// 0x…88aa800 / 0x…88ab300: 24 + 29 ms of GPU per frame for ~60K atomics each). An unused result
// skips the operation; a used one takes a relaxed atomic load of the current value instead, which is
// what the no-op update would have returned. APS5_NO_ATOMIC_ZERO_SKIP=1 restores the plain atomics.
bool HasZeroIdentity(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::BufferAtomicIAdd32:
    case IrOpcode::BufferAtomicISub32:
    case IrOpcode::BufferAtomicOr32:
    case IrOpcode::BufferAtomicXor32:
        return true;
    default:
        return false;
    }
}

bool AtomicZeroSkipEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ATOMIC_ZERO_SKIP") != nullptr;
    return !disabled;
}

// Emission sites by kind under APS5_PROFILE_DRAW (a two-lane wave64 program counts each site twice).
// Printing is emission-driven: the line appears with the first site compiled at least 10 s after the
// previous line (the first line 10 s after the first site), so it lags the true totals until the
// next compile that emits a buffer atomic; the last sites of a run may never be printed.
void NoteBufferAtomicSite(bool zeroSkip, bool zeroLoad) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    static std::atomic<std::uint64_t> plain{0}, skipped{0}, loads{0};
    static std::atomic<std::int64_t> lastReport{0};
    (zeroSkip ? skipped : zeroLoad ? loads : plain).fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = lastReport.load(std::memory_order_relaxed);
    if (last == 0) {
        // First site: start the 10 s window instead of printing a one-site line.
        lastReport.compare_exchange_strong(last, now);
        return;
    }
    if (now - last < 10 || !lastReport.compare_exchange_strong(last, now)) return;
    std::fprintf(stderr, "[recompile] buffer atomic sites: %llu plain, %llu zero-skipped (unused result), %llu zero-load\n", static_cast<unsigned long long>(plain.load()), static_cast<unsigned long long>(skipped.load()), static_cast<unsigned long long>(loads.load()));
}

std::uint32_t Atomic32(SpirvValueEmitContext& ctx, const IrValue& inst, const MemoryInfo& mem) {
    auto& state = ctx.state;
    const bool lds = mem.kind == ResourceKind::Lds;
    const std::uint32_t scope = lds ? spv::ScopeWorkgroup : spv::ScopeDevice;
    // Buffer atomic arguments are {resource, index, offset, soffset, value, exec}. A non-zero
    // immediate operand (counters bumped by 1) can never take the zero path, so it keeps the plain
    // atomic instead of a compare and a dead branch the main build has no optimizer to fold.
    const auto nonZeroImmediateOperand = [&]() {
        const IrValue* operand = inst.Argument(inst.ArgumentCount() - 2u)->Resolve();
        return operand->HasImmediate() && operand->Type() == IrType::U32 && operand->ImmediateU32() != 0u;
    };
    const bool zeroIdentity = !lds && AtomicZeroSkipEnabled() && HasZeroIdentity(inst.Opcode()) && !nonZeroImmediateOperand();
    const bool zeroSkip = zeroIdentity && !inst.HasUses();
    std::uint32_t active = ActiveArgument(ctx, inst);
    std::uint32_t nonZero = 0;
    if (zeroIdentity) {
        nonZero = Binary(state, spv::OpINotEqual, TypeBool(state), ctx.Arg(inst, inst.ArgumentCount() - 2u), ConstantU32(state, 0u));
        if (zeroSkip) active = AndCondition(state, active, nonZero);
    }
    if (mem.kind == ResourceKind::Buffer) NoteBufferAtomicSite(zeroSkip, zeroIdentity && !zeroSkip);
    return EmitValueOrZeroIfCondition(state, active, [&]() {
        const auto access = PrepareMemoryElement(ctx, mem, DwordIndex(ctx, inst, mem));
        return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, access.resource, access.index), [&]() {
            const auto pointer = EmitMemoryElementPointer(state, access.resource, access.index);
            const auto operation = [&]() {
                const auto old = EmitAtomicOperation(ctx, inst, pointer, scope);
                if (lds) {
                    const std::uint32_t semantics = spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask;
                    state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, scope), ConstantU32(state, semantics));
                } else {
                    EmitDeviceAtomicMemoryBarrier(state);
                }
                return old;
            };
            if (!zeroIdentity || zeroSkip) return operation();
            return EmitValueIfElse(state, nonZero, TypeU32(state), operation, [&]() {
                // The barrier stays so an "add 0" used as a coherent read keeps its acquire.
                const auto current = state.module.AllocateId();
                state.module.AddFunction(spv::OpAtomicLoad, TypeU32(state), current, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone));
                EmitDeviceAtomicMemoryBarrier(state);
                return current;
            });
        });
    });
}

std::uint32_t BufferAtomic64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto& mem = BufferMemory(ctx, inst);
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU64(state), ConstantU64(state, 0u), [&]() {
        const auto resource = PrepareStorageBufferResourceAccess(state, mem, state.storageBufferU64Variable, TypeStorageBufferU64Pointer(state));
        const auto byteAddress = Binary(state, spv::OpIAdd, TypeU32(state), ByteAddress(ctx, inst, mem), resource.byteOffset);
        const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteAddress, ConstantU32(state, 3u));
        return EmitValueOrDefaultIfCondition(state, EmitMemoryElementInBounds(state, resource, index), TypeU64(state), ConstantU64(state, 0u), [&]() {
            const auto value = Unary(state, spv::OpBitcast, TypeScalarU64(state), ctx.Arg(inst, inst.ArgumentCount() - 2u));
            const auto old = state.module.AllocateId();
            const auto pointer = EmitStorageBufferElementPointer(state, resource, index, TypeStorageBufferU64ElementPointer(state));
            if (inst.Opcode() == IrOpcode::BufferAtomicCmpSwap64) {
                const auto desired = Unary(state, spv::OpBitcast, TypeScalarU64(state), ctx.Arg(inst, inst.ArgumentCount() - 3u));
                state.module.AddFunction(spv::OpAtomicCompareExchange, TypeScalarU64(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), desired, value);
            } else {
                state.module.AddFunction(SpirvAtomicOpcode(inst.Opcode()), TypeScalarU64(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
            }
            EmitDeviceAtomicMemoryBarrier(state);
            return Unary(state, spv::OpBitcast, TypeU64(state), old);
        });
    });
}

struct AtomicFloatBits {
    SpirvEmitterState& state;
    bool wide;
    std::uint32_t type() const { return wide ? TypeScalarU64(state) : TypeU32(state); }
    std::uint32_t constant(std::uint64_t value) const {
        return wide ? state.module.Constant(spv::OpConstant, TypeScalarU64(state), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u)) : ConstantU32(state, static_cast<std::uint32_t>(value));
    }
    std::uint64_t sign() const { return wide ? 0x8000000000000000ull : 0x80000000ull; }
    std::uint64_t quiet() const { return wide ? 0x0008000000000000ull : 0x00400000ull; }
    std::uint64_t infinity() const { return wide ? 0x7ff0000000000000ull : 0x7f800000ull; }
    std::uint32_t op(spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) const { return Binary(state, opcode, type(), lhs, rhs); }
    std::uint32_t test(spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) const { return Binary(state, opcode, TypeBool(state), lhs, rhs); }
    std::uint32_t select(std::uint32_t condition, std::uint32_t lhs, std::uint32_t rhs) const { return Select(state, type(), condition, lhs, rhs); }
    std::uint32_t magnitude(std::uint32_t value) const { return op(spv::OpBitwiseAnd, value, constant(sign() - 1u)); }
    std::uint32_t nan(std::uint32_t value) const { return test(spv::OpUGreaterThan, magnitude(value), constant(infinity())); }
    std::uint32_t signalingNan(std::uint32_t value) const {
        return Binary(state, spv::OpLogicalAnd, TypeBool(state), nan(value), test(spv::OpIEqual, op(spv::OpBitwiseAnd, value, constant(quiet())), constant(0u)));
    }
    std::uint32_t orderKey(std::uint32_t value) const {
        const auto negative = test(spv::OpINotEqual, op(spv::OpBitwiseAnd, value, constant(sign())), constant(0u));
        return select(negative, Unary(state, spv::OpNot, type(), value), op(spv::OpBitwiseOr, value, constant(sign())));
    }
    std::uint32_t equal(std::uint32_t lhs, std::uint32_t rhs) const {
        const auto same = Binary(state, spv::OpLogicalAnd, TypeBool(state), test(spv::OpIEqual, lhs, rhs), Unary(state, spv::OpLogicalNot, TypeBool(state), nan(lhs)));
        const auto zeros = test(spv::OpIEqual, magnitude(op(spv::OpBitwiseOr, lhs, rhs)), constant(0u));
        return Binary(state, spv::OpLogicalOr, TypeBool(state), same, zeros);
    }
    std::uint32_t minMax(std::uint32_t old, std::uint32_t source, bool maxValue) const {
        const auto better = test(maxValue ? spv::OpUGreaterThan : spv::OpULessThan, orderKey(source), orderKey(old));
        auto result = select(better, source, old);
        result = select(nan(old), source, result);
        result = select(nan(source), old, result);
        result = select(Binary(state, spv::OpLogicalAnd, TypeBool(state), nan(source), nan(old)), source, result);
        result = select(signalingNan(old), op(spv::OpBitwiseOr, old, constant(quiet())), result);
        return select(signalingNan(source), op(spv::OpBitwiseOr, source, constant(quiet())), result);
    }
    std::uint32_t compareSwap(std::uint32_t old, std::uint32_t comparator, std::uint32_t desired) const {
        return select(equal(old, comparator), desired, old);
    }
    std::uint32_t increment(std::uint32_t old, std::uint32_t limit) const {
        return select(test(spv::OpUGreaterThanEqual, old, limit), constant(0u), op(spv::OpIAdd, old, constant(1u)));
    }
    std::uint32_t decrement(std::uint32_t old, std::uint32_t limit) const {
        const auto wrap = Binary(state, spv::OpLogicalOr, TypeBool(state), test(spv::OpIEqual, old, constant(0u)), test(spv::OpUGreaterThan, old, limit));
        return select(wrap, limit, op(spv::OpISub, old, constant(1u)));
    }
};

std::uint32_t BufferFloatAtomic(SpirvValueEmitContext& ctx, const IrValue& inst, bool maxValue) {
    return EmitAtomicUpdate(ctx, inst, BufferMemory(ctx, inst), [maxValue](SpirvEmitterState& state, std::uint32_t old, std::uint32_t value) {
        return AtomicFloatBits{state, false}.minMax(old, value, maxValue);
    });
}

template<typename TReplacement>
std::uint32_t BufferAtomic64Update(SpirvValueEmitContext& ctx, const IrValue& inst, TReplacement&& replacement) {
    auto& state = ctx.state;
    const auto& mem = BufferMemory(ctx, inst);
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU64(state), ConstantU64(state, 0u), [&]() {
        const auto resource = PrepareStorageBufferResourceAccess(state, mem, state.storageBufferU64Variable, TypeStorageBufferU64Pointer(state));
        const auto byteAddress = Binary(state, spv::OpIAdd, TypeU32(state), ByteAddress(ctx, inst, mem), resource.byteOffset);
        const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteAddress, ConstantU32(state, 3u));
        return EmitValueOrDefaultIfCondition(state, EmitMemoryElementInBounds(state, resource, index), TypeU64(state), ConstantU64(state, 0u), [&]() {
            const auto pointer = EmitStorageBufferElementPointer(state, resource, index, TypeStorageBufferU64ElementPointer(state));
            const auto old = AtomicUpdateTyped(state, pointer, ResourceKind::Buffer, TypeScalarU64(state), [&](std::uint32_t observed) {
                return replacement(observed);
            });
            return Unary(state, spv::OpBitcast, TypeU64(state), old);
        });
    });
}

std::uint32_t ScalarU64Argument(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t fromEnd) {
    return Unary(ctx.state, spv::OpBitcast, TypeScalarU64(ctx.state), ctx.Arg(inst, inst.ArgumentCount() - fromEnd));
}

template<typename TReplacement>
std::uint32_t SharedAtomicUpdate(SpirvValueEmitContext& ctx, const IrValue& inst, TReplacement&& replacement) {
    auto& state = ctx.state;
    const auto& mem = SharedMemory(ctx, inst);
    return EmitAtomicAccess(ctx, inst, mem, [&](std::uint32_t pointer) {
        const auto old = AtomicUpdate(state, pointer, mem.kind, [&](std::uint32_t current) { return replacement(state, current); });
        const std::uint32_t semantics = spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask;
        state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, semantics));
        return old;
    });
}

std::uint32_t SharedFloatMinMax(SpirvValueEmitContext& ctx, const IrValue& inst, bool maxValue) {
    const auto data = ctx.Arg(inst, 1);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto oldFloat = Unary(state, spv::OpBitcast, TypeF32(state), old);
        const auto dataFloat = Unary(state, spv::OpBitcast, TypeF32(state), data);
        const auto oldNan = Unary(state, spv::OpIsNan, TypeBool(state), oldFloat);
        const auto dataNan = Unary(state, spv::OpIsNan, TypeBool(state), dataFloat);
        const auto better = Binary(state, maxValue ? spv::OpFOrdGreaterThan : spv::OpFOrdLessThan, TypeBool(state), dataFloat, oldFloat);
        const auto magnitudes = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpBitwiseOr, TypeU32(state), old, data), ConstantU32(state, 0x7fffffffu));
        const auto zeros = Binary(state, spv::OpIEqual, TypeBool(state), magnitudes, ConstantU32(state, 0u));
        const auto zero = Binary(state, maxValue ? spv::OpBitwiseAnd : spv::OpBitwiseOr, TypeU32(state), old, data);
        auto result = Select(state, TypeU32(state), better, data, old);
        result = Select(state, TypeU32(state), zeros, zero, result);
        result = Select(state, TypeU32(state), dataNan, old, result);
        return Select(state, TypeU32(state), oldNan, data, result);
    });
}

std::uint32_t AddressAtomicOpcode(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::AddressAtomicSwap32:
    case IrOpcode::AddressAtomicSwap64:
        return spv::OpAtomicExchange;
    case IrOpcode::AddressAtomicCmpSwap32:
    case IrOpcode::AddressAtomicCmpSwap64:
        return spv::OpAtomicCompareExchange;
    case IrOpcode::AddressAtomicIAdd32:
    case IrOpcode::AddressAtomicIAdd64:
        return spv::OpAtomicIAdd;
    case IrOpcode::AddressAtomicISub32:
    case IrOpcode::AddressAtomicISub64:
        return spv::OpAtomicISub;
    case IrOpcode::AddressAtomicSMin32:
    case IrOpcode::AddressAtomicSMin64:
        return spv::OpAtomicSMin;
    case IrOpcode::AddressAtomicUMin32:
    case IrOpcode::AddressAtomicUMin64:
        return spv::OpAtomicUMin;
    case IrOpcode::AddressAtomicSMax32:
    case IrOpcode::AddressAtomicSMax64:
        return spv::OpAtomicSMax;
    case IrOpcode::AddressAtomicUMax32:
    case IrOpcode::AddressAtomicUMax64:
        return spv::OpAtomicUMax;
    case IrOpcode::AddressAtomicAnd32:
    case IrOpcode::AddressAtomicAnd64:
        return spv::OpAtomicAnd;
    case IrOpcode::AddressAtomicOr32:
    case IrOpcode::AddressAtomicOr64:
        return spv::OpAtomicOr;
    case IrOpcode::AddressAtomicXor32:
    case IrOpcode::AddressAtomicXor64:
        return spv::OpAtomicXor;
    default:
        throw std::runtime_error("AddressAtomicOpcode: opcode has no SPIR-V atomic instruction");
    }
}

std::uint32_t AddressAtomic(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto& mem = ctx.Memory(inst);
    if (mem.kind != ResourceKind::Flat && mem.kind != ResourceKind::Global) {
        ctx.Fail(inst, "must access a physical address resource");
    }
    const bool wide = inst.Type() == IrType::U64;
    const auto type = wide ? TypeU64(state) : TypeU32(state);
    const auto zero = wide ? ConstantU64(state, 0u) : ConstantU32(state, 0u);
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), type, zero, [&]() {
        const auto scalarType = wide ? TypeScalarU64(state) : TypeU32(state);
        const auto scalar = [&](std::uint32_t value) { return wide ? Unary(state, spv::OpBitcast, scalarType, value) : value; };
        const auto value = scalar(ctx.Arg(inst, 3));
        const auto old = EmitBdaAtomic(ctx, inst, GuestAddress(ctx, inst, mem), wide ? 8u : 4u, [&](std::uint32_t pointer) {
            if (inst.Opcode() == IrOpcode::AddressAtomicInc32 || inst.Opcode() == IrOpcode::AddressAtomicDec32) {
                const bool increment = inst.Opcode() == IrOpcode::AddressAtomicInc32;
                return AtomicUpdate(state, pointer, mem.kind, [&](std::uint32_t current) {
                    return increment ? AtomicIncrement(state, current, value) : AtomicDecrement(state, current, value);
                });
            }
            if (inst.Opcode() == IrOpcode::AddressAtomicUSubSat32) {
                return AtomicUpdate(state, pointer, mem.kind, [&](std::uint32_t current) { return AtomicUSubSat(state, current, value); });
            }
            const AtomicFloatBits bits{state, wide};
            const auto update = [&](auto&& replacement) { return AtomicUpdateTyped(state, pointer, mem.kind, scalarType, replacement); };
            switch (inst.Opcode()) {
            case IrOpcode::AddressAtomicFCmpSwap32:
            case IrOpcode::AddressAtomicFCmpSwap64: {
                const auto comparator = scalar(ctx.Arg(inst, 4));
                return update([&](std::uint32_t current) { return bits.compareSwap(current, comparator, value); });
            }
            case IrOpcode::AddressAtomicFMin32:
            case IrOpcode::AddressAtomicFMin64:
                return update([&](std::uint32_t current) { return bits.minMax(current, value, false); });
            case IrOpcode::AddressAtomicFMax32:
            case IrOpcode::AddressAtomicFMax64:
                return update([&](std::uint32_t current) { return bits.minMax(current, value, true); });
            case IrOpcode::AddressAtomicInc64:
                return update([&](std::uint32_t current) { return bits.increment(current, value); });
            case IrOpcode::AddressAtomicDec64:
                return update([&](std::uint32_t current) { return bits.decrement(current, value); });
            default:
                break;
            }
            const auto scope = ConstantU32(state, spv::ScopeDevice);
            const auto semantics = ConstantU32(state, spv::MemorySemanticsMaskNone);
            const auto opcode = AddressAtomicOpcode(inst.Opcode());
            const auto result = state.module.AllocateId();
            if (opcode == spv::OpAtomicCompareExchange) {
                state.module.AddFunction(opcode, scalarType, result, pointer, scope, semantics, semantics, value, scalar(ctx.Arg(inst, 4)));
            } else {
                state.module.AddFunction(opcode, scalarType, result, pointer, scope, semantics, value);
            }
            EmitDeviceAtomicMemoryBarrier(state);
            return result;
        });
        return wide ? Unary(state, spv::OpBitcast, type, old) : old;
    });
}

std::uint32_t AppendConsume(SpirvValueEmitContext& ctx, const IrValue& inst, bool append) {
    auto& state = ctx.state;
    if (ctx.half == 1u) {
        if (ctx.otherHalf == nullptr) {
            ctx.Fail(inst, "has no first lane half to read the result from");
        }
        return ctx.otherHalf->Def(&inst);
    }
    const auto& mem = SharedMemory(ctx, inst);
    const bool wave64 = state.laneCount == 2u;
    const auto m0 = ctx.Arg(inst, 0);
    const auto base = Binary(state, spv::OpShiftRightLogical, TypeU32(state), m0, ConstantU32(state, 16u));
    const auto size = Binary(state, spv::OpBitwiseAnd, TypeU32(state), m0, ConstantU32(state, 0xffffu));
    const auto address = Binary(state, spv::OpIAdd, TypeU32(state), base, ConstantU32(state, mem.offset));
    const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto access = PrepareMemoryResourceAccess(state, mem);
    const auto index = EmitMemoryElementIndex(state, access, rawIndex);
    const auto exec = ctx.Arg(inst, 1);
    const auto ballot = ctx.Ballot(inst.Argument(1));
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1u);
    const auto count = Binary(state, spv::OpIAdd, TypeU32(state), Unary(state, spv::OpBitCount, TypeU32(state), low), Unary(state, spv::OpBitCount, TypeU32(state), high));
    const auto first = ctx.FirstLane(ballot);
    const auto sourceLane = wave64 ? Binary(state, spv::OpBitwiseAnd, TypeU32(state), first, ConstantU32(state, 31u)) : first;
    const auto isFirst = Binary(state, spv::OpIEqual, TypeBool(state), EmitSubgroupLocalInvocationId(state), sourceLane);
    const auto storageBounds = EmitMemoryElementInBounds(state, access, index);
    const auto m0Bounds = mem.kind == ResourceKind::Gds ? Binary(state, spv::OpINotEqual, TypeBool(state), size, ConstantU32(state, 0u)) : Binary(state, spv::OpULessThan, TypeBool(state), ConstantU32(state, mem.offset + 3u), size);
    const auto lanesActive = wave64 ? Binary(state, spv::OpINotEqual, TypeBool(state), count, ConstantU32(state, 0u)) : exec;
    const auto condition = AndCondition(state, isFirst, AndCondition(state, lanesActive, AndCondition(state, storageBounds, m0Bounds)));
    const auto atomic = EmitValueOrZeroIfCondition(state, condition, [&]() {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(append ? spv::OpAtomicIAdd : spv::OpAtomicISub, TypeU32(state), value, EmitMemoryElementPointer(state, access, index), ConstantU32(state, mem.kind == ResourceKind::Gds ? spv::ScopeDevice : spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsMaskNone), count);
        return value;
    });
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, TypeU32(state), result, ConstantU32(state, spv::ScopeSubgroup), atomic, EmitHostSubgroupLane(state, sourceLane));
    return result;
}

template<typename TUpdate>
std::uint32_t LockedLdsUpdate(SpirvEmitterState& state, TUpdate&& update) {
    const auto lock = EmitLdsLockPointer(state);
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto critical = state.module.AllocateId();
    const auto after = state.module.AllocateId();
    const auto cont = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    state.module.AddFunction(spv::OpLoopMerge, merge, cont, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranch, body);
    EmitLabel(state, body);
    const auto previous = state.module.AllocateId();
    state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), previous, lock, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireMask | spv::MemorySemanticsWorkgroupMemoryMask), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, 1u), ConstantU32(state, 0u));
    const auto acquired = Binary(state, spv::OpIEqual, TypeBool(state), previous, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelectionMerge, after, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, acquired, critical, after);
    EmitLabel(state, critical);
    const auto old = update();
    state.module.AddFunction(spv::OpAtomicStore, lock, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask), ConstantU32(state, 0u));
    const auto criticalExit = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, after);
    EmitLabel(state, after);
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeU64(state), result, old, criticalExit, ConstantU64(state, 0u), body);
    state.module.AddFunction(spv::OpBranchConditional, acquired, merge, cont);
    EmitLabel(state, cont);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    return result;
}

template<typename TReplacement>
std::uint32_t SharedAtomic64(SpirvValueEmitContext& ctx, const IrValue& inst, TReplacement&& replacement) {
    auto& state = ctx.state;
    const auto& mem = SharedMemory(ctx, inst);
    return EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), TypeU64(state), ConstantU64(state, 0u), [&]() {
        const auto resource = PrepareMemoryResourceAccess(state, mem);
        const auto rawIndex = DwordIndex(ctx, inst, mem);
        const auto low = EmitMemoryElementIndex(state, resource, rawIndex);
        const auto high = EmitMemoryElementIndex(state, resource, Binary(state, spv::OpIAdd, TypeU32(state), rawIndex, ConstantU32(state, 1u)));
        return EmitValueOrDefaultIfCondition(state, EmitMemoryElementInBounds(state, resource, high), TypeU64(state), ConstantU64(state, 0u), [&]() {
            const auto update = [&]() {
                const auto lowPointer = EmitMemoryElementPointer(state, resource, low);
                const auto highPointer = EmitMemoryElementPointer(state, resource, high);
                const auto lowValue = state.module.AllocateId();
                const auto highValue = state.module.AllocateId();
                state.module.AddFunction(spv::OpLoad, TypeU32(state), lowValue, lowPointer);
                state.module.AddFunction(spv::OpLoad, TypeU32(state), highValue, highPointer);
                const auto old = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeU64(state), old, lowValue, highValue);
                const auto next = Unary(state, spv::OpBitcast, TypeU64(state), replacement(state, Unary(state, spv::OpBitcast, TypeScalarU64(state), old)));
                const auto nextLow = state.module.AllocateId();
                const auto nextHigh = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), nextLow, next, 0u);
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), nextHigh, next, 1u);
                state.module.AddFunction(spv::OpStore, lowPointer, nextLow);
                state.module.AddFunction(spv::OpStore, highPointer, nextHigh);
                return old;
            };
            return state.requirements.ldsLock ? LockedLdsUpdate(state, update) : update();
        });
    });
}

template<typename TOperation>
std::uint32_t SharedAtomic64Binary(SpirvValueEmitContext& ctx, const IrValue& inst, TOperation&& operation) {
    const auto data = ScalarU64Argument(ctx, inst, 2u);
    return SharedAtomic64(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        return operation(state, old, data);
    });
}

std::uint32_t SharedAtomic64Op(SpirvValueEmitContext& ctx, const IrValue& inst, spv::Op opcode, bool dataFirst) {
    return SharedAtomic64Binary(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old, std::uint32_t data) {
        return dataFirst ? Binary(state, opcode, TypeScalarU64(state), data, old) : Binary(state, opcode, TypeScalarU64(state), old, data);
    });
}

std::uint32_t SharedAtomic64Select(SpirvValueEmitContext& ctx, const IrValue& inst, spv::Op compare) {
    return SharedAtomic64Binary(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old, std::uint32_t data) {
        return Select(state, TypeScalarU64(state), Binary(state, compare, TypeBool(state), data, old), data, old);
    });
}

std::uint32_t SharedFloatMinMax64(SpirvValueEmitContext& ctx, const IrValue& inst, bool maxValue) {
    return SharedAtomic64Binary(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old, std::uint32_t data) {
        const AtomicFloatBits bits{state, true};
        auto result = bits.select(bits.test(maxValue ? spv::OpUGreaterThan : spv::OpULessThan, bits.orderKey(data), bits.orderKey(old)), data, old);
        result = bits.select(bits.nan(data), old, result);
        return bits.select(bits.nan(old), data, result);
    });
}

std::uint32_t ScalarU64Constant(SpirvEmitterState& state, std::uint32_t value) {
    return state.module.Constant(spv::OpConstant, TypeScalarU64(state), value, 0u);
}

}

std::uint32_t AtomicIncrement(SpirvEmitterState& state, std::uint32_t old, std::uint32_t limit) {
    const auto wrap = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), old, limit);
    const auto next = Binary(state, spv::OpIAdd, TypeU32(state), old, ConstantU32(state, 1u));
    return Select(state, TypeU32(state), wrap, ConstantU32(state, 0u), next);
}

std::uint32_t AtomicDecrement(SpirvEmitterState& state, std::uint32_t old, std::uint32_t limit) {
    const auto zero = Binary(state, spv::OpIEqual, TypeBool(state), old, ConstantU32(state, 0u));
    const auto above = Binary(state, spv::OpUGreaterThan, TypeBool(state), old, limit);
    const auto wrap = Binary(state, spv::OpLogicalOr, TypeBool(state), zero, above);
    const auto next = Binary(state, spv::OpISub, TypeU32(state), old, ConstantU32(state, 1u));
    return Select(state, TypeU32(state), wrap, limit, next);
}

std::uint32_t AtomicUSubSat(SpirvEmitterState& state, std::uint32_t old, std::uint32_t value) {
    const auto fits = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), old, value);
    const auto difference = Binary(state, spv::OpISub, TypeU32(state), old, value);
    return Select(state, TypeU32(state), fits, difference, ConstantU32(state, 0u));
}

std::uint32_t AtomicFloatMinMax(SpirvEmitterState& state, std::uint32_t old, std::uint32_t source, bool maxValue) {
    return AtomicFloatBits{state, false}.minMax(old, source, maxValue);
}

std::uint32_t AtomicFloatCompareSwap(SpirvEmitterState& state, std::uint32_t old, std::uint32_t desired, std::uint32_t comparator) {
    const AtomicFloatBits bits{state, false};
    return bits.select(bits.equal(old, comparator), desired, old);
}

std::uint32_t EmitReadConst(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    if (state.flattenedSrtVariable == 0) {
        ctx.Fail(inst, "requires the flattened SRT descriptor");
    }
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.flattenedSrtVariable, ConstantU32(state, 0u), ctx.Arg(inst, 1));
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

void EmitReadConstBuffer(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto& mem = ctx.Memory(inst);
    if (mem.planningOnly) {
        return;
    }
    if (mem.kind != ResourceKind::ScalarBuffer) {
        ctx.Fail(inst, "must read a scalar buffer resource");
    }
    auto& state = ctx.state;
    const auto address = Binary(state, spv::OpIAdd, TypeU32(state), ctx.Arg(inst, 1), ConstantU32(state, mem.offset));
    const auto noCarry = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), address, ctx.Arg(inst, 1));
    if (mem.gpuDescriptor) {
        const IrValue* handle = inst.Argument(0)->Resolve();
        if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) {
            ctx.Fail(inst, "has no GPU-selected V#");
        }
        const auto u32 = TypeU32(state);
        const auto word1 = ctx.Arg(*handle, 1);
        const auto records = ctx.Arg(*handle, 2);
        const auto stride = EmitBitFieldUExtract(state, word1, ConstantU32(state, 16u), ConstantU32(state, 14u));
        const auto size = Select(state, u32, Binary(state, spv::OpIEqual, TypeBool(state), stride, ConstantU32(state, 0u)), records, Binary(state, spv::OpIMul, u32, stride, records));
        const auto byte = Binary(state, spv::OpBitwiseAnd, u32, address, ConstantU32(state, ~3u));
        const auto inBounds = AndCondition(state, noCarry, Binary(state, spv::OpULessThan, TypeBool(state), byte, size));
        const auto base = DeviceAddressFromWords(state, ctx.Arg(*handle, 0), EmitBitFieldUExtract(state, word1, ConstantU32(state, 0u), ConstantU32(state, 16u)));
        const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), base, Unary(state, spv::OpUConvert, TypeScalarU64(state), byte));
        ctx.Define(inst, EmitValueOrZeroIfCondition(state, inBounds, [&] { return EmitBdaRead(ctx, inst, guest, 32u); }));
        return;
    }
    const auto rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    const auto access = PrepareMemoryResourceAccess(state, mem);
    const auto element = EmitMemoryElementIndex(state, access, rawIndex);
    const auto inBounds = AndCondition(state, noCarry, EmitMemoryElementInBounds(state, access, element));
    ctx.Define(inst, EmitValueOrZeroIfCondition(state, inBounds, [&]() {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, EmitMemoryElementPointer(state, access, element));
        return value;
    }));
}

void EmitGetSrtResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitGetBufferResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitGetAddressResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitGetScratchResource(SpirvValueEmitContext& context) {
    EmitVoid(context);
}

void EmitLoadAddressU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddress(ctx, inst, 8u);
}

void EmitLoadAddressU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddress(ctx, inst, 16u);
}

void EmitLoadAddressU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddress(ctx, inst, 32u);
}

void EmitLoadAddressU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddressWide(ctx, inst, 2u);
}

void EmitLoadAddressU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddressWide(ctx, inst, 3u);
}

void EmitLoadAddressU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    LoadAddressWide(ctx, inst, 4u);
}

void EmitStoreAddressU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddress(ctx, inst, 8u);
}

void EmitStoreAddressU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddress(ctx, inst, 16u);
}

void EmitStoreAddressU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddress(ctx, inst, 32u);
}

std::uint32_t EmitAddressAtomic(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return AddressAtomic(ctx, inst);
}

void EmitLoadBufferU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadSubword(ctx, inst, BufferMemory(ctx, inst), 8u));
}

void EmitLoadBufferU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadSubword(ctx, inst, BufferMemory(ctx, inst), 16u));
}

void EmitLoadBufferU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 1u, false)) return;
    const auto& mem = BufferMemory(ctx, inst);
    ctx.Define(inst, mem.formatted ? FormattedLoad(ctx, inst, mem) : LoadWord(ctx, inst, mem));
}

void EmitLoadBufferU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 2u, false)) return;
    ctx.Define(inst, LoadWideBuffer(ctx, inst, BufferMemory(ctx, inst), 2u));
}

void EmitLoadBufferU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 3u, false)) return;
    ctx.Define(inst, LoadWideBuffer(ctx, inst, BufferMemory(ctx, inst), 3u));
}

void EmitLoadBufferU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 4u, false)) return;
    ctx.Define(inst, LoadWideBuffer(ctx, inst, BufferMemory(ctx, inst), 4u));
}

void EmitStoreBufferU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreSubword(ctx, inst, BufferMemory(ctx, inst), 8u);
}

void EmitStoreBufferU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreSubword(ctx, inst, BufferMemory(ctx, inst), 16u);
}

void EmitStoreBufferU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 1u, true)) return;
    const auto& mem = BufferMemory(ctx, inst);
    if (mem.formatted) {
        FormattedStore(ctx, inst, mem);
    } else {
        StoreWord(ctx, inst, mem);
    }
}

void EmitStoreBufferU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 2u, true)) return;
    StoreWideBuffer(ctx, inst, BufferMemory(ctx, inst), 2u);
}

void EmitStoreBufferU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 3u, true)) return;
    StoreWideBuffer(ctx, inst, BufferMemory(ctx, inst), 3u);
}

void EmitStoreBufferU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (EmitGpuDescriptorAccess(ctx, inst, 4u, true)) return;
    StoreWideBuffer(ctx, inst, BufferMemory(ctx, inst), 4u);
}

std::uint32_t EmitBufferAtomicSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicCmpSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicIAdd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicISub32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicSMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicUMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicSMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicUMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicAnd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicOr32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicXor32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, BufferMemory(ctx, inst));
}

std::uint32_t EmitBufferAtomicSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicOr64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicFMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferFloatAtomic(ctx, inst, false);
}

std::uint32_t EmitBufferAtomicFMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferFloatAtomic(ctx, inst, true);
}

void EmitLoadSharedU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadSubword(ctx, inst, SharedMemory(ctx, inst), 8u));
}

void EmitLoadSharedU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadSubword(ctx, inst, SharedMemory(ctx, inst), 16u));
}

void EmitLoadSharedU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadWord(ctx, inst, SharedMemory(ctx, inst)));
}

void EmitLoadSharedU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadWideShared(ctx, inst, SharedMemory(ctx, inst), 2u));
}

void EmitLoadSharedU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadWideShared(ctx, inst, SharedMemory(ctx, inst), 3u));
}

void EmitLoadSharedU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, LoadWideShared(ctx, inst, SharedMemory(ctx, inst), 4u));
}

void EmitWriteSharedU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreSubword(ctx, inst, SharedMemory(ctx, inst), 8u);
}

void EmitWriteSharedU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreSubword(ctx, inst, SharedMemory(ctx, inst), 16u);
}

void EmitWriteSharedU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreWord(ctx, inst, SharedMemory(ctx, inst));
}

void EmitWriteSharedU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreWideShared(ctx, inst, SharedMemory(ctx, inst), 2u);
}

void EmitWriteSharedU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreWideShared(ctx, inst, SharedMemory(ctx, inst), 3u);
}

void EmitWriteSharedU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreWideShared(ctx, inst, SharedMemory(ctx, inst), 4u);
}

std::uint32_t EmitSharedAtomicFMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedFloatMinMax(ctx, inst, false);
}

std::uint32_t EmitSharedAtomicFMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedFloatMinMax(ctx, inst, true);
}

std::uint32_t EmitSharedAtomicSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicIAdd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicISub32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicSMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicUMin32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicSMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicUMax32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicAnd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicOr32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicXor32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return Atomic32(ctx, inst, SharedMemory(ctx, inst));
}

std::uint32_t EmitSharedAtomicInc32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitAtomicUpdate(ctx, inst, SharedMemory(ctx, inst), AtomicIncrement);
}

std::uint32_t EmitSharedAtomicDec32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitAtomicUpdate(ctx, inst, SharedMemory(ctx, inst), AtomicDecrement);
}

std::uint32_t EmitDataAppend(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return AppendConsume(ctx, inst, true);
}

std::uint32_t EmitDataConsume(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return AppendConsume(ctx, inst, false);
}


std::uint32_t EmitSharedAtomicRsub32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto data = ctx.Arg(inst, 1);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        return Binary(state, spv::OpISub, TypeU32(state), data, old);
    });
}

std::uint32_t EmitSharedAtomicFAdd32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto data = ctx.Arg(inst, 1);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto sum = Binary(state, spv::OpFAdd, TypeF32(state), Unary(state, spv::OpBitcast, TypeF32(state), old), Unary(state, spv::OpBitcast, TypeF32(state), data));
        state.module.AddAnnotation(spv::OpDecorate, sum, spv::DecorationNoContraction);
        return Unary(state, spv::OpBitcast, TypeU32(state), sum);
    });
}

std::uint32_t EmitSharedAtomicCmpst32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto compare = ctx.Arg(inst, 1);
    const auto source = ctx.Arg(inst, 2);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        return Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), old, compare), source, old);
    });
}

std::uint32_t EmitSharedAtomicCmpstF32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto compare = ctx.Arg(inst, 1);
    const auto source = ctx.Arg(inst, 2);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto equal = Binary(state, spv::OpFOrdEqual, TypeBool(state), Unary(state, spv::OpBitcast, TypeF32(state), old), Unary(state, spv::OpBitcast, TypeF32(state), compare));
        return Select(state, TypeU32(state), equal, source, old);
    });
}

std::uint32_t EmitSharedAtomicMskor32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto mask = ctx.Arg(inst, 1);
    const auto source = ctx.Arg(inst, 2);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto kept = Binary(state, spv::OpBitwiseAnd, TypeU32(state), old, Unary(state, spv::OpNot, TypeU32(state), mask));
        return Binary(state, spv::OpBitwiseOr, TypeU32(state), kept, source);
    });
}

std::uint32_t EmitSharedAtomicWrap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto limit = ctx.Arg(inst, 1);
    const auto step = ctx.Arg(inst, 2);
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto wraps = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), old, limit);
        return Select(state, TypeU32(state), wraps, Binary(state, spv::OpISub, TypeU32(state), old, limit), Binary(state, spv::OpIAdd, TypeU32(state), old, step));
    });
}

std::uint32_t EmitSharedAtomicSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Binary(ctx, inst, [](SpirvEmitterState&, std::uint32_t, std::uint32_t data) { return data; });
}

std::uint32_t EmitSharedAtomicIAdd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpIAdd, false);
}

std::uint32_t EmitSharedAtomicISub64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpISub, false);
}

std::uint32_t EmitSharedAtomicRsub64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpISub, true);
}

std::uint32_t EmitSharedAtomicInc64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Binary(ctx, inst, [](SpirvEmitterState& state, std::uint32_t old, std::uint32_t limit) {
        const auto wrap = Binary(state, spv::OpUGreaterThanEqual, TypeBool(state), old, limit);
        return Select(state, TypeScalarU64(state), wrap, ScalarU64Constant(state, 0u), Binary(state, spv::OpIAdd, TypeScalarU64(state), old, ScalarU64Constant(state, 1u)));
    });
}

std::uint32_t EmitSharedAtomicDec64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Binary(ctx, inst, [](SpirvEmitterState& state, std::uint32_t old, std::uint32_t limit) {
        const auto zero = Binary(state, spv::OpIEqual, TypeBool(state), old, ScalarU64Constant(state, 0u));
        const auto wrap = Binary(state, spv::OpLogicalOr, TypeBool(state), zero, Binary(state, spv::OpUGreaterThan, TypeBool(state), old, limit));
        return Select(state, TypeScalarU64(state), wrap, limit, Binary(state, spv::OpISub, TypeScalarU64(state), old, ScalarU64Constant(state, 1u)));
    });
}

std::uint32_t EmitSharedAtomicSMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Select(ctx, inst, spv::OpSLessThan);
}

std::uint32_t EmitSharedAtomicUMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Select(ctx, inst, spv::OpULessThan);
}

std::uint32_t EmitSharedAtomicSMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Select(ctx, inst, spv::OpSGreaterThan);
}

std::uint32_t EmitSharedAtomicUMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Select(ctx, inst, spv::OpUGreaterThan);
}

std::uint32_t EmitSharedAtomicAnd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpBitwiseAnd, false);
}

std::uint32_t EmitSharedAtomicOr64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpBitwiseOr, false);
}

std::uint32_t EmitSharedAtomicXor64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedAtomic64Op(ctx, inst, spv::OpBitwiseXor, false);
}

std::uint32_t EmitSharedAtomicFMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedFloatMinMax64(ctx, inst, false);
}

std::uint32_t EmitSharedAtomicFMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return SharedFloatMinMax64(ctx, inst, true);
}

std::uint32_t EmitSharedAtomicCmpst64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto compare = ScalarU64Argument(ctx, inst, 3u);
    const auto source = ScalarU64Argument(ctx, inst, 2u);
    return SharedAtomic64(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        return Select(state, TypeScalarU64(state), Binary(state, spv::OpIEqual, TypeBool(state), old, compare), source, old);
    });
}

std::uint32_t EmitSharedAtomicCmpstF64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto compare = ScalarU64Argument(ctx, inst, 3u);
    const auto source = ScalarU64Argument(ctx, inst, 2u);
    return SharedAtomic64(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const AtomicFloatBits bits{state, true};
        return bits.select(bits.equal(old, compare), source, old);
    });
}

std::uint32_t EmitSharedAtomicMskor64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto mask = ScalarU64Argument(ctx, inst, 3u);
    const auto source = ScalarU64Argument(ctx, inst, 2u);
    return SharedAtomic64(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const auto kept = Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), old, Unary(state, spv::OpNot, TypeScalarU64(state), mask));
        return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), kept, source);
    });
}

std::uint32_t EmitBufferAtomicInc32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitAtomicUpdate(ctx, inst, BufferMemory(ctx, inst), AtomicIncrement);
}

std::uint32_t EmitBufferAtomicDec32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitAtomicUpdate(ctx, inst, BufferMemory(ctx, inst), AtomicDecrement);
}

std::uint32_t EmitBufferAtomicUSubSat32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitAtomicUpdate(ctx, inst, BufferMemory(ctx, inst), AtomicUSubSat);
}

std::uint32_t EmitBufferAtomicIAdd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicISub64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicSMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicUMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicSMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicUMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicAnd64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicXor64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicCmpSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return BufferAtomic64(ctx, inst);
}

std::uint32_t EmitBufferAtomicFCmpSwap32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto desired = ctx.Arg(inst, inst.ArgumentCount() - 3u);
    const auto comparator = ctx.Arg(inst, inst.ArgumentCount() - 2u);
    const auto& mem = BufferMemory(ctx, inst);
    return EmitAtomicAccess(ctx, inst, mem, [&](std::uint32_t pointer) {
        return AtomicUpdate(ctx.state, pointer, mem.kind, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, false}.compareSwap(old, comparator, desired); });
    });
}

std::uint32_t EmitBufferAtomicFCmpSwap64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto desired = ScalarU64Argument(ctx, inst, 3u);
    const auto comparator = ScalarU64Argument(ctx, inst, 2u);
    return BufferAtomic64Update(ctx, inst, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, true}.compareSwap(old, comparator, desired); });
}

std::uint32_t EmitBufferAtomicFMin64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto value = ScalarU64Argument(ctx, inst, 2u);
    return BufferAtomic64Update(ctx, inst, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, true}.minMax(old, value, false); });
}

std::uint32_t EmitBufferAtomicFMax64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto value = ScalarU64Argument(ctx, inst, 2u);
    return BufferAtomic64Update(ctx, inst, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, true}.minMax(old, value, true); });
}

std::uint32_t EmitBufferAtomicInc64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto limit = ScalarU64Argument(ctx, inst, 2u);
    return BufferAtomic64Update(ctx, inst, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, true}.increment(old, limit); });
}

std::uint32_t EmitBufferAtomicDec64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto limit = ScalarU64Argument(ctx, inst, 2u);
    return BufferAtomic64Update(ctx, inst, [&](std::uint32_t old) { return AtomicFloatBits{ctx.state, true}.decrement(old, limit); });
}

}
