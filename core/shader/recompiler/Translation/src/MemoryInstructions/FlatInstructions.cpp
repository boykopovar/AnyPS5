#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

// A multi-dword global load is one address instruction so the emitter can resolve its guest address
// once (see EmitBdaDwordReads). APS5_BDA_BYTE_READS=1 keeps one LoadAddressU32 per dword, as before.
bool wideAddressLoadsEnabled() {
    static const bool byteReads = std::getenv("APS5_BDA_BYTE_READS") != nullptr;
    return !byteReads;
}

IrOpcode wideAddressLoadOpcode(std::uint32_t dwords) {
    switch (dwords) {
    case 2u:
        return IrOpcode::LoadAddressU32x2;
    case 3u:
        return IrOpcode::LoadAddressU32x3;
    default:
        return IrOpcode::LoadAddressU32x4;
    }
}

ResourceKind flatSegmentResourceKind(std::uint32_t segment) {
    switch (segment) {
    case 1u:
        return ResourceKind::Scratch;
    case 2u:
        return ResourceKind::Global;
    default:
        return ResourceKind::Flat;
    }
}

MemoryInfo flatMemoryInfoFromInstruction(const RdnaInstruction& inst) {
    if (inst.family != RdnaInstructionFamily::FLAT) {
        throw std::runtime_error("flatMemoryInfoFromInstruction requires a FLAT instruction");
    }
    MemoryInfo memory;
    memory.kind = flatSegmentResourceKind(inst.memorySegment);
    memory.offset = inst.memoryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.dataBits = inst.dataBits;
    memory.componentCount = inst.dataDwordCount;
    memory.dataSigned = inst.dataSigned;
    memory.addressIsFull = memory.kind == ResourceKind::Flat || (memory.kind == ResourceKind::Global && inst.source1.kind == RdnaOperandKind::VectorRegister);
    return memory;
}

}

bool TranslationContext::flatLoad(const RdnaInstruction& inst) {
    const MemoryInfo memory = flatMemoryInfoFromInstruction(inst);
    IrOpcode opcode;
    switch (memory.dataBits) {
    case 8u:
        opcode = IrOpcode::LoadAddressU8;
        break;
    case 16u:
        opcode = IrOpcode::LoadAddressU16;
        break;
    case 32u:
        opcode = IrOpcode::LoadAddressU32;
        break;
    default:
        throw std::runtime_error("flatLoad does not support the requested data width");
    }
    const AddressOperands address = readAddressOperands(inst, 0u);
    IrValue& active = ir.GetExec();
    const std::uint32_t count = memory.dataBits == 32u ? std::min(memory.dataDwords, 4u) : 1u;
    if (count > 1u && memory.kind != ResourceKind::Scratch && wideAddressLoadsEnabled()) {
        MemoryInfo group = memory;
        group.dataDwords = count;
        group.componentCount = count;
        group.componentIndex = 0u;
        const IrOpcode wide = wideAddressLoadOpcode(count);
        IrValue& loaded = ir.Emit(wide, IrOpcodeType(wide), {address.resource, address.low, address.high, &active}, addMemoryInfo(group, inst.programCounter));
        for (std::uint32_t index = 0u; index < count; ++index) {
            writeOperand(offsetOperand(inst.destination, index), &ir.CompositeExtract(loaded, index));
        }
        return true;
    }
    for (std::uint32_t index = 0u; index < count; ++index) {
        MemoryInfo component = memory;
        component.offset += index * 4u;
        component.dataDwords = 1u;
        component.componentIndex = index;
        IrValue& loaded = ir.Emit(opcode, IrOpcodeType(opcode), {address.resource, address.low, address.high, &active}, addMemoryInfo(component, inst.programCounter));
        writeOperand(offsetOperand(inst.destination, index), memory.dataBits == 32u ? &loaded : &widenSubdword(&loaded, memory.dataBits, memory.dataSigned).Value());
    }
    return true;
}

bool TranslationContext::flatStore(const RdnaInstruction& inst) {
    const MemoryInfo memory = flatMemoryInfoFromInstruction(inst);
    IrOpcode opcode;
    switch (memory.dataBits) {
    case 8u:
        opcode = IrOpcode::StoreAddressU8;
        break;
    case 16u:
        opcode = IrOpcode::StoreAddressU16;
        break;
    case 32u:
        opcode = IrOpcode::StoreAddressU32;
        break;
    default:
        throw std::runtime_error("flatStore does not support the requested data width");
    }
    const AddressOperands address = readAddressOperands(inst, 0u);
    IrValue& active = ir.GetExec();
    const std::uint32_t count = memory.dataBits == 32u ? memory.dataDwords : 1u;
    for (std::uint32_t index = 0u; index < count; ++index) {
        MemoryInfo component = memory;
        component.offset += index * 4u;
        component.dataDwords = 1u;
        component.componentIndex = index;
        const IrU32 data = readU32(offsetOperand(inst.destination, index));
        IrValue* value = memory.dataBits == 32u ? &data.Value() : narrowSubdword(data, memory.dataBits);
        (void)ir.Emit(opcode, IrType::Void, {address.resource, address.low, address.high, value, &active}, addMemoryInfo(component, inst.programCounter));
    }
    return true;
}

}
