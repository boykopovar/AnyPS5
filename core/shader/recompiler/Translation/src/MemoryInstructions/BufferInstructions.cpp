#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

MemoryInfo bufferMemoryInfoFromInstruction(const RdnaInstruction& inst) {
    if (inst.family != RdnaInstructionFamily::MUBUF && inst.family != RdnaInstructionFamily::MTBUF) {
        throw std::runtime_error("bufferMemoryInfoFromInstruction requires a MUBUF or MTBUF instruction");
    }
    if (inst.source1.kind != RdnaOperandKind::ScalarRegister) {
        throw std::runtime_error("buffer resource descriptor must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = ResourceKind::Buffer;
    memory.resource = inst.source1.reg / 4u;
    memory.offset = inst.memoryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.dataBits = inst.dataBits;
    memory.componentCount = inst.dataDwordCount;
    memory.dataFormat = inst.dataFormat;
    memory.numberFormat = inst.numberFormat;
    memory.dataSigned = inst.dataSigned;
    memory.typed = inst.typed;
    memory.formatted = inst.formatted;
    memory.idxen = inst.idxen;
    memory.offen = inst.offen;
    return memory;
}

}

bool TranslationContext::bufferLoad(const RdnaInstruction& inst) {
    const MemoryInfo memory = bufferMemoryInfoFromInstruction(inst);
    IrOpcode opcode;
    switch (memory.dataBits) {
    case 8u:
        opcode = IrOpcode::LoadBufferU8;
        break;
    case 16u:
        opcode = IrOpcode::LoadBufferU16;
        break;
    case 32u:
        switch (memory.dataDwords) {
        case 1u:
            opcode = IrOpcode::LoadBufferU32;
            break;
        case 2u:
            opcode = IrOpcode::LoadBufferU32x2;
            break;
        case 3u:
            opcode = IrOpcode::LoadBufferU32x3;
            break;
        case 4u:
            opcode = IrOpcode::LoadBufferU32x4;
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }
    IrValue* resource = getBufferResource(memory);
    const BufferAddress address = readBufferAddress(inst);
    IrValue& exec = ir.GetExec();
    IrValue& loaded = ir.Emit(opcode, IrOpcodeType(opcode), {resource, &address.index.Value(), &address.offset.Value(), &address.soffset.Value(), &exec}, addMemoryInfo(memory, inst.programCounter));
    if (memory.dataBits != 32u) {
        writeOperand(inst.destination, &widenSubdword(&loaded, memory.dataBits, memory.dataSigned).Value());
    } else if (memory.dataDwords == 1u) {
        writeOperand(inst.destination, &loaded);
    } else {
        for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
            writeOperand(offsetOperand(inst.destination, component), &ir.CompositeExtract(loaded, component));
        }
    }
    return true;
}

bool TranslationContext::bufferStore(const RdnaInstruction& inst) {
    const MemoryInfo memory = bufferMemoryInfoFromInstruction(inst);
    IrValue* resource = getBufferResource(memory);
    const BufferAddress address = readBufferAddress(inst);
    const IrU32 data = readU32(inst.destination);
    IrOpcode opcode;
    IrValue* value;
    switch (memory.dataBits) {
    case 8u:
        opcode = IrOpcode::StoreBufferU8;
        value = narrowSubdword(data, 8u);
        break;
    case 16u:
        opcode = IrOpcode::StoreBufferU16;
        value = narrowSubdword(data, 16u);
        break;
    case 32u:
        switch (memory.dataDwords) {
        case 1u:
            opcode = IrOpcode::StoreBufferU32;
            value = &data.Value();
            break;
        case 2u: {
            opcode = IrOpcode::StoreBufferU32x2;
            const IrU32 second = readU32(offsetOperand(inst.destination, 1u));
            value = &ir.Emit(IrOpcode::CompositeConstructU32x2, IrType::U32x2, {&data.Value(), &second.Value()});
            break;
        }
        case 3u: {
            opcode = IrOpcode::StoreBufferU32x3;
            const IrU32 second = readU32(offsetOperand(inst.destination, 1u));
            const IrU32 third = readU32(offsetOperand(inst.destination, 2u));
            value = &ir.Emit(IrOpcode::CompositeConstructU32x3, IrType::U32x3, {&data.Value(), &second.Value(), &third.Value()});
            break;
        }
        case 4u: {
            opcode = IrOpcode::StoreBufferU32x4;
            const IrU32 second = readU32(offsetOperand(inst.destination, 1u));
            const IrU32 third = readU32(offsetOperand(inst.destination, 2u));
            const IrU32 fourth = readU32(offsetOperand(inst.destination, 3u));
            value = &ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&data.Value(), &second.Value(), &third.Value(), &fourth.Value()});
            break;
        }
        default:
            return false;
        }
        break;
    default:
        return false;
    }
    IrValue& exec = ir.GetExec();
    (void)ir.Emit(opcode, IrType::Void, {resource, &address.index.Value(), &address.offset.Value(), &address.soffset.Value(), value, &exec}, addMemoryInfo(memory, inst.programCounter));
    return true;
}

bool TranslationContext::bufferAtomic(const RdnaInstruction& inst, IrOpcode opcode) {
    const MemoryInfo memory = bufferMemoryInfoFromInstruction(inst);
    IrValue* resource = getBufferResource(memory);
    const BufferAddress address = readBufferAddress(inst);
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue& exec = ir.GetExec();
    IrValue* result;
    if (opcode == IrOpcode::BufferAtomicCmpSwap32) {
        const IrU32 desired = readU32(inst.destination);
        const IrU32 comparator = readU32(offsetOperand(inst.destination, 1u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &address.index.Value(), &address.offset.Value(), &address.soffset.Value(), &desired.Value(), &comparator.Value(), &exec}, flags);
    } else if (opcode == IrOpcode::BufferAtomicSwap64 || opcode == IrOpcode::BufferAtomicOr64) {
        const IrU64 value = readU64(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &address.index.Value(), &address.offset.Value(), &address.soffset.Value(), &value.Value(), &exec}, flags);
    } else {
        const IrU32 value = readU32(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, &address.index.Value(), &address.offset.Value(), &address.soffset.Value(), &value.Value(), &exec}, flags);
    }
    if (inst.glc) {
        writeOperand(inst.destination, result);
    }
    return true;
}

}
