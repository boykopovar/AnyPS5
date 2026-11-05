#include "Optimization/SsaBuilder/SsaVariable.hpp"

namespace ShaderRecompiler::Detail {

namespace {

struct VariableTypeVisitor {
    IrType operator()(ScalarReg) const { return IrType::U32; }
    IrType operator()(ThreadBitScalarReg) const { return IrType::Bool; }
    IrType operator()(ScalarMaskTag) const { return IrType::Bool; }
    IrType operator()(VectorReg) const { return IrType::U32; }
    IrType operator()(GotoVariable) const { return IrType::Bool; }
    IrType operator()(SccTag) const { return IrType::Bool; }
    IrType operator()(ExecTag) const { return IrType::Bool; }
    IrType operator()(ExecLoTag) const { return IrType::U32; }
    IrType operator()(ExecHiTag) const { return IrType::U32; }
    IrType operator()(VccTag) const { return IrType::Bool; }
    IrType operator()(VccLoTag) const { return IrType::U32; }
    IrType operator()(VccHiTag) const { return IrType::U32; }
    IrType operator()(M0Tag) const { return IrType::U32; }
};

}

IrType VariableType(const Variable& variable) {
    return std::visit(VariableTypeVisitor{}, variable);
}

}
