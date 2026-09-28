#include "Translation/VectorInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

void TranslateVectorInstruction(IrBuilder& builder, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateVectorInstruction not implemented");
}

bool TranslationContext::emitVector(const RdnaInstruction& inst) {
    switch (inst.op) {
    case RdnaOpcode::VNop:
        return true;
    case RdnaOpcode::VMovB32:
        movB32(inst, true);
        return true;
    case RdnaOpcode::VAddI32:
        addU32(inst, true, false);
        return true;
    case RdnaOpcode::VAddcU32:
        addU32(inst, true, true);
        return true;
    case RdnaOpcode::VSubI32:
        subU32(inst, true, false);
        return true;
    case RdnaOpcode::VSubrevI32:
        subU32(inst, true, true);
        return true;
    case RdnaOpcode::VSubCoCiU32:
        subbU32(inst, true, false);
        return true;
    case RdnaOpcode::VSubrevCoCiU32:
        subbU32(inst, true, true);
        return true;
    case RdnaOpcode::VMovrelsB32:
        vMovrelsB32(inst);
        return true;
    case RdnaOpcode::VMovreldB32:
        vMovreldB32(inst);
        return true;
    case RdnaOpcode::VReadfirstlaneB32:
        vReadfirstlaneB32(inst);
        return true;
    case RdnaOpcode::VReadlaneB32:
        vReadlaneB32(inst);
        return true;
    case RdnaOpcode::VWritelaneB32:
        vWritelaneB32(inst);
        return true;
    case RdnaOpcode::VPermlane16B32:
        vPermlane16B32(inst, false);
        return true;
    case RdnaOpcode::VPermlanex16B32:
        vPermlane16B32(inst, true);
        return true;
    case RdnaOpcode::VCmpFF32:
    case RdnaOpcode::VCmpFI32:
    case RdnaOpcode::VCmpFU32:
        emitCompareConstant(inst, false, false, false);
        return true;
    case RdnaOpcode::VCmpTruF32:
    case RdnaOpcode::VCmpTI32:
    case RdnaOpcode::VCmpTU32:
        emitCompareConstant(inst, true, false, false);
        return true;
    case RdnaOpcode::VCmpEqU32:
    case RdnaOpcode::VCmpEqI32:
        emitIntegerCompare(inst, IrOpcode::IEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxEqU32:
    case RdnaOpcode::VCmpxEqI32:
        emitIntegerCompare(inst, IrOpcode::IEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpNeU32:
    case RdnaOpcode::VCmpNeI32:
        emitIntegerCompare(inst, IrOpcode::INotEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxNeU32:
    case RdnaOpcode::VCmpxNeI32:
        emitIntegerCompare(inst, IrOpcode::INotEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpGtU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThan32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxGtU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThan32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpGeU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThanEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxGeU32:
        emitIntegerCompare(inst, IrOpcode::UGreaterThanEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpLtU32:
        emitIntegerCompare(inst, IrOpcode::ULessThan32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxLtU32:
        emitIntegerCompare(inst, IrOpcode::ULessThan32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpLeU32:
        emitIntegerCompare(inst, IrOpcode::ULessThanEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxLeU32:
        emitIntegerCompare(inst, IrOpcode::ULessThanEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpGtI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThan32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxGtI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThan32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpGeI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThanEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxGeI32:
        emitIntegerCompare(inst, IrOpcode::SGreaterThanEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpLtI32:
        emitIntegerCompare(inst, IrOpcode::SLessThan32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxLtI32:
        emitIntegerCompare(inst, IrOpcode::SLessThan32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpLeI32:
        emitIntegerCompare(inst, IrOpcode::SLessThanEqual32, IrType::U32, false, false);
        return true;
    case RdnaOpcode::VCmpxLeI32:
        emitIntegerCompare(inst, IrOpcode::SLessThanEqual32, IrType::U32, false, true);
        return true;
    case RdnaOpcode::VCmpEqI64:
    case RdnaOpcode::VCmpEqU64:
        emitIntegerCompare(inst, IrOpcode::IEqual64, IrType::U64, false, false);
        return true;
    case RdnaOpcode::VCmpLtU64:
        emitIntegerCompare(inst, IrOpcode::ULessThan64, IrType::U64, false, false);
        return true;
    case RdnaOpcode::VCmpGtU64:
        emitIntegerCompare(inst, IrOpcode::UGreaterThan64, IrType::U64, false, false);
        return true;
    case RdnaOpcode::VCmpNeU64:
        emitIntegerCompare(inst, IrOpcode::INotEqual64, IrType::U64, false, false);
        return true;
    case RdnaOpcode::VCmpxNeI64:
    case RdnaOpcode::VCmpxNeU64:
        emitIntegerCompare(inst, IrOpcode::INotEqual64, IrType::U64, false, true);
        return true;
    case RdnaOpcode::VCmpEqU16:
        emitInteger16Compare(inst, IrOpcode::IEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpEqI16:
        emitInteger16Compare(inst, IrOpcode::IEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpNeU16:
        emitInteger16Compare(inst, IrOpcode::INotEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpNeI16:
        emitInteger16Compare(inst, IrOpcode::INotEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpGtU16:
        emitInteger16Compare(inst, IrOpcode::UGreaterThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxGtU16:
        emitInteger16Compare(inst, IrOpcode::UGreaterThan32, false, true);
        return true;
    case RdnaOpcode::VCmpGeU16:
        emitInteger16Compare(inst, IrOpcode::UGreaterThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpLtU16:
        emitInteger16Compare(inst, IrOpcode::ULessThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxLtU16:
        emitInteger16Compare(inst, IrOpcode::ULessThan32, false, true);
        return true;
    case RdnaOpcode::VCmpLeU16:
        emitInteger16Compare(inst, IrOpcode::ULessThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpGtI16:
        emitInteger16Compare(inst, IrOpcode::SGreaterThan32, true, false);
        return true;
    case RdnaOpcode::VCmpGeI16:
        emitInteger16Compare(inst, IrOpcode::SGreaterThanEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpLtI16:
        emitInteger16Compare(inst, IrOpcode::SLessThan32, true, false);
        return true;
    case RdnaOpcode::VCmpLeI16:
        emitInteger16Compare(inst, IrOpcode::SLessThanEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpEqF32:
        emitFloatCompare(inst, IrOpcode::FPOrdEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxEqF32:
        emitFloatCompare(inst, IrOpcode::FPOrdEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpLgF32:
        emitFloatCompare(inst, IrOpcode::FPOrdNotEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxLgF32:
        emitFloatCompare(inst, IrOpcode::FPOrdNotEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpGtF32:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxGtF32:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThan32, false, true);
        return true;
    case RdnaOpcode::VCmpGeF32:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxGeF32:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThanEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpLtF32:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxLtF32:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThan32, false, true);
        return true;
    case RdnaOpcode::VCmpLeF32:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxLeF32:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThanEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpNlgF32:
        emitFloatCompare(inst, IrOpcode::FPUnordEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxNlgF32:
        emitFloatCompare(inst, IrOpcode::FPUnordEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpNeqF32:
        emitFloatCompare(inst, IrOpcode::FPUnordNotEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxNeqF32:
        emitFloatCompare(inst, IrOpcode::FPUnordNotEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpNleF32:
        emitFloatCompare(inst, IrOpcode::FPUnordGreaterThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxNleF32:
        emitFloatCompare(inst, IrOpcode::FPUnordGreaterThan32, false, true);
        return true;
    case RdnaOpcode::VCmpNltF32:
        emitFloatCompare(inst, IrOpcode::FPUnordGreaterThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxNltF32:
        emitFloatCompare(inst, IrOpcode::FPUnordGreaterThanEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpNgeF32:
        emitFloatCompare(inst, IrOpcode::FPUnordLessThan32, false, false);
        return true;
    case RdnaOpcode::VCmpxNgeF32:
        emitFloatCompare(inst, IrOpcode::FPUnordLessThan32, false, true);
        return true;
    case RdnaOpcode::VCmpNgtF32:
        emitFloatCompare(inst, IrOpcode::FPUnordLessThanEqual32, false, false);
        return true;
    case RdnaOpcode::VCmpxNgtF32:
        emitFloatCompare(inst, IrOpcode::FPUnordLessThanEqual32, false, true);
        return true;
    case RdnaOpcode::VCmpEqF16:
        emitFloatCompare(inst, IrOpcode::FPOrdEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpxEqF16:
        emitFloatCompare(inst, IrOpcode::FPOrdEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpLgF16:
        emitFloatCompare(inst, IrOpcode::FPOrdNotEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpGtF16:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThan32, true, false);
        return true;
    case RdnaOpcode::VCmpxGtF16:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThan32, true, true);
        return true;
    case RdnaOpcode::VCmpGeF16:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThanEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpxGeF16:
        emitFloatCompare(inst, IrOpcode::FPOrdGreaterThanEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpLtF16:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThan32, true, false);
        return true;
    case RdnaOpcode::VCmpxLtF16:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThan32, true, true);
        return true;
    case RdnaOpcode::VCmpLeF16:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThanEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpxLeF16:
        emitFloatCompare(inst, IrOpcode::FPOrdLessThanEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpxNgtF16:
        emitFloatCompare(inst, IrOpcode::FPUnordLessThanEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpNeqF16:
        emitFloatCompare(inst, IrOpcode::FPUnordNotEqual32, true, false);
        return true;
    case RdnaOpcode::VCmpxNeqF16:
        emitFloatCompare(inst, IrOpcode::FPUnordNotEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpxNltF16:
        emitFloatCompare(inst, IrOpcode::FPUnordGreaterThanEqual32, true, true);
        return true;
    case RdnaOpcode::VCmpOF32:
        emitFloatOrderedCompare(inst, true);
        return true;
    case RdnaOpcode::VCmpUF32:
        emitFloatOrderedCompare(inst, false);
        return true;
    case RdnaOpcode::VCmpClassF32:
        emitFloatClassCompare(inst, false);
        return true;
    case RdnaOpcode::VCmpxClassF32:
        emitFloatClassCompare(inst, true);
        return true;
    case RdnaOpcode::VCvtF32Ubyte0:
        vCvtF32Ubyte(inst, 0u);
        return true;
    case RdnaOpcode::VCvtF32Ubyte1:
        vCvtF32Ubyte(inst, 1u);
        return true;
    case RdnaOpcode::VCvtF32Ubyte2:
        vCvtF32Ubyte(inst, 2u);
        return true;
    case RdnaOpcode::VCvtF32Ubyte3:
        vCvtF32Ubyte(inst, 3u);
        return true;
    case RdnaOpcode::VCvtF32U32:
        vCvtF32U32(inst);
        return true;
    case RdnaOpcode::VCvtF32I32:
        vCvtF32I32(inst);
        return true;
    case RdnaOpcode::VCvtU32F32:
        vCvtU32F32(inst);
        return true;
    case RdnaOpcode::VCvtI32F32:
        vCvtI32F32(inst);
        return true;
    case RdnaOpcode::VCvtF16F32:
        vCvtF16F32(inst);
        return true;
    case RdnaOpcode::VCvtF32F16:
        vCvtF32F16(inst);
        return true;
    case RdnaOpcode::VCvtF16U16:
        vCvtF1616(inst, false);
        return true;
    case RdnaOpcode::VCvtF16I16:
        vCvtF1616(inst, true);
        return true;
    case RdnaOpcode::VCvtU16F16:
        vCvt16F16(inst, false);
        return true;
    case RdnaOpcode::VCvtI16F16:
        vCvt16F16(inst, true);
        return true;
    case RdnaOpcode::VCvtRpiI32F32:
        vCvtRpiI32F32(inst);
        return true;
    case RdnaOpcode::VCvtFlrI32F32:
        vCvtFlrI32F32(inst);
        return true;
    case RdnaOpcode::VFrexpExpI32F32:
        vFrexpExpI32F32(inst);
        return true;
    case RdnaOpcode::VCvtOffF32I4:
        vCvtOffF32I4(inst);
        return true;
    case RdnaOpcode::VCvtPkrtzF16F32:
        vCvtPkrtzF16F32(inst);
        return true;
    case RdnaOpcode::VCvtPknormI16F32:
        vCvtPknormF32(inst, IrOpcode::PackSnorm2x16);
        return true;
    case RdnaOpcode::VCvtPknormU16F32:
        vCvtPknormF32(inst, IrOpcode::PackUnorm2x16);
        return true;
    case RdnaOpcode::VCvtPkU8F32:
        vCvtPkU8F32(inst);
        return true;
    case RdnaOpcode::VPackB32F16:
        vPackB32F16(inst);
        return true;
    case RdnaOpcode::VCvtPkU16U32:
    case RdnaOpcode::VCvtPkI16I32:
        return packB16(inst, false, false);
    case RdnaOpcode::VLshlrevB16:
        return integer16Shift(inst, IrOpcode::ShiftLeftLogical32, false);
    case RdnaOpcode::VLshrrevB16:
        return integer16Shift(inst, IrOpcode::ShiftRightLogical32, false);
    case RdnaOpcode::VAshrrevI16:
        return integer16Shift(inst, IrOpcode::ShiftRightArithmetic32, true);
    case RdnaOpcode::VAddNcU16:
    case RdnaOpcode::VAddNcI16:
        return integer16Binary(inst, IrOpcode::IAdd32, false);
    case RdnaOpcode::VSubNcU16:
    case RdnaOpcode::VSubNcI16:
        return integer16Binary(inst, IrOpcode::ISub32, false);
    case RdnaOpcode::VMed3I16:
        return vMed3I16(inst);
    case RdnaOpcode::VMinI16:
        return integer16Binary(inst, IrOpcode::SMin32, true);
    case RdnaOpcode::VMaxI16:
        return integer16Binary(inst, IrOpcode::SMax32, true);
    case RdnaOpcode::VMinU16:
        return integer16Binary(inst, IrOpcode::UMin32, false);
    case RdnaOpcode::VMaxU16:
        return integer16Binary(inst, IrOpcode::UMax32, false);
    case RdnaOpcode::VPkLshlrevB16:
        return packedInteger16Shift(inst, IrOpcode::ShiftLeftLogical32, false);
    case RdnaOpcode::VPkLshrrevB16:
        return packedInteger16Shift(inst, IrOpcode::ShiftRightLogical32, false);
    case RdnaOpcode::VPkAshrrevI16:
        return packedInteger16Shift(inst, IrOpcode::ShiftRightArithmetic32, true);
    case RdnaOpcode::VPkMadI16:
        return packedInteger16Mad(inst, true);
    case RdnaOpcode::VPkMadU16:
        return packedInteger16Mad(inst, false);
    case RdnaOpcode::VPkMulLoU16:
        return packedInteger16Binary(inst, IrOpcode::IMul32);
    case RdnaOpcode::VPkAddI16:
    case RdnaOpcode::VPkAddU16:
        return packedInteger16Binary(inst, IrOpcode::IAdd32);
    case RdnaOpcode::VPkSubI16:
    case RdnaOpcode::VPkSubU16:
        return packedInteger16Binary(inst, IrOpcode::ISub32);
    case RdnaOpcode::VPkMaxI16:
        return packedInteger16MinMax(inst, IrOpcode::SMax32, true);
    case RdnaOpcode::VPkMinI16:
        return packedInteger16MinMax(inst, IrOpcode::SMin32, true);
    case RdnaOpcode::VPkMaxU16:
        return packedInteger16MinMax(inst, IrOpcode::UMax32, false);
    case RdnaOpcode::VPkMinU16:
        return packedInteger16MinMax(inst, IrOpcode::UMin32, false);
    case RdnaOpcode::VPkAddF16:
        return packedFloat16(inst, IrOpcode::FPAdd32, false, false);
    case RdnaOpcode::VPkMulF16:
        return packedFloat16(inst, IrOpcode::FPMul32, false, false);
    case RdnaOpcode::VPkMinF16:
        return packedFloat16(inst, IrOpcode::FPMin32, false, true);
    case RdnaOpcode::VPkMaxF16:
        return packedFloat16(inst, IrOpcode::FPMax32, false, true);
    case RdnaOpcode::VPkFmaF16:
        return packedFloat16(inst, IrOpcode::FPFma32, false, false);
    case RdnaOpcode::VPkFmacF16:
        return packedFloat16(inst, IrOpcode::FPFma32, true, false);
    case RdnaOpcode::VAddF16:
        return float16Binary(inst, IrOpcode::FPAdd32, false);
    case RdnaOpcode::VSubF16:
        return float16Binary(inst, IrOpcode::FPSub32, false);
    case RdnaOpcode::VSubrevF16:
        return float16Binary(inst, IrOpcode::FPSub32, true);
    case RdnaOpcode::VMulF16:
        return float16Binary(inst, IrOpcode::FPMul32, false);
    case RdnaOpcode::VMinF16:
        return float16Binary(inst, IrOpcode::FPMin32, false);
    case RdnaOpcode::VMaxF16:
        return float16Binary(inst, IrOpcode::FPMax32, false);
    case RdnaOpcode::VFmacF16:
        return float16Ternary(inst, IrOpcode::FPFma32, true, false);
    case RdnaOpcode::VFmamkF16:
    case RdnaOpcode::VFmaakF16:
    case RdnaOpcode::VFmaF16:
        return float16Ternary(inst, IrOpcode::FPFma32, false, false);
    case RdnaOpcode::VMadMixloF16:
    case RdnaOpcode::VMadMixhiF16:
        return float16Ternary(inst, IrOpcode::FPFma32, false, true);
    case RdnaOpcode::VRcpF16:
        return float16Unary(inst, IrOpcode::FPRecip32, false);
    case RdnaOpcode::VSqrtF16:
        return float16Unary(inst, IrOpcode::FPSqrt, true);
    case RdnaOpcode::VRsqF16:
        return float16Unary(inst, IrOpcode::FPRecipSqrt32, true);
    case RdnaOpcode::VLogF16:
        return float16Unary(inst, IrOpcode::FPLog2, true);
    case RdnaOpcode::VExpF16:
        return float16Unary(inst, IrOpcode::FPExp2, false);
    case RdnaOpcode::VFloorF16:
        return float16Unary(inst, IrOpcode::FPFloor32, false);
    case RdnaOpcode::VCeilF16:
        return float16Unary(inst, IrOpcode::FPCeil32, false);
    case RdnaOpcode::VTruncF16:
        return float16Unary(inst, IrOpcode::FPTrunc32, false);
    case RdnaOpcode::VRndneF16:
        return float16Unary(inst, IrOpcode::FPRoundEven32, false);
    case RdnaOpcode::VFractF16:
        return float16Unary(inst, IrOpcode::FPFract32, false);
    case RdnaOpcode::VSinF16:
        return float16Trig(inst, IrOpcode::FPSin);
    case RdnaOpcode::VCosF16:
        return float16Trig(inst, IrOpcode::FPCos);
    case RdnaOpcode::VMin3F16:
        return float16Ternary(inst, IrOpcode::FPMinTri32, false, false);
    case RdnaOpcode::VMax3F16:
        return float16Ternary(inst, IrOpcode::FPMaxTri32, false, false);
    case RdnaOpcode::VMed3F16:
        return float16Ternary(inst, IrOpcode::FPMedTri32, false, false);
    case RdnaOpcode::VFrexpMantF32:
        return vFrexpMantF32(inst);
    case RdnaOpcode::VRcpF32:
        return floatUnary(inst, IrOpcode::FPRecip32);
    case RdnaOpcode::VRcpIflagF32:
        return floatUnary(inst, IrOpcode::FPRecipIFlag32);
    case RdnaOpcode::VFractF32:
        return floatUnary(inst, IrOpcode::FPFract32);
    case RdnaOpcode::VTruncF32:
        return floatUnary(inst, IrOpcode::FPTrunc32);
    case RdnaOpcode::VCeilF32:
        return floatUnary(inst, IrOpcode::FPCeil32);
    case RdnaOpcode::VRndneF32:
        return floatUnary(inst, IrOpcode::FPRoundEven32);
    case RdnaOpcode::VFloorF32:
        return floatUnary(inst, IrOpcode::FPFloor32);
    case RdnaOpcode::VExpF32:
        return floatUnary(inst, IrOpcode::FPExp2);
    case RdnaOpcode::VLogF32:
        return floatUnary(inst, IrOpcode::FPLog2);
    case RdnaOpcode::VRsqF32:
        return floatUnary(inst, IrOpcode::FPRecipSqrt32);
    case RdnaOpcode::VSqrtF32:
        return floatUnary(inst, IrOpcode::FPSqrt);
    case RdnaOpcode::VSinF32:
        return floatUnary(inst, IrOpcode::FPSin);
    case RdnaOpcode::VCosF32:
        return floatUnary(inst, IrOpcode::FPCos);
    case RdnaOpcode::VAddF32:
        return floatBinary(inst, IrOpcode::FPAdd32, false);
    case RdnaOpcode::VSubF32:
        return floatBinary(inst, IrOpcode::FPSub32, false);
    case RdnaOpcode::VSubrevF32:
        return floatBinary(inst, IrOpcode::FPSub32, true);
    case RdnaOpcode::VMulF32:
        return floatBinary(inst, IrOpcode::FPMul32, false);
    case RdnaOpcode::VMinF32:
        return floatBinary(inst, IrOpcode::FPMin32, false);
    case RdnaOpcode::VMaxF32:
        return floatBinary(inst, IrOpcode::FPMax32, false);
    case RdnaOpcode::VLdexpF32:
        return floatBinary(inst, IrOpcode::FPLdexp, false);
    case RdnaOpcode::VMacF32:
        return floatTernary(inst, IrOpcode::FPFma32, true, true);
    case RdnaOpcode::VMadmkF32:
    case RdnaOpcode::VMadakF32:
    case RdnaOpcode::VMadF32:
    case RdnaOpcode::VFmaF32:
        return floatTernary(inst, IrOpcode::FPFma32, false, true);
    case RdnaOpcode::VMin3F32:
        return floatTernary(inst, IrOpcode::FPMinTri32, false, false);
    case RdnaOpcode::VMax3F32:
        return floatTernary(inst, IrOpcode::FPMaxTri32, false, false);
    case RdnaOpcode::VMed3F32:
        return floatTernary(inst, IrOpcode::FPMedTri32, false, false);
    case RdnaOpcode::VDot2cF32F16:
        return vDot2cF32F16(inst);
    case RdnaOpcode::VCubeidF32:
        return vCubeidF32(inst);
    case RdnaOpcode::VCubescF32:
        return vCubescF32(inst);
    case RdnaOpcode::VCubetcF32:
        return vCubetcF32(inst);
    case RdnaOpcode::VCubemaF32:
        return vCubemaF32(inst);
    case RdnaOpcode::VMulLoU32:
    case RdnaOpcode::VMulLoI32:
        return simpleInteger(inst, IrOpcode::IMul32, IrType::U32, false, false, false);
    case RdnaOpcode::VMulHiU32:
        return simpleInteger(inst, IrOpcode::UMulHi, IrType::U32, false, false, false);
    case RdnaOpcode::VMulHiI32:
        return simpleInteger(inst, IrOpcode::SMulHi, IrType::U32, false, false, false);
    case RdnaOpcode::VAddNcU32:
        return simpleInteger(inst, IrOpcode::IAdd32, IrType::U32, false, false, false);
    case RdnaOpcode::VSubNcU32:
        return simpleInteger(inst, IrOpcode::ISub32, IrType::U32, false, false, false);
    case RdnaOpcode::VSubrevNcU32:
        return simpleInteger(inst, IrOpcode::ISub32, IrType::U32, true, false, false);
    case RdnaOpcode::VMinI32:
        return simpleInteger(inst, IrOpcode::SMin32, IrType::U32, false, false, false);
    case RdnaOpcode::VMaxI32:
        return simpleInteger(inst, IrOpcode::SMax32, IrType::U32, false, false, false);
    case RdnaOpcode::VMinU32:
        return simpleInteger(inst, IrOpcode::UMin32, IrType::U32, false, false, false);
    case RdnaOpcode::VMaxU32:
        return simpleInteger(inst, IrOpcode::UMax32, IrType::U32, false, false, false);
    case RdnaOpcode::VMin3I32:
        return simpleInteger(inst, IrOpcode::SMinTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VMax3I32:
        return simpleInteger(inst, IrOpcode::SMaxTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VMed3I32:
        return simpleInteger(inst, IrOpcode::SMedTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VMin3U32:
        return simpleInteger(inst, IrOpcode::UMinTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VMax3U32:
        return simpleInteger(inst, IrOpcode::UMaxTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VMed3U32:
        return simpleInteger(inst, IrOpcode::UMedTri32, IrType::U32, false, false, false);
    case RdnaOpcode::VAndB32:
        return simpleInteger(inst, IrOpcode::BitwiseAnd32, IrType::U32, false, false, false);
    case RdnaOpcode::VOrB32:
        return simpleInteger(inst, IrOpcode::BitwiseOr32, IrType::U32, false, false, false);
    case RdnaOpcode::VXorB32:
        return simpleInteger(inst, IrOpcode::BitwiseXor32, IrType::U32, false, false, false);
    case RdnaOpcode::VNotB32:
        return simpleInteger(inst, IrOpcode::BitwiseNot32, IrType::U32, false, false, false);
    case RdnaOpcode::VBfrevB32:
        return simpleInteger(inst, IrOpcode::BitReverse32, IrType::U32, false, false, false);
    case RdnaOpcode::VFfblB32:
        return simpleInteger(inst, IrOpcode::FindILsb32, IrType::U32, false, false, false);
    case RdnaOpcode::VLshlB32:
        return simpleInteger(inst, IrOpcode::ShiftLeftLogical32, IrType::U32, false, true, false);
    case RdnaOpcode::VLshlrevB32:
        return simpleInteger(inst, IrOpcode::ShiftLeftLogical32, IrType::U32, true, true, false);
    case RdnaOpcode::VLshrB32:
        return simpleInteger(inst, IrOpcode::ShiftRightLogical32, IrType::U32, false, true, false);
    case RdnaOpcode::VLshrrevB32:
        return simpleInteger(inst, IrOpcode::ShiftRightLogical32, IrType::U32, true, true, false);
    case RdnaOpcode::VAshrI32:
        return simpleInteger(inst, IrOpcode::ShiftRightArithmetic32, IrType::U32, false, true, false);
    case RdnaOpcode::VAshrrevI32:
        return simpleInteger(inst, IrOpcode::ShiftRightArithmetic32, IrType::U32, true, true, false);
    case RdnaOpcode::VLshlrevB64:
        return simpleInteger(inst, IrOpcode::ShiftLeftLogical64, IrType::U64, true, false, false);
    case RdnaOpcode::VLshrrevB64:
        return simpleInteger(inst, IrOpcode::ShiftRightLogical64, IrType::U64, true, false, false);
    case RdnaOpcode::VXnorB32:
        return composedIntegerBinary(inst, IrOpcode::BitwiseXor32, false, true, false);
    case RdnaOpcode::VAndOrB32:
        return vAndOrB32(inst);
    case RdnaOpcode::VOr3B32:
        return vOr3B32(inst);
    case RdnaOpcode::VXor3B32:
        return vXor3B32(inst);
    case RdnaOpcode::VFfbhU32:
        return vFfbh32(inst, false);
    case RdnaOpcode::VFfbhI32:
        return vFfbh32(inst, true);
    case RdnaOpcode::VMulI32I24:
        return integer24(inst, true, false);
    case RdnaOpcode::VMulU32U24:
        return integer24(inst, false, false);
    case RdnaOpcode::VMadI32I24:
        return integer24(inst, true, true);
    case RdnaOpcode::VMadU32U24:
        return integer24(inst, false, true);
    case RdnaOpcode::VMadU64U32:
        return vMadU64U32(inst);
    case RdnaOpcode::VSadU32:
        return vSadU32(inst);
    case RdnaOpcode::VAdd3U32:
        return vAdd3U32(inst);
    case RdnaOpcode::VBcntU32B32:
        return vBcntU32B32(inst);
    case RdnaOpcode::VMbcntLoU32B32:
        return vMbcntU32B32(inst, true);
    case RdnaOpcode::VMbcntHiU32B32:
        return vMbcntU32B32(inst, false);
    case RdnaOpcode::VBfmB32:
        return bfmB32(inst);
    case RdnaOpcode::VBfeU32:
        return vBfeU32(inst, false);
    case RdnaOpcode::VBfeI32:
        return vBfeU32(inst, true);
    case RdnaOpcode::VBfiB32:
        return vBfiB32(inst);
    case RdnaOpcode::VAlignbitB32:
        return vAlignbitB32(inst);
    case RdnaOpcode::VAlignbyteB32:
        return vAlignbyteB32(inst);
    case RdnaOpcode::VLshlAddU32:
        return vLshlAddU32(inst);
    case RdnaOpcode::VAddLshlU32:
        return vAddLshlU32(inst);
    case RdnaOpcode::VPermB32:
        return vPermB32(inst);
    case RdnaOpcode::VXadU32:
        return vXadU32(inst);
    case RdnaOpcode::VLshlOrB32:
        return vLshlOrB32(inst);
    case RdnaOpcode::VCndmaskB32:
        return vCndmaskB32(inst);
    default:
        return false;
    }
}

void TranslateVectorInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateVectorInstruction not implemented");
}

}
