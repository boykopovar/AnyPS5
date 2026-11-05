#include "IntermediateRepresentation/IrBuilder.hpp"

namespace ShaderRecompiler {

IrValue& IrBuilder::GetGotoVariable(std::uint32_t id) {
    IrValue& idValue = Constant(id);
    return Emit(IrOpcode::GetGotoVariable, IrType::Bool, {&idValue});
}

void IrBuilder::SetGotoVariable(std::uint32_t id, IrValue& value) {
    IrValue& idValue = Constant(id);
    (void)Emit(IrOpcode::SetGotoVariable, IrType::Void, {&idValue, &value});
}

IrValue& IrBuilder::GetScc() {
    return Emit(IrOpcode::GetScc, IrType::Bool, {});
}

void IrBuilder::SetScc(IrValue& value) {
    (void)Emit(IrOpcode::SetScc, IrType::Void, {&value});
}

IrValue& IrBuilder::GetExec() {
    return Emit(IrOpcode::GetExec, IrType::Bool, {});
}

void IrBuilder::SetExec(IrValue& value) {
    (void)Emit(IrOpcode::SetExec, IrType::Void, {&value});
}

IrValue& IrBuilder::GetExecLo() {
    return Emit(IrOpcode::GetExecLo, IrType::U32, {});
}

void IrBuilder::SetExecLo(IrValue& value) {
    (void)Emit(IrOpcode::SetExecLo, IrType::Void, {&value});
}

IrValue& IrBuilder::GetExecHi() {
    return Emit(IrOpcode::GetExecHi, IrType::U32, {});
}

void IrBuilder::SetExecHi(IrValue& value) {
    (void)Emit(IrOpcode::SetExecHi, IrType::Void, {&value});
}

IrValue& IrBuilder::GetVcc() {
    return Emit(IrOpcode::GetVcc, IrType::Bool, {});
}

void IrBuilder::SetVcc(IrValue& value) {
    (void)Emit(IrOpcode::SetVcc, IrType::Void, {&value});
}

IrValue& IrBuilder::GetVccLo() {
    return Emit(IrOpcode::GetVccLo, IrType::U32, {});
}

void IrBuilder::SetVccLo(IrValue& value) {
    (void)Emit(IrOpcode::SetVccLo, IrType::Void, {&value});
}

IrValue& IrBuilder::GetVccHi() {
    return Emit(IrOpcode::GetVccHi, IrType::U32, {});
}

void IrBuilder::SetVccHi(IrValue& value) {
    (void)Emit(IrOpcode::SetVccHi, IrType::Void, {&value});
}

IrValue& IrBuilder::GetM0() {
    return Emit(IrOpcode::GetM0, IrType::U32, {});
}

void IrBuilder::SetM0(IrValue& value) {
    (void)Emit(IrOpcode::SetM0, IrType::Void, {&value});
}

}
