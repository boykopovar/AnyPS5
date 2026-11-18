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
        case IrType::U8: {
            IrValue& trueU32 = Emit(IrOpcode::ConvertU32U8, IrType::U32, {&trueValue});
            IrValue& falseU32 = Emit(IrOpcode::ConvertU32U8, IrType::U32, {&falseValue});
            IrValue& sel = Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &trueU32, &falseU32});
            return Emit(IrOpcode::ConvertU8U32, IrType::U8, {&sel});
        }
        case IrType::U16: {
            IrValue& trueU32 = Emit(IrOpcode::ConvertU32U16, IrType::U32, {&trueValue});
            IrValue& falseU32 = Emit(IrOpcode::ConvertU32U16, IrType::U32, {&falseValue});
            IrValue& sel = Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &trueU32, &falseU32});
            return Emit(IrOpcode::ConvertU16U32, IrType::U16, {&sel});
        }
        case IrType::F16: {
            IrValue& trueU16 = Emit(IrOpcode::BitCastU16F16, IrType::U16, {&trueValue});
            IrValue& falseU16 = Emit(IrOpcode::BitCastU16F16, IrType::U16, {&falseValue});
            IrValue& selU16 = Select(condition, trueU16, falseU16);
            return Emit(IrOpcode::BitCastF16U16, IrType::F16, {&selU16});
        }
        case IrType::U64: {
            IrValue& trueLo = CompositeExtract(trueValue, 0u);
            IrValue& trueHi = CompositeExtract(trueValue, 1u);
            IrValue& falseLo = CompositeExtract(falseValue, 0u);
            IrValue& falseHi = CompositeExtract(falseValue, 1u);
            return ConstructU64(Select(condition, trueLo, falseLo), Select(condition, trueHi, falseHi));
        }
        case IrType::U32x2: {
            IrValue& true0 = CompositeExtract(trueValue, 0u);
            IrValue& true1 = CompositeExtract(trueValue, 1u);
            IrValue& false0 = CompositeExtract(falseValue, 0u);
            IrValue& false1 = CompositeExtract(falseValue, 1u);
            return Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&Select(condition, true0, false0), &Select(condition, true1, false1)});
        }
        case IrType::U32x3: {
            IrValue& true0 = CompositeExtract(trueValue, 0u);
            IrValue& true1 = CompositeExtract(trueValue, 1u);
            IrValue& true2 = CompositeExtract(trueValue, 2u);
            IrValue& false0 = CompositeExtract(falseValue, 0u);
            IrValue& false1 = CompositeExtract(falseValue, 1u);
            IrValue& false2 = CompositeExtract(falseValue, 2u);
            return Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&Select(condition, true0, false0), &Select(condition, true1, false1), &Select(condition, true2, false2)});
        }
        case IrType::U32x4: {
            IrValue& true0 = CompositeExtract(trueValue, 0u);
            IrValue& true1 = CompositeExtract(trueValue, 1u);
            IrValue& true2 = CompositeExtract(trueValue, 2u);
            IrValue& true3 = CompositeExtract(trueValue, 3u);
            IrValue& false0 = CompositeExtract(falseValue, 0u);
            IrValue& false1 = CompositeExtract(falseValue, 1u);
            IrValue& false2 = CompositeExtract(falseValue, 2u);
            IrValue& false3 = CompositeExtract(falseValue, 3u);
            return Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&Select(condition, true0, false0), &Select(condition, true1, false1), &Select(condition, true2, false2), &Select(condition, true3, false3)});
        }
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
