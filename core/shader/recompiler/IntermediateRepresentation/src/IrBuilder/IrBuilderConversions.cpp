#include "IntermediateRepresentation/IrBuilder.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

IrValue& IrBuilder::BitCastF32(IrValue& value) {
    return Emit(IrOpcode::BitCastF32U32, IrType::F32, {&value});
}

IrValue& IrBuilder::BitCastU32(IrValue& value) {
    return Emit(IrOpcode::BitCastU32F32, IrType::U32, {&value});
}

IrValue& IrBuilder::BitCastF16(IrValue& value) {
    IrValue& half = Emit(IrOpcode::ConvertU16U32, IrType::U16, {&value});
    return Emit(IrOpcode::BitCastF16U16, IrType::F16, {&half});
}

IrValue& IrBuilder::BitCastU32FromF16(IrValue& value) {
    IrValue& half = Emit(IrOpcode::BitCastU16F16, IrType::U16, {&value});
    return Emit(IrOpcode::ConvertU32U16, IrType::U32, {&half});
}

IrValue& IrBuilder::ConstructU64(IrValue& low, IrValue& high) {
    return Emit(IrOpcode::CompositeConstructU64, IrType::U64, {&low, &high});
}

IrValue& IrBuilder::CompositeExtract(IrValue& composite, std::uint32_t index) {
    IrOpcode opcode;
    switch (composite.Type()) {
    case IrType::U64:
        opcode = IrOpcode::CompositeExtractU64;
        break;
    case IrType::U32x2:
        opcode = IrOpcode::CompositeExtractU32x2;
        break;
    case IrType::U32x3:
        opcode = IrOpcode::CompositeExtractU32x3;
        break;
    case IrType::U32x4:
        opcode = IrOpcode::CompositeExtractU32x4;
        break;
    default:
        throw std::invalid_argument("IrBuilder::CompositeExtract unsupported composite type");
    }
    IrValue& indexValue = Constant(index);
    return Emit(opcode, IrType::U32, {&composite, &indexValue});
}

}
