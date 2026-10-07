#include "SpirvBackend/SpirvAluEmitter.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

void EmitAluValue(SpirvModule& module, const IrValue& value, std::uint32_t resultId) {
    throw std::runtime_error("EmitAluValue not implemented");
}

void EmitAluValue(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId) {
    throw std::runtime_error("EmitAluValue not implemented");
}

void EmitGetThreadBitScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetThreadBitScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetScalarMaskTag(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetScalarMaskTag(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetScalarRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetVectorRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetVectorRegister(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetGotoVariable(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetGotoVariable(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetScc(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetScc(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetExec(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetExec(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetExecLo(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetExecLo(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetExecHi(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetExecHi(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetVcc(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetVcc(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetVccLo(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetVccLo(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetVccHi(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetVccHi(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitGetM0(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

void EmitSetM0(SpirvValueEmitContext& ctx, const IrValue& inst) { EmitUnreachable(ctx, inst); }

}
