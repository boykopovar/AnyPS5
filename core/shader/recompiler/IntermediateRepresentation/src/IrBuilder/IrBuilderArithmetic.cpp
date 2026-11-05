#include "IntermediateRepresentation/IrBuilder.hpp"

namespace ShaderRecompiler {

IrValue& IrBuilder::IAdd(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::IAdd32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::ISub(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::ISub32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::IMul(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::IMul32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::ShiftLeftLogical(IrValue& value, IrValue& shift) {
    return Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&value, &shift});
}

IrValue& IrBuilder::ShiftRightLogical(IrValue& value, IrValue& shift) {
    return Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&value, &shift});
}

IrValue& IrBuilder::ShiftRightArithmetic(IrValue& value, IrValue& shift) {
    return Emit(IrOpcode::ShiftRightArithmetic32, IrType::U32, {&value, &shift});
}

IrValue& IrBuilder::BitwiseAnd(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::BitwiseOr(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::BitwiseOr32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::BitwiseXor(IrValue& lhs, IrValue& rhs) {
    return Emit(IrOpcode::BitwiseXor32, IrType::U32, {&lhs, &rhs});
}

IrValue& IrBuilder::BitwiseNot(IrValue& value) {
    return Emit(IrOpcode::BitwiseNot32, IrType::U32, {&value});
}

}
