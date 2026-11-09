#include "Optimization/SsaBuilder/SsaInstructionVisitor.hpp"
#include <stdexcept>

namespace ShaderRecompiler::Detail {

namespace {

ScalarReg expectScalarRegister(IrValue& operand, RegisterBank bank) {
    const GuestRegister reg = operand.Register();
    if (reg.bank != bank) {
        throw std::runtime_error("SsaInstructionVisitor expected a scalar register operand");
    }
    return static_cast<ScalarReg>(reg.index);
}

VectorReg expectVectorRegister(IrValue& operand, RegisterBank bank) {
    const GuestRegister reg = operand.Register();
    if (reg.bank != bank) {
        throw std::runtime_error("SsaInstructionVisitor expected a vector register operand");
    }
    return static_cast<VectorReg>(reg.index);
}

}

void VisitInstruction(Pass& pass, IrBlock& block, IrValue& inst) {
    switch (inst.Opcode()) {
        case IrOpcode::SetScalarRegister:
            pass.Write(expectScalarRegister(*inst.Argument(0), RegisterBank::Scalar), &block, inst.Argument(1));
            break;
        case IrOpcode::SetThreadBitScalarRegister:
            pass.Write(ThreadBitScalarReg{expectScalarRegister(*inst.Argument(0), RegisterBank::ThreadBitScalar)}, &block, inst.Argument(1));
            break;
        case IrOpcode::SetScalarMaskTag:
            pass.Write(ScalarMaskTag{expectScalarRegister(*inst.Argument(0), RegisterBank::ScalarMaskTag)}, &block, inst.Argument(1));
            break;
        case IrOpcode::SetVectorRegister:
            pass.Write(expectVectorRegister(*inst.Argument(0), RegisterBank::Vector), &block, inst.Argument(1));
            break;
        case IrOpcode::SetGotoVariable:
            pass.Write(GotoVariable{inst.Argument(0)->ImmediateU32()}, &block, inst.Argument(1));
            break;
        case IrOpcode::SetScc: pass.Write(SccTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetExec: pass.Write(ExecTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetExecLo: pass.Write(ExecLoTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetExecHi: pass.Write(ExecHiTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetVcc: pass.Write(VccTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetVccLo: pass.Write(VccLoTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetVccHi: pass.Write(VccHiTag{}, &block, inst.Argument(0)); break;
        case IrOpcode::SetM0: pass.Write(M0Tag{}, &block, inst.Argument(0)); break;
        case IrOpcode::GetScalarRegister:
            inst.ReplaceUsesWith(pass.Read(expectScalarRegister(*inst.Argument(0), RegisterBank::Scalar), &block), true);
            break;
        case IrOpcode::GetThreadBitScalarRegister:
            inst.ReplaceUsesWith(pass.Read(ThreadBitScalarReg{expectScalarRegister(*inst.Argument(0), RegisterBank::ThreadBitScalar)}, &block), true);
            break;
        case IrOpcode::GetScalarMaskTag:
            inst.ReplaceUsesWith(pass.Read(ScalarMaskTag{expectScalarRegister(*inst.Argument(0), RegisterBank::ScalarMaskTag)}, &block), true);
            break;
        case IrOpcode::GetVectorRegister:
            inst.ReplaceUsesWith(pass.Read(expectVectorRegister(*inst.Argument(0), RegisterBank::Vector), &block), true);
            break;
        case IrOpcode::GetGotoVariable:
            inst.ReplaceUsesWith(pass.Read(GotoVariable{inst.Argument(0)->ImmediateU32()}, &block), true);
            break;
        case IrOpcode::GetScc: inst.ReplaceUsesWith(pass.Read(SccTag{}, &block), true); break;
        case IrOpcode::GetExec: inst.ReplaceUsesWith(pass.Read(ExecTag{}, &block), true); break;
        case IrOpcode::GetExecLo: inst.ReplaceUsesWith(pass.Read(ExecLoTag{}, &block), true); break;
        case IrOpcode::GetExecHi: inst.ReplaceUsesWith(pass.Read(ExecHiTag{}, &block), true); break;
        case IrOpcode::GetVcc: inst.ReplaceUsesWith(pass.Read(VccTag{}, &block), true); break;
        case IrOpcode::GetVccLo: inst.ReplaceUsesWith(pass.Read(VccLoTag{}, &block), true); break;
        case IrOpcode::GetVccHi: inst.ReplaceUsesWith(pass.Read(VccHiTag{}, &block), true); break;
        case IrOpcode::GetM0: inst.ReplaceUsesWith(pass.Read(M0Tag{}, &block), true); break;
        default: break;
    }
}

}
