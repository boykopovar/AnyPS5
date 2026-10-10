#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvRuntimeBuffer.hpp"
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
    const auto load = [&](std::uint32_t element) {
        const auto pointer = EmitMemoryElementPointer(state, resource, element);
        const auto value = state.module.AllocateId();
        if (resource.memoryAccess != 0u) state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, resource.memoryAccess);
        else state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    };
    if (resource.misalignment == 0u) return load(index);
    const auto shift = resource.misalignment * 8u;
    const auto low = Binary(state, spv::OpShiftRightLogical, TypeU32(state), load(index), ConstantU32(state, shift));
    const auto high = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), load(EmitAddU32(state, index, ConstantU32(state, 1u))), ConstantU32(state, 32u - shift));
    return Binary(state, spv::OpBitwiseOr, TypeU32(state), low, high);
}

void StoreMisalignedBits(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t index, std::uint32_t mask, std::uint32_t value) {
    auto& state = ctx.state;
    const auto shift = resource.misalignment * 8u;
    const auto merge = [&](std::uint32_t element, spv::Op direction, std::uint32_t amount) {
        const auto partMask = Binary(state, direction, TypeU32(state), mask, ConstantU32(state, amount));
        const auto partValue = Binary(state, direction, TypeU32(state), value, ConstantU32(state, amount));
        AtomicUpdate(state, EmitMemoryElementPointer(state, resource, element), resource.kind, [&](std::uint32_t old) {
            return Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), old, Unary(state, spv::OpNot, TypeU32(state), partMask)), partValue);
        });
    };
    merge(index, spv::OpShiftLeftLogical, shift);
    merge(EmitAddU32(state, index, ConstantU32(state, 1u)), spv::OpShiftRightLogical, 32u - shift);
}

void StoreMisalignedSubword(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t address, std::uint32_t index, std::uint32_t bits, std::uint32_t data) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u) {
        ctx.Fail("subword store has an unsupported width");
    }
    const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), address, ConstantU32(state, 3u)), ConstantU32(state, 3u));
    const auto fieldMask = ConstantU32(state, bits == 8u ? 0xffu : 0xffffu);
    const auto mask = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), fieldMask, shift);
    StoreMisalignedBits(ctx, resource, index, mask, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), data, fieldMask), shift));
}

void RequireAlignedAtomic(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource) {
    if (resource.misalignment != 0u) ctx.Fail("buffer atomic on a V# whose base is not DWORD aligned");
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

void StoreWordInBounds(SpirvValueEmitContext& ctx, const MemoryResourceAccess& resource, std::uint32_t index, std::uint32_t data) {
    auto& state = ctx.state;
    if (resource.misalignment != 0u) {
        StoreMisalignedBits(ctx, resource, index, ConstantU32(state, 0xffffffffu), data);
        return;
    }
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
        if (resource.misalignment != 0u) StoreMisalignedSubword(ctx, resource, address, index, bits, data);
        else StoreSubwordInBounds(ctx, mem, resource, address, index, bits, data);
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

std::uint32_t ConstructU32Composite(SpirvEmitterState& state, std::uint32_t components, const std::array<std::uint32_t, 4>& values) {
    const auto result = state.module.AllocateId();
    std::vector<std::uint32_t> words{spv::OpCompositeConstruct, TypeU32Composite(state, components), result};
    words.insert(words.end(), values.begin(), values.begin() + components);
    state.module.AddFunction(words);
    return result;
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
    return mem;
}

const MemoryInfo& SharedMemory(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto& mem = ctx.Memory(inst);
    if (mem.kind != ResourceKind::Lds && mem.kind != ResourceKind::Gds) {
        ctx.Fail(inst, "must access LDS or GDS");
    }
    return mem;
}

constexpr std::uint32_t SharedApertureHigh = 0x80000000u;
constexpr std::uint32_t PrivateApertureHigh = 0x70000000u;

bool RoutesApertures(const SpirvEmitterState& state, const MemoryInfo& mem) {
    return mem.kind == ResourceKind::Flat && state.program.Resources().stage == IrShaderStage::Compute;
}

struct ApertureAddress {
    std::uint32_t low = 0;
    std::uint32_t shared = 0;
    std::uint32_t priv = 0;
};

ApertureAddress SplitAperture(SpirvEmitterState& state, std::uint32_t address) {
    const auto u32 = TypeU32(state);
    const auto high = Unary(state, spv::OpUConvert, u32, Binary(state, spv::OpShiftRightLogical, TypeScalarU64(state), address, BdaConstant(state, 32u)));
    return {Unary(state, spv::OpUConvert, u32, address), Binary(state, spv::OpIEqual, TypeBool(state), high, ConstantU32(state, SharedApertureHigh)), Binary(state, spv::OpIEqual, TypeBool(state), high, ConstantU32(state, PrivateApertureHigh))};
}

std::uint32_t ApertureByteInBounds(SpirvEmitterState& state, const MemoryResourceAccess& resource, std::uint32_t byteOffset, std::uint32_t offset, std::uint32_t index) {
    const auto noWrap = Binary(state, spv::OpULessThanEqual, TypeBool(state), byteOffset, ConstantU32(state, 0xffffffffu - offset));
    return AndCondition(state, noWrap, EmitMemoryElementInBounds(state, resource, index));
}

std::uint32_t ApertureContained(SpirvEmitterState& state, std::uint32_t byteOffset, std::uint32_t bits) {
    const auto inDword = Binary(state, spv::OpBitwiseAnd, TypeU32(state), byteOffset, ConstantU32(state, 3u));
    return Binary(state, spv::OpULessThanEqual, TypeBool(state), EmitAddU32(state, inDword, ConstantU32(state, bits / 8u)), ConstantU32(state, 4u));
}

std::uint32_t LoadApertureDword(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t bits) {
    auto& state = ctx.state;
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return ConstantU32(state, 0u);
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteOffset, ConstantU32(state, 2u));
    return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&] {
        return bits == 32u ? LoadWordInBounds(ctx, resource, index) : LoadSubwordInBounds(ctx, resource, byteOffset, index, bits, false);
    });
}

std::uint32_t LoadApertureByte(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t offset) {
    auto& state = ctx.state;
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return ConstantU32(state, 0u);
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto address = EmitAddU32(state, byteOffset, ConstantU32(state, offset));
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    return EmitValueOrZeroIfCondition(state, ApertureByteInBounds(state, resource, byteOffset, offset, index), [&] {
        return LoadSubwordInBounds(ctx, resource, address, index, 8u, false);
    });
}

std::uint32_t ApertureElementInBounds(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t bits) {
    auto& state = ctx.state;
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto last = bits / 8u - 1u;
    const auto address = EmitAddU32(state, byteOffset, ConstantU32(state, last));
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    return ApertureByteInBounds(state, resource, byteOffset, last, index);
}

std::uint32_t LoadApertureElement(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t bits) {
    auto& state = ctx.state;
    if (bits == 8u) return LoadApertureByte(ctx, kind, byteOffset, 0u);
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return ConstantU32(state, 0u);
    return EmitValueOrZeroIfCondition(state, ApertureElementInBounds(ctx, kind, byteOffset, bits), [&] {
        return EmitValueIfElse(state, ApertureContained(state, byteOffset, bits), TypeU32(state), [&] {
            return LoadApertureDword(ctx, kind, byteOffset, bits);
        }, [&] {
            std::uint32_t value = ConstantU32(state, 0u);
            for (std::uint32_t byte = 0; byte < bits / 8u; ++byte) {
                const auto part = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), LoadApertureByte(ctx, kind, byteOffset, byte), ConstantU32(state, byte * 8u));
                value = Binary(state, spv::OpBitwiseOr, TypeU32(state), value, part);
            }
            return value;
        });
    });
}

void StoreApertureByte(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t offset, std::uint32_t data) {
    auto& state = ctx.state;
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return;
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto address = EmitAddU32(state, byteOffset, ConstantU32(state, offset));
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
    EmitIfCondition(state, ApertureByteInBounds(state, resource, byteOffset, offset, index), [&] { StoreSubwordInBounds(ctx, storage, resource, address, index, 8u, data); });
}

void StoreApertureDword(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t bits, std::uint32_t data) {
    auto& state = ctx.state;
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return;
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteOffset, ConstantU32(state, 2u));
    EmitIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&] {
        if (bits == 32u) {
            StoreWordInBounds(ctx, resource, index, data);
        } else {
            StoreSubwordInBounds(ctx, storage, resource, byteOffset, index, bits, data);
        }
    });
}

void StoreApertureElement(SpirvValueEmitContext& ctx, ResourceKind kind, std::uint32_t byteOffset, std::uint32_t bits, std::uint32_t data) {
    auto& state = ctx.state;
    if (bits == 8u) {
        StoreApertureByte(ctx, kind, byteOffset, 0u, data);
        return;
    }
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return;
    EmitIfCondition(state, ApertureElementInBounds(ctx, kind, byteOffset, bits), [&] {
        const auto contained = ApertureContained(state, byteOffset, bits);
        EmitIfCondition(state, contained, [&] { StoreApertureDword(ctx, kind, byteOffset, bits, data); });
        EmitIfCondition(state, Unary(state, spv::OpLogicalNot, TypeBool(state), contained), [&] {
            for (std::uint32_t byte = 0; byte < bits / 8u; ++byte) {
                StoreApertureByte(ctx, kind, byteOffset, byte, Binary(state, spv::OpShiftRightLogical, TypeU32(state), data, ConstantU32(state, byte * 8u)));
            }
        });
    });
}

template<typename TAperture, typename TGlobal>
std::uint32_t RouteFlatAccess(SpirvEmitterState& state, std::uint32_t address, std::uint32_t type, TAperture&& aperture, TGlobal&& global) {
    const auto split = SplitAperture(state, address);
    return EmitValueIfElse(state, split.shared, type, [&] { return aperture(ResourceKind::Lds, split.low); }, [&] {
        return EmitValueIfElse(state, split.priv, type, [&] { return aperture(ResourceKind::Scratch, split.low); }, global);
    });
}

void LoadAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t bits) {
    const auto& mem = ctx.Memory(inst);
    if (bits == 32u && mem.planningOnly) {
        return;
    }
    if (RoutesApertures(ctx.state, mem)) {
        ctx.Define(inst, EmitValueOrZeroIfCondition(ctx.state, ActiveArgument(ctx, inst), [&] {
            const auto address = GuestAddress(ctx, inst, mem);
            return RouteFlatAccess(ctx.state, address, TypeU32(ctx.state), [&](ResourceKind kind, std::uint32_t low) { return LoadApertureElement(ctx, kind, low, bits); }, [&] { return EmitBdaRead(ctx, inst, address, bits); });
        }));
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
    if (RoutesApertures(ctx.state, mem)) {
        auto& state = ctx.state;
        const auto type = TypeU32Composite(state, components);
        ctx.Define(inst, EmitValueOrDefaultIfCondition(state, ActiveArgument(ctx, inst), type, ConstantU32CompositeZero(state, components), [&] {
            const auto base = GuestAddressBase(ctx, inst, mem);
            const auto address = AddBdaImmediate(ctx, inst, base, static_cast<std::int32_t>(mem.offset));
            return RouteFlatAccess(state, address, type, [&](ResourceKind kind, std::uint32_t low) {
                std::array<std::uint32_t, 4> values{};
                for (std::uint32_t component = 0; component < components; component++) {
                    values[component] = LoadApertureElement(ctx, kind, Binary(state, spv::OpIAdd, TypeU32(state), low, ConstantU32(state, component * 4u)), 32u);
                }
                return ConstructU32Composite(state, components, values);
            }, [&] { return ConstructU32Composite(state, components, EmitBdaDwordReads(ctx, inst, base, mem.offset, components)); });
        }));
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
            auto& state = ctx.state;
            const auto address = GuestAddress(ctx, inst, mem);
            const auto data = ctx.Arg(inst, inst.ArgumentCount() - 2u);
            if (!RoutesApertures(state, mem)) {
                EmitBdaStore(ctx, inst, address, data, bits);
                return;
            }
            const auto split = SplitAperture(state, address);
            EmitIfCondition(state, split.shared, [&] { StoreApertureElement(ctx, ResourceKind::Lds, split.low, bits, data); });
            EmitIfCondition(state, split.priv, [&] { StoreApertureElement(ctx, ResourceKind::Scratch, split.low, bits, data); });
            const auto global = Unary(state, spv::OpLogicalNot, TypeBool(state), Binary(state, spv::OpLogicalOr, TypeBool(state), split.shared, split.priv));
            EmitIfCondition(state, global, [&] { EmitBdaStore(ctx, inst, address, data, bits); });
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
    RequireAlignedAtomic(ctx, resource);
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
    if (mem.kind == ResourceKind::Buffer) return EmitRuntimeBufferAtomic(ctx, inst, 32u, ActiveArgument(ctx, inst), operation);
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
    const auto apply = [&](std::uint32_t pointer) {
        const auto operation = [&]() {
            const auto old = EmitAtomicOperation(ctx, inst, pointer, scope);
            if (lds) {
                const std::uint32_t semantics = spv::MemorySemanticsAcquireReleaseMask | LdsMemorySemantics(state);
                state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, scope), ConstantU32(state, semantics));
            } else {
                EmitDeviceAtomicMemoryBarrier(state);
            }
            return old;
        };
        if (!zeroIdentity || zeroSkip) return operation();
        return EmitValueIfElse(state, nonZero, TypeU32(state), operation, [&]() {
            const auto current = state.module.AllocateId();
            state.module.AddFunction(spv::OpAtomicLoad, TypeU32(state), current, pointer, ConstantU32(state, scope), ConstantU32(state, spv::MemorySemanticsMaskNone));
            EmitDeviceAtomicMemoryBarrier(state);
            return current;
        });
    };
    if (mem.kind == ResourceKind::Buffer) return EmitRuntimeBufferAtomic(ctx, inst, 32u, active, apply);
    return EmitValueOrZeroIfCondition(state, active, [&] {
        const auto access = PrepareMemoryElement(ctx, mem, DwordIndex(ctx, inst, mem));
        return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, access.resource, access.index), [&] { return apply(EmitMemoryElementPointer(state, access.resource, access.index)); });
    });
}

std::uint32_t BufferAtomic64(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    static_cast<void>(BufferMemory(ctx, inst));
    return EmitRuntimeBufferAtomic(ctx, inst, 64u, ActiveArgument(ctx, inst), [&](std::uint32_t pointer) {
        const auto value = Unary(state, spv::OpBitcast, TypeScalarU64(state), ctx.Arg(inst, inst.ArgumentCount() - 2u));
        const auto old = state.module.AllocateId();
        if (inst.Opcode() == IrOpcode::BufferAtomicCmpSwap64) {
            const auto desired = Unary(state, spv::OpBitcast, TypeScalarU64(state), ctx.Arg(inst, inst.ArgumentCount() - 3u));
            state.module.AddFunction(spv::OpAtomicCompareExchange, TypeScalarU64(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, spv::MemorySemanticsMaskNone), desired, value);
        } else {
            state.module.AddFunction(SpirvAtomicOpcode(inst.Opcode()), TypeScalarU64(state), old, pointer, ConstantU32(state, spv::ScopeDevice), ConstantU32(state, spv::MemorySemanticsMaskNone), value);
        }
        EmitDeviceAtomicMemoryBarrier(state);
        return Unary(state, spv::OpBitcast, TypeU64(state), old);
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
    std::uint64_t minNormal() const { return wide ? 0x0010000000000000ull : 0x00800000ull; }
    std::uint32_t op(spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) const { return Binary(state, opcode, type(), lhs, rhs); }
    std::uint32_t test(spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) const { return Binary(state, opcode, TypeBool(state), lhs, rhs); }
    std::uint32_t select(std::uint32_t condition, std::uint32_t lhs, std::uint32_t rhs) const { return Select(state, type(), condition, lhs, rhs); }
    std::uint32_t magnitude(std::uint32_t value) const { return op(spv::OpBitwiseAnd, value, constant(sign() - 1u)); }
    std::uint32_t nan(std::uint32_t value) const { return test(spv::OpUGreaterThan, magnitude(value), constant(infinity())); }
    std::uint32_t flush(std::uint32_t value, bool enabled) const {
        return enabled ? select(test(spv::OpULessThan, magnitude(value), constant(minNormal())), op(spv::OpBitwiseAnd, value, constant(sign())), value) : value;
    }
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
    static_cast<void>(BufferMemory(ctx, inst));
    return EmitRuntimeBufferAtomic(ctx, inst, 64u, ActiveArgument(ctx, inst), [&](std::uint32_t pointer) {
        const auto old = AtomicUpdateTyped(state, pointer, ResourceKind::Buffer, TypeScalarU64(state), [&](std::uint32_t observed) { return replacement(observed); });
        return Unary(state, spv::OpBitcast, TypeU64(state), old);
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
        const std::uint32_t semantics = spv::MemorySemanticsAcquireReleaseMask | LdsMemorySemantics(state);
        state.module.AddFunction(spv::OpMemoryBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, semantics));
        return old;
    });
}

std::uint32_t SharedFloatMinMax(SpirvValueEmitContext& ctx, const IrValue& inst, bool maxValue) {
    const auto data = ctx.Arg(inst, 1);
    const bool flush = SharedMemory(ctx, inst).flushDenormals;
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const AtomicFloatBits bits{state, false};
        return bits.minMax(bits.flush(old, flush), bits.flush(data, flush), maxValue);
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

std::uint32_t FlatAtomicNext32(SpirvEmitterState& state, IrOpcode opcode, std::uint32_t old, std::uint32_t value, std::uint32_t comparator) {
    const auto u32 = TypeU32(state);
    const auto winner = [&](spv::Op compare) {
        return Select(state, u32, Binary(state, compare, TypeBool(state), old, value), old, value);
    };
    switch (opcode) {
    case IrOpcode::AddressAtomicSwap32:
        return value;
    case IrOpcode::AddressAtomicCmpSwap32:
        return Select(state, u32, Binary(state, spv::OpIEqual, TypeBool(state), old, comparator), value, old);
    case IrOpcode::AddressAtomicIAdd32:
        return Binary(state, spv::OpIAdd, u32, old, value);
    case IrOpcode::AddressAtomicISub32:
        return Binary(state, spv::OpISub, u32, old, value);
    case IrOpcode::AddressAtomicSMin32:
        return winner(spv::OpSLessThan);
    case IrOpcode::AddressAtomicUMin32:
        return winner(spv::OpULessThan);
    case IrOpcode::AddressAtomicSMax32:
        return winner(spv::OpSGreaterThan);
    case IrOpcode::AddressAtomicUMax32:
        return winner(spv::OpUGreaterThan);
    case IrOpcode::AddressAtomicAnd32:
        return Binary(state, spv::OpBitwiseAnd, u32, old, value);
    case IrOpcode::AddressAtomicOr32:
        return Binary(state, spv::OpBitwiseOr, u32, old, value);
    case IrOpcode::AddressAtomicXor32:
        return Binary(state, spv::OpBitwiseXor, u32, old, value);
    case IrOpcode::AddressAtomicInc32:
        return AtomicIncrement(state, old, value);
    case IrOpcode::AddressAtomicDec32:
        return AtomicDecrement(state, old, value);
    case IrOpcode::AddressAtomicUSubSat32:
        return AtomicUSubSat(state, old, value);
    case IrOpcode::AddressAtomicFMin32:
        return AtomicFloatBits{state, false}.minMax(old, value, false);
    case IrOpcode::AddressAtomicFMax32:
        return AtomicFloatBits{state, false}.minMax(old, value, true);
    case IrOpcode::AddressAtomicFCmpSwap32:
        return AtomicFloatBits{state, false}.compareSwap(old, comparator, value);
    default:
        throw std::runtime_error("FlatAtomicNext32: opcode has no aperture lowering");
    }
}

std::uint32_t ApertureAtomic32(SpirvValueEmitContext& ctx, const IrValue& inst, ResourceKind kind, std::uint32_t address, std::uint32_t byteOffset, std::uint32_t value, std::uint32_t comparator) {
    auto& state = ctx.state;
    const auto instruction = ConstantU32(state, inst.Flags<MemoryFlags>().pc);
    const auto unaligned = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), byteOffset, ConstantU32(state, 3u)), ConstantU32(state, 0u));
    EmitIfCondition(state, unaligned, [&] { RecordBdaFault(state, address, ConstantU32(state, 4u), instruction, BdaAbi::FaultReason::Unaligned); });
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return ConstantU32(state, 0u);
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto index = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteOffset, ConstantU32(state, 2u));
    const auto aligned = Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned);
    const auto opcode = inst.Opcode();
    return EmitValueOrZeroIfCondition(state, aligned, [&] {
        return EmitValueOrZeroIfCondition(state, EmitMemoryElementInBounds(state, resource, index), [&] {
            const auto pointer = EmitMemoryElementPointer(state, resource, index);
            if (kind == ResourceKind::Lds) {
                return AtomicUpdate(state, pointer, kind, [&](std::uint32_t current) { return FlatAtomicNext32(state, opcode, current, value, comparator); });
            }
            const auto old = state.module.AllocateId();
            state.module.AddFunction(spv::OpLoad, TypeU32(state), old, pointer);
            state.module.AddFunction(spv::OpStore, pointer, FlatAtomicNext32(state, opcode, old, value, comparator));
            return old;
        });
    });
}

std::uint32_t FlatAtomicNext64(SpirvEmitterState& state, IrOpcode opcode, std::uint32_t old, std::uint32_t value, std::uint32_t comparator) {
    const auto u64 = TypeScalarU64(state);
    const AtomicFloatBits bits{state, true};
    const auto winner = [&](spv::Op compare) {
        return Select(state, u64, Binary(state, compare, TypeBool(state), old, value), old, value);
    };
    switch (opcode) {
    case IrOpcode::AddressAtomicSwap64:
        return value;
    case IrOpcode::AddressAtomicCmpSwap64:
        return Select(state, u64, Binary(state, spv::OpIEqual, TypeBool(state), old, comparator), value, old);
    case IrOpcode::AddressAtomicIAdd64:
        return Binary(state, spv::OpIAdd, u64, old, value);
    case IrOpcode::AddressAtomicISub64:
        return Binary(state, spv::OpISub, u64, old, value);
    case IrOpcode::AddressAtomicSMin64:
        return winner(spv::OpSLessThan);
    case IrOpcode::AddressAtomicUMin64:
        return winner(spv::OpULessThan);
    case IrOpcode::AddressAtomicSMax64:
        return winner(spv::OpSGreaterThan);
    case IrOpcode::AddressAtomicUMax64:
        return winner(spv::OpUGreaterThan);
    case IrOpcode::AddressAtomicAnd64:
        return Binary(state, spv::OpBitwiseAnd, u64, old, value);
    case IrOpcode::AddressAtomicOr64:
        return Binary(state, spv::OpBitwiseOr, u64, old, value);
    case IrOpcode::AddressAtomicXor64:
        return Binary(state, spv::OpBitwiseXor, u64, old, value);
    case IrOpcode::AddressAtomicInc64:
        return bits.increment(old, value);
    case IrOpcode::AddressAtomicDec64:
        return bits.decrement(old, value);
    case IrOpcode::AddressAtomicFMin64:
        return bits.minMax(old, value, false);
    case IrOpcode::AddressAtomicFMax64:
        return bits.minMax(old, value, true);
    case IrOpcode::AddressAtomicFCmpSwap64:
        return bits.compareSwap(old, comparator, value);
    default:
        throw std::runtime_error("FlatAtomicNext64: opcode has no aperture lowering");
    }
}

template<typename TUpdate>
std::uint32_t LockedLdsUpdate(SpirvEmitterState& state, TUpdate&& update);

std::uint32_t ApertureAtomic64(SpirvValueEmitContext& ctx, const IrValue& inst, ResourceKind kind, std::uint32_t address, std::uint32_t byteOffset, std::uint32_t value, std::uint32_t comparator) {
    auto& state = ctx.state;
    const auto instruction = ConstantU32(state, inst.Flags<MemoryFlags>().pc);
    const auto unaligned = Binary(state, spv::OpINotEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeU32(state), byteOffset, ConstantU32(state, 7u)), ConstantU32(state, 0u));
    EmitIfCondition(state, unaligned, [&] { RecordBdaFault(state, address, ConstantU32(state, 8u), instruction, BdaAbi::FaultReason::Unaligned); });
    if (kind == ResourceKind::Scratch && state.program.Info().scratchDwords == 0u) return ConstantU64(state, 0u);
    if (kind == ResourceKind::Lds && !state.requirements.ldsLock) ctx.Fail(inst, "64-bit LDS atomic reached through a flat address without the workgroup lock");
    MemoryInfo storage{};
    storage.kind = kind;
    const auto resource = PrepareMemoryResourceAccess(state, storage);
    const auto low = Binary(state, spv::OpShiftRightLogical, TypeU32(state), byteOffset, ConstantU32(state, 2u));
    const auto high = EmitAddU32(state, low, ConstantU32(state, 1u));
    const auto aligned = Unary(state, spv::OpLogicalNot, TypeBool(state), unaligned);
    const auto opcode = inst.Opcode();
    return EmitValueOrDefaultIfCondition(state, aligned, TypeU64(state), ConstantU64(state, 0u), [&] {
        return EmitValueOrDefaultIfCondition(state, EmitMemoryElementInBounds(state, resource, high), TypeU64(state), ConstantU64(state, 0u), [&] {
            const auto update = [&]() {
                const auto lowPointer = EmitMemoryElementPointer(state, resource, low);
                const auto highPointer = EmitMemoryElementPointer(state, resource, high);
                const auto lowValue = state.module.AllocateId();
                const auto highValue = state.module.AllocateId();
                state.module.AddFunction(spv::OpLoad, TypeU32(state), lowValue, lowPointer);
                state.module.AddFunction(spv::OpLoad, TypeU32(state), highValue, highPointer);
                const auto old = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeU64(state), old, lowValue, highValue);
                const auto next = Unary(state, spv::OpBitcast, TypeU64(state), FlatAtomicNext64(state, opcode, Unary(state, spv::OpBitcast, TypeScalarU64(state), old), value, comparator));
                const auto nextLow = state.module.AllocateId();
                const auto nextHigh = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), nextLow, next, 0u);
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), nextHigh, next, 1u);
                state.module.AddFunction(spv::OpStore, lowPointer, nextLow);
                state.module.AddFunction(spv::OpStore, highPointer, nextHigh);
                return old;
            };
            return kind == ResourceKind::Lds ? LockedLdsUpdate(state, update) : update();
        });
    });
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
        const auto operation = [&](std::uint32_t pointer) {
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
        };
        if (RoutesApertures(state, mem)) {
            const auto address = GuestAddress(ctx, inst, mem);
            const bool hasComparator = inst.Opcode() == IrOpcode::AddressAtomicCmpSwap32 || inst.Opcode() == IrOpcode::AddressAtomicFCmpSwap32 || inst.Opcode() == IrOpcode::AddressAtomicCmpSwap64 || inst.Opcode() == IrOpcode::AddressAtomicFCmpSwap64;
            const auto comparator = hasComparator ? scalar(ctx.Arg(inst, 4)) : 0u;
            if (wide) {
                return RouteFlatAccess(state, address, type, [&](ResourceKind kind, std::uint32_t low) { return ApertureAtomic64(ctx, inst, kind, address, low, value, comparator); }, [&] { return Unary(state, spv::OpBitcast, type, EmitBdaAtomic(ctx, inst, address, 8u, operation)); });
            }
            return RouteFlatAccess(state, address, TypeU32(state), [&](ResourceKind kind, std::uint32_t low) { return ApertureAtomic32(ctx, inst, kind, address, low, value, comparator); }, [&] { return EmitBdaAtomic(ctx, inst, address, 4u, operation); });
        }
        const auto old = EmitBdaAtomic(ctx, inst, GuestAddress(ctx, inst, mem), wide ? 8u : 4u, operation);
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
    const bool gds = mem.kind == ResourceKind::Gds;
    std::uint32_t rawIndex = ConstantU32(state, mem.offset >> 2u);
    std::uint32_t m0Bounds = 0;
    if (gds) {
        const auto m0 = ctx.Arg(inst, 0);
        const auto base = Binary(state, spv::OpShiftRightLogical, TypeU32(state), m0, ConstantU32(state, 16u));
        const auto size = Binary(state, spv::OpBitwiseAnd, TypeU32(state), m0, ConstantU32(state, 0xffffu));
        const auto address = Binary(state, spv::OpIAdd, TypeU32(state), base, ConstantU32(state, mem.offset));
        rawIndex = Binary(state, spv::OpShiftRightLogical, TypeU32(state), address, ConstantU32(state, 2u));
        m0Bounds = Binary(state, spv::OpINotEqual, TypeBool(state), size, ConstantU32(state, 0u));
    }
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
    const auto bounds = gds ? AndCondition(state, storageBounds, m0Bounds) : storageBounds;
    const auto lanesActive = wave64 ? Binary(state, spv::OpINotEqual, TypeBool(state), count, ConstantU32(state, 0u)) : exec;
    const auto condition = AndCondition(state, isFirst, AndCondition(state, lanesActive, bounds));
    const auto atomic = EmitValueOrZeroIfCondition(state, condition, [&]() {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(append ? spv::OpAtomicIAdd : spv::OpAtomicISub, TypeU32(state), value, EmitMemoryElementPointer(state, access, index), ConstantU32(state, gds ? spv::ScopeDevice : spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsMaskNone), count);
        return value;
    });
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, TypeU32(state), result, ConstantU32(state, spv::ScopeSubgroup), atomic, EmitHostSubgroupLane(state, sourceLane));
    return result;
}

template<typename TUpdate>
std::uint32_t LockedLdsUpdate(SpirvEmitterState& state, TUpdate&& update) {
    const auto lock = EmitLdsLockPointer(state);
    const auto entry = state.currentLabel;
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto pending = state.module.AllocateId();
    const auto spinHeader = state.module.AllocateId();
    const auto spinBody = state.module.AllocateId();
    const auto spinCont = state.module.AllocateId();
    const auto critical = state.module.AllocateId();
    const auto servedMerge = state.module.AllocateId();
    const auto pendingMerge = state.module.AllocateId();
    const auto cont = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    const auto done = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    const auto doneNext = state.module.AllocateId();
    const auto resultNext = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    state.module.AddFunction(spv::OpPhi, TypeBool(state), done, ConstantBool(state, false), entry, doneNext, cont);
    state.module.AddFunction(spv::OpPhi, TypeU64(state), result, ConstantU64(state, 0u), entry, resultNext, cont);
    state.module.AddFunction(spv::OpLoopMerge, merge, cont, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranch, body);
    EmitLabel(state, body);
    state.module.AddFunction(spv::OpSelectionMerge, pendingMerge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, done, pendingMerge, pending);
    EmitLabel(state, pending);
    const auto elected = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformElect, TypeBool(state), elected, ConstantU32(state, spv::ScopeSubgroup));
    state.module.AddFunction(spv::OpSelectionMerge, servedMerge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, elected, spinHeader, servedMerge);
    EmitLabel(state, spinHeader);
    state.module.AddFunction(spv::OpLoopMerge, critical, spinCont, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranch, spinBody);
    EmitLabel(state, spinBody);
    const auto previous = state.module.AllocateId();
    state.module.AddFunction(spv::OpAtomicCompareExchange, TypeU32(state), previous, lock, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireMask | LdsMemorySemantics(state)), ConstantU32(state, spv::MemorySemanticsMaskNone), ConstantU32(state, 1u), ConstantU32(state, 0u));
    const auto acquired = Binary(state, spv::OpIEqual, TypeBool(state), previous, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpBranchConditional, acquired, critical, spinCont);
    EmitLabel(state, spinCont);
    state.module.AddFunction(spv::OpBranch, spinHeader);
    EmitLabel(state, critical);
    const auto old = update();
    state.module.AddFunction(spv::OpAtomicStore, lock, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsReleaseMask | LdsMemorySemantics(state)), ConstantU32(state, 0u));
    const auto criticalExit = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, servedMerge);
    EmitLabel(state, servedMerge);
    const auto served = state.module.AllocateId();
    const auto servedResult = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeBool(state), served, ConstantBool(state, true), criticalExit, ConstantBool(state, false), pending);
    state.module.AddFunction(spv::OpPhi, TypeU64(state), servedResult, old, criticalExit, result, pending);
    state.module.AddFunction(spv::OpBranch, pendingMerge);
    EmitLabel(state, pendingMerge);
    state.module.AddFunction(spv::OpPhi, TypeBool(state), doneNext, served, servedMerge, done, body);
    state.module.AddFunction(spv::OpPhi, TypeU64(state), resultNext, servedResult, servedMerge, result, body);
    const auto ballot = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformBallot, TypeU32Vector(state, 4u), ballot, ConstantU32(state, spv::ScopeSubgroup), Unary(state, spv::OpLogicalNot, TypeBool(state), doneNext));
    std::uint32_t remaining = ConstantU32(state, 0u);
    for (std::uint32_t component = 0; component < 4u; ++component) {
        const auto word = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), word, ballot, component);
        remaining = Binary(state, spv::OpBitwiseOr, TypeU32(state), remaining, word);
    }
    state.module.AddFunction(spv::OpBranchConditional, Binary(state, spv::OpINotEqual, TypeBool(state), remaining, ConstantU32(state, 0u)), cont, merge);
    EmitLabel(state, cont);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    return resultNext;
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
    const bool flush = SharedMemory(ctx, inst).flushDenormals;
    return SharedAtomic64Binary(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old, std::uint32_t data) {
        const AtomicFloatBits bits{state, true};
        return bits.minMax(bits.flush(old, flush), bits.flush(data, flush), maxValue);
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
    const auto& resources = state.program.Resources();
    const auto& guarded = resources.guardedSrtSlots;
    if (!guarded.empty()) {
        const IrValue* slot = inst.Argument(1)->Resolve();
        if (!slot->HasImmediate()) {
            ctx.Fail(inst, "reads the flattened SRT at a runtime slot while some slots are guarded");
        }
        const auto found = std::lower_bound(guarded.begin(), guarded.end(), slot->ImmediateU32());
        if (found != guarded.end() && *found == slot->ImmediateU32()) {
            if (state.faultBufferVariable == 0) {
                ctx.Fail(inst, "reads a guarded SRT slot without a fault buffer");
            }
            const auto u32 = TypeU32(state);
            const auto load = [&](std::uint32_t index) {
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.flattenedSrtVariable, ConstantU32(state, 0u), index);
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpLoad, u32, value, pointer);
                return value;
            };
            const auto guard = static_cast<std::uint32_t>(found - guarded.begin());
            const auto flag = load(ConstantU32(state, resources.srtGuardOffset + guard));
            const auto poisoned = Binary(state, spv::OpINotEqual, TypeBool(state), flag, ConstantU32(state, 0u));
            EmitIfCondition(state, poisoned, [&] {
                const auto records = resources.srtGuardOffset + static_cast<std::uint32_t>(guarded.size());
                const auto record = Binary(state, spv::OpIAdd, u32, ConstantU32(state, records), Binary(state, spv::OpIMul, u32, Binary(state, spv::OpISub, u32, flag, ConstantU32(state, 1u)), ConstantU32(state, 3u)));
                const auto word = [&](std::uint32_t offset) { return load(Binary(state, spv::OpIAdd, u32, record, ConstantU32(state, offset))); };
                const auto pc = word(0u);
                const auto low = word(1u);
                const auto high = word(2u);
                RecordBdaFaultWords(state, low, high, ConstantU32(state, 4u), pc, BdaAbi::FaultReason::Unmapped);
            });
            StopBdaInvocationIf(state, poisoned);
        }
    }
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypeStorageBufferElementPointer(state), pointer, state.flattenedSrtVariable, ConstantU32(state, 0u), ctx.Arg(inst, 1));
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
    return value;
}

void EmitReadConstBuffer(SpirvValueEmitContext& ctx, const IrValue& inst) {
    const auto& mem = ctx.Memory(inst);
    if (mem.planningOnly) return;
    if (mem.kind != ResourceKind::ScalarBuffer) ctx.Fail(inst, "must read a scalar buffer resource");
    ctx.Define(inst, EmitRuntimeScalarBufferLoad(ctx, inst));
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

void StoreAddressWide(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t components) {
    const auto& mem = ctx.Memory(inst);
    if (mem.kind != ResourceKind::Flat && mem.kind != ResourceKind::Global) ctx.Fail(inst, "must write a physical address resource");
    EmitIfCondition(ctx.state, ActiveArgument(ctx, inst), [&]() {
        auto& state = ctx.state;
        const auto base = GuestAddressBase(ctx, inst, mem);
        const auto data = ctx.Arg(inst, inst.ArgumentCount() - 2u);
        if (!RoutesApertures(state, mem)) {
            EmitBdaDwordWrites(ctx, inst, base, mem.offset, components, data);
            return;
        }
        const auto split = SplitAperture(state, AddBdaImmediate(ctx, inst, base, static_cast<std::int32_t>(mem.offset)));
        const auto storeAperture = [&](ResourceKind kind) {
            for (std::uint32_t component = 0; component < components; component++) {
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), value, data, component);
                StoreApertureElement(ctx, kind, Binary(state, spv::OpIAdd, TypeU32(state), split.low, ConstantU32(state, component * 4u)), 32u, value);
            }
        };
        EmitIfCondition(state, split.shared, [&] { storeAperture(ResourceKind::Lds); });
        EmitIfCondition(state, split.priv, [&] { storeAperture(ResourceKind::Scratch); });
        const auto global = Unary(state, spv::OpLogicalNot, TypeBool(state), Binary(state, spv::OpLogicalOr, TypeBool(state), split.shared, split.priv));
        EmitIfCondition(state, global, [&] { EmitBdaDwordWrites(ctx, inst, base, mem.offset, components, data); });
    });
}

void EmitStoreAddressU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddressWide(ctx, inst, 2u);
}

void EmitStoreAddressU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddressWide(ctx, inst, 3u);
}

void EmitStoreAddressU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    StoreAddressWide(ctx, inst, 4u);
}

std::uint32_t EmitAddressAtomic(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return AddressAtomic(ctx, inst);
}

void EmitLoadBufferU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 1u));
}

void EmitLoadBufferU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 1u));
}

void EmitLoadBufferU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 1u));
}

void EmitLoadBufferU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 2u));
}

void EmitLoadBufferU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 3u));
}

void EmitLoadBufferU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Define(inst, EmitRuntimeBufferLoad(ctx, inst, 4u));
}

void EmitStoreBufferU8(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 1u);
}

void EmitStoreBufferU16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 1u);
}

void EmitStoreBufferU32(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 1u);
}

void EmitStoreBufferU32x2(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 2u);
}

void EmitStoreBufferU32x3(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 3u);
}

void EmitStoreBufferU32x4(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitRuntimeBufferStore(ctx, inst, 4u);
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
    const bool flush = SharedMemory(ctx, inst).flushDenormals;
    return SharedAtomicUpdate(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const AtomicFloatBits bits{state, false};
        return bits.select(bits.equal(bits.flush(old, flush), bits.flush(compare, flush)), source, old);
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
    const bool flush = SharedMemory(ctx, inst).flushDenormals;
    return SharedAtomic64(ctx, inst, [&](SpirvEmitterState& state, std::uint32_t old) {
        const AtomicFloatBits bits{state, true};
        return bits.select(bits.equal(bits.flush(old, flush), bits.flush(compare, flush)), source, old);
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
