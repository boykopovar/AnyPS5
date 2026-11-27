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

bool scalarAtomicCompares(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::AddressAtomicCmpSwap32:
    case IrOpcode::AddressAtomicCmpSwap64:
    case IrOpcode::BufferAtomicCmpSwap32:
    case IrOpcode::BufferAtomicCmpSwap64:
        return true;
    default:
        return false;
    }
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
    IrValue& uniform = ir.ConstantBool(true);
    if (raw) {
        (void)ir.Emit(opcode, IrType::Void, {resource, &offset.Value(), &ir.Constant(0u), value, &uniform},
                      addMemoryInfo(memory, inst.programCounter));
    } else {
        (void)ir.Emit(opcode, IrType::Void,
                      {resource, &ir.Constant(0u), &offset.Value(), &ir.Constant(0u), value, &uniform},
                      addMemoryInfo(memory, inst.programCounter));
    }
    return true;
}

bool TranslationContext::sAtomic(const RdnaInstruction& inst, bool raw, IrOpcode opcode) {
    const bool wide = IrOpcodeType(opcode) == IrType::U64;
    MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, raw);
    memory.dataDwords = wide ? 2u : 1u;
    memory.componentCount = memory.dataDwords;
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = raw ? getScalarAddressResource(baseCode) : getBufferResource(memory);
    const IrU32 offset = readU32(inst.source1);
    IrValue* value = nullptr;
    IrValue* comparator = nullptr;
    if (wide) {
        const IrU64 data = readU64(inst.destination);
        value = &data.Value();
        if (scalarAtomicCompares(opcode)) {
            const IrU64 other = readU64(offsetOperand(inst.destination, 2u));
            comparator = &other.Value();
        }
    } else {
        const IrU32 data = readU32(inst.destination);
        value = &data.Value();
        if (scalarAtomicCompares(opcode)) {
            const IrU32 other = readU32(offsetOperand(inst.destination, 1u));
            comparator = &other.Value();
        }
    }
    IrValue& uniform = ir.ConstantBool(true);
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue* result = nullptr;
    if (raw) {
        if (comparator != nullptr) {
            result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), value, comparator, &uniform}, flags);
        } else {
            result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), value, &uniform}, flags);
        }
    } else {
        if (comparator != nullptr) {
            result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &ir.Constant(0u), &offset.Value(), &ir.Constant(0u), value, comparator, &uniform}, flags);
        } else {
            result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &ir.Constant(0u), &offset.Value(), &ir.Constant(0u), value, &uniform}, flags);
        }
    }
    if (inst.glc) writeOperand(inst.destination, result);
    return true;
}

bool TranslationContext::sScratch(const RdnaInstruction& inst, bool store) {
    const std::uint32_t components = inst.dataDwordCount == 0u ? 1u : inst.dataDwordCount;
    const IrU32 lane(ir.Emit(IrOpcode::LaneId, IrOpcodeType(IrOpcode::LaneId), {}));
    const IrU32 laneBytes(ir.IMul(lane.Value(), ir.Constant(program.Info().scratchDwords * 4u)));
    IrU32 offset(ir.ISub(readU32(inst.source1).Value(), laneBytes.Value()));
    IrValue* resource = &ir.Emit(IrOpcode::GetScratchResource, IrOpcodeType(IrOpcode::GetScratchResource), {});
    IrValue& uniform = ir.ConstantBool(true);
    for (std::uint32_t component = 0u; component < components; ++component) {
        if (component != 0u) offset = IrU32(ir.IAdd(offset.Value(), ir.Constant(4u)));
        MemoryInfo memory{};
        memory.kind = ResourceKind::Scratch;
        memory.offset = inst.memoryOffset;
        memory.dataDwords = 1u;
        memory.componentCount = 1u;
        const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
        if (store) {
            const IrU32 data = readU32(offsetOperand(inst.destination, component));
            (void)ir.Emit(IrOpcode::StoreAddressU32, IrOpcodeType(IrOpcode::StoreAddressU32), {resource, &offset.Value(), &ir.Constant(0u), &data.Value(), &uniform}, flags);
            continue;
        }
        IrValue& loaded = ir.Emit(IrOpcode::LoadAddressU32, IrOpcodeType(IrOpcode::LoadAddressU32), {resource, &offset.Value(), &ir.Constant(0u), &uniform}, flags);
        IrValue& value = ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&loaded, &uniform});
        writeOperand(scalarDestinationOperand(inst.destination, component), &value);
    }
    return true;
}

bool TranslationContext::sScratchStore(const RdnaInstruction& inst) {
    return sScratch(inst, true);
}

bool TranslationContext::sScratchLoad(const RdnaInstruction& inst) {
    return sScratch(inst, false);
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