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
    if (!raw) {
        throw std::runtime_error("s_buffer_store not yet implemented: writable scalar buffer descriptor required");
    }
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, true);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = getScalarAddressResource(baseCode);
    const IrU32 offset = readU32(inst.source1);
    IrValue& active = ir.ConstantBool(true);
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        MemoryInfo scalar = memory;
        scalar.offset += component * 4u;
        scalar.dataDwords = 1u;
        scalar.componentIndex = component;
        const IrU32 data = readU32(offsetOperand(inst.destination, component));
        (void)ir.Emit(IrOpcode::StoreAddressU32, IrType::Void, {resource, &offset.Value(), &ir.Constant(0u), &data.Value(), &active}, addMemoryInfo(scalar, inst.programCounter));
    }
    return true;
}

bool TranslationContext::sAtomic(const RdnaInstruction& inst, IrOpcode opcode) {
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, true);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = getScalarAddressResource(baseCode);
    const IrU32 offset = readU32(inst.source1);
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue& lane = ir.Emit(IrOpcode::LaneId, IrType::U32, {});
    IrValue& active = ir.IEqual(lane, ir.Constant(0u));
    IrValue* result;
    if (opcode == IrOpcode::AddressAtomicCmpSwap32) {
        const IrU32 desired = readU32(inst.destination);
        const IrU32 comparator = readU32(offsetOperand(inst.destination, 1u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), &desired.Value(), &comparator.Value(), &active}, flags);
    } else {
        const IrU32 value = readU32(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), &value.Value(), &active}, flags);
    }
    if (inst.glc) {
        IrValue& broadcast = ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {result, &ir.ConstantBool(true)});
        writeOperand(inst.destination, &broadcast);
    }
    return true;
}

bool TranslationContext::sAtomic64(const RdnaInstruction& inst, IrOpcode opcode) {
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, true);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = getScalarAddressResource(baseCode);
    const IrU32 offset = readU32(inst.source1);
    IrValue& active = ir.GetExec();
    const IrU64 value = readU64(inst.destination);
    IrValue* result;
    if (opcode == IrOpcode::AddressAtomicCmpSwap64) {
        const IrU64 comparator = readU64(offsetOperand(inst.destination, 2u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), &value.Value(), &comparator.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    } else {
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &offset.Value(), &ir.Constant(0u), &value.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    }
    if (inst.glc) {
        writeU32Pair(inst.destination, extractU64(IrU64(*result)));
    }
    return true;
}

bool TranslationContext::scalarBufferAtomic(const RdnaInstruction& inst, IrOpcode opcode) {
    if (inst.family != RdnaInstructionFamily::SMEM) {
        throw std::runtime_error("scalarBufferAtomic requires an SMEM instruction");
    }
    if (inst.source0.kind != RdnaOperandKind::ScalarRegister) {
        throw std::runtime_error("scalar buffer atomic descriptor must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = ResourceKind::Buffer;
    memory.resource = inst.source0.reg / 4u;
    memory.offset = inst.memoryOffset;
    memory.dataBits = 32u;
    memory.dataDwords = 1u;
    memory.componentCount = 1u;
    IrValue* resource = getBufferResource(memory);
    const IrU32 soffset = readU32(inst.source1);
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue& exec = ir.GetExec();
    IrValue* result;
    if (opcode == IrOpcode::BufferAtomicCmpSwap32) {
        const IrU32 desired = readU32(inst.destination);
        const IrU32 comparator = readU32(offsetOperand(inst.destination, 1u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &ir.Constant(0u), &ir.Constant(0u), &soffset.Value(), &desired.Value(), &comparator.Value(), &exec}, flags);
    } else {
        const IrU32 value = readU32(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &ir.Constant(0u), &ir.Constant(0u), &soffset.Value(), &value.Value(), &exec}, flags);
    }
    if (inst.glc) {
        writeOperand(inst.destination, result);
    }
    return true;
}

bool TranslationContext::scalarBufferAtomic64(const RdnaInstruction& inst, IrOpcode opcode) {
    MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, false);
    memory.kind = ResourceKind::Buffer;
    IrValue* resource = getBufferResource(memory);
    const IrU32 zero(ir.Constant(0u));
    const IrU32 soffset = readU32(inst.source1);
    IrValue& exec = ir.GetExec();
    IrValue* result;
    if (opcode == IrOpcode::BufferAtomicCmpSwap64) {
        const IrU64 desired = readU64(inst.destination);
        const IrU64 comparator = readU64(offsetOperand(inst.destination, 2u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &zero.Value(), &zero.Value(), &soffset.Value(), &desired.Value(), &comparator.Value(), &exec}, addMemoryInfo(memory, inst.programCounter));
    } else {
        const IrU64 value = readU64(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &zero.Value(), &zero.Value(), &soffset.Value(), &value.Value(), &exec}, addMemoryInfo(memory, inst.programCounter));
    }
    if (inst.glc) {
        writeOperand(inst.destination, result);
    }
    return true;
}

bool TranslationContext::sScratchLoad(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_load not yet implemented: wave-uniform or swizzled scratch layout required");
}

bool TranslationContext::sScratchStore(const RdnaInstruction& inst) {
    (void)inst;
    throw std::runtime_error("s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
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