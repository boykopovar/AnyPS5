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

bool TranslationContext::sStore(const RdnaInstruction& inst, bool raw) {
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, raw);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = raw ? getScalarAddressResource(baseCode) : getBufferResource(memory);
    const IrU32 offset = readU32(inst.source1);
    const IrU32 data = readU32(inst.destination);
    IrValue* value = &data.Value();
    IrOpcode opcode;
    if (raw) {
        opcode = memory.dataDwords == 1u ? IrOpcode::StoreAddressU32
               : memory.dataDwords == 2u ? IrOpcode::StoreAddressU32x2
                                        : IrOpcode::StoreAddressU32x4;
    } else {
        opcode = memory.dataDwords == 1u ? IrOpcode::StoreBufferU32
               : memory.dataDwords == 2u ? IrOpcode::StoreBufferU32x2
                                        : IrOpcode::StoreBufferU32x4;
    }
    if (memory.dataDwords != 1u) {
        const IrU32 second = readU32(offsetOperand(inst.destination, 1u));
        if (memory.dataDwords == 2u) {
            value = &ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&data.Value(), &second.Value()});
        } else {
            const IrU32 third = readU32(offsetOperand(inst.destination, 2u));
            const IrU32 fourth = readU32(offsetOperand(inst.destination, 3u));
            value = &ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4,
                             {&data.Value(), &second.Value(), &third.Value(), &fourth.Value()});
        }
    }
    IrValue& exec = ir.GetExec();
    if (raw) {
        (void)ir.Emit(opcode, IrType::Void, {resource, &offset.Value(), &ir.Constant(0u), value, &exec},
                      addMemoryInfo(memory, inst.programCounter));
    } else {
        (void)ir.Emit(opcode, IrType::Void,
                      {resource, &ir.Constant(0u), &offset.Value(), &ir.Constant(0u), value, &exec},
                      addMemoryInfo(memory, inst.programCounter));
    }
    return true;
}

bool TranslationContext::sScratchStore(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
}

bool TranslationContext::sScratchLoad(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_load not yet implemented: wave-uniform or swizzled scratch layout required");
}

void TranslationContext::TranslateCodeTableLoad(const RdnaInstruction& instruction, const ControlFlowGraph::CodeTableLoad& table) {
    if (table.values.empty()) throw std::runtime_error("empty shader code table");
    const IrU32 index = readRawU32(instruction.source1);
    IrU32 low(ir.Constant(static_cast<std::uint32_t>(table.values.back())));
    IrU32 high(ir.Constant(static_cast<std::uint32_t>(table.values.back() >> 32u)));
    for (std::size_t entry = table.values.size() - 1u; entry-- > 0u;) {
        auto& matches = ir.IEqual(index.Value(), ir.Constant(static_cast<std::uint32_t>(entry * instruction.dataDwordCount * 4u)));
        low = IrU32(ir.Select(matches, ir.Constant(static_cast<std::uint32_t>(table.values[entry])), low.Value()));
        if (instruction.dataDwordCount == 2u) high = IrU32(ir.Select(matches, ir.Constant(static_cast<std::uint32_t>(table.values[entry] >> 32u)), high.Value()));
    }
    if (instruction.dataDwordCount == 1u) {
        writeRawU32(instruction.destination, low);
        return;
    }
    writeU32Pair(instruction.destination, {low, high});
}

}