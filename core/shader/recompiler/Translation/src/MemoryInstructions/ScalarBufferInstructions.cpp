#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

MemoryInfo scalarMemoryInfoFromInstruction(const RdnaInstruction& inst, bool raw) {
    if (inst.family != RdnaInstructionFamily::SMEM) {
        throw std::runtime_error("scalarMemoryInfoFromInstruction requires an SMEM instruction");
    }
    // Raw address loads may take their 64-bit base from VCC; descriptor loads need four SGPRs.
    if (inst.source0.kind != RdnaOperandKind::ScalarRegister && !(raw && inst.source0.kind == RdnaOperandKind::VccLo)) {
        throw std::runtime_error("scalar memory base must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = raw ? ResourceKind::ScalarAddress : ResourceKind::ScalarBuffer;
    memory.offset = inst.memoryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.componentCount = inst.dataDwordCount;
    if (!raw) {
        memory.resource = inst.source0.reg / 4u;
    }
    return memory;
}

}

bool TranslationContext::sLoad(const RdnaInstruction& inst, bool raw) {
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, raw);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = raw ? getScalarAddressResource(baseCode) : getBufferResource(memory);
    const IrU32 offset = readU32(inst.source1);
    std::array<IrValue*, 16u> loaded{};
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        MemoryInfo scalar = memory;
        scalar.offset += component * 4u;
        scalar.dataDwords = 1u;
        scalar.componentIndex = component;
        const MemoryFlags flags = addMemoryInfo(scalar, inst.programCounter);
        if (raw) {
            loaded[component] = &ir.Emit(IrOpcode::LoadAddressU32, IrOpcodeType(IrOpcode::LoadAddressU32), {resource, &offset.Value(), &ir.Constant(0u), &ir.ConstantBool(true)}, flags);
        } else {
            loaded[component] = &ir.Emit(IrOpcode::ReadConstBuffer, IrOpcodeType(IrOpcode::ReadConstBuffer), {resource, &offset.Value()}, flags);
        }
    }
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        writeOperand(scalarDestinationOperand(inst.destination, component), loaded[component]);
    }
    return true;
}

}
