#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{
namespace {

std::uint32_t StorageBufferType(SpirvEmitterState& state) {
    const auto array = state.module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, TypeU32(state));
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpDecorate, {spv::DecorationBlock}},
         {spv::OpMemberDecorate, {0, spv::DecorationOffset, 0}}},
        array);
}

std::uint32_t StorageBufferU64Type(SpirvEmitterState& state) {
    const auto array = state.module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 8u}}}, TypeScalarU64(state));
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpDecorate, {spv::DecorationBlock}},
         {spv::OpMemberDecorate, {0, spv::DecorationOffset, 0}}},
        array);
}

}


std::uint32_t TypeVoid(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeVoid);
}

std::uint32_t TypeBool(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeBool);
}

std::uint32_t TypeBoolVector(SpirvEmitterState& state, std::uint32_t components) {
    return state.module.Type(spv::OpTypeVector, TypeBool(state), components);
}

std::uint32_t TypeU32(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeInt, 32u, 0u);
}

std::uint32_t TypeU64(SpirvEmitterState& state) {
    return TypeU32Vector(state, 2);
}

std::uint32_t TypeScalarU64(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeInt, 64u, 0u);
}

std::uint32_t TypeU32Pair(SpirvEmitterState& state) {
    const auto element = TypeU32(state);
    return state.module.Type(spv::OpTypeStruct, element, element);
}

std::uint32_t TypeI32(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeInt, 32u, 1u);
}

std::uint32_t TypeI32Pair(SpirvEmitterState& state) {
    const auto element = TypeI32(state);
    return state.module.Type(spv::OpTypeStruct, element, element);
}

std::uint32_t TypeF32(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeFloat, 32u);
}

std::uint32_t TypeU32Vector(SpirvEmitterState& state, std::uint32_t components) {
    return state.module.Type(spv::OpTypeVector, TypeU32(state), components);
}

std::uint32_t TypeU32Composite(SpirvEmitterState& state, std::uint32_t components) {
    if (components < 2u || components > 4u) {
        FailEmit("u32 composite component count is out of range");
    }
    return components == 2u ? TypeU32Pair(state) : TypeU32Vector(state, components);
}

std::uint32_t TypeI32Vector(SpirvEmitterState& state, std::uint32_t components) {
    return state.module.Type(spv::OpTypeVector, TypeI32(state), components);
}

std::uint32_t TypeF32Vector(SpirvEmitterState& state, std::uint32_t components) {
    return state.module.Type(spv::OpTypeVector, TypeF32(state), components);
}

std::uint32_t TypePointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t pointee) {
    return state.module.Type(spv::OpTypePointer, storageClass, pointee);
}

std::uint32_t TypeFunction(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeFunction, TypeVoid(state));
}

std::uint32_t TypeStorageBufferPointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassStorageBuffer, StorageBufferType(state));
}

std::uint32_t TypeStorageBufferElementPointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassStorageBuffer, TypeU32(state));
}

std::uint32_t TypeStorageBufferU64Pointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassStorageBuffer, StorageBufferU64Type(state));
}

std::uint32_t TypeStorageBufferU64ElementPointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassStorageBuffer, TypeScalarU64(state));
}

std::uint32_t TypePhysicalU32Pointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state));
}

std::uint32_t TypePushConstantElementPointer(SpirvEmitterState& state) {
    return TypePointer(state, spv::StorageClassPushConstant, TypeU32(state));
}

std::uint32_t TypeU32ArrayPointer(SpirvEmitterState& state, std::uint32_t storageClass, std::uint32_t dwords) {
    const auto count = ConstantU32(state, std::max(dwords, 1u));
    const auto array = state.module.Type(spv::OpTypeArray, TypeU32(state), count);
    return TypePointer(state, storageClass, array);
}

std::uint32_t TypeU32ElementPointer(SpirvEmitterState& state, std::uint32_t storageClass) {
    return TypePointer(state, storageClass, TypeU32(state));
}

std::uint32_t TypeId(SpirvEmitterState& state, IrType type) {
    switch (type) {
    case IrType::Bool: return TypeBool(state);
    case IrType::U8:
    case IrType::U16:
    case IrType::U32:
    case IrType::F16: return TypeU32(state);
    case IrType::U64: return TypeU64(state);
    case IrType::Vec2U32: return TypeU32Pair(state);
    case IrType::F32: return TypeF32(state);
    case IrType::Vec3U32: return TypeU32Vector(state, 3);
    case IrType::Vec4U32: return TypeU32Vector(state, 4);
    case IrType::Vec2F32: return TypeF32Vector(state, 2);
    default: return 0;
    }
}

std::uint32_t GlslStd450(SpirvEmitterState& state) {
    return state.module.Import("GLSL.std.450");
}

}
