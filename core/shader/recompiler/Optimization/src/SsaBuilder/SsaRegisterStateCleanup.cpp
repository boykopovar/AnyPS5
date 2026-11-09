#include "Optimization/SsaBuilder/SsaRegisterStateCleanup.hpp"

namespace ShaderRecompiler::Detail {

bool IsRegisterStateWrite(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::SetScalarRegister:
        case IrOpcode::SetThreadBitScalarRegister:
        case IrOpcode::SetScalarMaskTag:
        case IrOpcode::SetVectorRegister:
        case IrOpcode::SetGotoVariable:
        case IrOpcode::SetScc:
        case IrOpcode::SetExec:
        case IrOpcode::SetExecLo:
        case IrOpcode::SetExecHi:
        case IrOpcode::SetVcc:
        case IrOpcode::SetVccLo:
        case IrOpcode::SetVccHi:
        case IrOpcode::SetM0:
            return true;
        default:
            return false;
    }
}

void RemoveRegisterStatePseudos(std::span<IrBlock* const> blocks) {
    for (IrBlock* block : blocks) {
        auto& instructions = block->Instructions();
        for (auto it = instructions.begin(); it != instructions.end();) {
            IrValue* inst = *it;
            if (!IsRegisterStateWrite(inst->Opcode())) {
                ++it;
                continue;
            }
            inst->Invalidate();
            it = instructions.erase(it);
            inst->SetParent(nullptr);
        }
    }
}

}
