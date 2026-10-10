#include "Translation/MemoryInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include "Recompiler.hpp"
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
    memory.imageHasMip = inst.op == RdnaOpcode::ImageLoadMip || inst.op == RdnaOpcode::ImageStoreMip || inst.op == RdnaOpcode::ImageLoadMipPck || inst.op == RdnaOpcode::ImageLoadMipPckSgn || inst.op == RdnaOpcode::ImageStoreMipPck;
    memory.imagePacked = inst.op == RdnaOpcode::ImageLoadPck || inst.op == RdnaOpcode::ImageLoadPckSgn || inst.op == RdnaOpcode::ImageLoadMipPck || inst.op == RdnaOpcode::ImageLoadMipPckSgn || inst.op == RdnaOpcode::ImageStorePck || inst.op == RdnaOpcode::ImageStoreMipPck || inst.op == RdnaOpcode::ImageGather4hPck;
    memory.dataSigned = inst.op == RdnaOpcode::ImageLoadPckSgn || inst.op == RdnaOpcode::ImageLoadMipPckSgn;
    memory.imageR128 = inst.imageR128;
    return memory;
}

IrOpcode imageAtomic64Opcode(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::ImageAtomicSwap32: return IrOpcode::ImageAtomicSwap64;
    case IrOpcode::ImageAtomicIAdd32: return IrOpcode::ImageAtomicIAdd64;
    case IrOpcode::ImageAtomicISub32: return IrOpcode::ImageAtomicISub64;
    case IrOpcode::ImageAtomicUMin32: return IrOpcode::ImageAtomicUMin64;
    case IrOpcode::ImageAtomicUMax32: return IrOpcode::ImageAtomicUMax64;
    case IrOpcode::ImageAtomicSMin32: return IrOpcode::ImageAtomicSMin64;
    case IrOpcode::ImageAtomicSMax32: return IrOpcode::ImageAtomicSMax64;
    case IrOpcode::ImageAtomicAnd32: return IrOpcode::ImageAtomicAnd64;
    case IrOpcode::ImageAtomicOr32: return IrOpcode::ImageAtomicOr64;
    case IrOpcode::ImageAtomicXor32: return IrOpcode::ImageAtomicXor64;
    case IrOpcode::ImageAtomicCmpSwap32: return IrOpcode::ImageAtomicCmpSwap64;
    default: throw std::runtime_error("64-bit " + std::string(IrOpcodeName(opcode)) + " is not implemented");
    }
}

}

bool TranslationContext::imageBvhIntersectRay(const RdnaInstruction& inst) {
    if (RayTracingStrict()) {
        throw std::runtime_error("ray tracing is not implemented");
    }
    if (RayTracingMiss()) {
        IrValue& miss = ir.Constant(0xffffffffu);
        for (std::uint32_t i = 0u; i < inst.dataDwordCount; ++i) {
            writeOperand(offsetOperand(inst.destination, i), &miss);
        }
        return true;
    }
    if (inst.dataDwordCount != 4u || inst.imageD16) {
        throw std::runtime_error("image_bvh_intersect_ray returns four dwords");
    }
    MemoryInfo memory;
    memory.kind = ResourceKind::Global;
    memory.dataDwords = 4u;
    memory.imageSampleFlags = inst.imageA16 ? RdnaImageSampleFlagA16 : 0u;
    memory.addressIsFull = inst.op == RdnaOpcode::ImageBvh64IntersectRay;
    IrValue* descriptor = constructU32x4(inst.source1, 4u);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageBvhIntersectRay, IrOpcodeType(IrOpcode::ImageBvhIntersectRay), {descriptor, address, &ir.GetExec()}, addMemoryInfo(memory, inst.programCounter));
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        writeOperand(offsetOperand(inst.destination, i), &ir.CompositeExtract(result, i));
    }
    return true;
}

bool TranslationContext::imageAtomic(const RdnaInstruction& inst, IrOpcode opcode) {
    if (inst.dataBits == 64u) {
        opcode = imageAtomic64Opcode(opcode);
    }
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    const MemoryFlags flags = addMemoryInfo(memory, inst.programCounter);
    IrValue& exec = ir.GetExec();
    IrValue* result;
    if (opcode == IrOpcode::ImageAtomicCmpSwap64) {
        const IrU64 value = readU64(inst.destination);
        const IrU64 comparator = readU64(offsetOperand(inst.destination, 2u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, address, &value.Value(), &comparator.Value(), &exec}, flags);
    } else if (IsImageAtomic64Opcode(opcode)) {
        const IrU64 value = readU64(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, address, &value.Value(), &exec}, flags);
    } else if (opcode == IrOpcode::ImageAtomicCmpSwap32 || opcode == IrOpcode::ImageAtomicFCmpSwap32) {
        const IrU32 value = readU32(inst.destination);
        const IrU32 comparator = readU32(offsetOperand(inst.destination, 1u));
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, address, &value.Value(), &comparator.Value(), &exec}, flags);
    } else {
        const IrU32 value = readU32(inst.destination);
        result = &ir.Emit(opcode, IrOpcodeType(opcode), {resource, address, &value.Value(), &exec}, flags);
    }
    if (inst.glc) {
        writeOperand(inst.destination, result);
    }
    return true;
}

bool TranslationContext::imageGetResinfo(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageQueryDimensions, IrOpcodeType(IrOpcode::ImageQueryDimensions), {resource, address, &ir.GetExec()}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 4u);
    return true;
}

bool TranslationContext::imageGetLod(const RdnaInstruction& inst) {
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* sampler = getSamplerResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageQueryLod, IrOpcodeType(IrOpcode::ImageQueryLod), {resource, sampler, address, &ir.GetExec()}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 2u);
    return true;
}

bool TranslationContext::imageBy(const RdnaInstruction& inst) {
    const auto id = inst.imageOpcodeId;
    const bool packed = id >= 0x70u;
    if (packed ? id >= 0x76u : (id & 0x10u) != 0u) {
        throw std::runtime_error(packed ? "MIMG PCK2/PCK4 stores are not implemented" : "MIMG BY2/BY4 stores are not implemented");
    }
    MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    memory.imageHasMip = packed ? id == 0x73u || id == 0x74u : (id & 8u) != 0u;
    memory.imagePacked = packed;
    memory.imageByElements = packed ? (id == 0x71u || id == 0x74u ? 4u : 2u) : (id & 1u) != 0u ? 4u : 2u;
    IrValue* resource = getImageResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& exec = ir.GetExec();
    IrValue& result = ir.Emit(IrOpcode::ImageRead, IrOpcodeType(IrOpcode::ImageRead), {resource, address, &exec}, addMemoryInfo(memory, inst.programCounter));
    writeImageComponents(inst.destination, &result, memory, 4u);
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

bool TranslationContext::imageMsaaLoad(const RdnaInstruction& inst) {
    MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    memory.dataDwords = 1u;
    memory.componentCount = 1u;
    IrValue* resource = getImageResource(memory);
    IrValue& exec = ir.GetExec();
    for (std::uint32_t fragment = 0u; fragment < 4u; ++fragment) {
        IrValue* address = makeImageAddress(inst, inst.source0, fragment);
        IrValue& result = ir.Emit(IrOpcode::ImageRead, IrOpcodeType(IrOpcode::ImageRead), {resource, address, &exec}, addMemoryInfo(memory, inst.programCounter));
        writeImageComponents(offsetOperand(inst.destination, fragment), &result, memory, 4u);
    }
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
    IrValue& result = ir.Emit(IrOpcode::ImageSampleRaw, IrOpcodeType(IrOpcode::ImageSampleRaw), {resource, sampler, address, &ir.GetExec()}, addMemoryInfo(memory, inst.programCounter));
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
    if (inst.op == RdnaOpcode::ImageGather4hPck && inst.dataBits != 32u) {
        throw std::runtime_error("packed horizontal gather with D16 data is not measured");
    }
    const MemoryInfo memory = imageMemoryInfoFromInstruction(inst);
    IrValue* resource = getImageResource(memory);
    IrValue* sampler = getSamplerResource(memory);
    IrValue* address = makeImageAddress(inst, inst.source0);
    IrValue& result = ir.Emit(IrOpcode::ImageGatherRaw, IrOpcodeType(IrOpcode::ImageGatherRaw), {resource, sampler, address, &ir.GetExec()}, addMemoryInfo(memory, inst.programCounter));
    for (std::uint32_t index = 0; index < memory.dataDwords; ++index) {
        writeOperand(offsetOperand(inst.destination, index), &ir.CompositeExtract(result, index));
    }
    return true;
}

}
