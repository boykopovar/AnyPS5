#include "RdnaDecoder/RdnaOpcode.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

void requireClassifiableOpcode(RdnaOpcode opcode) {
    if (opcode == RdnaOpcode::Invalid || opcode == RdnaOpcode::Count) {
        throw std::invalid_argument("RdnaOpcode sentinel value cannot be classified");
    }
}

}

bool IsScalarAluOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::SMovB32:
        case RdnaOpcode::SMovB64:
        case RdnaOpcode::SMovkI32:
        case RdnaOpcode::SAddI32:
        case RdnaOpcode::SAddU32:
        case RdnaOpcode::SSubI32:
        case RdnaOpcode::SSubU32:
        case RdnaOpcode::SMulI32:
        case RdnaOpcode::SAndB32:
        case RdnaOpcode::SOrB32:
        case RdnaOpcode::SXorB32:
        case RdnaOpcode::SLshlB32:
        case RdnaOpcode::SLshrB32:
        case RdnaOpcode::SAshrI32:
        case RdnaOpcode::SBfeU32:
        case RdnaOpcode::SBfeI32:
        case RdnaOpcode::SCmpEqI32:
        case RdnaOpcode::SCmpLtI32:
        case RdnaOpcode::SCmpGtI32:
        case RdnaOpcode::SCmpEqU32:
        case RdnaOpcode::SCmovB32:
        case RdnaOpcode::SCmovB64:
        case RdnaOpcode::SAbsI32:
        case RdnaOpcode::SAbsdiffI32:
        case RdnaOpcode::SBrevB32:
        case RdnaOpcode::SSextI32I8:
        case RdnaOpcode::SSextI32I16:
        case RdnaOpcode::SBcnt0I32B32:
        case RdnaOpcode::SBcnt1I32B32:
        case RdnaOpcode::SBcnt1I32B64:
        case RdnaOpcode::SFf0I32B32:
        case RdnaOpcode::SFf1I32B32:
        case RdnaOpcode::SFf1I32B64:
        case RdnaOpcode::SFlbitI32B32:
        case RdnaOpcode::SFlbitI32B64:
        case RdnaOpcode::SBitreplicateB64B32:
        case RdnaOpcode::SQuadmaskB64:
        case RdnaOpcode::SAndSaveexecB32:
        case RdnaOpcode::SOrn2SaveexecB32:
        case RdnaOpcode::SAndn1SaveexecB32:
        case RdnaOpcode::SAndSaveexecB64:
        case RdnaOpcode::SOrSaveexecB64:
        case RdnaOpcode::SXorSaveexecB64:
        case RdnaOpcode::SAndn2SaveexecB64:
        case RdnaOpcode::SOrn2SaveexecB64:
        case RdnaOpcode::SAndn1SaveexecB64:
        case RdnaOpcode::SNotB32:
        case RdnaOpcode::SNotB64:
        case RdnaOpcode::SWqmB32:
        case RdnaOpcode::SWqmB64:
        case RdnaOpcode::SAddcU32:
        case RdnaOpcode::SSubbU32:
        case RdnaOpcode::SBitcmp0B32:
        case RdnaOpcode::SBitcmp1B32:
        case RdnaOpcode::SBitset0B32:
        case RdnaOpcode::SBitset1B32:
        case RdnaOpcode::SBitset0B64:
        case RdnaOpcode::SBitset1B64:
        case RdnaOpcode::SMinI32:
        case RdnaOpcode::SMaxI32:
        case RdnaOpcode::SMinU32:
        case RdnaOpcode::SMaxU32:
        case RdnaOpcode::SAndB64:
        case RdnaOpcode::SAndn2B32:
        case RdnaOpcode::SAndn2B64:
        case RdnaOpcode::SOrB64:
        case RdnaOpcode::SOrn2B32:
        case RdnaOpcode::SOrn2B64:
        case RdnaOpcode::SXorB64:
        case RdnaOpcode::SNandB32:
        case RdnaOpcode::SNandB64:
        case RdnaOpcode::SNorB32:
        case RdnaOpcode::SNorB64:
        case RdnaOpcode::SXnorB32:
        case RdnaOpcode::SXnorB64:
        case RdnaOpcode::SLshlB64:
        case RdnaOpcode::SLshl1AddU32:
        case RdnaOpcode::SLshl2AddU32:
        case RdnaOpcode::SLshl3AddU32:
        case RdnaOpcode::SLshl4AddU32:
        case RdnaOpcode::SLshrB64:
        case RdnaOpcode::SMulHiU32:
        case RdnaOpcode::SMulHiI32:
        case RdnaOpcode::SMulkI32:
        case RdnaOpcode::SBfeU64:
        case RdnaOpcode::SBfmB32:
        case RdnaOpcode::SBfmB64:
        case RdnaOpcode::SCselectB32:
        case RdnaOpcode::SCselectB64:
        case RdnaOpcode::SPackLlB32B16:
        case RdnaOpcode::SPackLhB32B16:
        case RdnaOpcode::SPackHhB32B16:
        case RdnaOpcode::SCmpLgI32:
        case RdnaOpcode::SCmpGeI32:
        case RdnaOpcode::SCmpLeI32:
        case RdnaOpcode::SCmpLgU32:
        case RdnaOpcode::SCmpGtU32:
        case RdnaOpcode::SCmpGeU32:
        case RdnaOpcode::SCmpLtU32:
        case RdnaOpcode::SCmpLeU32:
        case RdnaOpcode::SCmpEqU64:
        case RdnaOpcode::SCmpLgU64:
            return true;
        default:
            return false;
    }
}

bool IsVectorAluOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::VMovB32:
        case RdnaOpcode::VAddF32:
        case RdnaOpcode::VSubF32:
        case RdnaOpcode::VMulF32:
        case RdnaOpcode::VMadF32:
        case RdnaOpcode::VFmaF32:
        case RdnaOpcode::VAddI32:
        case RdnaOpcode::VSubI32:
        case RdnaOpcode::VMulLoU32:
        case RdnaOpcode::VMulHiU32:
        case RdnaOpcode::VMadU64U32:
        case RdnaOpcode::VSadU32:
        case RdnaOpcode::VAndB32:
        case RdnaOpcode::VOrB32:
        case RdnaOpcode::VXorB32:
        case RdnaOpcode::VLshlB32:
        case RdnaOpcode::VLshrB32:
        case RdnaOpcode::VAshrI32:
        case RdnaOpcode::VCmpEqF32:
        case RdnaOpcode::VCmpLtF32:
        case RdnaOpcode::VCmpGtF32:
        case RdnaOpcode::VCndmaskB32:
        case RdnaOpcode::VCvtF32I32:
        case RdnaOpcode::VCvtI32F32:
        case RdnaOpcode::VCvtF32U32:
        case RdnaOpcode::VCvtU32F32:
        case RdnaOpcode::VCvtF32F16:
        case RdnaOpcode::VCvtF16F32:
        case RdnaOpcode::VRcpF32:
        case RdnaOpcode::VRsqF32:
        case RdnaOpcode::VSqrtF32:
        case RdnaOpcode::VExpF32:
        case RdnaOpcode::VLogF32:
        case RdnaOpcode::VSinF32:
        case RdnaOpcode::VCosF32:
        case RdnaOpcode::VFloorF32:
        case RdnaOpcode::VCeilF32:
        case RdnaOpcode::VFractF32:
        case RdnaOpcode::VMinF32:
        case RdnaOpcode::VMaxF32:
        case RdnaOpcode::VMinI32:
        case RdnaOpcode::VMaxI32:
        case RdnaOpcode::VReadfirstlaneB32:
        case RdnaOpcode::VReadlaneB32:
        case RdnaOpcode::VWritelaneB32:
        case RdnaOpcode::VMovreldB32:
        case RdnaOpcode::VNop:
        case RdnaOpcode::VMovrelsB32:
        case RdnaOpcode::VPermlane16B32:
        case RdnaOpcode::VPermlanex16B32:
        case RdnaOpcode::VCubeidF32:
        case RdnaOpcode::VCubescF32:
        case RdnaOpcode::VCubetcF32:
        case RdnaOpcode::VCubemaF32:
        case RdnaOpcode::VDot2cF32F16:
        case RdnaOpcode::VCvtF16U16:
        case RdnaOpcode::VCvtU16F16:
        case RdnaOpcode::VCvtF16I16:
        case RdnaOpcode::VCvtI16F16:
        case RdnaOpcode::VCvtRpiI32F32:
        case RdnaOpcode::VCvtFlrI32F32:
        case RdnaOpcode::VFrexpExpI32F32:
        case RdnaOpcode::VFrexpMantF32:
        case RdnaOpcode::VCvtOffF32I4:
        case RdnaOpcode::VCvtF32Ubyte0:
        case RdnaOpcode::VCvtF32Ubyte1:
        case RdnaOpcode::VCvtF32Ubyte2:
        case RdnaOpcode::VCvtF32Ubyte3:
        case RdnaOpcode::VRcpIflagF32:
        case RdnaOpcode::VTruncF32:
        case RdnaOpcode::VRndneF32:
        case RdnaOpcode::VRcpF16:
        case RdnaOpcode::VSqrtF16:
        case RdnaOpcode::VRsqF16:
        case RdnaOpcode::VLogF16:
        case RdnaOpcode::VExpF16:
        case RdnaOpcode::VFloorF16:
        case RdnaOpcode::VCeilF16:
        case RdnaOpcode::VTruncF16:
        case RdnaOpcode::VRndneF16:
        case RdnaOpcode::VFractF16:
        case RdnaOpcode::VSinF16:
        case RdnaOpcode::VCosF16:
        case RdnaOpcode::VNotB32:
        case RdnaOpcode::VBfrevB32:
        case RdnaOpcode::VFfbhU32:
        case RdnaOpcode::VFfblB32:
        case RdnaOpcode::VFfbhI32:
        case RdnaOpcode::VSubrevF32:
        case RdnaOpcode::VMacF32:
        case RdnaOpcode::VMadmkF32:
        case RdnaOpcode::VMadakF32:
        case RdnaOpcode::VAddF16:
        case RdnaOpcode::VSubF16:
        case RdnaOpcode::VSubrevF16:
        case RdnaOpcode::VMulF16:
        case RdnaOpcode::VFmacF16:
        case RdnaOpcode::VFmamkF16:
        case RdnaOpcode::VFmaakF16:
        case RdnaOpcode::VMulI32I24:
        case RdnaOpcode::VMulU32U24:
        case RdnaOpcode::VMulLoI32:
        case RdnaOpcode::VMulHiI32:
        case RdnaOpcode::VCvtPkrtzF16F32:
        case RdnaOpcode::VCvtPkU8F32:
        case RdnaOpcode::VMadI32I24:
        case RdnaOpcode::VMadU32U24:
        case RdnaOpcode::VFmaF16:
        case RdnaOpcode::VPackB32F16:
        case RdnaOpcode::VBfeU32:
        case RdnaOpcode::VBfeI32:
        case RdnaOpcode::VBfiB32:
        case RdnaOpcode::VAlignbitB32:
        case RdnaOpcode::VAlignbyteB32:
        case RdnaOpcode::VMin3F32:
        case RdnaOpcode::VMin3I32:
        case RdnaOpcode::VMin3U32:
        case RdnaOpcode::VMin3F16:
        case RdnaOpcode::VMax3F32:
        case RdnaOpcode::VMax3I32:
        case RdnaOpcode::VMax3U32:
        case RdnaOpcode::VMax3F16:
        case RdnaOpcode::VMed3F32:
        case RdnaOpcode::VMed3I32:
        case RdnaOpcode::VMed3U32:
        case RdnaOpcode::VMed3F16:
        case RdnaOpcode::VMed3I16:
        case RdnaOpcode::VAdd3U32:
        case RdnaOpcode::VLshlAddU32:
        case RdnaOpcode::VAddLshlU32:
        case RdnaOpcode::VPermB32:
        case RdnaOpcode::VXadU32:
        case RdnaOpcode::VLshlOrB32:
        case RdnaOpcode::VAndOrB32:
        case RdnaOpcode::VOr3B32:
        case RdnaOpcode::VXor3B32:
        case RdnaOpcode::VSubrevI32:
        case RdnaOpcode::VBfmB32:
        case RdnaOpcode::VLdexpF32:
        case RdnaOpcode::VCvtPknormI16F32:
        case RdnaOpcode::VCvtPknormU16F32:
        case RdnaOpcode::VCvtPkU16U32:
        case RdnaOpcode::VCvtPkI16I32:
        case RdnaOpcode::VPkMadI16:
        case RdnaOpcode::VPkMulLoU16:
        case RdnaOpcode::VPkAddI16:
        case RdnaOpcode::VPkSubI16:
        case RdnaOpcode::VPkLshlrevB16:
        case RdnaOpcode::VPkLshrrevB16:
        case RdnaOpcode::VPkAshrrevI16:
        case RdnaOpcode::VPkMaxI16:
        case RdnaOpcode::VPkMinI16:
        case RdnaOpcode::VPkMadU16:
        case RdnaOpcode::VPkAddF16:
        case RdnaOpcode::VPkMulF16:
        case RdnaOpcode::VPkMinF16:
        case RdnaOpcode::VPkMaxF16:
        case RdnaOpcode::VPkFmaF16:
        case RdnaOpcode::VPkFmacF16:
        case RdnaOpcode::VPkAddU16:
        case RdnaOpcode::VPkSubU16:
        case RdnaOpcode::VPkMaxU16:
        case RdnaOpcode::VPkMinU16:
        case RdnaOpcode::VMadMixloF16:
        case RdnaOpcode::VMadMixhiF16:
        case RdnaOpcode::VAddNcU32:
        case RdnaOpcode::VAddcU32:
        case RdnaOpcode::VSubCoCiU32:
        case RdnaOpcode::VSubrevCoCiU32:
        case RdnaOpcode::VSubNcU32:
        case RdnaOpcode::VSubrevNcU32:
        case RdnaOpcode::VAddNcU16:
        case RdnaOpcode::VSubNcU16:
        case RdnaOpcode::VMaxU16:
        case RdnaOpcode::VMaxI16:
        case RdnaOpcode::VMinU16:
        case RdnaOpcode::VMinI16:
        case RdnaOpcode::VAddNcI16:
        case RdnaOpcode::VSubNcI16:
        case RdnaOpcode::VXnorB32:
        case RdnaOpcode::VLshlrevB32:
        case RdnaOpcode::VLshrrevB32:
        case RdnaOpcode::VAshrrevI32:
        case RdnaOpcode::VLshlrevB64:
        case RdnaOpcode::VLshrrevB64:
        case RdnaOpcode::VLshlrevB16:
        case RdnaOpcode::VLshrrevB16:
        case RdnaOpcode::VAshrrevI16:
        case RdnaOpcode::VBcntU32B32:
        case RdnaOpcode::VMbcntLoU32B32:
        case RdnaOpcode::VMbcntHiU32B32:
        case RdnaOpcode::VMinU32:
        case RdnaOpcode::VMaxU32:
        case RdnaOpcode::VMaxF16:
        case RdnaOpcode::VMinF16:
        case RdnaOpcode::VCmpFF32:
        case RdnaOpcode::VCmpLeF32:
        case RdnaOpcode::VCmpLgF32:
        case RdnaOpcode::VCmpGeF32:
        case RdnaOpcode::VCmpOF32:
        case RdnaOpcode::VCmpUF32:
        case RdnaOpcode::VCmpNgeF32:
        case RdnaOpcode::VCmpNlgF32:
        case RdnaOpcode::VCmpNgtF32:
        case RdnaOpcode::VCmpNleF32:
        case RdnaOpcode::VCmpNeqF32:
        case RdnaOpcode::VCmpNltF32:
        case RdnaOpcode::VCmpTruF32:
        case RdnaOpcode::VCmpxLtF32:
        case RdnaOpcode::VCmpxEqF32:
        case RdnaOpcode::VCmpxLeF32:
        case RdnaOpcode::VCmpxGtF32:
        case RdnaOpcode::VCmpxLgF32:
        case RdnaOpcode::VCmpxGeF32:
        case RdnaOpcode::VCmpxNgeF32:
        case RdnaOpcode::VCmpxNlgF32:
        case RdnaOpcode::VCmpxNgtF32:
        case RdnaOpcode::VCmpxNleF32:
        case RdnaOpcode::VCmpxNeqF32:
        case RdnaOpcode::VCmpxNltF32:
        case RdnaOpcode::VCmpFI32:
        case RdnaOpcode::VCmpLtI32:
        case RdnaOpcode::VCmpEqI32:
        case RdnaOpcode::VCmpLeI32:
        case RdnaOpcode::VCmpGtI32:
        case RdnaOpcode::VCmpNeI32:
        case RdnaOpcode::VCmpGeI32:
        case RdnaOpcode::VCmpTI32:
        case RdnaOpcode::VCmpClassF32:
        case RdnaOpcode::VCmpxClassF32:
        case RdnaOpcode::VCmpLtI16:
        case RdnaOpcode::VCmpEqI16:
        case RdnaOpcode::VCmpLeI16:
        case RdnaOpcode::VCmpGtI16:
        case RdnaOpcode::VCmpNeI16:
        case RdnaOpcode::VCmpGeI16:
        case RdnaOpcode::VCmpLtF16:
        case RdnaOpcode::VCmpEqF16:
        case RdnaOpcode::VCmpLeF16:
        case RdnaOpcode::VCmpGtF16:
        case RdnaOpcode::VCmpLgF16:
        case RdnaOpcode::VCmpGeF16:
        case RdnaOpcode::VCmpNeqF16:
        case RdnaOpcode::VCmpxLtF16:
        case RdnaOpcode::VCmpxEqF16:
        case RdnaOpcode::VCmpxLeF16:
        case RdnaOpcode::VCmpxGtF16:
        case RdnaOpcode::VCmpxGeF16:
        case RdnaOpcode::VCmpxNgtF16:
        case RdnaOpcode::VCmpxNeqF16:
        case RdnaOpcode::VCmpxNltF16:
        case RdnaOpcode::VCmpxLtI32:
        case RdnaOpcode::VCmpxEqI32:
        case RdnaOpcode::VCmpxLeI32:
        case RdnaOpcode::VCmpxGtI32:
        case RdnaOpcode::VCmpxNeI32:
        case RdnaOpcode::VCmpxGeI32:
        case RdnaOpcode::VCmpLtU16:
        case RdnaOpcode::VCmpEqU16:
        case RdnaOpcode::VCmpLeU16:
        case RdnaOpcode::VCmpGtU16:
        case RdnaOpcode::VCmpxLtU16:
        case RdnaOpcode::VCmpxGtU16:
        case RdnaOpcode::VCmpNeU16:
        case RdnaOpcode::VCmpGeU16:
        case RdnaOpcode::VCmpFU32:
        case RdnaOpcode::VCmpLtU32:
        case RdnaOpcode::VCmpEqU32:
        case RdnaOpcode::VCmpLeU32:
        case RdnaOpcode::VCmpGtU32:
        case RdnaOpcode::VCmpNeU32:
        case RdnaOpcode::VCmpGeU32:
        case RdnaOpcode::VCmpTU32:
        case RdnaOpcode::VCmpEqI64:
        case RdnaOpcode::VCmpLtU64:
        case RdnaOpcode::VCmpEqU64:
        case RdnaOpcode::VCmpGtU64:
        case RdnaOpcode::VCmpNeU64:
        case RdnaOpcode::VCmpxNeI64:
        case RdnaOpcode::VCmpxNeU64:
        case RdnaOpcode::VCmpxLtU32:
        case RdnaOpcode::VCmpxEqU32:
        case RdnaOpcode::VCmpxLeU32:
        case RdnaOpcode::VCmpxGtU32:
        case RdnaOpcode::VCmpxNeU32:
        case RdnaOpcode::VCmpxGeU32:
        case RdnaOpcode::VInterpP1F32:
        case RdnaOpcode::VInterpP2F32:
        case RdnaOpcode::VInterpMovF32:
            return true;
        default:
            return false;
    }
}

bool IsBranchOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    return IsDirectBranchOpcode(opcode);
}

bool IsScalarMemoryOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::SLoadDword:
        case RdnaOpcode::SLoadDwordx2:
        case RdnaOpcode::SLoadDwordx4:
        case RdnaOpcode::SLoadDwordx8:
        case RdnaOpcode::SLoadDwordx16:
        case RdnaOpcode::SBufferLoadDword:
        case RdnaOpcode::SBufferLoadDwordx2:
        case RdnaOpcode::SBufferLoadDwordx4:
        case RdnaOpcode::SBufferLoadDwordx8:
        case RdnaOpcode::SBufferLoadDwordx16:
            return true;
        default:
            return false;
    }
}

bool IsBufferMemoryOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::TBufferLoadFormatX:
        case RdnaOpcode::TBufferLoadFormatXyzw:
            throw std::logic_error("RdnaOpcode is an unused duplicate TBUFFER enum entry with no established classification");
        case RdnaOpcode::BufferLoadFormatX:
        case RdnaOpcode::BufferLoadFormatXy:
        case RdnaOpcode::BufferLoadFormatXyz:
        case RdnaOpcode::BufferLoadFormatXyzw:
        case RdnaOpcode::BufferLoadDword:
        case RdnaOpcode::BufferLoadDwordx2:
        case RdnaOpcode::BufferLoadDwordx4:
        case RdnaOpcode::BufferStoreFormatX:
        case RdnaOpcode::BufferStoreDword:
        case RdnaOpcode::BufferStoreDwordx4:
        case RdnaOpcode::BufferStoreFormatXy:
        case RdnaOpcode::BufferStoreFormatXyz:
        case RdnaOpcode::BufferStoreFormatXyzw:
        case RdnaOpcode::BufferLoadUbyte:
        case RdnaOpcode::BufferLoadSbyte:
        case RdnaOpcode::BufferLoadUshort:
        case RdnaOpcode::BufferLoadSshort:
        case RdnaOpcode::BufferLoadDwordx3:
        case RdnaOpcode::BufferStoreByte:
        case RdnaOpcode::BufferStoreShort:
        case RdnaOpcode::BufferStoreDwordx2:
        case RdnaOpcode::BufferStoreDwordx3:
        case RdnaOpcode::BufferAtomicSwap:
        case RdnaOpcode::BufferAtomicCmpswap:
        case RdnaOpcode::BufferAtomicSwapX2:
        case RdnaOpcode::BufferAtomicAdd:
        case RdnaOpcode::BufferAtomicSub:
        case RdnaOpcode::BufferAtomicSmin:
        case RdnaOpcode::BufferAtomicUmin:
        case RdnaOpcode::BufferAtomicSmax:
        case RdnaOpcode::BufferAtomicUmax:
        case RdnaOpcode::BufferAtomicAnd:
        case RdnaOpcode::BufferAtomicOr:
        case RdnaOpcode::BufferAtomicOrX2:
        case RdnaOpcode::BufferAtomicXor:
        case RdnaOpcode::BufferAtomicFmin:
        case RdnaOpcode::BufferAtomicFmax:
        case RdnaOpcode::TbufferLoadFormatX:
        case RdnaOpcode::TbufferLoadFormatXy:
        case RdnaOpcode::TbufferLoadFormatXyz:
        case RdnaOpcode::TbufferLoadFormatXyzw:
        case RdnaOpcode::TbufferStoreFormatX:
        case RdnaOpcode::TbufferStoreFormatXy:
        case RdnaOpcode::TbufferStoreFormatXyz:
        case RdnaOpcode::TbufferStoreFormatXyzw:
            return true;
        default:
            return false;
    }
}

bool IsImageOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::ImageSample:
        case RdnaOpcode::ImageSampleLz:
        case RdnaOpcode::ImageLoad:
        case RdnaOpcode::ImageStore:
        case RdnaOpcode::ImageGetResinfo:
        case RdnaOpcode::ImageGetLod:
        case RdnaOpcode::ImageLoadMip:
        case RdnaOpcode::ImageStoreMip:
        case RdnaOpcode::ImageAtomicSwap:
        case RdnaOpcode::ImageAtomicAdd:
        case RdnaOpcode::ImageAtomicUmin:
        case RdnaOpcode::ImageAtomicUmax:
        case RdnaOpcode::ImageAtomicAnd:
        case RdnaOpcode::ImageAtomicOr:
        case RdnaOpcode::ImageAtomicXor:
        case RdnaOpcode::ImageGather4Lz:
        case RdnaOpcode::ImageGather4C:
        case RdnaOpcode::ImageGather4CLz:
        case RdnaOpcode::ImageGather4LzO:
        case RdnaOpcode::ImageGather4CO:
        case RdnaOpcode::ImageGather4CLzO:
        case RdnaOpcode::ImageGather4h:
            return true;
        default:
            return false;
    }
}

bool IsExportOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::ExpMrt:
        case RdnaOpcode::ExpPos:
        case RdnaOpcode::ExpParam:
        case RdnaOpcode::Exp:
            return true;
        default:
            return false;
    }
}

bool IsConditionalBranchOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    switch (opcode) {
        case RdnaOpcode::SCbranchScc0:
        case RdnaOpcode::SCbranchScc1:
        case RdnaOpcode::SCbranchVccz:
        case RdnaOpcode::SCbranchVccnz:
        case RdnaOpcode::SCbranchExecz:
        case RdnaOpcode::SCbranchExecnz:
        case RdnaOpcode::SSubvectorLoopBegin:
        case RdnaOpcode::SSubvectorLoopEnd:
            return true;
        default:
            return false;
    }
}

bool IsDirectBranchOpcode(RdnaOpcode opcode) {
    requireClassifiableOpcode(opcode);
    if (opcode == RdnaOpcode::SBranch) {
        return true;
    }
    return IsConditionalBranchOpcode(opcode);
}

}
