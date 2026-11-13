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

bool TranslationContext::sScratchLoad(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_load not yet implemented: wave-uniform or swizzled scratch layout required");
}

bool TranslationContext::sScratchStore(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
}

MemoryInfo TranslationContext::scalarWriteMemoryInfo(const RdnaInstruction& inst, bool raw) {
    if (inst.source0.kind != RdnaOperandKind::ScalarRegister && !(raw && inst.source0.kind == RdnaOperandKind::VccLo)) {
        throw std::runtime_error("scalar memory base must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = raw ? ResourceKind::Global : ResourceKind::Buffer;
    memory.offset = inst.memoryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.dataBits = 32u;
    memory.componentCount = inst.dataDwordCount;
    memory.coherent = inst.glc;
    if (!raw) memory.resource = inst.source0.reg / 4u;
    return memory;
}

IrValue* TranslationContext::scalarWriteResource(const RdnaInstruction& inst, const MemoryInfo& memory, bool raw) {
    if (!raw) return getBufferResource(memory);
    const IrU32 baseLow = readU32(inst.source0);
    const IrU32 baseHigh = readU32(offsetOperand(inst.source0, 1u));
    return getAddressResource(&baseLow.Value(), &baseHigh.Value());
}

bool TranslationContext::sStore(const RdnaInstruction& inst, bool raw) {
    const MemoryInfo memory = scalarWriteMemoryInfo(inst, raw);
    IrValue* resource = scalarWriteResource(inst, memory, raw);
    const IrU32 soffset = readU32(inst.source1);
    IrValue& zero = ir.Constant(0u);
    IrValue& always = ir.ConstantBool(true);
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        MemoryInfo scalar = memory;
        scalar.offset += component * 4u;
        scalar.dataDwords = 1u;
        scalar.componentIndex = component;
        const IrU32 data = readU32(scalarDestinationOperand(inst.destination, component));
        const MemoryFlags flags = addMemoryInfo(scalar, inst.programCounter);
        if (raw) {
            (void)ir.Emit(IrOpcode::StoreAddressU32, IrType::Void, {resource, &soffset.Value(), &zero, &data.Value(), &always}, flags);
        } else {
            (void)ir.Emit(IrOpcode::StoreBufferU32, IrType::Void, {resource, &zero, &zero, &soffset.Value(), &data.Value(), &always}, flags);
        }
    }
    return true;
}

bool TranslationContext::sAtomic(const RdnaInstruction& inst, IrOpcode opcode, bool raw) {
    MemoryInfo memory = scalarWriteMemoryInfo(inst, raw);
    memory.dataDwords = ScalarMemoryAtomicReturnDwords(inst.op);
    memory.componentCount = memory.dataDwords;
    IrValue* resource = scalarWriteResource(inst, memory, raw);
    const IrU32 soffset = readU32(inst.source1);
    IrValue& zero = ir.Constant(0u);
    IrValue& always = ir.ConstantBool(true);
    IrValue& lane = ir.Emit(IrOpcode::LaneId, IrOpcodeType(IrOpcode::LaneId), {});
    IrValue& elected = ir.IEqual(lane, ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&lane, &always}));
    const bool wide = IrOpcodeType(opcode) == IrType::U64;
    const bool compare = opcode == IrOpcode::AddressAtomicCmpSwap32 || opcode == IrOpcode::AddressAtomicCmpSwap64 || opcode == IrOpcode::BufferAtomicCmpSwap32 || opcode == IrOpcode::BufferAtomicCmpSwap64;
    const auto component = [&](std::uint32_t index) -> IrValue* {
        if (!wide) return &readU32(scalarDestinationOperand(inst.destination, index)).Value();
        const IrU32 low = readU32(scalarDestinationOperand(inst.destination, index));
        const IrU32 high = readU32(scalarDestinationOperand(inst.destination, index + 1u));
        return &ir.ConstructU64(low.Value(), high.Value());
    };
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue* value = component(0u);
    IrValue* comparator = compare ? component(wide ? 2u : 1u) : nullptr;
    IrValue& result = raw ? (compare ? ir.Emit(opcode, IrOpcodeType(opcode), {resource, &soffset.Value(), &zero, value, comparator, &elected}, flags)
                                     : ir.Emit(opcode, IrOpcodeType(opcode), {resource, &soffset.Value(), &zero, value, &elected}, flags))
                          : (compare ? ir.Emit(opcode, IrOpcodeType(opcode), {resource, &zero, &zero, &soffset.Value(), value, comparator, &elected}, flags)
                                     : ir.Emit(opcode, IrOpcodeType(opcode), {resource, &zero, &zero, &soffset.Value(), value, &elected}, flags));
    if (!inst.glc) return true;
    if (!wide) {
        writeOperand(scalarDestinationOperand(inst.destination, 0u), &ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&result, &always}));
        return true;
    }
    const std::array<IrU32, 2> parts = extractU64(IrU64(result));
    for (std::uint32_t index = 0u; index < 2u; ++index) {
        writeOperand(scalarDestinationOperand(inst.destination, index), &ir.Emit(IrOpcode::ReadFirstLane, IrType::U32, {&parts[index].Value(), &always}));
    }
    return true;
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