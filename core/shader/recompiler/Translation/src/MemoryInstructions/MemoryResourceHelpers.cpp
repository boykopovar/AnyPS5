#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

ResourceKind flatSegmentResourceKind(const RdnaInstruction& inst) {
    if (inst.family != RdnaInstructionFamily::FLAT) {
        throw std::runtime_error("flatSegmentResourceKind requires a FLAT instruction");
    }
    switch (inst.memorySegment) {
    case 1u:
        return ResourceKind::Scratch;
    case 2u:
        return ResourceKind::Global;
    default:
        return ResourceKind::Flat;
    }
}

}

MemoryFlags TranslationContext::addMemoryInfo(const MemoryInfo& memory, std::uint32_t pc) {
    const std::uint32_t index = static_cast<std::uint32_t>(program.Resources().memoryInfo.size());
    program.Resources().memoryInfo.push_back(memory);
    return MemoryFlags{index, pc};
}

TranslationContext::AddressOperands TranslationContext::readAddressOperands(const RdnaInstruction& inst, std::uint32_t firstSource) {
    const ResourceKind kind = flatSegmentResourceKind(inst);
    const IrU32 low = readU32(sourceAt(inst, firstSource));
    const RdnaOperand& highOrBase = sourceAt(inst, firstSource + 1u);
    if (kind == ResourceKind::Scratch) {
        IrValue& resource = ir.Emit(IrOpcode::GetScratchResource, IrOpcodeType(IrOpcode::GetScratchResource), {});
        IrValue& offset = highOrBase.kind != RdnaOperandKind::VectorRegister ? readU32(highOrBase).Value() : low.Value();
        return AddressOperands{&resource, &offset, &ir.Constant(0u)};
    }
    if (kind == ResourceKind::Global && highOrBase.kind != RdnaOperandKind::VectorRegister) {
        const IrU32 baseLow = readU32(highOrBase);
        const IrU32 baseHigh = readU32(offsetOperand(highOrBase, 1u));
        IrValue* resource = getAddressResource(&baseLow.Value(), &baseHigh.Value());
        return AddressOperands{resource, &low.Value(), &ir.Constant(0u)};
    }
    const IrU32 high = readU32(highOrBase);
    IrValue* resource = getAddressResource(&low.Value(), &high.Value());
    return AddressOperands{resource, &low.Value(), &high.Value()};
}

IrU32 TranslationContext::getResourceDword(std::uint32_t index, std::uint32_t dword) {
    return readScalarCode(index * 4u + dword);
}

IrValue* TranslationContext::getBufferResource(const MemoryInfo& memory) {
    const IrU32 dword0 = getResourceDword(memory.resource, 0u);
    const IrU32 dword1 = getResourceDword(memory.resource, 1u);
    const IrU32 dword2 = getResourceDword(memory.resource, 2u);
    const IrU32 dword3 = getResourceDword(memory.resource, 3u);
    return &ir.Emit(IrOpcode::GetBufferResource, IrOpcodeType(IrOpcode::GetBufferResource), {&dword0.Value(), &dword1.Value(), &dword2.Value(), &dword3.Value()});
}

IrValue* TranslationContext::getAddressResource(IrValue* low, IrValue* high) {
    return &ir.Emit(IrOpcode::GetAddressResource, IrOpcodeType(IrOpcode::GetAddressResource), {low, high});
}

IrValue* TranslationContext::getScalarAddressResource(std::uint32_t base) {
    const IrU32 low = readScalarCode(base);
    const IrU32 high = readScalarCode(base + 1u);
    return getAddressResource(&low.Value(), &high.Value());
}

IrValue* TranslationContext::getImageResource(const MemoryInfo& memory) {
    const auto dword = [&](std::uint32_t index) -> IrValue* {
        if (memory.imageR128 && index >= 4u) {
            return &ir.Constant(0u);
        }
        return &getResourceDword(memory.resource, index).Value();
    };
    return &ir.Emit(IrOpcode::GetImageResource, IrOpcodeType(IrOpcode::GetImageResource),
                     {dword(0u), dword(1u), dword(2u), dword(3u), dword(4u), dword(5u), dword(6u), dword(7u)});
}

IrValue* TranslationContext::getSamplerResource(const MemoryInfo& memory) {
    const IrU32 dword0 = getResourceDword(memory.sampler, 0u);
    const IrU32 dword1 = getResourceDword(memory.sampler, 1u);
    const IrU32 dword2 = getResourceDword(memory.sampler, 2u);
    const IrU32 dword3 = getResourceDword(memory.sampler, 3u);
    return &ir.Emit(IrOpcode::GetSamplerResource, IrOpcodeType(IrOpcode::GetSamplerResource), {&dword0.Value(), &dword1.Value(), &dword2.Value(), &dword3.Value()});
}

IrValue* TranslationContext::makeImageAddress(const RdnaInstruction& inst, const RdnaOperand& base) {
    std::array<IrValue*, 13> components{};
    IrValue& zero = ir.Constant(0u);
    components.fill(&zero);
    const std::uint32_t count = GetRdnaImageAddressDwordCount(inst.imageSampleFlags, inst.imageAddressComponents);
    if (count > components.size()) {
        throw std::runtime_error("image address component count exceeds the maximum supported");
    }
    const std::uint32_t nsaComponents = std::min(inst.imageNsaDwordCount * 4u, MaxRdnaImageNsaAddressComponents);
    const RdnaOperand plainBase = plainOperand(base);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (index != 0u && index - 1u < nsaComponents) {
            components[index] = &ir.GetVectorReg(static_cast<VectorReg>(inst.imageNsaVectorRegisters[index - 1u]));
        } else {
            components[index] = &readRawU32(offsetOperand(plainBase, index)).Value();
        }
    }
    return &ir.Emit(IrOpcode::MakeImageAddress, IrOpcodeType(IrOpcode::MakeImageAddress),
                     {components[0], components[1], components[2], components[3], components[4], components[5], components[6],
                      components[7], components[8], components[9], components[10], components[11], components[12]});
}

IrValue* TranslationContext::constructU32x4(const RdnaOperand& base, std::uint32_t count) {
    std::array<IrValue*, 4> components{};
    IrValue& zero = ir.Constant(0u);
    components.fill(&zero);
    const RdnaOperand plainBase = plainOperand(base);
    for (std::uint32_t index = 0; index < std::min(count, 4u); ++index) {
        components[index] = &readRawU32(offsetOperand(plainBase, index)).Value();
    }
    return &ir.Emit(IrOpcode::CompositeConstructU32x4, IrOpcodeType(IrOpcode::CompositeConstructU32x4), {components[0], components[1], components[2], components[3]});
}

void TranslationContext::writeImageComponents(const RdnaOperand& dst, IrValue* value, const MemoryInfo& memory, std::uint32_t componentLimit) {
    if (memory.dataBits == 16u) {
        for (std::uint32_t index = 0; index < memory.dataDwords; ++index) {
            writeOperand(offsetOperand(dst, index), &ir.CompositeExtract(*value, index));
        }
        return;
    }
    const std::uint32_t mask = memory.dmask != 0u ? memory.dmask : 1u;
    std::uint32_t dstIndex = 0;
    for (std::uint32_t component = 0; component < componentLimit; ++component) {
        if (((mask >> component) & 1u) == 0u) {
            continue;
        }
        writeOperand(offsetOperand(dst, dstIndex++), &ir.CompositeExtract(*value, component));
    }
}

// MUBUF/MTBUF sources: VADDR (the index first with IDXEN, then the offset with OFFEN), the resource, SOFFSET.
TranslationContext::BufferAddress TranslationContext::readBufferAddress(const RdnaInstruction& inst) {
    const IrU32 index = inst.idxen ? readU32(inst.source0) : IrU32(ir.Constant(0u));
    const IrU32 offset = inst.offen ? readU32(offsetOperand(inst.source0, inst.idxen ? 1u : 0u)) : IrU32(ir.Constant(0u));
    const IrU32 soffset = readU32(inst.source2);
    return BufferAddress{index, offset, soffset};
}

IrU32 TranslationContext::widenSubdword(IrValue* value, std::uint32_t bits, bool sign) {
    const IrOpcode opcode = bits == 8u ? IrOpcode::ConvertU32U8 : IrOpcode::ConvertU32U16;
    const IrU32 widened(ir.Emit(opcode, IrOpcodeType(opcode), {value}));
    if (!sign) {
        return widened;
    }
    return IrU32(ir.Emit(IrOpcode::BitFieldSExtract, IrOpcodeType(IrOpcode::BitFieldSExtract), {&widened.Value(), &ir.Constant(0u), &ir.Constant(bits)}));
}

IrValue* TranslationContext::narrowSubdword(IrU32 value, std::uint32_t bits) {
    const IrOpcode opcode = bits == 8u ? IrOpcode::ConvertU8U32 : IrOpcode::ConvertU16U32;
    return &ir.Emit(opcode, IrOpcodeType(opcode), {&value.Value()});
}

}
