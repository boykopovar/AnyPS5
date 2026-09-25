#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_TRANSLATIONCONTEXT_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_TRANSLATIONCONTEXT_HPP

#include "Translation/InstructionTranslator.hpp"
#include <array>
#include <cstdint>

namespace ShaderRecompiler {

class TranslationContext {
public:
    TranslationContext(IrProgram& program, IrBlock& block, std::uint32_t vectorLimit);

    void TranslateInstruction(const RdnaInstruction& instruction);
    void TranslateEmbeddedFetch(const RdnaInstruction& instruction, std::uint32_t attribute, std::uint32_t componentCount, const ShaderBufferResource& resource);
    void AddBranchCondition(const BasicBlock& source, BlockInfo& info);

private:
    struct AddressOperands {
        IrValue* resource = nullptr;
        IrValue* low = nullptr;
        IrValue* high = nullptr;
    };

    struct BufferAddress {
        IrU32 index;
        IrU32 offset;
        IrU32 soffset;
    };

    const RdnaOperand& sourceAt(const RdnaInstruction& inst, std::uint32_t index);
    RdnaOperand destinationOperand(const RdnaInstruction& inst);
    RdnaOperand offsetOperand(const RdnaOperand& operand, std::uint32_t offset);
    RdnaOperand scalarDestinationOperand(const RdnaOperand& operand, std::uint32_t offset);
    RdnaOperand plainOperand(const RdnaOperand& operand);
    std::array<IrU32, 2> ballotMask(IrU1 value);
    IrU32 readRawU32(const RdnaOperand& operand);
    IrU32 readScalarCode(std::uint32_t code);
    IrU32 applyBitSourceModifiers(const RdnaOperand& operand, IrU32 value);
    IrValue* readOperand(const RdnaOperand& operand, IrType type);
    IrU1 threadBit(const std::array<IrU32, 2>& mask);
    void writeRawU32(const RdnaOperand& operand, IrU32 value);
    IrF32 applyF32ResultModifiers(const RdnaOperand& operand, IrF32 value);
    void writeOperand(const RdnaOperand& operand, IrValue* value);
    IrU32 packHalf2x16(IrF32 low, IrF32 high);
    void write16Bits(const RdnaOperand& operand, IrU32 value);
    void writeF16(const RdnaOperand& operand, IrF32 value);
    IrU32 readU32(const RdnaOperand& operand);
    std::array<IrU32, 2> readU32Pair(const RdnaOperand& operand);
    IrU64 readU64(const RdnaOperand& operand);
    IrF32 readF16LaneAsF32(const RdnaOperand& operand, bool highLane, bool packed = false);
    IrF32 readF16AsF32(const RdnaOperand& operand);
    IrF32 readMixF32(const RdnaOperand& operand);
    IrU32 readU16LaneRaw(const RdnaOperand& operand, bool highLane);
    IrU32 readU16LaneAsU32(const RdnaOperand& operand, bool highLane, bool signExtend);
    IrU32 readU16AsU32(const RdnaOperand& operand, bool signExtend);
    IrU32 readF16LaneBits(const RdnaOperand& operand, bool highLane);
    std::array<IrU32, 2> extractU64(IrU64 value);
    void writeU32Pair(const RdnaOperand& operand, const std::array<IrU32, 2>& value);
    IrU32 conditionBit(const RdnaOperand& operand);
    IrU1 readMask(const RdnaOperand& operand);
    IrU1 readMaskValid(const RdnaOperand& operand);
    std::array<IrU32, 2> writeMask(const RdnaOperand& operand, IrU1 value, bool write64 = false);
    void writeCompareResult(const RdnaOperand& operand, IrU1 value);
    MemoryFlags addMemoryInfo(const MemoryInfo& memory, std::uint32_t pc);
    ExportFlags addExportInfo(const RdnaInstruction& inst);
    AddressOperands readAddressOperands(const RdnaInstruction& inst, std::uint32_t firstSource);
    IrU32 getResourceDword(std::uint32_t index, std::uint32_t dword);
    IrValue* getBufferResource(const MemoryInfo& memory);
    IrValue* getAddressResource(IrValue* low, IrValue* high);
    IrValue* getScalarAddressResource(std::uint32_t base);
    IrValue* getImageResource(const MemoryInfo& memory);
    IrValue* getSamplerResource(const MemoryInfo& memory);
    IrValue* makeImageAddress(const RdnaInstruction& inst, const RdnaOperand& base);
    IrValue* constructU32x4(const RdnaOperand& base, std::uint32_t count);
    void writeImageComponents(const RdnaOperand& dst, IrValue* value, const MemoryInfo& memory, std::uint32_t componentLimit);
    BufferAddress readBufferAddress(const RdnaInstruction& inst);
    IrU32 widenSubdword(IrValue* value, std::uint32_t bits, bool sign);
    IrValue* narrowSubdword(IrU32 value, std::uint32_t bits);
    bool sLoad(const RdnaInstruction& inst, bool raw);
    bool bufferLoad(const RdnaInstruction& inst);
    bool bufferStore(const RdnaInstruction& inst);
    bool bufferAtomic(const RdnaInstruction& inst, IrOpcode opcode);
    bool imageAtomic(const RdnaInstruction& inst, IrOpcode opcode);
    bool dsAtomic(const RdnaInstruction& inst, IrOpcode opcode, bool returnsValue);
    bool flatLoad(const RdnaInstruction& inst);
    bool flatStore(const RdnaInstruction& inst);
    bool imageGetResinfo(const RdnaInstruction& inst);
    bool imageGetLod(const RdnaInstruction& inst);
    bool imageLoad(const RdnaInstruction& inst);
    bool imageStore(const RdnaInstruction& inst);
    bool imageSample(const RdnaInstruction& inst);
    bool imageGather(const RdnaInstruction& inst);
    IrValue* loadSharedU32(std::uint32_t width, IrU32 address, const MemoryInfo& memory, std::uint32_t pc);
    IrValue* extractSharedU32(IrValue* value, std::uint32_t width, std::uint32_t index);
    void writeSharedU32(std::uint32_t width, IrU32 address, const std::array<IrValue*, 4>& values, const MemoryInfo& memory, std::uint32_t pc);
    bool dsRead(const RdnaInstruction& inst);
    bool dsRead2(const RdnaInstruction& inst);
    bool dsWrite(const RdnaInstruction& inst);
    bool dsWrite2(const RdnaInstruction& inst);
    bool dsMinmaxF32(const RdnaInstruction& inst, IrOpcode opcode);
    bool dsAppendConsume(const RdnaInstruction& inst, IrOpcode opcode);
    bool dsAddtid(const RdnaInstruction& inst, bool write);
    bool dsSwizzleB32(const RdnaInstruction& inst);
    bool dsBpermuteB32(const RdnaInstruction& inst);
    IrF32 selectF32(IrU1 condition, IrF32 trueValue, IrF32 falseValue);
    IrU32 convertF32ToU32Saturated(IrF32 value, float upperBound, float safeUpper, std::uint32_t highResult);
    IrU32 convertF32ToI32Saturated(IrF32 value, float lowerBound, float upperBound, float safeUpper, std::uint32_t lowerResult, std::uint32_t upperResult);
    IrU32 packU16Lanes(IrU32 low, IrU32 high);
    void emitCompareResult(const RdnaInstruction& inst, IrU1 value, bool scalar, bool cmpx);
    void emitCompareConstant(const RdnaInstruction& inst, bool value, bool scalar, bool cmpx);
    void emitIntegerCompare(const RdnaInstruction& inst, IrOpcode opcode, IrType type, bool scalar, bool cmpx);
    void emitInteger16Compare(const RdnaInstruction& inst, IrOpcode opcode, bool signedValue, bool cmpx);
    void emitFloatCompare(const RdnaInstruction& inst, IrOpcode opcode, bool half, bool cmpx);
    void emitFloatOrderedCompare(const RdnaInstruction& inst, bool ordered);
    void emitFloatClassCompare(const RdnaInstruction& inst, bool cmpx);
    void vCvtF32Ubyte(const RdnaInstruction& inst, std::uint32_t byteIndex);
    void vCvtF32U32(const RdnaInstruction& inst);
    void vCvtF32I32(const RdnaInstruction& inst);
    void vCvtU32F32(const RdnaInstruction& inst);
    void vCvtI32F32(const RdnaInstruction& inst);
    void vCvtF16F32(const RdnaInstruction& inst);
    void vCvtF32F16(const RdnaInstruction& inst);
    void vCvtF1616(const RdnaInstruction& inst, bool signedValue);
    void vCvt16F16(const RdnaInstruction& inst, bool signedValue);
    void vCvtRpiI32F32(const RdnaInstruction& inst);
    void vCvtFlrI32F32(const RdnaInstruction& inst);
    void vFrexpExpI32F32(const RdnaInstruction& inst);
    void vCvtOffF32I4(const RdnaInstruction& inst);
    void vCvtPkrtzF16F32(const RdnaInstruction& inst);
    void vCvtPknormF32(const RdnaInstruction& inst, IrOpcode opcode);
    void vCvtPkU8F32(const RdnaInstruction& inst);
    void vPackB32F16(const RdnaInstruction& inst);
    bool packedFloat16(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool quietSnan);
    bool float16Unary(const RdnaInstruction& inst, IrOpcode opcode, bool invalidNegative);
    bool float16Trig(const RdnaInstruction& inst, IrOpcode opcode);
    bool float16Binary(const RdnaInstruction& inst, IrOpcode opcode, bool reverse);
    bool float16Ternary(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool mix);
    bool floatUnary(const RdnaInstruction& inst, IrOpcode opcode);
    bool floatBinary(const RdnaInstruction& inst, IrOpcode opcode, bool reverse);
    bool floatTernary(const RdnaInstruction& inst, IrOpcode opcode, bool accumulator, bool mix);
    bool vFrexpMantF32(const RdnaInstruction& inst);
    bool vDot2cF32F16(const RdnaInstruction& inst);
    bool vCubeidF32(const RdnaInstruction& inst);
    bool vCubescF32(const RdnaInstruction& inst);
    bool vCubetcF32(const RdnaInstruction& inst);
    bool vCubemaF32(const RdnaInstruction& inst);
    bool floatCube(const RdnaInstruction& inst, std::uint32_t resultKind);
    bool integer16Shift(const RdnaInstruction& inst, IrOpcode opcode, bool arithmetic);
    bool integer16Binary(const RdnaInstruction& inst, IrOpcode opcode, bool sign);
    bool vMed3I16(const RdnaInstruction& inst);
    bool packedInteger16Shift(const RdnaInstruction& inst, IrOpcode opcode, bool arithmetic);
    bool packedInteger16Binary(const RdnaInstruction& inst, IrOpcode opcode);
    bool packedInteger16Mad(const RdnaInstruction& inst, bool sign);
    bool packedInteger16MinMax(const RdnaInstruction& inst, IrOpcode opcode, bool sign);
    bool sU64Mask(const RdnaInstruction& inst, IrOpcode logicalOpcode, IrOpcode bitOpcode, bool negateRhs, bool negateResult, bool unary);
    IrU1 u64MaskBinary(const RdnaInstruction& inst, IrOpcode opcode, bool negateRhs, bool negateResult);
    bool simpleInteger(const RdnaInstruction& inst, IrOpcode opcode, IrType type, bool reverse, bool maskShiftCount, bool updateScc);
    bool composedIntegerBinary(const RdnaInstruction& inst, IrOpcode opcode, bool negateRhs, bool negateResult, bool updateScc);
    bool vAndOrB32(const RdnaInstruction& inst);
    bool vOr3B32(const RdnaInstruction& inst);
    bool vXor3B32(const RdnaInstruction& inst);
    bool sFf1I32B64(const RdnaInstruction& inst);
    bool vFfbh32(const RdnaInstruction& inst, bool sign);
    bool sFlbitI32B64(const RdnaInstruction& inst);
    bool integer24(const RdnaInstruction& inst, bool sign, bool addend);
    bool vMadU64U32(const RdnaInstruction& inst);
    bool vSadU32(const RdnaInstruction& inst);
    bool vAdd3U32(const RdnaInstruction& inst);
    bool sBitsetB32(const RdnaInstruction& inst, bool set);
    bool sBitsetB64(const RdnaInstruction& inst, bool set);
    bool vBcntU32B32(const RdnaInstruction& inst);
    bool vMbcntU32B32(const RdnaInstruction& inst, bool low);
    bool sBitreplicateB64B32(const RdnaInstruction& inst);
    bool sQuadmaskB64(const RdnaInstruction& inst);
    bool bfmB32(const RdnaInstruction& inst);
    IrU32 rightMask32(IrU32 count);
    IrU64 rightMask64(IrU32 count);
    bool sBfmB64(const RdnaInstruction& inst);
    bool sBfeU32(const RdnaInstruction& inst, bool sign);
    bool sBfeU64(const RdnaInstruction& inst);
    bool vBfeU32(const RdnaInstruction& inst, bool sign);
    bool vBfiB32(const RdnaInstruction& inst);
    bool sBitcmpB32(const RdnaInstruction& inst, bool expected);
    bool vAlignbitB32(const RdnaInstruction& inst);
    bool vAlignbyteB32(const RdnaInstruction& inst);
    bool vLshlAddU32(const RdnaInstruction& inst);
    bool vAddLshlU32(const RdnaInstruction& inst);
    bool vXadU32(const RdnaInstruction& inst);
    bool vLshlOrB32(const RdnaInstruction& inst);
    bool vCndmaskB32(const RdnaInstruction& inst);
    bool packB16(const RdnaInstruction& inst, bool high0, bool high1);
    void sSubvectorLoop(const RdnaInstruction& inst, bool begin);
    void sSaveexec(const RdnaInstruction& inst, IrOpcode operation, bool negateExec, bool negateSource, bool write64);
    void addU32(const RdnaInstruction& inst, bool vector, bool useCarryIn);
    void subU32(const RdnaInstruction& inst, bool vector, bool reverse);
    void subbU32(const RdnaInstruction& inst, bool vector, bool reverse);
    void sAbsdiffI32(const RdnaInstruction& inst);
    void sAddSubI32(const RdnaInstruction& inst, bool subtract);
    void sLshlAddU32(const RdnaInstruction& inst, std::uint32_t shiftAmount);
    void scalarMinMax32(const RdnaInstruction& inst, IrOpcode valueOpcode, IrOpcode compareOpcode);
    void emitControlNop();
    void emitWaitcnt();
    void sBarrier();
    void sSendmsg(const RdnaInstruction& inst);
    void sTtracedata();
    void sInstPrefetch();
    void sGetpcB64(const RdnaInstruction& inst);
    void sCselectB32(const RdnaInstruction& inst);
    void scalarSelect64(const RdnaInstruction& inst, const RdnaOperand& falseSource);
    void movB32(const RdnaInstruction& inst, bool applyFloatModifiers);
    void sMovB64(const RdnaInstruction& inst);
    void sWqm(const RdnaInstruction& inst, bool wide);
    void vMovrelsB32(const RdnaInstruction& inst);
    void vMovreldB32(const RdnaInstruction& inst);
    void vReadfirstlaneB32(const RdnaInstruction& inst);
    void vReadlaneB32(const RdnaInstruction& inst);
    void vWritelaneB32(const RdnaInstruction& inst);
    void vPermlane16B32(const RdnaInstruction& inst, bool x16);
    void vInterpP1F32();
    void vInterpP2F32(const RdnaInstruction& inst);
    void vInterpMovF32(const RdnaInstruction& inst);
    void eXP(const RdnaInstruction& inst);
    bool emitScalar(const RdnaInstruction& inst);
    bool emitVector(const RdnaInstruction& inst);
    bool emitInterpolation(const RdnaInstruction& inst);
    bool emitMemory(const RdnaInstruction& inst);

    IrProgram& program;
    IrBuilder ir;
    IrBlock& block;
    IrU1 instructionBranchCondition;
    RdnaOpcode currentOpcode = RdnaOpcode::Unknown;
    std::uint32_t currentProgramCounter = 0;
    std::uint32_t currentVectorLimit = 1;
};

// Debug aid: APS5_PROBE=<pc hex>:<vgpr>[:<shift>] copies VGPR <vgpr> into v255 right after the
// instruction at <pc> is translated, and every image store then writes (v255 >> shift) instead of its
// data, so a dumped output image is a per-lane trace of that register.
struct DebugProbe {
    bool enabled = false;
    std::uint32_t programCounter = 0;
    std::uint32_t vgpr = 0;
    std::uint32_t shift = 0;
};
[[nodiscard]] DebugProbe DebugProbeConfig();

}

#endif
