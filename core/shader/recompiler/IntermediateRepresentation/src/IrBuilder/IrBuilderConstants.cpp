#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrBuilderInternal.hpp"

namespace ShaderRecompiler {

IrValue& IrBuilder::Constant(std::uint32_t value) {
    IrValue& constant = createLeafValue(program, IrType::U32);
    constant.SetImmediateU32(value);
    return constant;
}

IrValue& IrBuilder::ConstantU64(std::uint64_t value) {
    IrValue& constant = createLeafValue(program, IrType::U64);
    constant.SetImmediateU64(value);
    return constant;
}

IrValue& IrBuilder::ConstantF16(std::uint16_t bits) {
    IrValue& constant = createLeafValue(program, IrType::F16);
    constant.SetImmediateF16Bits(bits);
    return constant;
}

IrValue& IrBuilder::ConstantF32(float value) {
    IrValue& constant = createLeafValue(program, IrType::F32);
    constant.SetImmediateF32(value);
    return constant;
}

IrValue& IrBuilder::ConstantBool(bool value) {
    IrValue& constant = createLeafValue(program, IrType::Bool);
    constant.SetImmediateBool(value);
    return constant;
}

IrValue& IrBuilder::ConstantU8(std::uint8_t value) {
    IrValue& constant = createLeafValue(program, IrType::U8);
    constant.SetImmediateU8(value);
    return constant;
}

IrValue& IrBuilder::ConstantU16(std::uint16_t value) {
    IrValue& constant = createLeafValue(program, IrType::U16);
    constant.SetImmediateU16(value);
    return constant;
}

}
