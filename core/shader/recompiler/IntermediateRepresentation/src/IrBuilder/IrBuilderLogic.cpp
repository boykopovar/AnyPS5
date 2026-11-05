#include "IntermediateRepresentation/IrBuilder.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

    IrValue& IrBuilder::Select(IrValue& condition, IrValue& trueValue, IrValue& falseValue) {
        if (trueValue.Type() != falseValue.Type()) {
            throw std::invalid_argument("IrBuilder::Select trueValue and falseValue type mismatch");
        }
        switch (trueValue.Type()) {
        case IrType::Bool:
            return Emit(IrOpcode::SelectU1, IrType::Bool, {&condition, &trueValue, &falseValue});
        case IrType::U32:
            return Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &trueValue, &falseValue});
        case IrType::F32:
            return Emit(IrOpcode::SelectF32, IrType::F32, {&condition, &trueValue, &falseValue});
        default:
            throw std::invalid_argument("IrBuilder::Select unsupported value type");
        }
    }

    IrValue& IrBuilder::IEqual(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::IEqual32, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::INotEqual(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::INotEqual32, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::ULessThan(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::ULessThan32, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::UGreaterThan(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::LogicalAnd(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::LogicalAnd, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::LogicalOr(IrValue& lhs, IrValue& rhs) {
        return Emit(IrOpcode::LogicalOr, IrType::Bool, {&lhs, &rhs});
    }

    IrValue& IrBuilder::LogicalNot(IrValue& value) {
        return Emit(IrOpcode::LogicalNot, IrType::Bool, {&value});
    }

}
