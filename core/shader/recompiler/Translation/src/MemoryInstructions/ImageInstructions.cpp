#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

MemoryInfo imageMemoryInfoFromInstruction(const RdnaInstruction& inst) {
    if (inst.family != RdnaInstructionFamily::MIMG) {
        throw std::runtime_error("imageMemoryInfoFromInstruction requires a MIMG instruction");
    }
    if (inst.destination.kind != RdnaOperandKind::VectorRegister) {
        throw std::runtime_error("image data operand must be a vector register");
    }
    if (inst.source0.kind != RdnaOperandKind::VectorRegister) {
        throw std::runtime_error("image address operand must be a vector register");
    }
    if (inst.source1.kind != RdnaOperandKind::ScalarRegister) {
        throw std::runtime_error("image resource descriptor must be a scalar register");
    }
    if (inst.source2.kind != RdnaOperandKind::ScalarRegister) {
        throw std::runtime_error("image sampler descriptor must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = ResourceKind::Image;
    memory.resource = inst.source1.reg / 4u;
    memory.sampler = inst.source2.reg / 4u;
    memory.dmask = inst.imageDmask;
    memory.dataDwords = inst.dataDwordCount;
    memory.dataBits = inst.dataBits;
    memory.componentCount = inst.dataComponents;
    memory.imageSampleFlags = inst.imageSampleFlags;
    memory.imageDimension = inst.imageDimension;
    memory.imageAddressComponents = inst.imageAddressComponents;
    memory.imageHasMip = inst.op == RdnaOpcode::ImageLoadMip || inst.op == RdnaOpcode::ImageStoreMip;
    memory.imageR128 = inst.imageR128;
    return memory;
}

}

bool TranslationContext::imageAtomic(const RdnaInstruction& inst, IrOpcode opcode) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    const IrU32 value = readU32(inst.destination);
    IrValue& exec = ir.GetExec();
    IrValue& result = ir.Emit(opcode, IrOpcodeType(opcode), {resource, address, &value.Value(), &exec}, addMemoryInfo(memory, inst.programCounter));
    if (inst.glc) {
        writeOperand(inst.destination, &result);
    }
    return true;
}

bool TranslationContext::imageGetResinfo(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageQueryDimensions, IrOpcodeType(IrOpcode::ImageQueryDimensions), {resource, address}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 4u);
    return true;
}

bool TranslationContext::imageGetLod(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* sampler = getSamplerResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageQueryLod, IrOpcodeType(IrOpcode::ImageQueryLod), {resource, sampler, address}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 2u);
    return true;
}

bool TranslationContext::imageLoad(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& exec = ir.GetExec();
    IrValue& result = ir.Emit(IrOpcode::ImageRead, IrOpcodeType(IrOpcode::ImageRead), {resource, address, &exec}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 4u);
    return true;
}

bool TranslationContext::imageStore(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue* data = constructU32x4(inst.destination, memory.dataDwords);
    if (const DebugProbe probe = DebugProbeConfig(); probe.enabled) {
        RdnaOperand probeReg{};
        probeReg.kind = RdnaOperandKind::VectorRegister;
        probeReg.reg = 255u;
        IrValue& probed = ir.ShiftRightLogical(readRawU32(probeReg).Value(), ir.Constant(probe.shift));
        IrValue& zero = ir.Constant(0u);
        data = &ir.Emit(IrOpcode::CompositeConstructU32x4, IrOpcodeType(IrOpcode::CompositeConstructU32x4), {&probed, &zero, &zero, &zero});
    }
    IrValue& exec = ir.GetExec();
    (void)ir.Emit(IrOpcode::ImageWrite, IrOpcodeType(IrOpcode::ImageWrite), {resource, address, data, &exec}, addMemoryInfo(memory, inst.programCounter));
    return true;
}

bool TranslationContext::imageSample(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* sampler = getSamplerResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageSampleRaw, IrOpcodeType(IrOpcode::ImageSampleRaw), {resource, sampler, address}, addMemoryInfo(memory, inst.programCounter));
    const bool dref = (memory.imageSampleFlags & RdnaImageSampleFlagCompare) != 0u;
    if (dref && memory.dataBits != 16u) {
        IrValue& component = ir.CompositeExtract(result, 0u);
        for (std::uint32_t index = 0; index < memory.dataDwords; ++index) {
            writeOperand(offsetOperand(inst.destination, index), &component);
        }
    } else {
        writeImageComponents(inst.destination, &result, memory, 4u);
    }
    return true;
}

bool TranslationContext::imageGather(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* sampler = getSamplerResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageGatherRaw, IrOpcodeType(IrOpcode::ImageGatherRaw), {resource, sampler, address}, addMemoryInfo(memory, inst.programCounter));
    for (std::uint32_t index = 0; index < memory.dataDwords; ++index) {
        writeOperand(offsetOperand(inst.destination, index), &ir.CompositeExtract(result, index));
    }
    return true;
}

}
