#include "../FunctionLifter.hpp"

#include <cstddef>
#include <utility>

namespace Ps5Lift::Lifting {

void FunctionLifter::RandomNumber() {
    auto* value = _builder.CreateCall(_module.Runtime("ps5lift_random", llvm::FunctionType::get(I64(), false)));
    Write(0, _builder.CreateTrunc(value, Int(Bits(0))));
    SetFlag(PS5LIFT_CF, _builder.getTrue());
    for (const auto flag : {PS5LIFT_OF, PS5LIFT_SF, PS5LIFT_ZF, PS5LIFT_AF, PS5LIFT_PF}) SetFlag(flag, _builder.getFalse());
}

void FunctionLifter::ProcessorId() {
    Write(0, _builder.CreateZExt(_builder.CreateCall(_module.Runtime("ps5lift_processor_id", llvm::FunctionType::get(_builder.getInt32Ty(), false))), Int(Bits(0))));
}

void FunctionLifter::TimeStamp() {
    auto* ticks = _builder.CreateCall(_module.Runtime("ps5lift_read_tsc", llvm::FunctionType::get(I64(), false)));
    SetGpr(PS5LIFT_RAX, _builder.CreateAnd(ticks, _builder.getInt64(0xffffffff)));
    SetGpr(PS5LIFT_RDX, _builder.CreateLShr(ticks, _builder.getInt64(32)));
}

void FunctionLifter::TimeStampAndProcessor() {
    TimeStamp();
    SetGpr(PS5LIFT_RCX, _builder.CreateZExt(_builder.CreateCall(_module.Runtime("ps5lift_processor_id", llvm::FunctionType::get(_builder.getInt32Ty(), false))), I64()));
}

void FunctionLifter::LoadFlagsIntoAh() {
    llvm::Value* value = _builder.getInt8(2);
    for (const auto& [flag, bit] : {std::pair{PS5LIFT_SF, 7}, std::pair{PS5LIFT_ZF, 6}, std::pair{PS5LIFT_AF, 4}, std::pair{PS5LIFT_PF, 2}, std::pair{PS5LIFT_CF, 0}})
        value = _builder.CreateOr(value, _builder.CreateShl(_builder.CreateZExt(Flag(flag), _builder.getInt8Ty()), bit));
    WriteRegister(ZYDIS_REGISTER_AH, value);
}

void FunctionLifter::StoreAhIntoFlags() {
    auto* value = ReadRegister(ZYDIS_REGISTER_AH);
    for (const auto& [flag, bit] : {std::pair{PS5LIFT_SF, 7}, std::pair{PS5LIFT_ZF, 6}, std::pair{PS5LIFT_AF, 4}, std::pair{PS5LIFT_PF, 2}, std::pair{PS5LIFT_CF, 0}})
        SetFlag(flag, _builder.CreateTrunc(_builder.CreateLShr(value, bit), _builder.getInt1Ty()));
}

void FunctionLifter::CacheLine(const char* function) {
    _builder.CreateCall(_module.Runtime(function, llvm::FunctionType::get(_builder.getVoidTy(), {I64()}, false)), {Address(Operand(0))});
}

void FunctionLifter::Cpuid() {
    for (const int index : {PS5LIFT_RAX, PS5LIFT_RCX}) StateStore(Gpr(index), offsetof(Ps5LiftState, Gpr) + 8 * static_cast<std::size_t>(index));
    _builder.CreateCall(_module.Runtime("ps5lift_cpuid", llvm::FunctionType::get(_builder.getVoidTy(), {_module.Pointer(), I64()}, false)), {_state, _builder.getInt64(_instruction->Address)});
    for (const int index : {PS5LIFT_RAX, PS5LIFT_RBX, PS5LIFT_RCX, PS5LIFT_RDX}) SetGpr(index, StateLoad(I64(), offsetof(Ps5LiftState, Gpr) + 8 * static_cast<std::size_t>(index)));
}

void FunctionLifter::ExtendedControlRegister() {
    auto* value = _builder.CreateCall(_module.Runtime("ps5lift_xgetbv", llvm::FunctionType::get(I64(), {I64(), _builder.getInt32Ty()}, false)),
                                      {_builder.getInt64(_instruction->Address), _builder.CreateTrunc(Gpr(PS5LIFT_RCX), _builder.getInt32Ty())});
    SetGpr(PS5LIFT_RAX, _builder.CreateAnd(value, _builder.getInt64(0xffffffff)));
    SetGpr(PS5LIFT_RDX, _builder.CreateLShr(value, _builder.getInt64(32)));
}

}
