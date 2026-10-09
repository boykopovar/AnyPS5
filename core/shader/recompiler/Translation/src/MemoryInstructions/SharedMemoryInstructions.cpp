#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

RdnaOperand makeM0Operand() {
    RdnaOperand operand{};
    operand.kind = RdnaOperandKind::M0;
    return operand;
}

MemoryInfo sharedMemoryInfoFromInstruction(const RdnaInstruction& inst) {
    if (inst.family != RdnaInstructionFamily::DS) {
        throw std::runtime_error("sharedMemoryInfoFromInstruction requires a DS instruction");
    }
    MemoryInfo memory;
    memory.kind = inst.gds ? ResourceKind::Gds : ResourceKind::Lds;
    memory.offset = inst.memoryOffset;
    memory.secondaryOffset = inst.secondaryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.dataBits = inst.dataBits;
    memory.componentCount = inst.dataDwordCount;
    memory.dataSigned = inst.dataSigned;
    return memory;
}

}

bool TranslationContext::dsAtomic(const RdnaInstruction& inst, IrOpcode opcode, bool returnsValue) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(inst.source0);
    const IrU32 value = readU32(inst.source1);
    IrValue& active = ir.GetExec();
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &value.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    if (returnsValue) {
        writeOperand(inst.destination, &result);
    }
    return true;
}

bool TranslationContext::dsWrxchg2(const RdnaInstruction& inst) {
    MemoryInfo first = sharedMemoryInfoFromInstruction(inst);
    const std::uint32_t width = first.dataDwords / 2u;
    if (width == 2u && inst.gds) {
        throw std::runtime_error("64-bit GDS atomics are not supported");
    }
    first.dataDwords = width;
    first.componentCount = width;
    MemoryInfo second = first;
    second.offset = first.secondaryOffset;
    const IrU32 address = readU32(inst.source0);
    const auto data = [&](const RdnaOperand& operand) { return width == 2u ? &readU64(operand).Value() : &readU32(operand).Value(); };
    IrValue* firstValue = data(inst.source1);
    IrValue* secondValue = data(inst.source2);
    IrValue& active = ir.GetExec();
    const IrOpcode opcode = width == 2u ? IrOpcode::SharedAtomicSwap64 : IrOpcode::SharedAtomicSwap32;
    IrValue& firstOld = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), firstValue, &active}, addMemoryInfo(first, inst.programCounter));
    IrValue& secondOld = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), secondValue, &active}, addMemoryInfo(second, inst.programCounter));
    writeOperand(inst.destination, &firstOld);
    writeOperand(offsetOperand(inst.destination, width), &secondOld);
    return true;
}

IrValue* TranslationContext::loadSharedU32(std::uint32_t width, IrU32 address, const MemoryInfo& memory, std::uint32_t pc) {
    IrOpcode opcode;
    switch (width) {
    case 1u:
        opcode = IrOpcode::LoadSharedU32;
        break;
    case 2u:
        opcode = IrOpcode::LoadSharedU32x2;
        break;
    case 3u:
        opcode = IrOpcode::LoadSharedU32x3;
        break;
    case 4u:
        opcode = IrOpcode::LoadSharedU32x4;
        break;
    default:
        throw std::runtime_error("loadSharedU32 does not support the requested width");
    }
    IrValue& active = ir.GetExec();
    return &ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &active}, addMemoryInfo(memory, pc));
}

IrValue* TranslationContext::extractSharedU32(IrValue* value, std::uint32_t width, std::uint32_t index) {
    if (width == 1u) {
        return value;
    }
    return &ir.CompositeExtract(*value, index);
}

void TranslationContext::writeSharedU32(std::uint32_t width, IrU32 address, const std::array<IrValue*, 4>& values, const MemoryInfo& memory, std::uint32_t pc) {
    IrValue& active = ir.GetExec();
    switch (width) {
    case 1u:
        (void)ir.Emit(IrOpcode::WriteSharedU32, IrType::Void, {&address.Value(), values[0], &active}, addMemoryInfo(memory, pc));
        break;
    case 2u:
        (void)ir.Emit(IrOpcode::WriteSharedU32x2, IrType::Void, {&address.Value(), values[0], values[1], &active}, addMemoryInfo(memory, pc));
        break;
    case 3u:
        (void)ir.Emit(IrOpcode::WriteSharedU32x3, IrType::Void, {&address.Value(), values[0], values[1], values[2], &active}, addMemoryInfo(memory, pc));
        break;
    case 4u:
        (void)ir.Emit(IrOpcode::WriteSharedU32x4, IrType::Void, {&address.Value(), values[0], values[1], values[2], values[3], &active}, addMemoryInfo(memory, pc));
        break;
    default:
        throw std::runtime_error("writeSharedU32 does not support the requested width");
    }
}

bool TranslationContext::dsRead(const RdnaInstruction& inst) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(inst.source0);
    if (memory.dataBits == 32u) {
        const std::uint32_t width = memory.dataDwords;
        IrValue* loaded = loadSharedU32(width, address, memory, inst.programCounter);
        for (std::uint32_t index = 0u; index < width; ++index) {
            writeOperand(offsetOperand(inst.destination, index), extractSharedU32(loaded, width, index));
        }
        return true;
    }
    const IrOpcode opcode = memory.dataBits == 8u ? IrOpcode::LoadSharedU8 : IrOpcode::LoadSharedU16;
    IrValue& active = ir.GetExec();
    IrValue& loaded = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    writeOperand(inst.destination, &widenSubdword(&loaded, memory.dataBits, memory.dataSigned).Value());
    return true;
}

bool TranslationContext::dsRead2(const RdnaInstruction& inst) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const std::uint32_t width = memory.dataDwords / 2u;
    const IrU32 address = readU32(inst.source0);
    MemoryInfo first = memory;
    first.dataDwords = width;
    first.componentCount = width;
    IrValue* firstValue = loadSharedU32(width, address, first, inst.programCounter);
    IrValue* secondValue = firstValue;
    if (memory.secondaryOffset != memory.offset) {
        MemoryInfo second = first;
        second.offset = memory.secondaryOffset;
        secondValue = loadSharedU32(width, address, second, inst.programCounter);
    }
    for (std::uint32_t index = 0u; index < width; ++index) {
        writeOperand(offsetOperand(inst.destination, index), extractSharedU32(firstValue, width, index));
        writeOperand(offsetOperand(inst.destination, width + index), extractSharedU32(secondValue, width, index));
    }
    return true;
}

bool TranslationContext::dsWrite(const RdnaInstruction& inst) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const std::uint32_t width = memory.dataDwords;
    std::array<IrValue*, 4> values{};
    for (std::uint32_t index = 0u; index < width; ++index) {
        values[index] = &readU32(offsetOperand(inst.source1, index)).Value();
    }
    const IrU32 address = readU32(inst.source0);
    if (memory.dataBits == 32u) {
        writeSharedU32(width, address, values, memory, inst.programCounter);
        return true;
    }
    const IrOpcode opcode = memory.dataBits == 8u ? IrOpcode::WriteSharedU8 : IrOpcode::WriteSharedU16;
    IrValue* narrowed = narrowSubdword(IrU32(*values[0]), memory.dataBits);
    IrValue& active = ir.GetExec();
    (void)ir.Emit(opcode, IrType::Void, {&address.Value(), narrowed, &active}, addMemoryInfo(memory, inst.programCounter));
    return true;
}

bool TranslationContext::dsWrite2(const RdnaInstruction& inst) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const std::uint32_t width = memory.dataDwords / 2u;
    const IrU32 address = readU32(inst.source0);
    std::array<IrValue*, 4> firstValues{};
    std::array<IrValue*, 4> secondValues{};
    for (std::uint32_t index = 0u; index < width; ++index) {
        firstValues[index] = &readU32(offsetOperand(inst.source1, index)).Value();
        secondValues[index] = &readU32(offsetOperand(inst.source2, index)).Value();
    }
    MemoryInfo first = memory;
    first.dataDwords = width;
    first.componentCount = width;
    writeSharedU32(width, address, firstValues, first, inst.programCounter);
    if (memory.secondaryOffset != memory.offset) {
        MemoryInfo second = first;
        second.offset = memory.secondaryOffset;
        writeSharedU32(width, address, secondValues, second, inst.programCounter);
    }
    return true;
}

bool TranslationContext::dsSrc2(const RdnaInstruction& inst, IrOpcode opcode) {
    if (inst.gds) {
        throw std::runtime_error("GDS src2 operations are not supported");
    }
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(inst.source0);
    MemoryInfo source = memory;
    source.offset = memory.secondaryOffset;
    source.secondaryOffset = 0u;
    IrValue* value = loadSharedU32(1u, address, source, inst.programCounter);
    MemoryInfo target = memory;
    target.secondaryOffset = 0u;
    IrValue& active = ir.GetExec();
    (void)ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), value, &active}, addMemoryInfo(target, inst.programCounter));
    return true;
}

bool TranslationContext::dsAtomic2(const RdnaInstruction& inst, IrOpcode opcode, bool returnsValue) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(inst.source0);
    const IrU32 data0 = readU32(inst.source1);
    const IrU32 data1 = readU32(inst.source2);
    IrValue& active = ir.GetExec();
    IrValue& old = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &data0.Value(), &data1.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    if (returnsValue) writeOperand(inst.destination, &old);
    return true;
}

bool TranslationContext::dsAtomic64(const RdnaInstruction& inst, IrOpcode opcode, bool returnsValue) {
    if (inst.gds) {
        throw std::runtime_error("64-bit GDS atomics are not supported");
    }
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(inst.source0);
    const IrU64 data0 = readU64(inst.source1);
    IrValue& active = ir.GetExec();
    IrValue* old;
    if (IrOpcodeOperandCount(opcode) == 4u) {
        const IrU64 data1 = readU64(inst.source2);
        old = &ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &data0.Value(), &data1.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    } else {
        old = &ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &data0.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
    }
    if (returnsValue) writeOperand(inst.destination, old);
    return true;
}

bool TranslationContext::dsCondxchg32(const RdnaInstruction& inst) {
    if (inst.gds) {
        throw std::runtime_error("DS conditional exchange GDS mode is not supported");
    }
    MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    memory.dataDwords = 1u;
    memory.componentCount = 1u;
    const auto data = readU32Pair(inst.source1);
    const IrU32 address = readU32(inst.source0);
    IrValue& active = ir.GetExec();
    std::array<IrValue*, 2> old{};
    for (std::uint32_t index = 0u; index < 2u; ++index) {
        IrValue& enabled = ir.UGreaterThan(data[index].Value(), ir.Constant(0x7fffffffu));
        IrValue& mask = ir.Select(enabled, ir.Constant(0xffffffffu), ir.Constant(0u));
        IrValue& value = ir.Select(enabled, ir.BitwiseAnd(data[index].Value(), ir.Constant(0x7fffffffu)), ir.Constant(0u));
        memory.offset = inst.memoryOffset + index * 4u;
        old[index] = &ir.Emit(IrOpcode::SharedAtomicMskor32, IrType::U32, {&address.Value(), &mask, &value, &active}, addMemoryInfo(memory, inst.programCounter));
    }
    writeOperand(inst.destination, old[0]);
    writeOperand(offsetOperand(inst.destination, 1u), old[1]);
    return true;
}

bool TranslationContext::dsAppendConsume(const RdnaInstruction& inst, IrOpcode opcode) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    const IrU32 address = readU32(makeM0Operand());
    IrValue& active = ir.GetExec();
    IrValue& activeLo = ir.GetExecLo();
    IrValue& activeHi = ir.GetExecHi();
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {&address.Value(), &active, &activeLo, &activeHi}, addMemoryInfo(memory, inst.programCounter));
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::dsAddtid(const RdnaInstruction& inst, bool write) {
    const MemoryInfo memory = sharedMemoryInfoFromInstruction(inst);
    IrValue& base = ir.BitwiseAnd(readU32(makeM0Operand()).Value(), ir.Constant(0xffffu));
    IrValue& lane = ir.Emit(IrOpcode::LaneId, IrOpcodeType(IrOpcode::LaneId), {});
    IrValue& address = ir.IAdd(base, ir.ShiftLeftLogical(lane, ir.Constant(2u)));
    IrValue& active = ir.GetExec();
    if (write) {
        const IrU32 value = readU32(inst.source1);
        (void)ir.Emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &value.Value(), &active}, addMemoryInfo(memory, inst.programCounter));
        return true;
    }
    IrValue& loaded = ir.Emit(IrOpcode::LoadSharedU32, IrOpcodeType(IrOpcode::LoadSharedU32), {&address, &active}, addMemoryInfo(memory, inst.programCounter));
    writeOperand(inst.destination, &loaded);
    return true;
}

bool TranslationContext::dsSwizzleB32(const RdnaInstruction& inst) {
    const IrU32 value = readU32(inst.source0);
    IrValue& pattern = ir.Constant(inst.memoryOffset & 0xffffu);
    IrValue& active = ir.GetExec();
    IrValue& result = ir.Emit(IrOpcode::SwizzleU32, IrOpcodeType(IrOpcode::SwizzleU32), {&value.Value(), &pattern, &active});
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::dsBpermuteB32(const RdnaInstruction& inst) {
    const IrU32 index = readU32(inst.source0);
    IrValue& address = ir.IAdd(index.Value(), ir.Constant(inst.memoryOffset));
    const IrU32 value = readU32(inst.source1);
    IrValue& active = ir.GetExec();
    IrValue& result = ir.Emit(IrOpcode::BpermuteU32, IrOpcodeType(IrOpcode::BpermuteU32), {&value.Value(), &address, &active});
    writeOperand(inst.destination, &result);
    return true;
}

bool TranslationContext::dsPermuteB32(const RdnaInstruction& inst) {
    const IrU32 index = readU32(inst.source0);
    IrValue& address = ir.IAdd(index.Value(), ir.Constant(inst.memoryOffset));
    const IrU32 value = readU32(inst.source1);
    IrValue& active = ir.GetExec();
    IrValue& result = ir.Emit(IrOpcode::PermuteU32, IrOpcodeType(IrOpcode::PermuteU32), {&value.Value(), &address, &active});
    writeOperand(inst.destination, &result);
    return true;
}

}
