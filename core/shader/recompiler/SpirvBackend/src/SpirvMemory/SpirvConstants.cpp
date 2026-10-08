#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{

std::uint32_t ConstantU32(SpirvEmitterState& state, std::uint32_t value) {
    return state.module.Constant(spv::OpConstant, TypeU32(state), value);
}

std::uint32_t ConstantI32(SpirvEmitterState& state, std::int32_t value) {
    return state.module.Constant(spv::OpConstant, TypeI32(state), static_cast<std::uint32_t>(value));
}

std::uint32_t ConstantF32(SpirvEmitterState& state, std::uint32_t bits) {
    return state.module.Constant(spv::OpConstant, TypeF32(state), bits);
}

std::uint32_t FloatBits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t ConstantF32Value(SpirvEmitterState& state, float value) {
    return ConstantF32(state, FloatBits(value));
}

std::uint32_t ConstantBool(SpirvEmitterState& state, bool value) {
    return state.module.Constant(value ? spv::OpConstantTrue : spv::OpConstantFalse, TypeBool(state));
}

std::uint32_t ConstantU64(SpirvEmitterState& state, std::uint64_t value) {
    return state.module.Constant(spv::OpConstantComposite, TypeU64(state), ConstantU32(state, static_cast<std::uint32_t>(value)), ConstantU32(state, static_cast<std::uint32_t>(value >> 32u)));
}

std::uint32_t ConstantU32CompositeZero(SpirvEmitterState& state, std::uint32_t components) {
    if (components < 2u || components > 4u) {
        FailEmit("u32 composite component count is out of range");
    }
    const auto zero = ConstantU32(state, 0);
    const auto type = TypeU32Composite(state, components);
    switch (components) {
        case 2u: return state.module.Constant(spv::OpConstantComposite, type, zero, zero);
        case 3u: return state.module.Constant(spv::OpConstantComposite, type, zero, zero, zero);
        default: return state.module.Constant(spv::OpConstantComposite, type, zero, zero, zero, zero);
    }
}

}
